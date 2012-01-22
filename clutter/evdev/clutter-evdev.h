/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012  Intel Corp.
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

#ifndef __CLUTTER_EVDEV_H__
#define __CLUTTER_EVDEV_H__

#include <glib.h>
#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

void  clutter_evdev_release_devices (void);
void  clutter_evdev_reclaim_devices (void);

G_END_DECLS

#endif /* __CLUTTER_EVDEV_H__ */
