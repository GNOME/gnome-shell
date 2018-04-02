/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2016 Red Hat
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *   Rob Bradford <rob@linux.intel.com> (from cogl-winsys-egl-kms.c)
 *   Kristian Høgsberg (from eglkms.c)
 *   Benjamin Franzke (from eglkms.c)
 *   Robert Bragg <robert@linux.intel.com> (from cogl-winsys-egl-kms.c)
 *   Neil Roberts <neil@linux.intel.com> (from cogl-winsys-egl-kms.c)
 *   Jonas Ådahl <jadahl@redhat.com>
 *
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <gbm.h>
#include <gio/gio.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <drm_fourcc.h>

#include "backends/meta-backend-private.h"
#include "backends/meta-crtc.h"
#include "backends/meta-gles3.h"
#include "backends/meta-egl.h"
#include "backends/meta-egl-ext.h"
#include "backends/meta-logical-monitor.h"
#include "backends/meta-output.h"
#include "backends/meta-renderer-view.h"
#include "backends/native/meta-crtc-kms.h"
#include "backends/native/meta-gpu-kms.h"
#include "backends/native/meta-monitor-manager-kms.h"
#include "backends/native/meta-renderer-native.h"
#include "backends/native/meta-renderer-native-gles3.h"
#include "cogl/cogl.h"
#include "core/boxes-private.h"

#ifndef EGL_DRM_MASTER_FD_EXT
#define EGL_DRM_MASTER_FD_EXT 0x333C
#endif

enum
{
  PROP_0,

  PROP_MONITOR_MANAGER,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

typedef enum _MetaSharedFramebufferCopyMode
{
  META_SHARED_FRAMEBUFFER_COPY_MODE_GPU,
  META_SHARED_FRAMEBUFFER_COPY_MODE_CPU
} MetaSharedFramebufferCopyMode;

typedef struct _MetaRendererNativeGpuData
{
  MetaRendererNative *renderer_native;

  struct {
    struct gbm_device *device;
  } gbm;

#ifdef HAVE_EGL_DEVICE
  struct {
    EGLDeviceEXT device;

    gboolean no_egl_output_drm_flip_event;
  } egl;
#endif

  MetaRendererNativeMode mode;

  gboolean no_add_fb2;

  EGLDisplay egl_display;

  /*
   * Fields used for blitting iGPU framebuffer content onto dGPU framebuffers.
   */
  struct {
    MetaSharedFramebufferCopyMode copy_mode;

    /* For GPU blit mode */
    EGLContext egl_context;
    EGLConfig egl_config;
  } secondary;
} MetaRendererNativeGpuData;

typedef struct _MetaDumbBuffer
{
  uint32_t fb_id;
  uint32_t handle;
  void *map;
  uint64_t map_size;
} MetaDumbBuffer;

typedef struct _MetaOnscreenNativeSecondaryGpuState
{
  MetaGpuKms *gpu_kms;
  MetaRendererNativeGpuData *renderer_gpu_data;

  EGLSurface egl_surface;

  struct {
    struct gbm_surface *surface;
    uint32_t current_fb_id;
    uint32_t next_fb_id;
    struct gbm_bo *current_bo;
    struct gbm_bo *next_bo;
  } gbm;

  struct {
    MetaDumbBuffer *dumb_fb;
    MetaDumbBuffer dumb_fbs[2];
  } cpu;

  int pending_flips;
} MetaOnscreenNativeSecondaryGpuState;

typedef struct _MetaOnscreenNative
{
  MetaRendererNative *renderer_native;
  MetaGpuKms *render_gpu;
  MetaLogicalMonitor *logical_monitor;

  GHashTable *secondary_gpu_states;

  struct {
    struct gbm_surface *surface;
    uint32_t current_fb_id;
    uint32_t next_fb_id;
    struct gbm_bo *current_bo;
    struct gbm_bo *next_bo;
  } gbm;

#ifdef HAVE_EGL_DEVICE
  struct {
    EGLStreamKHR stream;

    MetaDumbBuffer dumb_fb;
  } egl;
#endif

  gboolean pending_queue_swap_notify;
  gboolean pending_swap_notify;

  gboolean pending_set_crtc;

  int64_t pending_queue_swap_notify_frame_count;
  int64_t pending_swap_notify_frame_count;

  MetaRendererView *view;
  int total_pending_flips;
} MetaOnscreenNative;

struct _MetaRendererNative
{
  MetaRenderer parent;

  MetaMonitorManagerKms *monitor_manager_kms;
  MetaGles3 *gles3;

  GHashTable *gpu_datas;

  CoglClosure *swap_notify_idle;

  int64_t frame_counter;
  gboolean pending_unset_disabled_crtcs;
};

static void
initable_iface_init (GInitableIface *initable_iface);

G_DEFINE_TYPE_WITH_CODE (MetaRendererNative,
                         meta_renderer_native,
                         META_TYPE_RENDERER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init))

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;
static const CoglWinsysVtable *parent_vtable;

static void
release_dumb_fb (MetaDumbBuffer *dumb_fb,
                 MetaGpuKms     *gpu_kms);

static gboolean
init_dumb_fb (MetaDumbBuffer *dumb_fb,
              MetaGpuKms     *gpu_kms,
              int             width,
              int             height,
              uint32_t        format,
              GError        **error);

static MetaEgl *
meta_renderer_native_get_egl (MetaRendererNative *renderer_native);

static void
free_current_secondary_bo (MetaGpuKms                          *gpu_kms,
                           MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state);

static void
free_next_secondary_bo (MetaGpuKms                          *gpu_kms,
                        MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state);

static void
meta_renderer_native_gpu_data_free (MetaRendererNativeGpuData *renderer_gpu_data)
{
  MetaRendererNative *renderer_native = renderer_gpu_data->renderer_native;
  MetaEgl *egl = meta_renderer_native_get_egl (renderer_native);

  if (renderer_gpu_data->egl_display != EGL_NO_DISPLAY)
    meta_egl_terminate (egl, renderer_gpu_data->egl_display, NULL);

  g_clear_pointer (&renderer_gpu_data->gbm.device, gbm_device_destroy);
  g_free (renderer_gpu_data);
}

static MetaRendererNativeGpuData *
meta_renderer_native_get_gpu_data (MetaRendererNative *renderer_native,
                                   MetaGpuKms         *gpu_kms)
{
  return g_hash_table_lookup (renderer_native->gpu_datas, gpu_kms);
}

static MetaRendererNative *
meta_renderer_native_from_gpu (MetaGpuKms *gpu_kms)
{
  MetaMonitorManager *monitor_manager =
    meta_gpu_get_monitor_manager (META_GPU (gpu_kms));
  MetaBackend *backend = meta_monitor_manager_get_backend (monitor_manager);

  return META_RENDERER_NATIVE (meta_backend_get_renderer (backend));
}

struct gbm_device *
meta_gbm_device_from_gpu (MetaGpuKms *gpu_kms)
{
  MetaRendererNative *renderer_native = meta_renderer_native_from_gpu (gpu_kms);
  MetaRendererNativeGpuData *renderer_gpu_data;

  renderer_gpu_data = meta_renderer_native_get_gpu_data (renderer_native,
                                                         gpu_kms);

  return renderer_gpu_data->gbm.device;
}

static MetaRendererNativeGpuData *
meta_create_renderer_native_gpu_data (MetaGpuKms *gpu_kms)
{
  return g_new0 (MetaRendererNativeGpuData, 1);
}

static MetaOnscreenNativeSecondaryGpuState *
get_secondary_gpu_state (CoglOnscreen *onscreen,
                         MetaGpuKms   *gpu_kms)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;

  return g_hash_table_lookup (onscreen_native->secondary_gpu_states, gpu_kms);
}

static MetaEgl *
meta_renderer_native_get_egl (MetaRendererNative *renderer_native)
{
  MetaMonitorManager *monitor_manager =
    META_MONITOR_MANAGER (renderer_native->monitor_manager_kms);
  MetaBackend *backend = meta_monitor_manager_get_backend (monitor_manager);

  return meta_backend_get_egl (backend);
}

static MetaEgl *
meta_onscreen_native_get_egl (MetaOnscreenNative *onscreen_native)
{
  return meta_renderer_native_get_egl (onscreen_native->renderer_native);
}

static GArray *
get_supported_kms_modifiers (CoglOnscreen *onscreen,
                             MetaGpu      *gpu,
                             uint32_t      format)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaLogicalMonitor *logical_monitor = onscreen_native->logical_monitor;
  GArray *modifiers;
  GArray *base_mods;
  GList *l_crtc;
  MetaCrtc *base_crtc = NULL;
  GList *other_crtcs = NULL;
  unsigned int i;

  if (!logical_monitor)
    return NULL;

  /* Find our base CRTC to intersect against. */
  for (l_crtc = meta_gpu_get_crtcs (gpu); l_crtc; l_crtc = l_crtc->next)
    {
      MetaCrtc *crtc = l_crtc->data;

      if (crtc->logical_monitor != logical_monitor)
        continue;

      if (!base_crtc)
        base_crtc = crtc;
      else if (crtc == base_crtc)
        continue;
      else if (g_list_index (other_crtcs, crtc) == -1)
        other_crtcs = g_list_append (other_crtcs, crtc);
    }

  if (!base_crtc)
    goto out;

  base_mods = meta_crtc_kms_get_modifiers (base_crtc, format);
  if (!base_mods)
    goto out;

  /*
   * If this is the only CRTC we have, we don't need to intersect the sets of
   * modifiers.
   */
  if (other_crtcs == NULL)
    {
      modifiers = g_array_sized_new (FALSE, FALSE, sizeof (uint64_t),
                                     base_mods->len);
      g_array_append_vals (modifiers, base_mods->data, base_mods->len);
      return modifiers;
    }

  modifiers = g_array_new (FALSE, FALSE, sizeof (uint64_t));

  /*
   * For each modifier from base_crtc, check if it's available on all other
   * CRTCs.
   */
  for (i = 0; i < base_mods->len; i++)
    {
      uint64_t modifier = g_array_index (base_mods, uint64_t, i);
      gboolean found_everywhere = TRUE;
      GList *k;

      /* Check if we have the same modifier available for all CRTCs. */
      for (k = other_crtcs; k; k = k->next)
        {
          MetaCrtc *crtc = k->data;
          GArray *crtc_mods;
          unsigned int m;
          gboolean found_here = FALSE;

          if (crtc->logical_monitor != logical_monitor)
            continue;

          crtc_mods = meta_crtc_kms_get_modifiers (crtc, format);
          if (!crtc_mods)
            {
              g_array_free (modifiers, TRUE);
              goto out;
            }

          for (m = 0; m < crtc_mods->len; m++)
            {
              uint64_t local_mod = g_array_index (crtc_mods, uint64_t, m);

              if (local_mod == modifier)
                {
                  found_here = TRUE;
                  break;
                }
            }

          if (!found_here)
            {
              found_everywhere = FALSE;
              break;
            }
        }

      if (found_everywhere)
        g_array_append_val (modifiers, modifier);
    }

  if (modifiers->len == 0)
    {
      g_array_free (modifiers, TRUE);
      goto out;
    }

  return modifiers;

out:
  g_list_free (other_crtcs);
  return NULL;
}

static GArray *
get_supported_egl_modifiers (CoglOnscreen *onscreen,
                             MetaGpu      *gpu,
                             uint32_t      format)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaRendererNative *renderer_native = onscreen_native->renderer_native;
  MetaEgl *egl = meta_onscreen_native_get_egl (onscreen_native);
  MetaRendererNativeGpuData *renderer_gpu_data;
  EGLint num_modifiers;
  GArray *modifiers;
  GError *error = NULL;
  gboolean ret;

  renderer_gpu_data = meta_renderer_native_get_gpu_data (renderer_native,
                                                         META_GPU_KMS (gpu));

  if (!meta_egl_has_extensions (egl, renderer_gpu_data->egl_display, NULL,
                                "EGL_EXT_image_dma_buf_import_modifiers",
                                NULL))
    return NULL;

  ret = meta_egl_query_dma_buf_modifiers (egl, renderer_gpu_data->egl_display,
                                          format, 0, NULL, NULL,
                                          &num_modifiers, NULL);
  if (!ret || num_modifiers == 0)
    return NULL;

  modifiers = g_array_sized_new (FALSE, FALSE, sizeof (uint64_t),
                                 num_modifiers);
  ret = meta_egl_query_dma_buf_modifiers (egl, renderer_gpu_data->egl_display,
                                          format, num_modifiers,
                                          (EGLuint64KHR *) modifiers->data, NULL,
                                          &num_modifiers, &error);

  if (!ret)
    {
      g_warning ("Failed to query DMABUF modifiers: %s", error->message);
      g_error_free (error);
      g_array_free (modifiers, TRUE);
      return NULL;
    }

  return modifiers;
}

