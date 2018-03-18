/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2013 Red Hat Inc.
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
 * Author: Giovanni Campagna <gcampagn@redhat.com>
 */

#include "config.h"

#include "meta-monitor-manager-kms.h"
#include "meta-monitor-config-manager.h"
#include "meta-backend-native.h"
#include "meta-crtc.h"
#include "meta-launcher.h"
#include "meta-output.h"
#include "meta-backend-private.h"
#include "meta-renderer-native.h"
#include "meta-crtc-kms.h"
#include "meta-gpu-kms.h"
#include "meta-output-kms.h"

#include <string.h>
#include <stdlib.h>
#include <clutter/clutter.h>

#include <drm.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <meta/main.h>
#include <meta/errors.h>

#include <gudev/gudev.h>

#define DRM_CARD_UDEV_DEVICE_TYPE "drm_minor"

typedef struct
{
  GSource source;

  gpointer fd_tag;
  MetaMonitorManagerKms *manager_kms;
} MetaKmsSource;

struct _MetaMonitorManagerKms
{
  MetaMonitorManager parent_instance;

  MetaGpuKms *primary_gpu;

  GUdevClient *udev;
  guint uevent_handler_id;
};

struct _MetaMonitorManagerKmsClass
{
  MetaMonitorManagerClass parent_class;
};

static void
initable_iface_init (GInitableIface *initable_iface);

G_DEFINE_TYPE_WITH_CODE (MetaMonitorManagerKms, meta_monitor_manager_kms,
                         META_TYPE_MONITOR_MANAGER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init))

static GBytes *
meta_monitor_manager_kms_read_edid (MetaMonitorManager *manager,
                                    MetaOutput         *output)
{
  return meta_output_kms_read_edid (output);
}

static void
meta_monitor_manager_kms_set_power_save_mode (MetaMonitorManager *manager,
                                              MetaPowerSave       mode)
{
  uint64_t state;
  GList *l;

  switch (mode) {
  case META_POWER_SAVE_ON:
    state = DRM_MODE_DPMS_ON;
    break;
  case META_POWER_SAVE_STANDBY:
    state = DRM_MODE_DPMS_STANDBY;
    break;
  case META_POWER_SAVE_SUSPEND:
    state = DRM_MODE_DPMS_SUSPEND;
    break;
  case META_POWER_SAVE_OFF:
    state = DRM_MODE_DPMS_OFF;
    break;
  default:
    return;
  }

  for (l = manager->gpus; l; l = l->next)
    {
      MetaGpuKms *gpu_kms = l->data;

      meta_gpu_kms_set_power_save_mode (gpu_kms, state);
    }
}

static void
meta_monitor_manager_kms_ensure_initial_config (MetaMonitorManager *manager)
{
  MetaMonitorsConfig *config;

  config = meta_monitor_manager_ensure_configured (manager);

  meta_monitor_manager_update_logical_state (manager, config);
}

