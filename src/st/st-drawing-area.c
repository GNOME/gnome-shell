/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:st-drawing-area
 * @short_description: A dynamically-sized Cairo drawing area
 *
 * #StDrawingArea is similar to #ClutterCairoTexture in that
 * it allows drawing via Cairo; the primary difference is that
 * it is dynamically sized.  To use, connect to the #StDrawingArea::redraw
 * signal, and inside the signal handler, call
 * clutter_cairo_texture_create() to begin drawing.  The
 * #StDrawingArea::redraw signal will be emitted by default when the area is
 * resized or the CSS style changes; you can use the
 * st_drawing_area_emit_redraw() as well.
 */

#include "st-drawing-area.h"

#include <cairo.h>

G_DEFINE_TYPE(StDrawingArea, st_drawing_area, ST_TYPE_BIN);

struct _StDrawingAreaPrivate {
  ClutterCairoTexture *texture;
};

/* Signals */
enum
{
  REDRAW,
  LAST_SIGNAL
};

static guint st_drawing_area_signals [LAST_SIGNAL] = { 0 };

static void
st_drawing_area_allocate (ClutterActor          *self,
                          const ClutterActorBox *box,
                          ClutterAllocationFlags flags)
{
  StThemeNode *theme_node;
  ClutterActorBox content_box;
  StDrawingArea *area = ST_DRAWING_AREA (self);
  int width = box->x2 - box->x1;
  int height = box->y2 - box->y1;

  (CLUTTER_ACTOR_CLASS (st_drawing_area_parent_class))->allocate (self, box, flags);

  theme_node = st_widget_get_theme_node (ST_WIDGET (self));

  st_theme_node_get_content_box (theme_node, box, &content_box);

  if (width > 0 && height > 0)
    {
      clutter_cairo_texture_set_surface_size (area->priv->texture,
                                              content_box.x2 - content_box.x1,
                                              content_box.y2 - content_box.y1);
      g_signal_emit (G_OBJECT (self), st_drawing_area_signals[REDRAW], 0,
                     area->priv->texture);
    }
}

static void
st_drawing_area_style_changed (StWidget  *self)
{
  (ST_WIDGET_CLASS (st_drawing_area_parent_class))->style_changed (self);

  st_drawing_area_emit_redraw (ST_DRAWING_AREA (self));
}

static void
st_drawing_area_class_init (StDrawingAreaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  StWidgetClass *widget_class = ST_WIDGET_CLASS (klass);

  actor_class->allocate = st_drawing_area_allocate;
  widget_class->style_changed = st_drawing_area_style_changed;

  st_drawing_area_signals[REDRAW] =
    g_signal_new ("redraw",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (StDrawingAreaClass, redraw),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, CLUTTER_TYPE_CAIRO_TEXTURE);

  g_type_class_add_private (gobject_class, sizeof (StDrawingAreaPrivate));
}

static void
st_drawing_area_init (StDrawingArea *area)
{
  area->priv = G_TYPE_INSTANCE_GET_PRIVATE (area, ST_TYPE_DRAWING_AREA,
                                            StDrawingAreaPrivate);
  area->priv->texture = CLUTTER_CAIRO_TEXTURE (clutter_cairo_texture_new (1, 1));
  clutter_container_add_actor (CLUTTER_CONTAINER (area), CLUTTER_ACTOR (area->priv->texture));
}

/**
 * st_drawing_area_get_texture:
 *
 * Return Value: (transfer none):
 */
ClutterCairoTexture *
st_drawing_area_get_texture (StDrawingArea *area)
{
  return area->priv->texture;
}

/**
 * st_drawing_area_emit_redraw:
 * @area: A #StDrawingArea
 *
 * Immediately emit a redraw signal.  Useful if
 * some parameters for the area being drawn other
 * than the size or style have changed.
 */
void
st_drawing_area_emit_redraw (StDrawingArea *area)
{
  g_signal_emit ((GObject*)area, st_drawing_area_signals[REDRAW], 0, area->priv->texture);
}
