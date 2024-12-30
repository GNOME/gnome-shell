/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-drawing-area.c: A dynamically-sized Cairo drawing area
 *
 * Copyright 2009, 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * StDrawingArea:
 *
 * A dynamically-sized Cairo drawing area
 *
 * #StDrawingArea allows drawing via Cairo; the primary difference is that
 * it is dynamically sized. To use, connect to the [signal@St.DrawingArea::repaint]
 * signal, and inside the signal handler, call
 * [method@St.DrawingArea.get_context] to get the Cairo context to draw to.  The
 * [signal@St.DrawingArea::repaint] signal will be emitted by default when the area is
 * resized or the CSS style changes; you can use the
 * [method@St.DrawingArea.queue_repaint] as well.
 */

#include "st-drawing-area.h"

#include <cairo.h>
#include <math.h>

typedef struct _StDrawingAreaPrivate StDrawingAreaPrivate;
struct _StDrawingAreaPrivate {
  cairo_t *context;

  int width;
  int height;
  float scale_factor;

  CoglTexture *texture;
  CoglBitmap *buffer;

  gboolean dirty;
  guint in_repaint : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (StDrawingArea, st_drawing_area, ST_TYPE_WIDGET);

/* Signals */
enum
{
  REPAINT,
  LAST_SIGNAL
};

static guint st_drawing_area_signals [LAST_SIGNAL] = { 0 };

static void
st_drawing_area_allocate (ClutterActor          *self,
                          const ClutterActorBox *box)
{
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (self));
  StDrawingAreaPrivate *priv =
      st_drawing_area_get_instance_private (ST_DRAWING_AREA (self));
  ClutterActorBox content_box;

  priv->scale_factor = clutter_actor_get_resource_scale (self);

  clutter_actor_set_allocation (self, box);
  st_theme_node_get_content_box (theme_node, box, &content_box);

  priv->width = (int)(0.5 + content_box.x2 - content_box.x1);
  priv->height = (int)(0.5 + content_box.y2 - content_box.y1);

  st_drawing_area_queue_repaint (ST_DRAWING_AREA (self));
}

static void
st_drawing_area_paint_node (ClutterActor        *actor,
                            ClutterPaintNode    *root,
                            ClutterPaintContext *paint_context)
{
  StDrawingArea *area = ST_DRAWING_AREA (actor);
  StDrawingAreaPrivate *priv = st_drawing_area_get_instance_private (area);
  ClutterPaintNode *node;

  if (priv->buffer == NULL)
    return;

  if (priv->dirty)
    g_clear_object (&priv->texture);

  if (priv->texture == NULL)
    priv->texture = cogl_texture_2d_new_from_bitmap (priv->buffer);

  if (priv->texture == NULL)
    return;

  node = clutter_actor_create_texture_paint_node (actor, priv->texture);
  clutter_paint_node_set_static_name (node, "Canvas Content");
  clutter_paint_node_add_child (root, node);
  clutter_paint_node_unref (node);

  priv->dirty = FALSE;
}

static void
st_drawing_area_style_changed (StWidget  *self)
{
  (ST_WIDGET_CLASS (st_drawing_area_parent_class))->style_changed (self);

  st_drawing_area_queue_repaint (ST_DRAWING_AREA (self));
}

static void
st_drawing_area_resource_scale_changed (ClutterActor *self)
{
  StDrawingArea *area = ST_DRAWING_AREA (self);
  StDrawingAreaPrivate *priv = st_drawing_area_get_instance_private (area);
  float resource_scale;

  resource_scale = clutter_actor_get_resource_scale (self);
  if (priv->scale_factor != resource_scale)
    {
      priv->scale_factor = resource_scale;

      st_drawing_area_queue_repaint (area);

      if (CLUTTER_ACTOR_CLASS (st_drawing_area_parent_class)->resource_scale_changed)
        CLUTTER_ACTOR_CLASS (st_drawing_area_parent_class)->resource_scale_changed (self);
    }
}

static void
st_drawing_area_finalize (GObject *self)
{
  StDrawingAreaPrivate *priv =
      st_drawing_area_get_instance_private (ST_DRAWING_AREA (self));

  g_clear_object (&priv->buffer);
  g_clear_object (&priv->texture);

  G_OBJECT_CLASS (st_drawing_area_parent_class)->finalize (self);
}

