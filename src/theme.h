/* Metacity Theme Rendering */

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

#ifndef META_THEME_H
#define META_THEME_H

#include "frames.h"
#include "gradient.h"

/* theme.[hc] is basically responsible for drawing parts of the UI using
 * theme data
 */


GdkPixbuf* meta_theme_get_gradient (MetaGradientType  type,
                                    const GdkColor   *color_one,
                                    const GdkColor   *color_two,
                                    int               width,
                                    int               height);

#endif
