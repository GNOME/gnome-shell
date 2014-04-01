/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <wayland-client.h>
#include <wayland-egl.h>
#include <string.h>
#include <errno.h>

#include "cogl-winsys-egl-wayland-private.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-renderer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-wayland-renderer.h"
#include "cogl-error-private.h"
#include "cogl-poll-private.h"
#include "cogl-frame-info-private.h"

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;

static const CoglWinsysVtable *parent_vtable;

typedef struct _CoglRendererWayland
{
  struct wl_display *wayland_display;
  struct wl_compositor *wayland_compositor;
  struct wl_shell *wayland_shell;
  struct wl_registry *wayland_registry;
  int fd;
} CoglRendererWayland;

typedef struct _CoglDisplayWayland
{
  struct wl_surface *dummy_wayland_surface;
  struct wl_egl_window *dummy_wayland_egl_native_window;
} CoglDisplayWayland;

typedef struct _CoglOnscreenWayland
{
  struct wl_egl_window *wayland_egl_native_window;
  struct wl_surface *wayland_surface;
  struct wl_shell_surface *wayland_shell_surface;

  /* Resizing a wayland framebuffer doesn't take affect
   * until the next swap buffers request, so we have to
   * track the resize geometry until then... */
  int pending_width;
  int pending_height;
  int pending_dx;
  int pending_dy;
  CoglBool has_pending;

  CoglBool shell_surface_type_set;

  CoglList frame_callbacks;
} CoglOnscreenWayland;

typedef struct
{
  CoglList link;
  CoglFrameInfo *frame_info;
  struct wl_callback *callback;
  CoglOnscreen *onscreen;
} FrameCallbackData;

static void
registry_handle_global_cb (void *data,
                           struct wl_registry *registry,
                           uint32_t id,
                           const char *interface,
                           uint32_t version)
{
  CoglRendererEGL *egl_renderer = (CoglRendererEGL *)data;
  CoglRendererWayland *wayland_renderer = egl_renderer->platform;

  if (strcmp (interface, "wl_compositor") == 0)
    wayland_renderer->wayland_compositor =
      wl_registry_bind (registry, id, &wl_compositor_interface, 1);
  else if (strcmp(interface, "wl_shell") == 0)
    wayland_renderer->wayland_shell =
      wl_registry_bind (registry, id, &wl_shell_interface, 1);
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererWayland *wayland_renderer = egl_renderer->platform;

  if (egl_renderer->edpy)
    eglTerminate (egl_renderer->edpy);

  if (wayland_renderer->wayland_display)
    {
      _cogl_poll_renderer_remove_fd (renderer, wayland_renderer->fd);

      if (renderer->foreign_wayland_display == NULL)
        wl_display_disconnect (wayland_renderer->wayland_display);
    }

  g_slice_free (CoglRendererWayland, egl_renderer->platform);
  g_slice_free (CoglRendererEGL, egl_renderer);
}

static const struct wl_registry_listener registry_listener = {
  registry_handle_global_cb,
};

static int64_t
prepare_wayland_display_events (void *user_data)
{
  CoglRenderer *renderer = user_data;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererWayland *wayland_renderer = egl_renderer->platform;
  int flush_ret;

  flush_ret = wl_display_flush (wayland_renderer->wayland_display);

  if (flush_ret == -1)
    {
      /* If the socket buffer became full then we need to wake up the
       * main loop once it is writable again */
      if (errno == EAGAIN)
        {
          _cogl_poll_renderer_modify_fd (renderer,
                                         wayland_renderer->fd,
                                         COGL_POLL_FD_EVENT_IN |
                                         COGL_POLL_FD_EVENT_OUT);
        }
      else if (errno != EINTR)
        {
          /* If the flush failed for some other reason then it's
           * likely that it's going to consistently fail so we'll stop
           * waiting on the file descriptor instead of making the
           * application take up 100% CPU. FIXME: it would be nice if
           * there was some way to report this to the application so
           * that it can quit or recover */
          _cogl_poll_renderer_remove_fd (renderer, wayland_renderer->fd);
        }
    }

  /* Calling this here is a bit dodgy because Cogl usually tries to
   * say that it won't do any event processing until
   * cogl_poll_renderer_dispatch is called. However Wayland doesn't
   * seem to provide any way to query whether the event queue is empty
   * and we would need to do that in order to force the main loop to
   * wake up to call it from dispatch. */
  wl_display_dispatch_pending (wayland_renderer->wayland_display);

  return -1;
}

