/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:meta-window-actor
 * @title: MetaWindowActor
 * @short_description: An actor representing a top-level window in the scene graph
 */

#include <config.h>

#include <math.h>

#include <clutter/x11/clutter-x11.h>
#include <cogl/cogl-texture-pixmap-x11.h>
#include <gdk/gdk.h> /* for gdk_rectangle_union() */
#include <string.h>

#include <meta/display.h>
#include <meta/errors.h>
#include "frame.h"
#include <meta/window.h>
#include <meta/meta-shaped-texture.h>

#include "compositor-private.h"
#include "meta-shaped-texture-private.h"
#include "meta-shadow-factory-private.h"
#include "meta-window-actor-private.h"
#include "meta-texture-rectangle.h"
#include "region-utils.h"
#include "meta-monitor-manager.h"
#include "meta-cullable.h"

#include "meta-surface-actor.h"
#include "meta-surface-actor-x11.h"
#include "meta-surface-actor-wayland.h"

#include "wayland/meta-wayland-surface.h"

struct _MetaWindowActorPrivate
{
  MetaWindow *window;
  MetaCompositor *compositor;

  MetaSurfaceActor *surface;

  /* MetaShadowFactory only caches shadows that are actually in use;
   * to avoid unnecessary recomputation we do two things: 1) we store
   * both a focused and unfocused shadow for the window. If the window
   * doesn't have different focused and unfocused shadow parameters,
   * these will be the same. 2) when the shadow potentially changes we
   * don't immediately unreference the old shadow, we just flag it as
   * dirty and recompute it when we next need it (recompute_focused_shadow,
   * recompute_unfocused_shadow.) Because of our extraction of
   * size-invariant window shape, we'll often find that the new shadow
   * is the same as the old shadow.
   */
  MetaShadow       *focused_shadow;
  MetaShadow       *unfocused_shadow;

  /* A region that matches the shape of the window, including frame bounds */
  cairo_region_t   *shape_region;
  /* The region we should clip to when painting the shadow */
  cairo_region_t   *shadow_clip;

  /* Extracted size-invariant shape used for shadows */
  MetaWindowShape  *shadow_shape;
  char *            shadow_class;

  guint             send_frame_messages_timer;
  gint64            frame_drawn_time;

  guint             repaint_scheduled_id;
  guint             allocation_changed_id;

  /*
   * These need to be counters rather than flags, since more plugins
   * can implement same effect; the practicality of stacking effects
   * might be dubious, but we have to at least handle it correctly.
   */
  gint              minimize_in_progress;
  gint              maximize_in_progress;
  gint              unmaximize_in_progress;
  gint              map_in_progress;
  gint              destroy_in_progress;

  /* List of FrameData for recent frames */
  GList            *frames;
  guint             freeze_count;

  guint		    visible                : 1;
  guint		    disposed               : 1;

  /* If set, the client needs to be sent a _NET_WM_FRAME_DRAWN
   * client message using the most recent frame in ->frames */
  guint             needs_frame_drawn      : 1;
  guint             repaint_scheduled      : 1;

  guint             needs_reshape          : 1;
  guint             recompute_focused_shadow   : 1;
  guint             recompute_unfocused_shadow : 1;

  guint		    needs_destroy	   : 1;

  guint             no_shadow              : 1;

  guint             updates_frozen         : 1;
};

typedef struct _FrameData FrameData;

struct _FrameData
{
  int64_t frame_counter;
  guint64 sync_request_serial;
  gint64 frame_drawn_time;
};

enum
{
  PROP_META_WINDOW = 1,
  PROP_NO_SHADOW,
  PROP_SHADOW_CLASS
};

static void meta_window_actor_dispose    (GObject *object);
static void meta_window_actor_finalize   (GObject *object);
static void meta_window_actor_constructed (GObject *object);
static void meta_window_actor_set_property (GObject       *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
static void meta_window_actor_get_property (GObject      *object,
                                            guint         prop_id,
                                            GValue       *value,
                                            GParamSpec   *pspec);

static void meta_window_actor_paint (ClutterActor *actor);

static gboolean meta_window_actor_get_paint_volume (ClutterActor       *actor,
                                                    ClutterPaintVolume *volume);


static gboolean meta_window_actor_has_shadow (MetaWindowActor *self);

static void meta_window_actor_handle_updates (MetaWindowActor *self);

static void check_needs_reshape (MetaWindowActor *self);

static void do_send_frame_drawn (MetaWindowActor *self, FrameData *frame);
static void do_send_frame_timings (MetaWindowActor  *self,
                                   FrameData        *frame,
                                   gint             refresh_interval,
                                   gint64           presentation_time);

static void cullable_iface_init (MetaCullableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaWindowActor, meta_window_actor, CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (META_TYPE_CULLABLE, cullable_iface_init));

static void
frame_data_free (FrameData *frame)
{
  g_slice_free (FrameData, frame);
}

static void
meta_window_actor_class_init (MetaWindowActorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec   *pspec;

  g_type_class_add_private (klass, sizeof (MetaWindowActorPrivate));

  object_class->dispose      = meta_window_actor_dispose;
  object_class->finalize     = meta_window_actor_finalize;
  object_class->set_property = meta_window_actor_set_property;
  object_class->get_property = meta_window_actor_get_property;
  object_class->constructed  = meta_window_actor_constructed;

  actor_class->paint = meta_window_actor_paint;
  actor_class->get_paint_volume = meta_window_actor_get_paint_volume;

  pspec = g_param_spec_object ("meta-window",
                               "MetaWindow",
                               "The displayed MetaWindow",
                               META_TYPE_WINDOW,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_property (object_class,
                                   PROP_META_WINDOW,
                                   pspec);

  pspec = g_param_spec_boolean ("no-shadow",
                                "No shadow",
                                "Do not add shaddow to this window",
                                FALSE,
                                G_PARAM_READWRITE);

  g_object_class_install_property (object_class,
                                   PROP_NO_SHADOW,
                                   pspec);

  pspec = g_param_spec_string ("shadow-class",
                               "Name of the shadow class for this window.",
                               "NULL means to use the default shadow class for this window type",
                               NULL,
                               G_PARAM_READWRITE);

  g_object_class_install_property (object_class,
                                   PROP_SHADOW_CLASS,
                                   pspec);
}

static void
meta_window_actor_init (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
						   META_TYPE_WINDOW_ACTOR,
						   MetaWindowActorPrivate);
  priv->shadow_class = NULL;
}

static void
window_appears_focused_notify (MetaWindow *mw,
                               GParamSpec *arg1,
                               gpointer    data)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (data));
}

static void
surface_allocation_changed_notify (ClutterActor           *actor,
                                   const ClutterActorBox  *allocation,
                                   ClutterAllocationFlags  flags,
                                   MetaWindowActor        *self)
{
  meta_window_actor_sync_actor_geometry (self, FALSE);
  meta_window_actor_update_shape (self);
}

static void
surface_repaint_scheduled (MetaSurfaceActor *actor,
                           gpointer          user_data)
{
  MetaWindowActor *self = META_WINDOW_ACTOR (user_data);
  MetaWindowActorPrivate *priv = self->priv;

  priv->repaint_scheduled = TRUE;
}

static gboolean
is_argb32 (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  /* assume we're argb until we get the window (because
     in practice we're drawing nothing, so we're fully
     transparent)
  */
  if (priv->surface)
    return meta_surface_actor_is_argb32 (priv->surface);
  else
    return TRUE;
}

