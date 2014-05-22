/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:clutter-stage
 * @short_description: Top level visual element to which actors are placed.
 *
 * #ClutterStage is a top level 'window' on which child actors are placed
 * and manipulated.
 *
 * Backends might provide support for multiple stages. The support for this
 * feature can be checked at run-time using the clutter_feature_available()
 * function and the %CLUTTER_FEATURE_STAGE_MULTIPLE flag. If the backend used
 * supports multiple stages, new #ClutterStage instances can be created
 * using clutter_stage_new(). These stages must be managed by the developer
 * using clutter_actor_destroy(), which will take care of destroying all the
 * actors contained inside them.
 *
 * #ClutterStage is a proxy actor, wrapping the backend-specific
 * implementation of the windowing system. It is possible to subclass
 * #ClutterStage, as long as every overridden virtual function chains up to
 * the parent class corresponding function.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <cairo.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include "clutter-stage.h"
#include "deprecated/clutter-stage.h"
#include "deprecated/clutter-container.h"

#include "clutter-actor-private.h"
#include "clutter-backend-private.h"
#include "clutter-cairo.h"
#include "clutter-color.h"
#include "clutter-container.h"
#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-enum-types.h"
#include "clutter-event-private.h"
#include "clutter-id-pool.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-master-clock.h"
#include "clutter-paint-volume-private.h"
#include "clutter-private.h"
#include "clutter-profile.h"
#include "clutter-stage-manager-private.h"
#include "clutter-stage-private.h"
#include "clutter-version.h" 	/* For flavour */
#include "clutter-private.h"

#include "cogl/cogl.h"

/* <private>
 * ClutterStageHint:
 * @CLUTTER_STAGE_NONE: No hint set
 * @CLUTTER_STAGE_NO_CLEAR_ON_PAINT: When this hint is set, the stage
 *   should not clear the viewport; this flag is useful when painting
 *   fully opaque actors covering the whole visible area of the stage,
 *   i.e. when no blending with the stage color happens over the whole
 *   stage viewport
 *
 * A series of hints that enable or disable behaviours on the stage
 */
typedef enum { /*< prefix=CLUTTER_STAGE >*/
  CLUTTER_STAGE_HINT_NONE = 0,

  CLUTTER_STAGE_NO_CLEAR_ON_PAINT = 1 << 0
} ClutterStageHint;

#define STAGE_NO_CLEAR_ON_PAINT(s)      ((((ClutterStage *) (s))->priv->stage_hints & CLUTTER_STAGE_NO_CLEAR_ON_PAINT) != 0)

struct _ClutterStageQueueRedrawEntry
{
  ClutterActor *actor;
  gboolean has_clip;
  ClutterPaintVolume clip;
};

struct _ClutterStagePrivate
{
  /* the stage implementation */
  ClutterStageWindow *impl;

  ClutterPerspective perspective;
  CoglMatrix projection;
  CoglMatrix inverse_projection;
  CoglMatrix view;
  float viewport[4];

  ClutterFog fog;

  gchar *title;
  ClutterActor *key_focused_actor;

  GQueue *event_queue;

  ClutterStageHint stage_hints;

  GArray *paint_volume_stack;

  ClutterPlane current_clip_planes[4];

  GList *pending_queue_redraws;

  CoglFramebuffer *active_framebuffer;

  gint sync_delay;

  GTimer *fps_timer;
  gint32 timer_n_frames;

  ClutterIDPool *pick_id_pool;

#ifdef CLUTTER_ENABLE_DEBUG
  gulong redraw_count;
#endif /* CLUTTER_ENABLE_DEBUG */

  ClutterStageState current_state;

  ClutterStagePaintFunc paint_callback;
  gpointer paint_data;
  GDestroyNotify paint_notify;

  guint relayout_pending       : 1;
  guint redraw_pending         : 1;
  guint is_fullscreen          : 1;
  guint is_cursor_visible      : 1;
  guint is_user_resizable      : 1;
  guint use_fog                : 1;
  guint throttle_motion_events : 1;
  guint use_alpha              : 1;
  guint min_size_changed       : 1;
  guint dirty_viewport         : 1;
  guint dirty_projection       : 1;
  guint accept_focus           : 1;
  guint motion_events_enabled  : 1;
  guint has_custom_perspective : 1;
};

enum
{
  PROP_0,

  PROP_COLOR,
  PROP_FULLSCREEN_SET,
  PROP_OFFSCREEN,
  PROP_CURSOR_VISIBLE,
  PROP_PERSPECTIVE,
  PROP_TITLE,
  PROP_USER_RESIZABLE,
  PROP_USE_FOG,
  PROP_FOG,
  PROP_USE_ALPHA,
  PROP_KEY_FOCUS,
  PROP_NO_CLEAR_HINT,
  PROP_ACCEPT_FOCUS
};

enum
{
  FULLSCREEN,
  UNFULLSCREEN,
  ACTIVATE,
  DEACTIVATE,
  DELETE_EVENT,

  LAST_SIGNAL
};

static guint stage_signals[LAST_SIGNAL] = { 0, };

static const ClutterColor default_stage_color = { 255, 255, 255, 255 };

static void clutter_stage_maybe_finish_queue_redraws (ClutterStage *stage);
static void free_queue_redraw_entry (ClutterStageQueueRedrawEntry *entry);
static void clutter_stage_invoke_paint_callback (ClutterStage *stage);

static void clutter_container_iface_init (ClutterContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStage, clutter_stage, CLUTTER_TYPE_GROUP,
                         G_ADD_PRIVATE (ClutterStage)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init))

static void
clutter_stage_real_add (ClutterContainer *container,
                        ClutterActor     *child)
{
  clutter_actor_add_child (CLUTTER_ACTOR (container), child);
}

static void
clutter_stage_real_remove (ClutterContainer *container,
                           ClutterActor     *child)
{
  clutter_actor_remove_child (CLUTTER_ACTOR (container), child);
}

static void
clutter_stage_real_foreach (ClutterContainer *container,
                            ClutterCallback   callback,
                            gpointer          user_data)
{
  ClutterActorIter iter;
  ClutterActor *child;

  clutter_actor_iter_init (&iter, CLUTTER_ACTOR (container));

  while (clutter_actor_iter_next (&iter, &child))
    callback (child, user_data);
}

static void
clutter_stage_real_raise (ClutterContainer *container,
                          ClutterActor     *child,
                          ClutterActor     *sibling)
{
  clutter_actor_set_child_above_sibling (CLUTTER_ACTOR (container),
                                         child,
                                         sibling);
}

static void
clutter_stage_real_lower (ClutterContainer *container,
                          ClutterActor     *child,
                          ClutterActor     *sibling)
{
  clutter_actor_set_child_below_sibling (CLUTTER_ACTOR (container),
                                         child,
                                         sibling);
}

static void
clutter_stage_real_sort_depth_order (ClutterContainer *container)
{
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = clutter_stage_real_add;
  iface->remove = clutter_stage_real_remove;
  iface->foreach = clutter_stage_real_foreach;
  iface->raise = clutter_stage_real_raise;
  iface->lower = clutter_stage_real_lower;
  iface->sort_depth_order = clutter_stage_real_sort_depth_order;
}

static void
clutter_stage_get_preferred_width (ClutterActor *self,
                                   gfloat        for_height,
                                   gfloat       *min_width_p,
                                   gfloat       *natural_width_p)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  cairo_rectangle_int_t geom;

  if (priv->impl == NULL)
    return;

  _clutter_stage_window_get_geometry (priv->impl, &geom);

  if (min_width_p)
    *min_width_p = geom.width;

  if (natural_width_p)
    *natural_width_p = geom.width;
}

static void
clutter_stage_get_preferred_height (ClutterActor *self,
                                    gfloat        for_width,
                                    gfloat       *min_height_p,
                                    gfloat       *natural_height_p)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  cairo_rectangle_int_t geom;

  if (priv->impl == NULL)
    return;

  _clutter_stage_window_get_geometry (priv->impl, &geom);

  if (min_height_p)
    *min_height_p = geom.height;

  if (natural_height_p)
    *natural_height_p = geom.height;
}

static inline void
queue_full_redraw (ClutterStage *stage)
{
  ClutterStageWindow *stage_window;

  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  /* Just calling clutter_actor_queue_redraw will typically only
   * redraw the bounding box of the children parented on the stage but
   * in this case we really need to ensure that the full stage is
   * redrawn so we add a NULL redraw clip to the stage window. */
  stage_window = _clutter_stage_get_window (stage);
  if (stage_window == NULL)
    return;

  _clutter_stage_window_add_redraw_clip (stage_window, NULL);
}

static gboolean
stage_is_default (ClutterStage *stage)
{
  ClutterStageManager *stage_manager;
  ClutterStageWindow *impl;

  stage_manager = clutter_stage_manager_get_default ();
  if (stage != clutter_stage_manager_get_default_stage (stage_manager))
    return FALSE;

  impl = _clutter_stage_get_window (stage);
  if (impl != _clutter_stage_get_default_window ())
    return FALSE;

  return TRUE;
}

static void
clutter_stage_allocate (ClutterActor           *self,
                        const ClutterActorBox  *box,
                        ClutterAllocationFlags  flags)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  ClutterActorBox alloc = CLUTTER_ACTOR_BOX_INIT_ZERO;
  float old_width, old_height;
  float new_width, new_height;
  float width, height;
  cairo_rectangle_int_t window_size;
  int scale_factor;

  if (priv->impl == NULL)
    return;

  /* our old allocation */
  clutter_actor_get_allocation_box (self, &alloc);
  clutter_actor_box_get_size (&alloc, &old_width, &old_height);

  /* the current allocation */
  clutter_actor_box_get_size (box, &width, &height);

  /* the current Stage implementation size */
  _clutter_stage_window_get_geometry (priv->impl, &window_size);

  /* if the stage is fixed size (for instance, it's using a EGL framebuffer)
   * then we simply ignore any allocation request and override the
   * allocation chain - because we cannot forcibly change the size of the
   * stage window.
   */
  if (!clutter_feature_available (CLUTTER_FEATURE_STAGE_STATIC))
    {
      CLUTTER_NOTE (LAYOUT,
                    "Following allocation to %.2fx%.2f (absolute origin %s)",
                    width, height,
                    (flags & CLUTTER_ABSOLUTE_ORIGIN_CHANGED)
                      ? "changed"
                      : "not changed");

      clutter_actor_set_allocation (self, box,
                                    flags | CLUTTER_DELEGATE_LAYOUT);

      /* Ensure the window is sized correctly */
      if (!priv->is_fullscreen)
        {
          if (priv->min_size_changed)
            {
              gfloat min_width, min_height;
              gboolean min_width_set, min_height_set;

              g_object_get (G_OBJECT (self),
                            "min-width", &min_width,
                            "min-width-set", &min_width_set,
                            "min-height", &min_height,
                            "min-height-set", &min_height_set,
                            NULL);

              if (!min_width_set)
                min_width = 1;
              if (!min_height_set)
                min_height = 1;

              if (width < min_width)
                width = min_width;
              if (height < min_height)
                height = min_height;

              priv->min_size_changed = FALSE;
            }

          if (window_size.width != CLUTTER_NEARBYINT (width) ||
              window_size.height != CLUTTER_NEARBYINT (height))
            {
              _clutter_stage_window_resize (priv->impl,
                                            CLUTTER_NEARBYINT (width),
                                            CLUTTER_NEARBYINT (height));
            }
        }
    }
  else
    {
      ClutterActorBox override = { 0, };

      /* override the passed allocation */
      override.x1 = 0;
      override.y1 = 0;
      override.x2 = window_size.width;
      override.y2 = window_size.height;

      CLUTTER_NOTE (LAYOUT,
                    "Overriding original allocation of %.2fx%.2f "
                    "with %.2fx%.2f (absolute origin %s)",
                    width, height,
                    override.x2, override.y2,
                    (flags & CLUTTER_ABSOLUTE_ORIGIN_CHANGED)
                      ? "changed"
                      : "not changed");

      /* and store the overridden allocation */
      clutter_actor_set_allocation (self, &override,
                                    flags | CLUTTER_DELEGATE_LAYOUT);
    }

  /* XXX: Until Cogl becomes fully responsible for backend windows
   * Clutter need to manually keep it informed of the current window
   * size. We do this after the allocation above so that the stage
   * window has a chance to update the window size based on the
   * allocation.
   */
  _clutter_stage_window_get_geometry (priv->impl, &window_size);

  scale_factor = _clutter_stage_window_get_scale_factor (priv->impl);

  window_size.width *= scale_factor;
  window_size.height *= scale_factor;

  cogl_onscreen_clutter_backend_set_size (window_size.width,
                                          window_size.height);

  /* reset the viewport if the allocation effectively changed */
  clutter_actor_get_allocation_box (self, &alloc);
  clutter_actor_box_get_size (&alloc, &new_width, &new_height);

  if (CLUTTER_NEARBYINT (old_width) != CLUTTER_NEARBYINT (new_width) ||
      CLUTTER_NEARBYINT (old_height) != CLUTTER_NEARBYINT (new_height))
    {
      int real_width = CLUTTER_NEARBYINT (new_width);
      int real_height = CLUTTER_NEARBYINT (new_height);

      _clutter_stage_set_viewport (CLUTTER_STAGE (self),
                                   0, 0,
                                   real_width,
                                   real_height);

      /* Note: we don't assume that set_viewport will queue a full redraw
       * since it may bail-out early if something preemptively set the
       * viewport before the stage was really allocated its new size.
       */
      queue_full_redraw (CLUTTER_STAGE (self));
    }
}

typedef struct _Vector4
{
  float x, y, z, w;
} Vector4;

static void
_cogl_util_get_eye_planes_for_screen_poly (float *polygon,
                                           int n_vertices,
                                           float *viewport,
                                           const CoglMatrix *projection,
                                           const CoglMatrix *inverse_project,
                                           ClutterPlane *planes)
{
  float Wc;
  Vector4 *tmp_poly;
  ClutterPlane *plane;
  int i;
  float b[3];
  float c[3];
  int count;

  tmp_poly = g_alloca (sizeof (Vector4) * n_vertices * 2);

#define DEPTH -50

  /* Determine W in clip-space (Wc) for a point (0, 0, DEPTH, 1)
   *
   * Note: the depth could be anything except 0.
   *
   * We will transform the polygon into clip coordinates using this
   * depth and then into eye coordinates. Our clip planes will be
   * defined by triangles that extend between points of the polygon at
   * DEPTH and corresponding points of the same polygon at DEPTH * 2.
   *
   * NB: Wc defines the position of the clip planes in clip
   * coordinates. Given a screen aligned cross section through the
   * frustum; coordinates range from [-Wc,Wc] left to right on the
   * x-axis and [Wc,-Wc] top to bottom on the y-axis.
   */
  Wc = DEPTH * projection->wz + projection->ww;

#define CLIP_X(X) ((((float)X - viewport[0]) * (2.0 / viewport[2])) - 1) * Wc
#define CLIP_Y(Y) ((((float)Y - viewport[1]) * (2.0 / viewport[3])) - 1) * -Wc

  for (i = 0; i < n_vertices; i++)
    {
      tmp_poly[i].x = CLIP_X (polygon[i * 2]);
      tmp_poly[i].y = CLIP_Y (polygon[i * 2 + 1]);
      tmp_poly[i].z = DEPTH;
      tmp_poly[i].w = Wc;
    }

  Wc = DEPTH * 2 * projection->wz + projection->ww;

  /* FIXME: technically we don't need to project all of the points
   * twice, it would be enough project every other point since
   * we can share points in this set to define the plane vectors. */
  for (i = 0; i < n_vertices; i++)
    {
      tmp_poly[n_vertices + i].x = CLIP_X (polygon[i * 2]);
      tmp_poly[n_vertices + i].y = CLIP_Y (polygon[i * 2 + 1]);
      tmp_poly[n_vertices + i].z = DEPTH * 2;
      tmp_poly[n_vertices + i].w = Wc;
    }

#undef CLIP_X
#undef CLIP_Y

  cogl_matrix_project_points (inverse_project,
                              4,
                              sizeof (Vector4),
                              tmp_poly,
                              sizeof (Vector4),
                              tmp_poly,
                              n_vertices * 2);

  /* XXX: It's quite ugly that we end up with these casts between
   * Vector4 types and CoglVector3s, it might be better if the
   * cogl_vector APIs just took pointers to floats.
   */

  count = n_vertices - 1;
  for (i = 0; i < count; i++)
    {
      plane = &planes[i];
      memcpy (plane->v0, tmp_poly + i, sizeof (float) * 3);
      memcpy (b, tmp_poly + n_vertices + i, sizeof (float) * 3);
      memcpy (c, tmp_poly + n_vertices + i + 1, sizeof (float) * 3);
      cogl_vector3_subtract (b, b, plane->v0);
      cogl_vector3_subtract (c, c, plane->v0);
      cogl_vector3_cross_product (plane->n, b, c);
      cogl_vector3_normalize (plane->n);
    }

  plane = &planes[n_vertices - 1];
  memcpy (plane->v0, tmp_poly + 0, sizeof (float) * 3);
  memcpy (b, tmp_poly + (2 * n_vertices - 1), sizeof (float) * 3);
  memcpy (c, tmp_poly + n_vertices, sizeof (float) * 3);
  cogl_vector3_subtract (b, b, plane->v0);
  cogl_vector3_subtract (c, c, plane->v0);
  cogl_vector3_cross_product (plane->n, b, c);
  cogl_vector3_normalize (plane->n);
}

