/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010,2011,2013 Intel Corporation.
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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "cogl-config.h"
#endif

#include "cogl-i18n-private.h"
#include "cogl-util.h"
#include "cogl-winsys-private.h"
#include "cogl-feature-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer.h"
#include "cogl-swap-chain-private.h"
#include "cogl-renderer-private.h"
#include "cogl-glx-renderer-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-glx-display-private.h"
#include "cogl-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-rectangle-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-frame-info-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-swap-chain-private.h"
#include "cogl-xlib-renderer.h"
#include "cogl-util.h"
#include "cogl-winsys-glx-private.h"
#include "cogl-error-private.h"
#include "cogl-poll-private.h"
#include "cogl-version.h"
#include "cogl-glx.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <GL/glx.h>
#include <X11/Xlib.h>

#include <glib.h>

/* This is a relatively new extension */
#ifndef GLX_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV
#define GLX_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV 0x20F7
#endif

#define COGL_ONSCREEN_X11_EVENT_MASK (StructureNotifyMask | ExposureMask)
#define MAX_GLX_CONFIG_ATTRIBS 30

typedef struct _CoglContextGLX
{
  GLXDrawable current_drawable;
} CoglContextGLX;

typedef struct _CoglOnscreenXlib
{
  Window xwin;
  int x, y;
  CoglBool is_foreign_xwin;
  CoglOutput *output;
} CoglOnscreenXlib;

typedef struct _CoglOnscreenGLX
{
  CoglOnscreenXlib _parent;
  GLXDrawable glxwin;
  uint32_t last_swap_vsync_counter;
  CoglBool pending_sync_notify;
  CoglBool pending_complete_notify;
  CoglBool pending_resize_notify;

  GThread *swap_wait_thread;
  GQueue *swap_wait_queue;
  GCond swap_wait_cond;
  GMutex swap_wait_mutex;
  int swap_wait_pipe[2];
  GLXContext swap_wait_context;
  CoglBool closing_down;
} CoglOnscreenGLX;

typedef struct _CoglPixmapTextureEyeGLX
{
  CoglTexture *glx_tex;
  CoglBool bind_tex_image_queued;
  CoglBool pixmap_bound;
} CoglPixmapTextureEyeGLX;

typedef struct _CoglTexturePixmapGLX
{
  GLXPixmap glx_pixmap;
  CoglBool has_mipmap_space;
  CoglBool can_mipmap;

  CoglPixmapTextureEyeGLX left;
  CoglPixmapTextureEyeGLX right;
} CoglTexturePixmapGLX;

/* Define a set of arrays containing the functions required from GL
   for each winsys feature */
#define COGL_WINSYS_FEATURE_BEGIN(major_version, minor_version,         \
                                  name, namespaces, extension_names,    \
                                  feature_flags,                        \
                                  winsys_feature)                       \
  static const CoglFeatureFunction                                      \
  cogl_glx_feature_ ## name ## _funcs[] = {
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)                   \
  { G_STRINGIFY (name), G_STRUCT_OFFSET (CoglGLXRenderer, name) },
#define COGL_WINSYS_FEATURE_END()               \
  { NULL, 0 },                                  \
    };
#include "cogl-winsys-glx-feature-functions.h"

/* Define an array of features */
#undef COGL_WINSYS_FEATURE_BEGIN
#define COGL_WINSYS_FEATURE_BEGIN(major_version, minor_version,         \
                                  name, namespaces, extension_names,    \
                                  feature_flags,                        \
                                  winsys_feature)                       \
  { major_version, minor_version,                                       \
      0, namespaces, extension_names,                                   \
      feature_flags,                                                    \
      0,                                                                \
      winsys_feature, \
      cogl_glx_feature_ ## name ## _funcs },
#undef COGL_WINSYS_FEATURE_FUNCTION
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)
#undef COGL_WINSYS_FEATURE_END
#define COGL_WINSYS_FEATURE_END()

static const CoglFeatureData winsys_feature_data[] =
  {
#include "cogl-winsys-glx-feature-functions.h"
  };

static CoglFuncPtr
_cogl_winsys_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name,
                                        CoglBool in_core)
{
  CoglGLXRenderer *glx_renderer = renderer->winsys;

  /* The GLX_ARB_get_proc_address extension documents that this should
   * work for core functions too so we don't need to do anything
   * special with in_core */

  return glx_renderer->glXGetProcAddress ((const GLubyte *) name);
}

static CoglOnscreen *
find_onscreen_for_xid (CoglContext *context, uint32_t xid)
{
  GList *l;

  for (l = context->framebuffers; l; l = l->next)
    {
      CoglFramebuffer *framebuffer = l->data;
      CoglOnscreenXlib *xlib_onscreen;

      if (framebuffer->type != COGL_FRAMEBUFFER_TYPE_ONSCREEN)
        continue;

      /* Does the GLXEvent have the GLXDrawable or the X Window? */
      xlib_onscreen = COGL_ONSCREEN (framebuffer)->winsys;
      if (xlib_onscreen != NULL && xlib_onscreen->xwin == (Window)xid)
        return COGL_ONSCREEN (framebuffer);
    }

  return NULL;
}