static gboolean
is_non_opaque (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaWindow *window = priv->window;

  return is_argb32 (self) || (window->opacity != 0xFF);
}

static gboolean
is_frozen (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  return priv->surface == NULL || priv->freeze_count > 0;
}

static void
meta_window_actor_freeze (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  if (priv->freeze_count == 0 && priv->surface)
    meta_surface_actor_set_frozen (priv->surface, TRUE);

  priv->freeze_count ++;
}

static void
meta_window_actor_thaw (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  if (priv->freeze_count <= 0)
    g_error ("Error in freeze/thaw accounting");

  priv->freeze_count--;
  if (priv->freeze_count > 0)
    return;

  if (priv->surface)
    meta_surface_actor_set_frozen (priv->surface, FALSE);

  /* We sometimes ignore moves and resizes on frozen windows */
  meta_window_actor_sync_actor_geometry (self, FALSE);

  /* We do this now since we might be going right back into the
   * frozen state */
  meta_window_actor_handle_updates (self);
}

static void
set_surface (MetaWindowActor  *self,
             MetaSurfaceActor *surface)
{
  MetaWindowActorPrivate *priv = self->priv;

  if (priv->surface)
    {
      g_signal_handler_disconnect (priv->surface, priv->repaint_scheduled_id);
      priv->repaint_scheduled_id = 0;
      g_signal_handler_disconnect (priv->surface, priv->allocation_changed_id);
      priv->allocation_changed_id = 0;
      clutter_actor_remove_child (CLUTTER_ACTOR (self), CLUTTER_ACTOR (priv->surface));
      g_object_unref (priv->surface);
    }

  priv->surface = surface;

  if (priv->surface)
    {
      g_object_ref_sink (priv->surface);
      priv->repaint_scheduled_id = g_signal_connect (priv->surface, "repaint-scheduled",
                                                     G_CALLBACK (surface_repaint_scheduled), self);
      priv->allocation_changed_id = g_signal_connect (priv->surface, "allocation-changed",
                                                      G_CALLBACK (surface_allocation_changed_notify), self);
      clutter_actor_add_child (CLUTTER_ACTOR (self), CLUTTER_ACTOR (priv->surface));

      /* If the previous surface actor was frozen, start out
       * frozen as well... */
      meta_surface_actor_set_frozen (priv->surface, priv->freeze_count > 0);

      meta_window_actor_update_shape (self);
    }
}

void
meta_window_actor_update_surface (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaWindow *window = priv->window;
  MetaSurfaceActor *surface_actor;

  if (window->surface)
    surface_actor = window->surface->surface_actor;
  else if (!meta_is_wayland_compositor ())
    surface_actor = meta_surface_actor_x11_new (window);
  else
    surface_actor = NULL;

  set_surface (self, surface_actor);
}

static void
meta_window_actor_constructed (GObject *object)
{
  MetaWindowActor *self = META_WINDOW_ACTOR (object);
  MetaWindowActorPrivate *priv = self->priv;
  MetaWindow *window = priv->window;

  priv->compositor = window->display->compositor;

  meta_window_actor_update_surface (self);

  meta_window_actor_update_opacity (self);

  /* Start off with an empty shape region to maintain the invariant
   * that it's always set */
  priv->shape_region = cairo_region_create ();
}

static void
meta_window_actor_dispose (GObject *object)
{
  MetaWindowActor *self = META_WINDOW_ACTOR (object);
  MetaWindowActorPrivate *priv = self->priv;
  MetaCompositor *compositor = priv->compositor;

  if (priv->disposed)
    return;

  priv->disposed = TRUE;

  if (priv->send_frame_messages_timer != 0)
    {
      g_source_remove (priv->send_frame_messages_timer);
      priv->send_frame_messages_timer = 0;
    }

  g_clear_pointer (&priv->shape_region, cairo_region_destroy);
  g_clear_pointer (&priv->shadow_clip, cairo_region_destroy);

  g_clear_pointer (&priv->shadow_class, g_free);
  g_clear_pointer (&priv->focused_shadow, meta_shadow_unref);
  g_clear_pointer (&priv->unfocused_shadow, meta_shadow_unref);
  g_clear_pointer (&priv->shadow_shape, meta_window_shape_unref);

  compositor->windows = g_list_remove (compositor->windows, (gconstpointer) self);

  g_clear_object (&priv->window);

  set_surface (self, NULL);

  G_OBJECT_CLASS (meta_window_actor_parent_class)->dispose (object);
}

static void
meta_window_actor_finalize (GObject *object)
{
  MetaWindowActor        *self = META_WINDOW_ACTOR (object);
  MetaWindowActorPrivate *priv = self->priv;

  g_list_free_full (priv->frames, (GDestroyNotify) frame_data_free);

  G_OBJECT_CLASS (meta_window_actor_parent_class)->finalize (object);
}

