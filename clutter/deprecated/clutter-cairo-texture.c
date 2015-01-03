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
 * Copyright (C) 2008, 2009, 2010, 2011  Intel Corporation.
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
 * Since #ClutterCairoTexture uses a Cairo image surface
 * internally all the drawing operations will be performed in
 * software and not using hardware acceleration. This can lead to
 * performance degradation if the contents of the texture change
 * frequently.
 *
 * In order to use a #ClutterCairoTexture you should connect to the
 * #ClutterCairoTexture::draw signal; the signal is emitted each time
 * the #ClutterCairoTexture has been told to invalidate its contents,
 * by using clutter_cairo_texture_invalidate_rectangle() or its
 * sister function, clutter_cairo_texture_invalidate().
 *
 * Each callback to the #ClutterCairoTexture::draw signal will receive
 * a #cairo_t context which can be used for drawing; the Cairo context
 * is owned by the #ClutterCairoTexture and should not be destroyed
 * explicitly.
 *
 * #ClutterCairoTexture is available since Clutter 1.0.
 *
 * #ClutterCairoTexture is deprecated since Clutter 1.12. You should
 * use #ClutterCanvas instead.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include <cairo-gobject.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "deprecated/clutter-texture.h"
#include "deprecated/clutter-cairo-texture.h"

#include "clutter-cairo-texture.h"

#include "clutter-actor-private.h"
#include "clutter-cairo.h"
#include "clutter-color.h"
#include "clutter-debug.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

struct _ClutterCairoTexturePrivate
{
  cairo_surface_t *cr_surface;

  guint surface_width;
  guint surface_height;

  cairo_t *cr_context;

  guint auto_resize : 1;
};

enum
{
  PROP_0,

  PROP_SURFACE_WIDTH,
  PROP_SURFACE_HEIGHT,
  PROP_AUTO_RESIZE,

  PROP_LAST
};

enum
{
  CREATE_SURFACE,
  DRAW,

  LAST_SIGNAL
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

static guint cairo_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (ClutterCairoTexture,
                            clutter_cairo_texture,
                            CLUTTER_TYPE_TEXTURE)

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

typedef struct {
  ClutterCairoTexture *texture;

  cairo_rectangle_int_t rect;

  guint is_clipped : 1;
} DrawContext;

static const cairo_user_data_key_t clutter_cairo_texture_context_key;

static DrawContext *
draw_context_create (ClutterCairoTexture *texture)
{
  DrawContext *context = g_slice_new0 (DrawContext);

  context->texture = g_object_ref (texture);

  return context;
}

static void
draw_context_destroy (gpointer data)
{
  if (G_LIKELY (data != NULL))
    {
      DrawContext *context = data;

      g_object_unref (context->texture);

      g_slice_free (DrawContext, data);
    }
}

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
      /* we perform the resize on notify to coalesce separate
       * surface-width/surface-height property set
       */
      priv->surface_width = g_value_get_uint (value);
      break;

    case PROP_SURFACE_HEIGHT:
      priv->surface_height = g_value_get_uint (value);
      break;

    case PROP_AUTO_RESIZE:
      clutter_cairo_texture_set_auto_resize (CLUTTER_CAIRO_TEXTURE (object),
                                             g_value_get_boolean (value));
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
      g_value_set_uint (value, priv->surface_width);
      break;

    case PROP_SURFACE_HEIGHT:
      g_value_set_uint (value, priv->surface_height);
      break;

    case PROP_AUTO_RESIZE:
      g_value_set_boolean (value, priv->auto_resize);
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
                     priv->surface_width,
                     priv->surface_height,
                     &priv->cr_surface);
    }

  return priv->cr_surface;
}

static void
clutter_cairo_texture_context_destroy (void *data)
{
  DrawContext *ctxt = data;
  ClutterCairoTexture *cairo = ctxt->texture;
  ClutterCairoTexturePrivate *priv = cairo->priv;
  guint8 *cairo_data;
  gint cairo_width, cairo_height, cairo_stride;
  gint surface_width, surface_height;
  CoglHandle cogl_texture;

  if (priv->cr_surface == NULL)
    {
      /* the surface went away before we could use it */
      draw_context_destroy (ctxt);
      return;
    }

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
  if (cairo_width == 0 ||
      cairo_height == 0 ||
      cogl_texture == COGL_INVALID_HANDLE)
    {
      draw_context_destroy (ctxt);
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
  draw_context_destroy (ctxt);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (cairo));
}

