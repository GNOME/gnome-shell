/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "shell-drawing.h"
#include <math.h>

/**
 * shell_create_vertical_gradient:
 * @top: the color at the top
 * @bottom: the color at the bottom
 *
 * Creates a vertical gradient actor.
 *
 * Return value: (transfer none): a #ClutterCairoTexture actor with the
 *               gradient. The texture actor is floating, hence (transfer none).
 */
ClutterCairoTexture *
shell_create_vertical_gradient (ClutterColor *top,
                                ClutterColor *bottom)
{
  ClutterCairoTexture *texture;
  cairo_t *cr;
  cairo_pattern_t *pattern;

  /* Draw the gradient on an 8x8 pixel texture. Because the gradient is drawn
   * from the uppermost to the lowermost row, after stretching 1/16 of the
   * texture height has the top color and 1/16 has the bottom color. The 8
   * pixel width is chosen for reasons related to graphics hardware internals.
   */
  texture = CLUTTER_CAIRO_TEXTURE (clutter_cairo_texture_new (8, 8));
  cr = clutter_cairo_texture_create (texture);

  pattern = cairo_pattern_create_linear (0, 0, 0, 8);
  cairo_pattern_add_color_stop_rgba (pattern, 0,
                                     top->red / 255.,
                                     top->green / 255.,
                                     top->blue / 255.,
                                     top->alpha / 255.);
  cairo_pattern_add_color_stop_rgba (pattern, 1,
                                     bottom->red / 255.,
                                     bottom->green / 255.,
                                     bottom->blue / 255.,
                                     bottom->alpha / 255.);

  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  cairo_pattern_destroy (pattern);
  cairo_destroy (cr);

  return texture;
}

/**
 * shell_create_horizontal_gradient:
 * @left: the color on the left
 * @right: the color on the right
 *
 * Creates a horizontal gradient actor.
 *
 * Return value: (transfer none): a #ClutterCairoTexture actor with the
 *               gradient. The texture actor is floating, hence (transfer none).
 */
ClutterCairoTexture *
shell_create_horizontal_gradient (ClutterColor *left,
                                  ClutterColor *right)
{
  ClutterCairoTexture *texture;
  cairo_t *cr;
  cairo_pattern_t *pattern;

  /* Draw the gradient on an 8x1 pixel texture. Because the gradient is drawn
   * from the left to the right column, after stretching 1/16 of the
   * texture width has the left side color and 1/16 has the right side color.
   * There is no reason to use the 8 pixel height that would be similar to the
   * reason we are using the 8 pixel width for the vertical gradient, so we
   * are just using the 1 pixel height instead.
   */
  texture = CLUTTER_CAIRO_TEXTURE (clutter_cairo_texture_new (8, 1));
  cr = clutter_cairo_texture_create (texture);

  pattern = cairo_pattern_create_linear (0, 0, 8, 0);
  cairo_pattern_add_color_stop_rgba (pattern, 0,
                                     left->red / 255.,
                                     left->green / 255.,
                                     left->blue / 255.,
                                     left->alpha / 255.);
  cairo_pattern_add_color_stop_rgba (pattern, 1,
                                     right->red / 255.,
                                     right->green / 255.,
                                     right->blue / 255.,
                                     right->alpha / 255.);

  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  cairo_pattern_destroy (pattern);
  cairo_destroy (cr);

  return texture;
}

void
shell_draw_clock (ClutterCairoTexture *texture,
                  int                  hour,
                  int                  minute)
{
  cairo_t *cr;
  guint width, height;
  double xc, yc, radius, hour_radius, minute_radius;
  double angle;

  clutter_cairo_texture_get_surface_size (texture, &width, &height);
  xc = (double)width / 2;
  yc = (double)height / 2;
  radius = (double)(MIN(width, height)) / 2 - 2;
  minute_radius = radius - 3;
  hour_radius = radius / 2;

  clutter_cairo_texture_clear (texture);
  cr = clutter_cairo_texture_create (texture);
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

  cairo_destroy (cr);
}

void
shell_draw_box_pointer (ClutterCairoTexture *texture,
                        ClutterColor        *border_color,
                        ClutterColor        *background_color)
{
  guint width, height;
  cairo_t *cr;

  clutter_cairo_texture_get_surface_size (texture, &width, &height);

  clutter_cairo_texture_clear (texture);
  cr = clutter_cairo_texture_create (texture);

  cairo_set_line_width (cr, 1.0);

  clutter_cairo_set_source_color (cr, border_color);

  cairo_move_to (cr, width, 0);
  cairo_line_to (cr, 0, floor (height * 0.5));
  cairo_line_to (cr, width, height);

  cairo_stroke_preserve (cr);

  clutter_cairo_set_source_color (cr, background_color);

  cairo_fill (cr);

  cairo_destroy (cr);
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
