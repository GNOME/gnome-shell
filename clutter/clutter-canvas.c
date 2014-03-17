/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012  Intel Corporation.
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
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-canvas
 * @Title: ClutterCanvas
 * @Short_Description: Content for 2D painting
 * @See_Also: #ClutterContent
 *
 * The #ClutterCanvas class is a #ClutterContent implementation that allows
 * drawing using the Cairo API on a 2D surface.
 *
 * In order to draw on a #ClutterCanvas, you should connect a handler to the
 * #ClutterCanvas::draw signal; the signal will receive a #cairo_t context
 * that can be used to draw. #ClutterCanvas will emit the #ClutterCanvas::draw
 * signal when invalidated using clutter_content_invalidate().
 *
 * See [canvas.c](https://git.gnome.org/browse/clutter/tree/examples/canvas.c?h=clutter-1.18)
 * for an example of how to use #ClutterCanvas.
 *
 * #ClutterCanvas is available since Clutter 1.10.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cogl/cogl.h>
#include <cairo-gobject.h>

#include "clutter-canvas.h"

#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include "clutter-backend.h"
#include "clutter-cairo.h"
#include "clutter-color.h"
#include "clutter-content-private.h"
#include "clutter-debug.h"
#include "clutter-marshal.h"
#include "clutter-paint-node.h"
#include "clutter-paint-nodes.h"
#include "clutter-private.h"
#include "clutter-settings.h"

struct _ClutterCanvasPrivate
{
  cairo_t *cr;

  int width;
  int height;

  CoglBitmap *buffer;

  int scale_factor;
  guint scale_factor_set : 1;
};

enum
{
  PROP_0,

  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_SCALE_FACTOR,
  PROP_SCALE_FACTOR_SET,

  LAST_PROP
};

static GParamSpec *obj_props[LAST_PROP] = { NULL, };

enum
{
  DRAW,

  LAST_SIGNAL
};

static guint canvas_signals[LAST_SIGNAL] = { 0, };

static void clutter_content_iface_init (ClutterContentIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterCanvas, clutter_canvas, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (ClutterCanvas)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTENT,
                                                clutter_content_iface_init))

static void
clutter_cairo_context_draw_marshaller (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data)
{
  cairo_t *cr = g_value_get_boxed (&param_values[1]);

  cairo_save (cr);

  _clutter_marshal_BOOLEAN__BOXED_INT_INT (closure,
                                           return_value,
                                           n_param_values,
                                           param_values,
                                           invocation_hint,
                                           marshal_data);

  cairo_restore (cr);
}

static void
clutter_canvas_finalize (GObject *gobject)
{
  ClutterCanvasPrivate *priv = CLUTTER_CANVAS (gobject)->priv;

  if (priv->buffer != NULL)
    {
      cogl_object_unref (priv->buffer);
      priv->buffer = NULL;
    }

  G_OBJECT_CLASS (clutter_canvas_parent_class)->finalize (gobject);
}

