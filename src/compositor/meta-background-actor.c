/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * meta-background-actor.c: Actor for painting the root window background
 *
 * Copyright 2009 Sander Dijkhuis
 * Copyright 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Portions adapted from gnome-shell/src/shell-global.c
 */

#include <config.h>

#define COGL_ENABLE_EXPERIMENTAL_API
#include <cogl/cogl-texture-pixmap-x11.h>

#define CLUTTER_ENABLE_EXPERIMENTAL_API
#include <clutter/clutter.h>

#include <X11/Xatom.h>

#include "cogl-utils.h"
#include "compositor-private.h"
#include <meta/errors.h>
#include "meta-background-actor-private.h"

/* We allow creating multiple MetaBackgroundActors for the same MetaScreen to
 * allow different rendering options to be set for different copies.
 * But we want to share the same underlying CoglTexture for efficiency and
 * to avoid driver bugs that might occur if we created multiple CoglTexturePixmaps
 * for the same pixmap.
 *
 * This structure holds common information.
 */
typedef struct _MetaScreenBackground MetaScreenBackground;

struct _MetaScreenBackground
{
  MetaScreen *screen;
  GSList *actors;

  float texture_width;
  float texture_height;
  CoglTexture *texture;
  CoglMaterialWrapMode wrap_mode;
  guint have_pixmap : 1;
};

struct _MetaBackgroundActorPrivate
{
  MetaScreenBackground *background;
  CoglPipeline *pipeline;

  cairo_region_t *visible_region;
  float dim_factor;
};

enum
{
  PROP_0,

  PROP_DIM_FACTOR,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (MetaBackgroundActor, meta_background_actor, CLUTTER_TYPE_ACTOR);

static void set_texture                (MetaScreenBackground *background,
                                        CoglHandle            texture);
static void set_texture_to_stage_color (MetaScreenBackground *background);

static void
on_notify_stage_color (GObject              *stage,
                       GParamSpec           *pspec,
                       MetaScreenBackground *background)
{
  if (!background->have_pixmap)
    set_texture_to_stage_color (background);
}

static void
free_screen_background (MetaScreenBackground *background)
{
  set_texture (background, COGL_INVALID_HANDLE);

  if (background->screen != NULL)
    {
      ClutterActor *stage = meta_get_stage_for_screen (background->screen);
      g_signal_handlers_disconnect_by_func (stage,
                                            (gpointer) on_notify_stage_color,
                                            background);
      background->screen = NULL;
    }
}

static MetaScreenBackground *
meta_screen_background_get (MetaScreen *screen)
{
  MetaScreenBackground *background;

  background = g_object_get_data (G_OBJECT (screen), "meta-screen-background");
  if (background == NULL)
    {
      ClutterActor *stage;

      background = g_new0 (MetaScreenBackground, 1);

      background->screen = screen;
      g_object_set_data_full (G_OBJECT (screen), "meta-screen-background",
                              background, (GDestroyNotify) free_screen_background);

      stage = meta_get_stage_for_screen (screen);
      g_signal_connect (stage, "notify::color",
                        G_CALLBACK (on_notify_stage_color), background);

      meta_background_actor_update (screen);
    }

  return background;
}

static void
update_wrap_mode_of_actor (MetaBackgroundActor *self)
{
  MetaBackgroundActorPrivate *priv = self->priv;

  cogl_pipeline_set_layer_wrap_mode (priv->pipeline, 0, priv->background->wrap_mode);
}

static void
update_wrap_mode (MetaScreenBackground *background)
{
  GSList *l;
  int width, height;

  meta_screen_get_size (background->screen, &width, &height);

  /* We turn off repeating when we have a full-screen pixmap to keep from
   * getting artifacts from one side of the image sneaking into the other
   * side of the image via bilinear filtering.
   */
  if (width == background->texture_width && height == background->texture_height)
    background->wrap_mode = COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE;
  else
    background->wrap_mode = COGL_MATERIAL_WRAP_MODE_REPEAT;

  for (l = background->actors; l; l = l->next)
    update_wrap_mode_of_actor (l->data);
}

static void
set_texture_on_actor (MetaBackgroundActor *self)
{
  MetaBackgroundActorPrivate *priv = self->priv;
  MetaDisplay *display = meta_screen_get_display (priv->background->screen);

  /* This may trigger destruction of an old texture pixmap, which, if
   * the underlying X pixmap is already gone has the tendency to trigger
   * X errors inside DRI. For safety, trap errors */
  meta_error_trap_push (display);
  cogl_pipeline_set_layer_texture (priv->pipeline, 0, priv->background->texture);
  meta_error_trap_pop (display);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (self));
}