static GArray *
get_supported_modifiers (CoglOnscreen *onscreen,
                         uint32_t      format)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaLogicalMonitor *logical_monitor = onscreen_native->logical_monitor;
  GArray *modifiers = NULL;
  GArray *gpu_mods;
  GList *l_monitor;
  unsigned int i;

  if (!logical_monitor)
    return NULL;

  /* Find our base CRTC to intersect against. */
  for (l_monitor = meta_logical_monitor_get_monitors (logical_monitor);
       l_monitor;
       l_monitor = l_monitor->next)
    {
      MetaMonitor *monitor = l_monitor->data;
      MetaGpu *gpu = meta_monitor_get_gpu (monitor);

      if (gpu == META_GPU (onscreen_native->render_gpu))
        gpu_mods = get_supported_kms_modifiers (onscreen, gpu, format);
      else
        gpu_mods = get_supported_egl_modifiers (onscreen, gpu, format);

      if (!gpu_mods)
        {
          g_array_free (modifiers, TRUE);
          return NULL;
        }

      if (!modifiers)
        {
          modifiers = gpu_mods;
          continue;
        }

      for (i = 0; i < modifiers->len; i++)
        {
          uint64_t modifier = g_array_index (modifiers, uint64_t, i);
          gboolean found = FALSE;
          unsigned int m;

          for (m = 0; m < gpu_mods->len; m++)
            {
              uint64_t gpu_mod = g_array_index (gpu_mods, uint64_t, m);

              if (gpu_mod == modifier)
                {
                  found = TRUE;
                  break;
                }
            }

          if (!found)
            {
              g_array_remove_index_fast (modifiers, i);
              i--;
            }
        }

      g_array_free (gpu_mods, TRUE);
    }

  if (modifiers && modifiers->len == 0)
    {
      g_array_free (modifiers, TRUE);
      return NULL;
    }

  return modifiers;
}

static gboolean
init_secondary_gpu_state_gpu_copy_mode (MetaRendererNative         *renderer_native,
                                        CoglOnscreen               *onscreen,
                                        MetaRendererNativeGpuData  *renderer_gpu_data,
                                        MetaGpuKms                 *gpu_kms,
                                        GError                    **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaEgl *egl = meta_onscreen_native_get_egl (onscreen_native);
  int width, height;
  EGLNativeWindowType egl_native_window;
  struct gbm_surface *gbm_surface;
  EGLSurface egl_surface;
  MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state;

  width = cogl_framebuffer_get_width (framebuffer);
  height = cogl_framebuffer_get_height (framebuffer);

  gbm_surface = gbm_surface_create (renderer_gpu_data->gbm.device,
                                    width, height,
                                    GBM_FORMAT_XRGB8888,
                                    GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  if (!gbm_surface)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create gbm_surface: %s", strerror (errno));
      return FALSE;
    }

  egl_native_window = (EGLNativeWindowType) gbm_surface;
  egl_surface =
    meta_egl_create_window_surface (egl,
                                    renderer_gpu_data->egl_display,
                                    renderer_gpu_data->secondary.egl_config,
                                    egl_native_window,
                                    NULL,
                                    error);
  if (egl_surface == EGL_NO_SURFACE)
    {
      gbm_surface_destroy (gbm_surface);
      return FALSE;
    }

  secondary_gpu_state = g_new0 (MetaOnscreenNativeSecondaryGpuState, 1);

  secondary_gpu_state->gpu_kms = gpu_kms;
  secondary_gpu_state->renderer_gpu_data = renderer_gpu_data;
  secondary_gpu_state->gbm.surface = gbm_surface;
  secondary_gpu_state->egl_surface = egl_surface;

  g_hash_table_insert (onscreen_native->secondary_gpu_states,
                       gpu_kms, secondary_gpu_state);

  return TRUE;
}

static void
secondary_gpu_state_free (MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state)
{
  MetaBackend *backend = meta_get_backend ();
  MetaEgl *egl = meta_backend_get_egl (backend);
  MetaGpuKms *gpu_kms = secondary_gpu_state->gpu_kms;
  unsigned int i;

  if (secondary_gpu_state->egl_surface != EGL_NO_SURFACE)
    {
      MetaRendererNativeGpuData *renderer_gpu_data;

      renderer_gpu_data = secondary_gpu_state->renderer_gpu_data;
      meta_egl_destroy_surface (egl,
                                renderer_gpu_data->egl_display,
                                secondary_gpu_state->egl_surface,
                                NULL);
    }

  free_current_secondary_bo (gpu_kms, secondary_gpu_state);
  free_next_secondary_bo (gpu_kms, secondary_gpu_state);
  g_clear_pointer (&secondary_gpu_state->gbm.surface, gbm_surface_destroy);

  for (i = 0; i < G_N_ELEMENTS (secondary_gpu_state->cpu.dumb_fbs); i++)
    {
      MetaDumbBuffer *dumb_fb = &secondary_gpu_state->cpu.dumb_fbs[i];

      if (dumb_fb->fb_id)
        release_dumb_fb (dumb_fb, gpu_kms);
    }

  g_free (secondary_gpu_state);
}

static gboolean
init_secondary_gpu_state_cpu_copy_mode (MetaRendererNative         *renderer_native,
                                        CoglOnscreen               *onscreen,
                                        MetaRendererNativeGpuData  *renderer_gpu_data,
                                        MetaGpuKms                 *gpu_kms,
                                        GError                    **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state;
  int width, height;
  unsigned int i;

  width = cogl_framebuffer_get_width (framebuffer);
  height = cogl_framebuffer_get_height (framebuffer);

  secondary_gpu_state = g_new0 (MetaOnscreenNativeSecondaryGpuState, 1);
  secondary_gpu_state->renderer_gpu_data = renderer_gpu_data;
  secondary_gpu_state->gpu_kms = gpu_kms;
  secondary_gpu_state->egl_surface = EGL_NO_SURFACE;

  for (i = 0; i < G_N_ELEMENTS (secondary_gpu_state->cpu.dumb_fbs); i++)
    {
      MetaDumbBuffer *dumb_fb = &secondary_gpu_state->cpu.dumb_fbs[i];

      if (!init_dumb_fb (dumb_fb,
                         gpu_kms,
                         width, height,
                         GBM_FORMAT_XBGR8888,
                         error))
        {
          secondary_gpu_state_free (secondary_gpu_state);
          return FALSE;
        }
    }

  g_hash_table_insert (onscreen_native->secondary_gpu_states,
                       gpu_kms, secondary_gpu_state);

  return TRUE;
}

static gboolean
init_secondary_gpu_state (MetaRendererNative  *renderer_native,
                          CoglOnscreen        *onscreen,
                          MetaGpuKms          *gpu_kms,
                          GError             **error)
{
  MetaRendererNativeGpuData *renderer_gpu_data;

  renderer_gpu_data = meta_renderer_native_get_gpu_data (renderer_native,
                                                         gpu_kms);

  switch (renderer_gpu_data->secondary.copy_mode)
    {
    case META_SHARED_FRAMEBUFFER_COPY_MODE_GPU:
      if (!init_secondary_gpu_state_gpu_copy_mode (renderer_native,
                                                   onscreen,
                                                   renderer_gpu_data,
                                                   gpu_kms,
                                                   error))
        return FALSE;
      break;
    case META_SHARED_FRAMEBUFFER_COPY_MODE_CPU:
      if (!init_secondary_gpu_state_cpu_copy_mode (renderer_native,
                                                   onscreen,
                                                   renderer_gpu_data,
                                                   gpu_kms,
                                                   error))
        return FALSE;
      break;
    }

  return TRUE;
}

static void
meta_renderer_native_disconnect (CoglRenderer *cogl_renderer)
{
  CoglRendererEGL *cogl_renderer_egl = cogl_renderer->winsys;

  g_slice_free (CoglRendererEGL, cogl_renderer_egl);
}

static void
flush_pending_swap_notify (CoglFramebuffer *framebuffer)
{
  if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
    {
      CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
      CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
      MetaOnscreenNative *onscreen_native = onscreen_egl->platform;

      if (onscreen_native->pending_swap_notify)
        {
          CoglFrameInfo *info;

          while ((info = g_queue_peek_head (&onscreen->pending_frame_infos)) &&
                 info->global_frame_counter <= onscreen_native->pending_swap_notify_frame_count)
            {
              _cogl_onscreen_notify_frame_sync (onscreen, info);
              _cogl_onscreen_notify_complete (onscreen, info);
              cogl_object_unref (info);
              g_queue_pop_head (&onscreen->pending_frame_infos);
            }

          onscreen_native->pending_swap_notify = FALSE;
          cogl_object_unref (onscreen);
        }
    }
}

static void
flush_pending_swap_notify_idle (void *user_data)
{
  CoglContext *cogl_context = user_data;
  CoglRendererEGL *cogl_renderer_egl = cogl_context->display->renderer->winsys;
  MetaRendererNativeGpuData *renderer_gpu_data = cogl_renderer_egl->platform;
  MetaRendererNative *renderer_native = renderer_gpu_data->renderer_native;
  GList *l;

  /* This needs to be disconnected before invoking the callbacks in
   * case the callbacks cause it to be queued again */
  _cogl_closure_disconnect (renderer_native->swap_notify_idle);
  renderer_native->swap_notify_idle = NULL;

  l = cogl_context->framebuffers;
  while (l)
    {
      GList *next = l->next;
      CoglFramebuffer *framebuffer = l->data;

      flush_pending_swap_notify (framebuffer);

      l = next;
    }
}

static void
free_current_secondary_bo (MetaGpuKms                          *gpu_kms,
                           MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state)
{
  MetaRendererNativeGpuData *renderer_gpu_data;
  int kms_fd;

  kms_fd = meta_gpu_kms_get_fd (gpu_kms);

  renderer_gpu_data = secondary_gpu_state->renderer_gpu_data;
  switch (renderer_gpu_data->secondary.copy_mode)
    {
    case META_SHARED_FRAMEBUFFER_COPY_MODE_GPU:
      if (secondary_gpu_state->gbm.current_fb_id)
        {
          drmModeRmFB (kms_fd, secondary_gpu_state->gbm.current_fb_id);
          secondary_gpu_state->gbm.current_fb_id = 0;
        }
      if (secondary_gpu_state->gbm.current_bo)
        {
          gbm_surface_release_buffer (secondary_gpu_state->gbm.surface,
                                      secondary_gpu_state->gbm.current_bo);
          secondary_gpu_state->gbm.current_bo = NULL;
        }
      break;
    case META_SHARED_FRAMEBUFFER_COPY_MODE_CPU:
      break;
    }
}

static void
free_current_bo (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaGpuKms *render_gpu = onscreen_native->render_gpu;
  int kms_fd;

  kms_fd = meta_gpu_kms_get_fd (render_gpu);

  if (onscreen_native->gbm.current_fb_id)
    {
      drmModeRmFB (kms_fd, onscreen_native->gbm.current_fb_id);
      onscreen_native->gbm.current_fb_id = 0;
    }
  if (onscreen_native->gbm.current_bo)
    {
      gbm_surface_release_buffer (onscreen_native->gbm.surface,
                                  onscreen_native->gbm.current_bo);
      onscreen_native->gbm.current_bo = NULL;
    }

  g_hash_table_foreach (onscreen_native->secondary_gpu_states,
                        (GHFunc) free_current_secondary_bo,
                        NULL);
}

static void
meta_onscreen_native_queue_swap_notify (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *onscreen_egl =  onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaRendererNative *renderer_native = onscreen_native->renderer_native;

  onscreen_native->pending_swap_notify_frame_count =
    onscreen_native->pending_queue_swap_notify_frame_count;

  if (onscreen_native->pending_swap_notify)
    return;

  /* We only want to notify that the swap is complete when the
   * application calls cogl_context_dispatch so instead of
   * immediately notifying we queue an idle callback */
  if (!renderer_native->swap_notify_idle)
    {
      CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
      CoglContext *cogl_context = framebuffer->context;
      CoglRenderer *cogl_renderer = cogl_context->display->renderer;

      renderer_native->swap_notify_idle =
        _cogl_poll_renderer_add_idle (cogl_renderer,
                                      flush_pending_swap_notify_idle,
                                      cogl_context,
                                      NULL);
    }

  /*
   * The framebuffer will have its own referenc while the swap notify is
   * pending. Otherwise when destroying the view would drop the pending
   * notification with if the destruction happens before the idle callback
   * is invoked.
   */
  cogl_object_ref (onscreen);
  onscreen_native->pending_swap_notify = TRUE;
}

static gboolean
meta_renderer_native_connect (CoglRenderer *cogl_renderer,
                              GError      **error)
{
  CoglRendererEGL *cogl_renderer_egl;
  MetaGpuKms *gpu_kms = cogl_renderer->custom_winsys_user_data;
  MetaRendererNative *renderer_native = meta_renderer_native_from_gpu (gpu_kms);
  MetaRendererNativeGpuData *renderer_gpu_data;

  cogl_renderer->winsys = g_slice_new0 (CoglRendererEGL);
  cogl_renderer_egl = cogl_renderer->winsys;

  renderer_gpu_data = meta_renderer_native_get_gpu_data (renderer_native,
                                                         gpu_kms);

  cogl_renderer_egl->platform_vtable = &_cogl_winsys_egl_vtable;
  cogl_renderer_egl->platform = renderer_gpu_data;
  cogl_renderer_egl->edpy = renderer_gpu_data->egl_display;

  if (!_cogl_winsys_egl_renderer_connect_common (cogl_renderer, error))
    goto fail;

  return TRUE;

fail:
  meta_renderer_native_disconnect (cogl_renderer);

  return FALSE;
}

