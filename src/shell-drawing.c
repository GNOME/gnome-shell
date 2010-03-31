/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "shell-drawing.h"
#include <math.h>

void
shell_draw_clock (StDrawingArea       *area,
                  int                  hour,
                  int                  minute)
{
  cairo_t *cr;
  guint width, height;
  double xc, yc, radius, hour_radius, minute_radius;
  double angle;

  st_drawing_area_get_surface_size (area, &width, &height);
  xc = (double)width / 2;
  yc = (double)height / 2;
  radius = (double)(MIN(width, height)) / 2 - 2;
  minute_radius = radius - 3;
  hour_radius = radius / 2;

  cr = st_drawing_area_get_context (area);
  cairo_set_line_width (cr, 1.0);

  /* Outline */
  cairo_arc (cr, xc, yc, radius, 0.0, 2.0 * M_PI);
  cairo_stroke (cr);

  /* Hour hand. (We add a fraction to @hour for the minutes, then
   * convert to radians, and then subtract pi/2 because cairo's origin
   * is at 3:00, not 12:00.)
   */
  angle = ((hour + minute / 60.0) / 12.0) * 2.0 * M_PI - M_PI / 2.0;
  cairo_move_to (cr, xc, yc);
  cairo_line_to (cr,
                 xc + hour_radius * cos (angle),
                 yc + hour_radius * sin (angle));
  cairo_stroke (cr);

  /* Minute hand */
  angle = (minute / 60.0) * 2.0 * M_PI - M_PI / 2.0;
  cairo_move_to (cr, xc, yc);
  cairo_line_to (cr,
                 xc + minute_radius * cos (angle),
                 yc + minute_radius * sin (angle));
  cairo_stroke (cr);
}

void
shell_draw_box_pointer (StDrawingArea         *area,
                        ShellPointerDirection  direction)
{
  StThemeNode *theme_node;
  ClutterColor border_color, body_color;
  guint width, height;
  cairo_t *cr;

  theme_node = st_widget_get_theme_node (ST_WIDGET (area));
  st_theme_node_get_border_color (theme_node, (StSide)direction, &border_color);
  st_theme_node_get_foreground_color (theme_node, &body_color);

  st_drawing_area_get_surface_size (area, &width, &height);

  cr = st_drawing_area_get_context (area);

  cairo_set_line_width (cr, 1.0);

  clutter_cairo_set_source_color (cr, &border_color);

  switch (direction)
    {
    case SHELL_POINTER_UP:
      cairo_move_to (cr, 0, height);
      cairo_line_to (cr, floor (width * 0.5), 0);
      cairo_line_to (cr, width, height);
      break;

    case SHELL_POINTER_DOWN:
      cairo_move_to (cr, width, 0);
      cairo_line_to (cr, floor (width * 0.5), height);
      cairo_line_to (cr, 0, 0);
      break;

    case SHELL_POINTER_LEFT:
      cairo_move_to (cr, width, height);
      cairo_line_to (cr, 0, floor (height * 0.5));
      cairo_line_to (cr, width, 0);
      break;

    case SHELL_POINTER_RIGHT:
      cairo_move_to (cr, 0, 0);
      cairo_line_to (cr, width, floor (height * 0.5));
      cairo_line_to (cr, 0, height);
      break;

    default:
      g_assert_not_reached();
    }

  cairo_stroke_preserve (cr);

  clutter_cairo_set_source_color (cr, &body_color);

  cairo_fill (cr);
}

static void
hook_paint_red_border (ClutterActor  *actor,
                       gpointer       user_data)
{
  CoglColor color;
  ClutterGeometry geom;
  float width = 2;

  cogl_color_set_from_4ub (&color, 0xff, 0, 0, 0xc4);
  cogl_set_source_color (&color);

  clutter_actor_get_allocation_geometry (actor, &geom);

  /** clockwise order **/
  cogl_rectangle (0, 0, geom.width, width);
  cogl_rectangle (geom.width - width, width,
                  geom.width, geom.height);
  cogl_rectangle (0, geom.height,
                  geom.width - width, geom.height - width);
  cogl_rectangle (0, geom.height - width,
                  width, width);
}

guint
shell_add_hook_paint_red_border (ClutterActor *actor)
{
  return g_signal_connect_after (G_OBJECT (actor), "paint",
                                 G_CALLBACK (hook_paint_red_border), NULL);
}