static void
apply_crtc_assignments (MetaMonitorManager *manager,
                        MetaCrtcInfo       **crtcs,
                        unsigned int         n_crtcs,
                        MetaOutputInfo     **outputs,
                        unsigned int         n_outputs)
{
  unsigned i;
  GList *l;

  for (i = 0; i < n_crtcs; i++)
    {
      MetaCrtcInfo *crtc_info = crtcs[i];
      MetaCrtc *crtc = crtc_info->crtc;

      crtc->is_dirty = TRUE;

      if (crtc_info->mode == NULL)
        {
          crtc->rect.x = 0;
          crtc->rect.y = 0;
          crtc->rect.width = 0;
          crtc->rect.height = 0;
          crtc->current_mode = NULL;
        }
      else
        {
          MetaCrtcMode *mode;
          unsigned int j;
          int width, height;

          mode = crtc_info->mode;

          if (meta_monitor_transform_is_rotated (crtc_info->transform))
            {
              width = mode->height;
              height = mode->width;
            }
          else
            {
              width = mode->width;
              height = mode->height;
            }

          crtc->rect.x = crtc_info->x;
          crtc->rect.y = crtc_info->y;
          crtc->rect.width = width;
          crtc->rect.height = height;
          crtc->current_mode = mode;
          crtc->transform = crtc_info->transform;

          for (j = 0; j < crtc_info->outputs->len; j++)
            {
              MetaOutput *output = g_ptr_array_index (crtc_info->outputs, j);

              output->is_dirty = TRUE;
              output->crtc = crtc;
            }
        }

      meta_crtc_kms_apply_transform (crtc);
    }
  /* Disable CRTCs not mentioned in the list (they have is_dirty == FALSE,
     because they weren't seen in the first loop) */
  for (l = manager->gpus; l; l = l->next)
    {
      MetaGpu *gpu = l->data;
      GList *k;

      for (k = meta_gpu_get_crtcs (gpu); k; k = k->next)
        {
          MetaCrtc *crtc = k->data;

          crtc->logical_monitor = NULL;

          if (crtc->is_dirty)
            {
              crtc->is_dirty = FALSE;
              continue;
            }

          crtc->rect.x = 0;
          crtc->rect.y = 0;
          crtc->rect.width = 0;
          crtc->rect.height = 0;
          crtc->current_mode = NULL;
        }
    }

  for (i = 0; i < n_outputs; i++)
    {
      MetaOutputInfo *output_info = outputs[i];
      MetaOutput *output = output_info->output;

      output->is_primary = output_info->is_primary;
      output->is_presentation = output_info->is_presentation;
      output->is_underscanning = output_info->is_underscanning;

      meta_output_kms_set_underscan (output);
    }

  /* Disable outputs not mentioned in the list */
  for (l = manager->gpus; l; l = l->next)
    {
      MetaGpu *gpu = l->data;
      GList *k;

      for (k = meta_gpu_get_outputs (gpu); k; k = k->next)
        {
          MetaOutput *output = k->data;

          if (output->is_dirty)
            {
              output->is_dirty = FALSE;
              continue;
            }

          output->crtc = NULL;
          output->is_primary = FALSE;
        }
    }
}

static void
update_screen_size (MetaMonitorManager *manager,
                    MetaMonitorsConfig *config)
{
  GList *l;
  int screen_width = 0;
  int screen_height = 0;

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;
      int right_edge;
      int bottom_edge;

      right_edge = (logical_monitor_config->layout.width +
                    logical_monitor_config->layout.x);
      if (right_edge > screen_width)
        screen_width = right_edge;

      bottom_edge = (logical_monitor_config->layout.height +
                     logical_monitor_config->layout.y);
      if (bottom_edge > screen_height)
        screen_height = bottom_edge;
    }

  manager->screen_width = screen_width;
  manager->screen_height = screen_height;
}

static gboolean
meta_monitor_manager_kms_apply_monitors_config (MetaMonitorManager      *manager,
                                                MetaMonitorsConfig      *config,
                                                MetaMonitorsConfigMethod method,
                                                GError                 **error)
{
  GPtrArray *crtc_infos;
  GPtrArray *output_infos;

  if (!config)
    {
      manager->screen_width = META_MONITOR_MANAGER_MIN_SCREEN_WIDTH;
      manager->screen_height = META_MONITOR_MANAGER_MIN_SCREEN_HEIGHT;
      meta_monitor_manager_rebuild (manager, NULL);
      return TRUE;
    }

  if (!meta_monitor_config_manager_assign (manager, config,
                                           &crtc_infos, &output_infos,
                                           error))
    return FALSE;

  if (method == META_MONITORS_CONFIG_METHOD_VERIFY)
    {
      g_ptr_array_free (crtc_infos, TRUE);
      g_ptr_array_free (output_infos, TRUE);
      return TRUE;
    }

  apply_crtc_assignments (manager,
                          (MetaCrtcInfo **) crtc_infos->pdata,
                          crtc_infos->len,
                          (MetaOutputInfo **) output_infos->pdata,
                          output_infos->len);

  g_ptr_array_free (crtc_infos, TRUE);
  g_ptr_array_free (output_infos, TRUE);

  update_screen_size (manager, config);
  meta_monitor_manager_rebuild (manager, config);

  return TRUE;
}

static void
meta_monitor_manager_kms_get_crtc_gamma (MetaMonitorManager  *manager,
                                         MetaCrtc            *crtc,
                                         gsize               *size,
                                         unsigned short     **red,
                                         unsigned short     **green,
                                         unsigned short     **blue)
{
  MetaGpu *gpu = meta_crtc_get_gpu (crtc);
  int kms_fd = meta_gpu_kms_get_fd (META_GPU_KMS (gpu));
  drmModeCrtc *kms_crtc;

  kms_crtc = drmModeGetCrtc (kms_fd, crtc->crtc_id);

  *size = kms_crtc->gamma_size;
  *red = g_new (unsigned short, *size);
  *green = g_new (unsigned short, *size);
  *blue = g_new (unsigned short, *size);

  drmModeCrtcGetGamma (kms_fd, crtc->crtc_id, *size, *red, *green, *blue);

  drmModeFreeCrtc (kms_crtc);
}

