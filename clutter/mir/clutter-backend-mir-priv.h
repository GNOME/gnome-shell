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

#ifndef __CLUTTER_BACKEND_MIR_PRIV_H__
#define __CLUTTER_BACKEND_MIR_PRIV_H__

#include <glib-object.h>
#include <clutter/clutter-backend.h>
#include <mir_toolkit/mir_client_library.h>

#include "clutter-backend-private.h"

G_BEGIN_DECLS

struct _ClutterBackendMir
{
  ClutterBackend parent_instance;

  MirConnection *mir_connection;
  GSource *mir_source;
};

G_END_DECLS

#endif /* __CLUTTER_BACKEND_MIR_PRIV_H__ */
