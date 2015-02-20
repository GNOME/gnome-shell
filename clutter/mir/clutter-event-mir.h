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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Marco Trevisan <marco.trevisan@canonical.com>
 */

#ifndef __CLUTTER_EVENT_MIR_H__
#define __CLUTTER_EVENT_MIR_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>

#include <mir_toolkit/event.h>

GSource *
_clutter_event_source_mir_new (void);

void
_clutter_mir_handle_event (ClutterBackend *backend, MirSurface *surface, MirEvent *event);

#endif /* __CLUTTER_EVENT_MIR_H__ */
