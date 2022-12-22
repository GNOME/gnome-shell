/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* na-tray-manager.h
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003-2006 Vincent Untz
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Used to be: eggtraymanager.h
 */

#ifndef __NA_TRAY_MANAGER_H__
#define __NA_TRAY_MANAGER_H__

#include <clutter/clutter.h>

#include "na-tray-child.h"

G_BEGIN_DECLS

#define NA_TYPE_TRAY_MANAGER (na_tray_manager_get_type ())
G_DECLARE_FINAL_TYPE (NaTrayManager, na_tray_manager, NA, TRAY_MANAGER, GObject)

NaTrayManager *na_tray_manager_new (MetaX11Display *x11_display);

gboolean na_tray_manager_manage (NaTrayManager *manager);

void na_tray_manager_set_colors (NaTrayManager *manager,
                                 ClutterColor  *fg,
                                 ClutterColor  *error,
                                 ClutterColor  *warning,
                                 ClutterColor  *success);

G_END_DECLS

#endif /* __NA_TRAY_MANAGER_H__ */
