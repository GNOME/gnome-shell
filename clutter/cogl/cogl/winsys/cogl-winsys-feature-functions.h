/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *
 *
 */

/* This can be included multiple times with different definitions for
   the COGL_WINSYS_FEATURE_* functions */

#ifdef COGL_HAS_GLX_SUPPORT

COGL_WINSYS_FEATURE_BEGIN (texture_from_pixmap,
                           "EXT\0",
                           "texture_from_pixmap\0",
                           COGL_WINSYS_FEATURE_TEXTURE_FROM_PIXMAP,
                           0)
COGL_WINSYS_FEATURE_FUNCTION (void, glXBindTexImage,
                              (Display     *display,
                               GLXDrawable  drawable,
                               int          buffer,
                               int         *attribList))
COGL_WINSYS_FEATURE_FUNCTION (void, glXReleaseTexImage,
                              (Display     *display,
                               GLXDrawable  drawable,
                               int          buffer))
COGL_WINSYS_FEATURE_END ()

#endif
