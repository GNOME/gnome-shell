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

G_END_DECLS

#endif /* __COGL_H__ */