static void
_clutter_stage_update_active_framebuffer (ClutterStage *stage)
{
  ClutterStagePrivate *priv = stage->priv;

  /* We track the CoglFramebuffer that corresponds to the stage itself
   * so, for example, we can disable culling when rendering to an
   * offscreen framebuffer.
   */

  priv->active_framebuffer =
    _clutter_stage_window_get_active_framebuffer (priv->impl);

  if (!priv->active_framebuffer)
    priv->active_framebuffer = cogl_get_draw_framebuffer ();
}

/* This provides a common point of entry for painting the scenegraph
 * for picking or painting...
 *
 * XXX: Instead of having a toplevel 2D clip region, it might be
 * better to have a clip volume within the view frustum. This could
 * allow us to avoid projecting actors into window coordinates to
 * be able to cull them.
 */
void
_clutter_stage_do_paint (ClutterStage                *stage,
                         const cairo_rectangle_int_t *clip)
{
  ClutterStagePrivate *priv = stage->priv;
  float clip_poly[8];
  float viewport[4];
  cairo_rectangle_int_t geom;
  int window_scale;

  if (priv->impl == NULL)
    return;

  _clutter_stage_window_get_geometry (priv->impl, &geom);
  window_scale = _clutter_stage_window_get_scale_factor (priv->impl);

  viewport[0] = priv->viewport[0] * window_scale;
  viewport[1] = priv->viewport[1] * window_scale;
  viewport[2] = priv->viewport[2] * window_scale;
  viewport[3] = priv->viewport[3] * window_scale;

  if (clip)
    {
      clip_poly[0] = MAX (clip->x * window_scale, 0);
      clip_poly[1] = MAX (clip->y * window_scale, 0);
      clip_poly[2] = MIN ((clip->x + clip->width) * window_scale, geom.width * window_scale);
      clip_poly[3] = clip_poly[1];
      clip_poly[4] = clip_poly[2];
      clip_poly[5] = MIN ((clip->y + clip->height) * window_scale, geom.height * window_scale);
      clip_poly[6] = clip_poly[0];
      clip_poly[7] = clip_poly[5];
    }
  else
    {
      clip_poly[0] = 0;
      clip_poly[1] = 0;
      clip_poly[2] = geom.width * window_scale;
      clip_poly[3] = 0;
      clip_poly[4] = geom.width * window_scale;
      clip_poly[5] = geom.height * window_scale;
      clip_poly[6] = 0;
      clip_poly[7] = geom.height * window_scale;
    }

  CLUTTER_NOTE (CLIPPING, "Setting stage clip too: "
                "x=%f, y=%f, width=%f, height=%f",
                clip_poly[0], clip_poly[1],
                clip_poly[2] - clip_poly[0],
                clip_poly[5] - clip_poly[1]);

  _cogl_util_get_eye_planes_for_screen_poly (clip_poly,
                                             4,
                                             viewport,
                                             &priv->projection,
                                             &priv->inverse_projection,
                                             priv->current_clip_planes);

  _clutter_stage_paint_volume_stack_free_all (stage);
  _clutter_stage_update_active_framebuffer (stage);
  clutter_actor_paint (CLUTTER_ACTOR (stage));

  clutter_stage_invoke_paint_callback (stage);
}

/* If we don't implement this here, we get the paint function
 * from the deprecated clutter-group class, which doesn't
 * respect the Z order as it uses our empty sort_depth_order.
 */
static void
clutter_stage_paint (ClutterActor *self)
{
  ClutterActorIter iter;
  ClutterActor *child;

  clutter_actor_iter_init (&iter, self);
  while (clutter_actor_iter_next (&iter, &child))
    clutter_actor_paint (child);
}

#if 0
/* the Stage is cleared in clutter_actor_paint_node() */
static void
clutter_stage_paint (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  CoglBufferBit clear_flags;
  ClutterColor bg_color;
  CoglColor stage_color;
  ClutterActorIter iter;
  ClutterActor *child;
  guint8 real_alpha;

  CLUTTER_STATIC_TIMER (stage_clear_timer,
                        "Painting actors", /* parent */
                        "Stage clear",
                        "The time spent clearing the stage",
                        0 /* no application private data */);

  CLUTTER_NOTE (PAINT, "Initializing stage paint");

  /* composite the opacity to the stage color */
  clutter_actor_get_background_color (self, &bg_color);
  real_alpha = clutter_actor_get_opacity (self)
             * bg_color.alpha
             / 255;

  clear_flags = COGL_BUFFER_BIT_DEPTH;
  if (!STAGE_NO_CLEAR_ON_PAINT (self))
    clear_flags |= COGL_BUFFER_BIT_COLOR;

  CLUTTER_TIMER_START (_clutter_uprof_context, stage_clear_timer);
  /* we use the real alpha to clear the stage if :use-alpha is
   * set; the effect depends entirely on the Clutter backend
   */
  cogl_color_init_from_4ub (&stage_color,
                            bg_color.red,
                            bg_color.green,
                            bg_color.blue,
                            priv->use_alpha ? real_alpha : 255);
  cogl_color_premultiply (&stage_color);
  cogl_clear (&stage_color, clear_flags);
  CLUTTER_TIMER_STOP (_clutter_uprof_context, stage_clear_timer);

  clutter_actor_iter_init (&iter, self);
  while (clutter_actor_iter_next (&iter, &child))
    clutter_actor_paint (child);
}
#endif

static void
clutter_stage_pick (ClutterActor       *self,
		    const ClutterColor *color)
{
  ClutterActorIter iter;
  ClutterActor *child;

  /* Note: we don't chain up to our parent as we don't want any geometry
   * emitted for the stage itself. The stage's pick id is effectively handled
   * by the call to cogl_clear done in clutter-main.c:_clutter_do_pick_async()
   */
  clutter_actor_iter_init (&iter, self);
  while (clutter_actor_iter_next (&iter, &child))
    clutter_actor_paint (child);
}

static gboolean
clutter_stage_get_paint_volume (ClutterActor *self,
                                ClutterPaintVolume *volume)
{
  /* Returning False effectively means Clutter has to assume it covers
   * everything... */
  return FALSE;
}

static void
clutter_stage_realize (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;
  gboolean is_realized;

  /* Make sure the viewport and projection matrix are valid for the
   * first paint (which will likely occur before the ConfigureNotify
   * is received)
   */
  priv->dirty_viewport = TRUE;
  priv->dirty_projection = TRUE;

  g_assert (priv->impl != NULL);
  is_realized = _clutter_stage_window_realize (priv->impl);

  /* ensure that the stage is using the context if the
   * realization sequence was successful
   */
  if (is_realized)
    {
      ClutterBackend *backend = clutter_get_default_backend ();

      /* We want to select the context without calling
         clutter_backend_ensure_context so that it doesn't call any
         Cogl functions. Otherwise it would create the Cogl context
         before we get a chance to check whether the GL version is
         valid */
      _clutter_backend_ensure_context_internal (backend, CLUTTER_STAGE (self));
    }
  else
    CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_REALIZED);
}

static void
clutter_stage_unrealize (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  /* and then unrealize the implementation */
  g_assert (priv->impl != NULL);
  _clutter_stage_window_unrealize (priv->impl);

  CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_REALIZED);

  clutter_stage_ensure_current (CLUTTER_STAGE (self));
}

static void
clutter_stage_show_all (ClutterActor *self)
{
  ClutterActorIter iter;
  ClutterActor *child;

  /* we don't do a recursive show_all(), to maintain the old
   * invariants from ClutterGroup
   */
  clutter_actor_iter_init (&iter, self);
  while (clutter_actor_iter_next (&iter, &child))
    clutter_actor_show (child);

  clutter_actor_show (self);
}

static void
clutter_stage_show (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  CLUTTER_ACTOR_CLASS (clutter_stage_parent_class)->show (self);

  /* Possibly do an allocation run so that the stage will have the
     right size before we map it */
  _clutter_stage_maybe_relayout (self);

  g_assert (priv->impl != NULL);
  _clutter_stage_window_show (priv->impl, TRUE);
}

static void
clutter_stage_hide_all (ClutterActor *self)
{
  ClutterActorIter iter;
  ClutterActor *child;

  clutter_actor_hide (self);

  /* we don't do a recursive hide_all(), to maintain the old invariants
   * from ClutterGroup
   */
  clutter_actor_iter_init (&iter, self);
  while (clutter_actor_iter_next (&iter, &child))
    clutter_actor_hide (child);
}

static void
clutter_stage_hide (ClutterActor *self)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (self)->priv;

  g_assert (priv->impl != NULL);
  _clutter_stage_window_hide (priv->impl);

  CLUTTER_ACTOR_CLASS (clutter_stage_parent_class)->hide (self);
}

static void
clutter_stage_emit_key_focus_event (ClutterStage *stage,
                                    gboolean      focus_in)
{
  ClutterStagePrivate *priv = stage->priv;

  if (priv->key_focused_actor == NULL)
    return;

  if (focus_in)
    g_signal_emit_by_name (priv->key_focused_actor, "key-focus-in");
  else
    g_signal_emit_by_name (priv->key_focused_actor, "key-focus-out");

  g_object_notify (G_OBJECT (stage), "key-focus");
}

static void
clutter_stage_real_activate (ClutterStage *stage)
{
  clutter_stage_emit_key_focus_event (stage, TRUE);
}

static void
clutter_stage_real_deactivate (ClutterStage *stage)
{
  clutter_stage_emit_key_focus_event (stage, FALSE);
}

static void
clutter_stage_real_fullscreen (ClutterStage *stage)
{
  ClutterStagePrivate *priv = stage->priv;
  cairo_rectangle_int_t geom;
  ClutterActorBox box;

  /* we need to force an allocation here because the size
   * of the stage might have been changed by the backend
   *
   * this is a really bad solution to the issues caused by
   * the fact that fullscreening the stage on the X11 backends
   * is really an asynchronous operation
   */
  _clutter_stage_window_get_geometry (priv->impl, &geom);

  box.x1 = 0;
  box.y1 = 0;
  box.x2 = geom.width;
  box.y2 = geom.height;

  /* we need to blow the caching on the Stage size, given that
   * we're about to force an allocation, because if anything
   * ends up querying the size of the stage during the allocate()
   * call, like constraints or signal handlers, we'll get into an
   * inconsistent state: the stage will report the old cached size,
   * but the allocation will be updated anyway.
   */
  clutter_actor_set_size (CLUTTER_ACTOR (stage), -1.0, -1.0);
  clutter_actor_allocate (CLUTTER_ACTOR (stage),
                          &box,
                          CLUTTER_ALLOCATION_NONE);
}

void
_clutter_stage_queue_event (ClutterStage *stage,
                            ClutterEvent *event,
                            gboolean      copy_event)
{
  ClutterStagePrivate *priv;
  gboolean first_event;
  ClutterInputDevice *device;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  first_event = priv->event_queue->length == 0;

  if (copy_event)
    event = clutter_event_copy (event);

  g_queue_push_tail (priv->event_queue, event);

  if (first_event)
    {
      ClutterMasterClock *master_clock = _clutter_master_clock_get_default ();
      _clutter_master_clock_start_running (master_clock);
      _clutter_stage_schedule_update (stage);
    }

  /* if needed, update the state of the input device of the event.
   * we do it here to avoid calling the same code from every backend
   * event processing function
   */
  device = clutter_event_get_device (event);
  if (device != NULL)
    {
      ClutterModifierType event_state = clutter_event_get_state (event);
      ClutterEventSequence *sequence = clutter_event_get_event_sequence (event);
      guint32 event_time = clutter_event_get_time (event);
      gfloat event_x, event_y;

      clutter_event_get_coords (event, &event_x, &event_y);

      _clutter_input_device_set_coords (device, sequence, event_x, event_y, stage);
      _clutter_input_device_set_state (device, event_state);
      _clutter_input_device_set_time (device, event_time);
    }
}

gboolean
_clutter_stage_has_queued_events (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  priv = stage->priv;

  return priv->event_queue->length > 0;
}

void
_clutter_stage_process_queued_events (ClutterStage *stage)
{
  ClutterStagePrivate *priv;
  GList *events, *l;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (priv->event_queue->length == 0)
    return;

  /* In case the stage gets destroyed during event processing */
  g_object_ref (stage);

  /* Steal events before starting processing to avoid reentrancy
   * issues */
  events = priv->event_queue->head;
  priv->event_queue->head =  NULL;
  priv->event_queue->tail = NULL;
  priv->event_queue->length = 0;

  for (l = events; l != NULL; l = l->next)
    {
      ClutterEvent *event;
      ClutterEvent *next_event;
      ClutterInputDevice *device;
      ClutterInputDevice *next_device;
      gboolean check_device = FALSE;

      event = l->data;
      next_event = l->next ? l->next->data : NULL;

      device = clutter_event_get_device (event);

      if (next_event != NULL)
        next_device = clutter_event_get_device (next_event);
      else
        next_device = NULL;

      if (device != NULL && next_device != NULL)
        check_device = TRUE;

      /* Skip consecutive motion events coming from the same device */
      if (priv->throttle_motion_events && next_event != NULL)
        {
          if (event->type == CLUTTER_MOTION &&
              (next_event->type == CLUTTER_MOTION ||
               next_event->type == CLUTTER_LEAVE) &&
              (!check_device || (device == next_device)))
            {
              CLUTTER_NOTE (EVENT,
                            "Omitting motion event at %d, %d",
                            (int) event->motion.x,
                            (int) event->motion.y);
              goto next_event;
            }
          else if (event->type == CLUTTER_TOUCH_UPDATE &&
                   next_event->type == CLUTTER_TOUCH_UPDATE &&
                   event->touch.sequence == next_event->touch.sequence &&
                   (!check_device || (device == next_device)))
            {
              CLUTTER_NOTE (EVENT,
                            "Omitting touch update event at %d, %d",
                            (int) event->touch.x,
                            (int) event->touch.y);
              goto next_event;
            }
        }

      _clutter_process_event (event);

    next_event:
      clutter_event_free (event);
    }

  g_list_free (events);

  g_object_unref (stage);
}

/**
 * _clutter_stage_needs_update:
 * @stage: A #ClutterStage
 *
 * Determines if _clutter_stage_do_update() needs to be called.
 *
 * Return value: %TRUE if the stage need layout or painting
 */
gboolean
_clutter_stage_needs_update (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  priv = stage->priv;

  return priv->relayout_pending || priv->redraw_pending;
}

void
_clutter_stage_maybe_relayout (ClutterActor *actor)
{
  ClutterStage *stage = CLUTTER_STAGE (actor);
  ClutterStagePrivate *priv = stage->priv;
  gfloat natural_width, natural_height;
  ClutterActorBox box = { 0, };
  CLUTTER_STATIC_TIMER (relayout_timer,
                        "Mainloop", /* no parent */
                        "Layouting",
                        "The time spent reallocating the stage",
                        0 /* no application private data */);

  if (!priv->relayout_pending)
    return;

  /* avoid reentrancy */
  if (!CLUTTER_ACTOR_IN_RELAYOUT (stage))
    {
      priv->relayout_pending = FALSE;

      CLUTTER_TIMER_START (_clutter_uprof_context, relayout_timer);
      CLUTTER_NOTE (ACTOR, "Recomputing layout");

      CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_IN_RELAYOUT);

      natural_width = natural_height = 0;
      clutter_actor_get_preferred_size (CLUTTER_ACTOR (stage),
                                        NULL, NULL,
                                        &natural_width, &natural_height);

      box.x1 = 0;
      box.y1 = 0;
      box.x2 = natural_width;
      box.y2 = natural_height;

      CLUTTER_NOTE (ACTOR, "Allocating (0, 0 - %d, %d) for the stage",
                    (int) natural_width,
                    (int) natural_height);

      clutter_actor_allocate (CLUTTER_ACTOR (stage),
                              &box, CLUTTER_ALLOCATION_NONE);

      CLUTTER_UNSET_PRIVATE_FLAGS (stage, CLUTTER_IN_RELAYOUT);
      CLUTTER_TIMER_STOP (_clutter_uprof_context, relayout_timer);
    }
}

