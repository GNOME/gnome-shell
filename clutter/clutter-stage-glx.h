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

#ifndef _HAVE_CLUTTER_STAGE_GLX_H
#define _HAVE_CLUTTER_STAGE_GLX_H

G_BEGIN_DECLS

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <GL/glx.h>
#include <GL/gl.h>

#include <clutter/clutter-stage.h>

void
clutter_stage_backend_init_vtable (ClutterStageVTable *vtable) G_GNUC_INTERNAL;

ClutterStageBackend*
clutter_stage_backend_init (ClutterStage *stage) G_GNUC_INTERNAL;

Window
clutter_stage_glx_window (ClutterStage *stage);

gboolean
clutter_stage_glx_set_window_foreign (ClutterStage *stage,
				      Window        xid);

const XVisualInfo*
clutter_stage_glx_get_xvisual (ClutterStage *stage);


G_END_DECLS

#endif
