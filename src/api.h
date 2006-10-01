/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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

#ifndef META_API_H
#define META_API_H

/* don't add any internal headers here; api.h is an installed/public
 * header. Only theme.h is also installed.
 */
#include <X11/Xlib.h>
#include <pango/pangox.h>

/* Colors/state stuff matches GTK since we get the info from
 * the GTK UI slave
 */
typedef struct _MetaUIColors MetaUIColors;

typedef enum
{
  META_STATE_NORMAL,
  META_STATE_ACTIVE,
  META_STATE_PRELIGHT,
  META_STATE_SELECTED,
  META_STATE_INSENSITIVE
} MetaUIState;

struct _MetaUIColors
{
  PangoColor fg[5];
  PangoColor bg[5];
  PangoColor light[5];
  PangoColor dark[5];
  PangoColor mid[5];
  PangoColor text[5];
  PangoColor base[5];
  PangoColor text_aa[5];
};

PangoContext*       meta_get_pango_context (Screen                     *xscreen,
                                            const PangoFontDescription *desc);
gulong              meta_get_x_pixel       (Screen                     *xscreen,
                                            const PangoColor           *color);

#endif