static void
clutter_stage_do_redraw (ClutterStage *stage)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterActor *actor = CLUTTER_ACTOR (stage);
  ClutterStagePrivate *priv = stage->priv;

  CLUTTER_STATIC_COUNTER (redraw_counter,
                          "clutter_stage_do_redraw counter",
                          "Increments for each Stage redraw",
                          0 /* no application private data */);
  CLUTTER_STATIC_TIMER (redraw_timer,
                        "Master Clock", /* parent */
                        "Redrawing",
                        "The time spent redrawing everything",
                        0 /* no application private data */);

  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return;

  if (priv->impl == NULL)
    return;

  CLUTTER_NOTE (PAINT, "Redraw started for stage '%s'[%p]",
                _clutter_actor_get_debug_name (actor),
                stage);

  _clutter_backend_ensure_context (backend, stage);

  if (_clutter_context_get_show_fps ())
    {
      if (priv->fps_timer == NULL)
        priv->fps_timer = g_timer_new ();
    }

  _clutter_stage_maybe_setup_viewport (stage);

  CLUTTER_COUNTER_INC (_clutter_uprof_context, redraw_counter);
  CLUTTER_TIMER_START (_clutter_uprof_context, redraw_timer);

  _clutter_stage_window_redraw (priv->impl);

  CLUTTER_TIMER_STOP (_clutter_uprof_context, redraw_timer);

  if (_clutter_context_get_show_fps ())
    {
      priv->timer_n_frames += 1;

      if (g_timer_elapsed (priv->fps_timer, NULL) >= 1.0)
        {
          g_print ("*** FPS for %s: %i ***\n",
                   _clutter_actor_get_debug_name (actor),
                   priv->timer_n_frames);

          priv->timer_n_frames = 0;
          g_timer_start (priv->fps_timer);
        }
    }

  CLUTTER_NOTE (PAINT, "Redraw finished for stage '%s'[%p]",
                _clutter_actor_get_debug_name (actor),
                stage);
}

/**
 * _clutter_stage_do_update:
 * @stage: A #ClutterStage
 *
 * Handles per-frame layout and repaint for the stage.
 *
 * Return value: %TRUE if the stage was updated
 */
gboolean
_clutter_stage_do_update (ClutterStage *stage)
{
  ClutterStagePrivate *priv = stage->priv;

  /* if the stage is being destroyed, or if the destruction already
   * happened and we don't have an StageWindow any more, then we
   * should bail out
   */
  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage) || priv->impl == NULL)
    return FALSE;

  if (!CLUTTER_ACTOR_IS_REALIZED (stage))
    return FALSE;

  /* NB: We need to ensure we have an up to date layout *before* we
   * check or clear the pending redraws flag since a relayout may
   * queue a redraw.
   */
  _clutter_stage_maybe_relayout (CLUTTER_ACTOR (stage));

  if (!priv->redraw_pending)
    return FALSE;

  clutter_stage_maybe_finish_queue_redraws (stage);

  clutter_stage_do_redraw (stage);

  /* reset the guard, so that new redraws are possible */
  priv->redraw_pending = FALSE;

#ifdef CLUTTER_ENABLE_DEBUG
  if (priv->redraw_count > 0)
    {
      CLUTTER_NOTE (SCHEDULER, "Queued %lu redraws during the last cycle",
                    priv->redraw_count);

      priv->redraw_count = 0;
    }
#endif /* CLUTTER_ENABLE_DEBUG */

  return TRUE;
}

static void
clutter_stage_real_queue_relayout (ClutterActor *self)
{
  ClutterStage *stage = CLUTTER_STAGE (self);
  ClutterStagePrivate *priv = stage->priv;
  ClutterActorClass *parent_class;

  if (!priv->relayout_pending)
    {
      _clutter_stage_schedule_update (stage);
      priv->relayout_pending = TRUE;
    }

  /* chain up */
  parent_class = CLUTTER_ACTOR_CLASS (clutter_stage_parent_class);
  parent_class->queue_relayout (self);
}

static void
clutter_stage_real_queue_redraw (ClutterActor *actor,
                                 ClutterActor *leaf)
{
  ClutterStage *stage = CLUTTER_STAGE (actor);
  ClutterStageWindow *stage_window;
  ClutterPaintVolume *redraw_clip;
  ClutterActorBox bounding_box;
  ClutterActorBox intersection_box;
  cairo_rectangle_int_t geom, stage_clip;

  if (CLUTTER_ACTOR_IN_DESTRUCTION (actor))
    return;

  /* If the backend can't do anything with redraw clips (e.g. it already knows
   * it needs to redraw everything anyway) then don't spend time transforming
   * any clip volume into stage coordinates... */
  stage_window = _clutter_stage_get_window (stage);
  if (stage_window == NULL)
    return;

  if (_clutter_stage_window_ignoring_redraw_clips (stage_window))
    {
      _clutter_stage_window_add_redraw_clip (stage_window, NULL);
      return;
    }

  /* Convert the clip volume into stage coordinates and then into an
   * axis aligned stage coordinates bounding box...
   */
  redraw_clip = _clutter_actor_get_queue_redraw_clip (leaf);
  if (redraw_clip == NULL)
    {
      _clutter_stage_window_add_redraw_clip (stage_window, NULL);
      return;
    }

  if (redraw_clip->is_empty)
    return;

  _clutter_paint_volume_get_stage_paint_box (redraw_clip,
                                             stage,
                                             &bounding_box);

  _clutter_stage_window_get_geometry (stage_window, &geom);

  intersection_box.x1 = MAX (bounding_box.x1, 0);
  intersection_box.y1 = MAX (bounding_box.y1, 0);
  intersection_box.x2 = MIN (bounding_box.x2, geom.width);
  intersection_box.y2 = MIN (bounding_box.y2, geom.height);

  /* There is no need to track degenerate/empty redraw clips */
  if (intersection_box.x2 <= intersection_box.x1 ||
      intersection_box.y2 <= intersection_box.y1)
    return;

  /* when converting to integer coordinates make sure we round the edges of the
   * clip rectangle outwards... */
  stage_clip.x = intersection_box.x1;
  stage_clip.y = intersection_box.y1;
  stage_clip.width = intersection_box.x2 - stage_clip.x;
  stage_clip.height = intersection_box.y2 - stage_clip.y;

  _clutter_stage_window_add_redraw_clip (stage_window, &stage_clip);
}

gboolean
_clutter_stage_has_full_redraw_queued (ClutterStage *stage)
{
  ClutterStageWindow *stage_window = _clutter_stage_get_window (stage);

  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage) || stage_window == NULL)
    return FALSE;

  if (stage->priv->redraw_pending &&
      !_clutter_stage_window_has_redraw_clips (stage_window))
    return TRUE;
  else
    return FALSE;
}

/**
 * clutter_stage_get_redraw_clip_bounds:
 * @stage: A #ClutterStage
 * @clip: (out caller-allocates): Return location for the clip bounds
 *
 * Gets the bounds of the current redraw for @stage in stage pixel
 * coordinates. E.g., if only a single actor has queued a redraw then
 * Clutter may redraw the stage with a clip so that it doesn't have to
 * paint every pixel in the stage. This function would then return the
 * bounds of that clip. An application can use this information to
 * avoid some extra work if it knows that some regions of the stage
 * aren't going to be painted. This should only be called while the
 * stage is being painted. If there is no current redraw clip then
 * this function will set @clip to the full extents of the stage.
 *
 * Since: 1.8
 */
void
clutter_stage_get_redraw_clip_bounds (ClutterStage          *stage,
                                      cairo_rectangle_int_t *clip)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (clip != NULL);

  priv = stage->priv;

  if (!_clutter_stage_window_get_redraw_clip_bounds (priv->impl, clip))
    {
      /* Set clip to the full extents of the stage */
      _clutter_stage_window_get_geometry (priv->impl, clip);
    }
}

static void
read_pixels_to_file (char *filename_stem,
                     int   x,
                     int   y,
                     int   width,
                     int   height)
{
  guint8 *data;
  cairo_surface_t *surface;
  static int read_count = 0;
  char *filename = g_strdup_printf ("%s-%05d.png",
                                    filename_stem,
                                    read_count);

  data = g_malloc (4 * width * height);
  cogl_read_pixels (x, y, width, height,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    CLUTTER_CAIRO_FORMAT_ARGB32,
                    data);

  surface = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_RGB24,
                                                 width, height,
                                                 width * 4);

  cairo_surface_write_to_png (surface, filename);
  cairo_surface_destroy (surface);

  g_free (data);
  g_free (filename);

  read_count++;
}

ClutterActor *
_clutter_stage_do_pick (ClutterStage   *stage,
                        gint            x,
                        gint            y,
                        ClutterPickMode mode)
{
  ClutterActor *actor = CLUTTER_ACTOR (stage);
  ClutterStagePrivate *priv = stage->priv;
  ClutterMainContext *context;
  guchar pixel[4] = { 0xff, 0xff, 0xff, 0xff };
  CoglColor stage_pick_id;
  gboolean dither_enabled_save;
  ClutterActor *retval;
  CoglFramebuffer *fb;
  gint dirty_x;
  gint dirty_y;
  gint read_x;
  gint read_y;
  float stage_width, stage_height;
  int window_scale;

  CLUTTER_STATIC_COUNTER (do_pick_counter,
                          "_clutter_stage_do_pick counter",
                          "Increments for each full pick run",
                          0 /* no application private data */);
  CLUTTER_STATIC_TIMER (pick_timer,
                        "Mainloop", /* parent */
                        "Picking",
                        "The time spent picking",
                        0 /* no application private data */);
  CLUTTER_STATIC_TIMER (pick_clear,
                        "Picking", /* parent */
                        "Stage clear (pick)",
                        "The time spent clearing stage for picking",
                        0 /* no application private data */);
  CLUTTER_STATIC_TIMER (pick_paint,
                        "Picking", /* parent */
                        "Painting actors (pick mode)",
                        "The time spent painting actors in pick mode",
                        0 /* no application private data */);
  CLUTTER_STATIC_TIMER (pick_read,
                        "Picking", /* parent */
                        "Read Pixels",
                        "The time spent issuing a read pixels",
                        0 /* no application private data */);

  priv = stage->priv;

  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return actor;

  if (G_UNLIKELY (clutter_pick_debug_flags & CLUTTER_DEBUG_NOP_PICKING))
    return actor;

  if (G_UNLIKELY (priv->impl == NULL))
    return actor;

  clutter_actor_get_size (CLUTTER_ACTOR (stage), &stage_width, &stage_height);
  if (x < 0 || x >= stage_width || y < 0 || y >= stage_height)
    return actor;

#ifdef CLUTTER_ENABLE_PROFILE
  if (clutter_profile_flags & CLUTTER_PROFILE_PICKING_ONLY)
    _clutter_profile_resume ();
#endif /* CLUTTER_ENABLE_PROFILE */

  CLUTTER_COUNTER_INC (_clutter_uprof_context, do_pick_counter);
  CLUTTER_TIMER_START (_clutter_uprof_context, pick_timer);

  context = _clutter_context_get_default ();
  clutter_stage_ensure_current (stage);
  window_scale = _clutter_stage_window_get_scale_factor (priv->impl);

  fb = cogl_get_draw_framebuffer ();

  _clutter_backend_ensure_context (context->backend, stage);

  /* needed for when a context switch happens */
  _clutter_stage_maybe_setup_viewport (stage);

  _clutter_stage_window_get_dirty_pixel (priv->impl, &dirty_x, &dirty_y);

  if (G_LIKELY (!(clutter_pick_debug_flags & CLUTTER_DEBUG_DUMP_PICK_BUFFERS)))
    cogl_framebuffer_push_scissor_clip (fb, dirty_x * window_scale, dirty_y * window_scale, 1, 1);

  cogl_set_viewport (priv->viewport[0] * window_scale - x * window_scale + dirty_x * window_scale,
                     priv->viewport[1] * window_scale - y * window_scale + dirty_y * window_scale,
                     priv->viewport[2] * window_scale,
                     priv->viewport[3] * window_scale);

  read_x = dirty_x * window_scale;
  read_y = dirty_y * window_scale;

  CLUTTER_NOTE (PICK, "Performing pick at %i,%i", x, y);

  CLUTTER_TIMER_START (_clutter_uprof_context, pick_clear);
  cogl_color_init_from_4ub (&stage_pick_id, 255, 255, 255, 255);
  cogl_clear (&stage_pick_id, COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH);
  CLUTTER_TIMER_STOP (_clutter_uprof_context, pick_clear);

  /* Disable dithering (if any) when doing the painting in pick mode */
  dither_enabled_save = cogl_framebuffer_get_dither_enabled (fb);
  cogl_framebuffer_set_dither_enabled (fb, FALSE);

  /* Render the entire scence in pick mode - just single colored silhouette's
   * are drawn offscreen (as we never swap buffers)
  */
  CLUTTER_TIMER_START (_clutter_uprof_context, pick_paint);
  context->pick_mode = mode;
  _clutter_stage_do_paint (stage, NULL);
  context->pick_mode = CLUTTER_PICK_NONE;
  CLUTTER_TIMER_STOP (_clutter_uprof_context, pick_paint);

  /* Read the color of the screen co-ords pixel. RGBA_8888_PRE is used
     even though we don't care about the alpha component because under
     GLES this is the only format that is guaranteed to work so Cogl
     will end up having to do a conversion if any other format is
     used. The format is requested as pre-multiplied because Cogl
     assumes that all pixels in the framebuffer are premultiplied so
     it avoids a conversion. */
  CLUTTER_TIMER_START (_clutter_uprof_context, pick_read);
  cogl_read_pixels (read_x, read_y, 1, 1,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    pixel);
  CLUTTER_TIMER_STOP (_clutter_uprof_context, pick_read);

  if (G_UNLIKELY (clutter_pick_debug_flags & CLUTTER_DEBUG_DUMP_PICK_BUFFERS))
    {
      char *file_name =
        g_strconcat ("pick-buffer-",
                     _clutter_actor_get_debug_name (actor),
                     NULL);

      read_pixels_to_file (file_name, 0, 0, stage_width, stage_height);

      g_free (file_name);
    }

  /* Restore whether GL_DITHER was enabled */
  cogl_framebuffer_set_dither_enabled (fb, dither_enabled_save);

  if (G_LIKELY (!(clutter_pick_debug_flags & CLUTTER_DEBUG_DUMP_PICK_BUFFERS)))
    cogl_framebuffer_pop_clip (fb);

  _clutter_stage_dirty_viewport (stage);

  if (pixel[0] == 0xff && pixel[1] == 0xff && pixel[2] == 0xff)
    retval = actor;
  else
    {
      guint32 id_ = _clutter_pixel_to_id (pixel);

      retval = _clutter_get_actor_by_id (stage, id_);
    }

  CLUTTER_TIMER_STOP (_clutter_uprof_context, pick_timer);

#ifdef CLUTTER_ENABLE_PROFILE
  if (clutter_profile_flags & CLUTTER_PROFILE_PICKING_ONLY)
    _clutter_profile_suspend ();
#endif

  return retval;
}

static gboolean
clutter_stage_real_delete_event (ClutterStage *stage,
                                 ClutterEvent *event)
{
  if (stage_is_default (stage))
    clutter_main_quit ();
  else
    clutter_actor_destroy (CLUTTER_ACTOR (stage));

  return TRUE;
}

static void
clutter_stage_real_apply_transform (ClutterActor *stage,
                                    CoglMatrix   *matrix)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (stage)->priv;

  /* FIXME: we probably shouldn't be explicitly reseting the matrix
   * here... */
  cogl_matrix_init_identity (matrix);
  cogl_matrix_multiply (matrix, matrix, &priv->view);
}