static int
meta_renderer_native_add_egl_config_attributes (CoglDisplay           *cogl_display,
                                                CoglFramebufferConfig *config,
                                                EGLint                *attributes)
{
  CoglRendererEGL *cogl_renderer_egl = cogl_display->renderer->winsys;
  MetaRendererNativeGpuData *renderer_gpu_data = cogl_renderer_egl->platform;
  int i = 0;

  switch (renderer_gpu_data->mode)
    {
    case META_RENDERER_NATIVE_MODE_GBM:
      attributes[i++] = EGL_SURFACE_TYPE;
      attributes[i++] = EGL_WINDOW_BIT;
      break;
#ifdef HAVE_EGL_DEVICE
    case META_RENDERER_NATIVE_MODE_EGL_DEVICE:
      attributes[i++] = EGL_SURFACE_TYPE;
      attributes[i++] = EGL_STREAM_BIT_KHR;
      break;
#endif
    }

  return i;
}

static gboolean
choose_egl_config_from_gbm_format (MetaEgl       *egl,
                                   EGLDisplay     egl_display,
                                   const EGLint  *attributes,
                                   uint32_t       gbm_format,
                                   EGLConfig     *out_config,
                                   GError       **error)
{
  EGLConfig *egl_configs;
  EGLint n_configs;
  EGLint i;

  egl_configs = meta_egl_choose_all_configs (egl, egl_display,
                                             attributes,
                                             &n_configs,
                                             error);
  if (!egl_configs)
    return FALSE;

  for (i = 0; i < n_configs; i++)
    {
      EGLint visual_id;

      if (!meta_egl_get_config_attrib (egl, egl_display,
                                       egl_configs[i],
                                       EGL_NATIVE_VISUAL_ID,
                                       &visual_id,
                                       error))
        {
          g_free (egl_configs);
          return FALSE;
        }

      if ((uint32_t) visual_id == gbm_format)
        {
          *out_config = egl_configs[i];
          g_free (egl_configs);
          return TRUE;
        }
    }

  g_free (egl_configs);
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
               "No EGL config matching supported GBM format found");
  return FALSE;
}

static gboolean
meta_renderer_native_choose_egl_config (CoglDisplay  *cogl_display,
                                        EGLint       *attributes,
                                        EGLConfig    *out_config,
                                        GError      **error)
{
  CoglRenderer *cogl_renderer = cogl_display->renderer;
  CoglRendererEGL *cogl_renderer_egl = cogl_renderer->winsys;
  MetaBackend *backend = meta_get_backend ();
  MetaEgl *egl = meta_backend_get_egl (backend);
  EGLDisplay egl_display = cogl_renderer_egl->edpy;

  return choose_egl_config_from_gbm_format (egl,
                                            egl_display,
                                            attributes,
                                            GBM_FORMAT_XRGB8888,
                                            out_config,
                                            error);
}

static gboolean
meta_renderer_native_setup_egl_display (CoglDisplay *cogl_display,
                                        GError     **error)
{
  CoglDisplayEGL *cogl_display_egl = cogl_display->winsys;
  CoglRendererEGL *cogl_renderer_egl = cogl_display->renderer->winsys;
  MetaRendererNativeGpuData *renderer_gpu_data = cogl_renderer_egl->platform;
  MetaRendererNative *renderer_native = renderer_gpu_data->renderer_native;

  cogl_display_egl->platform = renderer_native;

  /* Force a full modeset / drmModeSetCrtc on
   * the first swap buffers call.
   */
  meta_renderer_native_queue_modes_reset (renderer_native);

  return TRUE;
}

static void
meta_renderer_native_destroy_egl_display (CoglDisplay *cogl_display)
{
}

static EGLSurface
create_dummy_pbuffer_surface (EGLDisplay egl_display,
                              GError   **error)
{
  MetaBackend *backend = meta_get_backend ();
  MetaEgl *egl = meta_backend_get_egl (backend);
  EGLConfig pbuffer_config;
  static const EGLint pbuffer_config_attribs[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RED_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_BLUE_SIZE, 1,
    EGL_ALPHA_SIZE, 0,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };
  static const EGLint pbuffer_attribs[] = {
    EGL_WIDTH, 16,
    EGL_HEIGHT, 16,
    EGL_NONE
  };

  if (!meta_egl_choose_first_config (egl, egl_display, pbuffer_config_attribs,
                                     &pbuffer_config, error))
    return EGL_NO_SURFACE;

  return meta_egl_create_pbuffer_surface (egl, egl_display,
                                          pbuffer_config, pbuffer_attribs,
                                          error);
}

static gboolean
meta_renderer_native_egl_context_created (CoglDisplay *cogl_display,
                                          GError     **error)
{
  CoglDisplayEGL *cogl_display_egl = cogl_display->winsys;
  CoglRenderer *cogl_renderer = cogl_display->renderer;
  CoglRendererEGL *cogl_renderer_egl = cogl_renderer->winsys;

  if ((cogl_renderer_egl->private_features &
       COGL_EGL_WINSYS_FEATURE_SURFACELESS_CONTEXT) == 0)
    {
      cogl_display_egl->dummy_surface =
        create_dummy_pbuffer_surface (cogl_renderer_egl->edpy, error);
      if (cogl_display_egl->dummy_surface == EGL_NO_SURFACE)
        return FALSE;
    }

  if (!_cogl_winsys_egl_make_current (cogl_display,
                                      cogl_display_egl->dummy_surface,
                                      cogl_display_egl->dummy_surface,
                                      cogl_display_egl->egl_context))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Failed to make context current");
      return FALSE;
    }

  return TRUE;
}

static void
meta_renderer_native_egl_cleanup_context (CoglDisplay *cogl_display)
{
  CoglDisplayEGL *cogl_display_egl = cogl_display->winsys;
  CoglRenderer *cogl_renderer = cogl_display->renderer;
  CoglRendererEGL *cogl_renderer_egl = cogl_renderer->winsys;
  MetaRendererNativeGpuData *renderer_gpu_data = cogl_renderer_egl->platform;
  MetaRendererNative *renderer_native = renderer_gpu_data->renderer_native;
  MetaEgl *egl = meta_renderer_native_get_egl (renderer_native);

  if (cogl_display_egl->dummy_surface != EGL_NO_SURFACE)
    {
      meta_egl_destroy_surface (egl,
                                cogl_renderer_egl->edpy,
                                cogl_display_egl->dummy_surface,
                                NULL);
      cogl_display_egl->dummy_surface = EGL_NO_SURFACE;
    }
}

static void
swap_secondary_drm_fb (MetaGpuKms                          *gpu_kms,
                       MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state)
{
  secondary_gpu_state->gbm.current_fb_id = secondary_gpu_state->gbm.next_fb_id;
  secondary_gpu_state->gbm.next_fb_id = 0;

  secondary_gpu_state->gbm.current_bo = secondary_gpu_state->gbm.next_bo;
  secondary_gpu_state->gbm.next_bo = NULL;
}

static void
meta_onscreen_native_swap_drm_fb (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *onscreen_egl =  onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;

  free_current_bo (onscreen);

  onscreen_native->gbm.current_fb_id = onscreen_native->gbm.next_fb_id;
  onscreen_native->gbm.next_fb_id = 0;

  onscreen_native->gbm.current_bo = onscreen_native->gbm.next_bo;
  onscreen_native->gbm.next_bo = NULL;

  g_hash_table_foreach (onscreen_native->secondary_gpu_states,
                        (GHFunc) swap_secondary_drm_fb,
                        NULL);
}

static void
on_crtc_flipped (GClosure         *closure,
                 MetaGpuKms       *gpu_kms,
                 MetaRendererView *view)
{
  ClutterStageView *stage_view = CLUTTER_STAGE_VIEW (view);
  CoglFramebuffer *framebuffer =
    clutter_stage_view_get_onscreen (stage_view);
  CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
  CoglOnscreenEGL *onscreen_egl =  onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaRendererNative *renderer_native = onscreen_native->renderer_native;
  MetaGpuKms *render_gpu = onscreen_native->render_gpu;

  if (gpu_kms != render_gpu)
    {
      MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state;

      secondary_gpu_state = get_secondary_gpu_state (onscreen, gpu_kms);
      secondary_gpu_state->pending_flips--;
    }

  onscreen_native->total_pending_flips--;
  if (onscreen_native->total_pending_flips == 0)
    {
      MetaRendererNativeGpuData *renderer_gpu_data;

      onscreen_native->pending_queue_swap_notify = FALSE;

      meta_onscreen_native_queue_swap_notify (onscreen);

      renderer_gpu_data =
        meta_renderer_native_get_gpu_data (renderer_native,
                                           onscreen_native->render_gpu);
      switch (renderer_gpu_data->mode)
        {
        case META_RENDERER_NATIVE_MODE_GBM:
          meta_onscreen_native_swap_drm_fb (onscreen);
          break;
#ifdef HAVE_EGL_DEVICE
        case META_RENDERER_NATIVE_MODE_EGL_DEVICE:
          break;
#endif
        }
    }
}

static void
free_next_secondary_bo (MetaGpuKms                          *gpu_kms,
                        MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state)
{
  MetaRendererNativeGpuData *renderer_gpu_data;

  renderer_gpu_data = secondary_gpu_state->renderer_gpu_data;
  switch (renderer_gpu_data->secondary.copy_mode)
    {
    case META_SHARED_FRAMEBUFFER_COPY_MODE_GPU:
      if (secondary_gpu_state->gbm.next_fb_id)
        {
          int kms_fd;

          kms_fd = meta_gpu_kms_get_fd (gpu_kms);
          drmModeRmFB (kms_fd, secondary_gpu_state->gbm.next_fb_id);
          gbm_surface_release_buffer (secondary_gpu_state->gbm.surface,
                                      secondary_gpu_state->gbm.next_bo);
          secondary_gpu_state->gbm.next_fb_id = 0;
          secondary_gpu_state->gbm.next_bo = NULL;
        }
      break;
    case META_SHARED_FRAMEBUFFER_COPY_MODE_CPU:
      break;
    }
}

static void
flip_closure_destroyed (MetaRendererView *view)
{
  ClutterStageView *stage_view = CLUTTER_STAGE_VIEW (view);
  CoglFramebuffer *framebuffer =
    clutter_stage_view_get_onscreen (stage_view);
  CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
  CoglOnscreenEGL *onscreen_egl =  onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaRendererNative *renderer_native = onscreen_native->renderer_native;
  MetaGpuKms *render_gpu = onscreen_native->render_gpu;
  MetaRendererNativeGpuData *renderer_gpu_data;

  renderer_gpu_data = meta_renderer_native_get_gpu_data (renderer_native,
                                                         render_gpu);
  switch (renderer_gpu_data->mode)
    {
    case META_RENDERER_NATIVE_MODE_GBM:
      if (onscreen_native->gbm.next_fb_id)
        {
          int kms_fd;

          kms_fd = meta_gpu_kms_get_fd (render_gpu);
          drmModeRmFB (kms_fd, onscreen_native->gbm.next_fb_id);
          gbm_surface_release_buffer (onscreen_native->gbm.surface,
                                      onscreen_native->gbm.next_bo);
          onscreen_native->gbm.next_bo = NULL;
          onscreen_native->gbm.next_fb_id = 0;
        }

      g_hash_table_foreach (onscreen_native->secondary_gpu_states,
                            (GHFunc) free_next_secondary_bo,
                            NULL);

      break;
#ifdef HAVE_EGL_DEVICE
    case META_RENDERER_NATIVE_MODE_EGL_DEVICE:
      break;
#endif
    }

  if (onscreen_native->pending_queue_swap_notify)
    {
      meta_onscreen_native_queue_swap_notify (onscreen);
      onscreen_native->pending_queue_swap_notify = FALSE;
    }

  g_object_unref (view);
}

#ifdef HAVE_EGL_DEVICE
static gboolean
flip_egl_stream (MetaOnscreenNative *onscreen_native,
                 GClosure           *flip_closure)
{
  MetaRendererNativeGpuData *renderer_gpu_data;
  EGLDisplay *egl_display;
  MetaEgl *egl = meta_onscreen_native_get_egl (onscreen_native);
  EGLAttrib *acquire_attribs;
  GError *error = NULL;

  renderer_gpu_data =
    meta_renderer_native_get_gpu_data (onscreen_native->renderer_native,
                                       onscreen_native->render_gpu);
  if (renderer_gpu_data->egl.no_egl_output_drm_flip_event)
    return FALSE;

  acquire_attribs = (EGLAttrib[]) {
    EGL_DRM_FLIP_EVENT_DATA_NV,
    (EGLAttrib) flip_closure,
    EGL_NONE
  };

  egl_display = renderer_gpu_data->egl_display;
  if (!meta_egl_stream_consumer_acquire_attrib (egl,
                                                egl_display,
                                                onscreen_native->egl.stream,
                                                acquire_attribs,
                                                &error))
    {
      if (error->domain != META_EGL_ERROR ||
          error->code != EGL_RESOURCE_BUSY_EXT)
        {
          g_warning ("Failed to flip EGL stream (%s), relying on clock from "
                     "now on", error->message);
          renderer_gpu_data->egl.no_egl_output_drm_flip_event = TRUE;
        }
      g_error_free (error);
      return FALSE;
    }

  g_closure_ref (flip_closure);

  return TRUE;
}
#endif /* HAVE_EGL_DEVICE */

