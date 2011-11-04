/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2006, 2007 OpenedHand
 * Copyright (C) 2010 Intel Corp
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
 * Authors:
 *  Matthew Allum
 *  Robert Bragg
 */

#ifndef __CLUTTER_BACKEND_CEX100_H__
#define __CLUTTER_BACKEND_CEX100_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>
#include <clutter/clutter-backend.h>
#include <clutter/clutter-device-manager.h>

#include "clutter-backend-private.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_CEX100                (_clutter_backend_cex100_get_type ())
#define CLUTTER_BACKEND_CEX100(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND_CEX100, ClutterBackendCex100))
#define CLUTTER_IS_BACKEND_CEX100(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND_CEX100))
#define CLUTTER_BACKEND_CEX100_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND_CEX100, ClutterBackendCex100Class))
#define CLUTTER_IS_BACKEND_CEX100_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND_CEX100))
#define CLUTTER_BACKEND_CEX100_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND_CEX100, ClutterBackendCex100Class))

typedef struct _ClutterBackendCex100       ClutterBackendCex100;
typedef struct _ClutterBackendCex100Class  ClutterBackendCex100Class;

struct _ClutterBackendCex100
{
  ClutterBackend parent_instance;

  /* device manager (ie evdev) */
  ClutterDeviceManager *device_manager;

  /* event source */
  GSource *event_source;

  /* event timer */
  GTimer *event_timer;
};

struct _ClutterBackendCex100Class
{
  ClutterBackendClass parent_class;
};

GType _clutter_backend_cex100_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_BACKEND_CEX100_H__ */