static void
meta_window_actor_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  MetaWindowActor        *self   = META_WINDOW_ACTOR (object);
  MetaWindowActorPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_META_WINDOW:
      priv->window = g_value_dup_object (value);
      g_signal_connect_object (priv->window, "notify::appears-focused",
                               G_CALLBACK (window_appears_focused_notify), self, 0);
      break;
    case PROP_NO_SHADOW:
      {
        gboolean newv = g_value_get_boolean (value);

        if (newv == priv->no_shadow)
          return;

        priv->no_shadow = newv;

        meta_window_actor_invalidate_shadow (self);
      }
      break;
    case PROP_SHADOW_CLASS:
      {
        const char *newv = g_value_get_string (value);

        if (g_strcmp0 (newv, priv->shadow_class) == 0)
          return;

        g_free (priv->shadow_class);
        priv->shadow_class = g_strdup (newv);

        meta_window_actor_invalidate_shadow (self);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_window_actor_get_property (GObject      *object,
                                guint         prop_id,
                                GValue       *value,
                                GParamSpec   *pspec)
{
  MetaWindowActorPrivate *priv = META_WINDOW_ACTOR (object)->priv;

  switch (prop_id)
    {
    case PROP_META_WINDOW:
      g_value_set_object (value, priv->window);
      break;
    case PROP_NO_SHADOW:
      g_value_set_boolean (value, priv->no_shadow);
      break;
    case PROP_SHADOW_CLASS:
      g_value_set_string (value, priv->shadow_class);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static const char *
meta_window_actor_get_shadow_class (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  if (priv->shadow_class != NULL)
    return priv->shadow_class;
  else
    {
      MetaWindowType window_type = meta_window_get_window_type (priv->window);

      switch (window_type)
        {
        case META_WINDOW_DROPDOWN_MENU:
          return "dropdown-menu";
        case META_WINDOW_POPUP_MENU:
          return "popup-menu";
        default:
          {
            MetaFrameType frame_type = meta_window_get_frame_type (priv->window);
            return meta_frame_type_to_string (frame_type);
          }
        }
    }
}

static void
meta_window_actor_get_shadow_params (MetaWindowActor  *self,
                                     gboolean          appears_focused,
                                     MetaShadowParams *params)
{
  const char *shadow_class = meta_window_actor_get_shadow_class (self);

  meta_shadow_factory_get_params (meta_shadow_factory_get_default (),
                                  shadow_class, appears_focused,
                                  params);
}

void
meta_window_actor_get_shape_bounds (MetaWindowActor       *self,
                                    cairo_rectangle_int_t *bounds)
{
  MetaWindowActorPrivate *priv = self->priv;

  cairo_region_get_extents (priv->shape_region, bounds);

  if (META_IS_SURFACE_ACTOR_WAYLAND (priv->surface))
    {
      double scale = priv->surface ?
                     meta_surface_actor_wayland_get_scale (META_SURFACE_ACTOR_WAYLAND (priv->surface)) : 1.;
      bounds->x *= scale;
      bounds->y *= scale;
      bounds->width *= scale;
      bounds->height *= scale;
    }
}

static void
meta_window_actor_get_shadow_bounds (MetaWindowActor       *self,
                                     gboolean               appears_focused,
                                     cairo_rectangle_int_t *bounds)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaShadow *shadow = appears_focused ? priv->focused_shadow : priv->unfocused_shadow;
  cairo_rectangle_int_t shape_bounds;
  MetaShadowParams params;

  meta_window_actor_get_shape_bounds (self, &shape_bounds);
  meta_window_actor_get_shadow_params (self, appears_focused, &params);

  meta_shadow_get_bounds (shadow,
                          params.x_offset + shape_bounds.x,
                          params.y_offset + shape_bounds.y,
                          shape_bounds.width,
                          shape_bounds.height,
                          bounds);
}

/* If we have an ARGB32 window that we decorate with a frame, it's
 * probably something like a translucent terminal - something where
 * the alpha channel represents transparency rather than a shape.  We
 * don't want to show the shadow through the translucent areas since
 * the shadow is wrong for translucent windows (it should be
 * translucent itself and colored), and not only that, will /look/
 * horribly wrong - a misplaced big black blob. As a hack, what we
 * want to do is just draw the shadow as normal outside the frame, and
 * inside the frame draw no shadow.  This is also not even close to
 * the right result, but looks OK. We also apply this approach to
 * windows set to be partially translucent with _NET_WM_WINDOW_OPACITY.
 */
static gboolean
clip_shadow_under_window (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  return is_non_opaque (self) && priv->window->frame;
}

static void
meta_window_actor_paint (ClutterActor *actor)
{
  MetaWindowActor *self = META_WINDOW_ACTOR (actor);
  MetaWindowActorPrivate *priv = self->priv;
  gboolean appears_focused = meta_window_appears_focused (priv->window);
  MetaShadow *shadow = appears_focused ? priv->focused_shadow : priv->unfocused_shadow;

 /* This window got damage when obscured; we set up a timer
  * to send frame completion events, but since we're drawing
  * the window now (for some other reason) cancel the timer
  * and send the completion events normally */
  if (priv->send_frame_messages_timer != 0)
    {
      g_source_remove (priv->send_frame_messages_timer);
      priv->send_frame_messages_timer = 0;
    }

  if (shadow != NULL)
    {
      MetaShadowParams params;
      cairo_rectangle_int_t shape_bounds;
      cairo_region_t *clip = priv->shadow_clip;
      MetaWindow *window = priv->window;

      meta_window_actor_get_shape_bounds (self, &shape_bounds);
      meta_window_actor_get_shadow_params (self, appears_focused, &params);

      /* The frame bounds are already subtracted from priv->shadow_clip
       * if that exists.
       */
      if (!clip && clip_shadow_under_window (self))
        {
          cairo_region_t *frame_bounds = meta_window_get_frame_bounds (priv->window);
          cairo_rectangle_int_t bounds;

          meta_window_actor_get_shadow_bounds (self, appears_focused, &bounds);
          clip = cairo_region_create_rectangle (&bounds);

          cairo_region_subtract (clip, frame_bounds);
        }

      meta_shadow_paint (shadow,
                         params.x_offset + shape_bounds.x,
                         params.y_offset + shape_bounds.y,
                         shape_bounds.width,
                         shape_bounds.height,
                         (clutter_actor_get_paint_opacity (actor) * params.opacity * window->opacity) / (255 * 255),
                         clip,
                         clip_shadow_under_window (self)); /* clip_strictly - not just as an optimization */

      if (clip && clip != priv->shadow_clip)
        cairo_region_destroy (clip);
    }

  CLUTTER_ACTOR_CLASS (meta_window_actor_parent_class)->paint (actor);
}

static gboolean
meta_window_actor_get_paint_volume (ClutterActor       *actor,
                                    ClutterPaintVolume *volume)
{
  MetaWindowActor *self = META_WINDOW_ACTOR (actor);
  MetaWindowActorPrivate *priv = self->priv;
  cairo_rectangle_int_t unobscured_bounds, bounds;
  gboolean appears_focused = meta_window_appears_focused (priv->window);
  ClutterVertex origin;

  /* The paint volume is computed before paint functions are called
   * so our bounds might not be updated yet. Force an update. */
  meta_window_actor_handle_updates (self);

  meta_window_actor_get_shape_bounds (self, &bounds);

  if (priv->surface)
    {
      if (meta_surface_actor_get_unobscured_bounds (priv->surface, &unobscured_bounds))
        gdk_rectangle_intersect (&bounds, &unobscured_bounds, &bounds);
    }

  if (appears_focused ? priv->focused_shadow : priv->unfocused_shadow)
    {
      cairo_rectangle_int_t shadow_bounds;

      /* We could compute an full clip region as we do for the window
       * texture, but the shadow is relatively cheap to draw, and
       * a little more complex to clip, so we just catch the case where
       * the shadow is completely obscured and doesn't need to be drawn
       * at all.
       */

      meta_window_actor_get_shadow_bounds (self, appears_focused, &shadow_bounds);
      gdk_rectangle_union (&bounds, &shadow_bounds, &bounds);
    }

  origin.x = bounds.x;
  origin.y = bounds.y;
  origin.z = 0.0f;
  clutter_paint_volume_set_origin (volume, &origin);

  clutter_paint_volume_set_width (volume, bounds.width);
  clutter_paint_volume_set_height (volume, bounds.height);

  return TRUE;
}

static gboolean
meta_window_actor_has_shadow (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  if (priv->no_shadow)
    return FALSE;

  /* Leaving out shadows for maximized and fullscreen windows is an effeciency
   * win and also prevents the unsightly effect of the shadow of maximized
   * window appearing on an adjacent window */
  if ((meta_window_get_maximized (priv->window) == META_MAXIMIZE_BOTH) ||
      meta_window_is_fullscreen (priv->window))
    return FALSE;

  /*
   * If we have two snap-tiled windows, we don't want the shadow to obstruct
   * the other window.
   */
  if (meta_window_get_tile_match (priv->window))
    return FALSE;

  /*
   * Always put a shadow around windows with a frame - This should override
   * the restriction about not putting a shadow around ARGB windows.
   */
  if (meta_window_get_frame (priv->window))
    return TRUE;

  /*
   * Do not add shadows to non-opaque windows; eventually we should generate
   * a shadow from the input shape for such windows.
   */
  if (is_non_opaque (self))
    return FALSE;

  /*
   * Add shadows to override redirect windows on X11 unless the toolkit
   * indicates that it is handling shadows itself (e.g., Gtk menus).
   */
  if (priv->window->override_redirect &&
      !priv->window->has_custom_frame_extents)
    return TRUE;

  return FALSE;
}

/**
 * meta_window_actor_get_meta_window:
 * @self: a #MetaWindowActor
 *
 * Gets the #MetaWindow object that the the #MetaWindowActor is displaying
 *
 * Return value: (transfer none): the displayed #MetaWindow
 */
MetaWindow *
meta_window_actor_get_meta_window (MetaWindowActor *self)
{
  return self->priv->window;
}

/**
 * meta_window_actor_get_texture:
 * @self: a #MetaWindowActor
 *
 * Gets the ClutterActor that is used to display the contents of the window,
 * or NULL if no texture is shown yet, because the window is not mapped.
 *
 * Return value: (transfer none): the #ClutterActor for the contents
 */
ClutterActor *
meta_window_actor_get_texture (MetaWindowActor *self)
{
  if (self->priv->surface)
    return CLUTTER_ACTOR (meta_surface_actor_get_texture (self->priv->surface));
  else
    return NULL;
}

/**
 * meta_window_actor_get_surface:
 * @self: a #MetaWindowActor
 *
 * Gets the MetaSurfaceActor that draws the content of this window,
 * or NULL if there is no surface yet associated with this window.
 *
 * Return value: (transfer none): the #MetaSurfaceActor for the contents
 */
MetaSurfaceActor *
meta_window_actor_get_surface (MetaWindowActor *self)
{
  return self->priv->surface;
}

/**
 * meta_window_actor_is_destroyed:
 * @self: a #MetaWindowActor
 *
 * Gets whether the X window that the actor was displaying has been destroyed
 *
 * Return value: %TRUE when the window is destroyed, otherwise %FALSE
 */
gboolean
meta_window_actor_is_destroyed (MetaWindowActor *self)
{
  return self->priv->disposed;
}

static gboolean
send_frame_messages_timeout (gpointer data)
{
  MetaWindowActor *self = (MetaWindowActor *) data;
  MetaWindowActorPrivate *priv = self->priv;
  FrameData *frame = g_slice_new0 (FrameData);

  frame->sync_request_serial = priv->window->sync_request_serial;

  do_send_frame_drawn (self, frame);
  do_send_frame_timings (self, frame, 0, 0);

  priv->needs_frame_drawn = FALSE;
  priv->send_frame_messages_timer = 0;
  frame_data_free (frame);

  return FALSE;
}

static void
queue_send_frame_messages_timeout (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaDisplay *display = meta_window_get_display (priv->window);
  gint64 current_time = meta_compositor_monotonic_time_to_server_time (display, g_get_monotonic_time ());
  MetaMonitorManager *monitor_manager = meta_monitor_manager_get ();
  MetaWindow *window = priv->window;

  MetaOutput *outputs;
  guint n_outputs, i;
  float refresh_rate = 60.0f;
  gint interval, offset;

  outputs = meta_monitor_manager_get_outputs (monitor_manager, &n_outputs);
  for (i = 0; i < n_outputs; i++)
    {
      if (outputs[i].output_id == window->monitor->output_id && outputs[i].crtc)
        {
          refresh_rate = outputs[i].crtc->current_mode->refresh_rate;
          break;
        }
    }

  interval = (int)(1000000 / refresh_rate) * 6;
  offset = MAX (0, priv->frame_drawn_time + interval - current_time) / 1000;

 /* The clutter master clock source has already been added with META_PRIORITY_REDRAW,
  * so the timer will run *after* the clutter frame handling, if a frame is ready
  * to be drawn when the timer expires.
  */
  priv->send_frame_messages_timer = g_timeout_add_full (META_PRIORITY_REDRAW, offset, send_frame_messages_timeout, self, NULL);
  g_source_set_name_by_id (priv->send_frame_messages_timer, "[mutter] send_frame_messages_timeout");
}

void
meta_window_actor_queue_frame_drawn (MetaWindowActor *self,
                                     gboolean         no_delay_frame)
{
  MetaWindowActorPrivate *priv = self->priv;
  FrameData *frame = g_slice_new0 (FrameData);

  priv->needs_frame_drawn = TRUE;

  frame->sync_request_serial = priv->window->sync_request_serial;

  priv->frames = g_list_prepend (priv->frames, frame);

  if (no_delay_frame)
    {
      ClutterActor *stage = clutter_actor_get_stage (CLUTTER_ACTOR (self));
      clutter_stage_skip_sync_delay (CLUTTER_STAGE (stage));
    }

  if (!priv->repaint_scheduled)
    {
      gboolean is_obscured;

      if (priv->surface)
        is_obscured = meta_surface_actor_is_obscured (priv->surface);
      else
        is_obscured = FALSE;

      /* A frame was marked by the client without actually doing any
       * damage or any unobscured, or while we had the window frozen
       * (e.g. during an interactive resize.) We need to make sure that the
       * pre_paint/post_paint functions get called, enabling us to
       * send a _NET_WM_FRAME_DRAWN. We do a 1-pixel redraw to get
       * consistent timing with non-empty frames. If the window
       * is completely obscured we fire off the send_frame_messages timeout.
       */
      if (is_obscured)
        {
          queue_send_frame_messages_timeout (self);
        }
      else
        {
          if (priv->surface)
            {
              const cairo_rectangle_int_t clip = { 0, 0, 1, 1 };
              clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (priv->surface), &clip);
              priv->repaint_scheduled = TRUE;
            }
        }
    }
}

gboolean
meta_window_actor_effect_in_progress (MetaWindowActor *self)
{
  return (self->priv->minimize_in_progress ||
	  self->priv->maximize_in_progress ||
	  self->priv->unmaximize_in_progress ||
	  self->priv->map_in_progress ||
	  self->priv->destroy_in_progress);
}

static gboolean
is_freeze_thaw_effect (gulong event)
{
  switch (event)
  {
  case META_PLUGIN_DESTROY:
  case META_PLUGIN_MAXIMIZE:
  case META_PLUGIN_UNMAXIMIZE:
    return TRUE;
    break;
  default:
    return FALSE;
  }
}

static gboolean
start_simple_effect (MetaWindowActor *self,
                     gulong        event)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaCompositor *compositor = priv->compositor;
  gint *counter = NULL;
  gboolean use_freeze_thaw = FALSE;

  switch (event)
  {
  case META_PLUGIN_MINIMIZE:
    counter = &priv->minimize_in_progress;
    break;
  case META_PLUGIN_MAP:
    counter = &priv->map_in_progress;
    break;
  case META_PLUGIN_DESTROY:
    counter = &priv->destroy_in_progress;
    break;
  case META_PLUGIN_UNMAXIMIZE:
  case META_PLUGIN_MAXIMIZE:
  case META_PLUGIN_SWITCH_WORKSPACE:
    g_assert_not_reached ();
    break;
  }

  g_assert (counter);

  use_freeze_thaw = is_freeze_thaw_effect (event);

  if (use_freeze_thaw)
    meta_window_actor_freeze (self);

  (*counter)++;

  if (!meta_plugin_manager_event_simple (compositor->plugin_mgr, self, event))
    {
      (*counter)--;
      if (use_freeze_thaw)
        meta_window_actor_thaw (self);
      return FALSE;
    }

  return TRUE;
}

