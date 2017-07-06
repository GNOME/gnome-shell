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

#include "meta-default-modes.h"

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

  int fd;
  char *file_path;
  MetaKmsSource *source;

  drmModeConnector **connectors;
  unsigned int       n_connectors;

  GUdevClient *udev;
  guint uevent_handler_id;

  gboolean page_flips_not_supported;

  int max_buffer_width;
  int max_buffer_height;
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

int
meta_monitor_manager_kms_get_fd (MetaMonitorManagerKms *manager_kms)
{
  return manager_kms->fd;
}

const char *
meta_monitor_manager_kms_get_file_path (MetaMonitorManagerKms *manager_kms)
{
  return manager_kms->file_path;
}

static void
free_resources (MetaMonitorManagerKms *manager_kms)
{
  unsigned i;

  for (i = 0; i < manager_kms->n_connectors; i++)
    drmModeFreeConnector (manager_kms->connectors[i]);

  g_free (manager_kms->connectors);
}

static int
compare_outputs (gconstpointer one,
                 gconstpointer two)
{
  const MetaOutput *o_one = one, *o_two = two;

  return strcmp (o_one->name, o_two->name);
}

static void
meta_monitor_mode_destroy_notify (MetaCrtcMode *mode)
{
  g_slice_free (drmModeModeInfo, mode->driver_private);
}

gboolean
meta_drm_mode_equal (const drmModeModeInfo *one,
                     const drmModeModeInfo *two)
{
  return (one->clock == two->clock &&
          one->hdisplay == two->hdisplay &&
          one->hsync_start == two->hsync_start &&
          one->hsync_end == two->hsync_end &&
          one->htotal == two->htotal &&
          one->hskew == two->hskew &&
          one->vdisplay == two->vdisplay &&
          one->vsync_start == two->vsync_start &&
          one->vsync_end == two->vsync_end &&
          one->vtotal == two->vtotal &&
          one->vscan == two->vscan &&
          one->vrefresh == two->vrefresh &&
          one->flags == two->flags &&
          one->type == two->type &&
          strncmp (one->name, two->name, DRM_DISPLAY_MODE_LEN) == 0);
}

static guint
drm_mode_hash (gconstpointer ptr)
{
  const drmModeModeInfo *mode = ptr;
  guint hash = 0;

  /* We don't include the name in the hash because it's generally
     derived from the other fields (hdisplay, vdisplay and flags)
  */

  hash ^= mode->clock;
  hash ^= mode->hdisplay ^ mode->hsync_start ^ mode->hsync_end;
  hash ^= mode->vdisplay ^ mode->vsync_start ^ mode->vsync_end;
  hash ^= mode->vrefresh;
  hash ^= mode->flags ^ mode->type;

  return hash;
}

MetaCrtcMode *
meta_monitor_manager_kms_get_mode_from_drm_mode (MetaMonitorManagerKms *manager_kms,
                                                 const drmModeModeInfo *drm_mode)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_kms);
  GList *l;

  for (l = manager->modes; l; l = l->next)
    {
      MetaCrtcMode *mode = l->data;

      if (meta_drm_mode_equal (drm_mode, mode->driver_private))
        return mode;
    }

  g_assert_not_reached ();
  return NULL;
}

float
meta_calculate_drm_mode_refresh_rate (const drmModeModeInfo *mode)
{
  float refresh = 0.0;

  if (mode->htotal > 0 && mode->vtotal > 0)
    {
      /* Calculate refresh rate in milliHz first for extra precision. */
      refresh = (mode->clock * 1000000LL) / mode->htotal;
      refresh += (mode->vtotal / 2);
      refresh /= mode->vtotal;
      if (mode->vscan > 1)
        refresh /= mode->vscan;
      refresh /= 1000.0;
    }
  return refresh;
}

static MetaCrtcMode *
create_mode (const drmModeModeInfo *drm_mode,
             long                   mode_id)
{
  MetaCrtcMode *mode;

  mode = g_object_new (META_TYPE_CRTC_MODE, NULL);
  mode->mode_id = mode_id;
  mode->name = g_strndup (drm_mode->name, DRM_DISPLAY_MODE_LEN);
  mode->width = drm_mode->hdisplay;
  mode->height = drm_mode->vdisplay;
  mode->flags = drm_mode->flags;
  mode->refresh_rate = meta_calculate_drm_mode_refresh_rate (drm_mode);
  mode->driver_private = g_slice_dup (drmModeModeInfo, drm_mode);
  mode->driver_notify = (GDestroyNotify) meta_monitor_mode_destroy_notify;

  return mode;
}

