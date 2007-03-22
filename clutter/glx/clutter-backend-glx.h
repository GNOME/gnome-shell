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

#ifndef __CLUTTER_BACKEND_GLX_H__
#define __CLUTTER_BACKEND_GLX_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>
#include <clutter/clutter-backend.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <GL/glx.h>
#include <GL/gl.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_GLX                (clutter_backend_glx_get_type ())
#define CLUTTER_BACKEND_GLX(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND_GLX, ClutterBackendGlx))
#define CLUTTER_IS_BACKEND_GLX(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND_GLX))
#define CLUTTER_BACKEND_GLX_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND_GLX, ClutterBackendGlxClass))
#define CLUTTER_IS_BACKEND_GLX_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND_GLX))
#define CLUTTER_BACKEND_GLX_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND_GLX, ClutterBackendGlxClass))

typedef struct _ClutterBackendGlx       ClutterBackendGlx;
typedef struct _ClutterBackendGlxClass  ClutterBackendGlxClass;

struct _ClutterBackendGlx
{
  ClutterBackend parent_instance;

  Display *xdpy;
  Window xwin_root;
  int xscreen;
  gchar *display_name;

  /* main stage singleton */
  ClutterActor *stage;

  /* event source */
  GSource *event_source;
};

struct _ClutterBackendGlxClass
{
  ClutterBackendClass parent_class;
};

GType clutter_backend_glx_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_BACKEND_GLX_H__ */
