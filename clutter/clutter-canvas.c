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
 * <informalexample id="canvas-example">
 *   <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../examples/canvas.c">
 *   <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 *   </programlisting>
 * </informalexample>
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
#include "clutter-marshal.h"
#include "clutter-paint-node.h"
#include "clutter-paint-nodes.h"
#include "clutter-private.h"

struct _ClutterCanvasPrivate
{
  cairo_t *cr;

  int width;
  int height;

  CoglTexture *texture;
  gboolean dirty;

  CoglBitmap *buffer;
};

enum
{
  PROP_0,

  PROP_WIDTH,
  PROP_HEIGHT,

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

  g_clear_pointer (&priv->texture, cogl_object_unref);

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
      if (priv->width != g_value_get_int (value))
        {
          priv->width = g_value_get_int (value);
          clutter_content_invalidate (CLUTTER_CONTENT (gobject));
        }
      break;

    case PROP_HEIGHT:
      if (priv->height != g_value_get_int (value))
        {
          priv->height = g_value_get_int (value);
          clutter_content_invalidate (CLUTTER_CONTENT (gobject));
        }
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_canvas_class_init (ClutterCanvasClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterCanvasPrivate));

  /**
   * ClutterCanvas:width:
   *
   * The width of the canvas.
   *
   *
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
   *
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
   *
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
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, CLUTTER_TYPE_CANVAS,
                                            ClutterCanvasPrivate);

  self->priv->width = -1;
  self->priv->height = -1;
}

static void
clutter_canvas_paint_content (ClutterContent   *content,
                              ClutterActor     *actor,
                              ClutterPaintNode *root)
{
  ClutterCanvas *self = CLUTTER_CANVAS (content);
  ClutterCanvasPrivate *priv = self->priv;
  ClutterPaintNode *node;
  ClutterActorBox box;
  ClutterColor color;
  guint8 paint_opacity;
  ClutterScalingFilter min_f, mag_f;
  ClutterContentRepeat repeat;

  if (priv->buffer == NULL)
    return;

  if (priv->texture && priv->dirty)
    g_clear_pointer (&priv->texture, cogl_object_unref);

  if (!priv->texture)
    priv->texture = cogl_texture_new_from_bitmap (self->priv->buffer,
                                                  COGL_TEXTURE_NO_SLICING,
                                                  CLUTTER_CAIRO_FORMAT_ARGB32);

  if (priv->texture == NULL)
    return;

  clutter_actor_get_content_box (actor, &box);
  paint_opacity = clutter_actor_get_paint_opacity (actor);
  clutter_actor_get_content_scaling_filters (actor, &min_f, &mag_f);
  repeat = clutter_actor_get_content_repeat (actor);

  color.red = paint_opacity;
  color.green = paint_opacity;
  color.blue = paint_opacity;
  color.alpha = paint_opacity;

  node = clutter_texture_node_new (priv->texture, &color, min_f, mag_f);

  clutter_paint_node_set_name (node, "Canvas");

  if (repeat == CLUTTER_REPEAT_NONE)
    clutter_paint_node_add_rectangle (node, &box);
  else
    {
      float t_w = 1.f, t_h = 1.f;

      if ((repeat & CLUTTER_REPEAT_X_AXIS) != FALSE)
        t_w = (box.x2 - box.x1) / cogl_texture_get_width (priv->texture);

      if ((repeat & CLUTTER_REPEAT_Y_AXIS) != FALSE)
        t_h = (box.y2 - box.y1) / cogl_texture_get_height (priv->texture);

      clutter_paint_node_add_texture_rectangle (node, &box,
                                                0.f, 0.f,
                                                t_w, t_h);
    }

  clutter_paint_node_add_child (root, node);
  clutter_paint_node_unref (node);

  priv->dirty = FALSE;
}

static void
clutter_canvas_emit_draw (ClutterCanvas *self)
{
  ClutterCanvasPrivate *priv = self->priv;
  cairo_surface_t *surface;
  gboolean mapped_buffer;
  unsigned char *data;
  CoglBuffer *buffer;
  gboolean res;
  cairo_t *cr;

  g_assert (priv->width > 0 && priv->width > 0);

  priv->dirty = TRUE;

  if (priv->buffer == NULL)
    {
      CoglContext *ctx;

      ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
      priv->buffer = cogl_bitmap_new_with_size (ctx,
                                                priv->width,
                                                priv->height,
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
                                                     priv->width,
                                                     priv->height,
                                                     bitmap_stride);
      mapped_buffer = TRUE;
    }
  else
    {
      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            priv->width,
                                            priv->height);

      mapped_buffer = FALSE;
    }

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
 *
 */
ClutterContent *
clutter_canvas_new (void)
{
  return g_object_new (CLUTTER_TYPE_CANVAS, NULL);
}

/**
 * clutter_canvas_set_size:
 * @canvas: a #ClutterCanvas
 * @width: the width of the canvas, in pixels
 * @height: the height of the canvas, in pixels
 *
 * Sets the size of the @canvas.
 *
 * This function will cause the @canvas to be invalidated.
 *
 *
 */
void
clutter_canvas_set_size (ClutterCanvas *canvas,
                         int            width,
                         int            height)
{
  GObject *obj;
  gboolean width_changed = FALSE, height_changed = FALSE;

  g_return_if_fail (CLUTTER_IS_CANVAS (canvas));
  g_return_if_fail (width >= -1 && height >= -1);

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
    clutter_content_invalidate (CLUTTER_CONTENT (canvas));

  g_object_thaw_notify (obj);
}