static MetaOutput *
find_output_by_id (GList *outputs,
                   glong  id)
{
  GList *l;

  for (l = outputs; l; l = l->next)
    {
      MetaOutput *output = l->data;

      if (output->winsys_id == id)
        return output;
    }

  return NULL;
}

static void
setup_output_clones (MetaMonitorManager *manager)
{
  GList *l;

  for (l = manager->outputs; l; l = l->next)
    {
      MetaOutput *output = l->data;
      GList *k;

      for (k = manager->outputs; k; k = k->next)
        {
          MetaOutput *other_output = k->data;

          if (other_output == output)
            continue;

          if (meta_output_kms_can_clone (output, other_output))
            {
              output->n_possible_clones++;
              output->possible_clones = g_renew (MetaOutput *,
                                                 output->possible_clones,
                                                 output->n_possible_clones);
              output->possible_clones[output->n_possible_clones - 1] =
                other_output;
            }
        }
    }
}

static void
init_connectors (MetaMonitorManager *manager,
                 drmModeRes         *resources)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  unsigned int i;

  manager_kms->n_connectors = resources->count_connectors;
  manager_kms->connectors = g_new (drmModeConnector *, manager_kms->n_connectors);
  for (i = 0; i < manager_kms->n_connectors; i++)
    {
      drmModeConnector *drm_connector;

      drm_connector = drmModeGetConnector (manager_kms->fd,
                                           resources->connectors[i]);
      manager_kms->connectors[i] = drm_connector;
    }
}

static void
init_modes (MetaMonitorManager *manager,
            drmModeRes         *resources)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  GHashTable *modes;
  GHashTableIter iter;
  drmModeModeInfo *drm_mode;
  unsigned int i;
  long mode_id;

  /*
   * Gather all modes on all connected connectors.
   */
  modes = g_hash_table_new (drm_mode_hash, (GEqualFunc) meta_drm_mode_equal);
  for (i = 0; i < manager_kms->n_connectors; i++)
    {
      drmModeConnector *drm_connector;

      drm_connector = manager_kms->connectors[i];
      if (drm_connector && drm_connector->connection == DRM_MODE_CONNECTED)
        {
          unsigned int j;

          for (j = 0; j < (unsigned int) drm_connector->count_modes; j++)
            g_hash_table_add (modes, &drm_connector->modes[j]);
        }
    }

  manager->modes = NULL;

  g_hash_table_iter_init (&iter, modes);
  mode_id = 0;
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &drm_mode))
    {
      MetaCrtcMode *mode;

      mode = create_mode (drm_mode, (long) mode_id);
      manager->modes = g_list_append (manager->modes, mode);

      mode_id++;
    }

  g_hash_table_destroy (modes);

  for (i = 0; i < G_N_ELEMENTS (meta_default_drm_mode_infos); i++)
    {
      MetaCrtcMode *mode;

      mode = create_mode (&meta_default_drm_mode_infos[i], (long) mode_id);
      manager->modes = g_list_append (manager->modes, mode);

      mode_id++;
    }
}

static void
init_crtcs (MetaMonitorManager *manager,
            MetaKmsResources   *resources)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  unsigned int i;

  manager->crtcs = NULL;

  for (i = 0; i < (unsigned int) resources->resources->count_crtcs; i++)
    {
      drmModeCrtc *drm_crtc;
      MetaCrtc *crtc;

      drm_crtc = drmModeGetCrtc (manager_kms->fd,
                                 resources->resources->crtcs[i]);

      crtc = meta_create_kms_crtc (manager, drm_crtc, i);

      drmModeFreeCrtc (drm_crtc);

      manager->crtcs = g_list_append (manager->crtcs, crtc);
    }
}

static void
init_outputs (MetaMonitorManager *manager,
              MetaKmsResources   *resources)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  GList *old_outputs;
  unsigned int i;

  old_outputs = manager->outputs;

  manager->outputs = NULL;

  for (i = 0; i < manager_kms->n_connectors; i++)
    {
      drmModeConnector *connector;

      connector = manager_kms->connectors[i];

      if (connector && connector->connection == DRM_MODE_CONNECTED)
        {
          MetaOutput *output;
          MetaOutput *old_output;

          old_output = find_output_by_id (old_outputs, connector->connector_id);
          output = meta_create_kms_output (manager, connector, resources, old_output);
          manager->outputs = g_list_prepend (manager->outputs, output);
        }
    }


  /* Sort the outputs for easier handling in MetaMonitorConfig */
  manager->outputs = g_list_sort (manager->outputs, compare_outputs);

  setup_output_clones (manager);
}

