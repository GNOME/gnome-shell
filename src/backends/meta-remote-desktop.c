/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015-2017 Red Hat Inc.
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

#define _GNU_SOURCE

#include "config.h"

#include "backends/meta-remote-desktop.h"

#include <errno.h>
#include <glib.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "meta-dbus-remote-desktop.h"
#include "backends/meta-backend-private.h"
#include "backends/meta-cursor-renderer.h"
#include "backends/meta-remote-desktop-session.h"
#include "backends/native/meta-cursor-renderer-native.h"
#include "meta/errors.h"
#include "meta/meta-backend.h"

#define META_REMOTE_DESKTOP_DBUS_SERVICE "org.gnome.Mutter.RemoteDesktop"
#define META_REMOTE_DESKTOP_DBUS_PATH "/org/gnome/Mutter/RemoteDesktop"

struct _MetaRemoteDesktop
{
  MetaDBusRemoteDesktopSkeleton parent;

  int dbus_name_id;

  GHashTable *sessions;

  MetaDbusSessionWatcher *session_watcher;
};

static void
meta_remote_desktop_init_iface (MetaDBusRemoteDesktopIface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaRemoteDesktop,
                         meta_remote_desktop,
                         META_DBUS_TYPE_REMOTE_DESKTOP_SKELETON,
                         G_IMPLEMENT_INTERFACE (META_DBUS_TYPE_REMOTE_DESKTOP,
                                                meta_remote_desktop_init_iface));

GDBusConnection *
meta_remote_desktop_get_connection (MetaRemoteDesktop *remote_desktop)
{
  GDBusInterfaceSkeleton *interface_skeleton =
    G_DBUS_INTERFACE_SKELETON (remote_desktop);

  return g_dbus_interface_skeleton_get_connection (interface_skeleton);
}

MetaRemoteDesktopSession *
meta_remote_desktop_get_session (MetaRemoteDesktop *remote_desktop,
                                 const char        *session_id)
{
  return g_hash_table_lookup (remote_desktop->sessions, session_id);
}

static void
on_session_closed (MetaRemoteDesktopSession *session,
                   MetaRemoteDesktop        *remote_desktop)
{
  char *session_id;

  session_id = meta_remote_desktop_session_get_session_id (session);
  g_hash_table_remove (remote_desktop->sessions, session_id);
}

static gboolean
handle_create_session (MetaDBusRemoteDesktop *skeleton,
                       GDBusMethodInvocation *invocation)
{
  MetaRemoteDesktop *remote_desktop = META_REMOTE_DESKTOP (skeleton);
  const char *peer_name;
  MetaRemoteDesktopSession *session;
  GError *error = NULL;
  char *session_id;
  char *session_path;
  const char *client_dbus_name;

  peer_name = g_dbus_method_invocation_get_sender (invocation);
  session = meta_remote_desktop_session_new (remote_desktop,
                                             peer_name,
                                             &error);
  if (!session)
    {
      g_warning ("Failed to create remote desktop session: %s",
                 error->message);

      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "Failed to create session: %s",
                                             error->message);
      g_error_free (error);

      return TRUE;
    }

  session_id = meta_remote_desktop_session_get_session_id (session);
  g_hash_table_insert (remote_desktop->sessions,
                       session_id,
                       session);

  client_dbus_name = g_dbus_method_invocation_get_sender (invocation);
  meta_dbus_session_watcher_watch_session (remote_desktop->session_watcher,
                                           client_dbus_name,
                                           META_DBUS_SESSION (session));

  session_path = meta_remote_desktop_session_get_object_path (session);
  meta_dbus_remote_desktop_complete_create_session (skeleton,
                                                    invocation,
                                                    session_path);

  g_signal_connect (session, "session-closed",
                    G_CALLBACK (on_session_closed),
                    remote_desktop);

  return TRUE;
}

static void
meta_remote_desktop_init_iface (MetaDBusRemoteDesktopIface *iface)
{
  iface->handle_create_session = handle_create_session;
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const char      *name,
                 gpointer         user_data)
{
  MetaRemoteDesktop *remote_desktop = user_data;
  GDBusInterfaceSkeleton *interface_skeleton =
    G_DBUS_INTERFACE_SKELETON (remote_desktop);
  GError *error = NULL;

  if (!g_dbus_interface_skeleton_export (interface_skeleton,
                                         connection,
                                         META_REMOTE_DESKTOP_DBUS_PATH,
                                         &error))
    g_warning ("Failed to export remote desktop object: %s\n", error->message);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
  g_info ("Acquired name %s\n", name);
}

static void
on_name_lost (GDBusConnection *connection,
              const char      *name,
              gpointer         user_data)
{
  g_warning ("Lost or failed to acquire name %s\n", name);
}

static void
meta_remote_desktop_constructed (GObject *object)
{
  MetaRemoteDesktop *remote_desktop = META_REMOTE_DESKTOP (object);

  remote_desktop->dbus_name_id =
    g_bus_own_name (G_BUS_TYPE_SESSION,
                    META_REMOTE_DESKTOP_DBUS_SERVICE,
                    G_BUS_NAME_OWNER_FLAGS_NONE,
                    on_bus_acquired,
                    on_name_acquired,
                    on_name_lost,
                    remote_desktop,
                    NULL);
}

static void
meta_remote_desktop_finalize (GObject *object)
{
  MetaRemoteDesktop *remote_desktop = META_REMOTE_DESKTOP (object);
  GList *sessions;

  if (remote_desktop->dbus_name_id != 0)
    g_bus_unown_name (remote_desktop->dbus_name_id);

  sessions = g_list_copy (g_hash_table_get_values (remote_desktop->sessions));
  g_list_free_full (sessions,
                    (GDestroyNotify) meta_remote_desktop_session_close);
  g_hash_table_destroy (remote_desktop->sessions);

  G_OBJECT_CLASS (meta_remote_desktop_parent_class)->finalize (object);
}

MetaRemoteDesktop *
meta_remote_desktop_new (MetaDbusSessionWatcher *session_watcher)
{
  MetaRemoteDesktop *remote_desktop;

  remote_desktop = g_object_new (META_TYPE_REMOTE_DESKTOP, NULL);
  remote_desktop->session_watcher = session_watcher;

  return remote_desktop;
}

static void
meta_remote_desktop_init (MetaRemoteDesktop *remote_desktop)
{
  remote_desktop->sessions = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
meta_remote_desktop_class_init (MetaRemoteDesktopClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = meta_remote_desktop_constructed;
  object_class->finalize = meta_remote_desktop_finalize;
}
