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

#include "cogl.h"
#include <GLES/gl.h>

CoglFuncPtr
cogl_get_proc_address (const gchar* name)
{
  return NULL;
}

gboolean 
cogl_check_extension (const gchar *name, const gchar *ext)
{
  return FALSE;
}

void
cogl_paint_init (ClutterColor *color)
{
  glClearColorx ((color->red << 16) / 0xff, 
		 (color->green << 16) / 0xff,
		 (color->blue << 16) / 0xff, 
		 0xff);

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glDisable (GL_LIGHTING); 
  glDisable (GL_DEPTH_TEST);
}

/* FIXME: inline most of these  */
void
cogl_push_matrix (void)
{
  glPushMatrix();
}

void
cogl_pop_matrix (void)
{
  glPopMatrix();
}

void
cogl_scaled (ClutterFixed x, ClutterFixed y)
{
  glScalex (x, y, CFX_ONE); 
}

void
cogl_translatex (ClutterFixed x, ClutterFixed y, ClutterFixed z)
{
  glTranslatex (x, y, z);
}

void
cogl_translate (gint x, gint y, gint z)
{
  glTranslatex (CLUTTER_INT_TO_FIXED(x), 
		CLUTTER_INT_TO_FIXED(y), 
		CLUTTER_INT_TO_FIXED(z));
}

void
cogl_rotatex (ClutterFixed angle, 
	      ClutterFixed x, 
	      ClutterFixed y, 
	      ClutterFixed z)
{
  glRotatex (angle,x,y,z);
}

void
cogl_rotate (gint angle, gint x, gint y, gint z)
{
  glRotatef (CLUTTER_INT_TO_FIXED(angle),
	     CLUTTER_INT_TO_FIXED(x), 
	     CLUTTER_INT_TO_FIXED(y), 
	     CLUTTER_INT_TO_FIXED(z));
}
