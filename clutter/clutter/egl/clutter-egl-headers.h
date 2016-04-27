/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
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

#ifndef __CLUTTER_EGL_HEADERS_H__
#define __CLUTTER_EGL_HEADERS_H__

/* Clutter relies on Cogl to abstract GLES1/2/OpenGL and GLX/EGL etc
 * so we simply include the Cogl header to pull in the appropriate EGL
 * header. */

#include <cogl/cogl.h>

/* Since Cogl 1.11.2, the EGL headers are no longer included from
 * cogl.h when the experimental 2.0 API is requested. Clutter requests
 * this in its configure script so we need to switch the header we
 * include in that case. COGL_VERSION_CHECK is also new in 1.11.2 */
#ifdef COGL_VERSION_CHECK
#if COGL_VERSION_CHECK (1, 11, 2)
#include <cogl/cogl-egl.h>
#endif
#endif

#endif /* __CLUTTER_EGL_HEADERS_H__ */
