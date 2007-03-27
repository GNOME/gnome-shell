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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __CLUTTER_EGL_H__
#define __CLUTTER_EGL_H__

#include <glib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <GLES/gl.h>
#include <GLES/egl.h>

#include <clutter/clutter-stage.h>

G_BEGIN_DECLS

void     clutter_egl_trap_x_errors       (void);
gint     clutter_egl_untrap_x_errors     (void);

Display *clutter_egl_get_default_display (void);
gint     clutter_egl_get_default_screen  (void);
Window   clutter_egl_get_default_root_window (void);

Window       clutter_egl_get_stage_window (ClutterStage *stage);
XVisualInfo *clutter_egl_get_stage_visual (ClutterStage *stage);

void         clutter_egl_set_stage_foreign (ClutterStage *stage,
                                            Window        window);

EGLDisplay
clutter_egl_display (void);

G_END_DECLS

#endif /* __CLUTTER_EGL_H__ */