static void
meta_onscreen_native_flip_crtc (CoglOnscreen *onscreen,
                                GClosure     *flip_closure,
                                MetaCrtc     *crtc,
                                int           x,
                                int           y,
                                gboolean     *fb_in_use)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaRendererNative *renderer_native = onscreen_native->renderer_native;
  MetaGpuKms *render_gpu = onscreen_native->render_gpu;
  MetaRendererNativeGpuData *renderer_gpu_data;
  MetaGpuKms *gpu_kms;
  MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state = NULL;
  uint32_t fb_id;

  gpu_kms = META_GPU_KMS (meta_crtc_get_gpu (crtc));
  if (!meta_gpu_kms_is_crtc_active (gpu_kms, crtc))
    {
      *fb_in_use = FALSE;
      return;
    }

  renderer_gpu_data = meta_renderer_native_get_gpu_data (renderer_native,
                                                         render_gpu);
  switch (renderer_gpu_data->mode)
    {
    case META_RENDERER_NATIVE_MODE_GBM:
      if (gpu_kms == render_gpu)
        {
          fb_id = onscreen_native->gbm.next_fb_id;
        }
      else
        {
          secondary_gpu_state = get_secondary_gpu_state (onscreen, gpu_kms);
          fb_id = secondary_gpu_state->gbm.next_fb_id;
        }

      if (!meta_gpu_kms_flip_crtc (gpu_kms,
                                   crtc,
                                   x, y,
                                   fb_id,
                                   flip_closure,
                                   fb_in_use))
        return;

      onscreen_native->total_pending_flips++;
      if (secondary_gpu_state)
        secondary_gpu_state->pending_flips++;

      break;
#ifdef HAVE_EGL_DEVICE
    case META_RENDERER_NATIVE_MODE_EGL_DEVICE:
      if (flip_egl_stream (onscreen_native,
                           flip_closure))
        onscreen_native->total_pending_flips++;
      *fb_in_use = TRUE;
      break;
#endif
    }
}

typedef struct _SetCrtcFbData
{
  MetaGpuKms *render_gpu;
  CoglOnscreen *onscreen;
  uint32_t fb_id;
} SetCrtcFbData;

static void
set_crtc_fb (MetaLogicalMonitor *logical_monitor,
             MetaCrtc           *crtc,
             gpointer            user_data)
{
  SetCrtcFbData *data = user_data;
  MetaGpuKms *render_gpu = data->render_gpu;
  CoglOnscreen *onscreen = data->onscreen;
  MetaGpuKms *gpu_kms;
  uint32_t fb_id;
  int x, y;

  gpu_kms = META_GPU_KMS (meta_crtc_get_gpu (crtc));
  if (gpu_kms == render_gpu)
    {
      fb_id = data->fb_id;
    }
  else
    {
      MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state;

      secondary_gpu_state = get_secondary_gpu_state (onscreen, gpu_kms);
      if (!secondary_gpu_state)
        return;

      fb_id = secondary_gpu_state->gbm.next_fb_id;
    }

  x = crtc->rect.x - logical_monitor->rect.x;
  y = crtc->rect.y - logical_monitor->rect.y;

  meta_gpu_kms_apply_crtc_mode (gpu_kms, crtc, x, y, fb_id);
}

static void
meta_onscreen_native_set_crtc_modes (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaRendererNative *renderer_native = onscreen_native->renderer_native;
  MetaGpuKms *render_gpu = onscreen_native->render_gpu;
  MetaRendererNativeGpuData *renderer_gpu_data;
  MetaRendererView *view = onscreen_native->view;
  uint32_t fb_id = 0;
  MetaLogicalMonitor *logical_monitor;

  renderer_gpu_data = meta_renderer_native_get_gpu_data (renderer_native,
                                                         render_gpu);
  switch (renderer_gpu_data->mode)
    {
    case META_RENDERER_NATIVE_MODE_GBM:
      fb_id = onscreen_native->gbm.next_fb_id;
      break;
#ifdef HAVE_EGL_DEVICE
    case META_RENDERER_NATIVE_MODE_EGL_DEVICE:
      fb_id = onscreen_native->egl.dumb_fb.fb_id;
      break;
#endif
    }

  g_assert (fb_id != 0);

  logical_monitor = meta_renderer_view_get_logical_monitor (view);
  if (logical_monitor)
    {
      SetCrtcFbData data = {
        .render_gpu = render_gpu,
        .onscreen = onscreen,
        .fb_id = fb_id
      };

      meta_logical_monitor_foreach_crtc (logical_monitor,
                                         set_crtc_fb,
                                         &data);
    }
  else
    {
      GList *l;

      for (l = meta_gpu_get_crtcs (META_GPU (render_gpu)); l; l = l->next)
        {
          MetaCrtc *crtc = l->data;

          meta_gpu_kms_apply_crtc_mode (render_gpu,
                                        crtc,
                                        crtc->rect.x, crtc->rect.y,
                                        fb_id);
        }
    }
}

typedef struct _FlipCrtcData
{
  CoglOnscreen *onscreen;
  GClosure *flip_closure;

  gboolean out_fb_in_use;
} FlipCrtcData;

static void
flip_crtc (MetaLogicalMonitor *logical_monitor,
           MetaCrtc           *crtc,
           gpointer            user_data)
{
  FlipCrtcData *data = user_data;
  int x, y;

  x = crtc->rect.x - logical_monitor->rect.x;
  y = crtc->rect.y - logical_monitor->rect.y;

  meta_onscreen_native_flip_crtc (data->onscreen,
                                  data->flip_closure,
                                  crtc, x, y,
                                  &data->out_fb_in_use);
}

static void
meta_onscreen_native_flip_crtcs (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaGpuKms *render_gpu = onscreen_native->render_gpu;
  MetaRendererView *view = onscreen_native->view;
  GClosure *flip_closure;
  MetaLogicalMonitor *logical_monitor;
  gboolean fb_in_use = FALSE;

  /*
   * Create a closure that either will be invoked or destructed.
   * Invoking the closure represents a completed flip. If the closure
   * is destructed before being invoked, the framebuffer references will be
   * cleaned up accordingly.
   *
   * Each successful flip will each own one reference to the closure, thus keep
   * it alive until either invoked or destructed. If flipping failed, the
   * closure will be destructed before this function goes out of scope.
   */
  flip_closure = g_cclosure_new (G_CALLBACK (on_crtc_flipped),
                                 g_object_ref (view),
                                 (GClosureNotify) flip_closure_destroyed);
  g_closure_set_marshal (flip_closure, g_cclosure_marshal_VOID__OBJECT);

  /* Either flip the CRTC's of the monitor info, if we are drawing just part
   * of the stage, or all of the CRTC's if we are drawing the whole stage.
   */
  logical_monitor = meta_renderer_view_get_logical_monitor (view);
  if (logical_monitor)
    {
      FlipCrtcData data = {
        .onscreen = onscreen,
        .flip_closure = flip_closure,
      };

      meta_logical_monitor_foreach_crtc (logical_monitor,
                                         flip_crtc,
                                         &data);
      fb_in_use = data.out_fb_in_use;
    }
  else
    {
      GList *l;

      for (l = meta_gpu_get_crtcs (META_GPU (render_gpu)); l; l = l->next)
        {
          MetaCrtc *crtc = l->data;

          meta_onscreen_native_flip_crtc (onscreen, flip_closure,
                                          crtc, crtc->rect.x, crtc->rect.y,
                                          &fb_in_use);
        }
    }

  /*
   * If the framebuffer is in use, but we don't have any pending flips it means
   * that flipping is not supported and we set the next framebuffer directly.
   * Since we won't receive a flip callback, lets just notify listeners
   * directly.
   */
  if (fb_in_use && onscreen_native->total_pending_flips == 0)
    {
      MetaRendererNative *renderer_native = onscreen_native->renderer_native;
      MetaRendererNativeGpuData *renderer_gpu_data;

      renderer_gpu_data = meta_renderer_native_get_gpu_data (renderer_native,
                                                             render_gpu);
      switch (renderer_gpu_data->mode)
        {
        case META_RENDERER_NATIVE_MODE_GBM:
          meta_onscreen_native_swap_drm_fb (onscreen);
          break;
#ifdef HAVE_EGL_DEVICE
        case META_RENDERER_NATIVE_MODE_EGL_DEVICE:
          break;
#endif
        }
    }

  onscreen_native->pending_queue_swap_notify = TRUE;

  g_closure_unref (flip_closure);
}

static gboolean
gbm_get_next_fb_id (MetaGpuKms         *gpu_kms,
                    struct gbm_surface *gbm_surface,
                    struct gbm_bo     **out_next_bo,
                    uint32_t           *out_next_fb_id)
{
  struct gbm_bo *next_bo;
  uint32_t next_fb_id;
  int kms_fd;
  uint32_t handles[4] = { 0, };
  uint32_t strides[4] = { 0, };
  uint32_t offsets[4] = { 0, };
  uint64_t modifiers[4] = { 0, };
  int i;

  /* Now we need to set the CRTC to whatever is the front buffer */
  next_bo = gbm_surface_lock_front_buffer (gbm_surface);

  for (i = 0; i < gbm_bo_get_plane_count (next_bo); i++)
    {
      strides[i] = gbm_bo_get_stride_for_plane (next_bo, i);
      handles[i] = gbm_bo_get_handle_for_plane (next_bo, i).u32;
      offsets[i] = gbm_bo_get_offset (next_bo, i);
      modifiers[i] = gbm_bo_get_modifier (next_bo);
    }

  kms_fd = meta_gpu_kms_get_fd (gpu_kms);

  if (modifiers[0] != DRM_FORMAT_MOD_INVALID)
    {
      if (drmModeAddFB2WithModifiers (kms_fd,
                                      gbm_bo_get_width (next_bo),
                                      gbm_bo_get_height (next_bo),
                                      gbm_bo_get_format (next_bo),
                                      handles,
                                      strides,
                                      offsets,
                                      modifiers,
                                      &next_fb_id,
                                      DRM_MODE_FB_MODIFIERS))
        {
          g_warning ("Failed to create new back buffer handle: %m");
          gbm_surface_release_buffer (gbm_surface, next_bo);
          return FALSE;
        }
    }
  else if (drmModeAddFB2 (kms_fd,
                          gbm_bo_get_width (next_bo),
                          gbm_bo_get_height (next_bo),
                          gbm_bo_get_format (next_bo),
                          handles,
                          strides,
                          offsets,
                          &next_fb_id,
                          0))
    {
      if (drmModeAddFB (kms_fd,
                        gbm_bo_get_width (next_bo),
                        gbm_bo_get_height (next_bo),
                        24, /* depth */
                        32, /* bpp */
                        strides[0],
                        handles[0],
                        &next_fb_id))
        {
          g_warning ("Failed to create new back buffer handle: %m");
          gbm_surface_release_buffer (gbm_surface, next_bo);
          return FALSE;
        }
    }

  *out_next_bo = next_bo;
  *out_next_fb_id = next_fb_id;
  return TRUE;
}

static void
wait_for_pending_flips (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, onscreen_native->secondary_gpu_states);
  while (g_hash_table_iter_next (&iter,
                                 NULL,
                                 (gpointer *) &secondary_gpu_state))
    {
      while (secondary_gpu_state->pending_flips)
        meta_gpu_kms_wait_for_flip (secondary_gpu_state->gpu_kms, NULL);
    }

  while (onscreen_native->total_pending_flips)
    meta_gpu_kms_wait_for_flip (onscreen_native->render_gpu, NULL);
}

static void
copy_shared_framebuffer_gpu (CoglOnscreen                        *onscreen,
                             MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state,
                             MetaRendererNativeGpuData           *renderer_gpu_data,
                             gboolean                            *egl_context_changed)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaRendererNative *renderer_native = renderer_gpu_data->renderer_native;
  MetaEgl *egl = meta_renderer_native_get_egl (renderer_native);
  GError *error = NULL;

  if (!meta_egl_make_current (egl,
                              renderer_gpu_data->egl_display,
                              secondary_gpu_state->egl_surface,
                              secondary_gpu_state->egl_surface,
                              renderer_gpu_data->secondary.egl_context,
                              &error))
    {
      g_warning ("Failed to make current: %s", error->message);
      g_error_free (error);
      return;
    }

  *egl_context_changed = TRUE;

  if (!meta_renderer_native_gles3_blit_shared_bo (egl,
                                                  renderer_native->gles3,
                                                  renderer_gpu_data->egl_display,
                                                  renderer_gpu_data->secondary.egl_context,
                                                  secondary_gpu_state->egl_surface,
                                                  onscreen_native->gbm.next_bo,
                                                  &error))
    {
      g_warning ("Failed to blit shared framebuffer: %s", error->message);
      g_error_free (error);
      return;
    }

  if (!meta_egl_swap_buffers (egl,
                              renderer_gpu_data->egl_display,
                              secondary_gpu_state->egl_surface,
                              &error))
    {
      g_warning ("Failed to swap buffers: %s", error->message);
      g_error_free (error);
      return;
    }

  gbm_get_next_fb_id (secondary_gpu_state->gpu_kms,
                      secondary_gpu_state->gbm.surface,
                      &secondary_gpu_state->gbm.next_bo,
                      &secondary_gpu_state->gbm.next_fb_id);
}

