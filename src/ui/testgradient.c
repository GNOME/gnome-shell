/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter gradient test program */

/* 
 * Copyright (C) 2002 Havoc Pennington
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.  */

#include <meta/gradient.h>
#include <gtk/gtk.h>

typedef void (* RenderGradientFunc) (cairo_t     *cr,
                                     int          width,
                                     int          height);

static void
draw_checkerboard (cairo_t *cr,
                   int      width,
                   int      height)
{
  gint i, j, xcount, ycount;
  GdkRGBA color1, color2;
  
#define CHECK_SIZE 10
#define SPACING 2  

  color1.red = 30000. / 65535.;
  color1.green = 30000. / 65535.;
  color1.blue = 30000. / 65535.;
  color1.alpha = 1.0;

  color2.red = 50000. / 65535.;
  color2.green = 50000. / 65535.;
  color2.blue = 50000. / 65535.;
  color2.alpha = 1.0;

  xcount = 0;
  i = SPACING;
  while (i < width)
    {
      j = SPACING;
      ycount = xcount % 2; /* start with even/odd depending on row */
      while (j < height)
	{
	  if (ycount % 2)
	    gdk_cairo_set_source_rgba (cr, &color1);
	  else
	    gdk_cairo_set_source_rgba (cr, &color2);

	  /* If we're outside event->area, this will do nothing.
	   * It might be mildly more efficient if we handled
	   * the clipping ourselves, but again we're feeling lazy.
	   */
          cairo_rectangle (cr, i, j, CHECK_SIZE, CHECK_SIZE);
          cairo_fill (cr);

	  j += CHECK_SIZE + SPACING;
	  ++ycount;
	}

      i += CHECK_SIZE + SPACING;
      ++xcount;
    }
}

static void
render_simple (cairo_t     *cr,
               int width, int height,
               MetaGradientType type,
               gboolean    with_alpha)
{
  GdkPixbuf *pixbuf;
  GdkRGBA from, to;
  
  gdk_rgba_parse (&from, "blue");
  gdk_rgba_parse (&to, "green");

  pixbuf = meta_gradient_create_simple (width, height,
                                        &from, &to,
                                        type);

  if (with_alpha)
    {
      const unsigned char alphas[] = { 0xff, 0xaa, 0x2f, 0x0, 0xcc, 0xff, 0xff };
      
      if (!gdk_pixbuf_get_has_alpha (pixbuf))
        {
          GdkPixbuf *new_pixbuf;
          
          new_pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
          g_object_unref (G_OBJECT (pixbuf));
          pixbuf = new_pixbuf;
        }
      
      meta_gradient_add_alpha (pixbuf,
                               alphas, G_N_ELEMENTS (alphas),
                               META_GRADIENT_HORIZONTAL);
      
      draw_checkerboard (cr , width, height);
    }
    
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);

  g_object_unref (G_OBJECT (pixbuf));
}

static void
render_vertical_func (cairo_t *cr,
                      int width, int height)
{
  render_simple (cr, width, height, META_GRADIENT_VERTICAL, FALSE);
}

static void
render_horizontal_func (cairo_t *cr,
                        int width, int height)
{
  render_simple (cr, width, height, META_GRADIENT_HORIZONTAL, FALSE);
}

static void
render_diagonal_func (cairo_t *cr,
                      int width, int height)
{
  render_simple (cr, width, height, META_GRADIENT_DIAGONAL, FALSE);
}

static void
render_diagonal_alpha_func (cairo_t *cr,
                            int width, int height)
{
  render_simple (cr, width, height, META_GRADIENT_DIAGONAL, TRUE);
}

static void
render_multi (cairo_t     *cr,
              int width, int height,
              MetaGradientType type)
{
  GdkPixbuf *pixbuf;
#define N_COLORS 5
  GdkRGBA colors[N_COLORS];

  gdk_rgba_parse (&colors[0], "red");
  gdk_rgba_parse (&colors[1], "blue");
  gdk_rgba_parse (&colors[2], "orange");
  gdk_rgba_parse (&colors[3], "pink");
  gdk_rgba_parse (&colors[4], "green");

  pixbuf = meta_gradient_create_multi (width, height,
                                       colors, N_COLORS,
                                       type);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);

  g_object_unref (G_OBJECT (pixbuf));
#undef N_COLORS
}

static void
render_vertical_multi_func (cairo_t *cr,
                            int width, int height)
{
  render_multi (cr, width, height, META_GRADIENT_VERTICAL);
}

static void
render_horizontal_multi_func (cairo_t *cr,
                              int width, int height)
{
  render_multi (cr, width, height, META_GRADIENT_HORIZONTAL);
}

static void
render_diagonal_multi_func (cairo_t *cr,
                            int width, int height)
{
  render_multi (cr, width, height, META_GRADIENT_DIAGONAL);
}

static void
render_interwoven_func (cairo_t *cr,
                        int width, int height)
{
  GdkPixbuf *pixbuf;
#define N_COLORS 4
  GdkRGBA colors[N_COLORS];

  gdk_rgba_parse (&colors[0], "red");
  gdk_rgba_parse (&colors[1], "blue");
  gdk_rgba_parse (&colors[2], "pink");
  gdk_rgba_parse (&colors[3], "green");

  pixbuf = meta_gradient_create_interwoven (width, height,
                                            colors, height / 10,
                                            colors + 2, height / 14);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);

  g_object_unref (G_OBJECT (pixbuf));
}

static gboolean
draw_callback (GtkWidget *widget,
               cairo_t   *cr,
               gpointer   data)
{
  RenderGradientFunc func = data;
  GtkStyleContext *style;
  GdkRGBA color;

  style = gtk_widget_get_style_context (widget);

  gtk_style_context_save (style);
  gtk_style_context_set_state (style, gtk_widget_get_state_flags (widget));
  gtk_style_context_lookup_color (style, "foreground-color", &color);
  gtk_style_context_restore (style);

  gdk_cairo_set_source_rgba (cr, &color);

  (* func) (cr,
            gtk_widget_get_allocated_width (widget),
            gtk_widget_get_allocated_height (widget));

  return FALSE;
}

static GtkWidget*
create_gradient_window (const char *title,
                        RenderGradientFunc func)
{
  GtkWidget *window;
  GtkWidget *drawing_area;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title (GTK_WINDOW (window), title);
  
  drawing_area = gtk_drawing_area_new ();

  gtk_widget_set_size_request (drawing_area, 1, 1);

  gtk_window_set_default_size (GTK_WINDOW (window), 175, 175);
  
  g_signal_connect (G_OBJECT (drawing_area),
                    "draw",
                    G_CALLBACK (draw_callback),
                    func);

  gtk_container_add (GTK_CONTAINER (window), drawing_area);

  gtk_widget_show_all (window);
  
  return window;
}

static void
meta_gradient_test (void)
{
  create_gradient_window ("Simple vertical",
                          render_vertical_func);

  create_gradient_window ("Simple horizontal",
                          render_horizontal_func);

  create_gradient_window ("Simple diagonal",
                          render_diagonal_func);

  create_gradient_window ("Multi vertical",
                          render_vertical_multi_func);

  create_gradient_window ("Multi horizontal",
                          render_horizontal_multi_func);

  create_gradient_window ("Multi diagonal",
                          render_diagonal_multi_func);

  create_gradient_window ("Interwoven",
                          render_interwoven_func);

  create_gradient_window ("Simple diagonal with horizontal multi alpha",
                          render_diagonal_alpha_func);

}

int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  meta_gradient_test ();

  gtk_main ();

  return 0;
}

