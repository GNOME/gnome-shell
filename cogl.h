/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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

#ifndef __COGL_H__
#define __COGL_H__

#include <glib.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

typedef void (*CoglFuncPtr) (void);

CoglFuncPtr
cogl_get_proc_address (const gchar* name);

gboolean 
cogl_check_extension (const gchar *name, const gchar *ext);

void
cogl_paint_init (ClutterColor *color);

void
cogl_push_matrix (void);

void
cogl_pop_matrix (void);

void
cogl_scaled (ClutterFixed x, ClutterFixed z);

void
cogl_translatex (ClutterFixed x, ClutterFixed y, ClutterFixed z);

void
cogl_translate (gint x, gint y, gint z);

void
cogl_rotatex (ClutterFixed angle, gint x, gint y, gint z);

void
cogl_rotate (gint angle, gint x, gint y, gint z);

void
cogl_color (ClutterColor *color);

#if 0

../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glColor3f'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glColor4ub'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glGetTexLevelParameteriv'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glSelectBuffer'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glScaled'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glPushName'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glRecti'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glBegin'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glInitNames'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glVertex2i'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glGetTexImage'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glTexCoord2f'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glRenderMode'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glTranslated'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glRotated'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glLoadName'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glTexEnvi'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glVertex2d'
../clutter/.libs/libclutter-egl-0.3.so: undefined reference to `glEnd'

#endif

G_END_DECLS

#endif /* __COGL_H__ */

