/* Metacity gradient rendering */

/* 
 * Copyright (C) 2001 Havoc Pennington, 99% copied from wrlib in
 * WindowMaker, Copyright (C) 1997-2000 Dan Pascu and Alfredo Kojima
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
 * 02111-1307, USA.  */

#ifndef META_GRADIENT_H
#define META_GRADIENT_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkcolor.h>

typedef enum
{
  META_GRADIENT_VERTICAL,
  META_GRADIENT_HORIZONTAL,
  META_GRADIENT_DIAGONAL
} MetaGradientType;

typedef struct _MetaGradientDescription MetaGradientDescription;

/* this doesn't support interwoven at the moment, since
 * I don't know what interwoven is good for
 */
struct _MetaGradientDescription
{
  MetaGradientType type;
  GdkColor *colors;
  int       n_colors;
};

MetaGradientDescription* meta_gradient_description_new    (MetaGradientType               type,
                                                           const GdkColor                *colors,
                                                           int                            n_colors);
void                     meta_gradient_description_free   (MetaGradientDescription       *desc);
GdkPixbuf*               meta_gradient_description_render (const MetaGradientDescription *desc,
                                                           int                            width,
                                                           int                            height);


GdkPixbuf* meta_gradient_create_simple     (int               width,
                                            int               height,
                                            const GdkColor   *from,
                                            const GdkColor   *to,
                                            MetaGradientType  style);
GdkPixbuf* meta_gradient_create_multi      (int               width,
                                            int               height,
                                            const GdkColor   *colors,
                                            int               n_colors,
                                            MetaGradientType  style);
GdkPixbuf* meta_gradient_create_interwoven (int               width,
                                            int               height,
                                            const GdkColor    colors1[2],
                                            int               thickness1,
                                            const GdkColor    colors2[2],
                                            int               thickness2);

#endif