static void
dispatch_wayland_display_events (void *user_data, int revents)
{
  CoglRenderer *renderer = user_data;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererWayland *wayland_renderer = egl_renderer->platform;

  if ((revents & COGL_POLL_FD_EVENT_IN))
    {
      if (wl_display_dispatch (wayland_renderer->wayland_display) == -1 &&
          errno != EAGAIN &&
          errno != EINTR)
        goto socket_error;
    }

  if ((revents & COGL_POLL_FD_EVENT_OUT))
    {
      int ret = wl_display_flush (wayland_renderer->wayland_display);

      if (ret == -1)
        {
          if (errno != EAGAIN && errno != EINTR)
            goto socket_error;
        }
      else
        {
          /* There is no more data to write so we don't need to wake
           * up when the write buffer is emptied anymore */
          _cogl_poll_renderer_modify_fd (renderer,
                                         wayland_renderer->fd,
                                         COGL_POLL_FD_EVENT_IN);
        }
    }

  return;

 socket_error:
  /* If there was an error on the wayland socket then it's likely that
   * it's going to consistently fail so we'll stop waiting on the file
   * descriptor instead of making the application take up 100% CPU.
   * FIXME: it would be nice if there was some way to report this to
   * the application so that it can quit or recover */
  _cogl_poll_renderer_remove_fd (renderer, wayland_renderer->fd);
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               CoglError **error)
{
  CoglRendererEGL *egl_renderer;
  CoglRendererWayland *wayland_renderer;

  renderer->winsys = g_slice_new0 (CoglRendererEGL);
  egl_renderer = renderer->winsys;
  wayland_renderer = g_slice_new0 (CoglRendererWayland);
  egl_renderer->platform = wayland_renderer;

  egl_renderer->platform_vtable = &_cogl_winsys_egl_vtable;

  if (renderer->foreign_wayland_display)
    {
      wayland_renderer->wayland_display = renderer->foreign_wayland_display;
    }
  else
    {
      wayland_renderer->wayland_display = wl_display_connect (NULL);
      if (!wayland_renderer->wayland_display)
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Failed to connect wayland display");
          goto error;
        }
    }

  wayland_renderer->wayland_registry =
    wl_display_get_registry (wayland_renderer->wayland_display);

  wl_registry_add_listener (wayland_renderer->wayland_registry,
                            &registry_listener,
                            egl_renderer);

  /*
   * Ensure that that we've received the messages setting up the
   * compostor and shell object.
   */
  wl_display_roundtrip (wayland_renderer->wayland_display);
  if (!wayland_renderer->wayland_compositor || !wayland_renderer->wayland_shell)
    {
      _cogl_set_error (error,
                       COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Unable to find wl_compositor or wl_shell");
      goto error;
    }

  egl_renderer->edpy =
    eglGetDisplay ((EGLNativeDisplayType) wayland_renderer->wayland_display);

  if (!_cogl_winsys_egl_renderer_connect_common (renderer, error))
    goto error;

  wayland_renderer->fd = wl_display_get_fd (wayland_renderer->wayland_display);

  if (renderer->wayland_enable_event_dispatch)
    _cogl_poll_renderer_add_fd (renderer,
                                wayland_renderer->fd,
                                COGL_POLL_FD_EVENT_IN,
                                prepare_wayland_display_events,
                                dispatch_wayland_display_events,
                                renderer);

  return TRUE;

error:
  _cogl_winsys_renderer_disconnect (renderer);
  return FALSE;
}