static int64_t
get_monotonic_time_ns (void)
{
  struct timespec ts;

  clock_gettime (CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * G_GINT64_CONSTANT (1000000000) + ts.tv_nsec;
}

static void
ensure_ust_type (CoglRenderer *renderer,
                 GLXDrawable drawable)
{
  CoglGLXRenderer *glx_renderer =  renderer->winsys;
  CoglXlibRenderer *xlib_renderer = _cogl_xlib_renderer_get_data (renderer);
  int64_t ust;
  int64_t msc;
  int64_t sbc;
  struct timeval tv;
  int64_t current_system_time;
  int64_t current_monotonic_time;

  if (glx_renderer->ust_type != COGL_GLX_UST_IS_UNKNOWN)
    return;

  glx_renderer->ust_type = COGL_GLX_UST_IS_OTHER;

  if (glx_renderer->glXGetSyncValues == NULL)
    goto out;

  if (!glx_renderer->glXGetSyncValues (xlib_renderer->xdpy, drawable,
                                       &ust, &msc, &sbc))
    goto out;

  /* This is the time source that existing (buggy) linux drm drivers
   * use */
  gettimeofday (&tv, NULL);
  current_system_time = (tv.tv_sec * G_GINT64_CONSTANT (1000000)) + tv.tv_usec;

  if (current_system_time > ust - 1000000 &&
      current_system_time < ust + 1000000)
    {
      glx_renderer->ust_type = COGL_GLX_UST_IS_GETTIMEOFDAY;
      goto out;
    }

  /* This is the time source that the newer (fixed) linux drm
   * drivers use (Linux >= 3.8) */
  current_monotonic_time = get_monotonic_time_ns () / 1000;

  if (current_monotonic_time > ust - 1000000 &&
      current_monotonic_time < ust + 1000000)
    {
      glx_renderer->ust_type = COGL_GLX_UST_IS_MONOTONIC_TIME;
      goto out;
    }

 out:
  COGL_NOTE (WINSYS, "Classified OML system time as: %s",
             glx_renderer->ust_type == COGL_GLX_UST_IS_GETTIMEOFDAY ? "gettimeofday" :
             (glx_renderer->ust_type == COGL_GLX_UST_IS_MONOTONIC_TIME ? "monotonic" :
              "other"));
  return;
}

static int64_t
ust_to_nanoseconds (CoglRenderer *renderer,
                    GLXDrawable drawable,
                    int64_t ust)
{
  CoglGLXRenderer *glx_renderer =  renderer->winsys;

  ensure_ust_type (renderer, drawable);

  switch (glx_renderer->ust_type)
    {
    case COGL_GLX_UST_IS_UNKNOWN:
      g_assert_not_reached ();
      break;
    case COGL_GLX_UST_IS_GETTIMEOFDAY:
    case COGL_GLX_UST_IS_MONOTONIC_TIME:
      return 1000 * ust;
    case COGL_GLX_UST_IS_OTHER:
      /* In this case the scale of UST is undefined so we can't easily
       * scale to nanoseconds.
       *
       * For example the driver may be reporting the rdtsc CPU counter
       * as UST values and so the scale would need to be determined
       * empirically.
       *
       * Potentially we could block for a known duration within
       * ensure_ust_type() to measure the timescale of UST but for now
       * we just ignore unknown time sources */
      return 0;
    }

  return 0;
}

static int64_t
_cogl_winsys_get_clock_time (CoglContext *context)
{
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;

  if (!glx_renderer->glXWaitForMsc)
    return get_monotonic_time_ns ();

  /* We don't call ensure_ust_type() because we don't have a drawable
   * to work with. cogl_get_clock_time() is documented to only work
   * once a valid, non-zero, timestamp has been retrieved from Cogl.
   */

  switch (glx_renderer->ust_type)
    {
    case COGL_GLX_UST_IS_UNKNOWN:
    case COGL_GLX_UST_IS_OTHER:
      return 0;
    case COGL_GLX_UST_IS_GETTIMEOFDAY:
      {
        struct timeval tv;

        gettimeofday(&tv, NULL);
        return tv.tv_sec * G_GINT64_CONSTANT (1000000000) +
          tv.tv_usec * G_GINT64_CONSTANT (1000);
      }
    case COGL_GLX_UST_IS_MONOTONIC_TIME:
      {
        return get_monotonic_time_ns ();
      }
    }

  g_assert_not_reached();
  return 0;
}

static void
flush_pending_notifications_cb (void *data,
                                void *user_data)
{
  CoglFramebuffer *framebuffer = data;

  if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
    {
      CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
      CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
      CoglBool pending_sync_notify = glx_onscreen->pending_sync_notify;
      CoglBool pending_complete_notify = glx_onscreen->pending_complete_notify;

      /* If swap_region is called then notifying the sync event could
       * potentially immediately queue a subsequent pending notify so
       * we need to clear the flag before invoking the callback */
      glx_onscreen->pending_sync_notify = FALSE;
      glx_onscreen->pending_complete_notify = FALSE;

      if (pending_sync_notify)
        {
          CoglFrameInfo *info = g_queue_peek_head (&onscreen->pending_frame_infos);

          _cogl_onscreen_notify_frame_sync (onscreen, info);
        }

      if (pending_complete_notify)
        {
          CoglFrameInfo *info = g_queue_pop_head (&onscreen->pending_frame_infos);

          _cogl_onscreen_notify_complete (onscreen, info);

          cogl_object_unref (info);
        }

      if (glx_onscreen->pending_resize_notify)
        {
          _cogl_onscreen_notify_resize (onscreen);
          glx_onscreen->pending_resize_notify = FALSE;
        }
    }
}

static void
flush_pending_notifications_idle (void *user_data)
{
  CoglContext *context = user_data;
  CoglRenderer *renderer = context->display->renderer;
  CoglGLXRenderer *glx_renderer = renderer->winsys;

  /* This needs to be disconnected before invoking the callbacks in
   * case the callbacks cause it to be queued again */
  _cogl_closure_disconnect (glx_renderer->flush_notifications_idle);
  glx_renderer->flush_notifications_idle = NULL;

  g_list_foreach (context->framebuffers,
                  flush_pending_notifications_cb,
                  NULL);
}

static void
set_sync_pending (CoglOnscreen *onscreen)
{
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglGLXRenderer *glx_renderer = renderer->winsys;

  /* We only want to dispatch sync events when the application calls
   * cogl_context_dispatch so instead of immediately notifying we
   * queue an idle callback */
  if (!glx_renderer->flush_notifications_idle)
    {
      glx_renderer->flush_notifications_idle =
        _cogl_poll_renderer_add_idle (renderer,
                                      flush_pending_notifications_idle,
                                      context,
                                      NULL);
    }

  glx_onscreen->pending_sync_notify = TRUE;
}

static void
set_complete_pending (CoglOnscreen *onscreen)
{
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglGLXRenderer *glx_renderer = renderer->winsys;

  /* We only want to notify swap completion when the application calls
   * cogl_context_dispatch so instead of immediately notifying we
   * queue an idle callback */
  if (!glx_renderer->flush_notifications_idle)
    {
      glx_renderer->flush_notifications_idle =
        _cogl_poll_renderer_add_idle (renderer,
                                      flush_pending_notifications_idle,
                                      context,
                                      NULL);
    }

  glx_onscreen->pending_complete_notify = TRUE;
}

static void
notify_swap_buffers (CoglContext *context, GLXBufferSwapComplete *swap_event)
{
  CoglOnscreen *onscreen = find_onscreen_for_xid (context, (uint32_t)swap_event->drawable);
  CoglOnscreenGLX *glx_onscreen;

  if (!onscreen)
    return;
  glx_onscreen = onscreen->winsys;

  /* We only want to notify that the swap is complete when the
     application calls cogl_context_dispatch so instead of immediately
     notifying we'll set a flag to remember to notify later */
  set_sync_pending (onscreen);

  if (swap_event->ust != 0)
    {
      CoglFrameInfo *info = g_queue_peek_head (&onscreen->pending_frame_infos);

      info->presentation_time =
        ust_to_nanoseconds (context->display->renderer,
                            glx_onscreen->glxwin,
                            swap_event->ust);
    }

  set_complete_pending (onscreen);
}

static void
update_output (CoglOnscreen *onscreen)
{
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglOutput *output;
  int width, height;

  width = cogl_framebuffer_get_width (framebuffer);
  height = cogl_framebuffer_get_height (framebuffer);
  output = _cogl_xlib_renderer_output_for_rectangle (display->renderer,
                                                     xlib_onscreen->x,
                                                     xlib_onscreen->y,
                                                     width, height);
  if (xlib_onscreen->output != output)
    {
      if (xlib_onscreen->output)
        cogl_object_unref (xlib_onscreen->output);

      xlib_onscreen->output = output;

      if (output)
        cogl_object_ref (xlib_onscreen->output);
    }
}

static void
notify_resize (CoglContext *context,
               XConfigureEvent *configure_event)
{
  CoglOnscreen *onscreen = find_onscreen_for_xid (context,
                                                  configure_event->window);
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglRenderer *renderer = context->display->renderer;
  CoglGLXRenderer *glx_renderer = renderer->winsys;
  CoglOnscreenGLX *glx_onscreen;
  CoglOnscreenXlib *xlib_onscreen;

  if (!onscreen)
    return;

  glx_onscreen = onscreen->winsys;
  xlib_onscreen = onscreen->winsys;

  _cogl_framebuffer_winsys_update_size (framebuffer,
                                        configure_event->width,
                                        configure_event->height);

  /* We only want to notify that a resize happened when the
   * application calls cogl_context_dispatch so instead of immediately
   * notifying we queue an idle callback */
  if (!glx_renderer->flush_notifications_idle)
    {
      glx_renderer->flush_notifications_idle =
        _cogl_poll_renderer_add_idle (renderer,
                                      flush_pending_notifications_idle,
                                      context,
                                      NULL);
    }

  glx_onscreen->pending_resize_notify = TRUE;

  if (!xlib_onscreen->is_foreign_xwin)
    {
      int x, y;

      if (configure_event->send_event)
        {
          x = configure_event->x;
          y = configure_event->y;
        }
      else
        {
          Window child;
          XTranslateCoordinates (configure_event->display,
                                 configure_event->window,
                                 DefaultRootWindow (configure_event->display),
                                 0, 0, &x, &y, &child);
        }

      xlib_onscreen->x = x;
      xlib_onscreen->y = y;

      update_output (onscreen);
    }
}

static CoglFilterReturn
glx_event_filter_cb (XEvent *xevent, void *data)
{
  CoglContext *context = data;
#ifdef GLX_INTEL_swap_event
  CoglGLXRenderer *glx_renderer;
#endif

  if (xevent->type == ConfigureNotify)
    {
      notify_resize (context,
                     &xevent->xconfigure);

      /* we let ConfigureNotify pass through */
      return COGL_FILTER_CONTINUE;
    }

#ifdef GLX_INTEL_swap_event
  glx_renderer = context->display->renderer->winsys;

  if (xevent->type == (glx_renderer->glx_event_base + GLX_BufferSwapComplete))
    {
      GLXBufferSwapComplete *swap_event = (GLXBufferSwapComplete *) xevent;

      notify_swap_buffers (context, swap_event);

      /* remove SwapComplete events from the queue */
      return COGL_FILTER_REMOVE;
    }
#endif /* GLX_INTEL_swap_event */

  if (xevent->type == Expose)
    {
      CoglOnscreen *onscreen =
        find_onscreen_for_xid (context, xevent->xexpose.window);

      if (onscreen)
        {
          CoglOnscreenDirtyInfo info;

          info.x = xevent->xexpose.x;
          info.y = xevent->xexpose.y;
          info.width = xevent->xexpose.width;
          info.height = xevent->xexpose.height;

          _cogl_onscreen_queue_dirty (onscreen, &info);
        }

      return COGL_FILTER_CONTINUE;
    }

  return COGL_FILTER_CONTINUE;
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglGLXRenderer *glx_renderer = renderer->winsys;

  _cogl_xlib_renderer_disconnect (renderer);

  if (glx_renderer->libgl_module)
    g_module_close (glx_renderer->libgl_module);

  g_slice_free (CoglGLXRenderer, renderer->winsys);
}

static CoglBool
update_all_outputs (CoglRenderer *renderer)
{
  GList *l;

  _COGL_GET_CONTEXT (context, FALSE);

  if (context->display == NULL) /* during connection */
    return FALSE;

  if (context->display->renderer != renderer)
    return FALSE;

  for (l = context->framebuffers; l; l = l->next)
    {
      CoglFramebuffer *framebuffer = l->data;

      if (framebuffer->type != COGL_FRAMEBUFFER_TYPE_ONSCREEN)
        continue;

      update_output (COGL_ONSCREEN (framebuffer));
    }

  return TRUE;
}

static void
_cogl_winsys_renderer_outputs_changed (CoglRenderer *renderer)
{
  update_all_outputs (renderer);
}

static CoglBool
resolve_core_glx_functions (CoglRenderer *renderer,
                            CoglError **error)
{
  CoglGLXRenderer *glx_renderer;

  glx_renderer = renderer->winsys;

  if (!g_module_symbol (glx_renderer->libgl_module, "glXQueryExtension",
                        (void **) &glx_renderer->glXQueryExtension) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXQueryVersion",
                        (void **) &glx_renderer->glXQueryVersion) ||
      !g_module_symbol (glx_renderer->libgl_module, "glXQueryExtensionsString",
                        (void **) &glx_renderer->glXQueryExtensionsString) ||
      (!g_module_symbol (glx_renderer->libgl_module, "glXGetProcAddress",
                         (void **) &glx_renderer->glXGetProcAddress) &&
       !g_module_symbol (glx_renderer->libgl_module, "glXGetProcAddressARB",
                         (void **) &glx_renderer->glXGetProcAddress)) ||
       !g_module_symbol (glx_renderer->libgl_module, "glXQueryDrawable",
                         (void **) &glx_renderer->glXQueryDrawable))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Failed to resolve required GLX symbol");
      return FALSE;
    }

  return TRUE;
}

static void
update_base_winsys_features (CoglRenderer *renderer)
{
  CoglGLXRenderer *glx_renderer = renderer->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (renderer);
  const char *glx_extensions;
  int default_screen;
  char **split_extensions;
  int i;

  default_screen = DefaultScreen (xlib_renderer->xdpy);
  glx_extensions =
    glx_renderer->glXQueryExtensionsString (xlib_renderer->xdpy,
                                            default_screen);

  COGL_NOTE (WINSYS, "  GLX Extensions: %s", glx_extensions);

  split_extensions = g_strsplit (glx_extensions, " ", 0 /* max_tokens */);

  for (i = 0; i < G_N_ELEMENTS (winsys_feature_data); i++)
    if (_cogl_feature_check (renderer,
                             "GLX", winsys_feature_data + i,
                             glx_renderer->glx_major,
                             glx_renderer->glx_minor,
                             COGL_DRIVER_GL, /* the driver isn't used */
                             split_extensions,
                             glx_renderer))
      {
        glx_renderer->legacy_feature_flags |=
          winsys_feature_data[i].feature_flags;
        if (winsys_feature_data[i].winsys_feature)
          COGL_FLAGS_SET (glx_renderer->base_winsys_features,
                          winsys_feature_data[i].winsys_feature,
                          TRUE);
      }

  g_strfreev (split_extensions);

  /* The GLX_SGI_video_sync spec explicitly states this extension
   * only works for direct contexts; we don't know per-renderer
   * if the context is direct or not, so we turn off the feature
   * flag; we still use the extension within this file looking
   * instead at glx_display->have_vblank_counter.
   */
  COGL_FLAGS_SET (glx_renderer->base_winsys_features,
                  COGL_WINSYS_FEATURE_VBLANK_COUNTER,
                  FALSE);


  COGL_FLAGS_SET (glx_renderer->base_winsys_features,
                  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                  TRUE);

  /* Because of the direct-context dependency, the VBLANK_WAIT feature
   * doesn't reflect the presence of GLX_SGI_video_sync.
   */
  if (glx_renderer->glXWaitForMsc)
    COGL_FLAGS_SET (glx_renderer->base_winsys_features,
                    COGL_WINSYS_FEATURE_VBLANK_WAIT,
                    TRUE);
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               CoglError **error)
{
  CoglGLXRenderer *glx_renderer;
  CoglXlibRenderer *xlib_renderer;

  renderer->winsys = g_slice_new0 (CoglGLXRenderer);

  glx_renderer = renderer->winsys;
  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  if (!_cogl_xlib_renderer_connect (renderer, error))
    goto error;

  if (renderer->driver != COGL_DRIVER_GL &&
      renderer->driver != COGL_DRIVER_GL3)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "GLX Backend can only be used in conjunction with OpenGL");
      goto error;
    }

  glx_renderer->libgl_module = g_module_open (COGL_GL_LIBNAME,
                                              G_MODULE_BIND_LAZY);

  if (glx_renderer->libgl_module == NULL)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Failed to dynamically open the OpenGL library");
      goto error;
    }

  if (!resolve_core_glx_functions (renderer, error))
    goto error;

  if (!glx_renderer->glXQueryExtension (xlib_renderer->xdpy,
                                        &glx_renderer->glx_error_base,
                                        &glx_renderer->glx_event_base))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "XServer appears to lack required GLX support");
      goto error;
    }

  /* XXX: Note: For a long time Mesa exported a hybrid GLX, exporting
   * extensions specified to require GLX 1.3, but still reporting 1.2
   * via glXQueryVersion. */
  if (!glx_renderer->glXQueryVersion (xlib_renderer->xdpy,
                                      &glx_renderer->glx_major,
                                      &glx_renderer->glx_minor)
      || !(glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 2))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "XServer appears to lack required GLX 1.2 support");
      goto error;
    }

  update_base_winsys_features (renderer);

  glx_renderer->dri_fd = -1;

  return TRUE;

error:
  _cogl_winsys_renderer_disconnect (renderer);
  return FALSE;
}

