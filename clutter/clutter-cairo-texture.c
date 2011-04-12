/*
 * Clutter
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By: Emmanuele Bassi <ebassi@linux.intel.com>
 *              Matthew Allum <mallum@o-hand.com>
 *              Chris Lord <chris@o-hand.com>
 *              Iain Holmes <iain@o-hand.com>
 *              Neil Roberts <neil@linux.intel.com>
 *
 * Copyright (C) 2008, 2009, 2010  Intel Corporation.
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
 * SECTION:clutter-cairo-texture
 * @short_description: Texture with Cairo integration
 *
 * #ClutterCairoTexture is a #ClutterTexture that displays the contents
 * of a Cairo context. The #ClutterCairoTexture actor will create a
 * Cairo image surface which will then be uploaded to a GL texture when
 * needed.
 *
 * #ClutterCairoTexture will provide a #cairo_t context by using the
 * clutter_cairo_texture_create() and clutter_cairo_texture_create_region()
 * functions; you can use the Cairo API to draw on the context and then
 * call cairo_destroy() when done.
 *
 * As soon as the context is destroyed with cairo_destroy(), the contents
 * of the surface will be uploaded into the #ClutterCairoTexture actor:
 *
 * |[
 *   cairo_t *cr;
 *
 *   cr = clutter_cairo_texture_create (CLUTTER_CAIRO_TEXTURE (texture));
 *
 *   /&ast; draw on the context &ast;/
 *
 *   cairo_destroy (cr);
 * ]|
 *
 * Although a new #cairo_t is created each time you call
 * clutter_cairo_texture_create() or
 * clutter_cairo_texture_create_region(), it uses the same
 * #cairo_surface_t each time. You can call
 * clutter_cairo_texture_clear() to erase the contents between calls.
 *
 * <warning><para>Note that you should never use the code above inside the
 * #ClutterActor::paint or #ClutterActor::pick virtual functions or
 * signal handlers because it will lead to performance
 * degradation.</para></warning>
 *
 * <note><para>Since #ClutterCairoTexture uses a Cairo image surface
 * internally all the drawing operations will be performed in
 * software and not using hardware acceleration. This can lead to
 * performance degradation if the contents of the texture change
 * frequently.</para></note>
 *
 * #ClutterCairoTexture is available since Clutter 1.0.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <cairo-gobject.h>

#include "clutter-cairo-texture.h"

#include "clutter-actor-private.h"
#include "clutter-debug.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

G_DEFINE_TYPE (ClutterCairoTexture,
               clutter_cairo_texture,
               CLUTTER_TYPE_TEXTURE);

enum
{
  PROP_0,

  PROP_SURFACE_WIDTH,
  PROP_SURFACE_HEIGHT,

  PROP_LAST
};

enum
{
  CREATE_SURFACE,

  LAST_SIGNAL
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

static guint cairo_signals[LAST_SIGNAL] = { 0, };

#ifdef CLUTTER_ENABLE_DEBUG
#define clutter_warn_if_paint_fail(obj)                 G_STMT_START {  \
  if (CLUTTER_ACTOR_IN_PAINT (obj)) {                                   \
    g_warning ("%s should not be called during the paint sequence "     \
               "of a ClutterCairoTexture as it will likely cause "      \
               "performance issues.", G_STRFUNC);                       \
  }                                                     } G_STMT_END
#else
#define clutter_warn_if_paint_fail(obj)         /* void */
#endif /* CLUTTER_ENABLE_DEBUG */

#define CLUTTER_CAIRO_TEXTURE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_CAIRO_TEXTURE, ClutterCairoTexturePrivate))

struct _ClutterCairoTexturePrivate
{
  cairo_surface_t *cr_surface;
  gint surface_width;
  gint surface_height;

  guint width;
  guint height;
};

typedef struct {
  ClutterCairoTexture *cairo;
  cairo_rectangle_int_t rect;
} ClutterCairoTextureContext;

static const cairo_user_data_key_t clutter_cairo_texture_context_key;