static CoglBool
_cogl_winsys_egl_display_setup (CoglDisplay *display,
                                CoglError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayWayland *wayland_display;

  wayland_display = g_slice_new0 (CoglDisplayWayland);
  egl_display->platform = wayland_display;

  return TRUE;
}

static void
_cogl_winsys_egl_display_destroy (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;

  g_slice_free (CoglDisplayWayland, egl_display->platform);
}

static CoglBool
make_dummy_surface (CoglDisplay *display,
                    CoglError **error)
{
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererWayland *wayland_renderer = egl_renderer->platform;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayWayland *wayland_display = egl_display->platform;
  const char *error_message;

  wayland_display->dummy_wayland_surface =
    wl_compositor_create_surface (wayland_renderer->wayland_compositor);
  if (!wayland_display->dummy_wayland_surface)
    {
      error_message= "Failed to create a dummy wayland surface";
      goto fail;
    }

  wayland_display->dummy_wayland_egl_native_window =
    wl_egl_window_create (wayland_display->dummy_wayland_surface,
                          1,
                          1);
  if (!wayland_display->dummy_wayland_egl_native_window)
    {
      error_message= "Failed to create a dummy wayland native egl surface";
      goto fail;
    }

  egl_display->dummy_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (EGLNativeWindowType)
                            wayland_display->dummy_wayland_egl_native_window,
                            NULL);
  if (egl_display->dummy_surface == EGL_NO_SURFACE)
    {
      error_message= "Unable to create dummy window surface";
      goto fail;
    }

  return TRUE;

 fail:
  _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "%s", error_message);

  return FALSE;
}

static CoglBool
_cogl_winsys_egl_context_created (CoglDisplay *display,
                                  CoglError **error)
{
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglDisplayEGL *egl_display = display->winsys;

  if ((egl_renderer->private_features &
       COGL_EGL_WINSYS_FEATURE_SURFACELESS_CONTEXT) == 0 &&
      !make_dummy_surface(display, error))
    return FALSE;

  if (!_cogl_winsys_egl_make_current (display,
                                      egl_display->dummy_surface,
                                      egl_display->dummy_surface,
                                      egl_display->egl_context))
    {
      _cogl_set_error (error,
                       COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_CONTEXT,
                       "%s",
                       "Unable to eglMakeCurrent with dummy surface");
    }

  return TRUE;
}

static void
_cogl_winsys_egl_cleanup_context (CoglDisplay *display)
{
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayWayland *wayland_display = egl_display->platform;

  if (egl_display->dummy_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_display->dummy_surface);
      egl_display->dummy_surface = EGL_NO_SURFACE;
    }

  if (wayland_display->dummy_wayland_egl_native_window)
    {
      wl_egl_window_destroy (wayland_display->dummy_wayland_egl_native_window);
      wayland_display->dummy_wayland_egl_native_window = NULL;
    }

  if (wayland_display->dummy_wayland_surface)
    {
      wl_surface_destroy (wayland_display->dummy_wayland_surface);
      wayland_display->dummy_wayland_surface = NULL;
    }
}

static CoglBool
_cogl_winsys_egl_context_init (CoglContext *context,
                               CoglError **error)
{
  context->feature_flags |= COGL_FEATURE_ONSCREEN_MULTIPLE;
  COGL_FLAGS_SET (context->features,
                  COGL_FEATURE_ID_ONSCREEN_MULTIPLE, TRUE);
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                  TRUE);
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT,
                  TRUE);

  /* We'll manually handle queueing dirty events when the surface is
   * first shown or when it is resized. Note that this is slightly
   * different from the emulated behaviour that CoglFramebuffer would
   * provide if we didn't set this flag because we want to emit the
   * event on show instead of on allocation. The Wayland protocol
   * delays setting the surface type until the next buffer is attached
   * so attaching a buffer before setting the type would not cause
   * anything to be displayed */
  COGL_FLAGS_SET (context->private_features,
                  COGL_PRIVATE_FEATURE_DIRTY_EVENTS,
                  TRUE);

  return TRUE;
}