static void
meta_window_actor_after_effects (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  if (priv->needs_destroy)
    {
      clutter_actor_destroy (CLUTTER_ACTOR (self));
      return;
    }

  meta_window_actor_sync_visibility (self);
  meta_window_actor_sync_actor_geometry (self, FALSE);
}

void
meta_window_actor_effect_completed (MetaWindowActor *self,
                                    gulong           event)
{
  MetaWindowActorPrivate *priv   = self->priv;

  /* NB: Keep in mind that when effects get completed it possible
   * that the corresponding MetaWindow may have be been destroyed.
   * In this case priv->window will == NULL */

  switch (event)
  {
  case META_PLUGIN_MINIMIZE:
    {
      priv->minimize_in_progress--;
      if (priv->minimize_in_progress < 0)
	{
	  g_warning ("Error in minimize accounting.");
	  priv->minimize_in_progress = 0;
	}
    }
    break;
  case META_PLUGIN_MAP:
    /*
     * Make sure that the actor is at the correct place in case
     * the plugin fscked.
     */
    priv->map_in_progress--;

    if (priv->map_in_progress < 0)
      {
	g_warning ("Error in map accounting.");
	priv->map_in_progress = 0;
      }
    break;
  case META_PLUGIN_DESTROY:
    priv->destroy_in_progress--;

    if (priv->destroy_in_progress < 0)
      {
	g_warning ("Error in destroy accounting.");
	priv->destroy_in_progress = 0;
      }
    break;
  case META_PLUGIN_UNMAXIMIZE:
    priv->unmaximize_in_progress--;
    if (priv->unmaximize_in_progress < 0)
      {
	g_warning ("Error in unmaximize accounting.");
	priv->unmaximize_in_progress = 0;
      }
    break;
  case META_PLUGIN_MAXIMIZE:
    priv->maximize_in_progress--;
    if (priv->maximize_in_progress < 0)
      {
	g_warning ("Error in maximize accounting.");
	priv->maximize_in_progress = 0;
      }
    break;
  case META_PLUGIN_SWITCH_WORKSPACE:
    g_assert_not_reached ();
    break;
  }

  if (is_freeze_thaw_effect (event))
    meta_window_actor_thaw (self);

  if (!meta_window_actor_effect_in_progress (self))
    meta_window_actor_after_effects (self);
}