static void
meta_kms_resources_init (MetaKmsResources *resources,
                         int               fd)
{
  drmModeRes *drm_resources;
  unsigned int i;

  drm_resources = drmModeGetResources (fd);
  resources->resources = drm_resources;

  resources->n_encoders = (unsigned int) drm_resources->count_encoders;
  resources->encoders = g_new (drmModeEncoder *, resources->n_encoders);
  for (i = 0; i < resources->n_encoders; i++)
    resources->encoders[i] = drmModeGetEncoder (fd, drm_resources->encoders[i]);
}

static void
meta_kms_resources_release (MetaKmsResources *resources)
{
  unsigned int i;

  for (i = 0; i < resources->n_encoders; i++)
    drmModeFreeEncoder (resources->encoders[i]);
  g_free (resources->encoders);

  drmModeFreeResources (resources->resources);
}

static void
meta_monitor_manager_kms_read_current (MetaMonitorManager *manager)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  MetaKmsResources resources;

  meta_kms_resources_init (&resources, manager_kms->fd);

  manager_kms->max_buffer_width = resources.resources->max_width;
  manager_kms->max_buffer_height = resources.resources->max_height;

  manager->power_save_mode = META_POWER_SAVE_ON;

  /* Note: we must not free the public structures (output, crtc, monitor
     mode and monitor info) here, they must be kept alive until the API
     users are done with them after we emit monitors-changed, and thus
     are freed by the platform-independent layer. */
  free_resources (manager_kms);

  init_connectors (manager, resources.resources);
  init_modes (manager, resources.resources);
  init_crtcs (manager, &resources);
  init_outputs (manager, &resources);

  meta_kms_resources_release (&resources);
}

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

  for (l = manager->outputs; l; l = l->next)
    {
      MetaOutput *output = l->data;

      meta_output_kms_set_power_save_mode (output, state);
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
  for (l = manager->crtcs; l; l = l->next)
    {
      MetaCrtc *crtc = l->data;

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
  for (l = manager->outputs; l; l = l->next)
    {
      MetaOutput *output = l->data;

      if (output->is_dirty)
        {
          output->is_dirty = FALSE;
          continue;
        }

      output->crtc = NULL;
      output->is_primary = FALSE;
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
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);
  drmModeCrtc *kms_crtc;

  kms_crtc = drmModeGetCrtc (manager_kms->fd, crtc->crtc_id);

  *size = kms_crtc->gamma_size;
  *red = g_new (unsigned short, *size);
  *green = g_new (unsigned short, *size);
  *blue = g_new (unsigned short, *size);

  drmModeCrtcGetGamma (manager_kms->fd, crtc->crtc_id, *size, *red, *green, *blue);

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
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (manager);

  drmModeCrtcSetGamma (manager_kms->fd, crtc->crtc_id, size, red, green, blue);
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

static gboolean
kms_event_check (GSource *source)
{
  MetaKmsSource *kms_source = (MetaKmsSource *) source;

  return g_source_query_unix_fd (source, kms_source->fd_tag) & G_IO_IN;
}

static gboolean
kms_event_dispatch (GSource    *source,
                    GSourceFunc callback,
                    gpointer    user_data)
{
  MetaKmsSource *kms_source = (MetaKmsSource *) source;

  meta_monitor_manager_kms_wait_for_flip (kms_source->manager_kms);

  return G_SOURCE_CONTINUE;
}

static GSourceFuncs kms_event_funcs = {
  NULL,
  kms_event_check,
  kms_event_dispatch
};

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

static void
get_crtc_connectors (MetaMonitorManager *manager,
                     MetaCrtc           *crtc,
                     uint32_t          **connectors,
                     unsigned int       *n_connectors)
{
  GArray *connectors_array = g_array_new (FALSE, FALSE, sizeof (uint32_t));
  GList *l;

  for (l = manager->outputs; l; l = l->next)
    {
      MetaOutput *output = l->data;

      if (output->crtc == crtc)
        g_array_append_val (connectors_array, output->winsys_id);
    }

  *n_connectors = connectors_array->len;
  *connectors = (uint32_t *) g_array_free (connectors_array, FALSE);
}

gboolean
meta_monitor_manager_kms_apply_crtc_mode (MetaMonitorManagerKms *manager_kms,
                                          MetaCrtc              *crtc,
                                          int                    x,
                                          int                    y,
                                          uint32_t               fb_id)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_kms);
  uint32_t *connectors;
  unsigned int n_connectors;
  drmModeModeInfo *mode;

  get_crtc_connectors (manager, crtc, &connectors, &n_connectors);

  if (connectors)
    mode = crtc->current_mode->driver_private;
  else
    mode = NULL;

  if (drmModeSetCrtc (manager_kms->fd,
                      crtc->crtc_id,
                      fb_id,
                      x, y,
                      connectors, n_connectors,
                      mode) != 0)
    {
      g_warning ("Failed to set CRTC mode %s: %m", crtc->current_mode->name);
      return FALSE;
    }

  g_free (connectors);

  return TRUE;
}

