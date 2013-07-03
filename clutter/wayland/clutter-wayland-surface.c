/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 */

/**
 * SECTION:clutter-wayland-surface
 * @Title: ClutterWaylandSurface
 * @short_description: An actor which displays the content of a client surface
 *
 * #ClutterWaylandSurface is an actor for displaying the contents of a client
 * surface. It is intended to support developers implementing Clutter based
 * wayland compositors.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include "clutter-wayland-surface.h"

#include "clutter-actor-private.h"
#include "clutter-marshal.h"
#include "clutter-paint-volume-private.h"
#include "clutter-private.h"
#include "clutter-backend.h"

#include <cogl/cogl.h>
#include <cogl/cogl-wayland-server.h>

enum
{
  PROP_SURFACE = 1,
  PROP_SURFACE_WIDTH,
  PROP_SURFACE_HEIGHT,
  PROP_COGL_TEXTURE,
  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

enum
{
  QUEUE_DAMAGE_REDRAW,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

struct _ClutterWaylandSurfacePrivate
{
  struct wl_surface *surface;
  CoglTexture2D *buffer;
  int width, height;
  CoglPipeline *pipeline;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClutterWaylandSurface,
                            clutter_wayland_surface,
                            CLUTTER_TYPE_ACTOR)

static gboolean
clutter_wayland_surface_get_paint_volume (ClutterActor *self,
                                          ClutterPaintVolume *volume)
{
  return clutter_paint_volume_set_from_allocation (volume, self);
}

static void
clutter_wayland_surface_queue_damage_redraw (ClutterWaylandSurface *texture,
                                             gint x,
                                             gint y,
                                             gint width,
                                             gint height)
{
  ClutterWaylandSurfacePrivate *priv = texture->priv;
  ClutterActor *self = CLUTTER_ACTOR (texture);
  ClutterActorBox allocation;
  float scale_x;
  float scale_y;
  cairo_rectangle_int_t clip;

  /* NB: clutter_actor_queue_redraw_with_clip expects a box in the actor's
   * coordinate space so we need to convert from surface coordinates to
   * actor coordinates...
   */

  /* Calling clutter_actor_get_allocation_box() is enormously expensive
   * if the actor has an out-of-date allocation, since it triggers
   * a full redraw. clutter_actor_queue_redraw_with_clip() would redraw
   * the whole stage anyways in that case, so just go ahead and do
   * it here.
   */
  if (!clutter_actor_has_allocation (self))
    {
      clutter_actor_queue_redraw (self);
      return;
    }

  if (priv->width == 0 || priv->height == 0)
    return;

  clutter_actor_get_allocation_box (self, &allocation);

  scale_x = (allocation.x2 - allocation.x1) / priv->width;
  scale_y = (allocation.y2 - allocation.y1) / priv->height;

  clip.x = x * scale_x;
  clip.y = y * scale_y;
  clip.width = width * scale_x;
  clip.height = height * scale_y;
  clutter_actor_queue_redraw_with_clip (self, &clip);
}

static void
free_pipeline (ClutterWaylandSurface *self)
{
  ClutterWaylandSurfacePrivate *priv = self->priv;

  if (priv->pipeline)
    {
      cogl_object_unref (priv->pipeline);
      priv->pipeline = NULL;
    }
}

static void
opacity_change_cb (ClutterWaylandSurface *self)
{
  free_pipeline (self);
}

static void
clutter_wayland_surface_init (ClutterWaylandSurface *self)
{
  ClutterWaylandSurfacePrivate *priv;
  
  priv = clutter_wayland_surface_get_instance_private (self);
  priv->surface = NULL;
  priv->width = 0;
  priv->height = 0;

  self->priv = priv;

  g_signal_connect (self, "notify::opacity", G_CALLBACK (opacity_change_cb), NULL);
}

static void
free_surface_buffers (ClutterWaylandSurface *self)
{
  ClutterWaylandSurfacePrivate *priv = self->priv;

  if (priv->buffer)
    {
      cogl_object_unref (priv->buffer);
      priv->buffer = NULL;
      free_pipeline (self);
    }
}

static void
clutter_wayland_surface_dispose (GObject *object)
{
  ClutterWaylandSurface *self = CLUTTER_WAYLAND_SURFACE (object);
  ClutterWaylandSurfacePrivate *priv = self->priv;

  free_pipeline (self);
  free_surface_buffers (self);
  priv->surface = NULL;

  G_OBJECT_CLASS (clutter_wayland_surface_parent_class)->dispose (object);
}

