/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2006, 2007 OpenedHand
 * Copyright (C) 2010 Intel Corp
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
 * Authors:
 *  Matthew Allum
 *  Robert Bragg
 */

#ifndef __CLUTTER_BACKEND_COGL_H__
#define __CLUTTER_BACKEND_COGL_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>
#include <clutter/clutter-backend.h>
#include <clutter/clutter-device-manager.h>

#ifdef COGL_HAS_XLIB_SUPPORT
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

#include "clutter-backend-private.h"

#ifdef COGL_HAS_X11_SUPPORT
#include "../x11/clutter-backend-x11.h"
#endif

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_COGL                (_clutter_backend_cogl_get_type ())
#define CLUTTER_BACKEND_COGL(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND_COGL, ClutterBackendCogl))
#define CLUTTER_IS_BACKEND_COGL(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND_COGL))
#define CLUTTER_BACKEND_COGL_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND_COGL, ClutterBackendCoglClass))
#define CLUTTER_IS_BACKEND_COGL_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND_COGL))
#define CLUTTER_BACKEND_COGL_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND_COGL, ClutterBackendCoglClass))

typedef struct _ClutterBackendCogl       ClutterBackendCogl;
typedef struct _ClutterBackendCoglClass  ClutterBackendCoglClass;

struct _ClutterBackendCogl
{
#ifdef COGL_HAS_XLIB_SUPPORT
  ClutterBackendX11 parent_instance;

#else /* COGL_HAS_X11_SUPPORT */
  ClutterBackend parent_instance;

  /* main stage singleton */
  ClutterStageWindow *stage;

  /* device manager (ie evdev) */
  ClutterDeviceManager *device_manager;

  /* event source */
  GSource *event_source;

  /* event timer */
  GTimer *event_timer;

#endif /* COGL_HAS_X11_SUPPORT */

  CoglContext *cogl_context;

  gboolean can_blit_sub_buffer;
};

struct _ClutterBackendCoglClass
{
#ifdef COGL_HAS_XLIB_SUPPORT
  ClutterBackendX11Class parent_class;
#else
  ClutterBackendClass parent_class;
#endif
};

GType _clutter_backend_cogl_get_type (void) G_GNUC_CONST;

#ifdef HAVE_TSLIB
void _clutter_events_tslib_init   (ClutterBackendCogl *backend);
void _clutter_events_tslib_uninit (ClutterBackendCogl *backend);
#endif

const gchar *_clutter_backend_cogl_get_vblank (void);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_COGL_H__ */