static inline void
clutter_cairo_texture_emit_draw (ClutterCairoTexture        *self,
                                 DrawContext *ctxt)
{
  gboolean result;
  cairo_t *cr;

  /* 0x0 surfaces don't need a ::draw */
  if (self->priv->surface_width == 0 ||
      self->priv->surface_height == 0)
    return;

  /* if the size is !0 then we must have a surface */
  g_assert (self->priv->cr_surface != NULL);

  cr = cairo_create (self->priv->cr_surface);

  if (ctxt->is_clipped)
    {
      cairo_rectangle (cr,
                       ctxt->rect.x,
                       ctxt->rect.y,
                       ctxt->rect.width,
                       ctxt->rect.height);
      cairo_clip (cr);
    }

  /* store the cairo_t as a guard */
  self->priv->cr_context = cr;

  g_signal_emit (self, cairo_signals[DRAW], 0, cr, &result);

  self->priv->cr_context = NULL;

  clutter_cairo_texture_context_destroy (ctxt);

  cairo_destroy (cr);
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

          if (priv->surface_width == surface_width &&
              priv->surface_height == surface_height)
            return;
        }

      cairo_surface_finish (surface);
      cairo_surface_destroy (surface);
      priv->cr_surface = NULL;
    }

  if (priv->surface_width == 0 ||
      priv->surface_height == 0)
    return;

  g_signal_emit (cairo, cairo_signals[CREATE_SURFACE], 0,
                 priv->surface_width,
                 priv->surface_height,
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

  if (obj_props[PROP_SURFACE_WIDTH]->name == pspec->name ||
      obj_props[PROP_SURFACE_HEIGHT]->name == pspec->name)
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
    *natural_width = (gfloat) priv->surface_width;
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
    *natural_height = (gfloat) priv->surface_height;
}