gboolean
meta_window_actor_should_unredirect (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;
  if (priv->surface)
    return meta_surface_actor_should_unredirect (priv->surface);
  else
    return FALSE;
}

void
meta_window_actor_set_unredirected (MetaWindowActor *self,
                                    gboolean         unredirected)
{
  MetaWindowActorPrivate *priv = self->priv;

  g_assert(priv->surface); /* because otherwise should_unredirect() is FALSE */
  meta_surface_actor_set_unredirected (priv->surface, unredirected);
}

void
meta_window_actor_destroy (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaWindow *window = priv->window;
  MetaCompositor *compositor = priv->compositor;
  MetaWindowType window_type = meta_window_get_window_type (window);

  meta_window_set_compositor_private (window, NULL);

  if (priv->send_frame_messages_timer != 0)
    {
      g_source_remove (priv->send_frame_messages_timer);
      priv->send_frame_messages_timer = 0;
    }

  /*
   * We remove the window from internal lookup hashes and thus any other
   * unmap events etc fail
   */
  compositor->windows = g_list_remove (compositor->windows, (gconstpointer) self);

  if (window_type == META_WINDOW_DROPDOWN_MENU ||
      window_type == META_WINDOW_POPUP_MENU ||
      window_type == META_WINDOW_TOOLTIP ||
      window_type == META_WINDOW_NOTIFICATION ||
      window_type == META_WINDOW_COMBO ||
      window_type == META_WINDOW_DND ||
      window_type == META_WINDOW_OVERRIDE_OTHER)
    {
      /*
       * No effects, just kill it.
       */
      clutter_actor_destroy (CLUTTER_ACTOR (self));
      return;
    }

  priv->needs_destroy = TRUE;

  if (!meta_window_actor_effect_in_progress (self))
    clutter_actor_destroy (CLUTTER_ACTOR (self));
}

void
meta_window_actor_sync_actor_geometry (MetaWindowActor *self,
                                       gboolean         did_placement)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaRectangle window_rect;

  meta_window_get_buffer_rect (priv->window, &window_rect);

  /* When running as a Wayland compositor we catch size changes when new
   * buffers are attached */
  if (META_IS_SURFACE_ACTOR_X11 (priv->surface))
    meta_surface_actor_x11_set_size (META_SURFACE_ACTOR_X11 (priv->surface),
                                     window_rect.width, window_rect.height);

  /* Normally we want freezing a window to also freeze its position; this allows
   * windows to atomically move and resize together, either under app control,
   * or because the user is resizing from the left/top. But on initial placement
   * we need to assign a position, since immediately after the window
   * is shown, the map effect will go into effect and prevent further geometry
   * updates.
   */
  if (is_frozen (self) && !did_placement)
    return;

  if (meta_window_actor_effect_in_progress (self))
    return;

  clutter_actor_set_position (CLUTTER_ACTOR (self),
                              window_rect.x, window_rect.y);
  clutter_actor_set_size (CLUTTER_ACTOR (self),
                          window_rect.width, window_rect.height);
}

void
meta_window_actor_show (MetaWindowActor   *self,
                        MetaCompEffect     effect)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaCompositor *compositor = priv->compositor;
  gulong event = 0;

  g_return_if_fail (!priv->visible);

  self->priv->visible = TRUE;

  switch (effect)
    {
    case META_COMP_EFFECT_CREATE:
      event = META_PLUGIN_MAP;
      break;
    case META_COMP_EFFECT_UNMINIMIZE:
      /* FIXME: should have META_PLUGIN_UNMINIMIZE */
      event = META_PLUGIN_MAP;
      break;
    case META_COMP_EFFECT_NONE:
      break;
    case META_COMP_EFFECT_DESTROY:
    case META_COMP_EFFECT_MINIMIZE:
      g_assert_not_reached();
    }

  if (compositor->switch_workspace_in_progress ||
      event == 0 ||
      !start_simple_effect (self, event))
    {
      clutter_actor_show (CLUTTER_ACTOR (self));
    }
}

void
meta_window_actor_hide (MetaWindowActor *self,
                        MetaCompEffect   effect)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaCompositor *compositor = priv->compositor;
  gulong event = 0;

  g_return_if_fail (priv->visible);

  priv->visible = FALSE;

  /* If a plugin is animating a workspace transition, we have to
   * hold off on hiding the window, and do it after the workspace
   * switch completes
   */
  if (compositor->switch_workspace_in_progress)
    return;

  switch (effect)
    {
    case META_COMP_EFFECT_DESTROY:
      event = META_PLUGIN_DESTROY;
      break;
    case META_COMP_EFFECT_MINIMIZE:
      event = META_PLUGIN_MINIMIZE;
      break;
    case META_COMP_EFFECT_NONE:
      break;
    case META_COMP_EFFECT_UNMINIMIZE:
    case META_COMP_EFFECT_CREATE:
      g_assert_not_reached();
    }

  if (event == 0 ||
      !start_simple_effect (self, event))
    clutter_actor_hide (CLUTTER_ACTOR (self));
}

void
meta_window_actor_maximize (MetaWindowActor    *self,
                            MetaRectangle      *old_rect,
                            MetaRectangle      *new_rect)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaCompositor *compositor = priv->compositor;

  /* The window has already been resized (in order to compute new_rect),
   * which by side effect caused the actor to be resized. Restore it to the
   * old size and position */
  clutter_actor_set_position (CLUTTER_ACTOR (self), old_rect->x, old_rect->y);
  clutter_actor_set_size (CLUTTER_ACTOR (self), old_rect->width, old_rect->height);

  self->priv->maximize_in_progress++;
  meta_window_actor_freeze (self);

  if (!meta_plugin_manager_event_maximize (compositor->plugin_mgr,
                                           self,
                                           META_PLUGIN_MAXIMIZE,
                                           new_rect->x, new_rect->y,
                                           new_rect->width, new_rect->height))

    {
      self->priv->maximize_in_progress--;
      meta_window_actor_thaw (self);
    }
}

