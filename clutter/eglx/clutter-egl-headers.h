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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __CLUTTER_EGL_HEADERS_H__
#define __CLUTTER_EGL_HEADERS_H__

#ifdef HAVE_COGL_GLES2
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#else /* HAVE_COGL_GLES2 */
#include <GLES/gl.h>
#include <GLES/egl.h>
#endif /* HAVE_COGL_GLES2 */

#ifdef HAVE_COGL_GLES2
#define NativeDisplayType EGLNativeDisplayType
#define NativeWindowType EGLNativeWindowType
#endif /* HAVE_COGL_GLES2 */

#endif /* __CLUTTER_EGL_HEADERS_H__ */