static void
clutter_cairo_texture_allocate (ClutterActor           *self,
                                const ClutterActorBox  *allocation,
                                ClutterAllocationFlags  flags)
{
  ClutterCairoTexturePrivate *priv = CLUTTER_CAIRO_TEXTURE (self)->priv;
  ClutterActorClass *parent_class;

  parent_class = CLUTTER_ACTOR_CLASS (clutter_cairo_texture_parent_class);
  parent_class->allocate (self, allocation, flags);

  if (priv->auto_resize)
    {
      ClutterCairoTexture *texture = CLUTTER_CAIRO_TEXTURE (self);
      gfloat width, height;

      clutter_actor_box_get_size (allocation, &width, &height);

      priv->surface_width = ceilf (width);
      priv->surface_height = ceilf (height);

      clutter_cairo_texture_surface_resize_internal (texture);
      clutter_cairo_texture_invalidate (texture);
    }
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
clutter_cairo_texture_draw_marshaller (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data)
{
  cairo_t *cr = g_value_get_boxed (&param_values[1]);

  cairo_save (cr);

  _clutter_marshal_BOOLEAN__BOXED (closure,
                                   return_value,
                                   n_param_values,
                                   param_values,
                                   invocation_hint,
                                   marshal_data);

  cairo_restore (cr);
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
  actor_class->allocate =
    clutter_cairo_texture_allocate;

  klass->create_surface = clutter_cairo_texture_create_surface;

  /**
   * ClutterCairoTexture:surface-width:
   *
   * The width of the Cairo surface used by the #ClutterCairoTexture
   * actor, in pixels.
   *
   * Since: 1.0
   *
   * Deprecated: 1.12
   */
  obj_props[PROP_SURFACE_WIDTH] =
    g_param_spec_uint ("surface-width",
                       P_("Surface Width"),
                       P_("The width of the Cairo surface"),
                       0, G_MAXUINT,
                       0,
                       CLUTTER_PARAM_READWRITE |
                       G_PARAM_DEPRECATED);
  /**
   * ClutterCairoTexture:surface-height:
   *
   * The height of the Cairo surface used by the #ClutterCairoTexture
   * actor, in pixels.
   *
   * Since: 1.0
   *
   * Deprecated: 1.12
   */
  obj_props[PROP_SURFACE_HEIGHT] =
    g_param_spec_uint ("surface-height",
                       P_("Surface Height"),
                       P_("The height of the Cairo surface"),
                       0, G_MAXUINT,
                       0,
                       CLUTTER_PARAM_READWRITE |
                       G_PARAM_DEPRECATED);

  /**
   * ClutterCairoTexture:auto-resize:
   *
   * Controls whether the #ClutterCairoTexture should automatically
   * resize the Cairo surface whenever the actor's allocation changes.
   * If :auto-resize is set to %TRUE the surface contents will also
   * be invalidated automatically.
   *
   * Since: 1.8
   *
   * Deprecated: 1.12
   */
  obj_props[PROP_AUTO_RESIZE] =
    g_param_spec_boolean ("auto-resize",
                          P_("Auto Resize"),
                          P_("Whether the surface should match the allocation"),
                          FALSE,
                          CLUTTER_PARAM_READWRITE |
                          G_PARAM_DEPRECATED);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);

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
   *
   * Deprecated: 1.12
   */
  cairo_signals[CREATE_SURFACE] =
    g_signal_new (I_("create-surface"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                  G_STRUCT_OFFSET (ClutterCairoTextureClass, create_surface),
                  create_surface_accum, NULL,
                  _clutter_marshal_BOXED__UINT_UINT,
                  CAIRO_GOBJECT_TYPE_SURFACE, 2,
                  G_TYPE_UINT,
                  G_TYPE_UINT);

  /**
   * ClutterCairoTexture::draw:
   * @texture: the #ClutterCairoTexture that emitted the signal
   * @cr: the Cairo context to use to draw
   *
   * The ::draw signal is emitted each time a #ClutterCairoTexture has
   * been invalidated.
   *
   * The passed Cairo context passed will be clipped to the invalidated
   * area.
   *
   * It is safe to connect multiple callbacks to this signals; the state
   * of the Cairo context passed to each callback is automatically saved
   * and restored, so it's not necessary to call cairo_save() and
   * cairo_restore().
   *
   * Return value: %TRUE if the signal emission should stop, and %FALSE
   *   to continue
   *
   * Since: 1.8
   *
   * Deprecated: 1.12
   */
  cairo_signals[DRAW] =
    g_signal_new (I_("draw"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                  G_STRUCT_OFFSET (ClutterCairoTextureClass, draw),
                  _clutter_boolean_handled_accumulator, NULL,
                  clutter_cairo_texture_draw_marshaller,
                  G_TYPE_BOOLEAN, 1,
                  CAIRO_GOBJECT_TYPE_CONTEXT);
}

static void
clutter_cairo_texture_init (ClutterCairoTexture *self)
{
  self->priv = clutter_cairo_texture_get_instance_private (self);

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
 *
 * Deprecated: 1.12: Use #ClutterCanvas instead
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

static cairo_t *
clutter_cairo_texture_create_region_internal (ClutterCairoTexture *self,
                                              gint                 x_offset,
                                              gint                 y_offset,
                                              gint                 width,
                                              gint                 height)
{
  ClutterCairoTexturePrivate *priv = self->priv;
  cairo_rectangle_int_t region, area, inter;
  cairo_surface_t *surface;
  DrawContext *ctxt;
  cairo_t *cr;

  if (width < 0)
    width = priv->surface_width;

  if (height < 0)
    height = priv->surface_height;

  if (width == 0 || height == 0)
    {
      g_warning ("Unable to create a context for an image surface of "
                 "width %d and height %d. Set the surface size to be "
                 "at least 1 pixel by 1 pixel.",
                 width, height);
      return NULL;
    }

  surface = get_surface (self);

  ctxt = draw_context_create (self);

  region.x = x_offset;
  region.y = y_offset;
  region.width = width;
  region.height = height;

  area.x = 0;
  area.y = 0;
  area.width = priv->surface_width;
  area.height = priv->surface_height;

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
 * Do not call this function within the paint virtual
 * function or from a callback to the #ClutterActor::paint
 * signal.
 *
 * Return value: a newly created Cairo context. Use cairo_destroy()
 *   to upload the contents of the context when done drawing
 *
 * Since: 1.0
 *
 * Deprecated: 1.8: Use the #ClutterCairoTexture::draw signal and
 *   clutter_cairo_texture_invalidate_rectangle() to obtain a
 *   clipped Cairo context for 2D drawing.
 */
cairo_t *
clutter_cairo_texture_create_region (ClutterCairoTexture *self,
                                     gint                 x_offset,
                                     gint                 y_offset,
                                     gint                 width,
                                     gint                 height)
{
  g_return_val_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self), NULL);

  clutter_warn_if_paint_fail (self);

  return clutter_cairo_texture_create_region_internal (self,
                                                       x_offset, y_offset,
                                                       width, height);
}

/**
 * clutter_cairo_texture_invalidate_rectangle:
 * @self: a #ClutterCairoTexture
 * @rect: (allow-none): a rectangle with the area to invalida,
 *   or %NULL to perform an unbounded invalidation
 *
 * Invalidates a rectangular region of a #ClutterCairoTexture.
 *
 * The invalidation will cause the #ClutterCairoTexture::draw signal
 * to be emitted.
 *
 * See also: clutter_cairo_texture_invalidate()
 *
 * Since: 1.8
 * Deprecated: 1.12: Use #ClutterCanvas instead
 */
void
clutter_cairo_texture_invalidate_rectangle (ClutterCairoTexture   *self,
                                            cairo_rectangle_int_t *rect)
{
  DrawContext *ctxt = NULL;

  g_return_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self));

  if (self->priv->cr_context != NULL)
    {
      g_warning ("It is not possible to invalidate a Cairo texture"
                 "while drawing into it.");
      return;
    }

  ctxt = draw_context_create (self);

  if (rect != NULL)
    {
      cairo_rectangle_int_t area, inter;

      area.x = 0;
      area.y = 0;
      area.width = self->priv->surface_width;
      area.height = self->priv->surface_height;

      /* Limit the region to the visible rectangle */
      intersect_rectangles (&area, rect, &inter);

      ctxt->is_clipped = TRUE;
      ctxt->rect = inter;
    }
  else
    {
      ctxt->is_clipped = FALSE;
      ctxt->rect.x = ctxt->rect.y = 0;
      ctxt->rect.width = self->priv->surface_width;
      ctxt->rect.height = self->priv->surface_height;
    }

  /* XXX - it might be good to move the emission inside the paint cycle
   * using a repaint function, to avoid blocking inside this function
   */
  clutter_cairo_texture_emit_draw (self, ctxt);
}

