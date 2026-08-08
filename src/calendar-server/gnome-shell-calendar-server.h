/*
 * Copyright (C) 2011 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 *
 * Based on code from gnome-panel's clock-applet, file calendar-client.c, with Authors:
 *
 *     Mark McLoughlin  <mark@skynet.ie>
 *     William Jon McCann  <mccann@jhu.edu>
 *     Martin Grimme  <martin@pycage.de>
 *     Christian Kellner  <gicmo@xatom.net>
 *
 */

#pragma once

#include <gio/gio.h>

#define CALENDAR_SERVER_TYPE_APP calendar_server_app_get_type ()
G_DECLARE_FINAL_TYPE (CalendarServerApp, calendar_server_app, CALENDAR_SERVER, APP, GApplication)

G_BEGIN_DECLS

char *calendar_server_app_take_startup_notify_id (CalendarServerApp *application);

G_END_DECLS
