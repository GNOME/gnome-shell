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
#include <gudev/gudev.h>

#include "dbus-utils.h"
#include "meta-dbus-login1.h"

#include "backends/meta-backend-private.h"
#include "meta-cursor-renderer-native.h"
#include "meta-idle-monitor-native.h"

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUdevDevice, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUdevClient, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUdevEnumerator, g_object_unref)

struct _MetaLauncher
{
  Login1Session *session_proxy;
  Login1Seat *seat_proxy;

  gboolean session_active;

  int kms_fd;
};

static Login1Session *
get_session_proxy (GCancellable *cancellable,
                   GError      **error)
{
  g_autofree char *proxy_path = NULL;
  g_autofree char *session_id = NULL;
  Login1Session *session_proxy;

  if (sd_pid_get_session (getpid (), &session_id) < 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Could not get session ID: %m");
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

static void
session_unpause (void)
{
  ClutterBackend *clutter_backend;
  CoglContext *cogl_context;
  CoglDisplay *cogl_display;

  clutter_backend = clutter_get_default_backend ();
  cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  cogl_display = cogl_context_get_display (cogl_context);
  cogl_kms_display_queue_modes_reset (cogl_display);

  clutter_evdev_reclaim_devices ();
  clutter_egl_thaw_master_clock ();

  {
    MetaBackend *backend = meta_get_backend ();
    MetaCursorRenderer *renderer = meta_backend_get_cursor_renderer (backend);
    ClutterActor *stage = meta_backend_get_stage (backend);

    /* When we mode-switch back, we need to immediately queue a redraw
     * in case nothing else queued one for us, and force the cursor to
     * update. */

    clutter_actor_queue_redraw (stage);
    meta_cursor_renderer_native_force_update (META_CURSOR_RENDERER_NATIVE (renderer));
    meta_idle_monitor_native_reset_idletime (meta_idle_monitor_get_core ());
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
      goto out;
    }

  if (!login1_session_call_release_device_sync (self->session_proxy,
                                                major, minor,
                                                NULL, &error))
    {
      g_warning ("Could not release device %d,%d: %s", major, minor, error->message);
    }

out:
  close (fd);
}

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

static gchar *
get_primary_gpu_path (const gchar *seat_name)
{
  const gchar *subsystems[] = {"drm", NULL};
  gchar *path = NULL;
  GList *devices, *tmp;

  g_autoptr (GUdevClient) gudev_client = g_udev_client_new (subsystems);
  g_autoptr (GUdevEnumerator) enumerator = g_udev_enumerator_new (gudev_client);

  g_udev_enumerator_add_match_name (enumerator, "card*");
  g_udev_enumerator_add_match_tag (enumerator, "seat");

  devices = g_udev_enumerator_execute (enumerator);
  if (!devices)
    goto out;

  for (tmp = devices; tmp != NULL; tmp = tmp->next)
    {
      g_autoptr (GUdevDevice) platform_device = NULL;
      g_autoptr (GUdevDevice) pci_device = NULL;
      GUdevDevice *dev = tmp->data;
      gint boot_vga;
      const gchar *device_seat;

      /* filter out devices that are not character device, like card0-VGA-1 */
      if (g_udev_device_get_device_type (dev) != G_UDEV_DEVICE_TYPE_CHAR)
        continue;

      device_seat = g_udev_device_get_property (dev, "ID_SEAT");
      if (!device_seat)
        {
          /* when ID_SEAT is not set, it means seat0 */
          device_seat = "seat0";
        }
      else if (g_strcmp0 (device_seat, "seat0") != 0)
        {
          /* if the device has been explicitly assigned other seat
           * than seat0, it is probably the right device to use */
          path = g_strdup (g_udev_device_get_device_file (dev));
          break;
        }

      /* skip devices that do not belong to our seat */
      if (g_strcmp0 (seat_name, device_seat))
        continue;

      platform_device = g_udev_device_get_parent_with_subsystem (dev, "platform", NULL);
      if (platform_device != NULL)
        {
          path = g_strdup (g_udev_device_get_device_file (dev));
          break;
        }

      pci_device = g_udev_device_get_parent_with_subsystem (dev, "pci", NULL);
      if (pci_device != NULL)
        {
          /* get value of boot_vga attribute or 0 if the device has no boot_vga */
          boot_vga = g_udev_device_get_sysfs_attr_as_int (pci_device, "boot_vga");
          if (boot_vga == 1)
            {
              /* found the boot_vga device */
              path = g_strdup (g_udev_device_get_device_file (dev));
              break;
            }
        }
    }

  g_list_free_full (devices, g_object_unref);

out:
  return path;
}

static gboolean
get_kms_fd (Login1Session *session_proxy,
            const gchar   *seat_id,
            int           *fd_out,
            GError       **error)
{
  int major, minor;
  int fd;

  g_autofree gchar *path = get_primary_gpu_path (seat_id);
  if (!path)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "could not find drm kms device");
      return FALSE;
    }

  if (!get_device_info_from_path (path, &major, &minor))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Could not get device info for path %s: %m", path);
      return FALSE;
    }

  if (!take_device (session_proxy, major, minor, &fd, NULL, error))
    {
      g_prefix_error (error, "Could not open DRM device: ");
      return FALSE;
    }

  *fd_out = fd;

  return TRUE;
}

static gchar *
get_seat_id (GError **error)
{
  g_autofree char *session_id = NULL;
  char *seat_id = NULL;
  int r;

  r = sd_pid_get_session (0, &session_id);
  if (r < 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Could not get session for PID: %s", g_strerror (-r));
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
  int kms_fd;

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

  if (!get_kms_fd (session_proxy, seat_id, &kms_fd, error))
    goto fail;

  self = g_slice_new0 (MetaLauncher);
  self->session_proxy = g_object_ref (session_proxy);
  self->seat_proxy = g_object_ref (seat_proxy);

  self->session_active = TRUE;
  self->kms_fd = kms_fd;

  clutter_egl_set_kms_fd (kms_fd);
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

int
meta_launcher_get_kms_fd (MetaLauncher *self)
{
  return self->kms_fd;
}