static CoglBool
update_winsys_features (CoglContext *context, CoglError **error)
{
  CoglGLXDisplay *glx_display = context->display->winsys;
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;

  _COGL_RETURN_VAL_IF_FAIL (glx_display->glx_context, FALSE);

  if (!_cogl_context_update_features (context, error))
    return FALSE;

  memcpy (context->winsys_features,
          glx_renderer->base_winsys_features,
          sizeof (context->winsys_features));

  context->feature_flags |= glx_renderer->legacy_feature_flags;

  context->feature_flags |= COGL_FEATURE_ONSCREEN_MULTIPLE;
  COGL_FLAGS_SET (context->features,
                  COGL_FEATURE_ID_ONSCREEN_MULTIPLE, TRUE);

  if (glx_renderer->glXCopySubBuffer || context->glBlitFramebuffer)
    {
      CoglGpuInfo *info = &context->gpu;
      CoglGpuInfoArchitecture arch = info->architecture;

      COGL_FLAGS_SET (context->winsys_features, COGL_WINSYS_FEATURE_SWAP_REGION, TRUE);

      /*
       * "The "drisw" binding in Mesa for loading sofware renderers is
       * broken, and neither glBlitFramebuffer nor glXCopySubBuffer
       * work correctly."
       * - ajax
       * - https://bugzilla.gnome.org/show_bug.cgi?id=674208
       *
       * This is broken in software Mesa at least as of 7.10 and got
       * fixed in Mesa 10.1
       */

      if (info->driver_package == COGL_GPU_INFO_DRIVER_PACKAGE_MESA &&
          info->driver_package_version < COGL_VERSION_ENCODE (10, 1, 0) &&
          (arch == COGL_GPU_INFO_ARCHITECTURE_LLVMPIPE ||
           arch == COGL_GPU_INFO_ARCHITECTURE_SOFTPIPE ||
           arch == COGL_GPU_INFO_ARCHITECTURE_SWRAST))
	{
	  COGL_FLAGS_SET (context->winsys_features,
			  COGL_WINSYS_FEATURE_SWAP_REGION, FALSE);
	}
    }

  /* Note: glXCopySubBuffer and glBlitFramebuffer won't be throttled
   * by the SwapInterval so we have to throttle swap_region requests
   * manually... */
  if (_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_REGION) &&
      (glx_display->have_vblank_counter || glx_display->can_vblank_wait))
    COGL_FLAGS_SET (context->winsys_features,
                    COGL_WINSYS_FEATURE_SWAP_REGION_THROTTLE, TRUE);

  if (_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT))
    {
      COGL_FLAGS_SET (context->winsys_features,
		      COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT, TRUE);
      /* TODO: remove this deprecated feature */
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_SWAP_BUFFERS_EVENT,
                      TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_PRESENTATION_TIME,
                      TRUE);
    }
  else
    {
      CoglGpuInfo *info = &context->gpu;
      if (glx_display->have_vblank_counter &&
	  context->display->renderer->xlib_enable_threaded_swap_wait &&
	  info->vendor == COGL_GPU_INFO_VENDOR_NVIDIA)
        {
          COGL_FLAGS_SET (context->winsys_features,
                          COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT, TRUE);
          COGL_FLAGS_SET (context->winsys_features,
                          COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT, TRUE);
          /* TODO: remove this deprecated feature */
          COGL_FLAGS_SET (context->features,
                          COGL_FEATURE_ID_SWAP_BUFFERS_EVENT,
                          TRUE);
          COGL_FLAGS_SET (context->features,
                          COGL_FEATURE_ID_PRESENTATION_TIME,
                          TRUE);
          COGL_FLAGS_SET (context->private_features,
                          COGL_PRIVATE_FEATURE_THREADED_SWAP_WAIT,
                          TRUE);
        }
    }

  /* We'll manually handle queueing dirty events in response to
   * Expose events from X */
  COGL_FLAGS_SET (context->private_features,
                  COGL_PRIVATE_FEATURE_DIRTY_EVENTS,
                  TRUE);

  if (_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_BUFFER_AGE))
    COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_BUFFER_AGE, TRUE);

  return TRUE;
}

static void
glx_attributes_from_framebuffer_config (CoglDisplay *display,
                                        CoglFramebufferConfig *config,
                                        int *attributes)
{
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;
  int i = 0;

  attributes[i++] = GLX_DRAWABLE_TYPE;
  attributes[i++] = GLX_WINDOW_BIT;

  attributes[i++] = GLX_RENDER_TYPE;
  attributes[i++] = GLX_RGBA_BIT;

  attributes[i++] = GLX_DOUBLEBUFFER;
  attributes[i++] = GL_TRUE;

  attributes[i++] = GLX_RED_SIZE;
  attributes[i++] = 1;
  attributes[i++] = GLX_GREEN_SIZE;
  attributes[i++] = 1;
  attributes[i++] = GLX_BLUE_SIZE;
  attributes[i++] = 1;
  attributes[i++] = GLX_ALPHA_SIZE;
  attributes[i++] = config->swap_chain->has_alpha ? 1 : GLX_DONT_CARE;
  attributes[i++] = GLX_DEPTH_SIZE;
  attributes[i++] = 1;
  attributes[i++] = GLX_STENCIL_SIZE;
  attributes[i++] = config->need_stencil ? 1: GLX_DONT_CARE;
  if (config->stereo_enabled)
    {
      attributes[i++] = GLX_STEREO;
      attributes[i++] = TRUE;
    }

  if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 4 &&
      config->samples_per_pixel)
    {
      attributes[i++] = GLX_SAMPLE_BUFFERS;
      attributes[i++] = 1;
      attributes[i++] = GLX_SAMPLES;
      attributes[i++] = config->samples_per_pixel;
    }

  attributes[i++] = None;

  g_assert (i < MAX_GLX_CONFIG_ATTRIBS);
}

/* It seems the GLX spec never defined an invalid GLXFBConfig that
 * we could overload as an indication of error, so we have to return
 * an explicit boolean status. */
static CoglBool
find_fbconfig (CoglDisplay *display,
               CoglFramebufferConfig *config,
               GLXFBConfig *config_ret,
               CoglError **error)
{
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (display->renderer);
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;
  GLXFBConfig *configs = NULL;
  int n_configs;
  static int attributes[MAX_GLX_CONFIG_ATTRIBS];
  CoglBool ret = TRUE;
  int xscreen_num = DefaultScreen (xlib_renderer->xdpy);

  glx_attributes_from_framebuffer_config (display, config, attributes);

  configs = glx_renderer->glXChooseFBConfig (xlib_renderer->xdpy,
                                             xscreen_num,
                                             attributes,
                                             &n_configs);

  if (!configs || n_configs == 0)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Failed to find any compatible fbconfigs");
      ret = FALSE;
      goto done;
    }

  if (config->swap_chain->has_alpha)
    {
      int i;

      for (i = 0; i < n_configs; i++)
        {
          XVisualInfo *vinfo;

          vinfo = glx_renderer->glXGetVisualFromFBConfig (xlib_renderer->xdpy,
                                                          configs[i]);
          if (vinfo == NULL)
            continue;

          if (vinfo->depth == 32 &&
              (vinfo->red_mask | vinfo->green_mask | vinfo->blue_mask)
              != 0xffffffff)
            {
              COGL_NOTE (WINSYS, "Found an ARGB FBConfig [index:%d]", i);
              *config_ret = configs[i];
              goto done;
            }
        }

      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to find fbconfig with rgba visual");
      ret = FALSE;
      goto done;
    }
  else
    {
      COGL_NOTE (WINSYS, "Using the first available FBConfig");
      *config_ret = configs[0];
    }

done:
  XFree (configs);
  return ret;
}

static GLXContext
create_gl3_context (CoglDisplay *display,
                    GLXFBConfig fb_config)
{
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (display->renderer);
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;

  /* We want a core profile 3.1 context with no deprecated features */
  static const int attrib_list[] =
    {
      GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
      GLX_CONTEXT_MINOR_VERSION_ARB, 1,
      GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
      GLX_CONTEXT_FLAGS_ARB,         GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
      None
    };
  /* NV_robustness_video_memory_purge relies on GLX_ARB_create_context
     and in part on ARB_robustness. Namely, it needs the notification
     strategy to be set to GLX_LOSE_CONTEXT_ON_RESET_ARB and that the
     driver exposes the GetGraphicsResetStatusARB function. This means
     we don't actually enable robust buffer access. */
  static const int attrib_list_reset_on_purge[] =
    {
      GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
      GLX_CONTEXT_MINOR_VERSION_ARB, 1,
      GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
      GLX_CONTEXT_FLAGS_ARB,         GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
      GLX_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV,
                                     GL_TRUE,
      GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB,
                                     GLX_LOSE_CONTEXT_ON_RESET_ARB,
      None
    };

  /* Make sure that the display supports the GLX_ARB_create_context
     extension */
  if (glx_renderer->glXCreateContextAttribs == NULL)
    return NULL;

  /* We can't check the presence of this extension with the usual
     COGL_WINSYS_FEATURE machinery because that only gets initialized
     later when the CoglContext is created. */
  if (display->renderer->xlib_want_reset_on_video_memory_purge &&
      strstr (glx_renderer->glXQueryExtensionsString (xlib_renderer->xdpy,
                                                      DefaultScreen (xlib_renderer->xdpy)),
              "GLX_NV_robustness_video_memory_purge"))
    {
      CoglXlibTrapState old_state;
      GLXContext ctx;

      _cogl_xlib_renderer_trap_errors (display->renderer, &old_state);
      ctx = glx_renderer->glXCreateContextAttribs (xlib_renderer->xdpy,
                                                   fb_config,
                                                   NULL /* share_context */,
                                                   True, /* direct */
                                                   attrib_list_reset_on_purge);
      if (!_cogl_xlib_renderer_untrap_errors (display->renderer, &old_state) && ctx)
        return ctx;
    }

  return glx_renderer->glXCreateContextAttribs (xlib_renderer->xdpy,
                                                fb_config,
                                                NULL /* share_context */,
                                                True, /* direct */
                                                attrib_list);
}

static CoglBool
create_context (CoglDisplay *display, CoglError **error)
{
  CoglGLXDisplay *glx_display = display->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (display->renderer);
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;
  CoglBool support_transparent_windows =
    display->onscreen_template->config.swap_chain->has_alpha;
  GLXFBConfig config;
  CoglError *fbconfig_error = NULL;
  XSetWindowAttributes attrs;
  XVisualInfo *xvisinfo;
  GLXDrawable dummy_drawable;
  CoglXlibTrapState old_state;

  _COGL_RETURN_VAL_IF_FAIL (glx_display->glx_context == NULL, TRUE);

  glx_display->found_fbconfig =
    find_fbconfig (display, &display->onscreen_template->config, &config,
                   &fbconfig_error);
  if (!glx_display->found_fbconfig)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to find suitable fbconfig for the GLX context: %s",
                   fbconfig_error->message);
      cogl_error_free (fbconfig_error);
      return FALSE;
    }

  glx_display->fbconfig = config;
  glx_display->fbconfig_has_rgba_visual = support_transparent_windows;

  COGL_NOTE (WINSYS, "Creating GLX Context (display: %p)",
             xlib_renderer->xdpy);

  _cogl_xlib_renderer_trap_errors (display->renderer, &old_state);

  if (display->renderer->driver == COGL_DRIVER_GL3)
    glx_display->glx_context = create_gl3_context (display, config);
  else
    glx_display->glx_context =
      glx_renderer->glXCreateNewContext (xlib_renderer->xdpy,
                                         config,
                                         GLX_RGBA_TYPE,
                                         NULL,
                                         True);

  if (_cogl_xlib_renderer_untrap_errors (display->renderer, &old_state) ||
      glx_display->glx_context == NULL)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to create suitable GL context");
      return FALSE;
    }

  glx_display->is_direct =
    glx_renderer->glXIsDirect (xlib_renderer->xdpy, glx_display->glx_context);
  glx_display->have_vblank_counter = glx_display->is_direct && glx_renderer->glXWaitVideoSync;
  glx_display->can_vblank_wait = glx_renderer->glXWaitForMsc || glx_display->have_vblank_counter;

  COGL_NOTE (WINSYS, "Setting %s context",
             glx_display->is_direct ? "direct" : "indirect");

  /* XXX: GLX doesn't let us make a context current without a window
   * so we create a dummy window that we can use while no CoglOnscreen
   * framebuffer is in use.
   */

  xvisinfo = glx_renderer->glXGetVisualFromFBConfig (xlib_renderer->xdpy,
                                                     config);
  if (xvisinfo == NULL)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to retrieve the X11 visual");
      return FALSE;
    }

  _cogl_xlib_renderer_trap_errors (display->renderer, &old_state);

  attrs.override_redirect = True;
  attrs.colormap = XCreateColormap (xlib_renderer->xdpy,
                                    DefaultRootWindow (xlib_renderer->xdpy),
                                    xvisinfo->visual,
                                    AllocNone);
  attrs.border_pixel = 0;

  glx_display->dummy_xwin =
    XCreateWindow (xlib_renderer->xdpy,
                   DefaultRootWindow (xlib_renderer->xdpy),
                   -100, -100, 1, 1,
                   0,
                   xvisinfo->depth,
                   CopyFromParent,
                   xvisinfo->visual,
                   CWOverrideRedirect | CWColormap | CWBorderPixel,
                   &attrs);

  /* Try and create a GLXWindow to use with extensions dependent on
   * GLX versions >= 1.3 that don't accept regular X Windows as GLX
   * drawables. */
  if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 3)
    {
      glx_display->dummy_glxwin =
        glx_renderer->glXCreateWindow (xlib_renderer->xdpy,
                                       config,
                                       glx_display->dummy_xwin,
                                       NULL);
    }

  if (glx_display->dummy_glxwin)
    dummy_drawable = glx_display->dummy_glxwin;
  else
    dummy_drawable = glx_display->dummy_xwin;

  COGL_NOTE (WINSYS, "Selecting dummy 0x%x for the GLX context",
             (unsigned int) dummy_drawable);

  glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
                                       dummy_drawable,
                                       dummy_drawable,
                                       glx_display->glx_context);

  xlib_renderer->xvisinfo = xvisinfo;

  if (_cogl_xlib_renderer_untrap_errors (display->renderer, &old_state))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to select the newly created GLX context");
      return FALSE;
    }

  return TRUE;
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  CoglGLXDisplay *glx_display = display->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (display->renderer);
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;

  _COGL_RETURN_IF_FAIL (glx_display != NULL);

  if (glx_display->glx_context)
    {
      glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
                                           None, None, NULL);
      glx_renderer->glXDestroyContext (xlib_renderer->xdpy,
                                       glx_display->glx_context);
      glx_display->glx_context = NULL;
    }

  if (glx_display->dummy_glxwin)
    {
      glx_renderer->glXDestroyWindow (xlib_renderer->xdpy,
                                      glx_display->dummy_glxwin);
      glx_display->dummy_glxwin = None;
    }

  if (glx_display->dummy_xwin)
    {
      XDestroyWindow (xlib_renderer->xdpy, glx_display->dummy_xwin);
      glx_display->dummy_xwin = None;
    }

  g_slice_free (CoglGLXDisplay, display->winsys);
  display->winsys = NULL;
}