static CoglBool
_cogl_winsys_egl_onscreen_init (CoglOnscreen *onscreen,
                                EGLConfig egl_config,
                                CoglError **error)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenWayland *wayland_onscreen;
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererWayland *wayland_renderer = egl_renderer->platform;

  wayland_onscreen = g_slice_new0 (CoglOnscreenWayland);
  egl_onscreen->platform = wayland_onscreen;

  _cogl_list_init (&wayland_onscreen->frame_callbacks);

  if (onscreen->foreign_surface)
    wayland_onscreen->wayland_surface = onscreen->foreign_surface;
  else
    wayland_onscreen->wayland_surface =
      wl_compositor_create_surface (wayland_renderer->wayland_compositor);

  if (!wayland_onscreen->wayland_surface)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Error while creating wayland surface for CoglOnscreen");
      return FALSE;
    }

  wayland_onscreen->wayland_egl_native_window =
    wl_egl_window_create (wayland_onscreen->wayland_surface,
                          cogl_framebuffer_get_width (framebuffer),
                          cogl_framebuffer_get_height (framebuffer));
  if (!wayland_onscreen->wayland_egl_native_window)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Error while creating wayland egl native window "
                   "for CoglOnscreen");
      return FALSE;
    }

  egl_onscreen->egl_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_config,
                            (EGLNativeWindowType)
                            wayland_onscreen->wayland_egl_native_window,
                            NULL);

  if (!onscreen->foreign_surface)
    wayland_onscreen->wayland_shell_surface =
      wl_shell_get_shell_surface (wayland_renderer->wayland_shell,
                                  wayland_onscreen->wayland_surface);

  return TRUE;
}

static void
free_frame_callback_data (FrameCallbackData *callback_data)
{
  cogl_object_unref (callback_data->frame_info);
  wl_callback_destroy (callback_data->callback);
  _cogl_list_remove (&callback_data->link);
  g_slice_free (FrameCallbackData, callback_data);
}

static void
_cogl_winsys_egl_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenWayland *wayland_onscreen = egl_onscreen->platform;
  FrameCallbackData *frame_callback_data, *tmp;

  _cogl_list_for_each_safe (frame_callback_data,
                            tmp,
                            &wayland_onscreen->frame_callbacks,
                            link)
    free_frame_callback_data (frame_callback_data);

  if (wayland_onscreen->wayland_egl_native_window)
    {
      wl_egl_window_destroy (wayland_onscreen->wayland_egl_native_window);
      wayland_onscreen->wayland_egl_native_window = NULL;
    }

  if (!onscreen->foreign_surface)
    {
      /* NB: The wayland protocol docs explicitly state that
       * "wl_shell_surface_destroy() must be called before destroying
       * the wl_surface object." ... */
      if (wayland_onscreen->wayland_shell_surface)
        {
          wl_shell_surface_destroy (wayland_onscreen->wayland_shell_surface);
          wayland_onscreen->wayland_shell_surface = NULL;
        }

      if (wayland_onscreen->wayland_surface)
        {
          wl_surface_destroy (wayland_onscreen->wayland_surface);
          wayland_onscreen->wayland_surface = NULL;
        }
    }

  g_slice_free (CoglOnscreenWayland, wayland_onscreen);
}

static void
flush_pending_resize (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenWayland *wayland_onscreen = egl_onscreen->platform;

  if (wayland_onscreen->has_pending)
    {
      wl_egl_window_resize (wayland_onscreen->wayland_egl_native_window,
                            wayland_onscreen->pending_width,
                            wayland_onscreen->pending_height,
                            wayland_onscreen->pending_dx,
                            wayland_onscreen->pending_dy);

      _cogl_framebuffer_winsys_update_size (COGL_FRAMEBUFFER (onscreen),
                                            wayland_onscreen->pending_width,
                                            wayland_onscreen->pending_height);

      _cogl_onscreen_queue_full_dirty (onscreen);

      wayland_onscreen->pending_dx = 0;
      wayland_onscreen->pending_dy = 0;
      wayland_onscreen->has_pending = FALSE;
    }
}

