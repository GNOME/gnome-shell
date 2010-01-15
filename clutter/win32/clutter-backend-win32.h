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

#ifndef __CLUTTER_BACKEND_WIN32_H__
#define __CLUTTER_BACKEND_WIN32_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>
#include <clutter/clutter-backend.h>
#include <windows.h>

#include "clutter-win32.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_WIN32                (clutter_backend_win32_get_type ())
#define CLUTTER_BACKEND_WIN32(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND_WIN32, ClutterBackendWin32))
#define CLUTTER_IS_BACKEND_WIN32(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND_WIN32))
#define CLUTTER_BACKEND_WIN32_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND_WIN32, ClutterBackendWin32Class))
#define CLUTTER_IS_BACKEND_WIN32_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND_WIN32))
#define CLUTTER_BACKEND_WIN32_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND_WIN32, ClutterBackendWin32Class))

typedef struct _ClutterBackendWin32       ClutterBackendWin32;
typedef struct _ClutterBackendWin32Class  ClutterBackendWin32Class;

struct _ClutterBackendWin32
{
  ClutterBackend parent_instance;

  HGLRC          gl_context;
  gboolean       no_event_retrieval;

  HCURSOR        invisible_cursor;

  GSource       *event_source;
};

struct _ClutterBackendWin32Class
{
  ClutterBackendClass parent_class;
};

void   _clutter_backend_win32_events_init (ClutterBackend *backend);
void   _clutter_backend_win32_events_uninit (ClutterBackend *backend);

GType clutter_backend_win32_get_type (void) G_GNUC_CONST;

void
clutter_backend_win32_add_options (ClutterBackend *backend,
				   GOptionGroup   *group);

ClutterFeatureFlags
clutter_backend_win32_get_features (ClutterBackend *backend);

HCURSOR _clutter_backend_win32_get_invisible_cursor (ClutterBackend *backend);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_WIN32_H__ */