static void
clutter_cairo_texture_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ClutterCairoTexturePrivate *priv;

  priv = CLUTTER_CAIRO_TEXTURE (object)->priv;

  switch (prop_id)
    {
    case PROP_SURFACE_WIDTH:
      priv->width = g_value_get_uint (value);
      break;

    case PROP_SURFACE_HEIGHT:
      priv->height = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_cairo_texture_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ClutterCairoTexturePrivate *priv;

  priv = CLUTTER_CAIRO_TEXTURE (object)->priv;

  switch (prop_id)
    {
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
clutter_cairo_texture_finalize (GObject *object)
{
  ClutterCairoTexturePrivate *priv = CLUTTER_CAIRO_TEXTURE (object)->priv;

  if (priv->cr_surface != NULL)
    {
      cairo_surface_t *surface = priv->cr_surface;

      priv->cr_surface = NULL;

      cairo_surface_finish (surface);
      cairo_surface_destroy (surface);
    }

  G_OBJECT_CLASS (clutter_cairo_texture_parent_class)->finalize (object);
}

static cairo_surface_t *
get_surface (ClutterCairoTexture *self)
{
  ClutterCairoTexturePrivate *priv = self->priv;

  if (priv->cr_surface == NULL)
    {
      g_signal_emit (self, cairo_signals[CREATE_SURFACE], 0,
                     priv->width,
                     priv->height,
                     &priv->cr_surface);
    }

  return priv->cr_surface;
}

static inline void
clutter_cairo_texture_surface_resize_internal (ClutterCairoTexture *cairo)
{
  ClutterCairoTexturePrivate *priv = cairo->priv;

  if (priv->cr_surface != NULL)
    {
      cairo_surface_t *surface = priv->cr_surface;

      /* if the surface is an image one, and the size is already the
       * same, then we don't need to do anything
       */
      if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_IMAGE)
        {
          gint surface_width = cairo_image_surface_get_width (surface);
          gint surface_height = cairo_image_surface_get_height (surface);

          if (priv->width == surface_width &&
              priv->height == surface_height)
            return;
        }

      cairo_surface_finish (surface);
      cairo_surface_destroy (surface);
      priv->cr_surface = NULL;
    }

  if (priv->width == 0 || priv->height == 0)
    return;

  g_signal_emit (cairo, cairo_signals[CREATE_SURFACE], 0,
                 priv->width,
                 priv->height,
                 &priv->cr_surface);
}

static void
clutter_cairo_texture_notify (GObject    *object,
                              GParamSpec *pspec)
{
  /* When the surface width or height changes then resize the cairo
     surface. This is done here instead of directly in set_property so
     that if both the width and height properties are set using a
     single call to g_object_set then the surface will only be resized
     once because the notifications will be frozen in between */
  if (strcmp ("surface-width", pspec->name) == 0 ||
      strcmp ("surface-height", pspec->name) == 0)
    {
      ClutterCairoTexture *cairo = CLUTTER_CAIRO_TEXTURE (object);

      clutter_cairo_texture_surface_resize_internal (cairo);
    }

  if (G_OBJECT_CLASS (clutter_cairo_texture_parent_class)->notify)
    G_OBJECT_CLASS (clutter_cairo_texture_parent_class)->notify (object, pspec);
}

static void
clutter_cairo_texture_get_preferred_width (ClutterActor *actor,
                                           gfloat        for_height,
                                           gfloat       *min_width,
                                           gfloat       *natural_width)
{
  ClutterCairoTexturePrivate *priv = CLUTTER_CAIRO_TEXTURE (actor)->priv;

  if (min_width)
    *min_width = 0;

  if (natural_width)
    *natural_width = (gfloat) priv->width;
}

static void
clutter_cairo_texture_get_preferred_height (ClutterActor *actor,
                                            gfloat        for_width,
                                            gfloat       *min_height,
                                            gfloat       *natural_height)
{
  ClutterCairoTexturePrivate *priv = CLUTTER_CAIRO_TEXTURE (actor)->priv;

  if (min_height)
    *min_height = 0;

  if (natural_height)
    *natural_height = (gfloat) priv->height;
}

static gboolean
clutter_cairo_texture_get_paint_volume (ClutterActor       *self,
                                        ClutterPaintVolume *volume)
{
  return _clutter_actor_set_default_paint_volume (self,
                                                  CLUTTER_TYPE_CAIRO_TEXTURE,
                                                  volume);
}

static cairo_surface_t *
clutter_cairo_texture_create_surface (ClutterCairoTexture *self,
                                      guint                width,
                                      guint                height)
{
  cairo_surface_t *surface;
  guint cairo_stride;
  guint8 *cairo_data;
  CoglHandle cogl_texture;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        width,
                                        height);

  cairo_stride = cairo_image_surface_get_stride (surface);
  cairo_data = cairo_image_surface_get_data (surface);

  self->priv->surface_width = width;
  self->priv->surface_height = height;

  /* create a backing Cogl texture */
  cogl_texture = cogl_texture_new_from_data (width, height,
                                             COGL_TEXTURE_NONE,
                                             CLUTTER_CAIRO_FORMAT_ARGB32,
                                             COGL_PIXEL_FORMAT_ANY,
                                             cairo_stride,
                                             cairo_data);
  clutter_texture_set_cogl_texture (CLUTTER_TEXTURE (self), cogl_texture);
  cogl_handle_unref (cogl_texture);

  return surface;
}