/**
 * clutter_cairo_texture_invalidate:
 * @self: a #ClutterCairoTexture
 *
 * Invalidates the whole surface of a #ClutterCairoTexture.
 *
 * This function will cause the #ClutterCairoTexture::draw signal
 * to be emitted.
 *
 * See also: clutter_cairo_texture_invalidate_rectangle()
 *
 * Since: 1.8
 * Deprecated: 1.12: Use #ClutterCanvas instead
 */
void
clutter_cairo_texture_invalidate (ClutterCairoTexture *self)
{
  g_return_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self));

  clutter_cairo_texture_invalidate_rectangle (self, NULL);
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
 * Do not call this function within the paint virtual
 * function or from a callback to the #ClutterActor::paint
 * signal.
 *
 * Return value: a newly created Cairo context. Use cairo_destroy()
 *   to upload the contents of the context when done drawing
 *
 * Since: 1.0
 *
 * Deprecated: 1.8: Use the #ClutterCairoTexture::draw signal and
 *   the clutter_cairo_texture_invalidate() function to obtain a
 *   Cairo context for 2D drawing.
 */
cairo_t *
clutter_cairo_texture_create (ClutterCairoTexture *self)
{
  g_return_val_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self), NULL);

  clutter_warn_if_paint_fail (self);

  return clutter_cairo_texture_create_region_internal (self, 0, 0, -1, -1);
}

