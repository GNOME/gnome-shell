/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* na-tray-child.h
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003-2006 Vincent Untz
 * Copyright (C) 2008 Red Hat, Inc.
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
 */

#ifndef __NA_TRAY_CHILD_H__
#define __NA_TRAY_CHILD_H__

#include "na-xembed.h"

#include <meta/meta-x11-errors.h>

G_BEGIN_DECLS

#define NA_TYPE_TRAY_CHILD (na_tray_child_get_type ())
G_DECLARE_FINAL_TYPE (NaTrayChild, na_tray_child, NA, TRAY_CHILD, NaXembed)

NaTrayChild    *na_tray_child_new            (MetaX11Display *x11_display,
                                              Window          icon_window);
char           *na_tray_child_get_title      (NaTrayChild  *child);
void            na_tray_child_get_wm_class   (NaTrayChild  *child,
					      char        **res_name,
					      char        **res_class);

pid_t na_tray_child_get_pid (NaTrayChild *child);

void na_tray_child_emulate_event (NaTrayChild *tray_child,
				  ClutterEvent *event);

G_END_DECLS

#endif /* __NA_TRAY_CHILD_H__ */
