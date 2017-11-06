/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
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

#include "backends/native/meta-gpu-kms.h"

#include <drm.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "backends/meta-crtc.h"
#include "backends/meta-monitor-manager-private.h"
#include "backends/meta-output.h"
#include "backends/native/meta-backend-native.h"
#include "backends/native/meta-crtc-kms.h"
#include "backends/native/meta-launcher.h"
#include "backends/native/meta-output-kms.h"
#include "backends/native/meta-default-modes.h"

typedef struct _MetaKmsSource
{
  GSource source;

  gpointer fd_tag;
  MetaGpuKms *gpu_kms;
} MetaKmsSource;

struct _MetaGpuKms
{
  MetaGpu parent;

  int fd;
  char *file_path;
  GSource *source;

  drmModeConnector **connectors;
  unsigned int n_connectors;

  int max_buffer_width;
  int max_buffer_height;

  gboolean page_flips_not_supported;
};

G_DEFINE_TYPE (MetaGpuKms, meta_gpu_kms, META_TYPE_GPU)

static gboolean
kms_event_check (GSource *source)
{
  MetaKmsSource *kms_source = (MetaKmsSource *) source;

  return g_source_query_unix_fd (source, kms_source->fd_tag) & G_IO_IN;
}

static gboolean
kms_event_dispatch (GSource     *source,
                    GSourceFunc  callback,
                    gpointer     user_data)
{
  MetaKmsSource *kms_source = (MetaKmsSource *) source;

  meta_gpu_kms_wait_for_flip (kms_source->gpu_kms, NULL);

  return G_SOURCE_CONTINUE;
}

static GSourceFuncs kms_event_funcs = {
  NULL,
  kms_event_check,
  kms_event_dispatch
};

static void
get_crtc_drm_connectors (MetaGpu       *gpu,
                         MetaCrtc      *crtc,
                         uint32_t     **connectors,
                         unsigned int  *n_connectors)
{
  GArray *connectors_array = g_array_new (FALSE, FALSE, sizeof (uint32_t));
  GList *l;

  for (l = meta_gpu_get_outputs (gpu); l; l = l->next)
    {
      MetaOutput *output = l->data;

      if (output->crtc == crtc)
        g_array_append_val (connectors_array, output->winsys_id);
    }

  *n_connectors = connectors_array->len;
  *connectors = (uint32_t *) g_array_free (connectors_array, FALSE);
}

gboolean
meta_gpu_kms_apply_crtc_mode (MetaGpuKms *gpu_kms,
                              MetaCrtc   *crtc,
                              int         x,
                              int         y,
                              uint32_t    fb_id)
{
  MetaGpu *gpu = meta_crtc_get_gpu (crtc);
  int kms_fd = meta_gpu_kms_get_fd (gpu_kms);
  uint32_t *connectors;
  unsigned int n_connectors;
  drmModeModeInfo *mode;

  get_crtc_drm_connectors (gpu, crtc, &connectors, &n_connectors);

  if (connectors)
    mode = crtc->current_mode->driver_private;
  else
    mode = NULL;

  if (drmModeSetCrtc (kms_fd,
                      crtc->crtc_id,
                      fb_id,
                      x, y,
                      connectors, n_connectors,
                      mode) != 0)
    {
      g_warning ("Failed to set CRTC mode %s: %m", crtc->current_mode->name);
      g_free (connectors);
      return FALSE;
    }

  g_free (connectors);

  return TRUE;
}

static void
invoke_flip_closure (GClosure   *flip_closure,
                     MetaGpuKms *gpu_kms)
{
  GValue params[] = {
    G_VALUE_INIT,
    G_VALUE_INIT
  };

  g_value_init (&params[0], G_TYPE_POINTER);
  g_value_set_pointer (&params[0], flip_closure);
  g_value_init (&params[1], G_TYPE_OBJECT);
  g_value_set_object (&params[1], gpu_kms);
  g_closure_invoke (flip_closure, NULL, 2, params, NULL);
  g_closure_unref (flip_closure);
}

