/* Metacity misc. public entry points */

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

#include "api.h"
#include "display.h"
#include "colors.h"

PangoContext*
meta_get_pango_context (Screen                     *xscreen,
                        const PangoFontDescription *desc)
{
  MetaScreen *screen;

  screen = meta_screen_for_x_screen (xscreen);

  g_return_val_if_fail (screen != NULL, NULL);

  return meta_screen_get_pango_context (screen,
                                        desc,
                                        /* FIXME, from the frame window */
                                        PANGO_DIRECTION_LTR);
}

gulong
meta_get_x_pixel (Screen *xscreen, const PangoColor *color)
{
  MetaScreen *screen;

  screen = meta_screen_for_x_screen (xscreen);

  g_return_val_if_fail (screen != NULL, 0);
  
  return meta_screen_get_x_pixel (screen, color);
}