static void
set_size (ClutterWaylandSurface *self,
          int width,
          int height)
{
  ClutterWaylandSurfacePrivate *priv = self->priv;

  if (priv->width != width)
    {
      priv->width = width;
      g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_SURFACE_WIDTH]);
    }
  if (priv->height != height)
    {
      priv->height = height;
      g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_SURFACE_HEIGHT]);
    }

  clutter_actor_set_size (CLUTTER_ACTOR (self), priv->width, priv->height);
}

/**
 * clutter_wayland_surface_get_surface:
 * @self: a #ClutterWaylandSurface
 *
 * Retrieves a point to the Wayland surface used by the actor.
 *
 * Return value: (transfer none): a wl_surface pointer, or %NULL
 *
 * Since: 1.10
 */
struct wl_surface *
clutter_wayland_surface_get_surface (ClutterWaylandSurface *self)
{
  ClutterWaylandSurfacePrivate *priv = self->priv;
  return priv->surface;
}

/**
 * clutter_wayland_surface_set_surface:
 * @self: a #ClutterWaylandSurface
 * @surface: a Wayland wl_surface pointer
 *
 * Sets the Wayland surface to be used by the actor.
 *
 * Since: 1.10
 */
void
clutter_wayland_surface_set_surface (ClutterWaylandSurface *self,
                                     struct wl_surface *surface)
{
  ClutterWaylandSurfacePrivate *priv;

  g_return_if_fail (CLUTTER_WAYLAND_IS_SURFACE (self));

  priv = self->priv;

  if (priv->surface == surface)
    return;

  if (priv->surface)
    {
      free_pipeline (self);
      free_surface_buffers (self);
      g_signal_emit (self, signals[QUEUE_DAMAGE_REDRAW],
                     0,
                     0, 0, priv->width, priv->height);
    }

  priv->surface = surface;

  /* XXX: should we freeze/thaw notifications? */

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_SURFACE]);

  /* We have to wait until the next attach event to find out the surface
   * geometry... */
  set_size (self, 0, 0);
}