/**
 * clutter_cairo_texture_set_surface_size:
 * @self: a #ClutterCairoTexture
 * @width: the new width of the surface
 * @height: the new height of the surface
 *
 * Resizes the Cairo surface used by @self to @width and @height.
 *
 * This function will not invalidate the contents of the Cairo
 * texture: you will have to explicitly call either
 * clutter_cairo_texture_invalidate_rectangle() or
 * clutter_cairo_texture_invalidate().
 *
 * Since: 1.0
 * Deprecated: 1.12: Use #ClutterCanvas instead
 */
void
clutter_cairo_texture_set_surface_size (ClutterCairoTexture *self,
                                        guint                width,
                                        guint                height)
{
  ClutterCairoTexturePrivate *priv;

  g_return_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self));

  priv = self->priv;

  if (width == priv->surface_width &&
      height == priv->surface_height)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  if (priv->surface_width != width)
    {
      priv->surface_width = width;
      g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_SURFACE_WIDTH]);
    }

  if (priv->surface_height != height)
    {
      priv->surface_height = height;
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
 * Deprecated: 1.12: Use #ClutterCanvas instead
 */
void
clutter_cairo_texture_get_surface_size (ClutterCairoTexture *self,
                                        guint               *width,
                                        guint               *height)
{
  g_return_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self));

  if (width)
    *width = self->priv->surface_width;

  if (height)
    *height = self->priv->surface_height;
}

/**
 * clutter_cairo_texture_clear:
 * @self: a #ClutterCairoTexture
 *
 * Clears @self's internal drawing surface, so that the next upload
 * will replace the previous contents of the #ClutterCairoTexture
 * rather than adding to it.
 *
 * Calling this function from within a #ClutterCairoTexture::draw
 * signal handler will clear the invalidated area.
 *
 * Since: 1.0
 * Deprecated: 1.12: Use #ClutterCanvas instead
 */
void
clutter_cairo_texture_clear (ClutterCairoTexture *self)
{
  ClutterCairoTexturePrivate *priv;
  cairo_t *cr;

  g_return_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self));

  priv = self->priv;

  /* if we got called outside of a ::draw signal handler
   * then we clear the whole surface by creating a temporary
   * cairo_t; otherwise, we clear the current cairo_t, which
   * will take into account the clip region.
   */
  if (priv->cr_context == NULL)
    {
      cairo_surface_t *surface;

      surface = get_surface (self);

      cr = cairo_create (surface);
    }
  else
    cr = priv->cr_context;

  cairo_save (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_restore (cr);

  if (priv->cr_context == NULL)
    cairo_destroy (cr);
}

/**
 * clutter_cairo_texture_set_auto_resize:
 * @self: a #ClutterCairoTexture
 * @value: %TRUE if the #ClutterCairoTexture should bind the surface
 *   size to the allocation
 *
 * Sets whether the #ClutterCairoTexture should ensure that the
 * backing Cairo surface used matches the allocation assigned to
 * the actor. If the allocation changes, the contents of the
 * #ClutterCairoTexture will also be invalidated automatically.
 *
 * Since: 1.8
 * Deprecated: 1.12: Use #ClutterCanvas instead
 */
void
clutter_cairo_texture_set_auto_resize (ClutterCairoTexture *self,
                                       gboolean             value)
{
  ClutterCairoTexturePrivate *priv;

  g_return_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self));

  value = !!value;

  priv = self->priv;

  if (priv->auto_resize == value)
    return;

  priv->auto_resize = value;

  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_AUTO_RESIZE]);
}

/**
 * clutter_cairo_texture_get_auto_resize:
 * @self: a #ClutterCairoTexture
 *
 * Retrieves the value set using clutter_cairo_texture_set_auto_resize().
 *
 * Return value: %TRUE if the #ClutterCairoTexture should track the
 *   allocation, and %FALSE otherwise
 *
 * Since: 1.8
 * Deprecated: 1.12: Use #ClutterCanvas instead
 */
gboolean
clutter_cairo_texture_get_auto_resize (ClutterCairoTexture *self)
{
  g_return_val_if_fail (CLUTTER_IS_CAIRO_TEXTURE (self), FALSE);

  return self->priv->auto_resize;
}
