/*
 * Copyright (C) 2013 Red Hat, Inc.
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
 */

#include "config.h"

#include "meta-launcher.h"

#include <gio/gunixfdlist.h>

#include <clutter/clutter.h>
#include <clutter/evdev/clutter-evdev.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <systemd/sd-login.h>
#include <gudev/gudev.h>

#include "dbus-utils.h"
#include "meta-dbus-login1.h"

#include "backends/meta-backend-private.h"
#include "backends/native/meta-backend-native.h"
#include "meta-cursor-renderer-native.h"
#include "meta-renderer-native.h"

struct _MetaLauncher
{
  Login1Session *session_proxy;
  Login1Seat *seat_proxy;
  char *seat_id;

  GHashTable *sysfs_fds;
  gboolean session_active;
};

const char *
meta_launcher_get_seat_id (MetaLauncher *launcher)
{
  return launcher->seat_id;
}

static gboolean
find_systemd_session (gchar **session_id,
                      GError **error)
{
  const gchar * const graphical_session_types[] = { "wayland", "x11", "mir", NULL };
  const gchar * const active_states[] = { "active", "online", NULL };
  g_autofree gchar *class = NULL;
  g_autofree gchar *local_session_id = NULL;
  g_autofree gchar *type = NULL;
  g_autofree gchar *state = NULL;
  g_auto (GStrv) sessions = NULL;
  int n_sessions;
  int saved_errno;

  g_assert (session_id != NULL);
  g_assert (error == NULL || *error == NULL);

  /* if we are in a logind session, we can trust that value, so use it. This
   * happens for example when you run mutter directly from a VT but when
   * systemd starts us we will not be in a logind session. */
  saved_errno = sd_pid_get_session (0, &local_session_id);
  if (saved_errno < 0)
    {
      if (saved_errno != -ENODATA)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_FOUND,
                       "Failed to get session by pid for user %d (%s)",
                       getuid (),
                       g_strerror (-saved_errno));
          return FALSE;
        }
    }
  else
    {
      *session_id = g_steal_pointer (&local_session_id);
      return TRUE;
    }

  saved_errno = sd_uid_get_display (getuid (), &local_session_id);
  if (saved_errno < 0)
    {
      /* no session, maybe there's a greeter session */
      if (saved_errno == -ENODATA)
        {
          n_sessions = sd_uid_get_sessions (getuid (), 1, &sessions);
          if (n_sessions < 0)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_FOUND,
                           "Failed to get all sessions for user %d (%m)",
                           getuid ());
              return FALSE;
            }

        if (n_sessions == 0)
          {
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_NOT_FOUND,
                         "User %d has no sessions",
                         getuid ());
            return FALSE;
          }

        for (int i = 0; i < n_sessions; ++i)
          {
            saved_errno = sd_session_get_class (sessions[i], &class);
            if (saved_errno < 0)
              {
                g_warning ("Couldn't get class for session '%d': %s",
                           i,
                           g_strerror (-saved_errno));
                continue;
              }

            if (g_strcmp0 (class, "greeter") == 0)
              {
                local_session_id = g_strdup (sessions[i]);
                break;
              }
          }

        if (!local_session_id)
          {
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_NOT_FOUND,
                         "Couldn't find a session or a greeter session for user %d",
                         getuid ());
            return FALSE;
          }
        }
      else
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_FOUND,
                       "Couldn't get display for user %d: %s",
                       getuid (),
                       g_strerror (-saved_errno));
          return FALSE;
        }
    }

  /* sd_uid_get_display will return any session if there is no graphical
   * one, so let's check it really is graphical. */
  saved_errno = sd_session_get_type (local_session_id, &type);
  if (saved_errno < 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Couldn't get type for session '%s': %s",
                   local_session_id,
                   g_strerror (-saved_errno));
      return FALSE;
    }

  if (!g_strv_contains (graphical_session_types, type))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Session '%s' is not a graphical session (type: '%s')",
                   local_session_id,
                   type);
      return FALSE;
    }

    /* and display sessions can be 'closing' if they are logged out but
     * some processes are lingering; we shouldn't consider these */
    saved_errno = sd_session_get_state (local_session_id, &state);
    if (saved_errno < 0)
      {
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_NOT_FOUND,
                     "Couldn't get state for session '%s': %s",
                     local_session_id,
                     g_strerror (-saved_errno));
        return FALSE;
      }

    if (!g_strv_contains (active_states, state))
      {
         g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_NOT_FOUND,
                         "Session '%s' is not active",
                         local_session_id);
         return FALSE;
      }

  *session_id = g_steal_pointer (&local_session_id);

  return TRUE;
}

