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

#include "backends/meta-remote-desktop-session.h"

#include <linux/input.h>
#include <xkbcommon/xkbcommon.h>

#include "backends/meta-dbus-session-watcher.h"
#include "backends/meta-screen-cast-session.h"
#include "backends/native/meta-backend-native.h"
#include "backends/x11/meta-backend-x11.h"
#include "cogl/cogl.h"
#include "meta/meta-backend.h"
#include "meta/errors.h"
#include "meta-dbus-remote-desktop.h"

#define META_REMOTE_DESKTOP_SESSION_DBUS_PATH "/org/gnome/Mutter/RemoteDesktop/Session"

struct _MetaRemoteDesktopSession
{
  MetaDBusRemoteDesktopSessionSkeleton parent;

  char *session_id;
  char *object_path;

  MetaScreenCastSession *screen_cast_session;
  gulong screen_cast_session_closed_handler_id;

  ClutterVirtualInputDevice *virtual_pointer;
  ClutterVirtualInputDevice *virtual_keyboard;
};

static void
meta_remote_desktop_session_init_iface (MetaDBusRemoteDesktopSessionIface *iface);

static void
meta_dbus_session_init_iface (MetaDbusSessionInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaRemoteDesktopSession,
                         meta_remote_desktop_session,
                         META_DBUS_TYPE_REMOTE_DESKTOP_SESSION_SKELETON,
                         G_IMPLEMENT_INTERFACE (META_DBUS_TYPE_REMOTE_DESKTOP_SESSION,
                                                meta_remote_desktop_session_init_iface)
                         G_IMPLEMENT_INTERFACE (META_TYPE_DBUS_SESSION,
                                                meta_dbus_session_init_iface))

static gboolean
meta_remote_desktop_session_is_running (MetaRemoteDesktopSession *session)
{
  return !!session->virtual_pointer;
}

static gboolean
meta_remote_desktop_session_start (MetaRemoteDesktopSession *session,
                                   GError                  **error)
{
  ClutterDeviceManager *device_manager =
    clutter_device_manager_get_default ();

  g_assert (!session->virtual_pointer && !session->virtual_keyboard);

  if (session->screen_cast_session)
    {
      if (!meta_screen_cast_session_start (session->screen_cast_session, error))
        return FALSE;
    }

  session->virtual_pointer =
    clutter_device_manager_create_virtual_device (device_manager,
                                                  CLUTTER_POINTER_DEVICE);
  session->virtual_keyboard =
    clutter_device_manager_create_virtual_device (device_manager,
                                                  CLUTTER_KEYBOARD_DEVICE);

  return TRUE;
}

void
meta_remote_desktop_session_close (MetaRemoteDesktopSession *session)
{
  MetaDBusRemoteDesktopSession *skeleton =
    META_DBUS_REMOTE_DESKTOP_SESSION (session);

  if (session->screen_cast_session)
    {
      g_signal_handler_disconnect (session->screen_cast_session,
                                   session->screen_cast_session_closed_handler_id);
      meta_screen_cast_session_close (session->screen_cast_session);
      session->screen_cast_session = NULL;
    }

  g_clear_object (&session->virtual_pointer);
  g_clear_object (&session->virtual_keyboard);

  meta_dbus_session_notify_closed (META_DBUS_SESSION (session));
  meta_dbus_remote_desktop_session_emit_closed (skeleton);
  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (session));

  g_object_unref (session);
}

char *
meta_remote_desktop_session_get_object_path (MetaRemoteDesktopSession *session)
{
  return session->object_path;
}

char *
meta_remote_desktop_session_get_session_id (MetaRemoteDesktopSession *session)
{
  return session->session_id;
}

static void
on_screen_cast_session_closed (MetaScreenCastSession    *screen_cast_session,
                               MetaRemoteDesktopSession *session)
{
  session->screen_cast_session = NULL;
  meta_remote_desktop_session_close (session);
}

gboolean
meta_remote_desktop_session_register_screen_cast (MetaRemoteDesktopSession  *session,
                                                  MetaScreenCastSession     *screen_cast_session,
                                                  GError                   **error)
{
  if (session->screen_cast_session)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Remote desktop session already have an associated "
                   "screen cast session");
      return FALSE;
    }

  session->screen_cast_session = screen_cast_session;
  session->screen_cast_session_closed_handler_id =
    g_signal_connect (screen_cast_session, "session-closed",
                      G_CALLBACK (on_screen_cast_session_closed),
                      session);

  return TRUE;
}