static void
meta_monitor_manager_kms_set_crtc_gamma (MetaMonitorManager *manager,
                                         MetaCrtc           *crtc,
                                         gsize               size,
                                         unsigned short     *red,
                                         unsigned short     *green,
                                         unsigned short     *blue)
{
  MetaGpu *gpu = meta_crtc_get_gpu (crtc);
  int kms_fd = meta_gpu_kms_get_fd (META_GPU_KMS (gpu));

  drmModeCrtcSetGamma (kms_fd, crtc->crtc_id, size, red, green, blue);
}

static void
handle_hotplug_event (MetaMonitorManager *manager)
{
  meta_monitor_manager_read_current_state (manager);
  meta_monitor_manager_on_hotplug (manager);
}

static void
on_uevent (GUdevClient *client,
           const char  *action,
           GUdevDevice *device,
           gpointer     user_data)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (user_data);
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_kms);

  if (!g_udev_device_get_property_as_boolean (device, "HOTPLUG"))
    return;

  handle_hotplug_event (manager);
}

static void
meta_monitor_manager_kms_connect_uevent_handler (MetaMonitorManagerKms *manager_kms)
{
  manager_kms->uevent_handler_id = g_signal_connect (manager_kms->udev,
                                                     "uevent",
                                                     G_CALLBACK (on_uevent),
                                                     manager_kms);
}

static void
meta_monitor_manager_kms_disconnect_uevent_handler (MetaMonitorManagerKms *manager_kms)
{
  g_signal_handler_disconnect (manager_kms->udev,
                               manager_kms->uevent_handler_id);
  manager_kms->uevent_handler_id = 0;
}

void
meta_monitor_manager_kms_pause (MetaMonitorManagerKms *manager_kms)
{
  meta_monitor_manager_kms_disconnect_uevent_handler (manager_kms);
}

void
meta_monitor_manager_kms_resume (MetaMonitorManagerKms *manager_kms)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_kms);

  meta_monitor_manager_kms_connect_uevent_handler (manager_kms);
  handle_hotplug_event (manager);
}

static gboolean
meta_monitor_manager_kms_is_transform_handled (MetaMonitorManager  *manager,
                                               MetaCrtc            *crtc,
                                               MetaMonitorTransform transform)
{
  return meta_crtc_kms_is_transform_handled (crtc, transform);
}

static float
meta_monitor_manager_kms_calculate_monitor_mode_scale (MetaMonitorManager *manager,
                                                       MetaMonitor        *monitor,
                                                       MetaMonitorMode    *monitor_mode)
{
  return meta_monitor_calculate_mode_scale (monitor, monitor_mode);
}

static float *
meta_monitor_manager_kms_calculate_supported_scales (MetaMonitorManager          *manager,
                                                     MetaLogicalMonitorLayoutMode layout_mode,
                                                     MetaMonitor                 *monitor,
                                                     MetaMonitorMode             *monitor_mode,
                                                     int                         *n_supported_scales)
{
  MetaMonitorScalesConstraint constraints =
    META_MONITOR_SCALES_CONSTRAINT_NONE;

  switch (layout_mode)
    {
    case META_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
      break;
    case META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
      constraints |= META_MONITOR_SCALES_CONSTRAINT_NO_FRAC;
      break;
    }

  return meta_monitor_calculate_supported_scales (monitor, monitor_mode,
                                                  constraints,
                                                  n_supported_scales);
}

static MetaMonitorManagerCapability
meta_monitor_manager_kms_get_capabilities (MetaMonitorManager *manager)
{
  MetaBackend *backend = meta_monitor_manager_get_backend (manager);
  MetaSettings *settings = meta_backend_get_settings (backend);
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (renderer);
  MetaMonitorManagerCapability capabilities =
    META_MONITOR_MANAGER_CAPABILITY_NONE;

  if (meta_settings_is_experimental_feature_enabled (
        settings,
        META_EXPERIMENTAL_FEATURE_SCALE_MONITOR_FRAMEBUFFER))
    capabilities |= META_MONITOR_MANAGER_CAPABILITY_LAYOUT_MODE;

  if (meta_renderer_native_supports_mirroring (renderer_native))
    capabilities |= META_MONITOR_MANAGER_CAPABILITY_MIRRORING;

  return capabilities;
}