static void
frame_cb (void *data,
          struct wl_callback *callback,
          uint32_t time)
{
  FrameCallbackData *callback_data = data;
  CoglFrameInfo *info = callback_data->frame_info;
  CoglOnscreen *onscreen = callback_data->onscreen;

  g_assert (callback_data->callback == callback);

  _cogl_onscreen_queue_event (onscreen, COGL_FRAME_EVENT_SYNC, info);
  _cogl_onscreen_queue_event (onscreen, COGL_FRAME_EVENT_COMPLETE, info);

  free_frame_callback_data (callback_data);
}

static const struct wl_callback_listener
frame_listener =
{
  frame_cb
};

static void
_cogl_winsys_onscreen_swap_buffers_with_damage (CoglOnscreen *onscreen,
                                                const int *rectangles,
                                                int n_rectangles)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenWayland *wayland_onscreen = egl_onscreen->platform;
  FrameCallbackData *frame_callback_data = g_slice_new (FrameCallbackData);

  flush_pending_resize (onscreen);

  /* Before calling the winsys function,
   * cogl_onscreen_swap_buffers_with_damage() will have pushed the
   * frame info object onto the end of the pending frames. We can grab
   * it out of the queue now because we don't care about the order and
   * we will just directly queue the event corresponding to the exact
   * frame that Wayland reports as completed. This will steal the
   * reference */
  frame_callback_data->frame_info =
    g_queue_pop_tail (&onscreen->pending_frame_infos);
  frame_callback_data->onscreen = onscreen;

  frame_callback_data->callback =
    wl_surface_frame (wayland_onscreen->wayland_surface);
  wl_callback_add_listener (frame_callback_data->callback,
                            &frame_listener,
                            frame_callback_data);

  _cogl_list_insert (&wayland_onscreen->frame_callbacks,
                     &frame_callback_data->link);

  parent_vtable->onscreen_swap_buffers_with_damage (onscreen,
                                                    rectangles,
                                                    n_rectangles);
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      CoglBool visibility)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenWayland *wayland_onscreen = egl_onscreen->platform;

  /* The first time the onscreen is shown we will set it to toplevel
   * so that it will appear on the screen. If the surface is foreign
   * then we won't have the shell surface and we'll just let the
   * application deal with setting the surface type. */
  if (visibility &&
      wayland_onscreen->wayland_shell_surface &&
      !wayland_onscreen->shell_surface_type_set)
    {
      wl_shell_surface_set_toplevel (wayland_onscreen->wayland_shell_surface);
      wayland_onscreen->shell_surface_type_set = TRUE;
      _cogl_onscreen_queue_full_dirty (onscreen);
    }

  /* FIXME: We should also do something here to hide the surface when
   * visilibity == FALSE. It sounds like there are currently ongoing
   * discussions about adding support for hiding surfaces in the
   * Wayland protocol so we might as well wait until then to add that
   * here. */
}

void
cogl_wayland_renderer_set_foreign_display (CoglRenderer *renderer,
                                           struct wl_display *display)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));

  /* NB: Renderers are considered immutable once connected */
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->foreign_wayland_display = display;
}

void
cogl_wayland_renderer_set_event_dispatch_enabled (CoglRenderer *renderer,
                                                  CoglBool enable)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));
  /* NB: Renderers are considered immutable once connected */
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->wayland_enable_event_dispatch = enable;
}

struct wl_display *
cogl_wayland_renderer_get_display (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), NULL);

  if (renderer->foreign_wayland_display)
    return renderer->foreign_wayland_display;
  else if (renderer->connected)
    {
      CoglRendererEGL *egl_renderer = renderer->winsys;
      CoglRendererWayland *wayland_renderer = egl_renderer->platform;
      return wayland_renderer->wayland_display;
    }
  else
    return NULL;
}

