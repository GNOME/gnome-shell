/* Metacity X screen handler */

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

#ifndef META_SCREEN_H
#define META_SCREEN_H

#include "display.h"
#include "theme.h"

struct _MetaScreen
{
  MetaDisplay *display;
  int number;
  char *screen_name;
  Screen *xscreen;
  Window xroot;

  MetaThemeEngine *engine;

  MetaUISlave *uislave;
  
  /*< private >*/

  /* we only need one since we only draw to a single visual (that of
   * root window)
   */
  PangoContext *pango_context;
};

MetaScreen*   meta_screen_new                (MetaDisplay                *display,
                                              int                         number);
void          meta_screen_free               (MetaScreen                 *screen);
void          meta_screen_manage_all_windows (MetaScreen                 *screen);
PangoContext* meta_screen_get_pango_context  (MetaScreen                 *screen,
                                              const PangoFontDescription *desc,
                                              PangoDirection              direction);

MetaScreen*   meta_screen_for_x_screen       (Screen                     *xscreen);

#endif