void
meta_window_actor_unmaximize (MetaWindowActor   *self,
                              MetaRectangle     *old_rect,
                              MetaRectangle     *new_rect)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaCompositor *compositor = priv->compositor;

  /* The window has already been resized (in order to compute new_rect),
   * which by side effect caused the actor to be resized. Restore it to the
   * old size and position */
  clutter_actor_set_position (CLUTTER_ACTOR (self), old_rect->x, old_rect->y);
  clutter_actor_set_size (CLUTTER_ACTOR (self), old_rect->width, old_rect->height);

  self->priv->unmaximize_in_progress++;
  meta_window_actor_freeze (self);

  if (!meta_plugin_manager_event_maximize (compositor->plugin_mgr,
                                           self,
                                           META_PLUGIN_UNMAXIMIZE,
                                           new_rect->x, new_rect->y,
                                           new_rect->width, new_rect->height))
    {
      self->priv->unmaximize_in_progress--;
      meta_window_actor_thaw (self);
    }
}

MetaWindowActor *
meta_window_actor_new (MetaWindow *window)
{
  MetaDisplay *display = meta_window_get_display (window);
  MetaCompositor *compositor = display->compositor;
  MetaWindowActor        *self;
  MetaWindowActorPrivate *priv;
  ClutterActor           *window_group;

  self = g_object_new (META_TYPE_WINDOW_ACTOR,
                       "meta-window", window,
                       NULL);

  priv = self->priv;

  meta_window_actor_sync_updates_frozen (self);

  /* If a window doesn't start off with updates frozen, we should
   * we should send a _NET_WM_FRAME_DRAWN immediately after the first drawn.
   */
  if (priv->window->extended_sync_request_counter && !priv->updates_frozen)
    meta_window_actor_queue_frame_drawn (self, FALSE);

  meta_window_actor_sync_actor_geometry (self, priv->window->placed);

  /* Hang our compositor window state off the MetaWindow for fast retrieval */
  meta_window_set_compositor_private (window, G_OBJECT (self));

  if (window->layer == META_LAYER_OVERRIDE_REDIRECT)
    window_group = compositor->top_window_group;
  else
    window_group = compositor->window_group;

  clutter_actor_add_child (window_group, CLUTTER_ACTOR (self));

  clutter_actor_hide (CLUTTER_ACTOR (self));

  /* Initial position in the stack is arbitrary; stacking will be synced
   * before we first paint.
   */
  compositor->windows = g_list_append (compositor->windows, self);

  return self;
}

#if 0
/* Print out a region; useful for debugging */
static void
print_region (cairo_region_t *region)
{
  int n_rects;
  int i;

  n_rects = cairo_region_num_rectangles (region);
  g_print ("[");
  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (region, i, &rect);
      g_print ("+%d+%dx%dx%d ",
               rect.x, rect.y, rect.width, rect.height);
    }
  g_print ("]\n");
}
#endif

#if 0
/* Dump a region to a PNG file; useful for debugging */
static void
see_region (cairo_region_t *region,
            int             width,
            int             height,
            char           *filename)
{
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_A8, width, height);
  cairo_t *cr = cairo_create (surface);

  gdk_cairo_region (cr, region);
  cairo_fill (cr);

  cairo_surface_write_to_png (surface, filename);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);
}
#endif

/**
 * meta_window_actor_set_clip_region_beneath:
 * @self: a #MetaWindowActor
 * @clip_region: the region of the screen that isn't completely
 *  obscured beneath the main window texture.
 *
 * Provides a hint as to what areas need to be drawn *beneath*
 * the main window texture.  This is the relevant clip region
 * when drawing the shadow, properly accounting for areas of the
 * shadow hid by the window itself. This will be set before painting
 * then unset afterwards.
 */
static void
meta_window_actor_set_clip_region_beneath (MetaWindowActor *self,
                                           cairo_region_t  *beneath_region)
{
  MetaWindowActorPrivate *priv = self->priv;
  gboolean appears_focused = meta_window_appears_focused (priv->window);

  if (appears_focused ? priv->focused_shadow : priv->unfocused_shadow)
    {
      g_clear_pointer (&priv->shadow_clip, cairo_region_destroy);

      if (beneath_region)
        {
          priv->shadow_clip = cairo_region_copy (beneath_region);

          if (clip_shadow_under_window (self))
            {
              cairo_region_t *frame_bounds = meta_window_get_frame_bounds (priv->window);
              cairo_region_subtract (priv->shadow_clip, frame_bounds);
            }
        }
      else
        priv->shadow_clip = NULL;
    }
}

static void
meta_window_actor_cull_out (MetaCullable   *cullable,
                            cairo_region_t *unobscured_region,
                            cairo_region_t *clip_region)
{
  MetaWindowActor *self = META_WINDOW_ACTOR (cullable);

  meta_cullable_cull_out_children (cullable, unobscured_region, clip_region);
  meta_window_actor_set_clip_region_beneath (self, clip_region);
}

static void
meta_window_actor_reset_culling (MetaCullable *cullable)
{
  MetaWindowActor *self = META_WINDOW_ACTOR (cullable);
  MetaWindowActorPrivate *priv = self->priv;

  g_clear_pointer (&priv->shadow_clip, cairo_region_destroy);

  meta_cullable_reset_culling_children (cullable);
}

static void
cullable_iface_init (MetaCullableInterface *iface)
{
  iface->cull_out = meta_window_actor_cull_out;
  iface->reset_culling = meta_window_actor_reset_culling;
}

static void
check_needs_shadow (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaShadow *old_shadow = NULL;
  MetaShadow **shadow_location;
  gboolean recompute_shadow;
  gboolean should_have_shadow;
  gboolean appears_focused;

  /* Calling meta_window_actor_has_shadow() here at every pre-paint is cheap
   * and avoids the need to explicitly handle window type changes, which
   * we would do if tried to keep track of when we might be adding or removing
   * a shadow more explicitly. We only keep track of changes to the *shape* of
   * the shadow with priv->recompute_shadow.
   */

  should_have_shadow = meta_window_actor_has_shadow (self);
  appears_focused = meta_window_appears_focused (priv->window);

  if (appears_focused)
    {
      recompute_shadow = priv->recompute_focused_shadow;
      priv->recompute_focused_shadow = FALSE;
      shadow_location = &priv->focused_shadow;
    }
  else
    {
      recompute_shadow = priv->recompute_unfocused_shadow;
      priv->recompute_unfocused_shadow = FALSE;
      shadow_location = &priv->unfocused_shadow;
    }

  if (!should_have_shadow || recompute_shadow)
    {
      if (*shadow_location != NULL)
        {
          old_shadow = *shadow_location;
          *shadow_location = NULL;
        }
    }

  if (*shadow_location == NULL && should_have_shadow)
    {
      if (priv->shadow_shape == NULL)
        priv->shadow_shape = meta_window_shape_new (priv->shape_region);

      MetaShadowFactory *factory = meta_shadow_factory_get_default ();
      const char *shadow_class = meta_window_actor_get_shadow_class (self);
      cairo_rectangle_int_t shape_bounds;

      meta_window_actor_get_shape_bounds (self, &shape_bounds);
      *shadow_location = meta_shadow_factory_get_shadow (factory,
                                                         priv->shadow_shape,
                                                         shape_bounds.width, shape_bounds.height,
                                                         shadow_class, appears_focused);
    }

  if (old_shadow != NULL)
    meta_shadow_unref (old_shadow);
}

