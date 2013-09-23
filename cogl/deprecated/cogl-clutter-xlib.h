/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__COGL_XLIB_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl-xlib.h> can be included directly."
#endif

#ifndef __COGL_CLUTTER_XLIB_H__
#define __COGL_CLUTTER_XLIB_H__

#include <X11/Xutil.h>

COGL_BEGIN_DECLS

#define cogl_clutter_winsys_xlib_get_visual_info cogl_clutter_winsys_xlib_get_visual_info_CLUTTER
XVisualInfo *
cogl_clutter_winsys_xlib_get_visual_info (void);

COGL_END_DECLS

#endif /* __COGL_CLUTTER_XLIB_H__ */