static void
clutter_stage_constructed (GObject *gobject)
{
  ClutterStage *self = CLUTTER_STAGE (gobject);
  ClutterStageManager *stage_manager;

  stage_manager = clutter_stage_manager_get_default ();

  /* this will take care to sinking the floating reference */
  _clutter_stage_manager_add_stage (stage_manager, self);

  /* if this stage has been created on a backend that does not
   * support multiple stages then it becomes the default stage
   * as well; any other attempt at creating a ClutterStage will
   * fail.
   */
  if (!clutter_feature_available (CLUTTER_FEATURE_STAGE_MULTIPLE))
    {
      if (G_UNLIKELY (clutter_stage_manager_get_default_stage (stage_manager) != NULL))
        {
          g_error ("Unable to create another stage: the backend of "
                   "type '%s' does not support multiple stages. Use "
                   "clutter_stage_manager_get_default_stage() instead "
                   "to access the stage singleton.",
                   G_OBJECT_TYPE_NAME (clutter_get_default_backend ()));
        }

      _clutter_stage_manager_set_default_stage (stage_manager, self);
    }

  G_OBJECT_CLASS (clutter_stage_parent_class)->constructed (gobject);
}

static void
clutter_stage_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  ClutterStage *stage = CLUTTER_STAGE (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      clutter_actor_set_background_color (CLUTTER_ACTOR (stage),
                                          clutter_value_get_color (value));
      break;

    case PROP_OFFSCREEN:
      if (g_value_get_boolean (value))
        g_warning ("Offscreen stages are currently not supported\n");
      break;

    case PROP_CURSOR_VISIBLE:
      if (g_value_get_boolean (value))
        clutter_stage_show_cursor (stage);
      else
        clutter_stage_hide_cursor (stage);
      break;

    case PROP_PERSPECTIVE:
      clutter_stage_set_perspective (stage, g_value_get_boxed (value));
      break;

    case PROP_TITLE:
      clutter_stage_set_title (stage, g_value_get_string (value));
      break;

    case PROP_USER_RESIZABLE:
      clutter_stage_set_user_resizable (stage, g_value_get_boolean (value));
      break;

    case PROP_USE_FOG:
      clutter_stage_set_use_fog (stage, g_value_get_boolean (value));
      break;

    case PROP_FOG:
      clutter_stage_set_fog (stage, g_value_get_boxed (value));
      break;

    case PROP_USE_ALPHA:
      clutter_stage_set_use_alpha (stage, g_value_get_boolean (value));
      break;

    case PROP_KEY_FOCUS:
      clutter_stage_set_key_focus (stage, g_value_get_object (value));
      break;

    case PROP_NO_CLEAR_HINT:
      clutter_stage_set_no_clear_hint (stage, g_value_get_boolean (value));
      break;

    case PROP_ACCEPT_FOCUS:
      clutter_stage_set_accept_focus (stage, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_stage_get_property (GObject    *gobject,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  ClutterStagePrivate *priv = CLUTTER_STAGE (gobject)->priv;

  switch (prop_id)
    {
    case PROP_COLOR:
      {
        ClutterColor bg_color;

        clutter_actor_get_background_color (CLUTTER_ACTOR (gobject),
                                            &bg_color);
        clutter_value_set_color (value, &bg_color);
      }
      break;

    case PROP_OFFSCREEN:
      g_value_set_boolean (value, FALSE);
      break;

    case PROP_FULLSCREEN_SET:
      g_value_set_boolean (value, priv->is_fullscreen);
      break;

    case PROP_CURSOR_VISIBLE:
      g_value_set_boolean (value, priv->is_cursor_visible);
      break;

    case PROP_PERSPECTIVE:
      g_value_set_boxed (value, &priv->perspective);
      break;

    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;

    case PROP_USER_RESIZABLE:
      g_value_set_boolean (value, priv->is_user_resizable);
      break;

    case PROP_USE_FOG:
      g_value_set_boolean (value, priv->use_fog);
      break;

    case PROP_FOG:
      g_value_set_boxed (value, &priv->fog);
      break;

    case PROP_USE_ALPHA:
      g_value_set_boolean (value, priv->use_alpha);
      break;

    case PROP_KEY_FOCUS:
      g_value_set_object (value, priv->key_focused_actor);
      break;

    case PROP_NO_CLEAR_HINT:
      {
        gboolean hint =
          (priv->stage_hints & CLUTTER_STAGE_NO_CLEAR_ON_PAINT) != 0;

        g_value_set_boolean (value, hint);
      }
      break;

    case PROP_ACCEPT_FOCUS:
      g_value_set_boolean (value, priv->accept_focus);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_stage_dispose (GObject *object)
{
  ClutterStage        *stage = CLUTTER_STAGE (object);
  ClutterStagePrivate *priv = stage->priv;
  ClutterStageManager *stage_manager;

  clutter_actor_hide (CLUTTER_ACTOR (object));

  _clutter_clear_events_queue_for_stage (stage);

  if (priv->impl != NULL)
    {
      CLUTTER_NOTE (BACKEND, "Disposing of the stage implementation");

      if (CLUTTER_ACTOR_IS_REALIZED (object))
        _clutter_stage_window_unrealize (priv->impl);

      g_object_unref (priv->impl);
      priv->impl = NULL;
    }

  clutter_actor_destroy_all_children (CLUTTER_ACTOR (object));

  g_list_free_full (priv->pending_queue_redraws,
                    (GDestroyNotify) free_queue_redraw_entry);
  priv->pending_queue_redraws = NULL;

  /* this will release the reference on the stage */
  stage_manager = clutter_stage_manager_get_default ();
  _clutter_stage_manager_remove_stage (stage_manager, stage);

  G_OBJECT_CLASS (clutter_stage_parent_class)->dispose (object);
}

static void
clutter_stage_finalize (GObject *object)
{
  ClutterStage *stage = CLUTTER_STAGE (object);
  ClutterStagePrivate *priv = stage->priv;

  g_queue_foreach (priv->event_queue, (GFunc) clutter_event_free, NULL);
  g_queue_free (priv->event_queue);

  g_free (priv->title);

  g_array_free (priv->paint_volume_stack, TRUE);

  _clutter_id_pool_free (priv->pick_id_pool);

  if (priv->fps_timer != NULL)
    g_timer_destroy (priv->fps_timer);

  if (priv->paint_notify != NULL)
    priv->paint_notify (priv->paint_data);

  G_OBJECT_CLASS (clutter_stage_parent_class)->finalize (object);
}

static void
clutter_stage_class_init (ClutterStageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->constructed = clutter_stage_constructed;
  gobject_class->set_property = clutter_stage_set_property;
  gobject_class->get_property = clutter_stage_get_property;
  gobject_class->dispose = clutter_stage_dispose;
  gobject_class->finalize = clutter_stage_finalize;

  actor_class->allocate = clutter_stage_allocate;
  actor_class->get_preferred_width = clutter_stage_get_preferred_width;
  actor_class->get_preferred_height = clutter_stage_get_preferred_height;
  actor_class->paint = clutter_stage_paint;
  actor_class->pick = clutter_stage_pick;
  actor_class->get_paint_volume = clutter_stage_get_paint_volume;
  actor_class->realize = clutter_stage_realize;
  actor_class->unrealize = clutter_stage_unrealize;
  actor_class->show = clutter_stage_show;
  actor_class->show_all = clutter_stage_show_all;
  actor_class->hide = clutter_stage_hide;
  actor_class->hide_all = clutter_stage_hide_all;
  actor_class->queue_relayout = clutter_stage_real_queue_relayout;
  actor_class->queue_redraw = clutter_stage_real_queue_redraw;
  actor_class->apply_transform = clutter_stage_real_apply_transform;

  /**
   * ClutterStage:fullscreen:
   *
   * Whether the stage should be fullscreen or not.
   *
   * This property is set by calling clutter_stage_set_fullscreen()
   * but since the actual implementation is delegated to the backend
   * you should connect to the notify::fullscreen-set signal in order
   * to get notification if the fullscreen state has been successfully
   * achieved.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_boolean ("fullscreen-set",
                                P_("Fullscreen Set"),
                                P_("Whether the main stage is fullscreen"),
                                FALSE,
                                CLUTTER_PARAM_READABLE);
  g_object_class_install_property (gobject_class,
                                   PROP_FULLSCREEN_SET,
                                   pspec);
  /**
   * ClutterStage:offscreen:
   *
   * Whether the stage should be rendered in an offscreen buffer.
   *
   * Deprecated: 1.10: This property does not do anything.
   */
  pspec = g_param_spec_boolean ("offscreen",
                                P_("Offscreen"),
                                P_("Whether the main stage should be rendered offscreen"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE | G_PARAM_DEPRECATED);
  g_object_class_install_property (gobject_class,
                                   PROP_OFFSCREEN,
                                   pspec);
  /**
   * ClutterStage:cursor-visible:
   *
   * Whether the mouse pointer should be visible
   */
  pspec = g_param_spec_boolean ("cursor-visible",
                                P_("Cursor Visible"),
                                P_("Whether the mouse pointer is visible on the main stage"),
                                TRUE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_CURSOR_VISIBLE,
                                   pspec);
  /**
   * ClutterStage:user-resizable:
   *
   * Whether the stage is resizable via user interaction.
   *
   * Since: 0.4
   */
  pspec = g_param_spec_boolean ("user-resizable",
                                P_("User Resizable"),
                                P_("Whether the stage is able to be resized via user interaction"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_USER_RESIZABLE,
                                   pspec);
  /**
   * ClutterStage:color:
   *
   * The background color of the main stage.
   *
   * Deprecated: 1.10: Use the #ClutterActor:background-color property of
   *   #ClutterActor instead.
   */
  pspec = clutter_param_spec_color ("color",
                                    P_("Color"),
                                    P_("The color of the stage"),
                                    &default_stage_color,
                                    CLUTTER_PARAM_READWRITE |
                                    G_PARAM_DEPRECATED);
  g_object_class_install_property (gobject_class, PROP_COLOR, pspec);

  /**
   * ClutterStage:perspective:
   *
   * The parameters used for the perspective projection from 3D
   * coordinates to 2D
   *
   * Since: 0.8.2
   */
  pspec = g_param_spec_boxed ("perspective",
                              P_("Perspective"),
                              P_("Perspective projection parameters"),
                              CLUTTER_TYPE_PERSPECTIVE,
                              CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
                                   PROP_PERSPECTIVE,
                                   pspec);

  /**
   * ClutterStage:title:
   *
   * The stage's title - usually displayed in stage windows title decorations.
   *
   * Since: 0.4
   */
  pspec = g_param_spec_string ("title",
                               P_("Title"),
                               P_("Stage Title"),
                               NULL,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TITLE, pspec);

  /**
   * ClutterStage:use-fog:
   *
   * Whether the stage should use a linear GL "fog" in creating the
   * depth-cueing effect, to enhance the perception of depth by fading
   * actors farther from the viewpoint.
   *
   * Since: 0.6
   *
   * Deprecated: 1.10: This property does not do anything.
   */
  pspec = g_param_spec_boolean ("use-fog",
                                P_("Use Fog"),
                                P_("Whether to enable depth cueing"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE | G_PARAM_DEPRECATED);
  g_object_class_install_property (gobject_class, PROP_USE_FOG, pspec);

  /**
   * ClutterStage:fog:
   *
   * The settings for the GL "fog", used only if #ClutterStage:use-fog
   * is set to %TRUE
   *
   * Since: 1.0
   *
   * Deprecated: 1.10: This property does not do anything.
   */
  pspec = g_param_spec_boxed ("fog",
                              P_("Fog"),
                              P_("Settings for the depth cueing"),
                              CLUTTER_TYPE_FOG,
                              CLUTTER_PARAM_READWRITE | G_PARAM_DEPRECATED);
  g_object_class_install_property (gobject_class, PROP_FOG, pspec);

  /**
   * ClutterStage:use-alpha:
   *
   * Whether the #ClutterStage should honour the alpha component of the
   * #ClutterStage:color property when painting. If Clutter is run under
   * a compositing manager this will result in the stage being blended
   * with the underlying window(s)
   *
   * Since: 1.2
   */
  pspec = g_param_spec_boolean ("use-alpha",
                                P_("Use Alpha"),
                                P_("Whether to honour the alpha component of the stage color"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_USE_ALPHA, pspec);

  /**
   * ClutterStage:key-focus:
   *
   * The #ClutterActor that will receive key events from the underlying
   * windowing system.
   *
   * If %NULL, the #ClutterStage will receive the events.
   *
   * Since: 1.2
   */
  pspec = g_param_spec_object ("key-focus",
                               P_("Key Focus"),
                               P_("The currently key focused actor"),
                               CLUTTER_TYPE_ACTOR,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_KEY_FOCUS, pspec);

  /**
   * ClutterStage:no-clear-hint:
   *
   * Whether or not the #ClutterStage should clear its contents
   * before each paint cycle.
   *
   * See clutter_stage_set_no_clear_hint() for further information.
   *
   * Since: 1.4
   */
  pspec = g_param_spec_boolean ("no-clear-hint",
                                P_("No Clear Hint"),
                                P_("Whether the stage should clear its contents"),
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_NO_CLEAR_HINT, pspec);

  /**
   * ClutterStage:accept-focus:
   *
   * Whether the #ClutterStage should accept key focus when shown.
   *
   * Since: 1.6
   */
  pspec = g_param_spec_boolean ("accept-focus",
                                P_("Accept Focus"),
                                P_("Whether the stage should accept focus on show"),
                                TRUE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_ACCEPT_FOCUS, pspec);

  /**
   * ClutterStage::fullscreen:
   * @stage: the stage which was fullscreened
   *
   * The ::fullscreen signal is emitted when the stage is made fullscreen.
   *
   * Since: 0.6
   */
  stage_signals[FULLSCREEN] =
    g_signal_new (I_("fullscreen"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ClutterStageClass, fullscreen),
		  NULL, NULL,
		  _clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterStage::unfullscreen:
   * @stage: the stage which has left a fullscreen state.
   *
   * The ::unfullscreen signal is emitted when the stage leaves a fullscreen
   * state.
   *
   * Since: 0.6
   */
  stage_signals[UNFULLSCREEN] =
    g_signal_new (I_("unfullscreen"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, unfullscreen),
		  NULL, NULL,
		  _clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterStage::activate:
   * @stage: the stage which was activated
   *
   * The ::activate signal is emitted when the stage receives key focus
   * from the underlying window system.
   *
   * Since: 0.6
   */
  stage_signals[ACTIVATE] =
    g_signal_new (I_("activate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, activate),
		  NULL, NULL,
		  _clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterStage::deactivate:
   * @stage: the stage which was deactivated
   *
   * The ::activate signal is emitted when the stage loses key focus
   * from the underlying window system.
   *
   * Since: 0.6
   */
  stage_signals[DEACTIVATE] =
    g_signal_new (I_("deactivate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, deactivate),
		  NULL, NULL,
		  _clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * ClutterStage::delete-event:
   * @stage: the stage that received the event
   * @event: a #ClutterEvent of type %CLUTTER_DELETE
   *
   * The ::delete-event signal is emitted when the user closes a
   * #ClutterStage window using the window controls.
   *
   * Clutter by default will call clutter_main_quit() if @stage is
   * the default stage, and clutter_actor_destroy() for any other
   * stage.
   *
   * It is possible to override the default behaviour by connecting
   * a new handler and returning %TRUE there.
   *
   * This signal is emitted only on Clutter backends that
   * embed #ClutterStage in native windows. It is not emitted for
   * backends that use a static frame buffer.
   *
   * Since: 1.2
   */
  stage_signals[DELETE_EVENT] =
    g_signal_new (I_("delete-event"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterStageClass, delete_event),
                  _clutter_boolean_handled_accumulator, NULL,
                  _clutter_marshal_BOOLEAN__BOXED,
                  G_TYPE_BOOLEAN, 1,
                  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  klass->fullscreen = clutter_stage_real_fullscreen;
  klass->activate = clutter_stage_real_activate;
  klass->deactivate = clutter_stage_real_deactivate;
  klass->delete_event = clutter_stage_real_delete_event;
}

static void
clutter_stage_notify_min_size (ClutterStage *self)
{
  self->priv->min_size_changed = TRUE;
}

static void
clutter_stage_init (ClutterStage *self)
{
  cairo_rectangle_int_t geom = { 0, };
  ClutterStagePrivate *priv;
  ClutterStageWindow *impl;
  ClutterBackend *backend;
  int window_scale = 1;
  GError *error;

  /* a stage is a top-level object */
  CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_IS_TOPLEVEL);

  self->priv = priv = clutter_stage_get_instance_private (self);

  CLUTTER_NOTE (BACKEND, "Creating stage from the default backend");
  backend = clutter_get_default_backend ();

  error = NULL;
  impl = _clutter_backend_create_stage (backend, self, &error);

  if (G_LIKELY (impl != NULL))
    {
      _clutter_stage_set_window (self, impl);
      _clutter_stage_window_get_geometry (priv->impl, &geom);
      window_scale = _clutter_stage_window_get_scale_factor (priv->impl);
    }
  else
    {
      if (error != NULL)
        {
          g_critical ("Unable to create a new stage implementation: %s",
                      error->message);
          g_error_free (error);
        }
      else
        g_critical ("Unable to create a new stage implementation.");
    }

  priv->event_queue = g_queue_new ();

  priv->is_fullscreen = FALSE;
  priv->is_user_resizable = FALSE;
  priv->is_cursor_visible = TRUE;
  priv->use_fog = FALSE;
  priv->throttle_motion_events = TRUE;
  priv->min_size_changed = FALSE;
  priv->sync_delay = -1;

  /* XXX - we need to keep the invariant that calling
   * clutter_set_motion_event_enabled() before the stage creation
   * will cause motion event delivery to be disabled on any newly
   * created stage. this can go away when we break API and remove
   * deprecated functions.
   */
  priv->motion_events_enabled = _clutter_context_get_motion_events_enabled ();

  clutter_actor_set_background_color (CLUTTER_ACTOR (self),
                                      &default_stage_color);

  priv->perspective.fovy   = 60.0; /* 60 Degrees */
  priv->perspective.aspect = (float) geom.width / (float) geom.height;
  priv->perspective.z_near = 0.1;
  priv->perspective.z_far  = 100.0;

  cogl_matrix_init_identity (&priv->projection);
  cogl_matrix_perspective (&priv->projection,
                           priv->perspective.fovy,
                           priv->perspective.aspect,
                           priv->perspective.z_near,
                           priv->perspective.z_far);
  cogl_matrix_get_inverse (&priv->projection,
                           &priv->inverse_projection);
  cogl_matrix_init_identity (&priv->view);
  cogl_matrix_view_2d_in_perspective (&priv->view,
                                      priv->perspective.fovy,
                                      priv->perspective.aspect,
                                      priv->perspective.z_near,
                                      50, /* distance to 2d plane */
                                      geom.width * window_scale,
                                      geom.height * window_scale);


  /* FIXME - remove for 2.0 */
  priv->fog.z_near = 1.0;
  priv->fog.z_far  = 2.0;

  priv->relayout_pending = TRUE;

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);
  clutter_stage_set_title (self, g_get_prgname ());
  clutter_stage_set_key_focus (self, NULL);

  g_signal_connect (self, "notify::min-width",
                    G_CALLBACK (clutter_stage_notify_min_size), NULL);
  g_signal_connect (self, "notify::min-height",
                    G_CALLBACK (clutter_stage_notify_min_size), NULL);

  _clutter_stage_set_viewport (self,
                               0, 0,
                               geom.width,
                               geom.height);

  priv->paint_volume_stack =
    g_array_new (FALSE, FALSE, sizeof (ClutterPaintVolume));

  priv->pick_id_pool = _clutter_id_pool_new (256);
}

/**
 * clutter_stage_get_default:
 *
 * Retrieves a #ClutterStage singleton.
 *
 * This function is not as useful as it sounds, and will most likely
 * by deprecated in the future. Application code should only create
 * a #ClutterStage instance using clutter_stage_new(), and manage the
 * lifetime of the stage manually.
 *
 * The default stage singleton has a platform-specific behaviour: on
 * platforms without the %CLUTTER_FEATURE_STAGE_MULTIPLE feature flag
 * set, the first #ClutterStage instance will also be set to be the
 * default stage instance, and this function will always return a
 * pointer to it.
 *
 * On platforms with the %CLUTTER_FEATURE_STAGE_MULTIPLE feature flag
 * set, the default stage will be created by the first call to this
 * function, and every following call will return the same pointer to
 * it.
 *
 * Return value: (transfer none) (type Clutter.Stage): the main
 *   #ClutterStage. You should never destroy or unref the returned
 *   actor.
 *
 * Deprecated: 1.10: Use clutter_stage_new() instead.
 */
ClutterActor *
clutter_stage_get_default (void)
{
  ClutterStageManager *stage_manager = clutter_stage_manager_get_default ();
  ClutterStage *stage;

  stage = clutter_stage_manager_get_default_stage (stage_manager);
  if (G_UNLIKELY (stage == NULL))
    {
      /* This will take care of automatically adding the stage to the
       * stage manager and setting it as the default. Its floating
       * reference will be claimed by the stage manager.
       */
      stage = g_object_new (CLUTTER_TYPE_STAGE, NULL);
      _clutter_stage_manager_set_default_stage (stage_manager, stage);

      /* the default stage is realized by default */
      clutter_actor_realize (CLUTTER_ACTOR (stage));
    }

  return CLUTTER_ACTOR (stage);
}

/**
 * clutter_stage_set_color:
 * @stage: A #ClutterStage
 * @color: A #ClutterColor
 *
 * Sets the stage color.
 *
 * Deprecated: 1.10: Use clutter_actor_set_background_color() instead.
 */
void
clutter_stage_set_color (ClutterStage       *stage,
			 const ClutterColor *color)
{
  clutter_actor_set_background_color (CLUTTER_ACTOR (stage), color);

  g_object_notify (G_OBJECT (stage), "color");
}

/**
 * clutter_stage_get_color:
 * @stage: A #ClutterStage
 * @color: (out caller-allocates): return location for a #ClutterColor
 *
 * Retrieves the stage color.
 *
 * Deprecated: 1.10: Use clutter_actor_get_background_color() instead.
 */
void
clutter_stage_get_color (ClutterStage *stage,
			 ClutterColor *color)
{
  clutter_actor_get_background_color (CLUTTER_ACTOR (stage), color);
}

static void
clutter_stage_set_perspective_internal (ClutterStage       *stage,
                                        ClutterPerspective *perspective)
{
  ClutterStagePrivate *priv = stage->priv;

  if (priv->perspective.fovy == perspective->fovy &&
      priv->perspective.aspect == perspective->aspect &&
      priv->perspective.z_near == perspective->z_near &&
      priv->perspective.z_far == perspective->z_far)
    return;

  priv->perspective = *perspective;

  cogl_matrix_init_identity (&priv->projection);
  cogl_matrix_perspective (&priv->projection,
                           priv->perspective.fovy,
                           priv->perspective.aspect,
                           priv->perspective.z_near,
                           priv->perspective.z_far);
  cogl_matrix_get_inverse (&priv->projection,
                           &priv->inverse_projection);

  priv->dirty_projection = TRUE;
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

/**
 * clutter_stage_set_perspective:
 * @stage: A #ClutterStage
 * @perspective: A #ClutterPerspective
 *
 * Sets the stage perspective. Using this function is not recommended
 * because it will disable Clutter's attempts to generate an
 * appropriate perspective based on the size of the stage.
 */
void
clutter_stage_set_perspective (ClutterStage       *stage,
                               ClutterPerspective *perspective)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (perspective != NULL);
  g_return_if_fail (perspective->z_far - perspective->z_near != 0);

  priv = stage->priv;

  /* If the application ever calls this function then we'll stop
     automatically updating the perspective when the stage changes
     size */
  priv->has_custom_perspective = TRUE;

  clutter_stage_set_perspective_internal (stage, perspective);
}

/**
 * clutter_stage_get_perspective:
 * @stage: A #ClutterStage
 * @perspective: (out caller-allocates) (allow-none): return location for a
 *   #ClutterPerspective
 *
 * Retrieves the stage perspective.
 */
void
clutter_stage_get_perspective (ClutterStage       *stage,
                               ClutterPerspective *perspective)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (perspective != NULL);

  *perspective = stage->priv->perspective;
}

/*
 * clutter_stage_get_projection_matrix:
 * @stage: A #ClutterStage
 * @projection: return location for a #CoglMatrix representing the
 *              perspective projection applied to actors on the given
 *              @stage.
 *
 * Retrieves the @stage's projection matrix. This is derived from the
 * current perspective set using clutter_stage_set_perspective().
 *
 * Since: 1.6
 */
void
_clutter_stage_get_projection_matrix (ClutterStage *stage,
                                      CoglMatrix *projection)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (projection != NULL);

  *projection = stage->priv->projection;
}

/* This simply provides a simple mechanism for us to ensure that
 * the projection matrix gets re-asserted before painting.
 *
 * This is used when switching between multiple stages */
void
_clutter_stage_dirty_projection (ClutterStage *stage)
{
  stage->priv->dirty_projection = TRUE;
}

/*
 * clutter_stage_set_viewport:
 * @stage: A #ClutterStage
 * @x: The X postition to render the stage at, in window coordinates
 * @y: The Y position to render the stage at, in window coordinates
 * @width: The width to render the stage at, in window coordinates
 * @height: The height to render the stage at, in window coordinates
 *
 * Sets the stage viewport. The viewport defines a final scale and
 * translation of your rendered stage and actors. This lets you render
 * your stage into a subregion of the stage window or you could use it to
 * pan a subregion of the stage if your stage window is smaller then
 * the stage. (XXX: currently this isn't possible)
 *
 * Unlike a scale and translation done using the modelview matrix this
 * is done after everything has had perspective projection applied, so
 * for example if you were to pan across a subregion of the stage using
 * the viewport then you would not see a change in perspective for the
 * actors on the stage.
 *
 * Normally the stage viewport will automatically track the size of the
 * stage window with no offset so the stage will fill your window. This
 * behaviour can be changed with the "viewport-mimics-window" property
 * which will automatically be set to FALSE if you use this API. If
 * you want to revert to the original behaviour then you should set
 * this property back to %TRUE using
 * clutter_stage_set_viewport_mimics_window().
 * (XXX: If we were to make this API public then we might want to do
 *  add that property.)
 *
 * Note: currently this interface only support integer precision
 * offsets and sizes for viewports but the interface takes floats because
 * OpenGL 4.0 has introduced floating point viewports which we might
 * want to expose via this API eventually.
 *
 * Since: 1.6
 */
void
_clutter_stage_set_viewport (ClutterStage *stage,
                             float         x,
                             float         y,
                             float         width,
                             float         height)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;


  if (x == priv->viewport[0] &&
      y == priv->viewport[1] &&
      width == priv->viewport[2] &&
      height == priv->viewport[3])
    return;

  priv->viewport[0] = x;
  priv->viewport[1] = y;
  priv->viewport[2] = width;
  priv->viewport[3] = height;

  priv->dirty_viewport = TRUE;

  queue_full_redraw (stage);
}

/* This simply provides a simple mechanism for us to ensure that
 * the viewport gets re-asserted before next painting.
 *
 * This is used when switching between multiple stages */
void
_clutter_stage_dirty_viewport (ClutterStage *stage)
{
  stage->priv->dirty_viewport = TRUE;
}

/*
 * clutter_stage_get_viewport:
 * @stage: A #ClutterStage
 * @x: A location for the X position where the stage is rendered,
 *     in window coordinates.
 * @y: A location for the Y position where the stage is rendered,
 *     in window coordinates.
 * @width: A location for the width the stage is rendered at,
 *         in window coordinates.
 * @height: A location for the height the stage is rendered at,
 *          in window coordinates.
 *
 * Returns the viewport offset and size set using
 * clutter_stage_set_viewport() or if the "viewport-mimics-window" property
 * is TRUE then @x and @y will be set to 0 and @width and @height will equal
 * the width if the stage window.
 *
 * Since: 1.6
 */
void
_clutter_stage_get_viewport (ClutterStage *stage,
                             float        *x,
                             float        *y,
                             float        *width,
                             float        *height)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  *x = priv->viewport[0];
  *y = priv->viewport[1];
  *width = priv->viewport[2];
  *height = priv->viewport[3];
}

/**
 * clutter_stage_set_fullscreen:
 * @stage: a #ClutterStage
 * @fullscreen: %TRUE to to set the stage fullscreen
 *
 * Asks to place the stage window in the fullscreen or unfullscreen
 * states.
 *
 ( Note that you shouldn't assume the window is definitely full screen
 * afterward, because other entities (e.g. the user or window manager)
 * could unfullscreen it again, and not all window managers honor
 * requests to fullscreen windows.
 *
 * If you want to receive notification of the fullscreen state you
 * should either use the #ClutterStage::fullscreen and
 * #ClutterStage::unfullscreen signals, or use the notify signal
 * for the #ClutterStage:fullscreen-set property
 *
 * Since: 1.0
 */
void
clutter_stage_set_fullscreen (ClutterStage *stage,
                              gboolean      fullscreen)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (priv->is_fullscreen != fullscreen)
    {
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);

      /* Only set if backend implements.
       *
       * Also see clutter_stage_event() for setting priv->is_fullscreen
       * on state change event.
       */
      if (iface->set_fullscreen)
	iface->set_fullscreen (impl, fullscreen);
    }

  /* If the backend did fullscreen the stage window then we need to resize
   * the stage and update its viewport so we queue a relayout.  Note: if the
   * fullscreen request is handled asynchronously we can't rely on this
   * queue_relayout to update the viewport, but for example the X backend
   * will recieve a ConfigureNotify after a successful resize which is how
   * we ensure the viewport is updated on X.
   */
  clutter_actor_queue_relayout (CLUTTER_ACTOR (stage));
}

/**
 * clutter_stage_get_fullscreen:
 * @stage: a #ClutterStage
 *
 * Retrieves whether the stage is full screen or not
 *
 * Return value: %TRUE if the stage is full screen
 *
 * Since: 1.0
 */
gboolean
clutter_stage_get_fullscreen (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->is_fullscreen;
}

/**
 * clutter_stage_set_user_resizable:
 * @stage: a #ClutterStage
 * @resizable: whether the stage should be user resizable.
 *
 * Sets if the stage is resizable by user interaction (e.g. via
 * window manager controls)
 *
 * Since: 0.4
 */
void
clutter_stage_set_user_resizable (ClutterStage *stage,
                                  gboolean      resizable)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (clutter_feature_available (CLUTTER_FEATURE_STAGE_USER_RESIZE)
      && priv->is_user_resizable != resizable)
    {
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);
      if (iface->set_user_resizable)
        {
          priv->is_user_resizable = resizable;

          iface->set_user_resizable (impl, resizable);

          g_object_notify (G_OBJECT (stage), "user-resizable");
        }
    }
}

/**
 * clutter_stage_get_user_resizable:
 * @stage: a #ClutterStage
 *
 * Retrieves the value set with clutter_stage_set_user_resizable().
 *
 * Return value: %TRUE if the stage is resizable by the user.
 *
 * Since: 0.4
 */
gboolean
clutter_stage_get_user_resizable (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->is_user_resizable;
}

/**
 * clutter_stage_show_cursor:
 * @stage: a #ClutterStage
 *
 * Shows the cursor on the stage window
 */
void
clutter_stage_show_cursor (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;
  if (!priv->is_cursor_visible)
    {
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);
      if (iface->set_cursor_visible)
        {
          priv->is_cursor_visible = TRUE;

          iface->set_cursor_visible (impl, TRUE);

          g_object_notify (G_OBJECT (stage), "cursor-visible");
        }
    }
}

/**
 * clutter_stage_hide_cursor:
 * @stage: a #ClutterStage
 *
 * Makes the cursor invisible on the stage window
 *
 * Since: 0.4
 */
void
clutter_stage_hide_cursor (ClutterStage *stage)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;
  if (priv->is_cursor_visible)
    {
      ClutterStageWindow *impl = CLUTTER_STAGE_WINDOW (priv->impl);
      ClutterStageWindowIface *iface;

      iface = CLUTTER_STAGE_WINDOW_GET_IFACE (impl);
      if (iface->set_cursor_visible)
        {
          priv->is_cursor_visible = FALSE;

          iface->set_cursor_visible (impl, FALSE);

          g_object_notify (G_OBJECT (stage), "cursor-visible");
        }
    }
}

/**
 * clutter_stage_read_pixels:
 * @stage: A #ClutterStage
 * @x: x coordinate of the first pixel that is read from stage
 * @y: y coordinate of the first pixel that is read from stage
 * @width: Width dimention of pixels to be read, or -1 for the
 *   entire stage width
 * @height: Height dimention of pixels to be read, or -1 for the
 *   entire stage height
 *
 * Makes a screenshot of the stage in RGBA 8bit data, returns a
 * linear buffer with @width * 4 as rowstride.
 *
 * The alpha data contained in the returned buffer is driver-dependent,
 * and not guaranteed to hold any sensible value.
 *
 * Return value: (transfer full) (array): a pointer to newly allocated memory with the buffer
 *   or %NULL if the read failed. Use g_free() on the returned data
 *   to release the resources it has allocated.
 */
guchar *
clutter_stage_read_pixels (ClutterStage *stage,
                           gint          x,
                           gint          y,
                           gint          width,
                           gint          height)
{
  ClutterActorBox box;
  guchar *pixels;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  /* Force a redraw of the stage before reading back pixels */
  clutter_stage_ensure_current (stage);
  clutter_actor_paint (CLUTTER_ACTOR (stage));

  clutter_actor_get_allocation_box (CLUTTER_ACTOR (stage), &box);

  if (width < 0)
    width = ceilf (box.x2 - box.x1);

  if (height < 0)
    height = ceilf (box.y2 - box.y1);

  pixels = g_malloc (height * width * 4);

  cogl_read_pixels (x, y, width, height,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    pixels);

  return pixels;
}

/**
 * clutter_stage_get_actor_at_pos:
 * @stage: a #ClutterStage
 * @pick_mode: how the scene graph should be painted
 * @x: X coordinate to check
 * @y: Y coordinate to check
 *
 * Checks the scene at the coordinates @x and @y and returns a pointer
 * to the #ClutterActor at those coordinates.
 *
 * By using @pick_mode it is possible to control which actors will be
 * painted and thus available.
 *
 * Return value: (transfer none): the actor at the specified coordinates,
 *   if any
 */
ClutterActor *
clutter_stage_get_actor_at_pos (ClutterStage    *stage,
                                ClutterPickMode  pick_mode,
                                gint             x,
                                gint             y)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  return _clutter_stage_do_pick (stage, x, y, pick_mode);
}

/**
 * clutter_stage_event:
 * @stage: a #ClutterStage
 * @event: a #ClutterEvent
 *
 * This function is used to emit an event on the main stage.
 *
 * You should rarely need to use this function, except for
 * synthetised events.
 *
 * Return value: the return value from the signal emission
 *
 * Since: 0.4
 */
gboolean
clutter_stage_event (ClutterStage *stage,
                     ClutterEvent *event)
{
  ClutterStagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  priv = stage->priv;

  if (event->type == CLUTTER_DELETE)
    {
      gboolean retval = FALSE;

      g_signal_emit_by_name (stage, "event", event, &retval);

      if (!retval)
        g_signal_emit_by_name (stage, "delete-event", event, &retval);

      return retval;
    }

  if (event->type != CLUTTER_STAGE_STATE)
    return FALSE;

  /* emit raw event */
  if (clutter_actor_event (CLUTTER_ACTOR (stage), event, FALSE))
    return TRUE;

  if (event->stage_state.changed_mask & CLUTTER_STAGE_STATE_FULLSCREEN)
    {
      if (event->stage_state.new_state & CLUTTER_STAGE_STATE_FULLSCREEN)
	{
	  priv->is_fullscreen = TRUE;
	  g_signal_emit (stage, stage_signals[FULLSCREEN], 0);

          g_object_notify (G_OBJECT (stage), "fullscreen-set");
	}
      else
	{
	  priv->is_fullscreen = FALSE;
	  g_signal_emit (stage, stage_signals[UNFULLSCREEN], 0);

          g_object_notify (G_OBJECT (stage), "fullscreen-set");
	}
    }

  if (event->stage_state.changed_mask & CLUTTER_STAGE_STATE_ACTIVATED)
    {
      if (event->stage_state.new_state & CLUTTER_STAGE_STATE_ACTIVATED)
	g_signal_emit (stage, stage_signals[ACTIVATE], 0);
      else
	g_signal_emit (stage, stage_signals[DEACTIVATE], 0);
    }

  return TRUE;
}

/**
 * clutter_stage_set_title:
 * @stage: A #ClutterStage
 * @title: A utf8 string for the stage windows title.
 *
 * Sets the stage title.
 *
 * Since: 0.4
 **/
void
clutter_stage_set_title (ClutterStage       *stage,
			 const gchar        *title)
{
  ClutterStagePrivate *priv;
  ClutterStageWindow *impl;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  g_free (priv->title);
  priv->title = g_strdup (title);

  impl = CLUTTER_STAGE_WINDOW (priv->impl);
  if (CLUTTER_STAGE_WINDOW_GET_IFACE(impl)->set_title != NULL)
    CLUTTER_STAGE_WINDOW_GET_IFACE (impl)->set_title (impl, priv->title);

  g_object_notify (G_OBJECT (stage), "title");
}

/**
 * clutter_stage_get_title:
 * @stage: A #ClutterStage
 *
 * Gets the stage title.
 *
 * Return value: pointer to the title string for the stage. The
 * returned string is owned by the actor and should not
 * be modified or freed.
 *
 * Since: 0.4
 **/
const gchar *
clutter_stage_get_title (ClutterStage       *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  return stage->priv->title;
}

static void
on_key_focus_destroy (ClutterActor *actor,
                      ClutterStage *stage)
{
  /* unset the key focus */
  clutter_stage_set_key_focus (stage, NULL);
}

/**
 * clutter_stage_set_key_focus:
 * @stage: the #ClutterStage
 * @actor: (allow-none): the actor to set key focus to, or %NULL
 *
 * Sets the key focus on @actor. An actor with key focus will receive
 * all the key events. If @actor is %NULL, the stage will receive
 * focus.
 *
 * Since: 0.6
 */
void
clutter_stage_set_key_focus (ClutterStage *stage,
			     ClutterActor *actor)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (actor == NULL || CLUTTER_IS_ACTOR (actor));

  priv = stage->priv;

  /* normalize the key focus. NULL == stage */
  if (actor == CLUTTER_ACTOR (stage))
    actor = NULL;

  /* avoid emitting signals and notifications if we're setting the same
   * actor as the key focus
   */
  if (priv->key_focused_actor == actor)
    return;

  if (priv->key_focused_actor != NULL)
    {
      ClutterActor *old_focused_actor;

      old_focused_actor = priv->key_focused_actor;

      /* set key_focused_actor to NULL before emitting the signal or someone
       * might hide the previously focused actor in the signal handler and we'd
       * get re-entrant call and get glib critical from g_object_weak_unref
       */
      g_signal_handlers_disconnect_by_func (priv->key_focused_actor,
                                            G_CALLBACK (on_key_focus_destroy),
                                            stage);
      priv->key_focused_actor = NULL;

      g_signal_emit_by_name (old_focused_actor, "key-focus-out");
    }
  else
    g_signal_emit_by_name (stage, "key-focus-out");

  /* Note, if someone changes key focus in focus-out signal handler we'd be
   * overriding the latter call below moving the focus where it was originally
   * intended. The order of events would be:
   *   1st focus-out, 2nd focus-out (on stage), 2nd focus-in, 1st focus-in
   */
  if (actor != NULL)
    {
      priv->key_focused_actor = actor;

      g_signal_connect (actor,
                        "destroy", G_CALLBACK (on_key_focus_destroy),
                        stage);
      g_signal_emit_by_name (priv->key_focused_actor, "key-focus-in");
    }
  else
    g_signal_emit_by_name (stage, "key-focus-in");

  g_object_notify (G_OBJECT (stage), "key-focus");
}

/**
 * clutter_stage_get_key_focus:
 * @stage: the #ClutterStage
 *
 * Retrieves the actor that is currently under key focus.
 *
 * Return value: (transfer none): the actor with key focus, or the stage
 *
 * Since: 0.6
 */
ClutterActor *
clutter_stage_get_key_focus (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  if (stage->priv->key_focused_actor)
    return stage->priv->key_focused_actor;

  return CLUTTER_ACTOR (stage);
}

/**
 * clutter_stage_get_use_fog:
 * @stage: the #ClutterStage
 *
 * Gets whether the depth cueing effect is enabled on @stage.
 *
 * Return value: %TRUE if the depth cueing effect is enabled
 *
 * Since: 0.6
 *
 * Deprecated: 1.10: This function will always return %FALSE
 */
gboolean
clutter_stage_get_use_fog (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->use_fog;
}

/**
 * clutter_stage_set_use_fog:
 * @stage: the #ClutterStage
 * @fog: %TRUE for enabling the depth cueing effect
 *
 * Sets whether the depth cueing effect on the stage should be enabled
 * or not.
 *
 * Depth cueing is a 3D effect that makes actors farther away from the
 * viewing point less opaque, by fading them with the stage color.

 * The parameters of the GL fog used can be changed using the
 * clutter_stage_set_fog() function.
 *
 * Since: 0.6
 *
 * Deprecated: 1.10: Calling this function produces no visible effect
 */
void
clutter_stage_set_use_fog (ClutterStage *stage,
                           gboolean      fog)
{
}

/**
 * clutter_stage_set_fog:
 * @stage: the #ClutterStage
 * @fog: a #ClutterFog structure
 *
 * Sets the fog (also known as "depth cueing") settings for the @stage.
 *
 * A #ClutterStage will only use a linear fog progression, which
 * depends solely on the distance from the viewer. The cogl_set_fog()
 * function in COGL exposes more of the underlying implementation,
 * and allows changing the for progression function. It can be directly
 * used by disabling the #ClutterStage:use-fog property and connecting
 * a signal handler to the #ClutterActor::paint signal on the @stage,
 * like:
 *
 * |[
 *   clutter_stage_set_use_fog (stage, FALSE);
 *   g_signal_connect (stage, "paint", G_CALLBACK (on_stage_paint), NULL);
 * ]|
 *
 * The paint signal handler will call cogl_set_fog() with the
 * desired settings:
 *
 * |[
 *   static void
 *   on_stage_paint (ClutterActor *actor)
 *   {
 *     ClutterColor stage_color = { 0, };
 *     CoglColor fog_color = { 0, };
 *
 *     // set the fog color to the stage background color
 *     clutter_stage_get_color (CLUTTER_STAGE (actor), &stage_color);
 *     cogl_color_init_from_4ub (&fog_color,
 *                               stage_color.red,
 *                               stage_color.green,
 *                               stage_color.blue,
 *                               stage_color.alpha);
 *
 *     // enable fog //
 *     cogl_set_fog (&fog_color,
 *                   COGL_FOG_MODE_EXPONENTIAL, // mode
 *                   0.5,                       // density
 *                   5.0, 30.0);                // z_near and z_far
 *   }
 * ]|
 *
 * The fogging functions only work correctly when the visible actors use
 * unmultiplied alpha colors. By default Cogl will premultiply textures and
 * cogl_set_source_color() will premultiply colors, so unless you explicitly
 * load your textures requesting an unmultiplied internal format and use
 * cogl_material_set_color() you can only use fogging with fully opaque actors.
 * Support for premultiplied colors will improve in the future when we can
 * depend on fragment shaders.
 *
 * Since: 0.6
 *
 * Deprecated: 1.10: Fog settings are ignored.
 */
void
clutter_stage_set_fog (ClutterStage *stage,
                       ClutterFog   *fog)
{
}

/**
 * clutter_stage_get_fog:
 * @stage: the #ClutterStage
 * @fog: (out): return location for a #ClutterFog structure
 *
 * Retrieves the current depth cueing settings from the stage.
 *
 * Since: 0.6
 *
 * Deprecated: 1.10: This function will always return the default
 *   values of #ClutterFog
 */
void
clutter_stage_get_fog (ClutterStage *stage,
                        ClutterFog   *fog)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (fog != NULL);

  *fog = stage->priv->fog;
}