static Login1Session *
get_session_proxy (GCancellable *cancellable,
                   GError      **error)
{
  g_autofree char *proxy_path = NULL;
  g_autofree char *session_id = NULL;
  g_autoptr (GError) local_error = NULL;
  Login1Session *session_proxy;

  if (!find_systemd_session (&session_id, &local_error))
    {
      g_propagate_prefixed_error (error, local_error, "Could not get session ID: ");
      return NULL;
    }

  proxy_path = get_escaped_dbus_path ("/org/freedesktop/login1/session", session_id);

  session_proxy = login1_session_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                         "org.freedesktop.login1",
                                                         proxy_path,
                                                         cancellable, error);
  if (!session_proxy)
    g_prefix_error(error, "Could not get session proxy: ");

  return session_proxy;
}

static Login1Seat *
get_seat_proxy (GCancellable *cancellable,
                GError      **error)
{
  Login1Seat *seat = login1_seat_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                         "org.freedesktop.login1",
                                                         "/org/freedesktop/login1/seat/self",
                                                         cancellable, error);
  if (!seat)
    g_prefix_error(error, "Could not get seat proxy: ");

  return seat;
}

static gboolean
take_device (Login1Session *session_proxy,
             int            dev_major,
             int            dev_minor,
             int           *out_fd,
             GCancellable  *cancellable,
             GError       **error)
{
  g_autoptr (GVariant) fd_variant = NULL;
  g_autoptr (GUnixFDList) fd_list = NULL;
  int fd = -1;

  if (!login1_session_call_take_device_sync (session_proxy,
                                             dev_major,
                                             dev_minor,
                                             NULL,
                                             &fd_variant,
                                             NULL, /* paused */
                                             &fd_list,
                                             cancellable,
                                             error))
    return FALSE;

  fd = g_unix_fd_list_get (fd_list, g_variant_get_handle (fd_variant), error);
  if (fd == -1)
    return FALSE;

  *out_fd = fd;
  return TRUE;
}

static gboolean
get_device_info_from_path (const char *path,
                           int        *out_major,
                           int        *out_minor)
{
  int r;
  struct stat st;

  r = stat (path, &st);
  if (r < 0 || !S_ISCHR (st.st_mode))
    return FALSE;

  *out_major = major (st.st_rdev);
  *out_minor = minor (st.st_rdev);
  return TRUE;
}

static gboolean
get_device_info_from_fd (int  fd,
                         int *out_major,
                         int *out_minor)
{
  int r;
  struct stat st;

  r = fstat (fd, &st);
  if (r < 0 || !S_ISCHR (st.st_mode))
    return FALSE;

  *out_major = major (st.st_rdev);
  *out_minor = minor (st.st_rdev);
  return TRUE;
}

int
meta_launcher_open_restricted (MetaLauncher *launcher,
                               const char   *path,
                               GError      **error)
{
  int fd;
  int major, minor;

  if (!get_device_info_from_path (path, &major, &minor))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Could not get device info for path %s: %m", path);
      return -1;
    }

  if (!take_device (launcher->session_proxy, major, minor, &fd, NULL, error))
    return -1;

  return fd;
}

void
meta_launcher_close_restricted (MetaLauncher *launcher,
                                int           fd)
{
  int major, minor;
  GError *error = NULL;

  if (!get_device_info_from_fd (fd, &major, &minor))
    {
      g_warning ("Could not get device info for fd %d: %m", fd);
      goto out;
    }

  if (!login1_session_call_release_device_sync (launcher->session_proxy,
                                                major, minor,
                                                NULL, &error))
    {
      g_warning ("Could not release device (%d,%d): %s",
                 major, minor, error->message);
    }

out:
  close (fd);
}

static int
on_evdev_device_open (const char  *path,
                      int          flags,
                      gpointer     user_data,
                      GError     **error)
{
  MetaLauncher *self = user_data;

  /* Allow readonly access to sysfs */
  if (g_str_has_prefix (path, "/sys/"))
    {
      int fd;

      do
        {
          fd = open (path, flags);
        }
      while (fd < 0 && errno == EINTR);

      if (fd < 0)
        {
          g_set_error (error,
                       G_FILE_ERROR,
                       g_file_error_from_errno (errno),
                       "Could not open /sys file: %s: %m", path);
          return -1;
        }

      g_hash_table_add (self->sysfs_fds, GINT_TO_POINTER (fd));
      return fd;
    }

  return meta_launcher_open_restricted (self, path, error);
}