void
meta_window_actor_process_x11_damage (MetaWindowActor    *self,
                                      XDamageNotifyEvent *event)
{
  MetaWindowActorPrivate *priv = self->priv;

  if (priv->surface)
    meta_surface_actor_process_damage (priv->surface,
                                       event->area.x,
                                       event->area.y,
                                       event->area.width,
                                       event->area.height);
}

void
meta_window_actor_sync_visibility (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  if (CLUTTER_ACTOR_IS_VISIBLE (self) != priv->visible)
    {
      if (priv->visible)
        clutter_actor_show (CLUTTER_ACTOR (self));
      else
        clutter_actor_hide (CLUTTER_ACTOR (self));
    }
}

static cairo_region_t *
scan_visible_region (guchar         *mask_data,
                     int             stride,
                     cairo_region_t *scan_area)
{
  int i, n_rects = cairo_region_num_rectangles (scan_area);
  MetaRegionBuilder builder;

  meta_region_builder_init (&builder);

  for (i = 0; i < n_rects; i++)
    {
      int x, y;
      cairo_rectangle_int_t rect;

      cairo_region_get_rectangle (scan_area, i, &rect);

      for (y = rect.y; y < (rect.y + rect.height); y++)
        {
          for (x = rect.x; x < (rect.x + rect.width); x++)
            {
              int x2 = x;
              while (mask_data[y * stride + x2] == 255 && x2 < (rect.x + rect.width))
                x2++;

              if (x2 > x)
                {
                  meta_region_builder_add_rectangle (&builder, x, y, x2 - x, 1);
                  x = x2;
                }
            }
        }
    }

  return meta_region_builder_finish (&builder);
}

static void
build_and_scan_frame_mask (MetaWindowActor       *self,
                           cairo_rectangle_int_t *client_area,
                           cairo_region_t        *shape_region)
{
  MetaWindowActorPrivate *priv = self->priv;
  guchar *mask_data;
  guint tex_width, tex_height;
  MetaShapedTexture *stex;
  CoglTexture *paint_tex, *mask_texture;
  int stride;
  cairo_t *cr;
  cairo_surface_t *surface;

  stex = meta_surface_actor_get_texture (priv->surface);
  g_return_if_fail (stex);

  meta_shaped_texture_set_mask_texture (stex, NULL);

  paint_tex = meta_shaped_texture_get_texture (stex);
  if (paint_tex == NULL)
    return;

  tex_width = cogl_texture_get_width (paint_tex);
  tex_height = cogl_texture_get_height (paint_tex);

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_A8, tex_width);

  /* Create data for an empty image */
  mask_data = g_malloc0 (stride * tex_height);

  surface = cairo_image_surface_create_for_data (mask_data,
                                                 CAIRO_FORMAT_A8,
                                                 tex_width,
                                                 tex_height,
                                                 stride);
  cr = cairo_create (surface);

  gdk_cairo_region (cr, shape_region);
  cairo_fill (cr);

  if (priv->window->frame != NULL)
    {
      cairo_region_t *frame_paint_region, *scanned_region;
      cairo_rectangle_int_t rect = { 0, 0, tex_width, tex_height };

      /* Make sure we don't paint the frame over the client window. */
      frame_paint_region = cairo_region_create_rectangle (&rect);
      cairo_region_subtract_rectangle (frame_paint_region, client_area);

      gdk_cairo_region (cr, frame_paint_region);
      cairo_clip (cr);

      meta_frame_get_mask (priv->window->frame, cr);

      cairo_surface_flush (surface);
      scanned_region = scan_visible_region (mask_data, stride, frame_paint_region);
      cairo_region_union (shape_region, scanned_region);
      cairo_region_destroy (scanned_region);
      cairo_region_destroy (frame_paint_region);
    }

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  if (meta_texture_rectangle_check (paint_tex))
    {
      ClutterBackend *backend = clutter_get_default_backend ();
      CoglContext *context = clutter_backend_get_cogl_context (backend);

      mask_texture = COGL_TEXTURE (cogl_texture_rectangle_new_with_size (context, tex_width, tex_height));
      cogl_texture_set_components (mask_texture, COGL_TEXTURE_COMPONENTS_A);
      cogl_texture_set_region (mask_texture,
                               0, 0, /* src_x/y */
                               0, 0, /* dst_x/y */
                               tex_width, tex_height, /* dst_width/height */
                               tex_width, tex_height, /* width/height */
                               COGL_PIXEL_FORMAT_A_8,
                               stride, mask_data);
    }
  else
    {
      /* Note: we don't allow slicing for this texture because we
       * need to use it with multi-texturing which doesn't support
       * sliced textures */
      mask_texture = cogl_texture_new_from_data (tex_width, tex_height,
                                                 COGL_TEXTURE_NO_SLICING,
                                                 COGL_PIXEL_FORMAT_A_8,
                                                 COGL_PIXEL_FORMAT_ANY,
                                                 stride,
                                                 mask_data);
    }

  meta_shaped_texture_set_mask_texture (stex, mask_texture);
  if (mask_texture)
    cogl_object_unref (mask_texture);

  g_free (mask_data);
}

static void
meta_window_actor_update_shape_region (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;
  cairo_region_t *region = NULL;
  cairo_rectangle_int_t client_area;

  meta_window_get_client_area_rect (priv->window, &client_area);

  if (priv->window->frame != NULL && priv->window->shape_region != NULL)
    {
      region = cairo_region_copy (priv->window->shape_region);
      cairo_region_translate (region, client_area.x, client_area.y);
    }
  else if (priv->window->shape_region != NULL)
    {
      region = cairo_region_reference (priv->window->shape_region);
    }
  else
    {
      /* If we don't have a shape on the server, that means that
       * we have an implicit shape of one rectangle covering the
       * entire window. */
      region = cairo_region_create_rectangle (&client_area);
    }

  if ((priv->window->shape_region != NULL) || (priv->window->frame != NULL))
    build_and_scan_frame_mask (self, &client_area, region);

  g_clear_pointer (&priv->shape_region, cairo_region_destroy);
  priv->shape_region = region;

  g_clear_pointer (&priv->shadow_shape, meta_window_shape_unref);

  meta_window_actor_invalidate_shadow (self);
}

static void
meta_window_actor_update_input_region (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaWindow *window = priv->window;
  cairo_region_t *region;

  if (window->shape_region && window->input_region)
    {
      region = cairo_region_copy (window->shape_region);
      cairo_region_intersect (region, window->input_region);
    }
  else if (window->shape_region)
    region = cairo_region_reference (window->shape_region);
  else if (window->input_region)
    region = cairo_region_reference (window->input_region);
  else
    region = NULL;

  meta_surface_actor_set_input_region (priv->surface, region);
  cairo_region_destroy (region);
}

static void
meta_window_actor_update_opaque_region (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;
  cairo_region_t *opaque_region;
  gboolean argb32 = is_argb32 (self);

  if (argb32 && priv->window->opaque_region != NULL)
    {
      cairo_rectangle_int_t client_area;

      meta_window_get_client_area_rect (priv->window, &client_area);

      /* The opaque region is defined to be a part of the
       * window which ARGB32 will always paint with opaque
       * pixels. For these regions, we want to avoid painting
       * windows and shadows beneath them.
       *
       * If the client gives bad coordinates where it does not
       * fully paint, the behavior is defined by the specification
       * to be undefined, and considered a client bug. In mutter's
       * case, graphical glitches will occur.
       */
      opaque_region = cairo_region_copy (priv->window->opaque_region);
      cairo_region_translate (opaque_region, client_area.x, client_area.y);
      cairo_region_intersect (opaque_region, priv->shape_region);
    }
  else if (argb32)
    opaque_region = NULL;
  else
    opaque_region = cairo_region_reference (priv->shape_region);

  meta_surface_actor_set_opaque_region (priv->surface, opaque_region);
  cairo_region_destroy (opaque_region);
}