static gboolean
create_surface_accum (GSignalInvocationHint *ihint,
                      GValue                *return_accu,
                      const GValue          *handler_return,
                      gpointer               data)
{
  g_value_copy (handler_return, return_accu);

  /* stop on the first non-NULL return value */
  return g_value_get_boxed (handler_return) == NULL;
}

static void
clutter_cairo_texture_class_init (ClutterCairoTextureClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->finalize     = clutter_cairo_texture_finalize;
  gobject_class->set_property = clutter_cairo_texture_set_property;
  gobject_class->get_property = clutter_cairo_texture_get_property;
  gobject_class->notify       = clutter_cairo_texture_notify;

  actor_class->get_paint_volume =
    clutter_cairo_texture_get_paint_volume;
  actor_class->get_preferred_width =
    clutter_cairo_texture_get_preferred_width;
  actor_class->get_preferred_height =
    clutter_cairo_texture_get_preferred_height;

  klass->create_surface = clutter_cairo_texture_create_surface;

  g_type_class_add_private (gobject_class, sizeof (ClutterCairoTexturePrivate));

  /**
   * ClutterCairoTexture:surface-width:
   *
   * The width of the Cairo surface used by the #ClutterCairoTexture
   * actor, in pixels.
   *
   * Since: 1.0
   */
  obj_props[PROP_SURFACE_WIDTH] =
    g_param_spec_uint ("surface-width",
                       P_("Surface Width"),
                       P_("The width of the Cairo surface"),
                       0, G_MAXUINT,
                       0,
                       CLUTTER_PARAM_READWRITE);
  /**
   * ClutterCairoTexture:surface-height:
   *
   * The height of the Cairo surface used by the #ClutterCairoTexture
   * actor, in pixels.
   *
   * Since: 1.0
   */
  obj_props[PROP_SURFACE_HEIGHT] =
    g_param_spec_uint ("surface-height",
                       P_("Surface Height"),
                       P_("The height of the Cairo surface"),
                       0, G_MAXUINT,
                       0,
                       CLUTTER_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class,
                                     PROP_LAST,
                                     obj_props);

  /**
   * ClutterCairoTexture::create-surface:
   * @texture: the #ClutterCairoTexture that emitted the signal
   * @width: the width of the surface to create
   * @height: the height of the surface to create
   *
   * The ::create-surface signal is emitted when a #ClutterCairoTexture
   * news its surface (re)created, which happens either when the Cairo
   * context is created with clutter_cairo_texture_create() or
   * clutter_cairo_texture_create_region(), or when the surface is resized
   * through clutter_cairo_texture_set_surface_size().
   *
   * The first signal handler that returns a non-%NULL, valid surface will
   * stop any further signal emission, and the returned surface will be
   * the one used.
   *
   * Return value: the newly created #cairo_surface_t for the texture
   *
   * Since: 1.6
   */
  cairo_signals[CREATE_SURFACE] =
    g_signal_new (I_("create-surface"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterCairoTextureClass, create_surface),
                  create_surface_accum, NULL,
                  _clutter_marshal_BOXED__UINT_UINT,
                  CAIRO_GOBJECT_TYPE_SURFACE, 2,
                  G_TYPE_UINT,
                  G_TYPE_UINT);
}

static void
clutter_cairo_texture_init (ClutterCairoTexture *self)
{
  ClutterCairoTexturePrivate *priv;

  self->priv = priv = CLUTTER_CAIRO_TEXTURE_GET_PRIVATE (self);

  /* FIXME - we are hardcoding the format; it would be good to have
   * a :surface-format construct-only property for creating
   * textures with a different format and have the cairo surface
   * match that format
   *
   * priv->format = CAIRO_FORMAT_ARGB32;
   */

  /* the Cairo surface is responsible for driving the size of
   * the texture; if we let sync_size to its default of TRUE,
   * the Texture will try to queue a relayout every time we
   * change the size of the Cairo surface - which is not what
   * we want
   */
  clutter_texture_set_sync_size (CLUTTER_TEXTURE (self), FALSE);
}