/*** Perspective boxed type ******/

static gpointer
clutter_perspective_copy (gpointer data)
{
  if (G_LIKELY (data))
    return g_slice_dup (ClutterPerspective, data);

  return NULL;
}

static void
clutter_perspective_free (gpointer data)
{
  if (G_LIKELY (data))
    g_slice_free (ClutterPerspective, data);
}

G_DEFINE_BOXED_TYPE (ClutterPerspective, clutter_perspective,
                     clutter_perspective_copy,
                     clutter_perspective_free);

static gpointer
clutter_fog_copy (gpointer data)
{
  if (G_LIKELY (data))
    return g_slice_dup (ClutterFog, data);

  return NULL;
}

static void
clutter_fog_free (gpointer data)
{
  if (G_LIKELY (data))
    g_slice_free (ClutterFog, data);
}

G_DEFINE_BOXED_TYPE (ClutterFog, clutter_fog, clutter_fog_copy, clutter_fog_free);

/**
 * clutter_stage_new:
 *
 * Creates a new, non-default stage. A non-default stage is a new
 * top-level actor which can be used as another container. It works
 * exactly like the default stage, but while clutter_stage_get_default()
 * will always return the same instance, you will have to keep a pointer
 * to any #ClutterStage returned by clutter_stage_new().
 *
 * The ability to support multiple stages depends on the current
 * backend. Use clutter_feature_available() and
 * %CLUTTER_FEATURE_STAGE_MULTIPLE to check at runtime whether a
 * backend supports multiple stages.
 *
 * Return value: a new stage, or %NULL if the default backend does
 *   not support multiple stages. Use clutter_actor_destroy() to
 *   programmatically close the returned stage.
 *
 * Since: 0.8
 */