static void
set_texture (MetaScreenBackground *background,
             CoglHandle            texture)
{
  MetaDisplay *display = meta_screen_get_display (background->screen);
  GSList *l;

  /* This may trigger destruction of an old texture pixmap, which, if
   * the underlying X pixmap is already gone has the tendency to trigger
   * X errors inside DRI. For safety, trap errors */
  meta_error_trap_push (display);
  if (background->texture != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (background->texture);
      background->texture = COGL_INVALID_HANDLE;
    }
  meta_error_trap_pop (display);

  if (texture != COGL_INVALID_HANDLE)
    background->texture = cogl_handle_ref (texture);

  background->texture_width = cogl_texture_get_width (background->texture);
  background->texture_height = cogl_texture_get_height (background->texture);

  for (l = background->actors; l; l = l->next)
    set_texture_on_actor (l->data);

  update_wrap_mode (background);
}

/* Sets our pipeline to paint with a 1x1 texture of the stage's background
 * color; doing this when we have no pixmap allows the application to turn
 * off painting the stage. There might be a performance benefit to
 * painting in this case with a solid color, but the normal solid color
 * case is a 1x1 root pixmap, so we'd have to reverse-engineer that to
 * actually pick up the (small?) performance win. This is just a fallback.
 */
static void
set_texture_to_stage_color (MetaScreenBackground *background)
{
  ClutterActor *stage = meta_get_stage_for_screen (background->screen);
  ClutterColor color;
  CoglHandle texture;

  clutter_stage_get_color (CLUTTER_STAGE (stage), &color);

  /* Slicing will prevent COGL from using hardware texturing for
   * the tiled 1x1 pixmap, and will cause it to draw the window
   * background in millions of separate 1x1 rectangles */
  texture = meta_create_color_texture_4ub (color.red, color.green,
                                           color.blue, 0xff,
                                           COGL_TEXTURE_NO_SLICING);
  set_texture (background, texture);
  cogl_handle_unref (texture);
}

static void
meta_background_actor_dispose (GObject *object)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (object);
  MetaBackgroundActorPrivate *priv = self->priv;

  meta_background_actor_set_visible_region (self, NULL);

  if (priv->background != NULL)
    {
      priv->background->actors = g_slist_remove (priv->background->actors, self);
      priv->background = NULL;
    }

  g_clear_pointer(&priv->pipeline, cogl_object_unref);

  G_OBJECT_CLASS (meta_background_actor_parent_class)->dispose (object);
}

static void
meta_background_actor_get_preferred_width (ClutterActor *actor,
                                           gfloat        for_height,
                                           gfloat       *min_width_p,
                                           gfloat       *natural_width_p)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (actor);
  MetaBackgroundActorPrivate *priv = self->priv;
  int width, height;

  meta_screen_get_size (priv->background->screen, &width, &height);

  if (min_width_p)
    *min_width_p = width;
  if (natural_width_p)
    *natural_width_p = width;
}

static void
meta_background_actor_get_preferred_height (ClutterActor *actor,
                                            gfloat        for_width,
                                            gfloat       *min_height_p,
                                            gfloat       *natural_height_p)

