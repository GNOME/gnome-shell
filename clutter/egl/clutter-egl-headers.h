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

#include <cogl/cogl.h>

#ifdef COGL_HAS_GLES2
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#else /* COGL_HAS_GLES2 */
#include <GLES/gl.h>
#include <GLES/egl.h>
#endif /* COGL_HAS_GLES2 */

#ifdef COGL_HAS_GLES2
#define NativeDisplayType EGLNativeDisplayType
#define NativeWindowType EGLNativeWindowType
#endif /* COGL_HAS_GLES2 */

#endif /* __CLUTTER_EGL_HEADERS_H__ */