static void
st_drawing_area_class_init (StDrawingAreaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);

  gobject_class->finalize = st_drawing_area_finalize;
  actor_class->allocate = st_drawing_area_allocate;
  actor_class->paint_node = st_drawing_area_paint_node;
  widget_class->style_changed = st_drawing_area_style_changed;
  actor_class->resource_scale_changed = st_drawing_area_resource_scale_changed;

  st_drawing_area_signals[REPAINT] =
    g_signal_new ("repaint",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (StDrawingAreaClass, repaint),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
st_drawing_area_init (StDrawingArea *area)
{
  StDrawingAreaPrivate *priv = st_drawing_area_get_instance_private (area);

  priv->width = -1;
  priv->height = -1;
  priv->scale_factor = 1.0f;
}

static void
st_drawing_area_emit_repaint (StDrawingArea *area)
{
  StDrawingAreaPrivate *priv = st_drawing_area_get_instance_private (area);
  int real_width, real_height;
  cairo_surface_t *surface;
  gboolean mapped_buffer;
  unsigned char *data;
  CoglBuffer *buffer;
  cairo_t *cr;

  g_assert (priv->height > 0 && priv->width > 0);

  priv->dirty = TRUE;

  real_width = ceilf (priv->width * priv->scale_factor);
  real_height = ceilf (priv->height * priv->scale_factor);

  if (priv->buffer == NULL)
    {
      ClutterContext *context = clutter_actor_get_context (CLUTTER_ACTOR (area));
      ClutterBackend *backend = clutter_context_get_backend (context);
      CoglContext *ctx= clutter_backend_get_cogl_context (backend);

      priv->buffer = cogl_bitmap_new_with_size (ctx,
                                                real_width,
                                                real_height,
                                                COGL_PIXEL_FORMAT_CAIRO_ARGB32_COMPAT);
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

  cairo_surface_set_device_scale (surface,
                                  priv->scale_factor,
                                  priv->scale_factor);

  priv->context = cr = cairo_create (surface);

  priv->in_repaint = TRUE;

  cairo_save (priv->context);
  cairo_set_operator (priv->context, CAIRO_OPERATOR_CLEAR);
  cairo_paint (priv->context);
  cairo_restore (priv->context);

  g_signal_emit (area, st_drawing_area_signals[REPAINT], 0);

  priv->context = NULL;
  priv->in_repaint = FALSE;

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

/**
 * st_drawing_area_queue_repaint:
 * @area: the #StDrawingArea
 *
 * Will cause the actor to emit a [signal@St.DrawingArea::repaint] signal before it is
 * next drawn to the scene. Useful if some parameters for the area being
 * drawn other than the size or style have changed. Note that
 * clutter_actor_queue_redraw() will simply result in the same
 * contents being drawn to the scene again.
 */
void
st_drawing_area_queue_repaint (StDrawingArea *area)
{
  StDrawingAreaPrivate *priv;

  g_return_if_fail (ST_IS_DRAWING_AREA (area));

  priv = st_drawing_area_get_instance_private (area);

  g_clear_object (&priv->buffer);

  if (priv->width <= 0 || priv->height <= 0)
    return;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (area));
  st_drawing_area_emit_repaint (area);
}

/**
 * st_drawing_area_get_context:
 * @area: the #StDrawingArea
 *
 * Gets the Cairo context to paint to. This function must only be called
 * from a signal handler or virtual function for the [signal@St.DrawingArea::repaint]
 * signal.
 *
 * JavaScript code must call the special dispose function before returning from
 * the signal handler or virtual function to avoid leaking memory:
 *
 * ```js
 * function onRepaint(area) {
 *     let cr = area.get_context();
 *
 *     // Draw to the context
 *
 *     cr.$dispose();
 * }
 *
 * let area = new St.DrawingArea();
 * area.connect('repaint', onRepaint);
 * ```
 *
 * Returns: (transfer none): the Cairo context for the paint operation
 */
cairo_t *
st_drawing_area_get_context (StDrawingArea *area)
{
  StDrawingAreaPrivate *priv;

  g_return_val_if_fail (ST_IS_DRAWING_AREA (area), NULL);

  priv = st_drawing_area_get_instance_private (area);
  g_return_val_if_fail (priv->in_repaint, NULL);

  return priv->context;
}

/**
 * st_drawing_area_get_surface_size:
 * @area: the #StDrawingArea
 * @width: (out) (optional): location to store the width of the painted area
 * @height: (out) (optional): location to store the height of the painted area
 *
 * Gets the size of the cairo surface being painted to, which is equal
 * to the size of the content area of the widget. This function must
 * only be called from a signal handler for the #StDrawingArea::repaint signal.
 */
void
st_drawing_area_get_surface_size (StDrawingArea *area,
                                  guint         *width,
                                  guint         *height)
{
  StDrawingAreaPrivate *priv;

  g_return_if_fail (ST_IS_DRAWING_AREA (area));

  priv = st_drawing_area_get_instance_private (area);
  g_return_if_fail (priv->in_repaint);

  if (width)
    *width = priv->width;
  if (height)
    *height = priv->height;
}