static gboolean
meta_monitor_manager_kms_get_max_screen_size (MetaMonitorManager *manager,
                                              int                *max_width,
                                              int                *max_height)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);

  if (meta_is_stage_views_enabled ())
    return FALSE;

  meta_gpu_kms_get_max_buffer_size (manager_kms->primary_gpu,
                                    max_width, max_height);

  return TRUE;
}

static MetaLogicalMonitorLayoutMode
meta_monitor_manager_kms_get_default_layout_mode (MetaMonitorManager *manager)
{
  MetaBackend *backend = meta_monitor_manager_get_backend (manager);
  MetaSettings *settings = meta_backend_get_settings (backend);

  if (!meta_is_stage_views_enabled ())
    return META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;

  if (meta_settings_is_experimental_feature_enabled (
        settings,
        META_EXPERIMENTAL_FEATURE_SCALE_MONITOR_FRAMEBUFFER))
    return META_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL;
  else
    return META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;
}

MetaGpuKms *
meta_monitor_manager_kms_get_primary_gpu (MetaMonitorManagerKms *manager_kms)
{
  return manager_kms->primary_gpu;
}

typedef enum
{
  GPU_TYPE_PRIMARY,
  GPU_TYPE_SECONDARY
} GpuType;

static GList *
get_gpu_paths (MetaMonitorManagerKms *manager_kms,
               GpuType                gpu_type,
               const char            *filtered_gpu_path)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_kms);
  MetaBackend *backend = meta_monitor_manager_get_backend (manager);
  MetaBackendNative *backend_native = META_BACKEND_NATIVE (backend);
  MetaLauncher *launcher = meta_backend_native_get_launcher (backend_native);
  g_autoptr (GUdevEnumerator) enumerator = NULL;
  const char *seat_id;
  GList *devices;
  GList *l;
  GList *gpu_paths = NULL;

  enumerator = g_udev_enumerator_new (manager_kms->udev);

  g_udev_enumerator_add_match_name (enumerator, "card*");
  g_udev_enumerator_add_match_tag (enumerator, "seat");

  /*
   * We need to explicitly match the subsystem for now.
   * https://bugzilla.gnome.org/show_bug.cgi?id=773224
   */
  g_udev_enumerator_add_match_subsystem (enumerator, "drm");

  devices = g_udev_enumerator_execute (enumerator);
  if (!devices)
    return NULL;

  seat_id = meta_launcher_get_seat_id (launcher);

  for (l = devices; l; l = l->next)
    {
      GUdevDevice *dev = l->data;
      g_autoptr (GUdevDevice) platform_device = NULL;
      g_autoptr (GUdevDevice) pci_device = NULL;
      const char *device_path;
      const char *device_type;
      const char *device_seat;

      /* Filter out devices that are not character device, like card0-VGA-1. */
      if (g_udev_device_get_device_type (dev) != G_UDEV_DEVICE_TYPE_CHAR)
        continue;

      device_type = g_udev_device_get_property (dev, "DEVTYPE");
      if (g_strcmp0 (device_type, DRM_CARD_UDEV_DEVICE_TYPE) != 0)
        continue;

      device_path = g_udev_device_get_device_file (dev);
      if (g_strcmp0 (device_path, filtered_gpu_path) == 0)
        continue;

      device_seat = g_udev_device_get_property (dev, "ID_SEAT");
      if (!device_seat)
        {
          /* When ID_SEAT is not set, it means seat0. */
          device_seat = "seat0";
        }
      else if (g_strcmp0 (device_seat, "seat0") != 0 &&
               gpu_type == GPU_TYPE_PRIMARY)
        {
          /*
           * If the device has been explicitly assigned other seat
           * than seat0, it is probably the right device to use.
           */
          gpu_paths = g_list_append (gpu_paths, g_strdup (device_path));
          break;
        }

      /* Skip devices that do not belong to our seat. */
      if (g_strcmp0 (seat_id, device_seat))
        continue;

      platform_device = g_udev_device_get_parent_with_subsystem (dev,
                                                                 "platform",
                                                                 NULL);
      if (platform_device != NULL && gpu_type == GPU_TYPE_PRIMARY)
        {
          gpu_paths = g_list_append (gpu_paths, g_strdup (device_path));
          break;
        }

      pci_device = g_udev_device_get_parent_with_subsystem (dev, "pci", NULL);
      if (pci_device != NULL)
        {
          if (gpu_type == GPU_TYPE_PRIMARY)
            {
              int boot_vga;

              boot_vga = g_udev_device_get_sysfs_attr_as_int (pci_device,
                                                              "boot_vga");
              if (boot_vga == 1)
                {
                  gpu_paths = g_list_append (gpu_paths, g_strdup (device_path));
                  break;
                }
            }
          else
            {
              gpu_paths = g_list_append (gpu_paths, g_strdup (device_path));
            }
        }
    }

  g_list_free_full (devices, g_object_unref);

  return gpu_paths;
}