ClutterActor *
clutter_stage_new (void)
{
  return g_object_new (CLUTTER_TYPE_STAGE, NULL);
}

/**
 * clutter_stage_ensure_current:
 * @stage: the #ClutterStage
 *
 * This function essentially makes sure the right GL context is
 * current for the passed stage. It is not intended to
 * be used by applications.
 *
 * Since: 0.8
 */
void
clutter_stage_ensure_current (ClutterStage *stage)
{
  ClutterBackend *backend;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  backend = clutter_get_default_backend ();
  _clutter_backend_ensure_context (backend, stage);
}

/**
 * clutter_stage_ensure_viewport:
 * @stage: a #ClutterStage
 *
 * Ensures that the GL viewport is updated with the current
 * stage window size.
 *
 * This function will queue a redraw of @stage.
 *
 * This function should not be called by applications; it is used
 * when embedding a #ClutterStage into a toolkit with another
 * windowing system, like GTK+.
 *
 * Since: 1.0
 */
void
clutter_stage_ensure_viewport (ClutterStage *stage)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  _clutter_stage_dirty_viewport (stage);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

static void
clutter_stage_apply_scale (ClutterStage *stage)
{
  int factor;

  factor = _clutter_stage_window_get_scale_factor (stage->priv->impl);
  if (factor != 1)
    cogl_matrix_scale (&stage->priv->view, factor, factor, 1.f);
}

# define _DEG_TO_RAD(d)         ((d) * ((float) G_PI / 180.0f))

/* This calculates a distance into the view frustum to position the
 * stage so there is a decent amount of space to position geometry
 * between the stage and the near clipping plane.
 *
 * Some awkward issues with this problem are:
 * - It's not possible to have a gap as large as the stage size with
 *   a fov > 53° which is basically always the case since the default
 *   fov is 60°.
 *    - This can be deduced if you consider that this requires a
 *      triangle as wide as it is deep to fit in the frustum in front
 *      of the z_near plane. That triangle will always have an angle
 *      of 53.13° at the point sitting on the z_near plane, but if the
 *      frustum has a wider fov angle the left/right clipping planes
 *      can never converge with the two corners of our triangle no
 *      matter what size the triangle has.
 * - With a fov > 53° there is a trade off between maximizing the gap
 *   size relative to the stage size but not loosing depth precision.
 * - Perhaps ideally we wouldn't just consider the fov on the y-axis
 *   that is usually used to define a perspective, we would consider
 *   the fov of the axis with the largest stage size so the gap would
 *   accommodate that size best.
 *
 * After going around in circles a few times with how to handle these
 * issues, we decided in the end to go for the simplest solution to
 * start with instead of an elaborate function that handles arbitrary
 * fov angles that we currently have no use-case for.
 *
 * The solution assumes a fovy of 60° and for that case gives a gap
 * that's 85% of the stage height. We can consider more elaborate
 * functions if necessary later.
 *
 * One guide we had to steer the gap size we support is the
 * interactive test, test-texture-quality which expects to animate an
 * actor to +400 on the z axis with a stage size of 640x480. A gap
 * that's 85% of the stage height gives a gap of 408 in that case.
 */