static void
invoke_flip_closure (GClosure *flip_closure)
{
  GValue param = G_VALUE_INIT;

  g_value_init (&param, G_TYPE_POINTER);
  g_value_set_pointer (&param, flip_closure);
  g_closure_invoke (flip_closure, NULL, 1, &param, NULL);
  g_closure_unref (flip_closure);
}

gboolean
meta_monitor_manager_kms_is_crtc_active (MetaMonitorManagerKms *manager_kms,
                                         MetaCrtc              *crtc)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_kms);
  GList *l;
  gboolean connected_crtc_found;

  if (manager->power_save_mode != META_POWER_SAVE_ON)
    return FALSE;

  connected_crtc_found = FALSE;
  for (l = manager->outputs; l; l = l->next)
    {
      MetaOutput *output = l->data;

      if (output->crtc == crtc)
        {
          connected_crtc_found = TRUE;
          break;
        }
    }

  if (!connected_crtc_found)
    return FALSE;

  return TRUE;
}

gboolean
meta_monitor_manager_kms_flip_crtc (MetaMonitorManagerKms *manager_kms,
                                    MetaCrtc              *crtc,
                                    int                    x,
                                    int                    y,
                                    uint32_t               fb_id,
                                    GClosure              *flip_closure,
                                    gboolean              *fb_in_use)
{
  MetaMonitorManager *manager = META_MONITOR_MANAGER (manager_kms);
  uint32_t *connectors;
  unsigned int n_connectors;
  int ret = -1;

  g_assert (manager->power_save_mode == META_POWER_SAVE_ON);

  get_crtc_connectors (manager, crtc, &connectors, &n_connectors);
  g_assert (n_connectors > 0);

  if (!manager_kms->page_flips_not_supported)
    {
      ret = drmModePageFlip (manager_kms->fd,
                             crtc->crtc_id,
                             fb_id,
                             DRM_MODE_PAGE_FLIP_EVENT,
                             flip_closure);
      if (ret != 0 && ret != -EACCES)
        {
          g_warning ("Failed to flip: %s", strerror (-ret));
          manager_kms->page_flips_not_supported = TRUE;
        }
    }

  if (manager_kms->page_flips_not_supported)
    {
      if (meta_monitor_manager_kms_apply_crtc_mode (manager_kms,
                                                    crtc,
                                                    x, y,
                                                    fb_id))
        {
          *fb_in_use = TRUE;
          return FALSE;
        }
    }

  if (ret != 0)
    return FALSE;

  *fb_in_use = TRUE;
  g_closure_ref (flip_closure);

  return TRUE;
}

static void
page_flip_handler (int fd,
                   unsigned int frame,
                   unsigned int sec,
                   unsigned int usec,
                   void *data)
{
  GClosure *flip_closure = data;

  invoke_flip_closure (flip_closure);
}

