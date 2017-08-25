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

#include "config.h"

#include "backends/meta-screen-cast-session.h"

#include "backends/meta-backend-private.h"
#include "backends/meta-dbus-session-watcher.h"
#include "backends/meta-screen-cast-monitor-stream.h"
#include "backends/meta-screen-cast-stream.h"

#define META_SCREEN_CAST_SESSION_DBUS_PATH "/org/gnome/Mutter/ScreenCast/Session"

struct _MetaScreenCastSession
{
  MetaDBusScreenCastSessionSkeleton parent;

  char *peer_name;

  MetaScreenCastSessionType session_type;
  char *object_path;

  GList *streams;
};

static void
meta_screen_cast_session_init_iface (MetaDBusScreenCastSessionIface *iface);

static void
meta_dbus_session_init_iface (MetaDbusSessionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaScreenCastSession,
                         meta_screen_cast_session,
                         META_DBUS_TYPE_SCREEN_CAST_SESSION_SKELETON,
                         G_IMPLEMENT_INTERFACE (META_DBUS_TYPE_SCREEN_CAST_SESSION,
                                                meta_screen_cast_session_init_iface)
                         G_IMPLEMENT_INTERFACE (META_TYPE_DBUS_SESSION,
                                                meta_dbus_session_init_iface))

gboolean
meta_screen_cast_session_start (MetaScreenCastSession  *session,
                                GError                **error)
{
  GList *l;

  for (l = session->streams; l; l = l->next)
    {
      MetaScreenCastStream *stream = l->data;

      if (!meta_screen_cast_stream_start (stream, error))
        return FALSE;
    }

  return TRUE;
}

void
meta_screen_cast_session_close (MetaScreenCastSession *session)
{
  MetaDBusScreenCastSession *skeleton = META_DBUS_SCREEN_CAST_SESSION (session);

  g_list_free_full (session->streams, g_object_unref);

  meta_dbus_session_notify_closed (META_DBUS_SESSION (session));

  switch (session->session_type)
    {
    case META_SCREEN_CAST_SESSION_TYPE_NORMAL:
      meta_dbus_screen_cast_session_emit_closed (skeleton);
      break;
    case META_SCREEN_CAST_SESSION_TYPE_REMOTE_DESKTOP:
      break;
    }

  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (session));

  g_object_unref (session);
}

char *
meta_screen_cast_session_get_object_path (MetaScreenCastSession *session)
{
  return session->object_path;
}

static gboolean
check_permission (MetaScreenCastSession *session,
                  GDBusMethodInvocation *invocation)
{
  return g_strcmp0 (session->peer_name,
                    g_dbus_method_invocation_get_sender (invocation)) == 0;
}

static gboolean
handle_start (MetaDBusScreenCastSession *skeleton,
              GDBusMethodInvocation     *invocation)
{
  MetaScreenCastSession *session = META_SCREEN_CAST_SESSION (skeleton);
  GError *error = NULL;

  if (!check_permission (session, invocation))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "Permission denied");
      return TRUE;
    }

  switch (session->session_type)
    {
    case META_SCREEN_CAST_SESSION_TYPE_NORMAL:
      break;
    case META_SCREEN_CAST_SESSION_TYPE_REMOTE_DESKTOP:
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "Must be started from remote desktop session");
      return TRUE;
    }

  if (!meta_screen_cast_session_start (session, &error))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "Failed to start screen cast: %s",
                                             error->message);
      g_error_free (error);

      return TRUE;
    }

  meta_dbus_screen_cast_session_complete_start (skeleton, invocation);

  return TRUE;
}

static gboolean
handle_stop (MetaDBusScreenCastSession *skeleton,
             GDBusMethodInvocation     *invocation)
{
  MetaScreenCastSession *session = META_SCREEN_CAST_SESSION (skeleton);

  if (!check_permission (session, invocation))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "Permission denied");
      return TRUE;
    }

  switch (session->session_type)
    {
    case META_SCREEN_CAST_SESSION_TYPE_NORMAL:
      break;
    case META_SCREEN_CAST_SESSION_TYPE_REMOTE_DESKTOP:
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "Must be stopped from remote desktop session");
      return TRUE;
    }

  meta_screen_cast_session_close (session);

  meta_dbus_screen_cast_session_complete_stop (skeleton, invocation);

  return TRUE;
}

static void
on_stream_closed (MetaScreenCastStream  *stream,
                  MetaScreenCastSession *session)
{
  meta_screen_cast_session_close (session);
}

