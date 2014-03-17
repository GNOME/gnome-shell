/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_CAIRO_H__
#define __CLUTTER_CAIRO_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

/**
 * CLUTTER_CAIRO_FORMAT_ARGB32:
 *
 * The #CoglPixelFormat to be used when uploading image data from
 * and to a Cairo image surface using %CAIRO_FORMAT_ARGB32 and
 * %CAIRO_FORMAT_RGB24 as #cairo_format_t.
 *
 * Since: 1.8
 */

/* Cairo stores the data in native byte order as ARGB but Cogl's pixel
 * formats specify the actual byte order. Therefore we need to use a
 * different format depending on the architecture
 */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define CLUTTER_CAIRO_FORMAT_ARGB32     (COGL_PIXEL_FORMAT_BGRA_8888_PRE)
#else
#define CLUTTER_CAIRO_FORMAT_ARGB32     (COGL_PIXEL_FORMAT_ARGB_8888_PRE)
#endif

CLUTTER_AVAILABLE_IN_1_12
void    clutter_cairo_clear             (cairo_t               *cr);
CLUTTER_AVAILABLE_IN_1_0
void    clutter_cairo_set_source_color  (cairo_t               *cr,
                                         const ClutterColor    *color);

G_END_DECLS

#endif /* __CLUTTER_CAIRO_H__ */
