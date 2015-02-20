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

/**
 * SECTION:clutter-mir
 * @short_description: Mir specific API
 *
 * The Mir backend for Clutter provides some specific API, allowing
 * integration with the Mir client API for acessing the underlying data
 * structures
 *
 * The Clutter Mir API is available since Clutter 1.22
 */

#ifndef __CLUTTER_MIR_H__
#define __CLUTTER_MIR_H__

#include <glib.h>
#include <mir_toolkit/mir_client_library.h>
#include <clutter/clutter.h>
G_BEGIN_DECLS

CLUTTER_AVAILABLE_IN_1_22
MirSurface *clutter_mir_stage_get_mir_surface (ClutterStage *stage);

CLUTTER_AVAILABLE_IN_1_22
void clutter_mir_stage_set_mir_surface (ClutterStage *stage, MirSurface *surface);

CLUTTER_AVAILABLE_IN_1_22
void clutter_mir_set_connection (MirConnection *connection);

CLUTTER_AVAILABLE_IN_1_22
void clutter_mir_disable_event_retrieval (void);

G_END_DECLS
#endif /* __CLUTTER_MIR_H__ */