static void
copy_shared_framebuffer_cpu (CoglOnscreen                        *onscreen,
                             MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state,
                             MetaRendererNativeGpuData           *renderer_gpu_data)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaRendererNative *renderer_native = onscreen_native->renderer_native;
  MetaEgl *egl = meta_renderer_native_get_egl (renderer_native);
  int width, height;
  uint8_t *target_data;
  uint32_t target_fb_id;
  MetaDumbBuffer *next_dumb_fb;
  MetaDumbBuffer *current_dumb_fb;

  width = cogl_framebuffer_get_width (framebuffer);
  height = cogl_framebuffer_get_height (framebuffer);

  current_dumb_fb = secondary_gpu_state->cpu.dumb_fb;
  if (current_dumb_fb == &secondary_gpu_state->cpu.dumb_fbs[0])
    next_dumb_fb = &secondary_gpu_state->cpu.dumb_fbs[1];
  else
    next_dumb_fb = &secondary_gpu_state->cpu.dumb_fbs[0];
  secondary_gpu_state->cpu.dumb_fb = next_dumb_fb;

  target_data = secondary_gpu_state->cpu.dumb_fb->map;
  target_fb_id = secondary_gpu_state->cpu.dumb_fb->fb_id;

  meta_renderer_native_gles3_read_pixels (egl,
                                          renderer_native->gles3,
                                          width, height,
                                          target_data);

  secondary_gpu_state->gbm.next_fb_id = target_fb_id;
}

static void
update_secondary_gpu_state_pre_swap_buffers (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  GHashTableIter iter;
  MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state;

  g_hash_table_iter_init (&iter, onscreen_native->secondary_gpu_states);
  while (g_hash_table_iter_next (&iter,
                                 NULL,
                                 (gpointer *) &secondary_gpu_state))
    {
      MetaRendererNativeGpuData *renderer_gpu_data;

      renderer_gpu_data = secondary_gpu_state->renderer_gpu_data;
      switch (renderer_gpu_data->secondary.copy_mode)
        {
        case META_SHARED_FRAMEBUFFER_COPY_MODE_GPU:
          /* Done after eglSwapBuffers. */
          break;
        case META_SHARED_FRAMEBUFFER_COPY_MODE_CPU:
          copy_shared_framebuffer_cpu (onscreen,
                                       secondary_gpu_state,
                                       renderer_gpu_data);
          break;
        }
    }
}

static void
update_secondary_gpu_state_post_swap_buffers (CoglOnscreen *onscreen,
                                              gboolean     *egl_context_changed)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaRendererNative *renderer_native = onscreen_native->renderer_native;
  GHashTableIter iter;
  MetaOnscreenNativeSecondaryGpuState *secondary_gpu_state;

  g_hash_table_iter_init (&iter, onscreen_native->secondary_gpu_states);
  while (g_hash_table_iter_next (&iter,
                                 NULL,
                                 (gpointer *) &secondary_gpu_state))
    {
      MetaRendererNativeGpuData *renderer_gpu_data;

      renderer_gpu_data =
        meta_renderer_native_get_gpu_data (renderer_native,
                                           secondary_gpu_state->gpu_kms);
      switch (renderer_gpu_data->secondary.copy_mode)
        {
        case META_SHARED_FRAMEBUFFER_COPY_MODE_GPU:
          copy_shared_framebuffer_gpu (onscreen,
                                       secondary_gpu_state,
                                       renderer_gpu_data,
                                       egl_context_changed);
          break;
        case META_SHARED_FRAMEBUFFER_COPY_MODE_CPU:
          /* Done before eglSwapBuffers. */
          break;
        }
    }
}

static void
meta_onscreen_native_swap_buffers_with_damage (CoglOnscreen *onscreen,
                                               const int    *rectangles,
                                               int           n_rectangles)
{
  CoglContext *cogl_context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglDisplay *cogl_display = cogl_context_get_display (cogl_context);
  CoglRenderer *cogl_renderer = cogl_context->display->renderer;
  CoglRendererEGL *cogl_renderer_egl = cogl_renderer->winsys;
  MetaRendererNativeGpuData *renderer_gpu_data = cogl_renderer_egl->platform;
  MetaRendererNative *renderer_native = renderer_gpu_data->renderer_native;
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaGpuKms *render_gpu = onscreen_native->render_gpu;
  CoglFrameInfo *frame_info;
  gboolean egl_context_changed = FALSE;

  frame_info = g_queue_peek_tail (&onscreen->pending_frame_infos);
  frame_info->global_frame_counter = renderer_native->frame_counter;

  update_secondary_gpu_state_pre_swap_buffers (onscreen);

  parent_vtable->onscreen_swap_buffers_with_damage (onscreen,
                                                    rectangles,
                                                    n_rectangles);

  /*
   * Wait for the flip callback before continuing, as we might have started the
   * animation earlier due to the animation being driven by some other monitor.
   */
  wait_for_pending_flips (onscreen);

  renderer_gpu_data = meta_renderer_native_get_gpu_data (renderer_native,
                                                         render_gpu);
  switch (renderer_gpu_data->mode)
    {
    case META_RENDERER_NATIVE_MODE_GBM:
      g_warn_if_fail (onscreen_native->gbm.next_bo == NULL &&
                      onscreen_native->gbm.next_fb_id == 0);

      if (!gbm_get_next_fb_id (render_gpu,
                               onscreen_native->gbm.surface,
                               &onscreen_native->gbm.next_bo,
                               &onscreen_native->gbm.next_fb_id))
        return;

      break;
#ifdef HAVE_EGL_DEVICE
    case META_RENDERER_NATIVE_MODE_EGL_DEVICE:
      break;
#endif
    }

  update_secondary_gpu_state_post_swap_buffers (onscreen, &egl_context_changed);

  /* If this is the first framebuffer to be presented then we now setup the
   * crtc modes, else we flip from the previous buffer */
  if (onscreen_native->pending_set_crtc)
    {
      meta_onscreen_native_set_crtc_modes (onscreen);
      onscreen_native->pending_set_crtc = FALSE;
    }

  onscreen_native->pending_queue_swap_notify_frame_count = renderer_native->frame_counter;
  meta_onscreen_native_flip_crtcs (onscreen);

  /*
   * If we changed EGL context, cogl will have the wrong idea about what is
   * current, making it fail to set it when it needs to. Avoid that by making
   * EGL_NO_CONTEXT current now, making cogl eventually set the correct
   * context.
   */
  if (egl_context_changed)
    {
      _cogl_winsys_egl_make_current (cogl_display,
                                     EGL_NO_SURFACE,
                                     EGL_NO_SURFACE,
                                     EGL_NO_CONTEXT);
    }
}

static gboolean
meta_renderer_native_init_egl_context (CoglContext *cogl_context,
                                       GError     **error)
{
#ifdef HAVE_EGL_DEVICE
  CoglRenderer *cogl_renderer = cogl_context->display->renderer;
  CoglRendererEGL *cogl_renderer_egl = cogl_renderer->winsys;
  MetaRendererNativeGpuData *renderer_gpu_data = cogl_renderer_egl->platform;
#endif

  COGL_FLAGS_SET (cogl_context->features,
                  COGL_FEATURE_ID_SWAP_BUFFERS_EVENT, TRUE);
  /* TODO: remove this deprecated feature */
  COGL_FLAGS_SET (cogl_context->winsys_features,
                  COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT,
                  TRUE);
  COGL_FLAGS_SET (cogl_context->winsys_features,
                  COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT,
                  TRUE);
  COGL_FLAGS_SET (cogl_context->winsys_features,
                  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                  TRUE);

#ifdef HAVE_EGL_DEVICE
  if (renderer_gpu_data->mode == META_RENDERER_NATIVE_MODE_EGL_DEVICE)
    COGL_FLAGS_SET (cogl_context->features,
                    COGL_FEATURE_ID_TEXTURE_EGL_IMAGE_EXTERNAL, TRUE);
#endif

  return TRUE;
}

static gboolean
should_surface_be_sharable (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaRendererNative *renderer_native = onscreen_native->renderer_native;
  CoglContext *cogl_context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *cogl_renderer = cogl_context->display->renderer;
  CoglRendererEGL *cogl_renderer_egl = cogl_renderer->winsys;
  MetaRendererNativeGpuData *renderer_gpu_data = cogl_renderer_egl->platform;
  GList *l;

  if (!onscreen_native->logical_monitor)
    return FALSE;

  for (l = meta_logical_monitor_get_monitors (onscreen_native->logical_monitor);
       l;
       l = l->next)
    {
      MetaMonitor *monitor = l->data;
      MetaGpuKms *gpu_kms = META_GPU_KMS (meta_monitor_get_gpu (monitor));

      if (renderer_gpu_data != meta_renderer_native_get_gpu_data (renderer_native,
                                                                  gpu_kms))
        return TRUE;
    }

  return FALSE;
}

static gboolean
meta_renderer_native_create_surface_gbm (CoglOnscreen        *onscreen,
                                         int                  width,
                                         int                  height,
                                         struct gbm_surface **gbm_surface,
                                         EGLSurface          *egl_surface,
                                         GError             **error)
{
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaRendererNative *renderer_native = onscreen_native->renderer_native;
  MetaEgl *egl = meta_onscreen_native_get_egl (onscreen_native);
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *cogl_context = framebuffer->context;
  CoglDisplay *cogl_display = cogl_context->display;
  CoglDisplayEGL *cogl_display_egl = cogl_display->winsys;
  CoglRenderer *cogl_renderer = cogl_display->renderer;
  CoglRendererEGL *cogl_renderer_egl = cogl_renderer->winsys;
  MetaRendererNativeGpuData *renderer_gpu_data = cogl_renderer_egl->platform;
  struct gbm_surface *new_gbm_surface = NULL;
  EGLNativeWindowType egl_native_window;
  EGLSurface new_egl_surface;
  uint32_t format = GBM_FORMAT_XRGB8888;
  GArray *modifiers;

  renderer_gpu_data =
    meta_renderer_native_get_gpu_data (renderer_native,
                                       onscreen_native->render_gpu);

  modifiers = get_supported_modifiers (onscreen, format);

  if (modifiers)
    {
      new_gbm_surface =
        gbm_surface_create_with_modifiers (renderer_gpu_data->gbm.device,
                                           width, height, format,
                                           (uint64_t *) modifiers->data,
                                           modifiers->len);
      g_array_free (modifiers, TRUE);
    }

  if (!new_gbm_surface)
    {
      uint32_t flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;

      if (should_surface_be_sharable (onscreen))
        flags |= GBM_BO_USE_LINEAR;

      new_gbm_surface = gbm_surface_create (renderer_gpu_data->gbm.device,
                                            width, height,
                                            format,
                                            flags);
    }

  if (!new_gbm_surface)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to allocate surface");
      return FALSE;
    }

  egl_native_window = (EGLNativeWindowType) new_gbm_surface;
  new_egl_surface =
    meta_egl_create_window_surface (egl,
                                    cogl_renderer_egl->edpy,
                                    cogl_display_egl->egl_config,
                                    egl_native_window,
                                    NULL,
                                    error);
  if (new_egl_surface == EGL_NO_SURFACE)
    {
      gbm_surface_destroy (new_gbm_surface);
      return FALSE;
    }

  *gbm_surface = new_gbm_surface;
  *egl_surface = new_egl_surface;

  return TRUE;
}

#ifdef HAVE_EGL_DEVICE
static gboolean
meta_renderer_native_create_surface_egl_device (CoglOnscreen       *onscreen,
                                                MetaLogicalMonitor *logical_monitor,
                                                int                 width,
                                                int                 height,
                                                EGLStreamKHR       *out_egl_stream,
                                                EGLSurface         *out_egl_surface,
                                                GError            **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *cogl_context = framebuffer->context;
  CoglDisplay *cogl_display = cogl_context->display;
  CoglDisplayEGL *cogl_display_egl = cogl_display->winsys;
  CoglRenderer *cogl_renderer = cogl_display->renderer;
  CoglRendererEGL *cogl_renderer_egl = cogl_renderer->winsys;
  MetaRendererNativeGpuData *renderer_gpu_data = cogl_renderer_egl->platform;
  MetaEgl *egl =
    meta_renderer_native_get_egl (renderer_gpu_data->renderer_native);
  EGLDisplay egl_display = renderer_gpu_data->egl_display;
  MetaMonitor *monitor;
  MetaOutput *output;
  EGLConfig egl_config;
  EGLStreamKHR egl_stream;
  EGLSurface egl_surface;
  EGLint num_layers;
  EGLOutputLayerEXT output_layer;
  EGLAttrib output_attribs[3];
  EGLint stream_attribs[] = {
    EGL_STREAM_FIFO_LENGTH_KHR, 1,
    EGL_CONSUMER_AUTO_ACQUIRE_EXT, EGL_FALSE,
    EGL_NONE
  };
  EGLint stream_producer_attribs[] = {
    EGL_WIDTH, width,
    EGL_HEIGHT, height,
    EGL_NONE
  };

  egl_stream = meta_egl_create_stream (egl, egl_display, stream_attribs, error);
  if (egl_stream == EGL_NO_STREAM_KHR)
    return FALSE;

  monitor = meta_logical_monitor_get_monitors (logical_monitor)->data;
  output = meta_monitor_get_main_output (monitor);

  /*
   * An "logical_monitor" may have multiple outputs/crtcs in case its tiled,
   * but as far as I can tell, EGL only allows you to pass one crtc_id, so
   * lets pass the first one.
   */
  output_attribs[0] = EGL_DRM_CRTC_EXT;
  output_attribs[1] = output->crtc->crtc_id;
  output_attribs[2] = EGL_NONE;

  if (!meta_egl_get_output_layers (egl, egl_display,
                                   output_attribs,
                                   &output_layer, 1, &num_layers,
                                   error))
    {
      meta_egl_destroy_stream (egl, egl_display, egl_stream, NULL);
      return FALSE;
    }

  if (num_layers < 1)
    {
      meta_egl_destroy_stream (egl, egl_display, egl_stream, NULL);
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Unable to find output layers.");
      return FALSE;
    }

  if (!meta_egl_stream_consumer_output (egl, egl_display,
                                        egl_stream, output_layer,
                                        error))
    {
      meta_egl_destroy_stream (egl, egl_display, egl_stream, NULL);
      return FALSE;
    }

  egl_config = cogl_display_egl->egl_config;
  egl_surface = meta_egl_create_stream_producer_surface (egl,
                                                         egl_display,
                                                         egl_config,
                                                         egl_stream,
                                                         stream_producer_attribs,
                                                         error);
  if (egl_surface == EGL_NO_SURFACE)
    {
      meta_egl_destroy_stream (egl, egl_display, egl_stream, NULL);
      return FALSE;
    }

  *out_egl_stream = egl_stream;
  *out_egl_surface = egl_surface;

  return TRUE;
}
#endif /* HAVE_EGL_DEVICE */