static void
check_needs_reshape (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  if (!priv->needs_reshape)
    return;

  meta_window_actor_update_shape_region (self);

  if (priv->window->client_type == META_WINDOW_CLIENT_TYPE_X11)
    {
      meta_window_actor_update_input_region (self);
      meta_window_actor_update_opaque_region (self);
    }

  priv->needs_reshape = FALSE;
}

void
meta_window_actor_update_shape (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  priv->needs_reshape = TRUE;

  if (is_frozen (self))
    return;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (priv->surface));
}

static void
meta_window_actor_handle_updates (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  if (is_frozen (self))
    {
      /* The window is frozen due to a pending animation: we'll wait until
       * the animation finishes to reshape and repair the window */
      return;
    }

  if (meta_surface_actor_is_unredirected (priv->surface))
    return;

  meta_surface_actor_pre_paint (priv->surface);

  if (!meta_surface_actor_is_visible (priv->surface))
    return;

  check_needs_reshape (self);
  check_needs_shadow (self);
}

void
meta_window_actor_pre_paint (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;
  GList *l;

  meta_window_actor_handle_updates (self);

  for (l = priv->frames; l != NULL; l = l->next)
    {
      FrameData *frame = l->data;

      if (frame->frame_counter == 0)
        {
          CoglOnscreen *onscreen = COGL_ONSCREEN (cogl_get_draw_framebuffer());
          frame->frame_counter = cogl_onscreen_get_frame_counter (onscreen);
        }
    }
}

static void
do_send_frame_drawn (MetaWindowActor *self, FrameData *frame)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaDisplay *display = meta_window_get_display (priv->window);
  Display *xdisplay = meta_display_get_xdisplay (display);

  XClientMessageEvent ev = { 0, };

  frame->frame_drawn_time = meta_compositor_monotonic_time_to_server_time (display,
                                                                           g_get_monotonic_time ());
  priv->frame_drawn_time = frame->frame_drawn_time;

  ev.type = ClientMessage;
  ev.window = meta_window_get_xwindow (priv->window);
  ev.message_type = display->atom__NET_WM_FRAME_DRAWN;
  ev.format = 32;
  ev.data.l[0] = frame->sync_request_serial & G_GUINT64_CONSTANT(0xffffffff);
  ev.data.l[1] = frame->sync_request_serial >> 32;
  ev.data.l[2] = frame->frame_drawn_time & G_GUINT64_CONSTANT(0xffffffff);
  ev.data.l[3] = frame->frame_drawn_time >> 32;

  meta_error_trap_push (display);
  XSendEvent (xdisplay, ev.window, False, 0, (XEvent*) &ev);
  XFlush (xdisplay);
  meta_error_trap_pop (display);
}

void
meta_window_actor_post_paint (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  priv->repaint_scheduled = FALSE;

 /* This window had damage, but wasn't actually redrawn because
  * it is obscured. So we should wait until timer expiration
  * before sending _NET_WM_FRAME_* messages.
  */
  if (priv->send_frame_messages_timer != 0)
    return;

  if (priv->needs_frame_drawn)
    {
      do_send_frame_drawn (self, priv->frames->data);
      priv->needs_frame_drawn = FALSE;
    }
}

static void
do_send_frame_timings (MetaWindowActor  *self,
                       FrameData        *frame,
                       gint             refresh_interval,
                       gint64           presentation_time)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaDisplay *display = meta_window_get_display (priv->window);
  Display *xdisplay = meta_display_get_xdisplay (display);

  XClientMessageEvent ev = { 0, };

  ev.type = ClientMessage;
  ev.window = meta_window_get_xwindow (priv->window);
  ev.message_type = display->atom__NET_WM_FRAME_TIMINGS;
  ev.format = 32;
  ev.data.l[0] = frame->sync_request_serial & G_GUINT64_CONSTANT(0xffffffff);
  ev.data.l[1] = frame->sync_request_serial >> 32;

  if (presentation_time != 0)
    {
      gint64 presentation_time_server = meta_compositor_monotonic_time_to_server_time (display,
                                                                                       presentation_time);
      gint64 presentation_time_offset = presentation_time_server - frame->frame_drawn_time;
      if (presentation_time_offset == 0)
        presentation_time_offset = 1;

      if ((gint32)presentation_time_offset == presentation_time_offset)
        ev.data.l[2] = presentation_time_offset;
    }

  ev.data.l[3] = refresh_interval;
  ev.data.l[4] = 1000 * META_SYNC_DELAY;

  meta_error_trap_push (display);
  XSendEvent (xdisplay, ev.window, False, 0, (XEvent*) &ev);
  XFlush (xdisplay);
  meta_error_trap_pop (display);
}

static void
send_frame_timings (MetaWindowActor  *self,
                    FrameData        *frame,
                    CoglFrameInfo    *frame_info,
                    gint64            presentation_time)
{
  float refresh_rate;
  int refresh_interval;

  refresh_rate = cogl_frame_info_get_refresh_rate (frame_info);
  /* 0.0 is a flag for not known, but sanity-check against other odd numbers */
  if (refresh_rate >= 1.0)
    refresh_interval = (int) (0.5 + 1000000 / refresh_rate);
  else
    refresh_interval = 0;

  do_send_frame_timings (self, frame, refresh_interval, presentation_time);
}

void
meta_window_actor_frame_complete (MetaWindowActor *self,
                                  CoglFrameInfo   *frame_info,
                                  gint64           presentation_time)
{
  MetaWindowActorPrivate *priv = self->priv;
  GList *l;

  for (l = priv->frames; l;)
    {
      GList *l_next = l->next;
      FrameData *frame = l->data;

      if (frame->frame_counter == cogl_frame_info_get_frame_counter (frame_info))
        {
          if (frame->frame_drawn_time != 0)
            {
              priv->frames = g_list_delete_link (priv->frames, l);
              send_frame_timings (self, frame, frame_info, presentation_time);
              frame_data_free (frame);
            }
        }

      l = l_next;
    }
}

void
meta_window_actor_invalidate_shadow (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;

  priv->recompute_focused_shadow = TRUE;
  priv->recompute_unfocused_shadow = TRUE;

  if (is_frozen (self))
    return;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (self));
}

void
meta_window_actor_update_opacity (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaWindow *window = priv->window;

  if (priv->surface)
    clutter_actor_set_opacity (CLUTTER_ACTOR (priv->surface), window->opacity);
}

static void
meta_window_actor_set_updates_frozen (MetaWindowActor *self,
                                      gboolean         updates_frozen)
{
  MetaWindowActorPrivate *priv = self->priv;

  updates_frozen = updates_frozen != FALSE;

  if (priv->updates_frozen != updates_frozen)
    {
      priv->updates_frozen = updates_frozen;
      if (updates_frozen)
        meta_window_actor_freeze (self);
      else
        meta_window_actor_thaw (self);
    }
}

void
meta_window_actor_sync_updates_frozen (MetaWindowActor *self)
{
  MetaWindowActorPrivate *priv = self->priv;
  MetaWindow *window = priv->window;

  meta_window_actor_set_updates_frozen (self, meta_window_updates_are_frozen (window));
}
