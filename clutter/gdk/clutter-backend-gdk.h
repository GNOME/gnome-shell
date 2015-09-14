/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
 *               2011 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#ifndef __CLUTTER_BACKEND_GDK_H__
#define __CLUTTER_BACKEND_GDK_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>
#include <gdk/gdk.h>

#include "clutter-gdk.h"

#include "clutter-backend-private.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_GDK                (_clutter_backend_gdk_get_type ())
#define CLUTTER_BACKEND_GDK(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND_GDK, ClutterBackendGdk))
#define CLUTTER_IS_BACKEND_GDK(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND_GDK))
#define CLUTTER_BACKEND_GDK_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND_GDK, ClutterBackendGdkClass))
#define CLUTTER_IS_BACKEND_GDK_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND_GDK))
#define CLUTTER_BACKEND_GDK_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND_GDK, ClutterBackendGdkClass))

typedef struct _ClutterBackendGdk       ClutterBackendGdk;
typedef struct _ClutterBackendGdkClass  ClutterBackendGdkClass;

struct _ClutterBackendGdk
{
  ClutterBackend parent_instance;

  GdkDisplay *display;
  GdkScreen  *screen;

  ClutterDeviceManager *device_manager;
};

struct _ClutterBackendGdkClass
{
  ClutterBackendClass parent_class;

  /* nothing here, for now */
};

GType _clutter_backend_gdk_get_type (void) G_GNUC_CONST;

void   _clutter_backend_gdk_events_init (ClutterBackend *backend);

void   _clutter_backend_gdk_update_setting (ClutterBackendGdk *backend,
                                            const gchar *name);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_GDK_H__ */