static gboolean
init_dumb_fb (MetaDumbBuffer  *dumb_fb,
              MetaGpuKms      *gpu_kms,
              int              width,
              int              height,
              uint32_t         format,
              GError         **error)
{
  MetaRendererNative *renderer_native = meta_renderer_native_from_gpu (gpu_kms);
  MetaRendererNativeGpuData *renderer_gpu_data;
  struct drm_mode_create_dumb create_arg;
  struct drm_mode_destroy_dumb destroy_arg;
  struct drm_mode_map_dumb map_arg;
  uint32_t fb_id = 0;
  void *map;
  int kms_fd;

  kms_fd = meta_gpu_kms_get_fd (gpu_kms);

  create_arg = (struct drm_mode_create_dumb) {
    .bpp = 32, /* RGBX8888 */
    .width = width,
    .height = height
  };
  if (drmIoctl (kms_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_arg) != 0)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to create dumb drm buffer: %s",
                   g_strerror (errno));
      goto err_ioctl;
    }

  renderer_gpu_data = meta_renderer_native_get_gpu_data (renderer_native,
                                                         gpu_kms);
  if (!renderer_gpu_data->no_add_fb2)
    {
      uint32_t handles[4] = { create_arg.handle, };
      uint32_t pitches[4] = { create_arg.pitch, };
      uint32_t offsets[4] = { 0 };

      if (drmModeAddFB2 (kms_fd, width, height, format,
                         handles, pitches, offsets,
                         &fb_id, 0) != 0)
        {
          g_warning ("drmModeAddFB2 failed (%s), falling back to drmModeAddFB",
                     g_strerror (errno));
          renderer_gpu_data->no_add_fb2 = TRUE;
        }
    }

  if (renderer_gpu_data->no_add_fb2)
    {
      if (drmModeAddFB (kms_fd, width, height,
                        24 /* depth of RGBX8888 */,
                        32 /* bpp of RGBX8888 */,
                        create_arg.pitch,
                        create_arg.handle,
                        &fb_id) != 0)
        {
          g_set_error (error, G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "drmModeAddFB failed: %s",
                       g_strerror (errno));
          goto err_add_fb;
        }
    }

  map_arg = (struct drm_mode_map_dumb) {
    .handle = create_arg.handle
  };
  if (drmIoctl (kms_fd, DRM_IOCTL_MODE_MAP_DUMB,
                &map_arg) != 0)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to map dumb drm buffer: %s",
                   g_strerror (errno));
      goto err_map_dumb;
    }

  map = mmap (NULL, create_arg.size, PROT_WRITE, MAP_SHARED,
              kms_fd, map_arg.offset);
  if (map == MAP_FAILED)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to mmap dumb drm buffer memory: %s",
                   g_strerror (errno));
      goto err_mmap;
    }

  dumb_fb->fb_id = fb_id;
  dumb_fb->handle = create_arg.handle;
  dumb_fb->map = map;
  dumb_fb->map_size = create_arg.size;

  return TRUE;

err_mmap:
err_map_dumb:
  drmModeRmFB (kms_fd, fb_id);

err_add_fb:
  destroy_arg = (struct drm_mode_destroy_dumb) {
    .handle = create_arg.handle
  };
  drmIoctl (kms_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);

err_ioctl:
  return FALSE;
}

static void
release_dumb_fb (MetaDumbBuffer *dumb_fb,
                 MetaGpuKms     *gpu_kms)
{
  struct drm_mode_destroy_dumb destroy_arg;
  int kms_fd;

  if (!dumb_fb->map)
    return;

  munmap (dumb_fb->map, dumb_fb->map_size);
  dumb_fb->map = NULL;

  kms_fd = meta_gpu_kms_get_fd (gpu_kms);

  drmModeRmFB (kms_fd, dumb_fb->fb_id);

  destroy_arg = (struct drm_mode_destroy_dumb) {
    .handle = dumb_fb->handle
  };
  drmIoctl (kms_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);
}

static gboolean
meta_renderer_native_init_onscreen (CoglOnscreen *onscreen,
                                    GError      **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *cogl_context = framebuffer->context;
  CoglDisplay *cogl_display = cogl_context->display;
  CoglDisplayEGL *cogl_display_egl = cogl_display->winsys;
  CoglOnscreenEGL *onscreen_egl;
  MetaOnscreenNative *onscreen_native;

  _COGL_RETURN_VAL_IF_FAIL (cogl_display_egl->egl_context, FALSE);

  onscreen->winsys = g_slice_new0 (CoglOnscreenEGL);
  onscreen_egl = onscreen->winsys;

  onscreen_native = g_slice_new0 (MetaOnscreenNative);
  onscreen_egl->platform = onscreen_native;

  /*
   * Don't actually initialize anything here, since we may not have the
   * information available yet, and there is no way to pass it at this stage.
   * To properly allocate a MetaOnscreenNative, the caller must call
   * meta_onscreen_native_allocate() after cogl_framebuffer_allocate().
   *
   * TODO: Turn CoglFramebuffer/CoglOnscreen into GObjects, so it's possible
   * to add backend specific properties.
   */

  return TRUE;
}

static gboolean
meta_onscreen_native_allocate (CoglOnscreen *onscreen,
                               GError      **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native = onscreen_egl->platform;
  MetaRendererNativeGpuData *renderer_gpu_data;
  struct gbm_surface *gbm_surface;
  EGLSurface egl_surface;
  int width;
  int height;
#ifdef HAVE_EGL_DEVICE
  MetaRendererView *view;
  MetaLogicalMonitor *logical_monitor;
  EGLStreamKHR egl_stream;
#endif

  onscreen_native->pending_set_crtc = TRUE;

  /* If a kms_fd is set then the display width and height
   * won't be available until meta_renderer_native_set_layout
   * is called. In that case, defer creating the surface
   * until then.
   */
  width = cogl_framebuffer_get_width (framebuffer);
  height = cogl_framebuffer_get_height (framebuffer);
  if (width == 0 || height == 0)
    return TRUE;

  renderer_gpu_data =
    meta_renderer_native_get_gpu_data (onscreen_native->renderer_native,
                                       onscreen_native->render_gpu);
  switch (renderer_gpu_data->mode)
    {
    case META_RENDERER_NATIVE_MODE_GBM:
      if (!meta_renderer_native_create_surface_gbm (onscreen,
                                                    width, height,
                                                    &gbm_surface,
                                                    &egl_surface,
                                                    error))
        return FALSE;

      onscreen_native->gbm.surface = gbm_surface;
      onscreen_egl->egl_surface = egl_surface;
      break;
#ifdef HAVE_EGL_DEVICE
    case META_RENDERER_NATIVE_MODE_EGL_DEVICE:
      if (!init_dumb_fb (&onscreen_native->egl.dumb_fb,
                         onscreen_native->render_gpu,
                         width, height,
                         GBM_FORMAT_XRGB8888,
                         error))
        return FALSE;

      view = onscreen_native->view;
      logical_monitor = meta_renderer_view_get_logical_monitor (view);
      if (!meta_renderer_native_create_surface_egl_device (onscreen,
                                                           logical_monitor,
                                                           width, height,
                                                           &egl_stream,
                                                           &egl_surface,
                                                           error))
        return FALSE;

      onscreen_native->egl.stream = egl_stream;
      onscreen_egl->egl_surface = egl_surface;
      break;
#endif /* HAVE_EGL_DEVICE */
    }

  return TRUE;
}

static void
meta_renderer_native_release_onscreen (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *cogl_context = framebuffer->context;
  CoglRenderer *cogl_renderer = cogl_context->display->renderer;
  CoglRendererEGL *cogl_renderer_egl = cogl_renderer->winsys;
  CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
  MetaOnscreenNative *onscreen_native;
  MetaRendererNativeGpuData *renderer_gpu_data;

  /* If we never successfully allocated then there's nothing to do */
  if (onscreen_egl == NULL)
    return;

  onscreen_native = onscreen_egl->platform;

  if (onscreen_egl->egl_surface != EGL_NO_SURFACE)
    {
      MetaEgl *egl = meta_onscreen_native_get_egl (onscreen_native);

      meta_egl_destroy_surface (egl,
                                cogl_renderer_egl->edpy,
                                onscreen_egl->egl_surface,
                                NULL);
      onscreen_egl->egl_surface = EGL_NO_SURFACE;
    }

  renderer_gpu_data =
    meta_renderer_native_get_gpu_data (onscreen_native->renderer_native,
                                       onscreen_native->render_gpu);
  switch (renderer_gpu_data->mode)
    {
    case META_RENDERER_NATIVE_MODE_GBM:
      /* flip state takes a reference on the onscreen so there should
       * never be outstanding flips when we reach here. */
      g_return_if_fail (onscreen_native->gbm.next_fb_id == 0);

      free_current_bo (onscreen);

      if (onscreen_native->gbm.surface)
        {
          gbm_surface_destroy (onscreen_native->gbm.surface);
          onscreen_native->gbm.surface = NULL;
        }
      break;
#ifdef HAVE_EGL_DEVICE
    case META_RENDERER_NATIVE_MODE_EGL_DEVICE:
      release_dumb_fb (&onscreen_native->egl.dumb_fb,
                       onscreen_native->render_gpu);
      if (onscreen_native->egl.stream != EGL_NO_STREAM_KHR)
        {
          MetaEgl *egl = meta_onscreen_native_get_egl (onscreen_native);

          meta_egl_destroy_stream (egl,
                                   cogl_renderer_egl->edpy,
                                   onscreen_native->egl.stream,
                                   NULL);
          onscreen_native->egl.stream = EGL_NO_STREAM_KHR;
        }
      break;
#endif /* HAVE_EGL_DEVICE */
    }

  g_hash_table_destroy (onscreen_native->secondary_gpu_states);

  g_slice_free (MetaOnscreenNative, onscreen_native);
  g_slice_free (CoglOnscreenEGL, onscreen->winsys);
  onscreen->winsys = NULL;
}

static const CoglWinsysEGLVtable
_cogl_winsys_egl_vtable = {
  .add_config_attributes = meta_renderer_native_add_egl_config_attributes,
  .choose_config = meta_renderer_native_choose_egl_config,
  .display_setup = meta_renderer_native_setup_egl_display,
  .display_destroy = meta_renderer_native_destroy_egl_display,
  .context_created = meta_renderer_native_egl_context_created,
  .cleanup_context = meta_renderer_native_egl_cleanup_context,
  .context_init = meta_renderer_native_init_egl_context
};

gboolean
meta_renderer_native_supports_mirroring (MetaRendererNative *renderer_native)
{
  MetaMonitorManager *monitor_manager =
    META_MONITOR_MANAGER (renderer_native->monitor_manager_kms);
  GList *l;

  for (l = monitor_manager->gpus; l; l = l->next)
    {
      MetaGpuKms *gpu_kms = META_GPU_KMS (l->data);
      MetaRendererNativeGpuData *renderer_gpu_data;

      renderer_gpu_data = meta_renderer_native_get_gpu_data (renderer_native,
                                                             gpu_kms);
      switch (renderer_gpu_data->mode)
        {
        case META_RENDERER_NATIVE_MODE_GBM:
          break;
#ifdef HAVE_EGL_DEVICE
        case META_RENDERER_NATIVE_MODE_EGL_DEVICE:
          return FALSE;
#endif
        }
    }

  return TRUE;
}

void
meta_renderer_native_queue_modes_reset (MetaRendererNative *renderer_native)
{
  MetaRenderer *renderer = META_RENDERER (renderer_native);
  GList *l;

  for (l = meta_renderer_get_views (renderer); l; l = l->next)
    {
      ClutterStageView *stage_view = l->data;
      CoglFramebuffer *framebuffer =
        clutter_stage_view_get_onscreen (stage_view);
      CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
      CoglOnscreenEGL *onscreen_egl = onscreen->winsys;
      MetaOnscreenNative *onscreen_native = onscreen_egl->platform;

      onscreen_native->pending_set_crtc = TRUE;
    }

  renderer_native->pending_unset_disabled_crtcs = TRUE;
}

