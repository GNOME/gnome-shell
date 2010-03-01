/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

#ifndef __CLUTTER_FRUITY_H__
#define __CLUTTER_FRUITY_H__

#include <glib.h>

#if HAVE_CLUTTER_FRUITY 
/* extra include needed for the GLES header for arm-apple-darwin */
#include <CoreSurface/CoreSurface.h>
#endif

#include <GLES/gl.h>
#include <GLES/egl.h>

#include <clutter/clutter-stage.h>

G_BEGIN_DECLS

EGLDisplay clutter_egl_display (void);
void       clutter_uikit_main  (void);

G_END_DECLS

#endif /* __CLUTTER_EGL_H__ */
