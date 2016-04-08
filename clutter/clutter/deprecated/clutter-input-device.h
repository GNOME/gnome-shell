/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2009, 2010, 2011  Intel Corp.
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
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */
#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_INPUT_DEVICE_DEPRECATED_H__
#define __CLUTTER_INPUT_DEVICE_DEPRECATED_H__

#include <clutter/clutter-input-device.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_input_device_get_coords)
void clutter_input_device_get_device_coords  (ClutterInputDevice  *device,
                                              gint                *x,
                                              gint                *y);

G_END_DECLS

#endif /* __CLUTTER_INPUT_DEVICE_DEPRECATED_H__ */
