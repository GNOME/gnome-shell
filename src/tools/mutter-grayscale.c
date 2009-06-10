/* Hack for grayscaling an image */

/* 
 * Copyright (C) 2002 Red Hat Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)

static GdkPixbuf*
grayscale_pixbuf (GdkPixbuf *pixbuf)
{
  GdkPixbuf *gray;
  guchar *pixels;
  int rowstride;
  int pixstride;
  int row;
  int n_rows;
  int width;
  
  gray = gdk_pixbuf_copy (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (gray);
  pixstride = gdk_pixbuf_get_has_alpha (gray) ? 4 : 3;
  
  pixels = gdk_pixbuf_get_pixels (gray);
  n_rows = gdk_pixbuf_get_height (gray);
  width = gdk_pixbuf_get_width (gray);
  
  row = 0;
  while (row < n_rows)
    {
      guchar *p = pixels + row * rowstride;
      guchar *end = p + (pixstride * width);

      while (p != end)
        {
          double v = INTENSITY (p[0], p[1], p[2]);

          p[0] = (guchar) v;
          p[1] = (guchar) v;
          p[2] = (guchar) v;
          
          p += pixstride;
        }
      
      ++row;
    }
  
  return gray;
}

int
main (int argc, char **argv)
{
  GdkPixbuf *pixbuf;
  GdkPixbuf *gray;
  GError *err;
  
  if (argc != 2)
    {
      g_printerr ("specify a single image on the command line\n");
      return 1;
    }

  g_type_init ();
  
  err = NULL;
  pixbuf = gdk_pixbuf_new_from_file (argv[1], &err);
  if (err != NULL)
    {
      g_printerr ("failed to load image: %s\n", err->message);
      g_error_free (err);
      return 1;
    }

  gray = grayscale_pixbuf (pixbuf);
  
  err = NULL;
  gdk_pixbuf_save (gray, "grayscale.png", "png", &err, NULL);
  if (err != NULL)
    {
      g_printerr ("failed to save image: %s\n", err->message);
      g_error_free (err);
      return 1;
    }

  g_print ("wrote grayscale.png\n");
  
  return 0;
}