static CoglBool
_cogl_winsys_display_setup (CoglDisplay *display,
                            CoglError **error)
{
  CoglGLXDisplay *glx_display;
  int i;

  _COGL_RETURN_VAL_IF_FAIL (display->winsys == NULL, FALSE);

  glx_display = g_slice_new0 (CoglGLXDisplay);
  display->winsys = glx_display;

  if (!create_context (display, error))
    goto error;

  for (i = 0; i < COGL_GLX_N_CACHED_CONFIGS; i++)
    glx_display->glx_cached_configs[i].depth = -1;

  return TRUE;

error:
  _cogl_winsys_display_destroy (display);
  return FALSE;
}

static CoglBool
_cogl_winsys_context_init (CoglContext *context, CoglError **error)
{
  context->winsys = g_new0 (CoglContextGLX, 1);

  cogl_xlib_renderer_add_filter (context->display->renderer,
                                 glx_event_filter_cb,
                                 context);
  return update_winsys_features (context, error);
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
  cogl_xlib_renderer_remove_filter (context->display->renderer,
                                    glx_event_filter_cb,
                                    context);
  g_free (context->winsys);
}

static CoglBool
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            CoglError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglGLXDisplay *glx_display = display->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (display->renderer);
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;
  Window xwin;
  CoglOnscreenXlib *xlib_onscreen;
  CoglOnscreenGLX *glx_onscreen;
  GLXFBConfig fbconfig;
  CoglError *fbconfig_error = NULL;

  _COGL_RETURN_VAL_IF_FAIL (glx_display->glx_context, FALSE);

  if (!find_fbconfig (display, &framebuffer->config,
                      &fbconfig,
                      &fbconfig_error))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to find suitable fbconfig for the GLX context: %s",
                   fbconfig_error->message);
      cogl_error_free (fbconfig_error);
      return FALSE;
    }

  /* Update the real number of samples_per_pixel now that we have
   * found an fbconfig... */
  if (framebuffer->config.samples_per_pixel)
    {
      int samples;
      int status = glx_renderer->glXGetFBConfigAttrib (xlib_renderer->xdpy,
                                                       fbconfig,
                                                       GLX_SAMPLES,
                                                       &samples);
      g_return_val_if_fail (status == Success, TRUE);
      framebuffer->samples_per_pixel = samples;
    }

  /* FIXME: We need to explicitly Select for ConfigureNotify events.
   * For foreign windows we need to be careful not to mess up any
   * existing event mask.
   * We need to document that for windows we create then toolkits
   * must be careful not to clear event mask bits that we select.
   */

  /* XXX: Note we ignore the user's original width/height when
   * given a foreign X window. */
  if (onscreen->foreign_xid)
    {
      Status status;
      CoglXlibTrapState state;
      XWindowAttributes attr;
      int xerror;

      xwin = onscreen->foreign_xid;

      _cogl_xlib_renderer_trap_errors (display->renderer, &state);

      status = XGetWindowAttributes (xlib_renderer->xdpy, xwin, &attr);
      XSync (xlib_renderer->xdpy, False);
      xerror = _cogl_xlib_renderer_untrap_errors (display->renderer, &state);
      if (status == 0 || xerror)
        {
          char message[1000];
          XGetErrorText (xlib_renderer->xdpy, xerror, message, sizeof(message));
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Unable to query geometry of foreign xid 0x%08lX: %s",
                       xwin, message);
          return FALSE;
        }

      _cogl_framebuffer_winsys_update_size (framebuffer,
                                            attr.width, attr.height);

      /* Make sure the app selects for the events we require... */
      onscreen->foreign_update_mask_callback (onscreen,
                                              COGL_ONSCREEN_X11_EVENT_MASK,
                                              onscreen->foreign_update_mask_data);
    }
  else
    {
      int width;
      int height;
      CoglXlibTrapState state;
      XVisualInfo *xvisinfo;
      XSetWindowAttributes xattr;
      unsigned long mask;
      int xerror;

      width = cogl_framebuffer_get_width (framebuffer);
      height = cogl_framebuffer_get_height (framebuffer);

      _cogl_xlib_renderer_trap_errors (display->renderer, &state);

      xvisinfo = glx_renderer->glXGetVisualFromFBConfig (xlib_renderer->xdpy,
                                                         fbconfig);
      if (xvisinfo == NULL)
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Unable to retrieve the X11 visual of context's "
                       "fbconfig");
          return FALSE;
        }

      /* window attributes */
      xattr.background_pixel = WhitePixel (xlib_renderer->xdpy,
                                           DefaultScreen (xlib_renderer->xdpy));
      xattr.border_pixel = 0;
      /* XXX: is this an X resource that we are leaking‽... */
      xattr.colormap = XCreateColormap (xlib_renderer->xdpy,
                                        DefaultRootWindow (xlib_renderer->xdpy),
                                        xvisinfo->visual,
                                        AllocNone);
      xattr.event_mask = COGL_ONSCREEN_X11_EVENT_MASK;

      mask = CWBorderPixel | CWColormap | CWEventMask;

      xwin = XCreateWindow (xlib_renderer->xdpy,
                            DefaultRootWindow (xlib_renderer->xdpy),
                            0, 0,
                            width, height,
                            0,
                            xvisinfo->depth,
                            InputOutput,
                            xvisinfo->visual,
                            mask, &xattr);

      XFree (xvisinfo);

      XSync (xlib_renderer->xdpy, False);
      xerror = _cogl_xlib_renderer_untrap_errors (display->renderer, &state);
      if (xerror)
        {
          char message[1000];
          XGetErrorText (xlib_renderer->xdpy, xerror,
                         message, sizeof (message));
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "X error while creating Window for CoglOnscreen: %s",
                       message);
          return FALSE;
        }
    }

  onscreen->winsys = g_slice_new0 (CoglOnscreenGLX);
  xlib_onscreen = onscreen->winsys;
  glx_onscreen = onscreen->winsys;

  xlib_onscreen->xwin = xwin;
  xlib_onscreen->is_foreign_xwin = onscreen->foreign_xid ? TRUE : FALSE;

  /* Try and create a GLXWindow to use with extensions dependent on
   * GLX versions >= 1.3 that don't accept regular X Windows as GLX
   * drawables. */
  if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 3)
    {
      glx_onscreen->glxwin =
        glx_renderer->glXCreateWindow (xlib_renderer->xdpy,
                                       fbconfig,
                                       xlib_onscreen->xwin,
                                       NULL);
    }

#ifdef GLX_INTEL_swap_event
  if (_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT) &&
      !_cogl_has_private_feature (context, COGL_PRIVATE_FEATURE_THREADED_SWAP_WAIT))
    {
      GLXDrawable drawable =
        glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

      /* similarly to above, we unconditionally select this event
       * because we rely on it to advance the master clock, and
       * drive redraw/relayout, animations and event handling.
       */
      glx_renderer->glXSelectEvent (xlib_renderer->xdpy,
                                    drawable,
                                    GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
    }
#endif /* GLX_INTEL_swap_event */

  return TRUE;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglContextGLX *glx_context = context->winsys;
  CoglGLXDisplay *glx_display = context->display->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglXlibTrapState old_state;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  GLXDrawable drawable;

  /* If we never successfully allocated then there's nothing to do */
  if (glx_onscreen == NULL)
    return;

  if (xlib_onscreen->output != NULL)
    {
      cogl_object_unref (xlib_onscreen->output);
      xlib_onscreen->output = NULL;
    }

  if (glx_onscreen->swap_wait_thread)
    {
      g_mutex_lock (&glx_onscreen->swap_wait_mutex);
      glx_onscreen->closing_down = TRUE;
      g_cond_signal (&glx_onscreen->swap_wait_cond);
      g_mutex_unlock (&glx_onscreen->swap_wait_mutex);
      g_thread_join (glx_onscreen->swap_wait_thread);
      glx_onscreen->swap_wait_thread = NULL;

      g_cond_clear (&glx_onscreen->swap_wait_cond);
      g_mutex_clear (&glx_onscreen->swap_wait_mutex);

      g_queue_free (glx_onscreen->swap_wait_queue);
      glx_onscreen->swap_wait_queue = NULL;

      _cogl_poll_renderer_remove_fd (context->display->renderer,
                                     glx_onscreen->swap_wait_pipe[0]);
      
      close (glx_onscreen->swap_wait_pipe[0]);
      close (glx_onscreen->swap_wait_pipe[1]);

      glx_renderer->glXDestroyContext (xlib_renderer->xdpy,
                                       glx_onscreen->swap_wait_context);
    }

  _cogl_xlib_renderer_trap_errors (context->display->renderer, &old_state);

  drawable =
    glx_onscreen->glxwin == None ? xlib_onscreen->xwin : glx_onscreen->glxwin;

  /* Cogl always needs a valid context bound to something so if we are
   * destroying the onscreen that is currently bound we'll switch back
   * to the dummy drawable. Although the documentation for
   * glXDestroyWindow states that a currently bound window won't
   * actually be destroyed until it is unbound, it looks like this
   * doesn't work if the X window itself is destroyed */
  if (drawable == glx_context->current_drawable)
    {
      GLXDrawable dummy_drawable = (glx_display->dummy_glxwin == None ?
                                    glx_display->dummy_xwin :
                                    glx_display->dummy_glxwin);

      glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
                                           dummy_drawable,
                                           dummy_drawable,
                                           glx_display->glx_context);
      glx_context->current_drawable = dummy_drawable;
    }

  if (glx_onscreen->glxwin != None)
    {
      glx_renderer->glXDestroyWindow (xlib_renderer->xdpy,
                                      glx_onscreen->glxwin);
      glx_onscreen->glxwin = None;
    }

  if (!xlib_onscreen->is_foreign_xwin && xlib_onscreen->xwin != None)
    {
      XDestroyWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
      xlib_onscreen->xwin = None;
    }
  else
    xlib_onscreen->xwin = None;

  XSync (xlib_renderer->xdpy, False);

  _cogl_xlib_renderer_untrap_errors (context->display->renderer, &old_state);

  g_slice_free (CoglOnscreenGLX, onscreen->winsys);
  onscreen->winsys = NULL;
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextGLX *glx_context = context->winsys;
  CoglGLXDisplay *glx_display = context->display->winsys;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglXlibTrapState old_state;
  GLXDrawable drawable;

  drawable =
    glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

  if (glx_context->current_drawable == drawable)
    return;

  _cogl_xlib_renderer_trap_errors (context->display->renderer, &old_state);

  COGL_NOTE (WINSYS,
             "MakeContextCurrent dpy: %p, window: 0x%x (%s), context: %p",
             xlib_renderer->xdpy,
             (unsigned int) drawable,
             xlib_onscreen->is_foreign_xwin ? "foreign" : "native",
             glx_display->glx_context);

  glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
                                       drawable,
                                       drawable,
                                       glx_display->glx_context);

  /* In case we are using GLX_SGI_swap_control for vblank syncing
   * we need call glXSwapIntervalSGI here to make sure that it
   * affects the current drawable.
   *
   * Note: we explicitly set to 0 when we aren't using the swap
   * interval to synchronize since some drivers have a default
   * swap interval of 1. Sadly some drivers even ignore requests
   * to disable the swap interval.
   *
   * NB: glXSwapIntervalSGI applies to the context not the
   * drawable which is why we can't just do this once when the
   * framebuffer is allocated.
   *
   * FIXME: We should check for GLX_EXT_swap_control which allows
   * per framebuffer swap intervals. GLX_MESA_swap_control also
   * allows per-framebuffer swap intervals but the semantics tend
   * to be more muddled since Mesa drivers tend to expose both the
   * MESA and SGI extensions which should technically be mutually
   * exclusive.
   */
  if (glx_renderer->glXSwapInterval)
    {
      CoglFramebuffer *fb = COGL_FRAMEBUFFER (onscreen);
      if (fb->config.swap_throttled)
        glx_renderer->glXSwapInterval (1);
      else
        glx_renderer->glXSwapInterval (0);
    }

  XSync (xlib_renderer->xdpy, False);

  /* FIXME: We should be reporting a CoglError here
   */
  if (_cogl_xlib_renderer_untrap_errors (context->display->renderer,
                                         &old_state))
    {
      g_warning ("X Error received while making drawable 0x%08lX current",
                 drawable);
      return;
    }

  glx_context->current_drawable = drawable;
}

