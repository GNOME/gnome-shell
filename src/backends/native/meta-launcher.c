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
#include <clutter/egl/clutter-egl.h>
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

#include "dbus-utils.h"
#include "meta-dbus-login1.h"

#include "wayland/meta-wayland-private.h"
#include "backends/meta-backend.h"
#include "meta-cursor-renderer-native.h"

struct _MetaLauncher
{
  Login1Session *session_proxy;
  Login1Seat *seat_proxy;

  gboolean session_active;
};

/* AAA BBB CCC */

static Login1Session *
get_session_proxy (GCancellable *cancellable)
{
  char *proxy_path;
  char *session_id;
  Login1Session *session_proxy;

  if (sd_pid_get_session (getpid (), &session_id) < 0)
    return NULL;

  proxy_path = get_escaped_dbus_path ("/org/freedesktop/login1/session", session_id);

  session_proxy = login1_session_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                         "org.freedesktop.login1",
                                                         proxy_path,
                                                         cancellable, NULL);
  free (proxy_path);

  return session_proxy;
}

static Login1Seat *
get_seat_proxy (GCancellable *cancellable)
{
  return login1_seat_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                             G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                             "org.freedesktop.login1",
                                             "/org/freedesktop/login1/seat/self",
                                             cancellable, NULL);
}

/* QQQ RRR SSS */

static void
session_unpause (void)
{
  ClutterBackend *backend;
  CoglContext *cogl_context;
  CoglDisplay *cogl_display;

  backend = clutter_get_default_backend ();
  cogl_context = clutter_backend_get_cogl_context (backend);
  cogl_display = cogl_context_get_display (cogl_context);
  cogl_kms_display_queue_modes_reset (cogl_display);

  clutter_evdev_reclaim_devices ();
  clutter_egl_thaw_master_clock ();

  {
    MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
    MetaBackend *backend = meta_get_backend ();
    MetaCursorRenderer *renderer = meta_backend_get_cursor_renderer (backend);

    /* When we mode-switch back, we need to immediately queue a redraw
     * in case nothing else queued one for us, and force the cursor to
     * update. */

    clutter_actor_queue_redraw (compositor->stage);
    meta_cursor_renderer_native_force_update (META_CURSOR_RENDERER_NATIVE (renderer));
  }
}

static void
session_pause (void)
{
  clutter_evdev_release_devices ();
  clutter_egl_freeze_master_clock ();
}

static gboolean
take_device (Login1Session *session_proxy,
             int            dev_major,
             int            dev_minor,
             int           *out_fd,
             GCancellable  *cancellable,
             GError       **error)
{
  gboolean ret = FALSE;
  GVariant *fd_variant = NULL;
  int fd = -1;
  GUnixFDList *fd_list;

  if (!login1_session_call_take_device_sync (session_proxy,
                                             dev_major,
                                             dev_minor,
                                             NULL,
                                             &fd_variant,
                                             NULL, /* paused */
                                             &fd_list,
                                             cancellable,
                                             error))
    goto out;

  fd = g_unix_fd_list_get (fd_list, g_variant_get_handle (fd_variant), error);
  if (fd == -1)
    goto out;

  *out_fd = fd;
  ret = TRUE;

 out:
  if (fd_variant)
    g_variant_unref (fd_variant);
  if (fd_list)
    g_object_unref (fd_list);
  return ret;
}

static gboolean
get_device_info_from_path (const char *path,
                           int        *out_major,
                           int        *out_minor)
{
  gboolean ret = FALSE;
  int r;
  struct stat st;

  r = stat (path, &st);
  if (r < 0)
    goto out;
  if (!S_ISCHR (st.st_mode))
    goto out;

  *out_major = major (st.st_rdev);
  *out_minor = minor (st.st_rdev);
  ret = TRUE;

 out:
  return ret;
}

static gboolean
get_device_info_from_fd (int  fd,
                         int *out_major,
                         int *out_minor)
{
  gboolean ret = FALSE;
  int r;
  struct stat st;

  r = fstat (fd, &st);
  if (r < 0)
    goto out;
  if (!S_ISCHR (st.st_mode))
    goto out;

  *out_major = major (st.st_rdev);
  *out_minor = minor (st.st_rdev);
  ret = TRUE;

 out:
  return ret;
}

static int
on_evdev_device_open (const char  *path,
                      int          flags,
                      gpointer     user_data,
                      GError     **error)
{
  MetaLauncher *self = user_data;
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

  if (!take_device (self->session_proxy, major, minor, &fd, NULL, error))
    return -1;

  return fd;
}

static void
on_evdev_device_close (int      fd,
                       gpointer user_data)
{
  MetaLauncher *self = user_data;
  int major, minor;
  GError *error = NULL;

  if (!get_device_info_from_fd (fd, &major, &minor))
    {
      g_warning ("Could not get device info for fd %d: %m", fd);
      return;
    }

  if (!login1_session_call_release_device_sync (self->session_proxy,
                                                major, minor,
                                                NULL, &error))
    {
      g_warning ("Could not release device %d,%d: %s", major, minor, error->message);
    }
}

/* TTT UUU VVV */

static void
sync_active (MetaLauncher *self)
{
  gboolean active = login1_session_get_active (LOGIN1_SESSION (self->session_proxy));

  if (active == self->session_active)
    return;

  self->session_active = active;

  if (active)
    session_unpause ();
  else
    session_pause ();
}

static void
on_active_changed (Login1Session *session,
                   GParamSpec    *pspec,
                   gpointer       user_data)
{
  MetaLauncher *self = user_data;
  sync_active (self);
}

static gboolean
get_kms_fd (Login1Session *session_proxy,
            int *fd_out)
{
  int major, minor;
  int fd;
  GError *error = NULL;

  /* XXX -- use udev to find the DRM master device */
  if (!get_device_info_from_path ("/dev/dri/card0", &major, &minor))
    {
      g_warning ("Could not stat /dev/dri/card0: %m");
      return FALSE;
    }

  if (!take_device (session_proxy, major, minor, &fd, NULL, &error))
    {
      g_warning ("Could not open DRM device: %s\n", error->message);
      g_error_free (error);
      return FALSE;
    }

  *fd_out = fd;

  return TRUE;
}

/* XXX YYY ZZZ */

MetaLauncher *
meta_launcher_new (void)
{
  MetaLauncher *self;
  Login1Session *session_proxy;
  GError *error = NULL;
  int kms_fd;

  session_proxy = get_session_proxy (NULL);
  if (!login1_session_call_take_control_sync (session_proxy, FALSE, NULL, &error))
    {
      g_warning ("Could not take control: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  if (!get_kms_fd (session_proxy, &kms_fd))
    return NULL;

  self = g_slice_new0 (MetaLauncher);
  self->session_proxy = session_proxy;
  self->seat_proxy = get_seat_proxy (NULL);

  self->session_active = TRUE;

  clutter_egl_set_kms_fd (kms_fd);
  clutter_evdev_set_device_callbacks (on_evdev_device_open,
                                      on_evdev_device_close,
                                      self);

  g_signal_connect (self->session_proxy, "notify::active", G_CALLBACK (on_active_changed), self);

  return self;
}

void
meta_launcher_free (MetaLauncher *self)
{
  g_object_unref (self->seat_proxy);
  g_object_unref (self->session_proxy);
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