static void
on_evdev_device_close (int      fd,
                       gpointer user_data)
{
  MetaLauncher *self = user_data;

  if (g_hash_table_lookup (self->sysfs_fds, GINT_TO_POINTER (fd)))
    {
      /* /sys/ paths just need close() here */
      g_hash_table_remove (self->sysfs_fds, GINT_TO_POINTER (fd));
      close (fd);
      return;
    }

  meta_launcher_close_restricted (self, fd);
}

static void
sync_active (MetaLauncher *self)
{
  MetaBackend *backend = meta_get_backend ();
  MetaBackendNative *backend_native = META_BACKEND_NATIVE (backend);
  gboolean active = login1_session_get_active (LOGIN1_SESSION (self->session_proxy));

  if (active == self->session_active)
    return;

  self->session_active = active;

  if (active)
    meta_backend_native_resume (backend_native);
  else
    meta_backend_native_pause (backend_native);
}

static void
on_active_changed (Login1Session *session,
                   GParamSpec    *pspec,
                   gpointer       user_data)
{
  MetaLauncher *self = user_data;
  sync_active (self);
}

static gchar *
get_seat_id (GError **error)
{
  g_autoptr (GError) local_error = NULL;
  g_autofree char *session_id = NULL;
  char *seat_id = NULL;
  int r;

  if (!find_systemd_session (&session_id, &local_error))
    {
      g_propagate_prefixed_error (error, local_error, "Could not get session ID: ");
      return NULL;
    }

  r = sd_session_get_seat (session_id, &seat_id);
  if (r < 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Could not get seat for session: %s", g_strerror (-r));
      return NULL;
    }

  return seat_id;
}

MetaLauncher *
meta_launcher_new (GError **error)
{
  MetaLauncher *self = NULL;
  g_autoptr (Login1Session) session_proxy = NULL;
  g_autoptr (Login1Seat) seat_proxy = NULL;
  g_autofree char *seat_id = NULL;
  gboolean have_control = FALSE;

  session_proxy = get_session_proxy (NULL, error);
  if (!session_proxy)
    goto fail;

  if (!login1_session_call_take_control_sync (session_proxy, FALSE, NULL, error))
    {
      g_prefix_error (error, "Could not take control: ");
      goto fail;
    }

  have_control = TRUE;

  seat_id = get_seat_id (error);
  if (!seat_id)
    goto fail;

  seat_proxy = get_seat_proxy (NULL, error);
  if (!seat_proxy)
    goto fail;

  self = g_slice_new0 (MetaLauncher);
  self->session_proxy = g_object_ref (session_proxy);
  self->seat_proxy = g_object_ref (seat_proxy);
  self->seat_id = g_steal_pointer (&seat_id);
  self->sysfs_fds = g_hash_table_new (NULL, NULL);
  self->session_active = TRUE;

  clutter_evdev_set_seat_id (self->seat_id);

  clutter_evdev_set_device_callbacks (on_evdev_device_open,
                                      on_evdev_device_close,
                                      self);

  g_signal_connect (self->session_proxy, "notify::active", G_CALLBACK (on_active_changed), self);

  return self;

 fail:
  if (have_control)
    login1_session_call_release_control_sync (session_proxy, NULL, NULL);
  return NULL;
}

void
meta_launcher_free (MetaLauncher *self)
{
  g_free (self->seat_id);
  g_object_unref (self->seat_proxy);
  g_object_unref (self->session_proxy);
  g_hash_table_destroy (self->sysfs_fds);
  g_slice_free (MetaLauncher, self);
}

gboolean
meta_launcher_activate_session (MetaLauncher  *launcher,
                                GError       **error)
{
  if (!login1_session_call_activate_sync (launcher->session_proxy, NULL, error))
    return FALSE;

  sync_active (launcher);
  return TRUE;
}

gboolean
meta_launcher_activate_vt (MetaLauncher  *launcher,
                           signed char    vt,
                           GError       **error)
{
  return login1_seat_call_switch_to_sync (launcher->seat_proxy, vt, NULL, error);
}