static void
clutter_canvas_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ClutterCanvasPrivate *priv = CLUTTER_CANVAS (gobject)->priv;

  switch (prop_id)
    {
    case PROP_WIDTH:
      {
        gint new_size = g_value_get_int (value);

        if (priv->width != new_size)
          {
            priv->width = new_size;

            clutter_content_invalidate (CLUTTER_CONTENT (gobject));
          }
      }
      break;

    case PROP_HEIGHT:
      {
        gint new_size = g_value_get_int (value);

        if (priv->height != new_size)
          {
            priv->height = new_size;

            clutter_content_invalidate (CLUTTER_CONTENT (gobject));
          }
      }
      break;

    case PROP_SCALE_FACTOR:
      clutter_canvas_set_scale_factor (CLUTTER_CANVAS (gobject),
                                       g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_canvas_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ClutterCanvasPrivate *priv = CLUTTER_CANVAS (gobject)->priv;

  switch (prop_id)
    {
    case PROP_WIDTH:
      g_value_set_int (value, priv->width);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, priv->height);
      break;

    case PROP_SCALE_FACTOR:
      if (priv->scale_factor_set)
        g_value_set_int (value, priv->scale_factor);
      else
        g_value_set_int (value, -1);
      break;

    case PROP_SCALE_FACTOR_SET:
      g_value_set_boolean (value, priv->scale_factor_set);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_canvas_class_init (ClutterCanvasClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  /**
   * ClutterCanvas:width:
   *
   * The width of the canvas.
   *
   * Since: 1.10
   */
  obj_props[PROP_WIDTH] =
    g_param_spec_int ("width",
                      P_("Width"),
                      P_("The width of the canvas"),
                      -1, G_MAXINT,
                      -1,
                      G_PARAM_READWRITE |
                      G_PARAM_STATIC_STRINGS);

  /**
   * ClutterCanvas:height:
   *
   * The height of the canvas.
   *
   * Since: 1.10
   */
  obj_props[PROP_HEIGHT] =
    g_param_spec_int ("height",
                      P_("Height"),
                      P_("The height of the canvas"),
                      -1, G_MAXINT,
                      -1,
                      G_PARAM_READWRITE |
                      G_PARAM_STATIC_STRINGS);

  /**
   * ClutterCanvas:scale-factor-set:
   *
   * Whether the #ClutterCanvas:scale-factor property is set.
   *
   * If the #ClutterCanvas:scale-factor-set property is %FALSE
   * then #ClutterCanvas will use the #ClutterSettings:window-scaling-factor
   * property.
   *
   * Since: 1.18
   */
  obj_props[PROP_SCALE_FACTOR_SET] =
    g_param_spec_boolean ("scale-factor-set",
                          P_("Scale Factor Set"),
                          P_("Whether the scale-factor property is set"),
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * ClutterCanvas:scale-factor:
   *
   * The scaling factor to be applied to the Cairo surface used for
   * drawing.
   *
   * If #ClutterCanvas:scale-factor is set to a negative value, the
   * value of the #ClutterSettings:window-scaling-factor property is
   * used instead.
   *
   * Use #ClutterCanvas:scale-factor-set to check if the scale factor
   * is set.
   *
   * Since: 1.18
   */
  obj_props[PROP_SCALE_FACTOR] =
    g_param_spec_int ("scale-factor",
                      P_("Scale Factor"),
                      P_("The scaling factor for the surface"),
                      -1, 1000,
                      -1,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * ClutterCanvas::draw:
   * @canvas: the #ClutterCanvas that emitted the signal
   * @cr: the Cairo context used to draw
   * @width: the width of the @canvas
   * @height: the height of the @canvas
   *
   * The #ClutterCanvas::draw signal is emitted each time a canvas is
   * invalidated.
   *
   * It is safe to connect multiple handlers to this signal: each
   * handler invocation will be automatically protected by cairo_save()
   * and cairo_restore() pairs.
   *
   * Return value: %TRUE if the signal emission should stop, and
   *   %FALSE otherwise
   *
   * Since: 1.10
   */
  canvas_signals[DRAW] =
    g_signal_new (I_("draw"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                  G_STRUCT_OFFSET (ClutterCanvasClass, draw),
                  _clutter_boolean_handled_accumulator, NULL,
                  clutter_cairo_context_draw_marshaller,
                  G_TYPE_BOOLEAN, 3,
                  CAIRO_GOBJECT_TYPE_CONTEXT,
                  G_TYPE_INT,
                  G_TYPE_INT);

  gobject_class->set_property = clutter_canvas_set_property;
  gobject_class->get_property = clutter_canvas_get_property;
  gobject_class->finalize = clutter_canvas_finalize;

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_props);
}

static void
clutter_canvas_init (ClutterCanvas *self)
{
  self->priv = clutter_canvas_get_instance_private (self);

  self->priv->width = -1;
  self->priv->height = -1;
  self->priv->scale_factor = -1;
}

static void
clutter_canvas_paint_content (ClutterContent   *content,
                              ClutterActor     *actor,
                              ClutterPaintNode *root)
{
  ClutterCanvas *self = CLUTTER_CANVAS (content);
  ClutterPaintNode *node;
  CoglTexture *texture;
  ClutterActorBox box;
  ClutterColor color;
  guint8 paint_opacity;
  ClutterScalingFilter min_f, mag_f;
  ClutterContentRepeat repeat;

  if (self->priv->buffer == NULL)
    return;

  texture = cogl_texture_new_from_bitmap (self->priv->buffer,
                                          COGL_TEXTURE_NO_SLICING,
                                          CLUTTER_CAIRO_FORMAT_ARGB32);
  if (texture == NULL)
    return;

  clutter_actor_get_content_box (actor, &box);
  paint_opacity = clutter_actor_get_paint_opacity (actor);
  clutter_actor_get_content_scaling_filters (actor, &min_f, &mag_f);
  repeat = clutter_actor_get_content_repeat (actor);

  color.red = paint_opacity;
  color.green = paint_opacity;
  color.blue = paint_opacity;
  color.alpha = paint_opacity;

  node = clutter_texture_node_new (texture, &color, min_f, mag_f);
  cogl_object_unref (texture);

  clutter_paint_node_set_name (node, "Canvas");

  if (repeat == CLUTTER_REPEAT_NONE)
    clutter_paint_node_add_rectangle (node, &box);
  else
    {
      float t_w = 1.f, t_h = 1.f;

      if ((repeat & CLUTTER_REPEAT_X_AXIS) != FALSE)
        t_w = (box.x2 - box.x1) / cogl_texture_get_width (texture);

      if ((repeat & CLUTTER_REPEAT_Y_AXIS) != FALSE)
        t_h = (box.y2 - box.y1) / cogl_texture_get_height (texture);

      clutter_paint_node_add_texture_rectangle (node, &box,
                                                0.f, 0.f,
                                                t_w, t_h);
    }

  clutter_paint_node_add_child (root, node);
  clutter_paint_node_unref (node);
}

static void
clutter_canvas_emit_draw (ClutterCanvas *self)
{
  ClutterCanvasPrivate *priv = self->priv;
  int real_width, real_height;
  cairo_surface_t *surface;
  gboolean mapped_buffer;
  unsigned char *data;
  CoglBuffer *buffer;
  int window_scale = 1;
  gboolean res;
  cairo_t *cr;

  g_assert (priv->width > 0 && priv->width > 0);

  if (priv->scale_factor_set)
    window_scale = priv->scale_factor;
  else
    g_object_get (clutter_settings_get_default (),
                  "window-scaling-factor", &window_scale,
                  NULL);

  real_width = priv->width * window_scale;
  real_height = priv->height * window_scale;

  CLUTTER_NOTE (MISC, "Creating Cairo surface with size %d x %d (real: %d x %d, scale: %d)",
                priv->width, priv->height,
                real_width, real_height,
                window_scale);

  if (priv->buffer == NULL)
    {
      CoglContext *ctx;

      ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
      priv->buffer = cogl_bitmap_new_with_size (ctx,
                                                real_width,
                                                real_height,
                                                CLUTTER_CAIRO_FORMAT_ARGB32);
    }

  buffer = COGL_BUFFER (cogl_bitmap_get_buffer (priv->buffer));
  if (buffer == NULL)
    return;

  cogl_buffer_set_update_hint (buffer, COGL_BUFFER_UPDATE_HINT_DYNAMIC);

  data = cogl_buffer_map (buffer,
                          COGL_BUFFER_ACCESS_READ_WRITE,
                          COGL_BUFFER_MAP_HINT_DISCARD);

  if (data != NULL)
    {
      int bitmap_stride = cogl_bitmap_get_rowstride (priv->buffer);

      surface = cairo_image_surface_create_for_data (data,
                                                     CAIRO_FORMAT_ARGB32,
                                                     real_width,
                                                     real_height,
                                                     bitmap_stride);
      mapped_buffer = TRUE;
    }
  else
    {
      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            real_width,
                                            real_height);

      mapped_buffer = FALSE;
    }

#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
  cairo_surface_set_device_scale (surface, window_scale, window_scale);
#endif

  self->priv->cr = cr = cairo_create (surface);

  g_signal_emit (self, canvas_signals[DRAW], 0,
                 cr, priv->width, priv->height,
                 &res);

#ifdef CLUTTER_ENABLE_DEBUG
  if (_clutter_diagnostic_enabled () && cairo_status (cr))
    {
      g_warning ("Drawing failed for <ClutterCanvas>[%p]: %s",
                 self,
                 cairo_status_to_string (cairo_status (cr)));
    }
#endif

  self->priv->cr = NULL;
  cairo_destroy (cr);

  if (mapped_buffer)
    cogl_buffer_unmap (buffer);
  else
    {
      int size = cairo_image_surface_get_stride (surface) * priv->height;
      cogl_buffer_set_data (buffer,
                            0,
                            cairo_image_surface_get_data (surface),
                            size);
    }

  cairo_surface_destroy (surface);
}

static void
clutter_canvas_invalidate (ClutterContent *content)
{
  ClutterCanvas *self = CLUTTER_CANVAS (content);
  ClutterCanvasPrivate *priv = self->priv;

  if (priv->buffer != NULL)
    {
      cogl_object_unref (priv->buffer);
      priv->buffer = NULL;
    }

  if (priv->width <= 0 || priv->height <= 0)
    return;

  clutter_canvas_emit_draw (self);
}

static gboolean
clutter_canvas_get_preferred_size (ClutterContent *content,
                                   gfloat         *width,
                                   gfloat         *height)
{
  ClutterCanvasPrivate *priv = CLUTTER_CANVAS (content)->priv;

  if (priv->width < 0 || priv->height < 0)
    return FALSE;

  if (width != NULL)
    *width = priv->width;

  if (height != NULL)
    *height = priv->height;

  return TRUE;
}

static void
clutter_content_iface_init (ClutterContentIface *iface)
{
  iface->invalidate = clutter_canvas_invalidate;
  iface->paint_content = clutter_canvas_paint_content;
  iface->get_preferred_size = clutter_canvas_get_preferred_size;
}

/**
 * clutter_canvas_new:
 *
 * Creates a new instance of #ClutterCanvas.
 *
 * You should call clutter_canvas_set_size() to set the size of the canvas.
 *
 * You should call clutter_content_invalidate() every time you wish to
 * draw the contents of the canvas.
 *
 * Return value: (transfer full): The newly allocated instance of
 *   #ClutterCanvas. Use g_object_unref() when done.
 *
 * Since: 1.10
 */
ClutterContent *
clutter_canvas_new (void)
{
  return g_object_new (CLUTTER_TYPE_CANVAS, NULL);
}

static gboolean
clutter_canvas_invalidate_internal (ClutterCanvas *canvas,
                                    int            width,
                                    int            height)
{
  gboolean width_changed = FALSE, height_changed = FALSE;
  gboolean res = FALSE;
  GObject *obj;

  obj = G_OBJECT (canvas);

  g_object_freeze_notify (obj);

  if (canvas->priv->width != width)
    {
      canvas->priv->width = width;
      width_changed = TRUE;

      g_object_notify_by_pspec (obj, obj_props[PROP_WIDTH]);
    }

  if (canvas->priv->height != height)
    {
      canvas->priv->height = height;
      height_changed = TRUE;

      g_object_notify_by_pspec (obj, obj_props[PROP_HEIGHT]);
    }

  if (width_changed || height_changed)
    {
      clutter_content_invalidate (CLUTTER_CONTENT (canvas));
      res = TRUE;
    }

  g_object_thaw_notify (obj);

  return res;
}

/**
 * clutter_canvas_set_size:
 * @canvas: a #ClutterCanvas
 * @width: the width of the canvas, in pixels
 * @height: the height of the canvas, in pixels
 *
 * Sets the size of the @canvas, and invalidates the content.
 *
 * This function will cause the @canvas to be invalidated only
 * if the size of the canvas surface has changed.
 *
 * If you want to invalidate the contents of the @canvas when setting
 * the size, you can use the return value of the function to conditionally
 * call clutter_content_invalidate():
 *
 * |[
 *   if (!clutter_canvas_set_size (canvas, width, height))
 *     clutter_content_invalidate (CLUTTER_CONTENT (canvas));
 * ]|
 *
 * Return value: this function returns %TRUE if the size change
 *   caused a content invalidation, and %FALSE otherwise
 *
 * Since: 1.10
 */
gboolean
clutter_canvas_set_size (ClutterCanvas *canvas,
                         int            width,
                         int            height)
{
  g_return_val_if_fail (CLUTTER_IS_CANVAS (canvas), FALSE);
  g_return_val_if_fail (width >= -1 && height >= -1, FALSE);

  return clutter_canvas_invalidate_internal (canvas, width, height);
}

/**
 * clutter_canvas_set_scale_factor:
 * @canvas: a #ClutterCanvas
 * @scale: the scale factor, or -1 for the default
 *
 * Sets the scaling factor for the Cairo surface used by @canvas.
 *
 * This function should rarely be used.
 *
 * The default scaling factor of a #ClutterCanvas content uses the
 * #ClutterSettings:window-scaling-factor property, which is set by
 * the windowing system. By using this function it is possible to
 * override that setting.
 *
 * Changing the scale factor will invalidate the @canvas.
 *
 * Since: 1.18
 */
void
clutter_canvas_set_scale_factor (ClutterCanvas *canvas,
                                 int            scale)
{
  ClutterCanvasPrivate *priv;
  GObject *obj;

  g_return_if_fail (CLUTTER_IS_CANVAS (canvas));
  g_return_if_fail (scale != 0);

  priv = canvas->priv;

  if (scale < 0)
    {
      if (!priv->scale_factor_set)
        return;

      priv->scale_factor_set = FALSE;
      priv->scale_factor = -1;
    }
  else
    {
      if (priv->scale_factor_set && priv->scale_factor == scale)
        return;

      priv->scale_factor_set = TRUE;
      priv->scale_factor = scale;
    }

  clutter_content_invalidate (CLUTTER_CONTENT (canvas));

  obj = G_OBJECT (canvas);

  g_object_notify_by_pspec (obj, obj_props[PROP_SCALE_FACTOR]);
  g_object_notify_by_pspec (obj, obj_props[PROP_SCALE_FACTOR_SET]);
}

/**
 * clutter_canvas_get_scale_factor:
 * @canvas: a #ClutterCanvas
 *
 * Retrieves the scaling factor of @canvas, as set using
 * clutter_canvas_set_scale_factor().
 *
 * Return value: the scaling factor, or -1 if the @canvas
 *   uses the default from #ClutterSettings
 *
 * Since: 1.18
 */
int
clutter_canvas_get_scale_factor (ClutterCanvas *canvas)
{
  g_return_val_if_fail (CLUTTER_IS_CANVAS (canvas), -1);

  if (!canvas->priv->scale_factor_set)
    return -1;

  return canvas->priv->scale_factor;
}