void
meta_monitor_manager_kms_wait_for_flip (MetaMonitorManagerKms *manager_kms)
{
  drmEventContext evctx;

  if (manager_kms->page_flips_not_supported)
    return;

  memset (&evctx, 0, sizeof evctx);
  evctx.version = DRM_EVENT_CONTEXT_VERSION;
  evctx.page_flip_handler = page_flip_handler;
  drmHandleEvent (manager_kms->fd, &evctx);
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
  MetaBackend *backend = meta_get_backend ();
  MetaSettings *settings = meta_backend_get_settings (backend);
  MetaRenderer *renderer = meta_backend_get_renderer (backend);
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (renderer);
  MetaMonitorManagerCapability capabilities =
    META_MONITOR_MANAGER_CAPABILITY_NONE;

  if (meta_settings_is_experimental_feature_enabled (
        settings,
        META_EXPERIMENTAL_FEATURE_SCALE_MONITOR_FRAMEBUFFER))
    capabilities |= META_MONITOR_MANAGER_CAPABILITY_LAYOUT_MODE;

  switch (meta_renderer_native_get_mode (renderer_native))
    {
    case META_RENDERER_NATIVE_MODE_GBM:
      capabilities |= META_MONITOR_MANAGER_CAPABILITY_MIRRORING;
      break;
#ifdef HAVE_EGL_DEVICE
    case META_RENDERER_NATIVE_MODE_EGL_DEVICE:
      break;
#endif
    }

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

  *max_width = manager_kms->max_buffer_width;
  *max_height = manager_kms->max_buffer_height;

  return TRUE;
}

