/* Metacity RGB color stuff */

/* 
 * Copyright (C) 2001 Havoc Pennington
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

#include "colors.h"

gulong
meta_screen_get_x_pixel (MetaScreen       *screen,
                         const PangoColor *color)
{
#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)
  double r, g, b;

  r = color->red / (double) 0xffff;
  g = color->green / (double) 0xffff;
  b = color->blue / (double) 0xffff;    

  /* Now this is a low-bloat GdkRGB replacement! */
  if (INTENSITY (r, g, b) > 0.5)
    return WhitePixel (screen->display->xdisplay, screen->number);
  else
    return BlackPixel (screen->display->xdisplay, screen->number);
  
#undef INTENSITY
}
