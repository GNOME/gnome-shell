/*
 *
 * Copyright © 2001 Ximian, Inc.
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2017 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */

#ifndef CLUTTER_XKB_A11Y_X11_H
#define CLUTTER_XKB_A11Y_X11_H

#include "clutter-device-manager-private.h"
#include "clutter-backend-x11.h"

#include <X11/Xlib.h>

void
clutter_device_manager_x11_apply_kbd_a11y_settings (ClutterDeviceManager   *device_manager,
                                                    ClutterKbdA11ySettings *kbd_a11y_settings);

gboolean
clutter_device_manager_x11_a11y_init               (ClutterDeviceManager *device_manager);

#endif /* CLUTTER_XKB_A11Y_X11_H */