static float
calculate_z_translation (float z_near)
{
  /* This solution uses fairly basic trigonometry, but is seems worth
   * clarifying the particular geometry we are looking at in-case
   * anyone wants to develop this further later. Not sure how well an
   * ascii diagram is going to work :-)
   *
   *    |--- stage_height ---|
   *    |     stage line     |
   *   ╲━━━━━━━━━━━━━━━━━━━━━╱------------
   *    ╲.  (2)   │        .╱       |   |
   *   C ╲ .      │      . ╱     gap|   |
   * =0.5°╲  . a  │    .  ╱         |   |
   *      b╲(1). D│  .   ╱          |   |
   *        ╲   B.│.    ╱near plane |   |
   *      A= ╲━━━━━━━━━╱-------------   |
   *     120° ╲ c │   ╱  |            z_2d
   *           ╲  │  ╱  z_near          |
   *       left ╲ │ ╱    |              |
   *       clip  60°fovy |              |
   *       plane  ╳----------------------
   *              |
   *              |
   *         origin line
   *
   * The area of interest is the triangle labeled (1) at the top left
   * marked with the ... line (a) from where the origin line crosses
   * the near plane to the top left where the stage line cross the
   * left clip plane.
   *
   * The sides of the triangle are a, b and c and the corresponding
   * angles opposite those sides are A, B and C.
   *
   * The angle of C is what trades off the gap size we have relative
   * to the stage size vs the depth precision we have.
   *
   * As mentioned above we arove at the angle for C is by working
   * backwards from how much space we want for test-texture-quality.
   * With a stage_height of 480 we want a gap > 400, ideally we also
   * wanted a somewhat round number as a percentage of the height for
   * documentation purposes. ~87% or a gap of ~416 is the limit
   * because that's where we approach a C angle of 0° and effectively
   * loose all depth precision.
   *
   * So for our test app with a stage_height of 480 if we aim for a
   * gap of 408 (85% of 480) we can get the angle D as
   * atan (stage_height/2/408) = 30.5°.
   *
   * That gives us the angle for B as 90° - 30.5° = 59.5°
   *
   * We can already determine that A has an angle of (fovy/2 + 90°) =
   * 120°
   *
   * Therefore C = 180 - A - B = 0.5°
   *
   * The length of c = z_near * tan (30°)
   *
   * Now we can use the rule a/SinA = c/SinC to calculate the
   * length of a. After some rearranging that gives us:
   *
   *      a              c
   *  ----------  =  ----------
   *  sin (120°)     sin (0.5°)
   *
   *      c * sin (120°)
   *  a = --------------
   *        sin (0.5°)
   *
   * And with that we can determine z_2d = cos (D) * a =
   * cos (30.5°) * a + z_near:
   *
   *         c * sin (120°) * cos (30.5°)
   *  z_2d = --------------------------- + z_near
   *                 sin (0.5°)
   */

   /* We expect the compiler should boil this down to z_near * CONSTANT
    * already, but just in case we use precomputed constants
    */
#if 0
# define A      tanf (_DEG_TO_RAD (30.f))
# define B      sinf (_DEG_TO_RAD (120.f))
# define C      cosf (_DEG_TO_RAD (30.5f))
# define D      sinf (_DEG_TO_RAD (.5f))
#else
# define A      0.57735025882720947265625f
# define B      0.866025388240814208984375f
# define C      0.86162912845611572265625f
# define D      0.00872653536498546600341796875f
#endif

  return z_near
       * A * B * C
       / D
       + z_near;
}

void
_clutter_stage_maybe_setup_viewport (ClutterStage *stage)
{
  ClutterStagePrivate *priv = stage->priv;

  if (priv->dirty_viewport)
    {
      ClutterPerspective perspective;
      int window_scale;
      float z_2d;

      CLUTTER_NOTE (PAINT,
                    "Setting up the viewport { w:%f, h:%f }",
                    priv->viewport[2],
                    priv->viewport[3]);

      window_scale = _clutter_stage_window_get_scale_factor (priv->impl);

      cogl_set_viewport (priv->viewport[0] * window_scale,
                         priv->viewport[1] * window_scale,
                         priv->viewport[2] * window_scale,
                         priv->viewport[3] * window_scale);

      perspective = priv->perspective;

      /* Ideally we want to regenerate the perspective matrix whenever
       * the size changes but if the user has provided a custom matrix
       * then we don't want to override it */
      if (!priv->has_custom_perspective)
        {
          perspective.aspect = priv->viewport[2] / priv->viewport[3];
          z_2d = calculate_z_translation (perspective.z_near);

          /* NB: z_2d is only enough room for 85% of the stage_height between
           * the stage and the z_near plane. For behind the stage plane we
           * want a more consistent gap of 10 times the stage_height before
           * hitting the far plane so we calculate that relative to the final
           * height of the stage plane at the z_2d_distance we got... */
          perspective.z_far = z_2d +
            tanf (_DEG_TO_RAD (perspective.fovy / 2.0f)) * z_2d * 20.0f;

          clutter_stage_set_perspective_internal (stage, &perspective);
        }
      else
        z_2d = calculate_z_translation (perspective.z_near);

      cogl_matrix_init_identity (&priv->view);
      cogl_matrix_view_2d_in_perspective (&priv->view,
                                          perspective.fovy,
                                          perspective.aspect,
                                          perspective.z_near,
                                          z_2d,
                                          priv->viewport[2] * window_scale,
                                          priv->viewport[3] * window_scale);

      clutter_stage_apply_scale (stage);

      priv->dirty_viewport = FALSE;
    }

  if (priv->dirty_projection)
    {
      cogl_set_projection_matrix (&priv->projection);

      priv->dirty_projection = FALSE;
    }
}

#undef _DEG_TO_RAD

/**
 * clutter_stage_ensure_redraw:
 * @stage: a #ClutterStage
 *
 * Ensures that @stage is redrawn
 *
 * This function should not be called by applications: it is
 * used when embedding a #ClutterStage into a toolkit with
 * another windowing system, like GTK+.
 *
 * Since: 1.0
 */
void
clutter_stage_ensure_redraw (ClutterStage *stage)
{
  ClutterMasterClock *master_clock;
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (!priv->relayout_pending && !priv->redraw_pending)
    _clutter_stage_schedule_update (stage);

  priv->relayout_pending = TRUE;
  priv->redraw_pending = TRUE;

  master_clock = _clutter_master_clock_get_default ();
  _clutter_master_clock_start_running (master_clock);
}

/**
 * clutter_stage_queue_redraw:
 * @stage: the #ClutterStage
 *
 * Queues a redraw for the passed stage.
 *
 * Applications should call clutter_actor_queue_redraw() and not
 * this function.
 *
 * Since: 0.8
 *
 * Deprecated: 1.10: Use clutter_actor_queue_redraw() instead.
 */
void
clutter_stage_queue_redraw (ClutterStage *stage)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}

/**
 * clutter_stage_is_default:
 * @stage: a #ClutterStage
 *
 * Checks if @stage is the default stage, or an instance created using
 * clutter_stage_new() but internally using the same implementation.
 *
 * Return value: %TRUE if the passed stage is the default one
 *
 * Since: 0.8
 *
 * Deprecated: 1.10: Track the stage pointer inside your application
 *   code, or use clutter_actor_get_stage() to retrieve the stage for
 *   a given actor.
 */
gboolean
clutter_stage_is_default (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage_is_default (stage);
}

void
_clutter_stage_set_window (ClutterStage       *stage,
                           ClutterStageWindow *stage_window)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (stage_window));

  if (stage->priv->impl != NULL)
    g_object_unref (stage->priv->impl);

  stage->priv->impl = stage_window;
}

ClutterStageWindow *
_clutter_stage_get_window (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  return CLUTTER_STAGE_WINDOW (stage->priv->impl);
}

ClutterStageWindow *
_clutter_stage_get_default_window (void)
{
  ClutterStageManager *manager = clutter_stage_manager_get_default ();
  ClutterStage *stage;

  stage = clutter_stage_manager_get_default_stage (manager);
  if (stage == NULL)
    return NULL;

  return _clutter_stage_get_window (stage);
}

/**
 * clutter_stage_set_throttle_motion_events:
 * @stage: a #ClutterStage
 * @throttle: %TRUE to throttle motion events
 *
 * Sets whether motion events received between redraws should
 * be throttled or not. If motion events are throttled, those
 * events received by the windowing system between redraws will
 * be compressed so that only the last event will be propagated
 * to the @stage and its actors.
 *
 * This function should only be used if you want to have all
 * the motion events delivered to your application code.
 *
 * Since: 1.0
 */
void
clutter_stage_set_throttle_motion_events (ClutterStage *stage,
                                          gboolean      throttle)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (priv->throttle_motion_events != throttle)
    priv->throttle_motion_events = throttle;
}

/**
 * clutter_stage_get_throttle_motion_events:
 * @stage: a #ClutterStage
 *
 * Retrieves the value set with clutter_stage_set_throttle_motion_events()
 *
 * Return value: %TRUE if the motion events are being throttled,
 *   and %FALSE otherwise
 *
 * Since: 1.0
 */
gboolean
clutter_stage_get_throttle_motion_events (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->throttle_motion_events;
}

/**
 * clutter_stage_set_use_alpha:
 * @stage: a #ClutterStage
 * @use_alpha: whether the stage should honour the opacity or the
 *   alpha channel of the stage color
 *
 * Sets whether the @stage should honour the #ClutterActor:opacity and
 * the alpha channel of the #ClutterStage:color
 *
 * Since: 1.2
 */
void
clutter_stage_set_use_alpha (ClutterStage *stage,
                             gboolean      use_alpha)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (priv->use_alpha != use_alpha)
    {
      priv->use_alpha = use_alpha;

      clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

      g_object_notify (G_OBJECT (stage), "use-alpha");
    }
}

/**
 * clutter_stage_get_use_alpha:
 * @stage: a #ClutterStage
 *
 * Retrieves the value set using clutter_stage_set_use_alpha()
 *
 * Return value: %TRUE if the stage should honour the opacity and the
 *   alpha channel of the stage color
 *
 * Since: 1.2
 */
gboolean
clutter_stage_get_use_alpha (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->use_alpha;
}

/**
 * clutter_stage_set_minimum_size:
 * @stage: a #ClutterStage
 * @width: width, in pixels
 * @height: height, in pixels
 *
 * Sets the minimum size for a stage window, if the default backend
 * uses #ClutterStage inside a window
 *
 * This is a convenience function, and it is equivalent to setting the
 * #ClutterActor:min-width and #ClutterActor:min-height on @stage
 *
 * If the current size of @stage is smaller than the minimum size, the
 * @stage will be resized to the new @width and @height
 *
 * This function has no effect if @stage is fullscreen
 *
 * Since: 1.2
 */
void
clutter_stage_set_minimum_size (ClutterStage *stage,
                                guint         width,
                                guint         height)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail ((width > 0) && (height > 0));

  g_object_set (G_OBJECT (stage),
                "min-width", (gfloat) width,
                "min-height", (gfloat )height,
                NULL);
}

/**
 * clutter_stage_get_minimum_size:
 * @stage: a #ClutterStage
 * @width: (out): return location for the minimum width, in pixels,
 *   or %NULL
 * @height: (out): return location for the minimum height, in pixels,
 *   or %NULL
 *
 * Retrieves the minimum size for a stage window as set using
 * clutter_stage_set_minimum_size().
 *
 * The returned size may not correspond to the actual minimum size and
 * it is specific to the #ClutterStage implementation inside the
 * Clutter backend
 *
 * Since: 1.2
 */
void
clutter_stage_get_minimum_size (ClutterStage *stage,
                                guint        *width_p,
                                guint        *height_p)
{
  gfloat width, height;
  gboolean width_set, height_set;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  g_object_get (G_OBJECT (stage),
                "min-width", &width,
                "min-width-set", &width_set,
                "min-height", &height,
                "min-height-set", &height_set,
                NULL);

  /* if not width or height have been set, then the Stage
   * minimum size is defined to be 1x1
   */
  if (!width_set)
    width = 1;

  if (!height_set)
    height = 1;

  if (width_p)
    *width_p = (guint) width;

  if (height_p)
    *height_p = (guint) height;
}

void
_clutter_stage_schedule_update (ClutterStage *stage)
{
  ClutterStageWindow *stage_window;

  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return;

  stage_window = _clutter_stage_get_window (stage);
  if (stage_window == NULL)
    return;

  return _clutter_stage_window_schedule_update (stage_window,
                                                stage->priv->sync_delay);
}

/* Returns the earliest time the stage is ready to update */
gint64
_clutter_stage_get_update_time (ClutterStage *stage)
{
  ClutterStageWindow *stage_window;

  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return 0;

  stage_window = _clutter_stage_get_window (stage);
  if (stage_window == NULL)
    return 0;

  return _clutter_stage_window_get_update_time (stage_window);
}

void
_clutter_stage_clear_update_time (ClutterStage *stage)
{
  ClutterStageWindow *stage_window;

  stage_window = _clutter_stage_get_window (stage);
  if (stage_window)
    _clutter_stage_window_clear_update_time (stage_window);
}

/**
 * clutter_stage_set_no_clear_hint:
 * @stage: a #ClutterStage
 * @no_clear: %TRUE if the @stage should not clear itself on every
 *   repaint cycle
 *
 * Sets whether the @stage should clear itself at the beginning
 * of each paint cycle or not.
 *
 * Clearing the #ClutterStage can be a costly operation, especially
 * if the stage is always covered - for instance, in a full-screen
 * video player or in a game with a background texture.
 *
 * This setting is a hint; Clutter might discard this hint
 * depending on its internal state.
 *
 * If parts of the stage are visible and you disable clearing you
 * might end up with visual artifacts while painting the contents of
 * the stage.
 *
 * Since: 1.4
 */
void
clutter_stage_set_no_clear_hint (ClutterStage *stage,
                                 gboolean      no_clear)
{
  ClutterStagePrivate *priv;
  ClutterStageHint new_hints;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;
  new_hints = priv->stage_hints;

  if (no_clear)
    new_hints |= CLUTTER_STAGE_NO_CLEAR_ON_PAINT;
  else
    new_hints &= ~CLUTTER_STAGE_NO_CLEAR_ON_PAINT;

  if (priv->stage_hints == new_hints)
    return;

  priv->stage_hints = new_hints;

  g_object_notify (G_OBJECT (stage), "no-clear-hint");
}

/**
 * clutter_stage_get_no_clear_hint:
 * @stage: a #ClutterStage
 *
 * Retrieves the hint set with clutter_stage_set_no_clear_hint()
 *
 * Return value: %TRUE if the stage should not clear itself on every paint
 *   cycle, and %FALSE otherwise
 *
 * Since: 1.4
 */
gboolean
clutter_stage_get_no_clear_hint (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return (stage->priv->stage_hints & CLUTTER_STAGE_NO_CLEAR_ON_PAINT) != 0;
}

ClutterPaintVolume *
_clutter_stage_paint_volume_stack_allocate (ClutterStage *stage)
{
  GArray *paint_volume_stack = stage->priv->paint_volume_stack;

  g_array_set_size (paint_volume_stack,
                    paint_volume_stack->len+1);

  return &g_array_index (paint_volume_stack,
                         ClutterPaintVolume,
                         paint_volume_stack->len - 1);
}

void
_clutter_stage_paint_volume_stack_free_all (ClutterStage *stage)
{
  GArray *paint_volume_stack = stage->priv->paint_volume_stack;
  int i;

  for (i = 0; i < paint_volume_stack->len; i++)
    {
      ClutterPaintVolume *pv =
        &g_array_index (paint_volume_stack, ClutterPaintVolume, i);
      clutter_paint_volume_free (pv);
    }

  g_array_set_size (paint_volume_stack, 0);
}

/* The is an out-of-band paramater available while painting that
 * can be used to cull actors. */
const ClutterPlane *
_clutter_stage_get_clip (ClutterStage *stage)
{
  return stage->priv->current_clip_planes;
}

/* When an actor queues a redraw we add it to a list on the stage that
 * gets processed once all updates to the stage have been finished.
 *
 * This deferred approach to processing queue_redraw requests means
 * that we can avoid redundant transformations of clip volumes if
 * something later triggers a full stage redraw anyway. It also means
 * we can be more sure that all the referenced actors will have valid
 * allocations improving the chance that we can determine the actors
 * paint volume so we can clip the redraw request even if the user
 * didn't explicitly do so.
 */
