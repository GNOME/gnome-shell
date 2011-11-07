/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2006, 2007 OpenedHand
 * Copyright (C) 2008, 2009, 2010, 2011  Intel Corporation
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

#ifndef __CLUTTER_EVENT_TSLIB_H__
#define __CLUTTER_EVENT_TSLIB_H__

#include <clutter/clutter-backend.h>

G_BEGIN_DECLS

void _clutter_events_tslib_init   (ClutterBackend *backend);
void _clutter_events_tslib_uninit (ClutterBackend *backend);

G_END_DECLS

#endif /* __CLUTTER_EVENT_TSLIB_H__ */
