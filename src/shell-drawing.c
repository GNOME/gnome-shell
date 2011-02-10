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