static void
_cogl_winsys_wait_for_gpu (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *ctx = framebuffer->context;

  ctx->glFinish ();
}

static void
_cogl_winsys_wait_for_vblank (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *ctx = framebuffer->context;
  CoglGLXRenderer *glx_renderer;
  CoglXlibRenderer *xlib_renderer;
  CoglGLXDisplay *glx_display;

  glx_renderer = ctx->display->renderer->winsys;
  xlib_renderer = _cogl_xlib_renderer_get_data (ctx->display->renderer);
  glx_display = ctx->display->winsys;

  if (glx_display->can_vblank_wait)
    {
      CoglFrameInfo *info = g_queue_peek_tail (&onscreen->pending_frame_infos);

      if (glx_renderer->glXWaitForMsc)
        {
          CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
          Drawable drawable = glx_onscreen->glxwin;
          int64_t ust;
          int64_t msc;
          int64_t sbc;

          glx_renderer->glXWaitForMsc (xlib_renderer->xdpy, drawable,
                                       0, 1, 0,
                                       &ust, &msc, &sbc);
          info->presentation_time = ust_to_nanoseconds (ctx->display->renderer,
                                                        drawable,
                                                        ust);
        }
      else
        {
          uint32_t current_count;

          glx_renderer->glXGetVideoSync (&current_count);
          glx_renderer->glXWaitVideoSync (2,
                                          (current_count + 1) % 2,
                                          &current_count);

          info->presentation_time = get_monotonic_time_ns ();
        }
    }
}

static uint32_t
_cogl_winsys_get_vsync_counter (CoglContext *ctx)
{
  uint32_t video_sync_count;
  CoglGLXRenderer *glx_renderer;

  glx_renderer = ctx->display->renderer->winsys;

  glx_renderer->glXGetVideoSync (&video_sync_count);

  return video_sync_count;
}

#ifndef GLX_BACK_BUFFER_AGE_EXT
#define GLX_BACK_BUFFER_AGE_EXT 0x20F4
#endif

static int
_cogl_winsys_onscreen_get_buffer_age (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglXlibRenderer *xlib_renderer = _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  GLXDrawable drawable = glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;
  unsigned int age;

  if (!_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_BUFFER_AGE))
    return 0;

  glx_renderer->glXQueryDrawable (xlib_renderer->xdpy, drawable, GLX_BACK_BUFFER_AGE_EXT, &age);

  return age;
}

static void
set_frame_info_output (CoglOnscreen *onscreen,
                       CoglOutput *output)
{
  CoglFrameInfo *info = g_queue_peek_tail (&onscreen->pending_frame_infos);

  info->output = output;

  if (output)
    {
      float refresh_rate = cogl_output_get_refresh_rate (output);
      if (refresh_rate != 0.0)
        info->refresh_rate = refresh_rate;
    }
}

static gpointer
threaded_swap_wait (gpointer data)
{
  CoglOnscreen *onscreen = data;

  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;

  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglXlibRenderer *xlib_renderer = _cogl_xlib_renderer_get_data (display->renderer);
  CoglGLXDisplay *glx_display = display->winsys;
  CoglGLXRenderer *glx_renderer = display->renderer->winsys;
  GLXDrawable dummy_drawable;

  if (glx_display->dummy_glxwin)
    dummy_drawable = glx_display->dummy_glxwin;
  else
    dummy_drawable = glx_display->dummy_xwin;

  glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
                                       dummy_drawable,
                                       dummy_drawable,
                                       glx_onscreen->swap_wait_context);

  g_mutex_lock (&glx_onscreen->swap_wait_mutex);

  while (TRUE)
    {
      gpointer queue_element;
      uint32_t vblank_counter;

      while (!glx_onscreen->closing_down && glx_onscreen->swap_wait_queue->length == 0)
         g_cond_wait (&glx_onscreen->swap_wait_cond, &glx_onscreen->swap_wait_mutex);

      if (glx_onscreen->closing_down)
         break;

      queue_element = g_queue_pop_tail (glx_onscreen->swap_wait_queue);
      vblank_counter = GPOINTER_TO_UINT(queue_element);

      g_mutex_unlock (&glx_onscreen->swap_wait_mutex);
      glx_renderer->glXWaitVideoSync (2,
                                      (vblank_counter + 1) % 2,
                                      &vblank_counter);
      g_mutex_lock (&glx_onscreen->swap_wait_mutex);

      if (!glx_onscreen->closing_down)
         {
           int bytes_written = 0;

           union {
             char bytes[8];
             int64_t presentation_time;
           } u;

           u.presentation_time = get_monotonic_time_ns ();

           while (bytes_written < 8)
             {
               int res = write (glx_onscreen->swap_wait_pipe[1], u.bytes + bytes_written, 8 - bytes_written);
               if (res == -1)
                 {
                   if (errno != EINTR)
                     g_error ("Error writing to swap notification pipe: %s\n",
                              g_strerror (errno));
                 }
               else
                 {
                   bytes_written += res;
                 }
             }
         }
    }

  g_mutex_unlock (&glx_onscreen->swap_wait_mutex);

  glx_renderer->glXMakeContextCurrent (xlib_renderer->xdpy,
                                       None,
                                       None,
                                       NULL);

  return NULL;
}

static int64_t
threaded_swap_wait_pipe_prepare (void *user_data)
{
  return -1;
}

static void
threaded_swap_wait_pipe_dispatch (void *user_data, int revents)
{
  CoglOnscreen *onscreen = user_data;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;

  CoglFrameInfo *info;

  if ((revents & COGL_POLL_FD_EVENT_IN))
    {
      int bytes_read = 0;

      union {
         char bytes[8];
         int64_t presentation_time;
      } u;

      while (bytes_read < 8)
         {
           int res = read (glx_onscreen->swap_wait_pipe[0], u.bytes + bytes_read, 8 - bytes_read);
           if (res == -1)
             {
               if (errno != EINTR)
                 g_error ("Error reading from swap notification pipe: %s\n",
                          g_strerror (errno));
             }
           else
             {
               bytes_read += res;
             }
         }

      set_sync_pending (onscreen);
      set_complete_pending (onscreen);

      info = g_queue_peek_head (&onscreen->pending_frame_infos);
      info->presentation_time = u.presentation_time;
    }
}

static void
start_threaded_swap_wait (CoglOnscreen *onscreen,
                           uint32_t      vblank_counter)
{
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;

  if (glx_onscreen->swap_wait_thread == NULL)
    {
      CoglDisplay *display = context->display;
      CoglGLXRenderer *glx_renderer = display->renderer->winsys;
      CoglGLXDisplay *glx_display = display->winsys;
      CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
      CoglXlibRenderer *xlib_renderer =
        _cogl_xlib_renderer_get_data (display->renderer);

      GLXDrawable drawable =
        glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;
      int i;

      ensure_ust_type (display->renderer, drawable);
      
      if ((pipe (glx_onscreen->swap_wait_pipe) == -1))
        g_error ("Couldn't create pipe for swap notification: %s\n",
                 g_strerror (errno));

      for (i = 0; i < 2; i++)
	{
	  if (fcntl(glx_onscreen->swap_wait_pipe[i], F_SETFD,
		    fcntl(glx_onscreen->swap_wait_pipe[i], F_GETFD, 0) | FD_CLOEXEC) == -1)
	    g_error ("Couldn't set swap notification pipe CLOEXEC: %s\n",
		     g_strerror (errno));
	}

      _cogl_poll_renderer_add_fd (display->renderer,
                                  glx_onscreen->swap_wait_pipe[0],
                                  COGL_POLL_FD_EVENT_IN,
                                  threaded_swap_wait_pipe_prepare,
                                  threaded_swap_wait_pipe_dispatch,
                                  onscreen);

      glx_onscreen->swap_wait_queue = g_queue_new ();
      g_mutex_init (&glx_onscreen->swap_wait_mutex);
      g_cond_init (&glx_onscreen->swap_wait_cond);
      glx_onscreen->swap_wait_context =
         glx_renderer->glXCreateNewContext (xlib_renderer->xdpy,
                                            glx_display->fbconfig,
                                            GLX_RGBA_TYPE,
                                            glx_display->glx_context,
                                            True);
      glx_onscreen->swap_wait_thread = g_thread_new ("cogl_glx_swap_wait",
                                                     threaded_swap_wait,
                                                     onscreen);
    }

  g_mutex_lock (&glx_onscreen->swap_wait_mutex);
  g_queue_push_head (glx_onscreen->swap_wait_queue, GUINT_TO_POINTER(vblank_counter));
  g_cond_signal (&glx_onscreen->swap_wait_cond);
  g_mutex_unlock (&glx_onscreen->swap_wait_mutex);
}

