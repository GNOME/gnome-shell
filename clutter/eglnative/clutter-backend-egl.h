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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __CLUTTER_BACKEND_EGL_H__
#define __CLUTTER_BACKEND_EGL_H__

#include "clutter-egl-headers.h"

#include <glib-object.h>
#include <clutter/clutter-backend.h>
G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_EGL                (clutter_backend_egl_get_type ())
#define CLUTTER_BACKEND_EGL(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND_EGL, ClutterBackendEGL))
#define CLUTTER_IS_BACKEND_EGL(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND_EGL))
#define CLUTTER_BACKEND_EGL_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND_EGL, ClutterBackendEGLClass))
#define CLUTTER_IS_BACKEND_EGL_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND_EGL))
#define CLUTTER_BACKEND_EGL_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND_EGL, ClutterBackendEGLClass))

typedef struct _ClutterBackendEGL       ClutterBackendEGL;
typedef struct _ClutterBackendEGLClass  ClutterBackendEGLClass;


struct _ClutterBackendEGL
{
  ClutterBackend parent_instance;

  /* EGL Specific */
  EGLDisplay edpy;
  EGLSurface egl_surface;
  EGLContext egl_context;

  gint       egl_version_major;
  gint       egl_version_minor;

  /* main stage singleton */
  ClutterActor *stage;

  /* event source */
  GSource *event_source;

  /* event timer */
  GTimer *event_timer;

  /*< private >*/
};

struct _ClutterBackendEGLClass
{
  ClutterBackendClass parent_class;
};

GType clutter_backend_egl_get_type (void) G_GNUC_CONST;

void _clutter_events_init (ClutterBackend *backend);
void _clutter_events_uninit (ClutterBackend *backend);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_EGL_H__ */