static gboolean
handle_record_monitor (MetaDBusScreenCastSession *skeleton,
                       GDBusMethodInvocation     *invocation,
                       const char                *connector,
                       GVariant                  *properties_variant)
{
  MetaScreenCastSession *session = META_SCREEN_CAST_SESSION (skeleton);
  GDBusInterfaceSkeleton *interface_skeleton;
  GDBusConnection *connection;
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitor *monitor;
  ClutterStage *stage;
  GError *error = NULL;
  MetaScreenCastMonitorStream *monitor_stream;
  MetaScreenCastStream *stream;
  char *stream_path;

  if (!check_permission (session, invocation))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "Permission denied");
      return TRUE;
    }

  interface_skeleton = G_DBUS_INTERFACE_SKELETON (skeleton);
  connection = g_dbus_interface_skeleton_get_connection (interface_skeleton);

  if (g_str_equal (connector, ""))
    monitor = meta_monitor_manager_get_primary_monitor (monitor_manager);
  else
    monitor = meta_monitor_manager_get_monitor_from_connector (monitor_manager,
                                                               connector);

  if (!monitor)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "Unknown monitor");
      return TRUE;
    }

  stage = CLUTTER_STAGE (meta_backend_get_stage (backend));

  monitor_stream = meta_screen_cast_monitor_stream_new (connection,
                                                        monitor_manager,
                                                        monitor,
                                                        stage,
                                                        &error);
  if (!monitor_stream)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "Failed to record monitor: %s",
                                             error->message);
      g_error_free (error);
      return TRUE;
    }

  stream = META_SCREEN_CAST_STREAM (monitor_stream);
  stream_path = meta_screen_cast_stream_get_object_path (stream);

  session->streams = g_list_append (session->streams, stream);

  g_signal_connect (stream, "closed", G_CALLBACK (on_stream_closed), session);

  meta_dbus_screen_cast_session_complete_record_monitor (skeleton,
                                                         invocation,
                                                         stream_path);

  return TRUE;
}

static gboolean
handle_record_window (MetaDBusScreenCastSession *skeleton,
                      GDBusMethodInvocation     *invocation,
                      GVariant                  *properties_variant)
{
  MetaScreenCastSession *session = META_SCREEN_CAST_SESSION (skeleton);

  if (!check_permission (session, invocation))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_ACCESS_DENIED,
                                             "Permission denied");
      return TRUE;
    }

  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "Recording a window not yet supported");
  return TRUE;
}

static void
meta_screen_cast_session_init_iface (MetaDBusScreenCastSessionIface *iface)
{
  iface->handle_start = handle_start;
  iface->handle_stop = handle_stop;
  iface->handle_record_monitor = handle_record_monitor;
  iface->handle_record_window = handle_record_window;
}

static void
meta_screen_cast_session_client_vanished (MetaDbusSession *dbus_session)
{
  meta_screen_cast_session_close (META_SCREEN_CAST_SESSION (dbus_session));
}

static void
meta_dbus_session_init_iface (MetaDbusSessionInterface *iface)
{
  iface->client_vanished = meta_screen_cast_session_client_vanished;
}

MetaScreenCastSession *
meta_screen_cast_session_new (MetaScreenCast             *screen_cast,
                              MetaScreenCastSessionType   session_type,
                              const char                 *peer_name,
                              GError                    **error)
{
  GDBusInterfaceSkeleton *interface_skeleton;
  MetaScreenCastSession *session;
  GDBusConnection *connection;
  static unsigned int global_session_number = 0;

  session = g_object_new (META_TYPE_SCREEN_CAST_SESSION, NULL);
  session->session_type = session_type;
  session->peer_name = g_strdup (peer_name);
  session->object_path =
    g_strdup_printf (META_SCREEN_CAST_SESSION_DBUS_PATH "/u%u",
                     ++global_session_number);

  interface_skeleton = G_DBUS_INTERFACE_SKELETON (session);
  connection = meta_screen_cast_get_connection (screen_cast);
  if (!g_dbus_interface_skeleton_export (interface_skeleton,
                                         connection,
                                         session->object_path,
                                         error))
    return NULL;

  return session;
}

static void
meta_screen_cast_session_finalize (GObject *object)
{
  MetaScreenCastSession *session = META_SCREEN_CAST_SESSION (object);

  g_free (session->peer_name);
  g_free (session->object_path);

  G_OBJECT_CLASS (meta_screen_cast_session_parent_class)->finalize (object);
}

static void
meta_screen_cast_session_init (MetaScreenCastSession *session)
{
}

static void
meta_screen_cast_session_class_init (MetaScreenCastSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_screen_cast_session_finalize;
}