/**
 * clutter_cairo_texture_new:
 * @width: the width of the surface
 * @height: the height of the surface
 *
 * Creates a new #ClutterCairoTexture actor, with a surface of @width by
 * @height pixels.
 *
 * Return value: the newly created #ClutterCairoTexture actor
 *
 * Since: 1.0
 */
ClutterActor*
clutter_cairo_texture_new (guint width,
                           guint height)
{
  return g_object_new (CLUTTER_TYPE_CAIRO_TEXTURE,
                       "surface-width", width,
                       "surface-height", height,
                       NULL);
}

static void
clutter_cairo_texture_context_destroy (void *data)
{
  ClutterCairoTextureContext *ctxt = data;
  ClutterCairoTexture *cairo = ctxt->cairo;
  ClutterCairoTexturePrivate *priv = cairo->priv;
  guint8 *cairo_data;
  gint cairo_width, cairo_height, cairo_stride;
  gint surface_width, surface_height;
  CoglHandle cogl_texture;

  if (priv->cr_surface == NULL)
    return;

  /* for any other surface type, we presume that there exists a native
   * communication between Cairo and GL that is triggered by cairo_destroy().
   *
   * for instance, cairo-drm will flush the outstanding modifications to the
   * surface upon context destruction and so the texture is automatically
   * updated.
   */
  if (cairo_surface_get_type (priv->cr_surface) != CAIRO_SURFACE_TYPE_IMAGE)
    goto out;

  surface_width  = cairo_image_surface_get_width (priv->cr_surface);
  surface_height = cairo_image_surface_get_height (priv->cr_surface);

  cairo_width  = MIN (ctxt->rect.width, surface_width);
  cairo_height = MIN (ctxt->rect.height, surface_height);

  cogl_texture = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (cairo));
  if (!cairo_width || !cairo_height || cogl_texture == COGL_INVALID_HANDLE)
    {
      g_slice_free (ClutterCairoTextureContext, ctxt);
      return;
    }

  cairo_stride = cairo_image_surface_get_stride (priv->cr_surface);
  cairo_data = cairo_image_surface_get_data (priv->cr_surface);
  cairo_data += cairo_stride * ctxt->rect.y;
  cairo_data += 4 * ctxt->rect.x;

  cogl_texture_set_region (cogl_texture,
                           0, 0,
                           ctxt->rect.x, ctxt->rect.y,
                           cairo_width, cairo_height,
                           cairo_width, cairo_height,
                           CLUTTER_CAIRO_FORMAT_ARGB32,
                           cairo_stride,
                           cairo_data);

out:
  g_slice_free (ClutterCairoTextureContext, ctxt);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (cairo));
}

static void
intersect_rectangles (cairo_rectangle_int_t *a,
		      cairo_rectangle_int_t *b,
		      cairo_rectangle_int_t *inter)
{
  gint dest_x, dest_y;
  gint dest_width, dest_height;

  dest_x = MAX (a->x, b->x);
  dest_y = MAX (a->y, b->y);
  dest_width = MIN (a->x + a->width, b->x + b->width) - dest_x;
  dest_height = MIN (a->y + a->height, b->y + b->height) - dest_y;

  if (dest_width > 0 && dest_height > 0)
    {
      inter->x = dest_x;
      inter->y = dest_y;
      inter->width = dest_width;
      inter->height = dest_height;
    }
  else
    {
      inter->x = 0;
      inter->y = 0;
      inter->width = 0;
      inter->height = 0;
    }
}

/**
 * clutter_cairo_texture_create_region:
 * @self: a #ClutterCairoTexture
 * @x_offset: offset of the region on the X axis
 * @y_offset: offset of the region on the Y axis
 * @width: width of the region, or -1 for the full surface width
 * @height: height of the region, or -1 for the full surface height
 *
 * Creates a new Cairo context that will updat the region defined
 * by @x_offset, @y_offset, @width and @height.
 *
 * <warning><para>Do not call this function within the paint virtual
 * function or from a callback to the #ClutterActor::paint
 * signal.</para></warning>
 *
 * Return value: a newly created Cairo context. Use cairo_destroy()
 *   to upload the contents of the context when done drawing
 *
 * Since: 1.0
 */
