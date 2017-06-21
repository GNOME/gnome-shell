/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat Inc.
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

#ifndef META_DBUS_SESSION_WATCHER_H
#define META_DBUS_SESSION_WATCHER_H

#include <glib-object.h>

#define META_TYPE_DBUS_SESSION (meta_dbus_session_get_type ())
G_DECLARE_INTERFACE (MetaDbusSession, meta_dbus_session,
                     META, DBUS_SESSION,
                     GObject)

struct _MetaDbusSessionInterface
{
  GTypeInterface parent_iface;

  void (* client_vanished) (MetaDbusSession *session);
};

#define META_TYPE_DBUS_SESSION_WATCHER (meta_dbus_session_watcher_get_type ())
G_DECLARE_FINAL_TYPE (MetaDbusSessionWatcher,
                      meta_dbus_session_watcher,
                      META, DBUS_SESSION_WATCHER,
                      GObject)

void meta_dbus_session_watcher_watch_session (MetaDbusSessionWatcher *session_watcher,
                                              const char             *client_dbus_name,
                                              MetaDbusSession        *session);

void meta_dbus_session_notify_closed (MetaDbusSession *session);

#endif /* META_DBUS_SESSION_WATCHER_H */