static void
_cogl_winsys_onscreen_swap_region (CoglOnscreen *onscreen,
                                   const int *user_rectangles,
                                   int n_rectangles)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglGLXDisplay *glx_display = context->display->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  GLXDrawable drawable =
    glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;
  uint32_t end_frame_vsync_counter = 0;
  CoglBool have_counter;
  CoglBool can_wait;
  int x_min = 0, x_max = 0, y_min = 0, y_max = 0;

  /*
   * We assume that glXCopySubBuffer is synchronized which means it won't prevent multiple
   * blits per retrace if they can all be performed in the blanking period. If that's the
   * case then we still want to use the vblank sync menchanism but
   * we only need it to throttle redraws.
   */
  CoglBool blit_sub_buffer_is_synchronized =
     _cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_REGION_SYNCHRONIZED);

  int framebuffer_width =  cogl_framebuffer_get_width (framebuffer);
  int framebuffer_height =  cogl_framebuffer_get_height (framebuffer);
  int *rectangles = g_alloca (sizeof (int) * n_rectangles * 4);
  int i;

  /* glXCopySubBuffer expects rectangles relative to the bottom left corner but
   * we are given rectangles relative to the top left so we need to flip
   * them... */
  memcpy (rectangles, user_rectangles, sizeof (int) * n_rectangles * 4);
  for (i = 0; i < n_rectangles; i++)
    {
      int *rect = &rectangles[4 * i];

      if (i == 0)
        {
          x_min = rect[0];
          x_max = rect[0] + rect[2];
          y_min = rect[1];
          y_max = rect[1] + rect[3];
        }
      else
        {
          x_min = MIN (x_min, rect[0]);
          x_max = MAX (x_max, rect[0] + rect[2]);
          y_min = MIN (y_min, rect[1]);
          y_max = MAX (y_max, rect[1] + rect[3]);
        }

      rect[1] = framebuffer_height - rect[1] - rect[3];

    }

  _cogl_framebuffer_flush_state (framebuffer,
                                 framebuffer,
                                 COGL_FRAMEBUFFER_STATE_BIND);

  if (framebuffer->config.swap_throttled)
    {
      have_counter = glx_display->have_vblank_counter;
      can_wait = glx_display->can_vblank_wait;
    }
  else
    {
      have_counter = FALSE;
      can_wait = FALSE;
    }

  /* We need to ensure that all the rendering is done, otherwise
   * redraw operations that are slower than the framerate can
   * queue up in the pipeline during a heavy animation, causing a
   * larger and larger backlog of rendering visible as lag to the
   * user.
   *
   * For an exaggerated example consider rendering at 60fps (so 16ms
   * per frame) and you have a really slow frame that takes 160ms to
   * render, even though painting the scene and issuing the commands
   * to the GPU takes no time at all. If all we did was use the
   * video_sync extension to throttle the painting done by the CPU
   * then every 16ms we would have another frame queued up even though
   * the GPU has only rendered one tenth of the current frame. By the
   * time the GPU would get to the 2nd frame there would be 9 frames
   * waiting to be rendered.
   *
   * The problem is that we don't currently have a good way to throttle
   * the GPU, only the CPU so we have to resort to synchronizing the
   * GPU with the CPU to throttle it.
   *
   * Note: since calling glFinish() and synchronizing the CPU with
   * the GPU is far from ideal, we hope that this is only a short
   * term solution.
   * - One idea is to using sync objects to track render
   *   completion so we can throttle the backlog (ideally with an
   *   additional extension that lets us get notifications in our
   *   mainloop instead of having to busy wait for the
   *   completion.)
   * - Another option is to support clipped redraws by reusing the
   *   contents of old back buffers such that we can flip instead
   *   of using a blit and then we can use GLX_INTEL_swap_events
   *   to throttle. For this though we would still probably want an
   *   additional extension so we can report the limited region of
   *   the window damage to X/compositors.
   */
  _cogl_winsys_wait_for_gpu (onscreen);

  if (blit_sub_buffer_is_synchronized && have_counter && can_wait)
    {
      end_frame_vsync_counter = _cogl_winsys_get_vsync_counter (context);

      /* If we have the GLX_SGI_video_sync extension then we can
       * be a bit smarter about how we throttle blits by avoiding
       * any waits if we can see that the video sync count has
       * already progressed. */
      if (glx_onscreen->last_swap_vsync_counter == end_frame_vsync_counter)
        _cogl_winsys_wait_for_vblank (onscreen);
    }
  else if (can_wait)
    _cogl_winsys_wait_for_vblank (onscreen);

  if (glx_renderer->glXCopySubBuffer)
    {
      Display *xdpy = xlib_renderer->xdpy;
      int i;
      for (i = 0; i < n_rectangles; i++)
        {
          int *rect = &rectangles[4 * i];
          glx_renderer->glXCopySubBuffer (xdpy, drawable,
                                          rect[0], rect[1], rect[2], rect[3]);
        }
    }
  else if (context->glBlitFramebuffer)
    {
      int i;
      /* XXX: checkout how this state interacts with the code to use
       * glBlitFramebuffer in Neil's texture atlasing branch */

      /* glBlitFramebuffer is affected by the scissor so we need to
       * ensure we have flushed an empty clip stack to get rid of it.
       * We also mark that the clip state is dirty so that it will be
       * flushed to the correct state the next time something is
       * drawn */
      _cogl_clip_stack_flush (NULL, framebuffer);
      context->current_draw_buffer_changes |= COGL_FRAMEBUFFER_STATE_CLIP;

      context->glDrawBuffer (GL_FRONT);
      for (i = 0; i < n_rectangles; i++)
        {
          int *rect = &rectangles[4 * i];
          int x2 = rect[0] + rect[2];
          int y2 = rect[1] + rect[3];
          context->glBlitFramebuffer (rect[0], rect[1], x2, y2,
                                      rect[0], rect[1], x2, y2,
                                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
      context->glDrawBuffer (context->current_gl_draw_buffer);
    }

  /* NB: unlike glXSwapBuffers, glXCopySubBuffer and
   * glBlitFramebuffer don't issue an implicit glFlush() so we
   * have to flush ourselves if we want the request to complete in
   * a finite amount of time since otherwise the driver can batch
   * the command indefinitely. */
  context->glFlush ();

  /* NB: It's important we save the counter we read before acting on
   * the swap request since if we are mixing and matching different
   * swap methods between frames we don't want to read the timer e.g.
   * after calling glFinish() some times and not for others.
   *
   * In other words; this way we consistently save the time at the end
   * of the applications frame such that the counter isn't muddled by
   * the varying costs of different swap methods.
   */
  if (have_counter)
    glx_onscreen->last_swap_vsync_counter = end_frame_vsync_counter;

  if (!xlib_onscreen->is_foreign_xwin)
    {
      CoglOutput *output;

      x_min = CLAMP (x_min, 0, framebuffer_width);
      x_max = CLAMP (x_max, 0, framebuffer_width);
      y_min = CLAMP (y_min, 0, framebuffer_width);
      y_max = CLAMP (y_max, 0, framebuffer_height);

      output =
        _cogl_xlib_renderer_output_for_rectangle (context->display->renderer,
                                                  xlib_onscreen->x + x_min,
                                                  xlib_onscreen->y + y_min,
                                                  x_max - x_min,
                                                  y_max - y_min);

      set_frame_info_output (onscreen, output);
    }

  /* XXX: we don't get SwapComplete events based on how we implement
   * the _swap_region() API but if cogl-onscreen.c knows we are
   * handling _SYNC and _COMPLETE events in the winsys then we need to
   * send fake events in this case.
   */
  if (_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT))
    {
      set_sync_pending (onscreen);
      set_complete_pending (onscreen);
    }
}

static void
_cogl_winsys_onscreen_swap_buffers_with_damage (CoglOnscreen *onscreen,
                                                const int *rectangles,
                                                int n_rectangles)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglGLXRenderer *glx_renderer = context->display->renderer->winsys;
  CoglGLXDisplay *glx_display = context->display->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglBool have_counter;
  GLXDrawable drawable;

  /* XXX: theoretically this shouldn't be necessary but at least with
   * the Intel drivers we have see that if we don't call
   * glXMakeContextCurrent for the drawable we are swapping then
   * we get a BadDrawable error from the X server. */
  _cogl_framebuffer_flush_state (framebuffer,
                                 framebuffer,
                                 COGL_FRAMEBUFFER_STATE_BIND);

  drawable = glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

  if (framebuffer->config.swap_throttled)
    {
      have_counter = glx_display->have_vblank_counter;

      if (glx_renderer->glXSwapInterval)
        {
          if (_cogl_has_private_feature (context, COGL_PRIVATE_FEATURE_THREADED_SWAP_WAIT))
            {
	      /* If we didn't wait for the GPU here, then it's easy to get the case
	       * where there is a VBlank between the point where we get the vsync counter
	       * and the point where the GPU is ready to actually perform the glXSwapBuffers(),
	       * and the swap wait terminates at the first VBlank rather than the one
	       * where the swap buffers happens. Calling glFinish() here makes this a
	       * rare race since the GPU is already ready to swap when we call glXSwapBuffers().
	       * The glFinish() also prevents any serious damage if the rare race happens,
	       * since it will wait for the preceding glXSwapBuffers() and prevent us from
	       * getting premanently ahead. (For NVIDIA drivers, glFinish() after glXSwapBuffers()
	       * waits for the buffer swap to happen.)
	       */
              _cogl_winsys_wait_for_gpu (onscreen);
              start_threaded_swap_wait (onscreen, _cogl_winsys_get_vsync_counter (context));
            }
        }
      else
        {
          CoglBool can_wait = have_counter || glx_display->can_vblank_wait;

          uint32_t end_frame_vsync_counter = 0;

          /* If the swap_region API is also being used then we need to track
           * the vsync counter for each swap request so we can manually
           * throttle swap_region requests. */
          if (have_counter)
            end_frame_vsync_counter = _cogl_winsys_get_vsync_counter (context);

          /* If we are going to wait for VBLANK manually, we not only
           * need to flush out pending drawing to the GPU before we
           * sleep, we need to wait for it to finish. Otherwise, we
           * may end up with the situation:
           *
           *        - We finish drawing      - GPU drawing continues
           *        - We go to sleep         - GPU drawing continues
           * VBLANK - We call glXSwapBuffers - GPU drawing continues
           *                                 - GPU drawing continues
           *                                 - Swap buffers happens
           *
           * Producing a tear. Calling glFinish() first will cause us
           * to properly wait for the next VBLANK before we swap. This
           * obviously does not happen when we use _GLX_SWAP and let
           * the driver do the right thing
           */
          _cogl_winsys_wait_for_gpu (onscreen);

          if (have_counter && can_wait)
            {
              if (glx_onscreen->last_swap_vsync_counter ==
                  end_frame_vsync_counter)
                _cogl_winsys_wait_for_vblank (onscreen);
            }
          else if (can_wait)
            _cogl_winsys_wait_for_vblank (onscreen);
        }
    }
  else
    have_counter = FALSE;

  glx_renderer->glXSwapBuffers (xlib_renderer->xdpy, drawable);

  if (have_counter)
    glx_onscreen->last_swap_vsync_counter =
      _cogl_winsys_get_vsync_counter (context);

  set_frame_info_output (onscreen, xlib_onscreen->output);
}

static uint32_t
_cogl_winsys_onscreen_x11_get_window_xid (CoglOnscreen *onscreen)
{
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  return xlib_onscreen->xwin;
}

static void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextGLX *glx_context = context->winsys;
  CoglOnscreenGLX *glx_onscreen = onscreen->winsys;
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;
  GLXDrawable drawable =
    glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

  if (glx_context->current_drawable != drawable)
    return;

  glx_context->current_drawable = 0;
  _cogl_winsys_onscreen_bind (onscreen);
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      CoglBool visibility)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;

  if (visibility)
    XMapWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
  else
    XUnmapWindow (xlib_renderer->xdpy, xlib_onscreen->xwin);
}

static void
_cogl_winsys_onscreen_set_resizable (CoglOnscreen *onscreen,
                                     CoglBool resizable)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (context->display->renderer);
  CoglOnscreenXlib *xlib_onscreen = onscreen->winsys;

  XSizeHints *size_hints = XAllocSizeHints ();

  if (resizable)
    {
      /* TODO: Add cogl_onscreen_request_minimum_size () */
      size_hints->min_width = 1;
      size_hints->min_height = 1;

      size_hints->max_width = INT_MAX;
      size_hints->max_height = INT_MAX;
    }
  else
    {
      int width = cogl_framebuffer_get_width (framebuffer);
      int height = cogl_framebuffer_get_height (framebuffer);

      size_hints->min_width = width;
      size_hints->min_height = height;

      size_hints->max_width = width;
      size_hints->max_height = height;
    }

  XSetWMNormalHints (xlib_renderer->xdpy, xlib_onscreen->xwin, size_hints);

  XFree (size_hints);
}