ClutterStageQueueRedrawEntry *
_clutter_stage_queue_actor_redraw (ClutterStage *stage,
                                   ClutterStageQueueRedrawEntry *entry,
                                   ClutterActor *actor,
                                   ClutterPaintVolume *clip)
{
  ClutterStagePrivate *priv = stage->priv;

  CLUTTER_NOTE (CLIPPING, "stage_queue_actor_redraw (actor=%s, clip=%p): ",
                _clutter_actor_get_debug_name (actor), clip);

  if (!priv->redraw_pending)
    {
      ClutterMasterClock *master_clock;

      CLUTTER_NOTE (PAINT, "First redraw request");

      _clutter_stage_schedule_update (stage);
      priv->redraw_pending = TRUE;

      master_clock = _clutter_master_clock_get_default ();
      _clutter_master_clock_start_running (master_clock);
    }
#ifdef CLUTTER_ENABLE_DEBUG
  else
    {
      CLUTTER_NOTE (PAINT, "Redraw request number %lu",
                    priv->redraw_count + 1);

      priv->redraw_count += 1;
    }
#endif /* CLUTTER_ENABLE_DEBUG */

  if (entry)
    {
      /* Ignore all requests to queue a redraw for an actor if a full
       * (non-clipped) redraw of the actor has already been queued. */
      if (!entry->has_clip)
        {
          CLUTTER_NOTE (CLIPPING, "Bail from stage_queue_actor_redraw (%s): "
                        "Unclipped redraw of actor already queued",
                        _clutter_actor_get_debug_name (actor));
          return entry;
        }

      /* If queuing a clipped redraw and a clipped redraw has
       * previously been queued for this actor then combine the latest
       * clip together with the existing clip */
      if (clip)
        clutter_paint_volume_union (&entry->clip, clip);
      else
        {
          clutter_paint_volume_free (&entry->clip);
          entry->has_clip = FALSE;
        }
      return entry;
    }
  else
    {
      entry = g_slice_new (ClutterStageQueueRedrawEntry);
      entry->actor = g_object_ref (actor);

      if (clip)
        {
          entry->has_clip = TRUE;
          _clutter_paint_volume_init_static (&entry->clip, actor);
          _clutter_paint_volume_set_from_volume (&entry->clip, clip);
        }
      else
        entry->has_clip = FALSE;

      stage->priv->pending_queue_redraws =
        g_list_prepend (stage->priv->pending_queue_redraws, entry);

      return entry;
    }
}

static void
free_queue_redraw_entry (ClutterStageQueueRedrawEntry *entry)
{
  if (entry->actor)
    g_object_unref (entry->actor);
  if (entry->has_clip)
    clutter_paint_volume_free (&entry->clip);
  g_slice_free (ClutterStageQueueRedrawEntry, entry);
}

void
_clutter_stage_queue_redraw_entry_invalidate (ClutterStageQueueRedrawEntry *entry)
{
  if (entry == NULL)
    return;

  if (entry->actor != NULL)
    {
      g_object_unref (entry->actor);
      entry->actor = NULL;
    }

  if (entry->has_clip)
    {
      clutter_paint_volume_free (&entry->clip);
      entry->has_clip = FALSE;
    }
}

static void
clutter_stage_maybe_finish_queue_redraws (ClutterStage *stage)
{
  /* Note: we have to repeat until the pending_queue_redraws list is
   * empty because actors are allowed to queue redraws in response to
   * the queue-redraw signal. For example Clone actors or
   * texture_new_from_actor actors will have to queue a redraw if
   * their source queues a redraw.
   */
  while (stage->priv->pending_queue_redraws)
    {
      GList *l;
      /* XXX: we need to allow stage->priv->pending_queue_redraws to
       * be updated while we process the current entries in the list
       * so we steal the list pointer and then reset it to an empty
       * list before processing... */
      GList *stolen_list = stage->priv->pending_queue_redraws;
      stage->priv->pending_queue_redraws = NULL;

      for (l = stolen_list; l; l = l->next)
        {
          ClutterStageQueueRedrawEntry *entry = l->data;
          ClutterPaintVolume *clip;

          /* NB: Entries may be invalidated if the actor gets destroyed */
          if (G_LIKELY (entry->actor != NULL))
	    {
	      clip = entry->has_clip ? &entry->clip : NULL;

	      _clutter_actor_finish_queue_redraw (entry->actor, clip);
	    }

          free_queue_redraw_entry (entry);
        }
      g_list_free (stolen_list);
    }
}

/**
 * clutter_stage_set_accept_focus:
 * @stage: a #ClutterStage
 * @accept_focus: %TRUE to accept focus on show
 *
 * Sets whether the @stage should accept the key focus when shown.
 *
 * This function should be called before showing @stage using
 * clutter_actor_show().
 *
 * Since: 1.6
 */
void
clutter_stage_set_accept_focus (ClutterStage *stage,
                                gboolean      accept_focus)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  accept_focus = !!accept_focus;

  priv = stage->priv;

  if (priv->accept_focus != accept_focus)
    {
      _clutter_stage_window_set_accept_focus (priv->impl, accept_focus);
      g_object_notify (G_OBJECT (stage), "accept-focus");
    }
}

/**
 * clutter_stage_get_accept_focus:
 * @stage: a #ClutterStage
 *
 * Retrieves the value set with clutter_stage_set_accept_focus().
 *
 * Return value: %TRUE if the #ClutterStage should accept focus, and %FALSE
 *   otherwise
 *
 * Since: 1.6
 */
gboolean
clutter_stage_get_accept_focus (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), TRUE);

  return stage->priv->accept_focus;
}

/**
 * clutter_stage_set_motion_events_enabled:
 * @stage: a #ClutterStage
 * @enabled: %TRUE to enable the motion events delivery, and %FALSE
 *   otherwise
 *
 * Sets whether per-actor motion events (and relative crossing
 * events) should be disabled or not.
 *
 * The default is %TRUE.
 *
 * If @enable is %FALSE the following signals will not be emitted
 * by the actors children of @stage:
 *
 *  - #ClutterActor::motion-event
 *  - #ClutterActor::enter-event
 *  - #ClutterActor::leave-event
 *
 * The events will still be delivered to the #ClutterStage.
 *
 * The main side effect of this function is that disabling the motion
 * events will disable picking to detect the #ClutterActor underneath
 * the pointer for each motion event. This is useful, for instance,
 * when dragging a #ClutterActor across the @stage: the actor underneath
 * the pointer is not going to change, so it's meaningless to perform
 * a pick.
 *
 * Since: 1.8
 */
void
clutter_stage_set_motion_events_enabled (ClutterStage *stage,
                                         gboolean      enabled)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  enabled = !!enabled;

  if (priv->motion_events_enabled != enabled)
    priv->motion_events_enabled = enabled;
}

/**
 * clutter_stage_get_motion_events_enabled:
 * @stage: a #ClutterStage
 *
 * Retrieves the value set using clutter_stage_set_motion_events_enabled().
 *
 * Return value: %TRUE if the per-actor motion event delivery is enabled
 *   and %FALSE otherwise
 *
 * Since: 1.8
 */
gboolean
clutter_stage_get_motion_events_enabled (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);

  return stage->priv->motion_events_enabled;
}

/* NB: The presumption shouldn't be that a stage can't be comprised
 * of multiple internal framebuffers, so instead of simply naming
 * this function _clutter_stage_get_framebuffer(), the "active"
 * infix is intended to clarify that it gets the framebuffer that
 * is currently in use/being painted.
 */
CoglFramebuffer *
_clutter_stage_get_active_framebuffer (ClutterStage *stage)
{
  return stage->priv->active_framebuffer;
}

gint32
_clutter_stage_acquire_pick_id (ClutterStage *stage,
                                ClutterActor *actor)
{
  ClutterStagePrivate *priv = stage->priv;

  g_assert (priv->pick_id_pool != NULL);

  return _clutter_id_pool_add (priv->pick_id_pool, actor);
}

void
_clutter_stage_release_pick_id (ClutterStage *stage,
                                gint32        pick_id)
{
  ClutterStagePrivate *priv = stage->priv;

  g_assert (priv->pick_id_pool != NULL);

  _clutter_id_pool_remove (priv->pick_id_pool, pick_id);
}

ClutterActor *
_clutter_stage_get_actor_by_pick_id (ClutterStage *stage,
                                     gint32        pick_id)
{
  ClutterStagePrivate *priv = stage->priv;

  g_assert (priv->pick_id_pool != NULL);

  return _clutter_id_pool_lookup (priv->pick_id_pool, pick_id);
}

void
_clutter_stage_add_pointer_drag_actor (ClutterStage       *stage,
                                       ClutterInputDevice *device,
                                       ClutterActor       *actor)
{
  GHashTable *drag_actors;

  drag_actors = g_object_get_data (G_OBJECT (stage),
                                   "__clutter_stage_pointer_drag_actors");
  if (drag_actors == NULL)
    {
      drag_actors = g_hash_table_new (NULL, NULL);
      g_object_set_data_full (G_OBJECT (stage),
                              "__clutter_stage_pointer_drag_actors",
                              drag_actors,
                              (GDestroyNotify) g_hash_table_destroy);
    }

  g_hash_table_replace (drag_actors, device, actor);
}

ClutterActor *
_clutter_stage_get_pointer_drag_actor (ClutterStage       *stage,
                                       ClutterInputDevice *device)
{
  GHashTable *drag_actors;

  drag_actors = g_object_get_data (G_OBJECT (stage),
                                   "__clutter_stage_pointer_drag_actors");
  if (drag_actors == NULL)
    return NULL;

  return g_hash_table_lookup (drag_actors, device);
}

void
_clutter_stage_remove_pointer_drag_actor (ClutterStage       *stage,
                                          ClutterInputDevice *device)
{
  GHashTable *drag_actors;

  drag_actors = g_object_get_data (G_OBJECT (stage),
                                   "__clutter_stage_pointer_drag_actors");
  if (drag_actors == NULL)
    return;

  g_hash_table_remove (drag_actors, device);

  if (g_hash_table_size (drag_actors) == 0)
    g_object_set_data (G_OBJECT (stage),
                       "__clutter_stage_pointer_drag_actors",
                       NULL);
}

void
_clutter_stage_add_touch_drag_actor (ClutterStage         *stage,
                                     ClutterEventSequence *sequence,
                                     ClutterActor         *actor)
{
  GHashTable *drag_actors;

  drag_actors = g_object_get_data (G_OBJECT (stage),
                                   "__clutter_stage_touch_drag_actors");
  if (drag_actors == NULL)
    {
      drag_actors = g_hash_table_new (NULL, NULL);
      g_object_set_data_full (G_OBJECT (stage),
                              "__clutter_stage_touch_drag_actors",
                              drag_actors,
                              (GDestroyNotify) g_hash_table_destroy);
    }

  g_hash_table_replace (drag_actors, sequence, actor);
}

ClutterActor *
_clutter_stage_get_touch_drag_actor (ClutterStage         *stage,
                                     ClutterEventSequence *sequence)
{
  GHashTable *drag_actors;

  drag_actors = g_object_get_data (G_OBJECT (stage),
                                   "__clutter_stage_touch_drag_actors");
  if (drag_actors == NULL)
    return NULL;

  return g_hash_table_lookup (drag_actors, sequence);
}

void
_clutter_stage_remove_touch_drag_actor (ClutterStage         *stage,
                                        ClutterEventSequence *sequence)
{
  GHashTable *drag_actors;

  drag_actors = g_object_get_data (G_OBJECT (stage),
                                   "__clutter_stage_touch_drag_actors");
  if (drag_actors == NULL)
    return;

  g_hash_table_remove (drag_actors, sequence);

  if (g_hash_table_size (drag_actors) == 0)
    g_object_set_data (G_OBJECT (stage),
                       "__clutter_stage_touch_drag_actors",
                       NULL);
}

/*< private >
 * _clutter_stage_get_state:
 * @stage: a #ClutterStage
 *
 * Retrieves the current #ClutterStageState flags associated to the @stage.
 *
 * Return value: a bitwise OR of #ClutterStageState flags
 */
ClutterStageState
_clutter_stage_get_state (ClutterStage *stage)
{
  return stage->priv->current_state;
}

/*< private >
 * _clutter_stage_is_activated:
 * @stage: a #ClutterStage
 *
 * Checks whether the @stage state includes %CLUTTER_STAGE_STATE_ACTIVATED.
 *
 * Return value: %TRUE if the @stage is active
 */
gboolean
_clutter_stage_is_activated (ClutterStage *stage)
{
  return (stage->priv->current_state & CLUTTER_STAGE_STATE_ACTIVATED) != 0;
}

/*< private >
 * _clutter_stage_is_fullscreen:
 * @stage: a #ClutterStage
 *
 * Checks whether the @stage state includes %CLUTTER_STAGE_STATE_FULLSCREEN.
 *
 * Return value: %TRUE if the @stage is fullscreen
 */
gboolean
_clutter_stage_is_fullscreen (ClutterStage *stage)
{
  return (stage->priv->current_state & CLUTTER_STAGE_STATE_FULLSCREEN) != 0;
}

/*< private >
 * _clutter_stage_update_state:
 * @stage: a #ClutterStage
 * @unset_flags: flags to unset
 * @set_flags: flags to set
 *
 * Updates the state of @stage, by unsetting the @unset_flags and setting
 * the @set_flags.
 *
 * If the stage state has been changed, this function will queue a
 * #ClutterEvent of type %CLUTTER_STAGE_STATE.
 *
 * Return value: %TRUE if the state was updated, and %FALSE otherwise
 */
gboolean
_clutter_stage_update_state (ClutterStage      *stage,
                             ClutterStageState  unset_flags,
                             ClutterStageState  set_flags)
{
  ClutterStageState new_state;
  ClutterEvent *event;

  new_state = stage->priv->current_state;
  new_state |= set_flags;
  new_state &= ~unset_flags;

  if (new_state == stage->priv->current_state)
    return FALSE;

  event = clutter_event_new (CLUTTER_STAGE_STATE);
  clutter_event_set_stage (event, stage);

  event->stage_state.new_state = new_state;
  event->stage_state.changed_mask = new_state ^ stage->priv->current_state;

  stage->priv->current_state = new_state;

  _clutter_event_push (event, FALSE);

  return TRUE;
}

/**
 * clutter_stage_set_sync_delay:
 * @stage: a #ClutterStage
 * @sync_delay: number of milliseconds after frame presentation to wait
 *   before painting the next frame. If less than zero, restores the
 *   default behavior where redraw is throttled to the refresh rate but
 *   not synchronized to it.
 *
 * This function enables an alternate behavior where Clutter draws at
 * a fixed point in time after the frame presentation time (also known
 * as the VBlank time). This is most useful when the application
 * wants to show incoming data with predictable latency. (The primary
 * example of this would be a window system compositor.) By synchronizing
 * to provide new data before Clutter redraws, an external source of
 * updates (in the compositor, an application) can get a reliable latency.
 *
 * The appropriate value of @sync_delay depends on the complexity of
 * drawing the stage's scene graph - in general a value of between 0
 * and 8 ms (up to one-half of a typical 60hz frame rate) is appropriate.
 * using a larger value will reduce latency but risks skipping a frame if
 * drawing the stage takes too long.
 *
 * Since: 1.14
 * Stability: unstable
 */
void
clutter_stage_set_sync_delay (ClutterStage *stage,
                              gint          sync_delay)
{
  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  stage->priv->sync_delay = sync_delay;
}

/**
 * clutter_stage_skip_sync_delay:
 * @stage: a #ClutterStage
 *
 * Causes the next frame for the stage to be drawn as quickly as
 * possible, ignoring any delay that clutter_stage_set_sync_delay()
 * would normally cause.
 *
 * Since: 1.14
 * Stability: unstable
 */
void
clutter_stage_skip_sync_delay (ClutterStage *stage)
{
  ClutterStageWindow *stage_window;

  stage_window = _clutter_stage_get_window (stage);
  if (stage_window)
    _clutter_stage_window_schedule_update (stage_window, -1);
}

/**
 * clutter_stage_set_paint_callback:
 * @stage: a #ClutterStage
 * @callback: (allow-none): a callback
 * @data: (allow-none): data to be passed to @callback
 * @notify: (allow-none): function to be called when the callback is removed
 *
 * Sets a callback function to be invoked after the @stage has been
 * painted.
 *
 * Since: 1.14
 */
void
clutter_stage_set_paint_callback (ClutterStage          *stage,
                                  ClutterStagePaintFunc  callback,
                                  gpointer               data,
                                  GDestroyNotify         notify)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  priv = stage->priv;

  if (priv->paint_notify != NULL)
    priv->paint_notify (priv->paint_data);

  priv->paint_callback = callback;
  priv->paint_data = data;
  priv->paint_notify = notify;
}

static void
clutter_stage_invoke_paint_callback (ClutterStage *stage)
{
  if (stage->priv->paint_callback != NULL)
    stage->priv->paint_callback (stage, stage->priv->paint_data);
}

void
_clutter_stage_set_scale_factor (ClutterStage *stage,
                                 int           factor)
{
  ClutterStagePrivate *priv = stage->priv;

  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return;

  if (priv->impl == NULL)
    return;

  _clutter_stage_window_set_scale_factor (priv->impl, factor);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
}