{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (actor);
  MetaBackgroundActorPrivate *priv = self->priv;
  int width, height;

  meta_screen_get_size (priv->background->screen, &width, &height);

  if (min_height_p)
    *min_height_p = height;
  if (natural_height_p)
    *natural_height_p = height;
}

static void
meta_background_actor_paint (ClutterActor *actor)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (actor);
  MetaBackgroundActorPrivate *priv = self->priv;
  guint8 opacity = clutter_actor_get_paint_opacity (actor);
  guint8 color_component;
  int width, height;

  meta_screen_get_size (priv->background->screen, &width, &height);

  color_component = (int)(0.5 + opacity * priv->dim_factor);

  cogl_pipeline_set_color4ub (priv->pipeline,
                              color_component,
                              color_component,
                              color_component,
                              opacity);

  cogl_set_source (priv->pipeline);

  if (priv->visible_region)
    {
      int n_rectangles = cairo_region_num_rectangles (priv->visible_region);
      int i;

      for (i = 0; i < n_rectangles; i++)
        {
          cairo_rectangle_int_t rect;
          cairo_region_get_rectangle (priv->visible_region, i, &rect);

          cogl_rectangle_with_texture_coords (rect.x, rect.y,
                                              rect.x + rect.width, rect.y + rect.height,
                                              rect.x / priv->background->texture_width,
                                              rect.y / priv->background->texture_height,
                                              (rect.x + rect.width) / priv->background->texture_width,
                                              (rect.y + rect.height) / priv->background->texture_height);
        }
    }
  else
    {
      cogl_rectangle_with_texture_coords (0.0f, 0.0f,
                                          width, height,
                                          0.0f, 0.0f,
                                          width / priv->background->texture_width,
                                          height / priv->background->texture_height);
    }
}

static gboolean
meta_background_actor_get_paint_volume (ClutterActor       *actor,
                                        ClutterPaintVolume *volume)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (actor);
  MetaBackgroundActorPrivate *priv = self->priv;
  int width, height;

  meta_screen_get_size (priv->background->screen, &width, &height);

  clutter_paint_volume_set_width (volume, width);
  clutter_paint_volume_set_height (volume, height);

  return TRUE;
}

static void
meta_background_actor_set_dim_factor (MetaBackgroundActor *self,
                                      gfloat               dim_factor)
{
  MetaBackgroundActorPrivate *priv = self->priv;

  if (priv->dim_factor == dim_factor)
    return;

  priv->dim_factor = dim_factor;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (self));

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_DIM_FACTOR]);
}