static MetaLogicalMonitorLayoutMode
meta_monitor_manager_kms_get_default_layout_mode (MetaMonitorManager *manager)
{
  MetaBackend *backend = meta_get_backend ();
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

static guint
count_devices_with_connectors (const char *seat_id,
                               GList      *devices)
{
  g_autoptr (GHashTable) cards = NULL;
  GList *l;

  cards = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
  for (l = devices; l != NULL; l = l->next)
    {
      GUdevDevice *device = l->data;
      g_autoptr (GUdevDevice) parent_device = NULL;
      const gchar *parent_device_type = NULL;
      const gchar *parent_device_name = NULL;
      const gchar *card_seat;

      /* Filter out the real card devices, we only care about the connectors. */
      if (g_udev_device_get_device_type (device) != G_UDEV_DEVICE_TYPE_NONE)
        continue;

      /* Only connectors have a modes attribute. */
      if (!g_udev_device_has_sysfs_attr (device, "modes"))
        continue;

      parent_device = g_udev_device_get_parent (device);

      if (g_udev_device_get_device_type (parent_device) ==
          G_UDEV_DEVICE_TYPE_CHAR)
        parent_device_type = g_udev_device_get_property (parent_device,
                                                         "DEVTYPE");

      if (g_strcmp0 (parent_device_type, DRM_CARD_UDEV_DEVICE_TYPE) != 0)
        continue;

      card_seat = g_udev_device_get_property (parent_device, "ID_SEAT");

      if (!card_seat)
        card_seat = "seat0";

      if (g_strcmp0 (seat_id, card_seat) != 0)
        continue;

      parent_device_name = g_udev_device_get_name (parent_device);
      g_hash_table_insert (cards,
                           (gpointer) parent_device_name ,
                           g_steal_pointer (&parent_device));
    }

  return g_hash_table_size (cards);
}

static char *
get_primary_gpu_path (MetaMonitorManagerKms *manager_kms)
{
  MetaBackend *backend = meta_get_backend ();
  MetaBackendNative *backend_native = META_BACKEND_NATIVE (backend);
  MetaLauncher *launcher = meta_backend_native_get_launcher (backend_native);
  g_autoptr (GUdevEnumerator) enumerator = NULL;
  const char *seat_id;
  char *path = NULL;
  GList *devices;
  GList *l;

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
    goto out;

  seat_id = meta_launcher_get_seat_id (launcher);

  /*
   * For now, fail on systems where some of the connectors are connected to
   * secondary gpus.
   *
   * https://bugzilla.gnome.org/show_bug.cgi?id=771442
   */
  if (g_getenv ("MUTTER_ALLOW_HYBRID_GPUS") == NULL)
    {
      unsigned int num_devices;

      num_devices = count_devices_with_connectors (seat_id, devices);
      if (num_devices != 1)
        goto out;
    }

  for (l = devices; l; l = l->next)
    {
      GUdevDevice *dev = l->data;
      g_autoptr (GUdevDevice) platform_device = NULL;
      g_autoptr (GUdevDevice) pci_device = NULL;
      const char *device_type;
      const char *device_seat;

      /* Filter out devices that are not character device, like card0-VGA-1. */
      if (g_udev_device_get_device_type (dev) != G_UDEV_DEVICE_TYPE_CHAR)
        continue;

      device_type = g_udev_device_get_property (dev, "DEVTYPE");
      if (g_strcmp0 (device_type, DRM_CARD_UDEV_DEVICE_TYPE) != 0)
        continue;

      device_seat = g_udev_device_get_property (dev, "ID_SEAT");
      if (!device_seat)
        {
          /* When ID_SEAT is not set, it means seat0. */
          device_seat = "seat0";
        }
      else if (g_strcmp0 (device_seat, "seat0") != 0)
        {
          /*
           * If the device has been explicitly assigned other seat
           * than seat0, it is probably the right device to use.
           */
          path = g_strdup (g_udev_device_get_device_file (dev));
          break;
        }

      /* Skip devices that do not belong to our seat. */
      if (g_strcmp0 (seat_id, device_seat))
        continue;

      platform_device = g_udev_device_get_parent_with_subsystem (dev,
                                                                 "platform",
                                                                 NULL);
      if (platform_device != NULL)
        {
          path = g_strdup (g_udev_device_get_device_file (dev));
          break;
        }

      pci_device = g_udev_device_get_parent_with_subsystem (dev, "pci", NULL);
      if (pci_device != NULL)
        {
          int boot_vga;

          boot_vga = g_udev_device_get_sysfs_attr_as_int (pci_device,
                                                          "boot_vga");
          if (boot_vga == 1)
            {
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
open_primary_gpu (MetaMonitorManagerKms *manager_kms,
                  int                   *fd_out,
                  char                 **kms_file_path_out,
                  GError               **error)
{
  MetaBackend *backend = meta_get_backend ();
  MetaBackendNative *backend_native = META_BACKEND_NATIVE (backend);
  MetaLauncher *launcher = meta_backend_native_get_launcher (backend_native);
  g_autofree char *path = NULL;
  int fd;

  path = get_primary_gpu_path (manager_kms);
  if (!path)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "Could not find drm kms device");
      return FALSE;
    }

  fd = meta_launcher_open_restricted (launcher, path, error);
  if (fd == -1)
    return FALSE;

  *fd_out = fd;
  *kms_file_path_out = g_steal_pointer (&path);

  return TRUE;
}

static gboolean
meta_monitor_manager_kms_initable_init (GInitable    *initable,
                                        GCancellable *cancellable,
                                        GError      **error)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (initable);
  GSource *source;
  const char *subsystems[2] = { "drm", NULL };

  manager_kms->udev = g_udev_client_new (subsystems);

  if (!open_primary_gpu (manager_kms,
                         &manager_kms->fd,
                         &manager_kms->file_path,
                         error))
    return FALSE;

  drmSetClientCap (manager_kms->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

  meta_monitor_manager_kms_connect_uevent_handler (manager_kms);

  source = g_source_new (&kms_event_funcs, sizeof (MetaKmsSource));
  manager_kms->source = (MetaKmsSource *) source;
  manager_kms->source->fd_tag = g_source_add_unix_fd (source,
                                                      manager_kms->fd,
                                                      G_IO_IN | G_IO_ERR);
  manager_kms->source->manager_kms = manager_kms;
  g_source_attach (source, NULL);

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
meta_monitor_manager_kms_finalize (GObject *object)
{
  MetaMonitorManagerKms *manager_kms = META_MONITOR_MANAGER_KMS (object);
  MetaBackend *backend = meta_get_backend ();
  MetaBackendNative *backend_native = META_BACKEND_NATIVE (backend);
  MetaLauncher *launcher = meta_backend_native_get_launcher (backend_native);

  if (manager_kms->fd != -1)
    meta_launcher_close_restricted (launcher, manager_kms->fd);
  g_clear_pointer (&manager_kms->file_path, g_free);

  free_resources (manager_kms);
  g_source_destroy ((GSource *) manager_kms->source);

  G_OBJECT_CLASS (meta_monitor_manager_kms_parent_class)->finalize (object);
}

static void
meta_monitor_manager_kms_init (MetaMonitorManagerKms *manager_kms)
{
  manager_kms->fd = -1;
}

static void
meta_monitor_manager_kms_class_init (MetaMonitorManagerKmsClass *klass)
{
  MetaMonitorManagerClass *manager_class = META_MONITOR_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = meta_monitor_manager_kms_dispose;
  object_class->finalize = meta_monitor_manager_kms_finalize;

  manager_class->read_current = meta_monitor_manager_kms_read_current;
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
