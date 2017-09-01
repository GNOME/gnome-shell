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
#include "meta-idle-monitor-native.h"
#include "meta-renderer-native.h"

#define DRM_CARD_UDEV_DEVICE_TYPE "drm_minor"

struct _MetaLauncher
{
  Login1Session *session_proxy;
  Login1Seat *seat_proxy;

  GHashTable *sysfs_fds;
  gboolean session_active;

  int kms_fd;
  char *kms_file_path;
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

  /* Allow readonly access to sysfs */
  if (g_str_has_prefix (path, "/sys/"))
    {
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

  if (g_hash_table_lookup (self->sysfs_fds, GINT_TO_POINTER (fd)))
    {
      /* /sys/ paths just need close() here */
      g_hash_table_remove (self->sysfs_fds, GINT_TO_POINTER (fd));
      close (fd);
      return;
    }

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

static guint
count_devices_with_connectors (const gchar *seat_name,
                               GList       *devices)
{
  g_autoptr (GHashTable) cards = NULL;
  GList *tmp;

  cards = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
  for (tmp = devices; tmp != NULL; tmp = tmp->next)
    {
      GUdevDevice *device = tmp->data;
      g_autoptr (GUdevDevice) parent_device = NULL;
      const gchar *parent_device_type = NULL;
      const gchar *parent_device_name = NULL;
      const gchar *card_seat;

      /* filter out the real card devices, we only care about the connectors */
      if (g_udev_device_get_device_type (device) != G_UDEV_DEVICE_TYPE_NONE)
        continue;

      /* only connectors have a modes attribute */
      if (!g_udev_device_has_sysfs_attr (device, "modes"))
        continue;

      parent_device = g_udev_device_get_parent (device);

      if (g_udev_device_get_device_type (parent_device) == G_UDEV_DEVICE_TYPE_CHAR)
        parent_device_type = g_udev_device_get_property (parent_device, "DEVTYPE");

      if (g_strcmp0 (parent_device_type, DRM_CARD_UDEV_DEVICE_TYPE) != 0)
        continue;

      card_seat = g_udev_device_get_property (parent_device, "ID_SEAT");

      if (!card_seat)
        card_seat = "seat0";

      if (g_strcmp0 (seat_name, card_seat) != 0)
        continue;

      parent_device_name = g_udev_device_get_name (parent_device);
      g_hash_table_insert (cards,
                           (gpointer) parent_device_name ,
                           g_steal_pointer (&parent_device));
    }

  return g_hash_table_size (cards);
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

  /* We need to explicitly match the subsystem for now.
   * https://bugzilla.gnome.org/show_bug.cgi?id=773224
   */
  g_udev_enumerator_add_match_subsystem (enumerator, "drm");

  devices = g_udev_enumerator_execute (enumerator);
  if (!devices)
    goto out;

  /* For now, fail on systems where some of the connectors
   * are connected to secondary gpus.
   *
   * https://bugzilla.gnome.org/show_bug.cgi?id=771442
   */
  if (g_getenv ("MUTTER_ALLOW_HYBRID_GPUS") == NULL)
    {
      guint num_devices;

      num_devices = count_devices_with_connectors (seat_name, devices);
      if (num_devices != 1)
        goto out;
    }

  for (tmp = devices; tmp != NULL; tmp = tmp->next)
    {
      g_autoptr (GUdevDevice) platform_device = NULL;
      g_autoptr (GUdevDevice) pci_device = NULL;
      GUdevDevice *dev = tmp->data;
      gint boot_vga;
      const gchar *device_type;
      const gchar *device_seat;

      /* filter out devices that are not character device, like card0-VGA-1 */
      if (g_udev_device_get_device_type (dev) != G_UDEV_DEVICE_TYPE_CHAR)
        continue;

      device_type = g_udev_device_get_property (dev, "DEVTYPE");
      if (g_strcmp0 (device_type, DRM_CARD_UDEV_DEVICE_TYPE) != 0)
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

out:
  g_list_free_full (devices, g_object_unref);

  return path;
}

static gboolean
get_kms_fd (Login1Session *session_proxy,
            const gchar   *seat_id,
            int           *fd_out,
            char         **kms_file_path_out,
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
  *kms_file_path_out = g_steal_pointer (&path);

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
  char *kms_file_path;

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

  if (!get_kms_fd (session_proxy, seat_id, &kms_fd, &kms_file_path, error))
    goto fail;

  self = g_slice_new0 (MetaLauncher);
  self->session_proxy = g_object_ref (session_proxy);
  self->seat_proxy = g_object_ref (seat_proxy);
  self->sysfs_fds = g_hash_table_new (NULL, NULL);

  self->session_active = TRUE;
  self->kms_fd = kms_fd;
  self->kms_file_path = kms_file_path;

  clutter_evdev_set_seat_id (seat_id);

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
  g_hash_table_destroy (self->sysfs_fds);
  g_free (self->kms_file_path);
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

const char *
meta_launcher_get_kms_file_path (MetaLauncher *self)
{
  return self->kms_file_path;
}