static void
meta_background_actor_get_property(GObject         *object,
                                   guint            prop_id,
                                   GValue          *value,
                                   GParamSpec      *pspec)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (object);
  MetaBackgroundActorPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_DIM_FACTOR:
      g_value_set_float (value, priv->dim_factor);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_background_actor_set_property(GObject         *object,
                                   guint            prop_id,
                                   const GValue    *value,
                                   GParamSpec      *pspec)
{
  MetaBackgroundActor *self = META_BACKGROUND_ACTOR (object);

  switch (prop_id)
    {
    case PROP_DIM_FACTOR:
      meta_background_actor_set_dim_factor (self, g_value_get_float (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_background_actor_class_init (MetaBackgroundActorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MetaBackgroundActorPrivate));

  object_class->dispose = meta_background_actor_dispose;
  object_class->get_property = meta_background_actor_get_property;
  object_class->set_property = meta_background_actor_set_property;

  actor_class->get_preferred_width = meta_background_actor_get_preferred_width;
  actor_class->get_preferred_height = meta_background_actor_get_preferred_height;
  actor_class->paint = meta_background_actor_paint;
  actor_class->get_paint_volume = meta_background_actor_get_paint_volume;

  /**
   * MetaBackgroundActor:dim-factor:
   *
   * Factor to dim the background by, between 0.0 (black) and 1.0 (original
   * colors)
   */
  pspec = g_param_spec_float ("dim-factor",
                              "Dim factor",
                              "Factor to dim the background by",
                              0.0, 1.0,
                              1.0,
                              G_PARAM_READWRITE);
  obj_props[PROP_DIM_FACTOR] = pspec;
  g_object_class_install_property (object_class, PROP_DIM_FACTOR, pspec);
}

static void
meta_background_actor_init (MetaBackgroundActor *self)
{
  MetaBackgroundActorPrivate *priv;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                   META_TYPE_BACKGROUND_ACTOR,
                                                   MetaBackgroundActorPrivate);
  priv->dim_factor = 1.0;
}

/**
 * meta_background_actor_new:
 * @screen: the #MetaScreen
 *
 * Creates a new actor to draw the background for the given screen.
 *
 * Return value: the newly created background actor
 */
ClutterActor *
meta_background_actor_new_for_screen (MetaScreen *screen)
{
  MetaBackgroundActor *self;
  MetaBackgroundActorPrivate *priv;

  g_return_val_if_fail (META_IS_SCREEN (screen), NULL);

  self = g_object_new (META_TYPE_BACKGROUND_ACTOR, NULL);
  priv = self->priv;

  priv->background = meta_screen_background_get (screen);
  priv->background->actors = g_slist_prepend (priv->background->actors, self);

  /* A CoglMaterial and a CoglPipeline are the same thing */
  priv->pipeline = (CoglPipeline*) meta_create_texture_material (NULL);

  set_texture_on_actor (self);
  update_wrap_mode_of_actor (self);

  return CLUTTER_ACTOR (self);
}

/**
 * meta_background_actor_update:
 * @screen: a #MetaScreen
 *
 * Refetches the _XROOTPMAP_ID property for the root window and updates
 * the contents of the background actor based on that. There's no attempt
 * to optimize out pixmap values that don't change (since a root pixmap
 * could be replaced by with another pixmap with the same ID under some
 * circumstances), so this should only be called when we actually receive
 * a PropertyNotify event for the property.
 */
void
meta_background_actor_update (MetaScreen *screen)
{
  MetaScreenBackground *background;
  MetaDisplay *display;
  MetaCompositor *compositor;
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;
  Pixmap root_pixmap_id;

  background = meta_screen_background_get (screen);
  display = meta_screen_get_display (screen);
  compositor = meta_display_get_compositor (display);

  root_pixmap_id = None;
  if (!XGetWindowProperty (meta_display_get_xdisplay (display),
                           meta_screen_get_xroot (screen),
                           compositor->atom_x_root_pixmap,
                           0, LONG_MAX,
                           False,
                           AnyPropertyType,
                           &type, &format, &nitems, &bytes_after, &data) &&
      type != None)
  {
     /* Got a property. */
     if (type == XA_PIXMAP && format == 32 && nitems == 1)
       {
         /* Was what we expected. */
         root_pixmap_id = *(Pixmap *)data;
       }

     XFree(data);
  }

  if (root_pixmap_id != None)
    {
      CoglHandle texture;
      CoglContext *ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
      GError *error = NULL;

      meta_error_trap_push (display);
      texture = cogl_texture_pixmap_x11_new (ctx, root_pixmap_id, FALSE, &error);
      meta_error_trap_pop (display);

      if (texture != COGL_INVALID_HANDLE)
        {
          set_texture (background, texture);
          cogl_handle_unref (texture);

          background->have_pixmap = True;
          return;
        }
      else
        {
          g_warning ("Failed to create background texture from pixmap: %s",
                     error->message);
          g_error_free (error);
        }
    }

  background->have_pixmap = False;
  set_texture_to_stage_color (background);
}

/**
 * meta_background_actor_set_visible_region:
 * @self: a #MetaBackgroundActor
 * @visible_region: (allow-none): the area of the actor (in allocate-relative
 *   coordinates) that is visible.
 *
 * Sets the area of the background that is unobscured by overlapping windows.
 * This is used to optimize and only paint the visible portions.
 */
void
meta_background_actor_set_visible_region (MetaBackgroundActor *self,
                                          cairo_region_t      *visible_region)
{
  MetaBackgroundActorPrivate *priv;

  g_return_if_fail (META_IS_BACKGROUND_ACTOR (self));

  priv = self->priv;

  if (priv->visible_region)
    {
      cairo_region_destroy (priv->visible_region);
      priv->visible_region = NULL;
    }

  if (visible_region)
    {
      cairo_rectangle_int_t screen_rect = { 0 };
      meta_screen_get_size (priv->background->screen, &screen_rect.width, &screen_rect.height);

      /* Doing the intersection here is probably unnecessary - MetaWindowGroup
       * should never compute a visible area that's larger than the root screen!
       * but it's not that expensive and adds some extra robustness.
       */
      priv->visible_region = cairo_region_create_rectangle (&screen_rect);
      cairo_region_intersect (priv->visible_region, visible_region);
    }
}

/**
 * meta_background_actor_screen_size_changed:
 * @screen: a #MetaScreen
 *
 * Called by the compositor when the size of the #MetaScreen changes
 */
void
meta_background_actor_screen_size_changed (MetaScreen *screen)
{
  MetaScreenBackground *background = meta_screen_background_get (screen);
  GSList *l;

  update_wrap_mode (background);

  for (l = background->actors; l; l = l->next)
    clutter_actor_queue_relayout (l->data);
}

/**
 * meta_background_actor_add_glsl_snippet:
 * @actor: a #MetaBackgroundActor
 * @hook: where to insert the code
 * @declarations: GLSL declarations
 * @code: GLSL code
 * @is_replace: wheter Cogl code should be replaced by the custom shader
 *
 * Adds a GLSL snippet to the pipeline used for drawing the background.
 * See #CoglSnippet for details.
 */
void
meta_background_actor_add_glsl_snippet (MetaBackgroundActor *actor,
                                        MetaSnippetHook      hook,
                                        const char          *declarations,
                                        const char          *code,
                                        gboolean             is_replace)
{
  MetaBackgroundActorPrivate *priv;
  CoglSnippet *snippet;

  g_return_if_fail (META_IS_BACKGROUND_ACTOR (actor));

  priv = actor->priv;

  if (is_replace)
    {
      snippet = cogl_snippet_new (hook, declarations, NULL);
      cogl_snippet_set_replace (snippet, code);
    }
  else
    {
      snippet = cogl_snippet_new (hook, declarations, code);
    }

  if (hook == META_SNIPPET_HOOK_VERTEX ||
      hook == META_SNIPPET_HOOK_FRAGMENT)
    cogl_pipeline_add_snippet (priv->pipeline, snippet);
  else
    cogl_pipeline_add_layer_snippet (priv->pipeline, 0, snippet);

  cogl_object_unref (snippet);
}

/**
 * meta_background_actor_set_uniform_float:
 * @actor: a #MetaBackgroundActor
 * @uniform_name:
 * @n_components: number of components (for vector uniforms)
 * @count: number of uniforms (for array uniforms)
 * @uniform: (array length=uniform_length): the float values to set
 * @uniform_length: the length of @uniform. Must be exactly @n_components x @count,
 *                  and is provided mainly for language bindings.
 *
 * Sets a new GLSL uniform to the provided value. This is mostly
 * useful in congiunction with meta_background_actor_add_glsl_snippet().
 */

void
meta_background_actor_set_uniform_float (MetaBackgroundActor *actor,
                                         const char          *uniform_name,
                                         int                  n_components,
                                         int                  count,
                                         const float         *uniform,
                                         int                  uniform_length)
{
  MetaBackgroundActorPrivate *priv;

  g_return_if_fail (META_IS_BACKGROUND_ACTOR (actor));
  g_return_if_fail (uniform_length == n_components * count);

  priv = actor->priv;

  cogl_pipeline_set_uniform_float (priv->pipeline,
                                   cogl_pipeline_get_uniform_location (priv->pipeline,
                                                                       uniform_name),
                                   n_components, count, uniform);
}

