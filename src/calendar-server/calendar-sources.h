/*
 * Copyright (C) 2004 Free Software Foundation, Inc.
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
 * Authors:
 *     Mark McLoughlin  <mark@skynet.ie>
 *     William Jon McCann  <mccann@jhu.edu>
 *     Martin Grimme  <martin@pycage.de>
 *     Christian Kellner  <gicmo@xatom.net>
 */

#ifndef __CALENDAR_SOURCES_H__
#define __CALENDAR_SOURCES_H__

#include <glib-object.h>

#define EDS_DISABLE_DEPRECATED
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#include <libedataserver/libedataserver.h>
#include <libecal/libecal.h>
G_GNUC_END_IGNORE_DEPRECATIONS

G_BEGIN_DECLS

#define CALENDAR_TYPE_SOURCES (calendar_sources_get_type ())
G_DECLARE_FINAL_TYPE (CalendarSources, calendar_sources,
                      CALENDAR, SOURCES, GObject)

CalendarSources *calendar_sources_get                (void);
ESourceRegistry *calendar_sources_get_registry       (CalendarSources *sources);
GSList          *calendar_sources_ref_clients        (CalendarSources *sources);
gboolean         calendar_sources_has_clients        (CalendarSources *sources);

void             calendar_sources_connect_client     (CalendarSources *sources,
                                                      ESource *source,
                                                      ECalClientSourceType source_type,
                                                      guint32 wait_for_connected_seconds,
                                                      GCancellable *cancellable,
                                                      GAsyncReadyCallback callback,
                                                      gpointer user_data);
EClient         *calendar_sources_connect_client_finish
                                                     (CalendarSources *sources,
                                                      GAsyncResult *result,
                                                      GError **error);

/* Set the environment variable CALENDAR_SERVER_DEBUG to show debug */
void            print_debug                          (const gchar *str,
                                                      ...) G_GNUC_PRINTF (1, 2);

G_END_DECLS

#endif /* __CALENDAR_SOURCES_H__ */
