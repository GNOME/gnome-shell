/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

 * Authors:
 *  Damien Lespiau <damien.lespiau@intel.com>
 */

#ifndef __CLUTTER_XKB_UTILS_H__
#define __CLUTTER_XKB_UTILS_H__

#include <xkbcommon/xkbcommon.h>

#include "clutter-stage.h"
#include "clutter-event.h"
#include "clutter-input-device.h"

ClutterEvent *    _clutter_key_event_new_from_evdev (ClutterInputDevice *device,
						     ClutterInputDevice *core_keyboard,
                                                     ClutterStage       *stage,
                                                     struct xkb_state   *xkb_state,
						     uint32_t            button_state,
                                                     uint32_t            _time,
                                                     uint32_t            key,
                                                     uint32_t            state);
struct xkb_state * _clutter_xkb_state_new           (const gchar *model,
                                                     const gchar *layout,
                                                     const gchar *variant,
                                                     const gchar *options);

#endif /* __CLUTTER_XKB_UTILS_H__ */