gboolean
meta_gpu_kms_is_crtc_active (MetaGpuKms *gpu_kms,
                             MetaCrtc   *crtc)
{
  MetaGpu *gpu = META_GPU (gpu_kms);
  MetaMonitorManager *monitor_manager = meta_gpu_get_monitor_manager (gpu);
  GList *l;
  gboolean connected_crtc_found;

  g_assert (meta_crtc_get_gpu (crtc) == META_GPU (gpu_kms));

  if (monitor_manager->power_save_mode != META_POWER_SAVE_ON)
    return FALSE;

  connected_crtc_found = FALSE;
  for (l = meta_gpu_get_outputs (gpu); l; l = l->next)
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

typedef struct _GpuClosureContainer
{
  GClosure *flip_closure;
  MetaGpuKms *gpu_kms;
} GpuClosureContainer;

gboolean
meta_gpu_kms_flip_crtc (MetaGpuKms *gpu_kms,
                        MetaCrtc   *crtc,
                        int         x,
                        int         y,
                        uint32_t    fb_id,
                        GClosure   *flip_closure,
                        gboolean   *fb_in_use)
{
  MetaGpu *gpu = META_GPU (gpu_kms);
  MetaMonitorManager *monitor_manager = meta_gpu_get_monitor_manager (gpu);
  uint32_t *connectors;
  unsigned int n_connectors;
  int ret = -1;

  g_assert (meta_crtc_get_gpu (crtc) == gpu);
  g_assert (monitor_manager->power_save_mode == META_POWER_SAVE_ON);

  get_crtc_drm_connectors (gpu, crtc, &connectors, &n_connectors);
  g_assert (n_connectors > 0);
  g_free (connectors);

  if (!gpu_kms->page_flips_not_supported)
    {
      GpuClosureContainer *closure_container;
      int kms_fd = meta_gpu_kms_get_fd (gpu_kms);

      closure_container = g_new0 (GpuClosureContainer, 1);
      *closure_container = (GpuClosureContainer) {
        .flip_closure = flip_closure,
        .gpu_kms = gpu_kms
      };

      ret = drmModePageFlip (kms_fd,
                             crtc->crtc_id,
                             fb_id,
                             DRM_MODE_PAGE_FLIP_EVENT,
                             closure_container);
      if (ret != 0 && ret != -EACCES)
        {
          g_free (closure_container);
          g_warning ("Failed to flip: %s", strerror (-ret));
          gpu_kms->page_flips_not_supported = TRUE;
        }
    }

  if (gpu_kms->page_flips_not_supported)
    {
      if (meta_gpu_kms_apply_crtc_mode (gpu_kms, crtc, x, y, fb_id))
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
page_flip_handler (int           fd,
                   unsigned int  frame,
                   unsigned int  sec,
                   unsigned int  usec,
                   void         *user_data)
{
  GpuClosureContainer *closure_container = user_data;
  GClosure *flip_closure = closure_container->flip_closure;
  MetaGpuKms *gpu_kms = closure_container->gpu_kms;

  invoke_flip_closure (flip_closure, gpu_kms);
  g_free (closure_container);
}

gboolean
meta_gpu_kms_wait_for_flip (MetaGpuKms *gpu_kms,
                            GError    **error)
{
  drmEventContext evctx;

  if (gpu_kms->page_flips_not_supported)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Page flips not supported");
      return FALSE;
    }

  memset (&evctx, 0, sizeof evctx);
  evctx.version = DRM_EVENT_CONTEXT_VERSION;
  evctx.page_flip_handler = page_flip_handler;

  while (TRUE)
    {
      if (drmHandleEvent (gpu_kms->fd, &evctx) != 0)
        {
          struct pollfd pfd;
          int ret;

          if (errno != EAGAIN)
            {
              g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                   strerror (errno));
              return FALSE;
            }

          pfd.fd = gpu_kms->fd;
          pfd.events = POLL_IN | POLL_ERR;
          do
            {
              ret = poll (&pfd, 1, -1);
            }
          while (ret == -1 && errno == EINTR);
        }
      else
        {
          break;
        }
    }

  return TRUE;
}

void
meta_gpu_kms_get_max_buffer_size (MetaGpuKms *gpu_kms,
                                  int        *max_width,
                                  int        *max_height)
{
  *max_width = gpu_kms->max_buffer_width;
  *max_height = gpu_kms->max_buffer_height;
}

int
meta_gpu_kms_get_fd (MetaGpuKms *gpu_kms)
{
  return gpu_kms->fd;
}

const char *
meta_gpu_kms_get_file_path (MetaGpuKms *gpu_kms)
{
  return gpu_kms->file_path;
}

void
meta_gpu_kms_set_power_save_mode (MetaGpuKms *gpu_kms,
                                  uint64_t    state)
{
  GList *l;

  for (l = meta_gpu_get_outputs (META_GPU (gpu_kms)); l; l = l->next)
    {
      MetaOutput *output = l->data;

      meta_output_kms_set_power_save_mode (output, state);
    }
}

static void
free_resources (MetaGpuKms *gpu_kms)
{
  unsigned i;

  for (i = 0; i < gpu_kms->n_connectors; i++)
    drmModeFreeConnector (gpu_kms->connectors[i]);

  g_free (gpu_kms->connectors);
}

static int
compare_outputs (gconstpointer one,
                 gconstpointer two)
{
  const MetaOutput *o_one = one, *o_two = two;

  return strcmp (o_one->name, o_two->name);
}

static void
meta_crtc_mode_destroy_notify (MetaCrtcMode *mode)
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

  /*
   * We don't include the name in the hash because it's generally
   * derived from the other fields (hdisplay, vdisplay and flags)
   */

  hash ^= mode->clock;
  hash ^= mode->hdisplay ^ mode->hsync_start ^ mode->hsync_end;
  hash ^= mode->vdisplay ^ mode->vsync_start ^ mode->vsync_end;
  hash ^= mode->vrefresh;
  hash ^= mode->flags ^ mode->type;

  return hash;
}

