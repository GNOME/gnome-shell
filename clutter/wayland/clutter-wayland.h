/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

 * Authors:
 *  Matthew Allum
 *  Robert Bragg
 *  Kristian Høgsberg
 */

/**
 * SECTION:clutter-wayland
 * @short_description: Wayland specific API
 *
 * The Wayland backend for Clutter provides some Wayland specific API
 *
 * You need to include
 * <filename class="headerfile">&lt;clutter/egl/clutter-wayland.h&gt;</filename>
 * to have access to the functions documented here.
 */

#ifndef __CLUTTER_WAYLAND_H__
#define __CLUTTER_WAYLAND_H__

#include <glib.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

G_BEGIN_DECLS

/**
 * clutter_egl_display:
 *
 * Retrieves the <structname>EGLDisplay</structname> used by Clutter
 *
 * Return value: the EGL display
 */
EGLDisplay
clutter_egl_display (void);

G_END_DECLS

#endif /* __CLUTTER_WAYLAND_H__ */
