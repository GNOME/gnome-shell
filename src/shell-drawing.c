/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "shell-drawing.h"
#include <math.h>

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

/**
 * shell_fade_app_icon:
 * @source: Source #ClutterTexture
 *
 * Create a new texture by modifying the alpha channel of the
 * source texture, adding a horizontal gradient fade.
 *
 * Returns: (transfer none): A new #ClutterTexture
 */
ClutterTexture *
shell_fade_app_icon (ClutterTexture *source)
{
  CoglHandle  texture;
  guchar     *pixels;
  gint        width, height, rowstride;
  gint        fade_start;
  gint        fade_range;
  guint       i, j;
  ClutterTexture *result;

  texture = clutter_texture_get_cogl_texture (source);
  if (texture == COGL_INVALID_HANDLE)
    return NULL;

  width  = cogl_texture_get_width  (texture);
  height = cogl_texture_get_height (texture);
  rowstride = (width * 4 + 3) & ~3;

  pixels = g_malloc0 (rowstride * height);

  cogl_texture_get_data (texture, COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                         rowstride, pixels);
  
  fade_start = width / 2;
  fade_range = width - fade_start;
  for (i = fade_start; i < width; i++)
    {
      for (j = 0; j < height; j++)
        {
          guchar *pixel = &pixels[j * rowstride + i * 4];
          float fade = 1.0 - ((float) i - fade_start) / fade_range;
          pixel[0] = 0.5 + pixel[0] * fade;
          pixel[1] = 0.5 + pixel[1] * fade;
          pixel[2] = 0.5 + pixel[2] * fade;
          pixel[3] = 0.5 + pixel[3] * fade;
        }   
    }

  texture = cogl_texture_new_from_data (width,
                                        height,
                                        COGL_TEXTURE_NONE,
                                        COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                        COGL_PIXEL_FORMAT_ANY,
                                        rowstride,
                                        pixels);
  g_free (pixels);
    
  result = (ClutterTexture*)clutter_texture_new ();
  clutter_texture_set_cogl_texture (result, texture);
  cogl_texture_unref (texture);
  
  return result;
}

void
shell_draw_box_pointer (ClutterCairoTexture   *texture,
                        ShellPointerDirection  direction,
                        ClutterColor          *border_color,
                        ClutterColor          *background_color)
{
  guint width, height;
  cairo_t *cr;

  clutter_cairo_texture_get_surface_size (texture, &width, &height);

  clutter_cairo_texture_clear (texture);
  cr = clutter_cairo_texture_create (texture);

  cairo_set_line_width (cr, 1.0);

  clutter_cairo_set_source_color (cr, border_color);

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