MetaCrtcMode *
meta_gpu_kms_get_mode_from_drm_mode (MetaGpuKms            *gpu_kms,
                                     const drmModeModeInfo *drm_mode)
{
  MetaGpu *gpu = META_GPU (gpu_kms);
  GList *l;

  for (l = meta_gpu_get_modes (gpu); l; l = l->next)
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
  mode->driver_notify = (GDestroyNotify) meta_crtc_mode_destroy_notify;

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
setup_output_clones (MetaGpu *gpu)
{
  GList *l;

  for (l = meta_gpu_get_outputs (gpu); l; l = l->next)
    {
      MetaOutput *output = l->data;
      GList *k;

      for (k = meta_gpu_get_outputs (gpu); k; k = k->next)
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
init_connectors (MetaGpuKms *gpu_kms,
                 drmModeRes *resources)
{
  unsigned int i;

  gpu_kms->n_connectors = resources->count_connectors;
  gpu_kms->connectors = g_new (drmModeConnector *, gpu_kms->n_connectors);
  for (i = 0; i < gpu_kms->n_connectors; i++)
    {
      drmModeConnector *drm_connector;

      drm_connector = drmModeGetConnector (gpu_kms->fd,
                                           resources->connectors[i]);
      gpu_kms->connectors[i] = drm_connector;
    }
}

static void
init_modes (MetaGpuKms *gpu_kms,
            drmModeRes *resources)
{
  MetaGpu *gpu = META_GPU (gpu_kms);
  GHashTable *modes_table;
  GList *modes;
  GHashTableIter iter;
  drmModeModeInfo *drm_mode;
  unsigned int i;
  long mode_id;

  /*
   * Gather all modes on all connected connectors.
   */
  modes_table = g_hash_table_new (drm_mode_hash, (GEqualFunc) meta_drm_mode_equal);
  for (i = 0; i < gpu_kms->n_connectors; i++)
    {
      drmModeConnector *drm_connector;

      drm_connector = gpu_kms->connectors[i];
      if (drm_connector && drm_connector->connection == DRM_MODE_CONNECTED)
        {
          unsigned int j;

          for (j = 0; j < (unsigned int) drm_connector->count_modes; j++)
            g_hash_table_add (modes_table, &drm_connector->modes[j]);
        }
    }

  modes = NULL;

  g_hash_table_iter_init (&iter, modes_table);
  mode_id = 0;
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &drm_mode))
    {
      MetaCrtcMode *mode;

      mode = create_mode (drm_mode, (long) mode_id);
      modes = g_list_append (modes, mode);

      mode_id++;
    }

  g_hash_table_destroy (modes_table);

  for (i = 0; i < G_N_ELEMENTS (meta_default_drm_mode_infos); i++)
    {
      MetaCrtcMode *mode;

      mode = create_mode (&meta_default_drm_mode_infos[i], (long) mode_id);
      modes = g_list_append (modes, mode);

      mode_id++;
    }

  meta_gpu_take_modes (gpu, modes);
}

static void
init_crtcs (MetaGpuKms       *gpu_kms,
            MetaKmsResources *resources)
{
  MetaGpu *gpu = META_GPU (gpu_kms);
  GList *crtcs;
  unsigned int i;

  crtcs = NULL;

  for (i = 0; i < (unsigned int) resources->resources->count_crtcs; i++)
    {
      drmModeCrtc *drm_crtc;
      MetaCrtc *crtc;

      drm_crtc = drmModeGetCrtc (gpu_kms->fd,
                                 resources->resources->crtcs[i]);

      crtc = meta_create_kms_crtc (gpu_kms, drm_crtc, i);

      drmModeFreeCrtc (drm_crtc);

      crtcs = g_list_append (crtcs, crtc);
    }

  meta_gpu_take_crtcs (gpu, crtcs);
}

static void
init_outputs (MetaGpuKms       *gpu_kms,
              MetaKmsResources *resources)
{
  MetaGpu *gpu = META_GPU (gpu_kms);
  GList *old_outputs;
  GList *outputs;
  unsigned int i;

  old_outputs = meta_gpu_get_outputs (gpu);

  outputs = NULL;

  for (i = 0; i < gpu_kms->n_connectors; i++)
    {
      drmModeConnector *connector;

      connector = gpu_kms->connectors[i];

      if (connector && connector->connection == DRM_MODE_CONNECTED)
        {
          MetaOutput *output;
          MetaOutput *old_output;

          old_output = find_output_by_id (old_outputs, connector->connector_id);
          output = meta_create_kms_output (gpu_kms, connector, resources,
                                           old_output);
          outputs = g_list_prepend (outputs, output);
        }
    }


  /* Sort the outputs for easier handling in MetaMonitorConfig */
  outputs = g_list_sort (outputs, compare_outputs);
  meta_gpu_take_outputs (gpu, outputs);

  setup_output_clones (gpu);
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

static gboolean
meta_gpu_kms_read_current (MetaGpu  *gpu,
                           GError  **error)
{
  MetaGpuKms *gpu_kms = META_GPU_KMS (gpu);
  MetaMonitorManager *monitor_manager =
    meta_gpu_get_monitor_manager (gpu);
  MetaKmsResources resources;

  meta_kms_resources_init (&resources, gpu_kms->fd);

  gpu_kms->max_buffer_width = resources.resources->max_width;
  gpu_kms->max_buffer_height = resources.resources->max_height;

  monitor_manager->power_save_mode = META_POWER_SAVE_ON;

  /* Note: we must not free the public structures (output, crtc, monitor
     mode and monitor info) here, they must be kept alive until the API
     users are done with them after we emit monitors-changed, and thus
     are freed by the platform-independent layer. */
  free_resources (gpu_kms);

  init_connectors (gpu_kms, resources.resources);
  init_modes (gpu_kms, resources.resources);
  init_crtcs (gpu_kms, &resources);
  init_outputs (gpu_kms, &resources);

  meta_kms_resources_release (&resources);

  return TRUE;
}

MetaGpuKms *
meta_gpu_kms_new (MetaMonitorManagerKms  *monitor_manager_kms,
                  const char             *kms_file_path,
                  GError                **error)
{
  MetaMonitorManager *monitor_manager =
    META_MONITOR_MANAGER (monitor_manager_kms);
  MetaBackend *backend = meta_monitor_manager_get_backend (monitor_manager);
  MetaLauncher *launcher =
    meta_backend_native_get_launcher (META_BACKEND_NATIVE (backend));
  GSource *source;
  MetaKmsSource *kms_source;
  MetaGpuKms *gpu_kms;
  int kms_fd;

  kms_fd = meta_launcher_open_restricted (launcher, kms_file_path, error);
  if (kms_fd == -1)
    return FALSE;

  gpu_kms = g_object_new (META_TYPE_GPU_KMS,
                          "monitor-manager", monitor_manager_kms,
                          NULL);

  gpu_kms->fd = kms_fd;
  gpu_kms->file_path = g_strdup (kms_file_path);

  drmSetClientCap (gpu_kms->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

  source = g_source_new (&kms_event_funcs, sizeof (MetaKmsSource));
  kms_source = (MetaKmsSource *) source;
  kms_source->fd_tag = g_source_add_unix_fd (source,
                                             gpu_kms->fd,
                                             G_IO_IN | G_IO_ERR);
  kms_source->gpu_kms = gpu_kms;

  gpu_kms->source = source;
  g_source_attach (gpu_kms->source, NULL);

  return gpu_kms;
}

static void
meta_gpu_kms_finalize (GObject *object)
{
  MetaGpuKms *gpu_kms = META_GPU_KMS (object);
  MetaMonitorManager *monitor_manager =
    meta_gpu_get_monitor_manager (META_GPU (gpu_kms));
  MetaBackend *backend = meta_monitor_manager_get_backend (monitor_manager);
  MetaBackendNative *backend_native = META_BACKEND_NATIVE (backend);
  MetaLauncher *launcher = meta_backend_native_get_launcher (backend_native);

  if (gpu_kms->fd != -1)
    meta_launcher_close_restricted (launcher, gpu_kms->fd);
  g_clear_pointer (&gpu_kms->file_path, g_free);

  g_source_destroy (gpu_kms->source);

  free_resources (gpu_kms);

  G_OBJECT_CLASS (meta_gpu_kms_parent_class)->finalize (object);
}

static void
meta_gpu_kms_init (MetaGpuKms *gpu_kms)
{
  gpu_kms->fd = -1;
}

static void
meta_gpu_kms_class_init (MetaGpuKmsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MetaGpuClass *gpu_class = META_GPU_CLASS (klass);

  object_class->finalize = meta_gpu_kms_finalize;

  gpu_class->read_current = meta_gpu_kms_read_current;
}
