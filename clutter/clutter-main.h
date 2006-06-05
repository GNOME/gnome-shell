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

#ifndef _HAVE_CLUTTER_MAIN_H
#define _HAVE_CLUTTER_MAIN_H

#include <clutter/clutter-element.h>
#include <clutter/clutter-stage.h>

#include <X11/Xlib.h>

#include <GL/glx.h>
#include <GL/gl.h>

#define CLUTTER_HAS_DEBUG_MESSGES 1

#if (CLUTTER_HAS_DEBUG_MESSGES)

#define CLUTTER_DBG(x, a...) \
 if (clutter_want_debug())   \
   { g_printerr ( __FILE__ ":%d,%s() " x "\n", __LINE__, __func__, ##a); }

#define CLUTTER_GLERR()                                        \
 if (clutter_want_debug())                                     \
 {                                                             \
  GLenum err = glGetError (); 	/* Roundtrip */                \
  if (err != GL_NO_ERROR)                                      \
    {                                                          \
      g_printerr (__FILE__ ": GL Error: %x [at %s:%d]\n",      \
		  err, __func__, __LINE__);                    \
    }                                                          \
 }
#else
#define CLUTTER_DBG(x, a...) do {} while (0)
#define CLUTTER_GLERR()      do {} while (0)
#endif /* CLUTTER_HAS_DEBUG */

#define CLUTTER_MARK() CLUTTER_DBG("mark")

int
clutter_init (int *argc, char ***argv);

void
clutter_main (void);

void
clutter_main_quit (void);

gint
clutter_main_level (void);

void
clutter_redraw ();

Display*
clutter_xdisplay (void);

int
clutter_xscreen (void);

Window
clutter_root_xwindow (void);

XVisualInfo*
clutter_xvisual(void);

gboolean
clutter_want_debug (void);

void
clutter_threads_enter (void);

void
clutter_threads_leave (void);


#endif