static CoglBool
get_fbconfig_for_depth (CoglContext *context,
                        unsigned int depth,
                        CoglBool stereo,
                        GLXFBConfig *fbconfig_ret,
                        CoglBool *can_mipmap_ret)
{
  CoglXlibRenderer *xlib_renderer;
  CoglGLXRenderer *glx_renderer;
  CoglGLXDisplay *glx_display;
  Display *dpy;
  GLXFBConfig *fbconfigs;
  int n_elements, i;
  int db, stencil, alpha, mipmap, rgba, value;
  int spare_cache_slot = 0;
  CoglBool found = FALSE;

  xlib_renderer = _cogl_xlib_renderer_get_data (context->display->renderer);
  glx_renderer = context->display->renderer->winsys;
  glx_display = context->display->winsys;

  /* Check if we've already got a cached config for this depth and stereo */
  for (i = 0; i < COGL_GLX_N_CACHED_CONFIGS; i++)
    if (glx_display->glx_cached_configs[i].depth == -1)
      spare_cache_slot = i;
    else if (glx_display->glx_cached_configs[i].depth == depth &&
             glx_display->glx_cached_configs[i].stereo == stereo)
      {
        *fbconfig_ret = glx_display->glx_cached_configs[i].fb_config;
        *can_mipmap_ret = glx_display->glx_cached_configs[i].can_mipmap;
        return glx_display->glx_cached_configs[i].found;
      }

  dpy = xlib_renderer->xdpy;

  fbconfigs = glx_renderer->glXGetFBConfigs (dpy, DefaultScreen (dpy),
                                             &n_elements);

  db = G_MAXSHORT;
  stencil = G_MAXSHORT;
  mipmap = 0;
  rgba = 0;

  for (i = 0; i < n_elements; i++)
    {
      XVisualInfo *vi;
      int visual_depth;

      vi = glx_renderer->glXGetVisualFromFBConfig (dpy, fbconfigs[i]);
      if (vi == NULL)
        continue;

      visual_depth = vi->depth;

      XFree (vi);

      if (visual_depth != depth)
        continue;

      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_ALPHA_SIZE,
                                          &alpha);
      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_BUFFER_SIZE,
                                          &value);
      if (value != depth && (value - alpha) != depth)
        continue;

      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_STEREO,
                                          &value);
      if (!!value != !!stereo)
        continue;

      if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 4)
        {
          glx_renderer->glXGetFBConfigAttrib (dpy,
                                              fbconfigs[i],
                                              GLX_SAMPLES,
                                              &value);
          if (value > 1)
            continue;
        }

      value = 0;
      if (depth == 32)
        {
          glx_renderer->glXGetFBConfigAttrib (dpy,
                                              fbconfigs[i],
                                              GLX_BIND_TO_TEXTURE_RGBA_EXT,
                                              &value);
          if (value)
            rgba = 1;
        }

      if (!value)
        {
          if (rgba)
            continue;

          glx_renderer->glXGetFBConfigAttrib (dpy,
                                              fbconfigs[i],
                                              GLX_BIND_TO_TEXTURE_RGB_EXT,
                                              &value);
          if (!value)
            continue;
        }

      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_DOUBLEBUFFER,
                                          &value);
      if (value > db)
        continue;

      db = value;

      glx_renderer->glXGetFBConfigAttrib (dpy,
                                          fbconfigs[i],
                                          GLX_STENCIL_SIZE,
                                          &value);
      if (value > stencil)
        continue;

      stencil = value;

      /* glGenerateMipmap is defined in the offscreen extension */
      if (cogl_has_feature (context, COGL_FEATURE_ID_OFFSCREEN))
        {
          glx_renderer->glXGetFBConfigAttrib (dpy,
                                              fbconfigs[i],
                                              GLX_BIND_TO_MIPMAP_TEXTURE_EXT,
                                              &value);

          if (value < mipmap)
            continue;

          mipmap =  value;
        }

      *fbconfig_ret = fbconfigs[i];
      *can_mipmap_ret = mipmap;
      found = TRUE;
    }

  if (n_elements)
    XFree (fbconfigs);

  glx_display->glx_cached_configs[spare_cache_slot].depth = depth;
  glx_display->glx_cached_configs[spare_cache_slot].found = found;
  glx_display->glx_cached_configs[spare_cache_slot].fb_config = *fbconfig_ret;
  glx_display->glx_cached_configs[spare_cache_slot].can_mipmap = mipmap;

  return found;
}

static CoglBool
should_use_rectangle (CoglContext *context)
{

  if (context->rectangle_state == COGL_WINSYS_RECTANGLE_STATE_UNKNOWN)
    {
      if (cogl_has_feature (context, COGL_FEATURE_ID_TEXTURE_RECTANGLE))
        {
          const char *rect_env;

          /* Use the rectangle only if it is available and either:

             the COGL_PIXMAP_TEXTURE_RECTANGLE environment variable is
             set to 'force'

             *or*

             the env var is set to 'allow' or not set and NPOTs textures
             are not available */

          context->rectangle_state =
            cogl_has_feature (context, COGL_FEATURE_ID_TEXTURE_NPOT) ?
            COGL_WINSYS_RECTANGLE_STATE_DISABLE :
            COGL_WINSYS_RECTANGLE_STATE_ENABLE;

          if ((rect_env = g_getenv ("COGL_PIXMAP_TEXTURE_RECTANGLE")) ||
              /* For compatibility, we'll also look at the old Clutter
                 environment variable */
              (rect_env = g_getenv ("CLUTTER_PIXMAP_TEXTURE_RECTANGLE")))
            {
              if (g_ascii_strcasecmp (rect_env, "force") == 0)
                context->rectangle_state =
                  COGL_WINSYS_RECTANGLE_STATE_ENABLE;
              else if (g_ascii_strcasecmp (rect_env, "disable") == 0)
                context->rectangle_state =
                  COGL_WINSYS_RECTANGLE_STATE_DISABLE;
              else if (g_ascii_strcasecmp (rect_env, "allow"))
                g_warning ("Unknown value for COGL_PIXMAP_TEXTURE_RECTANGLE, "
                           "should be 'force' or 'disable'");
            }
        }
      else
        context->rectangle_state = COGL_WINSYS_RECTANGLE_STATE_DISABLE;
    }

  return context->rectangle_state == COGL_WINSYS_RECTANGLE_STATE_ENABLE;
}

static CoglBool
try_create_glx_pixmap (CoglContext *context,
                       CoglTexturePixmapX11 *tex_pixmap,
                       CoglBool mipmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap = tex_pixmap->winsys;
  CoglRenderer *renderer;
  CoglXlibRenderer *xlib_renderer;
  CoglGLXRenderer *glx_renderer;
  Display *dpy;
  /* We have to initialize this *opaque* variable because gcc tries to
   * be too smart for its own good and warns that the variable may be
   * used uninitialized otherwise. */
  GLXFBConfig fb_config = (GLXFBConfig)0;
  int attribs[7];
  int i = 0;
  GLenum target;
  CoglXlibTrapState trap_state;

  unsigned int depth = tex_pixmap->depth;
  Visual* visual = tex_pixmap->visual;

  renderer = context->display->renderer;
  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);
  glx_renderer = renderer->winsys;
  dpy = xlib_renderer->xdpy;

  if (!get_fbconfig_for_depth (context, depth,
                               tex_pixmap->stereo_mode != COGL_TEXTURE_PIXMAP_MONO,
                               &fb_config,
                               &glx_tex_pixmap->can_mipmap))
    {
      COGL_NOTE (TEXTURE_PIXMAP, "No suitable FBConfig found for depth %i",
                 depth);
      return FALSE;
    }

  if (should_use_rectangle (context))
    {
      target = GLX_TEXTURE_RECTANGLE_EXT;
      glx_tex_pixmap->can_mipmap = FALSE;
    }
  else
    target = GLX_TEXTURE_2D_EXT;

  if (!glx_tex_pixmap->can_mipmap)
    mipmap = FALSE;

  attribs[i++] = GLX_TEXTURE_FORMAT_EXT;

  /* Check whether an alpha channel is used by comparing the total
   * number of 1-bits in color masks against the color depth requested
   * by the client.
   */
  if (_cogl_util_popcountl (visual->red_mask |
                            visual->green_mask |
                            visual->blue_mask) == depth)
    attribs[i++] = GLX_TEXTURE_FORMAT_RGB_EXT;
  else
    attribs[i++] = GLX_TEXTURE_FORMAT_RGBA_EXT;

  attribs[i++] = GLX_MIPMAP_TEXTURE_EXT;
  attribs[i++] = mipmap;

  attribs[i++] = GLX_TEXTURE_TARGET_EXT;
  attribs[i++] = target;

  attribs[i++] = None;

  /* We need to trap errors from glXCreatePixmap because it can
   * sometimes fail during normal usage. For example on NVidia it gets
   * upset if you try to create two GLXPixmaps for the same drawable.
   */

  _cogl_xlib_renderer_trap_errors (renderer, &trap_state);

  glx_tex_pixmap->glx_pixmap =
    glx_renderer->glXCreatePixmap (dpy,
                                   fb_config,
                                   tex_pixmap->pixmap,
                                   attribs);
  glx_tex_pixmap->has_mipmap_space = mipmap;

  XSync (dpy, False);

  if (_cogl_xlib_renderer_untrap_errors (renderer, &trap_state))
    {
      COGL_NOTE (TEXTURE_PIXMAP, "Failed to create pixmap for %p", tex_pixmap);
      _cogl_xlib_renderer_trap_errors (renderer, &trap_state);
      glx_renderer->glXDestroyPixmap (dpy, glx_tex_pixmap->glx_pixmap);
      XSync (dpy, False);
      _cogl_xlib_renderer_untrap_errors (renderer, &trap_state);

      glx_tex_pixmap->glx_pixmap = None;
      return FALSE;
    }

  return TRUE;
}

static CoglBool
_cogl_winsys_texture_pixmap_x11_create (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap;
  CoglContext *ctx = COGL_TEXTURE (tex_pixmap)->context;

  if (!_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_TEXTURE_FROM_PIXMAP))
    {
      tex_pixmap->winsys = NULL;
      return FALSE;
    }

  glx_tex_pixmap = g_new0 (CoglTexturePixmapGLX, 1);

  glx_tex_pixmap->glx_pixmap = None;
  glx_tex_pixmap->can_mipmap = FALSE;
  glx_tex_pixmap->has_mipmap_space = FALSE;

  glx_tex_pixmap->left.glx_tex = NULL;
  glx_tex_pixmap->right.glx_tex = NULL;

  glx_tex_pixmap->left.bind_tex_image_queued = TRUE;
  glx_tex_pixmap->right.bind_tex_image_queued = TRUE;
  glx_tex_pixmap->left.pixmap_bound = FALSE;
  glx_tex_pixmap->right.pixmap_bound = FALSE;

  tex_pixmap->winsys = glx_tex_pixmap;

  if (!try_create_glx_pixmap (ctx, tex_pixmap, FALSE))
    {
      tex_pixmap->winsys = NULL;
      g_free (glx_tex_pixmap);
      return FALSE;
    }

  return TRUE;
}