static CoglOnscreen *
meta_renderer_native_create_onscreen (MetaRendererNative   *renderer_native,
                                      MetaGpuKms           *render_gpu,
                                      MetaLogicalMonitor   *logical_monitor,
                                      CoglContext          *context,
                                      MetaMonitorTransform  transform,
                                      gint                  view_width,
                                      gint                  view_height,
                                      GError              **error)
{
  CoglOnscreen *onscreen;
  CoglOnscreenEGL *onscreen_egl;
  MetaOnscreenNative *onscreen_native;
  gint width, height;
  GList *l;

  if (meta_monitor_transform_is_rotated (transform))
    {
      width = view_height;
      height = view_width;
    }
  else
    {
      width = view_width;
      height = view_height;
    }

  onscreen = cogl_onscreen_new (context, width, height);
  cogl_onscreen_set_swap_throttled (onscreen,
                                    _clutter_get_sync_to_vblank ());

  if (!cogl_framebuffer_allocate (COGL_FRAMEBUFFER (onscreen), error))
    {
      cogl_object_unref (onscreen);
      return NULL;
    }

  onscreen_egl = onscreen->winsys;
  onscreen_native = onscreen_egl->platform;
  onscreen_native->renderer_native = renderer_native;
  onscreen_native->render_gpu = render_gpu;
  onscreen_native->logical_monitor = logical_monitor;
  onscreen_native->secondary_gpu_states =
    g_hash_table_new_full (NULL, NULL,
                           NULL,
                           (GDestroyNotify) secondary_gpu_state_free);

  for (l = meta_logical_monitor_get_monitors (logical_monitor); l; l = l->next)
    {
      MetaMonitor *monitor = l->data;
      MetaGpuKms *gpu_kms = META_GPU_KMS (meta_monitor_get_gpu (monitor));

      if (gpu_kms == render_gpu)
        continue;

      if (get_secondary_gpu_state (onscreen, gpu_kms))
        continue;

      if (!init_secondary_gpu_state (renderer_native, onscreen, gpu_kms, error))
        {
          cogl_object_unref (onscreen);
          return NULL;
        }
    }

  return onscreen;
}

static CoglOffscreen *
meta_renderer_native_create_offscreen (MetaRendererNative    *renderer,
                                       CoglContext           *context,
                                       MetaMonitorTransform   transform,
                                       gint                   view_width,
                                       gint                   view_height,
                                       GError               **error)
{
  CoglOffscreen *fb;
  CoglTexture2D *tex;

  tex = cogl_texture_2d_new_with_size (context, view_width, view_height);
  cogl_primitive_texture_set_auto_mipmap (COGL_PRIMITIVE_TEXTURE (tex), FALSE);

  if (!cogl_texture_allocate (COGL_TEXTURE (tex), error))
    {
      cogl_object_unref (tex);
      return FALSE;
    }

  fb = cogl_offscreen_new_with_texture (COGL_TEXTURE (tex));
  cogl_object_unref (tex);
  if (!cogl_framebuffer_allocate (COGL_FRAMEBUFFER (fb), error))
    {
      cogl_object_unref (fb);
      return FALSE;
    }

  return fb;
}

static const CoglWinsysVtable *
get_native_cogl_winsys_vtable (CoglRenderer *cogl_renderer)
{
  static gboolean vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  if (!vtable_inited)
    {
      /* The this winsys is a subclass of the EGL winsys so we
         start by copying its vtable */

      parent_vtable = _cogl_winsys_egl_get_vtable ();
      vtable = *parent_vtable;

      vtable.id = COGL_WINSYS_ID_CUSTOM;
      vtable.name = "EGL_KMS";

      vtable.renderer_connect = meta_renderer_native_connect;
      vtable.renderer_disconnect = meta_renderer_native_disconnect;

      vtable.onscreen_init = meta_renderer_native_init_onscreen;
      vtable.onscreen_deinit = meta_renderer_native_release_onscreen;

      /* The KMS winsys doesn't support swap region */
      vtable.onscreen_swap_region = NULL;
      vtable.onscreen_swap_buffers_with_damage =
        meta_onscreen_native_swap_buffers_with_damage;

      vtable_inited = TRUE;
    }

  return &vtable;
}

static CoglRenderer *
create_cogl_renderer_for_gpu (MetaGpuKms *gpu_kms)
{
  CoglRenderer *cogl_renderer;

  cogl_renderer = cogl_renderer_new ();
  cogl_renderer_set_custom_winsys (cogl_renderer,
                                   get_native_cogl_winsys_vtable,
                                   gpu_kms);

  return cogl_renderer;
}

static CoglRenderer *
meta_renderer_native_create_cogl_renderer (MetaRenderer *renderer)
{
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (renderer);
  MetaMonitorManagerKms *monitor_manager_kms =
    renderer_native->monitor_manager_kms;
  MetaGpuKms *primary_gpu =
    meta_monitor_manager_kms_get_primary_gpu (monitor_manager_kms);

  return create_cogl_renderer_for_gpu (primary_gpu);
}

static void
meta_onscreen_native_set_view (CoglOnscreen     *onscreen,
                               MetaRendererView *view)
{
  CoglOnscreenEGL *onscreen_egl;
  MetaOnscreenNative *onscreen_native;

  onscreen_egl = onscreen->winsys;
  onscreen_native = onscreen_egl->platform;
  onscreen_native->view = view;
}

static MetaMonitorTransform
calculate_view_transform (MetaMonitorManager *monitor_manager,
                          MetaLogicalMonitor *logical_monitor)
{
  MetaMonitor *main_monitor;
  MetaOutput *main_output;
  MetaMonitorTransform crtc_transform;
  main_monitor = meta_logical_monitor_get_monitors (logical_monitor)->data;
  main_output = meta_monitor_get_main_output (main_monitor);
  crtc_transform =
    meta_monitor_logical_to_crtc_transform (main_monitor,
                                            logical_monitor->transform);

  /*
   * Pick any monitor and output and check; all CRTCs of a logical monitor will
   * always have the same transform assigned to them.
   */

  if (meta_monitor_manager_is_transform_handled (monitor_manager,
                                                 main_output->crtc,
                                                 crtc_transform))
    return META_MONITOR_TRANSFORM_NORMAL;
  else
    return crtc_transform;
}

static MetaRendererView *
meta_renderer_native_create_view (MetaRenderer       *renderer,
                                  MetaLogicalMonitor *logical_monitor)
{
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (renderer);
  MetaMonitorManagerKms *monitor_manager_kms =
    renderer_native->monitor_manager_kms;
  MetaMonitorManager *monitor_manager =
    META_MONITOR_MANAGER (monitor_manager_kms);
  MetaBackend *backend = meta_monitor_manager_get_backend (monitor_manager);
  ClutterBackend *clutter_backend = meta_backend_get_clutter_backend (backend);
  CoglContext *cogl_context =
    clutter_backend_get_cogl_context (clutter_backend);
  CoglDisplay *cogl_display = cogl_context_get_display (cogl_context);
  MetaGpuKms *primary_gpu;
  CoglDisplayEGL *cogl_display_egl;
  CoglOnscreenEGL *onscreen_egl;
  MetaMonitorTransform view_transform;
  CoglOnscreen *onscreen = NULL;
  CoglOffscreen *offscreen = NULL;
  float scale;
  int width, height;
  MetaRendererView *view;
  GError *error = NULL;

  view_transform = calculate_view_transform (monitor_manager, logical_monitor);

  if (meta_is_stage_views_scaled ())
    scale = meta_logical_monitor_get_scale (logical_monitor);
  else
    scale = 1.0;

  width = roundf (logical_monitor->rect.width * scale);
  height = roundf (logical_monitor->rect.height * scale);

  primary_gpu = meta_monitor_manager_kms_get_primary_gpu (monitor_manager_kms);
  onscreen = meta_renderer_native_create_onscreen (renderer_native,
                                                   primary_gpu,
                                                   logical_monitor,
                                                   cogl_context,
                                                   view_transform,
                                                   width,
                                                   height,
                                                   &error);
  if (!onscreen)
    g_error ("Failed to allocate onscreen framebuffer: %s", error->message);

  if (view_transform != META_MONITOR_TRANSFORM_NORMAL)
    {
      offscreen = meta_renderer_native_create_offscreen (renderer_native,
                                                         cogl_context,
                                                         view_transform,
                                                         width,
                                                         height,
                                                         &error);
      if (!offscreen)
        g_error ("Failed to allocate back buffer texture: %s", error->message);
    }

  view = g_object_new (META_TYPE_RENDERER_VIEW,
                       "layout", &logical_monitor->rect,
                       "scale", scale,
                       "framebuffer", onscreen,
                       "offscreen", offscreen,
                       "logical-monitor", logical_monitor,
                       "transform", view_transform,
                       NULL);
  g_clear_pointer (&offscreen, cogl_object_unref);

  meta_onscreen_native_set_view (onscreen, view);

  if (!meta_onscreen_native_allocate (onscreen, &error))
    {
      g_warning ("Could not create onscreen: %s", error->message);
      cogl_object_unref (onscreen);
      g_object_unref (view);
      g_error_free (error);
      return NULL;
    }

  cogl_object_unref (onscreen);

  /* Ensure we don't point to stale surfaces when creating the offscreen */
  onscreen_egl = onscreen->winsys;
  cogl_display_egl = cogl_display->winsys;
  _cogl_winsys_egl_make_current (cogl_display,
                                 onscreen_egl->egl_surface,
                                 onscreen_egl->egl_surface,
                                 cogl_display_egl->egl_context);

  return view;
}

void
meta_renderer_native_finish_frame (MetaRendererNative *renderer_native)
{
  renderer_native->frame_counter++;

  if (renderer_native->pending_unset_disabled_crtcs)
    {
      MetaMonitorManager *monitor_manager =
        META_MONITOR_MANAGER (renderer_native->monitor_manager_kms);
      GList *l;

      for (l = meta_monitor_manager_get_gpus (monitor_manager); l; l = l->next)
        {
          MetaGpu *gpu = l->data;
          MetaGpuKms *gpu_kms = META_GPU_KMS (gpu);
          GList *k;

          for (k = meta_gpu_get_crtcs (gpu); k; k = k->next)
            {
              MetaCrtc *crtc = k->data;

              if (crtc->current_mode)
                continue;

              meta_gpu_kms_apply_crtc_mode (gpu_kms, crtc, 0, 0, 0);
            }
        }

      renderer_native->pending_unset_disabled_crtcs = FALSE;
    }
}

int64_t
meta_renderer_native_get_frame_counter (MetaRendererNative *renderer_native)
{
  return renderer_native->frame_counter;
}

static void
meta_renderer_native_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (object);

  switch (prop_id)
    {
    case PROP_MONITOR_MANAGER:
      g_value_set_object (value, renderer_native->monitor_manager_kms);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_renderer_native_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (object);

  switch (prop_id)
    {
    case PROP_MONITOR_MANAGER:
      renderer_native->monitor_manager_kms = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
create_secondary_egl_config (MetaEgl   *egl,
                             EGLDisplay egl_display,
                             EGLConfig *egl_config,
                             GError   **error)
{
  EGLint attributes[] = {
    EGL_RED_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_BLUE_SIZE, 1,
    EGL_ALPHA_SIZE, EGL_DONT_CARE,
    EGL_BUFFER_SIZE, EGL_DONT_CARE,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_NONE
  };

  return choose_egl_config_from_gbm_format (egl,
                                            egl_display,
                                            attributes,
                                            GBM_FORMAT_XRGB8888,
                                            egl_config,
                                            error);
}

static EGLContext
create_secondary_egl_context (MetaEgl   *egl,
                              EGLDisplay egl_display,
                              EGLConfig  egl_config,
                              GError   **error)
{
  EGLint attributes[] = {
    EGL_CONTEXT_CLIENT_VERSION, 3,
    EGL_NONE
  };

  return meta_egl_create_context (egl,
                                  egl_display,
                                  egl_config,
                                  EGL_NO_CONTEXT,
                                  attributes,
                                  error);
}

static void
meta_renderer_native_ensure_gles3 (MetaRendererNative *renderer_native)
{
  MetaEgl *egl = meta_renderer_native_get_egl (renderer_native);

  if (renderer_native->gles3)
    return;

  renderer_native->gles3 = meta_gles3_new (egl);
}

static gboolean
init_secondary_gpu_data_gpu (MetaRendererNativeGpuData *renderer_gpu_data,
                             GError                   **error)
{
  MetaRendererNative *renderer_native = renderer_gpu_data->renderer_native;
  MetaEgl *egl = meta_renderer_native_get_egl (renderer_native);
  EGLDisplay egl_display = renderer_gpu_data->egl_display;
  EGLConfig egl_config;
  EGLContext egl_context;
  char **missing_gl_extensions;

  if (!create_secondary_egl_config (egl,egl_display, &egl_config, error))
    return FALSE;

  egl_context = create_secondary_egl_context (egl, egl_display, egl_config, error);
  if (egl_context == EGL_NO_CONTEXT)
    return FALSE;

  meta_renderer_native_ensure_gles3 (renderer_native);

  if (!meta_egl_make_current (egl,
                              egl_display,
                              EGL_NO_SURFACE,
                              EGL_NO_SURFACE,
                              egl_context,
                              error))
    {
      meta_egl_destroy_context (egl, egl_display, egl_context, NULL);
      return FALSE;
    }

  if (!meta_gles3_has_extensions (renderer_native->gles3,
                                  &missing_gl_extensions,
                                  "GL_OES_EGL_image_external",
                                  NULL))
    {
      char *missing_gl_extensions_str;

      missing_gl_extensions_str = g_strjoinv (", ", missing_gl_extensions);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Missing OpenGL ES extensions: %s",
                   missing_gl_extensions_str);
      g_free (missing_gl_extensions_str);
      g_free (missing_gl_extensions);
    }

  renderer_gpu_data->secondary.egl_context = egl_context;
  renderer_gpu_data->secondary.egl_config = egl_config;
  renderer_gpu_data->secondary.copy_mode = META_SHARED_FRAMEBUFFER_COPY_MODE_GPU;

  return TRUE;
}

static void
init_secondary_gpu_data_cpu (MetaRendererNativeGpuData *renderer_gpu_data)
{
  renderer_gpu_data->secondary.copy_mode = META_SHARED_FRAMEBUFFER_COPY_MODE_CPU;
}

static void
init_secondary_gpu_data (MetaRendererNativeGpuData *renderer_gpu_data)
{
  GError *error = NULL;

  if (init_secondary_gpu_data_gpu (renderer_gpu_data, &error))
    return;

  g_warning ("Failed to initialize accelerated iGPU/dGPU framebuffer sharing: %s",
             error->message);
  g_error_free (error);

  init_secondary_gpu_data_cpu (renderer_gpu_data);
}

static MetaRendererNativeGpuData *
create_renderer_gpu_data_gbm (MetaRendererNative  *renderer_native,
                              MetaGpuKms          *gpu_kms,
                              GError             **error)
{
  MetaMonitorManagerKms *monitor_manager_kms;
  MetaEgl *egl = meta_renderer_native_get_egl (renderer_native);
  struct gbm_device *gbm_device;
  EGLDisplay egl_display;
  int kms_fd;
  MetaRendererNativeGpuData *renderer_gpu_data;
  MetaGpuKms *primary_gpu;

  if (!meta_egl_has_extensions (egl, EGL_NO_DISPLAY, NULL,
                                "EGL_MESA_platform_gbm",
                                NULL) &&
      !meta_egl_has_extensions (egl, EGL_NO_DISPLAY, NULL,
                                "EGL_KHR_platform_gbm",
                                NULL))
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Missing extension for GBM renderer: EGL_KHR_platform_gbm");
      return NULL;
    }

  kms_fd = meta_gpu_kms_get_fd (gpu_kms);

  gbm_device = gbm_create_device (kms_fd);
  if (!gbm_device)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to create gbm device: %s", g_strerror (errno));
      return NULL;
    }

  egl_display = meta_egl_get_platform_display (egl,
                                               EGL_PLATFORM_GBM_KHR,
                                               gbm_device, NULL, error);
  if (egl_display == EGL_NO_DISPLAY)
    {
      gbm_device_destroy (gbm_device);
      return NULL;
    }

  if (!meta_egl_initialize (egl, egl_display, error))
    return NULL;

  renderer_gpu_data = meta_create_renderer_native_gpu_data (gpu_kms);
  renderer_gpu_data->renderer_native = renderer_native;
  renderer_gpu_data->gbm.device = gbm_device;
  renderer_gpu_data->mode = META_RENDERER_NATIVE_MODE_GBM;
  renderer_gpu_data->egl_display = egl_display;

  monitor_manager_kms = renderer_native->monitor_manager_kms;
  primary_gpu = meta_monitor_manager_kms_get_primary_gpu (monitor_manager_kms);
  if (gpu_kms != primary_gpu)
    init_secondary_gpu_data (renderer_gpu_data);

  return renderer_gpu_data;
}