cairo_t *
clutter_cairo_texture_create_region (ClutterCairoTexture *self,
                                     gint                 x_offset,
                                     gint                 y_offset,
                                     gint                 width,
                                     gint                 height)
{
  ClutterCairoTexturePrivate *priv;
  ClutterCairoTextureContext *ctxt;
  cairo_rectangle_int_t region, area, inter;
  cairo_surface_t *surface;
  cairo_t *cr;

  g_return_val_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self), NULL);

  clutter_warn_if_paint_fail (self);

  priv = self->priv;

  if (width < 0)
    width = priv->width;

  if (height < 0)
    height = priv->height;

  if (width == 0 || height == 0)
    {
      g_warning ("Unable to create a context for an image surface of "
                 "width %d and height %d. Set the surface size to be "
                 "at least 1 pixel by 1 pixel.",
                 width, height);
      return NULL;
    }

  surface = get_surface (self);

  ctxt = g_slice_new0 (ClutterCairoTextureContext);
  ctxt->cairo = self;

  region.x = x_offset;
  region.y = y_offset;
  region.width = width;
  region.height = height;

  area.x = 0;
  area.y = 0;
  area.width = priv->width;
  area.height = priv->height;

  /* Limit the region to the visible rectangle */
  intersect_rectangles (&area, &region, &inter);

  ctxt->rect = inter;

  cr = cairo_create (surface);
  cairo_set_user_data (cr, &clutter_cairo_texture_context_key,
		       ctxt,
                       clutter_cairo_texture_context_destroy);

  return cr;
}

/**
 * clutter_cairo_texture_create:
 * @self: a #ClutterCairoTexture
 *
 * Creates a new Cairo context for the @cairo texture. It is
 * similar to using clutter_cairo_texture_create_region() with @x_offset
 * and @y_offset of 0, @width equal to the @cairo texture surface width
 * and @height equal to the @cairo texture surface height.
 *
 * <warning><para>Do not call this function within the paint virtual
 * function or from a callback to the #ClutterActor::paint
 * signal.</para></warning>
 *
 * Return value: a newly created Cairo context. Use cairo_destroy()
 *   to upload the contents of the context when done drawing
 *
 * Since: 1.0
 */
cairo_t *
clutter_cairo_texture_create (ClutterCairoTexture *self)
{
  g_return_val_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self), NULL);

  clutter_warn_if_paint_fail (self);

  return clutter_cairo_texture_create_region (self, 0, 0, -1, -1);
}

/**
 * clutter_cairo_set_source_color:
 * @cr: a Cairo context
 * @color: a #ClutterColor
 *
 * Utility function for setting the source color of @cr using
 * a #ClutterColor.
 *
 * Since: 1.0
 */
void
clutter_cairo_set_source_color (cairo_t            *cr,
                                const ClutterColor *color)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (color != NULL);

  if (color->alpha == 0xff)
    cairo_set_source_rgb (cr,
                          color->red / 255.0,
                          color->green / 255.0,
                          color->blue / 255.0);
  else
    cairo_set_source_rgba (cr,
                           color->red / 255.0,
                           color->green / 255.0,
                           color->blue / 255.0,
                           color->alpha / 255.0);
}

/**
 * clutter_cairo_texture_set_surface_size:
 * @self: a #ClutterCairoTexture
 * @width: the new width of the surface
 * @height: the new height of the surface
 *
 * Resizes the Cairo surface used by @self to @width and @height.
 *
 * Since: 1.0
 */
void
clutter_cairo_texture_set_surface_size (ClutterCairoTexture *self,
                                        guint                width,
                                        guint                height)
{
  ClutterCairoTexturePrivate *priv;

  g_return_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self));

  priv = self->priv;

  if (width == priv->width && height == priv->height)
    return;

  g_object_freeze_notify (G_OBJECT (self));

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

  clutter_cairo_texture_surface_resize_internal (self);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * clutter_cairo_texture_get_surface_size:
 * @self: a #ClutterCairoTexture
 * @width: (out): return location for the surface width, or %NULL
 * @height: (out): return location for the surface height, or %NULL
 *
 * Retrieves the surface width and height for @self.
 *
 * Since: 1.0
 */
void
clutter_cairo_texture_get_surface_size (ClutterCairoTexture *self,
                                        guint               *width,
                                        guint               *height)
{
  g_return_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self));

  if (width)
    *width = self->priv->width;

  if (height)
    *height = self->priv->height;
}

/**
 * clutter_cairo_texture_clear:
 * @self: a #ClutterCairoTexture
 *
 * Clears @self's internal drawing surface, so that the next upload
 * will replace the previous contents of the #ClutterCairoTexture
 * rather than adding to it.
 *
 * Since: 1.0
 */
void
clutter_cairo_texture_clear (ClutterCairoTexture *self)
{
  cairo_surface_t *surface;
  cairo_t *cr;

  g_return_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self));

  surface = get_surface (self);

  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_destroy (cr);
}
