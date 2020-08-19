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
 * SECTION:st-drawing-area
 * @short_description: A dynamically-sized Cairo drawing area
 *
 * #StDrawingArea allows drawing via Cairo; the primary difference is that
 * it is dynamically sized. To use, connect to the #StDrawingArea::repaint
 * signal, and inside the signal handler, call
 * st_drawing_area_get_context() to get the Cairo context to draw to.  The
 * #StDrawingArea::repaint signal will be emitted by default when the area is
 * resized or the CSS style changes; you can use the
 * st_drawing_area_queue_repaint() as well.
 */

#include "st-drawing-area.h"

#include <cairo.h>
#include <math.h>

typedef struct _StDrawingAreaPrivate StDrawingAreaPrivate;
struct _StDrawingAreaPrivate {
  cairo_t *context;
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

static gboolean
draw_content (ClutterCanvas *canvas,
              cairo_t       *cr,
              int            width,
              int            height,
              gpointer       user_data)
{
  StDrawingArea *area = ST_DRAWING_AREA (user_data);
  StDrawingAreaPrivate *priv = st_drawing_area_get_instance_private (area);

  priv->context = cr;
  priv->in_repaint = TRUE;

  clutter_cairo_clear (cr);
  g_signal_emit (area, st_drawing_area_signals[REPAINT], 0);

  priv->context = NULL;
  priv->in_repaint = FALSE;

  return TRUE;
}

static void
st_drawing_area_allocate (ClutterActor          *self,
                          const ClutterActorBox *box)
{
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (self));
  ClutterContent *content = clutter_actor_get_content (self);
  ClutterActorBox content_box;
  int width, height;
  float resource_scale;

  resource_scale = clutter_actor_get_resource_scale (self);

  clutter_actor_set_allocation (self, box);
  st_theme_node_get_content_box (theme_node, box, &content_box);

  width = (int)(0.5 + content_box.x2 - content_box.x1);
  height = (int)(0.5 + content_box.y2 - content_box.y1);

  clutter_canvas_set_scale_factor (CLUTTER_CANVAS (content), resource_scale);
  clutter_canvas_set_size (CLUTTER_CANVAS (content), width, height);
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
  float resource_scale;
  ClutterContent *content = clutter_actor_get_content (self);

  resource_scale = clutter_actor_get_resource_scale (self);
  clutter_canvas_set_scale_factor (CLUTTER_CANVAS (content), resource_scale);

  if (CLUTTER_ACTOR_CLASS (st_drawing_area_parent_class)->resource_scale_changed)
    CLUTTER_ACTOR_CLASS (st_drawing_area_parent_class)->resource_scale_changed (self);
}

static void
st_drawing_area_class_init (StDrawingAreaClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);

  actor_class->allocate = st_drawing_area_allocate;
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
  ClutterContent *content = clutter_canvas_new ();
  g_signal_connect (content, "draw", G_CALLBACK (draw_content), area);
  clutter_actor_set_content (CLUTTER_ACTOR (area), content);
  g_object_unref (content);
}

/**
 * st_drawing_area_queue_repaint:
 * @area: the #StDrawingArea
 *
 * Will cause the actor to emit a #StDrawingArea::repaint signal before it is
 * next drawn to the scene. Useful if some parameters for the area being
 * drawn other than the size or style have changed. Note that
 * clutter_actor_queue_redraw() will simply result in the same
 * contents being drawn to the scene again.
 */
void
st_drawing_area_queue_repaint (StDrawingArea *area)
{
  g_return_if_fail (ST_IS_DRAWING_AREA (area));

  clutter_content_invalidate (clutter_actor_get_content (CLUTTER_ACTOR (area)));
}

/**
 * st_drawing_area_get_context:
 * @area: the #StDrawingArea
 *
 * Gets the Cairo context to paint to. This function must only be called
 * from a signal handler or virtual function for the #StDrawingArea::repaint
 * signal.
 *
 * JavaScript code must call the special dispose function before returning from
 * the signal handler or virtual function to avoid leaking memory:
 *
 * |[<!-- language="JavaScript" -->
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
 * ]|
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
  ClutterContent *content;
  float w, h, resource_scale;

  g_return_if_fail (ST_IS_DRAWING_AREA (area));

  priv = st_drawing_area_get_instance_private (area);
  g_return_if_fail (priv->in_repaint);

  content = clutter_actor_get_content (CLUTTER_ACTOR (area));
  clutter_content_get_preferred_size (content, &w, &h);

  resource_scale = clutter_actor_get_resource_scale (CLUTTER_ACTOR (area));

  w /= resource_scale;
  h /= resource_scale;

  if (width)
    *width = ceilf (w);
  if (height)
    *height = ceilf (h);
}