static gboolean
meta_monitor_manager_kms_initable_init (GInitable    *initable,
                                        GCancellable *cancellable,
                                        GError      **error)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (initable);
  const char *subsystems[2] = { "drm", NULL };
  GList *gpu_paths;
  g_autofree char *primary_gpu_path = NULL;
  GList *l;

  manager_kms->udev = g_udev_client_new (subsystems);

  gpu_paths = get_gpu_paths (manager_kms, GPU_TYPE_PRIMARY, NULL);
  if (g_list_length (gpu_paths) != 1)
    {
      g_list_free_full (gpu_paths, g_free);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "Could not find a primary drm kms device");
      return FALSE;
    }

  primary_gpu_path = g_strdup (gpu_paths->data);
  manager_kms->primary_gpu = meta_gpu_kms_new (manager_kms,
                                               primary_gpu_path,
                                               error);
  g_list_free_full (gpu_paths, g_free);
  if (!manager_kms->primary_gpu)
    return FALSE;

  meta_monitor_manager_kms_connect_uevent_handler (manager_kms);

  meta_monitor_manager_add_gpu (META_MONITOR_MANAGER (manager_kms),
                                META_GPU (manager_kms->primary_gpu));

  gpu_paths = get_gpu_paths (manager_kms, GPU_TYPE_SECONDARY, primary_gpu_path);
  for (l = gpu_paths; l; l = l->next)
    {
      g_autoptr (GError) secondary_error = NULL;
      char *gpu_path = l->data;
      MetaGpuKms *gpu_kms;

      gpu_kms = meta_gpu_kms_new (manager_kms, gpu_path, &secondary_error);
      if (!gpu_kms)
        {
          g_warning ("Failed to open secondary gpu '%s': %s",
                     gpu_path, secondary_error->message);
          continue;
        }

      meta_monitor_manager_add_gpu (META_MONITOR_MANAGER (manager_kms),
                                    META_GPU (gpu_kms));
    }
  g_list_free_full (gpu_paths, g_free);

  return TRUE;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = meta_monitor_manager_kms_initable_init;
}

static void
meta_monitor_manager_kms_dispose (GObject *object)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (object);

  g_clear_object (&manager_kms->udev);

  G_OBJECT_CLASS (meta_monitor_manager_kms_parent_class)->dispose (object);
}

static void
meta_monitor_manager_kms_init (MetaMonitorManagerKms *manager_kms)
{
}

static void
meta_monitor_manager_kms_class_init (MetaMonitorManagerKmsClass *klass)
{
  MetaMonitorManagerClass *manager_class = META_MONITOR_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = meta_monitor_manager_kms_dispose;

  manager_class->read_edid = meta_monitor_manager_kms_read_edid;
  manager_class->ensure_initial_config = meta_monitor_manager_kms_ensure_initial_config;
  manager_class->apply_monitors_config = meta_monitor_manager_kms_apply_monitors_config;
  manager_class->set_power_save_mode = meta_monitor_manager_kms_set_power_save_mode;
  manager_class->get_crtc_gamma = meta_monitor_manager_kms_get_crtc_gamma;
  manager_class->set_crtc_gamma = meta_monitor_manager_kms_set_crtc_gamma;
  manager_class->is_transform_handled = meta_monitor_manager_kms_is_transform_handled;
  manager_class->calculate_monitor_mode_scale = meta_monitor_manager_kms_calculate_monitor_mode_scale;
  manager_class->calculate_supported_scales = meta_monitor_manager_kms_calculate_supported_scales;
  manager_class->get_capabilities = meta_monitor_manager_kms_get_capabilities;
  manager_class->get_max_screen_size = meta_monitor_manager_kms_get_max_screen_size;
  manager_class->get_default_layout_mode = meta_monitor_manager_kms_get_default_layout_mode;
}