static void
free_glx_pixmap (CoglContext *context,
                 CoglTexturePixmapGLX *glx_tex_pixmap)
{
  CoglXlibTrapState trap_state;
  CoglRenderer *renderer;
  CoglXlibRenderer *xlib_renderer;
  CoglGLXRenderer *glx_renderer;

  renderer = context->display->renderer;
  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);
  glx_renderer = renderer->winsys;

  if (glx_tex_pixmap->left.pixmap_bound)
    glx_renderer->glXReleaseTexImage (xlib_renderer->xdpy,
                                      glx_tex_pixmap->glx_pixmap,
                                      GLX_FRONT_LEFT_EXT);
  if (glx_tex_pixmap->right.pixmap_bound)
    glx_renderer->glXReleaseTexImage (xlib_renderer->xdpy,
                                      glx_tex_pixmap->glx_pixmap,
                                      GLX_FRONT_RIGHT_EXT);

  /* FIXME - we need to trap errors and synchronize here because
   * of ordering issues between the XPixmap destruction and the
   * GLXPixmap destruction.
   *
   * If the X pixmap is destroyed, the GLX pixmap is destroyed as
   * well immediately, and thus, when Cogl calls glXDestroyPixmap()
   * it'll cause a BadDrawable error.
   *
   * this is technically a bug in the X server, which should not
   * destroy either pixmaps until the call to glXDestroyPixmap(); so
   * at some point we should revisit this code and remove the
   * trap+sync after verifying that the destruction is indeed safe.
   *
   * for reference, see:
   *   http://bugzilla.clutter-project.org/show_bug.cgi?id=2324
   */
  _cogl_xlib_renderer_trap_errors (renderer, &trap_state);
  glx_renderer->glXDestroyPixmap (xlib_renderer->xdpy,
                                  glx_tex_pixmap->glx_pixmap);
  XSync (xlib_renderer->xdpy, False);
  _cogl_xlib_renderer_untrap_errors (renderer, &trap_state);

  glx_tex_pixmap->glx_pixmap = None;
  glx_tex_pixmap->left.pixmap_bound = FALSE;
  glx_tex_pixmap->right.pixmap_bound = FALSE;
}

static void
_cogl_winsys_texture_pixmap_x11_free (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap;

  if (!tex_pixmap->winsys)
    return;

  glx_tex_pixmap = tex_pixmap->winsys;

  free_glx_pixmap (COGL_TEXTURE (tex_pixmap)->context, glx_tex_pixmap);

  if (glx_tex_pixmap->left.glx_tex)
    cogl_object_unref (glx_tex_pixmap->left.glx_tex);

  if (glx_tex_pixmap->right.glx_tex)
    cogl_object_unref (glx_tex_pixmap->right.glx_tex);

  tex_pixmap->winsys = NULL;
  g_free (glx_tex_pixmap);
}

static CoglBool
_cogl_winsys_texture_pixmap_x11_update (CoglTexturePixmapX11 *tex_pixmap,
                                        CoglTexturePixmapStereoMode stereo_mode,
                                        CoglBool needs_mipmap)
{
  CoglTexture *tex = COGL_TEXTURE (tex_pixmap);
  CoglContext *ctx = COGL_TEXTURE (tex_pixmap)->context;
  CoglTexturePixmapGLX *glx_tex_pixmap = tex_pixmap->winsys;
  CoglPixmapTextureEyeGLX *texture_info;
  int buffer;
  CoglGLXRenderer *glx_renderer;

  if (stereo_mode == COGL_TEXTURE_PIXMAP_RIGHT)
    {
      texture_info = &glx_tex_pixmap->right;
      buffer = GLX_FRONT_RIGHT_EXT;
    }
  else
    {
      texture_info = &glx_tex_pixmap->left;
      buffer = GLX_FRONT_LEFT_EXT;
    }

  /* If we don't have a GLX pixmap then fallback */
  if (glx_tex_pixmap->glx_pixmap == None)
    return FALSE;

  glx_renderer = ctx->display->renderer->winsys;

  /* Lazily create a texture to hold the pixmap */
  if (texture_info->glx_tex == NULL)
    {
      CoglPixelFormat texture_format;
      CoglError *error = NULL;

      texture_format = (tex_pixmap->depth >= 32 ?
                        COGL_PIXEL_FORMAT_RGBA_8888_PRE :
                        COGL_PIXEL_FORMAT_RGB_888);

      if (should_use_rectangle (ctx))
        {
          texture_info->glx_tex = COGL_TEXTURE (
            cogl_texture_rectangle_new_with_size (ctx,
                                                  tex->width,
                                                  tex->height));

          _cogl_texture_set_internal_format (tex, texture_format);

          if (cogl_texture_allocate (texture_info->glx_tex, &error))
            COGL_NOTE (TEXTURE_PIXMAP, "Created a texture rectangle for %p",
                       tex_pixmap);
          else
            {
              COGL_NOTE (TEXTURE_PIXMAP, "Falling back for %p because a "
                         "texture rectangle could not be created: %s",
                         tex_pixmap, error->message);
              cogl_error_free (error);
              free_glx_pixmap (ctx, glx_tex_pixmap);
              return FALSE;
            }
        }
      else
        {
          texture_info->glx_tex = COGL_TEXTURE (
            cogl_texture_2d_new_with_size (ctx,
                                           tex->width,
                                           tex->height));

          _cogl_texture_set_internal_format (tex, texture_format);

          if (cogl_texture_allocate (texture_info->glx_tex, &error))
            COGL_NOTE (TEXTURE_PIXMAP, "Created a texture 2d for %p",
                       tex_pixmap);
          else
            {
              COGL_NOTE (TEXTURE_PIXMAP, "Falling back for %p because a "
                         "texture 2d could not be created: %s",
                         tex_pixmap, error->message);
              cogl_error_free (error);
              free_glx_pixmap (ctx, glx_tex_pixmap);
              return FALSE;
            }
        }
    }

  if (needs_mipmap)
    {
      /* If we can't support mipmapping then temporarily fallback */
      if (!glx_tex_pixmap->can_mipmap)
        return FALSE;

      /* Recreate the GLXPixmap if it wasn't previously created with a
       * mipmap tree */
      if (!glx_tex_pixmap->has_mipmap_space)
        {
          free_glx_pixmap (ctx, glx_tex_pixmap);

          COGL_NOTE (TEXTURE_PIXMAP, "Recreating GLXPixmap with mipmap "
                     "support for %p", tex_pixmap);
          if (!try_create_glx_pixmap (ctx, tex_pixmap, TRUE))

            {
              /* If the pixmap failed then we'll permanently fallback
               * to using XImage. This shouldn't happen. */
              COGL_NOTE (TEXTURE_PIXMAP, "Falling back to XGetImage "
                         "updates for %p because creating the GLXPixmap "
                         "with mipmap support failed", tex_pixmap);

              if (texture_info->glx_tex)
                cogl_object_unref (texture_info->glx_tex);
              return FALSE;
            }

          glx_tex_pixmap->left.bind_tex_image_queued = TRUE;
          glx_tex_pixmap->right.bind_tex_image_queued = TRUE;
        }
    }

  if (texture_info->bind_tex_image_queued)
    {
      GLuint gl_handle, gl_target;
      CoglXlibRenderer *xlib_renderer =
        _cogl_xlib_renderer_get_data (ctx->display->renderer);

      cogl_texture_get_gl_texture (texture_info->glx_tex,
                                   &gl_handle, &gl_target);

      COGL_NOTE (TEXTURE_PIXMAP, "Rebinding GLXPixmap for %p", tex_pixmap);

      _cogl_bind_gl_texture_transient (gl_target, gl_handle, FALSE);

      if (texture_info->pixmap_bound)
        glx_renderer->glXReleaseTexImage (xlib_renderer->xdpy,
                                          glx_tex_pixmap->glx_pixmap,
                                          buffer);

      glx_renderer->glXBindTexImage (xlib_renderer->xdpy,
                                     glx_tex_pixmap->glx_pixmap,
                                     buffer,
                                     NULL);

      /* According to the recommended usage in the spec for
       * GLX_EXT_texture_pixmap we should release the texture after
       * we've finished drawing with it and it is undefined what
       * happens if you render to a pixmap that is bound to a texture.
       * However that would require the texture backend to know when
       * Cogl has finished painting and it may be more expensive to
       * keep unbinding the texture. Leaving it bound appears to work
       * on Mesa and NVidia drivers and it is also what Compiz does so
       * it is probably ok */

      texture_info->bind_tex_image_queued = FALSE;
      texture_info->pixmap_bound = TRUE;

      _cogl_texture_2d_externally_modified (texture_info->glx_tex);
    }

  return TRUE;
}

static void
_cogl_winsys_texture_pixmap_x11_damage_notify (CoglTexturePixmapX11 *tex_pixmap)
{
  CoglTexturePixmapGLX *glx_tex_pixmap = tex_pixmap->winsys;

  glx_tex_pixmap->left.bind_tex_image_queued = TRUE;
  glx_tex_pixmap->right.bind_tex_image_queued = TRUE;
}

static CoglTexture *
_cogl_winsys_texture_pixmap_x11_get_texture (CoglTexturePixmapX11 *tex_pixmap,
                                             CoglTexturePixmapStereoMode stereo_mode)
{
  CoglTexturePixmapGLX *glx_tex_pixmap = tex_pixmap->winsys;

  if (stereo_mode == COGL_TEXTURE_PIXMAP_RIGHT)
    return glx_tex_pixmap->right.glx_tex;
  else
    return glx_tex_pixmap->left.glx_tex;
}

static CoglWinsysVtable _cogl_winsys_vtable =
  {
    .id = COGL_WINSYS_ID_GLX,
    .name = "GLX",
    .constraints = (COGL_RENDERER_CONSTRAINT_USES_X11 |
                    COGL_RENDERER_CONSTRAINT_USES_XLIB),

    .renderer_get_proc_address = _cogl_winsys_renderer_get_proc_address,
    .renderer_connect = _cogl_winsys_renderer_connect,
    .renderer_disconnect = _cogl_winsys_renderer_disconnect,
    .renderer_outputs_changed = _cogl_winsys_renderer_outputs_changed,
    .display_setup = _cogl_winsys_display_setup,
    .display_destroy = _cogl_winsys_display_destroy,
    .context_init = _cogl_winsys_context_init,
    .context_deinit = _cogl_winsys_context_deinit,
    .context_get_clock_time = _cogl_winsys_get_clock_time,
    .onscreen_init = _cogl_winsys_onscreen_init,
    .onscreen_deinit = _cogl_winsys_onscreen_deinit,
    .onscreen_bind = _cogl_winsys_onscreen_bind,
    .onscreen_swap_buffers_with_damage =
      _cogl_winsys_onscreen_swap_buffers_with_damage,
    .onscreen_swap_region = _cogl_winsys_onscreen_swap_region,
    .onscreen_get_buffer_age = _cogl_winsys_onscreen_get_buffer_age,
    .onscreen_update_swap_throttled =
      _cogl_winsys_onscreen_update_swap_throttled,
    .onscreen_x11_get_window_xid =
      _cogl_winsys_onscreen_x11_get_window_xid,
    .onscreen_set_visibility = _cogl_winsys_onscreen_set_visibility,
    .onscreen_set_resizable =
      _cogl_winsys_onscreen_set_resizable,

    /* X11 tfp support... */
    /* XXX: instead of having a rather monolithic winsys vtable we could
     * perhaps look for a way to separate these... */
    .texture_pixmap_x11_create =
      _cogl_winsys_texture_pixmap_x11_create,
    .texture_pixmap_x11_free =
      _cogl_winsys_texture_pixmap_x11_free,
    .texture_pixmap_x11_update =
      _cogl_winsys_texture_pixmap_x11_update,
    .texture_pixmap_x11_damage_notify =
      _cogl_winsys_texture_pixmap_x11_damage_notify,
    .texture_pixmap_x11_get_texture =
      _cogl_winsys_texture_pixmap_x11_get_texture,
  };

/* XXX: we use a function because no doubt someone will complain
 * about using c99 member initializers because they aren't portable
 * to windows. We want to avoid having to rigidly follow the real
 * order of members since some members are #ifdefd and we'd have
 * to mirror the #ifdefing to add padding etc. For any winsys that
 * can assume the platform has a sane compiler then we can just use
 * c99 initializers for insane platforms they can initialize
 * the members by name in a function.
 */
const CoglWinsysVtable *
_cogl_winsys_glx_get_vtable (void)
{
  return &_cogl_winsys_vtable;
}

GLXContext
cogl_glx_context_get_glx_context (CoglContext *context)
{
  CoglGLXDisplay *glx_display = context->display->winsys;

  return glx_display->glx_context;
}