#ifdef HAVE_EGL_DEVICE
static const char *
get_drm_device_file (MetaEgl     *egl,
                     EGLDeviceEXT device,
                     GError     **error)
{
  if (!meta_egl_egl_device_has_extensions (egl, device,
                                           NULL,
                                           "EGL_EXT_device_drm",
                                           NULL))
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Missing required EGLDevice extension EGL_EXT_device_drm");
      return NULL;
    }

  return meta_egl_query_device_string (egl, device,
                                       EGL_DRM_DEVICE_FILE_EXT,
                                       error);
}

static EGLDeviceEXT
find_egl_device (MetaRendererNative  *renderer_native,
                 MetaGpuKms          *gpu_kms,
                 GError             **error)
{
  MetaEgl *egl = meta_renderer_native_get_egl (renderer_native);
  char **missing_extensions;
  EGLint num_devices;
  EGLDeviceEXT *devices;
  const char *kms_file_path;
  EGLDeviceEXT device;
  EGLint i;

  if (!meta_egl_has_extensions (egl,
                                EGL_NO_DISPLAY,
                                &missing_extensions,
                                "EGL_EXT_device_base",
                                NULL))
    {
      char *missing_extensions_str;

      missing_extensions_str = g_strjoinv (", ", missing_extensions);
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Missing EGL extensions required for EGLDevice renderer: %s",
                   missing_extensions_str);
      g_free (missing_extensions_str);
      g_free (missing_extensions);
      return EGL_NO_DEVICE_EXT;
    }

  if (!meta_egl_query_devices (egl, 0, NULL, &num_devices, error))
    return EGL_NO_DEVICE_EXT;

  devices = g_new0 (EGLDeviceEXT, num_devices);
  if (!meta_egl_query_devices (egl, num_devices, devices, &num_devices,
                               error))
    {
      g_free (devices);
      return EGL_NO_DEVICE_EXT;
    }

  kms_file_path = meta_gpu_kms_get_file_path (gpu_kms);

  device = EGL_NO_DEVICE_EXT;
  for (i = 0; i < num_devices; i++)
    {
      const char *egl_device_drm_path;

      g_clear_error (error);

      egl_device_drm_path = get_drm_device_file (egl, devices[i], error);
      if (!egl_device_drm_path)
        continue;

      if (g_str_equal (egl_device_drm_path, kms_file_path))
        {
          device = devices[i];
          break;
        }
    }
  g_free (devices);

  if (device == EGL_NO_DEVICE_EXT)
    {
      if (!*error)
        g_set_error (error, G_IO_ERROR,
                     G_IO_ERROR_FAILED,
                     "Failed to find matching EGLDeviceEXT");
      return EGL_NO_DEVICE_EXT;
    }

  return device;
}

static EGLDisplay
get_egl_device_display (MetaRendererNative  *renderer_native,
                        MetaGpuKms          *gpu_kms,
                        EGLDeviceEXT         egl_device,
                        GError             **error)
{
  MetaEgl *egl = meta_renderer_native_get_egl (renderer_native);
  int kms_fd = meta_gpu_kms_get_fd (gpu_kms);
  EGLint platform_attribs[] = {
    EGL_DRM_MASTER_FD_EXT, kms_fd,
    EGL_NONE
  };

  return meta_egl_get_platform_display (egl, EGL_PLATFORM_DEVICE_EXT,
                                        (void *) egl_device,
                                        platform_attribs,
                                        error);
}

static MetaRendererNativeGpuData *
create_renderer_gpu_data_egl_device (MetaRendererNative  *renderer_native,
                                     MetaGpuKms          *gpu_kms,
                                     GError             **error)
{
  MetaMonitorManagerKms *monitor_manager_kms =
    renderer_native->monitor_manager_kms;
  MetaEgl *egl = meta_renderer_native_get_egl (renderer_native);
  MetaGpuKms *primary_gpu;
  char **missing_extensions;
  EGLDeviceEXT egl_device;
  EGLDisplay egl_display;
  MetaRendererNativeGpuData *renderer_gpu_data;

  if (!meta_is_stage_views_enabled())
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "EGLDevice requires stage views enabled");
      return NULL;
    }

  primary_gpu = meta_monitor_manager_kms_get_primary_gpu (monitor_manager_kms);
  if (gpu_kms != primary_gpu)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "EGLDevice currently only works with single GPU systems");
      return NULL;
    }

  egl_device = find_egl_device (renderer_native, gpu_kms, error);
  if (egl_device == EGL_NO_DEVICE_EXT)
    return NULL;

  egl_display = get_egl_device_display (renderer_native, gpu_kms,
                                        egl_device, error);
  if (egl_display == EGL_NO_DISPLAY)
    return NULL;

  if (!meta_egl_initialize (egl, egl_display, error))
    return NULL;

  if (!meta_egl_has_extensions (egl,
                                egl_display,
                                &missing_extensions,
                                "EGL_NV_output_drm_flip_event",
                                "EGL_EXT_output_base",
                                "EGL_EXT_output_drm",
                                "EGL_KHR_stream",
                                "EGL_KHR_stream_producer_eglsurface",
                                "EGL_EXT_stream_consumer_egloutput",
                                "EGL_EXT_stream_acquire_mode",
                                NULL))
    {
      char *missing_extensions_str;

      missing_extensions_str = g_strjoinv (", ", missing_extensions);
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Missing EGL extensions required for EGLDevice renderer: %s",
                   missing_extensions_str);
      g_free (missing_extensions_str);
      g_free (missing_extensions);
      return NULL;
    }

  renderer_gpu_data = meta_create_renderer_native_gpu_data (gpu_kms);
  renderer_gpu_data->renderer_native = renderer_native;
  renderer_gpu_data->egl.device = egl_device;
  renderer_gpu_data->mode = META_RENDERER_NATIVE_MODE_EGL_DEVICE;
  renderer_gpu_data->egl_display = egl_display;

  return renderer_gpu_data;
}
#endif /* HAVE_EGL_DEVICE */

static MetaRendererNativeGpuData *
meta_renderer_native_create_renderer_gpu_data (MetaRendererNative  *renderer_native,
                                               MetaGpuKms          *gpu_kms,
                                               GError             **error)
{
  MetaRendererNativeGpuData *renderer_gpu_data;
  GError *gbm_error = NULL;
#ifdef HAVE_EGL_DEVICE
  GError *egl_device_error = NULL;
#endif

#ifdef HAVE_EGL_DEVICE
  /* Try to initialize the EGLDevice backend first. Whenever we use a
   * non-NVIDIA GPU, the EGLDevice enumeration function won't find a match, and
   * we'll fall back to GBM (which will always succeed as it has a software
   * rendering fallback)
   */
  renderer_gpu_data = create_renderer_gpu_data_egl_device (renderer_native,
                                                           gpu_kms,
                                                           &egl_device_error);
  if (renderer_gpu_data)
    return renderer_gpu_data;
#endif

  renderer_gpu_data = create_renderer_gpu_data_gbm (renderer_native,
                                                    gpu_kms,
                                                    &gbm_error);
  if (renderer_gpu_data)
    {
#ifdef HAVE_EGL_DEVICE
      g_error_free (egl_device_error);
#endif
      return renderer_gpu_data;
    }

  g_set_error (error, G_IO_ERROR,
               G_IO_ERROR_FAILED,
               "Failed to initialize renderer: "
               "%s"
#ifdef HAVE_EGL_DEVICE
               ", %s"
#endif
               , gbm_error->message
#ifdef HAVE_EGL_DEVICE
               , egl_device_error->message
#endif
  );

  g_error_free (gbm_error);
#ifdef HAVE_EGL_DEVICE
  g_error_free (egl_device_error);
#endif

  return NULL;
}

static gboolean
meta_renderer_native_initable_init (GInitable     *initable,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (initable);
  MetaMonitorManagerKms *monitor_manager_kms =
    renderer_native->monitor_manager_kms;
  MetaMonitorManager *monitor_manager =
    META_MONITOR_MANAGER (monitor_manager_kms);
  GList *gpus;
  GList *l;

  gpus = meta_monitor_manager_get_gpus (monitor_manager);
  for (l = gpus; l; l = l->next)
    {
      MetaGpuKms *gpu_kms = META_GPU_KMS (l->data);
      MetaRendererNativeGpuData *renderer_gpu_data;

      renderer_gpu_data =
        meta_renderer_native_create_renderer_gpu_data (renderer_native,
                                                       gpu_kms,
                                                       error);
      if (!renderer_gpu_data)
        return FALSE;

      g_hash_table_insert (renderer_native->gpu_datas,
                           gpu_kms,
                           renderer_gpu_data);
    }

  return TRUE;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = meta_renderer_native_initable_init;
}

static void
meta_renderer_native_finalize (GObject *object)
{
  MetaRendererNative *renderer_native = META_RENDERER_NATIVE (object);

  g_hash_table_destroy (renderer_native->gpu_datas);
  g_clear_object (&renderer_native->gles3);

  G_OBJECT_CLASS (meta_renderer_native_parent_class)->finalize (object);
}

static void
meta_renderer_native_init (MetaRendererNative *renderer_native)
{
  renderer_native->gpu_datas =
    g_hash_table_new_full (NULL, NULL,
                           NULL,
                           (GDestroyNotify) meta_renderer_native_gpu_data_free);
}

static void
meta_renderer_native_class_init (MetaRendererNativeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MetaRendererClass *renderer_class = META_RENDERER_CLASS (klass);

  object_class->get_property = meta_renderer_native_get_property;
  object_class->set_property = meta_renderer_native_set_property;
  object_class->finalize = meta_renderer_native_finalize;

  renderer_class->create_cogl_renderer = meta_renderer_native_create_cogl_renderer;
  renderer_class->create_view = meta_renderer_native_create_view;

  obj_props[PROP_MONITOR_MANAGER] =
    g_param_spec_object ("monitor-manager",
                         "monitor-manager",
                         "MetaMonitorManagerKms",
                         META_TYPE_MONITOR_MANAGER_KMS,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

MetaRendererNative *
meta_renderer_native_new (MetaMonitorManagerKms *monitor_manager_kms,
                          GError               **error)
{
  return g_initable_new (META_TYPE_RENDERER_NATIVE,
                         NULL,
                         error,
                         "monitor-manager", monitor_manager_kms,
                         NULL);
}