MetaRemoteDesktopSession *
meta_remote_desktop_session_new (MetaRemoteDesktop  *remote_desktop,
                                 GError            **error)
{
  GDBusInterfaceSkeleton *interface_skeleton;
  MetaRemoteDesktopSession *session;
  GDBusConnection *connection;

  session = g_object_new (META_TYPE_REMOTE_DESKTOP_SESSION, NULL);

  interface_skeleton = G_DBUS_INTERFACE_SKELETON (session);
  connection = meta_remote_desktop_get_connection (remote_desktop);
  if (!g_dbus_interface_skeleton_export (interface_skeleton,
                                         connection,
                                         session->object_path,
                                         error))
    {
      g_object_unref (session);
      return NULL;
    }

  return session;
}

static gboolean
handle_start (MetaDBusRemoteDesktopSession *skeleton,
              GDBusMethodInvocation        *invocation)
{
  MetaRemoteDesktopSession *session = META_REMOTE_DESKTOP_SESSION (skeleton);
  GError *error = NULL;

  if (!meta_remote_desktop_session_start (session, &error))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "Failed to start remote desktop: %s",
                                             error->message);
      g_error_free (error);

      meta_remote_desktop_session_close (session);

      return TRUE;
    }

  meta_dbus_remote_desktop_session_complete_start (skeleton, invocation);

  return TRUE;
}

static gboolean
handle_stop (MetaDBusRemoteDesktopSession *skeleton,
             GDBusMethodInvocation        *invocation)
{
  MetaRemoteDesktopSession *session = META_REMOTE_DESKTOP_SESSION (skeleton);

  meta_remote_desktop_session_close (session);

  meta_dbus_remote_desktop_session_complete_stop (skeleton, invocation);

  return TRUE;
}

static gboolean
handle_notify_keyboard_keysym (MetaDBusRemoteDesktopSession *skeleton,
                               GDBusMethodInvocation        *invocation,
                               unsigned int                  keysym,
                               gboolean                      pressed)
{
  MetaRemoteDesktopSession *session = META_REMOTE_DESKTOP_SESSION (skeleton);

  ClutterKeyState state;

  if (pressed)
    state = CLUTTER_KEY_STATE_PRESSED;
  else
    state = CLUTTER_KEY_STATE_RELEASED;

  clutter_virtual_input_device_notify_keyval (session->virtual_keyboard,
                                              CLUTTER_CURRENT_TIME,
                                              keysym,
                                              state);

  meta_dbus_remote_desktop_session_complete_notify_keyboard_keysym (skeleton,
                                                                    invocation);
  return TRUE;
}

/* Translation taken from the clutter evdev backend. */
static int
translate_to_clutter_button (int button)
{
  switch (button)
    {
    case BTN_LEFT:
      return CLUTTER_BUTTON_PRIMARY;
    case BTN_RIGHT:
      return CLUTTER_BUTTON_SECONDARY;
    case BTN_MIDDLE:
      return CLUTTER_BUTTON_MIDDLE;
    default:
      /*
       * For compatibility reasons, all additional buttons go after the old
       * 4-7 scroll ones.
       */
      return button - (BTN_LEFT - 1) + 4;
    }
}

static gboolean
handle_notify_pointer_button (MetaDBusRemoteDesktopSession *skeleton,
                              GDBusMethodInvocation        *invocation,
                              int                           button_code,
                              gboolean                      pressed)
{
  MetaRemoteDesktopSession *session = META_REMOTE_DESKTOP_SESSION (skeleton);
  uint32_t button;
  ClutterButtonState state;

  button = translate_to_clutter_button (button_code);

  if (pressed)
    state = CLUTTER_BUTTON_STATE_PRESSED;
  else
    state = CLUTTER_BUTTON_STATE_RELEASED;

  clutter_virtual_input_device_notify_button (session->virtual_pointer,
                                              CLUTTER_CURRENT_TIME,
                                              button,
                                              state);

  meta_dbus_remote_desktop_session_complete_notify_pointer_button (skeleton,
                                                                   invocation);

  return TRUE;
}