struct wl_surface *
cogl_wayland_onscreen_get_surface (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen;
  CoglOnscreenWayland *wayland_onscreen;

  cogl_framebuffer_allocate (COGL_FRAMEBUFFER (onscreen), NULL);

  egl_onscreen = onscreen->winsys;
  wayland_onscreen = egl_onscreen->platform;

  return wayland_onscreen->wayland_surface;
}

struct wl_shell_surface *
cogl_wayland_onscreen_get_shell_surface (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen;
  CoglOnscreenWayland *wayland_onscreen;

  cogl_framebuffer_allocate (COGL_FRAMEBUFFER (onscreen), NULL);

  egl_onscreen = onscreen->winsys;
  wayland_onscreen = egl_onscreen->platform;

  return wayland_onscreen->wayland_shell_surface;
}

void
cogl_wayland_onscreen_set_foreign_surface (CoglOnscreen *onscreen,
                                           struct wl_surface *surface)
{
  CoglFramebuffer *fb;

  fb = COGL_FRAMEBUFFER (onscreen);
  _COGL_RETURN_IF_FAIL (!fb->allocated);

  onscreen->foreign_surface = surface;
}

void
cogl_wayland_onscreen_resize (CoglOnscreen *onscreen,
                              int           width,
                              int           height,
                              int           offset_x,
                              int           offset_y)
{
  CoglFramebuffer *fb;

  fb = COGL_FRAMEBUFFER (onscreen);
  if (fb->allocated)
    {
      CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
      CoglOnscreenWayland *wayland_onscreen = egl_onscreen->platform;

      if (cogl_framebuffer_get_width (fb) != width ||
          cogl_framebuffer_get_height (fb) != height ||
          offset_x ||
          offset_y)
        {
          wayland_onscreen->pending_width = width;
          wayland_onscreen->pending_height = height;
          wayland_onscreen->pending_dx += offset_x;
          wayland_onscreen->pending_dy += offset_y;
          wayland_onscreen->has_pending = TRUE;

          /* If nothing has been drawn to the framebuffer since the
           * last swap then wl_egl_window_resize will take effect
           * immediately. Otherwise it might not take effect until the
           * next swap, depending on the version of Mesa. To keep
           * consistent behaviour we'll delay the resize until the
           * next swap unless we're sure nothing has been drawn */
          if (!fb->mid_scene)
            flush_pending_resize (onscreen);
        }
    }
  else
    _cogl_framebuffer_winsys_update_size (fb, width, height);
}

static const CoglWinsysEGLVtable
_cogl_winsys_egl_vtable =
  {
    .display_setup = _cogl_winsys_egl_display_setup,
    .display_destroy = _cogl_winsys_egl_display_destroy,
    .context_created = _cogl_winsys_egl_context_created,
    .cleanup_context = _cogl_winsys_egl_cleanup_context,
    .context_init = _cogl_winsys_egl_context_init,
    .onscreen_init = _cogl_winsys_egl_onscreen_init,
    .onscreen_deinit = _cogl_winsys_egl_onscreen_deinit
  };

const CoglWinsysVtable *
_cogl_winsys_egl_wayland_get_vtable (void)
{
  static CoglBool vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  if (!vtable_inited)
    {
      /* The EGL_WAYLAND winsys is a subclass of the EGL winsys so we
         start by copying its vtable */

      parent_vtable = _cogl_winsys_egl_get_vtable ();
      vtable = *parent_vtable;

      vtable.id = COGL_WINSYS_ID_EGL_WAYLAND;
      vtable.name = "EGL_WAYLAND";

      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;

      vtable.onscreen_swap_buffers_with_damage =
        _cogl_winsys_onscreen_swap_buffers_with_damage;

      vtable.onscreen_set_visibility =
        _cogl_winsys_onscreen_set_visibility;

      vtable_inited = TRUE;
    }

  return &vtable;
}