static void
clutter_wayland_surface_set_property (GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
  ClutterWaylandSurface *self = CLUTTER_WAYLAND_SURFACE (object);

  switch (prop_id)
    {
    case PROP_SURFACE:
      clutter_wayland_surface_set_surface (self, g_value_get_pointer (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_wayland_surface_get_property (GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
  ClutterWaylandSurface *self = CLUTTER_WAYLAND_SURFACE (object);
  ClutterWaylandSurfacePrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_SURFACE:
      g_value_set_pointer (value, priv->surface);
      break;
    case PROP_SURFACE_WIDTH:
      g_value_set_uint (value, priv->width);
      break;
    case PROP_SURFACE_HEIGHT:
      g_value_set_uint (value, priv->height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_wayland_surface_paint (ClutterActor *self)
{
  ClutterWaylandSurfacePrivate *priv;
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_WAYLAND_IS_SURFACE (self));

  priv = CLUTTER_WAYLAND_SURFACE (self)->priv;

  if (G_UNLIKELY (priv->pipeline == NULL))
    {
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());
      guint8 paint_opacity = clutter_actor_get_paint_opacity (self);

      priv->pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_color4ub (priv->pipeline,
                                  paint_opacity,
                                  paint_opacity,
                                  paint_opacity,
                                  paint_opacity);
      cogl_pipeline_set_layer_texture (priv->pipeline, 0,
                                       COGL_TEXTURE (priv->buffer));
    }

  cogl_set_source (priv->pipeline);
  clutter_actor_get_allocation_box (self, &box);
  cogl_rectangle (0, 0, box.x2 - box.x1, box.y2 - box.y1);
}

static void
clutter_wayland_surface_get_preferred_width (ClutterActor *self,
                                             gfloat for_height,
                                             gfloat *min_width_p,
                                             gfloat *natural_width_p)
{
  ClutterWaylandSurfacePrivate *priv;

  g_return_if_fail (CLUTTER_WAYLAND_IS_SURFACE (self));

  priv = CLUTTER_WAYLAND_SURFACE (self)->priv;

  if (min_width_p)
    *min_width_p = 0;

  if (natural_width_p)
    *natural_width_p = priv->width;
}

static void
clutter_wayland_surface_get_preferred_height (ClutterActor *self,
                                              gfloat for_width,
                                              gfloat *min_height_p,
                                              gfloat *natural_height_p)
{
  ClutterWaylandSurfacePrivate *priv;

  g_return_if_fail (CLUTTER_WAYLAND_IS_SURFACE (self));

  priv = CLUTTER_WAYLAND_SURFACE (self)->priv;

  if (min_height_p)
    *min_height_p = 0;

  if (natural_height_p)
    *natural_height_p = priv->height;
}

static gboolean
clutter_wayland_surface_has_overlaps (ClutterActor *self)
{
  /* Rectangles never need an offscreen redirect because there are
     never any overlapping primitives */
  return FALSE;
}

static void
clutter_wayland_surface_class_init (ClutterWaylandSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  actor_class->get_paint_volume = clutter_wayland_surface_get_paint_volume;
  actor_class->paint = clutter_wayland_surface_paint;
  actor_class->get_preferred_width =
    clutter_wayland_surface_get_preferred_width;
  actor_class->get_preferred_height =
    clutter_wayland_surface_get_preferred_height;
  actor_class->has_overlaps = clutter_wayland_surface_has_overlaps;

  object_class->dispose      = clutter_wayland_surface_dispose;
  object_class->set_property = clutter_wayland_surface_set_property;
  object_class->get_property = clutter_wayland_surface_get_property;

  pspec = g_param_spec_pointer ("surface",
			        P_("Surface"),
			        P_("The underlying wayland surface"),
                                CLUTTER_PARAM_READWRITE|
                                G_PARAM_CONSTRUCT_ONLY);
  obj_props[PROP_SURFACE] = pspec;
  g_object_class_install_property (object_class, PROP_SURFACE, pspec);

  pspec = g_param_spec_uint ("surface-width",
                             P_("Surface width"),
                             P_("The width of the underlying wayland surface"),
                             0, G_MAXUINT,
                             0,
                             G_PARAM_READABLE);
  obj_props[PROP_SURFACE_WIDTH] = pspec;
  g_object_class_install_property (object_class, PROP_SURFACE_WIDTH, pspec);

  pspec = g_param_spec_uint ("surface-height",
                             P_("Surface height"),
                             P_("The height of the underlying wayland surface"),
                             0, G_MAXUINT,
                             0,
                             G_PARAM_READABLE);
  obj_props[PROP_SURFACE_HEIGHT] = pspec;
  g_object_class_install_property (object_class, PROP_SURFACE_HEIGHT, pspec);

  pspec = g_param_spec_boxed ("cogl-texture",
                              P_("Cogl Texture"),
                              P_("The underlying Cogl texture handle used to draw this actor"),
                              COGL_TYPE_HANDLE,
                              CLUTTER_PARAM_READWRITE);
  obj_props[PROP_COGL_TEXTURE] = pspec;
  g_object_class_install_property (object_class, PROP_COGL_TEXTURE, pspec);

  /**
   * ClutterWaylandSurface::queue-damage-redraw:
   * @texture: the object which received the signal
   * @x: The top left x position of the damage region
   * @y: The top left y position of the damage region
   * @width: The width of the damage region
   * @height: The height of the damage region
   *
   * ::queue-damage-redraw is emitted to notify that some sub-region
   * of the texture has been changed. This usually means a redraw
   * needs to be queued for the actor.
   *
   * The default handler will queue a clipped redraw in response to
   * the damage, using the assumption that the pixmap is being painted
   * to a rectangle covering the transformed allocation of the actor.
   * If you sub-class and change the paint method so this isn't true
   * then you must also provide your own damage signal handler to
   * queue a redraw that blocks this default behaviour.
   *
   * Since: 1.10
   */
  signals[QUEUE_DAMAGE_REDRAW] =
    g_signal_new (g_intern_static_string ("queue-damage-redraw"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ClutterWaylandSurfaceClass, queue_damage_redraw),
                  NULL, NULL,
                  _clutter_marshal_VOID__INT_INT_INT_INT,
                  G_TYPE_NONE, 4,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  G_TYPE_INT);
  klass->queue_damage_redraw = clutter_wayland_surface_queue_damage_redraw;
}

/**
 * clutter_wayland_surface_new:
 * @surface: the Wayland surface this actor should represent
 *
 * Creates a new #ClutterWaylandSurface for @surface
 *
 * Return value: A new #ClutterWaylandSurface representing @surface
 *
 * Since: 1.8
 * Stability: unstable
 */
ClutterActor *
clutter_wayland_surface_new (struct wl_surface *surface)
{
  ClutterActor *actor;

  actor = g_object_new (CLUTTER_WAYLAND_TYPE_SURFACE,
                        "surface", surface,
                        NULL);

  return actor;
}

/**
 * clutter_wayland_surface_attach_buffer:
 * @self: A #ClutterWaylandSurface actor
 * @buffer: A compositor side struct wl_buffer pointer
 * @error: A #GError
 *
 * This associates a client's buffer with the #ClutterWaylandSurface
 * actor @self. This will automatically result in @self being re-drawn
 * with the new buffer contents.
 *
 * Since: 1.8
 * Stability: unstable
 */
gboolean
clutter_wayland_surface_attach_buffer (ClutterWaylandSurface *self,
                                       struct wl_buffer *buffer,
                                       GError **error)
{
  ClutterWaylandSurfacePrivate *priv;
  ClutterBackend *backend = clutter_get_default_backend ();
  CoglContext *context = clutter_backend_get_cogl_context (backend);

  g_return_val_if_fail (CLUTTER_WAYLAND_IS_SURFACE (self), TRUE);

  priv = self->priv;

  free_surface_buffers (self);

  set_size (self, buffer->width, buffer->height);

  priv->buffer =
    cogl_wayland_texture_2d_new_from_buffer (context, buffer, error);

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_COGL_TEXTURE]);

  /* NB: We don't queue a redraw of the actor here because we don't
   * know how much of the buffer has changed with respect to the
   * previous buffer. We only ever queue a redraw in response to
   * surface damage. */

  if (!priv->buffer)
    return FALSE;

  return TRUE;
}

/**
 * clutter_wayland_surface_damage_buffer:
 * @self: A #ClutterWaylandSurface actor
 * @buffer: A compositor side struct wl_buffer pointer
 * @x: The x coordinate of the damaged rectangle
 * @y: The y coordinate of the damaged rectangle
 * @width: The width of the damaged rectangle
 * @height: The height of the damaged rectangle
 *
 * This marks a region of the given @buffer has having been changed by
 * the client. This will automatically result in the corresponding damaged
 * region of the actor @self being redrawn.
 *
 * If multiple regions are changed then this should be called multiple
 * times with different damage rectangles.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
clutter_wayland_surface_damage_buffer (ClutterWaylandSurface *self,
                                       struct wl_buffer *buffer,
                                       gint32 x,
                                       gint32 y,
                                       gint32 width,
                                       gint32 height)
{
  ClutterWaylandSurfacePrivate *priv;

  g_return_if_fail (CLUTTER_WAYLAND_IS_SURFACE (self));

  priv = self->priv;

  if (priv->buffer && wl_buffer_is_shm (buffer))
    {
      CoglPixelFormat format;

      switch (wl_shm_buffer_get_format (buffer))
        {
#if G_BYTE_ORDER == G_BIG_ENDIAN
          case WL_SHM_FORMAT_ARGB8888:
            format = COGL_PIXEL_FORMAT_ARGB_8888_PRE;
            break;
          case WL_SHM_FORMAT_XRGB8888:
            format = COGL_PIXEL_FORMAT_ARGB_8888;
            break;
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
          case WL_SHM_FORMAT_ARGB8888:
            format = COGL_PIXEL_FORMAT_BGRA_8888_PRE;
            break;
          case WL_SHM_FORMAT_XRGB8888:
            format = COGL_PIXEL_FORMAT_BGRA_8888;
            break;
#endif
          default:
            g_warn_if_reached ();
            format = COGL_PIXEL_FORMAT_ARGB_8888;
        }

      cogl_texture_set_region (COGL_TEXTURE (priv->buffer),
                               x, y,
                               x, y,
                               width, height,
                               width, height,
                               format,
                               wl_shm_buffer_get_stride (buffer),
                               wl_shm_buffer_get_data (buffer));
    }

  g_signal_emit (self, signals[QUEUE_DAMAGE_REDRAW],
                 0,
                 x, y, width, height);
}

/**
 * clutter_wayland_surface_get_cogl_texture:
 * @self: a #ClutterWaylandSurface
 *
 * Retrieves the Cogl texture with the contents of the Wayland surface.
 *
 * Return value: (transfer none): a Cogl texture, or %NULL
 *
 * Since: 1.10
 */
CoglTexture *
clutter_wayland_surface_get_cogl_texture (ClutterWaylandSurface *self)
{
  g_return_val_if_fail (CLUTTER_WAYLAND_IS_SURFACE (self), NULL);

  return COGL_TEXTURE (self->priv->buffer);
}