static ClutterScrollDirection
discrete_steps_to_scroll_direction (unsigned int axis,
                                    int          steps)
{
  if (axis == 0 && steps < 0)
    return CLUTTER_SCROLL_UP;
  if (axis == 0 && steps > 0)
    return CLUTTER_SCROLL_DOWN;
  if (axis == 1 && steps < 0)
    return CLUTTER_SCROLL_LEFT;
  if (axis == 1 && steps > 0)
    return CLUTTER_SCROLL_RIGHT;

  g_assert_not_reached ();
}

static gboolean
handle_notify_pointer_axis_discrete (MetaDBusRemoteDesktopSession *skeleton,
                                     GDBusMethodInvocation        *invocation,
                                     unsigned int                  axis,
                                     int                           steps)
{
  MetaRemoteDesktopSession *session = META_REMOTE_DESKTOP_SESSION (skeleton);
  ClutterScrollDirection direction;

  if (axis <= 1)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "Invalid axis value");
      return TRUE;
    }

  if (steps == 0)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "Invalid axis steps value");
      return TRUE;
    }

  if (steps != -1 || steps != 1)
    g_warning ("Multiple steps at at once not yet implemented, treating as one.");

  /*
   * We don't have the actual scroll source, but only know they should be
   * considered as discrete steps. The device that produces such scroll events
   * is the scroll wheel, so pretend that is the scroll source.
   */
  direction = discrete_steps_to_scroll_direction (axis, steps);
  clutter_virtual_input_device_notify_discrete_scroll (session->virtual_pointer,
                                                       CLUTTER_CURRENT_TIME,
                                                       direction,
                                                       CLUTTER_SCROLL_SOURCE_WHEEL);

  meta_dbus_remote_desktop_session_complete_notify_pointer_axis_discrete (skeleton,
                                                                          invocation);

  return TRUE;
}

static gboolean
handle_notify_pointer_motion_absolute (MetaDBusRemoteDesktopSession *skeleton,
                                       GDBusMethodInvocation        *invocation,
                                       const char                   *stream_path,
                                       double                        x,
                                       double                        y)
{
  MetaRemoteDesktopSession *session = META_REMOTE_DESKTOP_SESSION (skeleton);

  clutter_virtual_input_device_notify_absolute_motion (session->virtual_pointer,
                                                       CLUTTER_CURRENT_TIME,
                                                       x, y);

  meta_dbus_remote_desktop_session_complete_notify_pointer_motion_absolute (skeleton,
                                                                            invocation);

  return TRUE;
}

static void
meta_remote_desktop_session_init_iface (MetaDBusRemoteDesktopSessionIface *iface)
{
  iface->handle_start = handle_start;
  iface->handle_stop = handle_stop;
  iface->handle_notify_keyboard_keysym = handle_notify_keyboard_keysym;
  iface->handle_notify_pointer_button = handle_notify_pointer_button;
  iface->handle_notify_pointer_axis_discrete = handle_notify_pointer_axis_discrete;
  iface->handle_notify_pointer_motion_absolute = handle_notify_pointer_motion_absolute;
}

static void
meta_remote_desktop_session_client_vanished (MetaDbusSession *dbus_session)
{
  meta_remote_desktop_session_close (META_REMOTE_DESKTOP_SESSION (dbus_session));
}

static void
meta_dbus_session_init_iface (MetaDbusSessionInterface *iface)
{
  iface->client_vanished = meta_remote_desktop_session_client_vanished;
}

static void
meta_remote_desktop_session_finalize (GObject *object)
{
  MetaRemoteDesktopSession *session = META_REMOTE_DESKTOP_SESSION (object);

  g_assert (!meta_remote_desktop_session_is_running (session));

  g_free (session->session_id);
  g_free (session->object_path);

  G_OBJECT_CLASS (meta_remote_desktop_session_parent_class)->finalize (object);
}

static void
meta_remote_desktop_session_init (MetaRemoteDesktopSession *session)
{
  MetaDBusRemoteDesktopSession *skeleton =
    META_DBUS_REMOTE_DESKTOP_SESSION  (session);
  GRand *rand;
  static unsigned int global_session_number = 0;

  rand = g_rand_new ();
  session->session_id = meta_generate_random_id (rand, 32);
  g_rand_free (rand);

  meta_dbus_remote_desktop_session_set_session_id (skeleton, session->session_id);

  session->object_path =
    g_strdup_printf (META_REMOTE_DESKTOP_SESSION_DBUS_PATH "/u%u",
                     ++global_session_number);
}

static void
meta_remote_desktop_session_class_init (MetaRemoteDesktopSessionClass *klass)
{

  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_remote_desktop_session_finalize;
}
