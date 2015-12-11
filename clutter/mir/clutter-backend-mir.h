/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2014 Canonical Ltd.
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
 *  Marco Trevisan <marco.trevisan@canonical.com>
 */

#ifndef __CLUTTER_BACKEND_MIR_H__
#define __CLUTTER_BACKEND_MIR_H__

#include <glib-object.h>
#include <clutter/clutter-backend.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BACKEND_MIR                (clutter_backend_mir_get_type ())
#define CLUTTER_BACKEND_MIR(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BACKEND_MIR, ClutterBackendMir))
#define CLUTTER_IS_BACKEND_MIR(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BACKEND_MIR))
#define CLUTTER_BACKEND_MIR_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BACKEND_MIR, ClutterBackendMirClass))
#define CLUTTER_IS_BACKEND_MIR_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BACKEND_MIR))
#define CLUTTER_BACKEND_MIR_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BACKEND_MIR, ClutterBackendMirClass))

typedef struct _ClutterBackendMir       ClutterBackendMir;
typedef struct _ClutterBackendMirClass  ClutterBackendMirClass;

struct _ClutterBackendMirClass
{
  ClutterBackendClass parent_class;
};

GType clutter_backend_mir_get_type (void) G_GNUC_CONST;

ClutterBackend *clutter_backend_mir_new (void);

void _clutter_events_mir_init (ClutterBackend *backend);

G_END_DECLS

#endif /* __CLUTTER_BACKEND_MIR_H__ */
