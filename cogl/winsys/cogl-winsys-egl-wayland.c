/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
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

#include "cogl-winsys-egl-wayland-private.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-renderer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-wayland-renderer.h"

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;

static const CoglWinsysVtable *parent_vtable;

typedef struct _CoglRendererWayland
{
  struct wl_display *wayland_display;
  struct wl_compositor *wayland_compositor;
  struct wl_shell *wayland_shell;
} CoglRendererWayland;

typedef struct _CoglDisplayWayland
{
  struct wl_surface *wayland_surface;
  struct wl_egl_window *wayland_egl_native_window;
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
} CoglOnscreenWayland;

static void
display_handle_global_cb (struct wl_display *display,
                          uint32_t id,
                          const char *interface,
                          uint32_t version,
                          void *data)
{
  CoglRendererEGL *egl_renderer = (CoglRendererEGL *)data;
  CoglRendererWayland *wayland_renderer = egl_renderer->platform;

  if (strcmp (interface, "wl_compositor") == 0)
    wayland_renderer->wayland_compositor =
      wl_display_bind (display, id, &wl_compositor_interface);
  else if (strcmp(interface, "wl_shell") == 0)
    wayland_renderer->wayland_shell =
      wl_display_bind (display, id, &wl_shell_interface);
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;

  eglTerminate (egl_renderer->edpy);

  g_slice_free (CoglRendererWayland, egl_renderer->platform);
  g_slice_free (CoglRendererEGL, egl_renderer);
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  CoglRendererEGL *egl_renderer;
  CoglRendererWayland *wayland_renderer;

  renderer->winsys = g_slice_new0 (CoglRendererEGL);
  egl_renderer = renderer->winsys;
  wayland_renderer = g_slice_new0 (CoglRendererWayland);
  egl_renderer->platform = wayland_renderer;

  egl_renderer->platform_vtable = &_cogl_winsys_egl_vtable;

  /* The EGL API doesn't provide for a way to explicitly select a
   * platform when the driver can support multiple. Mesa allows
   * selection using an environment variable though so that's what
   * we're doing here... */
  g_setenv ("EGL_PLATFORM", "wayland", 1);

  if (renderer->foreign_wayland_display)
    {
      wayland_renderer->wayland_display = renderer->foreign_wayland_display;
      /* XXX: For now we have to assume that if a foreign display is
       * given then a foreign compositor and shell must also have been
       * given because wayland doesn't provide a way to
       * retrospectively be notified of the these objects. */
      g_assert (renderer->foreign_wayland_compositor);
      g_assert (renderer->foreign_wayland_shell);
      wayland_renderer->wayland_compositor =
        renderer->foreign_wayland_compositor;
      wayland_renderer->wayland_shell = renderer->foreign_wayland_shell;
    }
  else
    {
      wayland_renderer->wayland_display = wl_display_connect (NULL);
      if (!wayland_renderer->wayland_display)
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Failed to connect wayland display");
          goto error;
        }

      wl_display_add_global_listener (wayland_renderer->wayland_display,
                                      display_handle_global_cb,
                                      egl_renderer);
    }

  /*
   * Ensure that that we've received the messages setting up the
   * compostor and shell object. This is better than just
   * wl_display_iterate since it will always ensure that something
   * is available to be read
   */
  while (!(wayland_renderer->wayland_compositor &&
           wayland_renderer->wayland_shell))
    wl_display_roundtrip (wayland_renderer->wayland_display);

  egl_renderer->edpy =
    eglGetDisplay ((EGLNativeDisplayType) wayland_renderer->wayland_display);

  if (!_cogl_winsys_egl_renderer_connect_common (renderer, error))
    goto error;

  return TRUE;

error:
  _cogl_winsys_renderer_disconnect (renderer);
  return FALSE;
}

static CoglBool
_cogl_winsys_egl_display_setup (CoglDisplay *display,
                                GError **error)
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
_cogl_winsys_egl_context_created (CoglDisplay *display,
                                  GError **error)
{
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererWayland *wayland_renderer = egl_renderer->platform;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayWayland *wayland_display = egl_display->platform;
  const char *error_message;

  wayland_display->wayland_surface =
    wl_compositor_create_surface (wayland_renderer->wayland_compositor);
  if (!wayland_display->wayland_surface)
    {
      error_message= "Failed to create a dummy wayland surface";
      goto fail;
    }

  wayland_display->wayland_egl_native_window =
    wl_egl_window_create (wayland_display->wayland_surface,
                          1,
                          1);
  if (!wayland_display->wayland_egl_native_window)
    {
      error_message= "Failed to create a dummy wayland native egl surface";
      goto fail;
    }

  egl_display->dummy_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (EGLNativeWindowType)
                            wayland_display->wayland_egl_native_window,
                            NULL);
  if (egl_display->dummy_surface == EGL_NO_SURFACE)
    {
      error_message= "Unable to eglMakeCurrent with dummy surface";
      goto fail;
    }

  if (!eglMakeCurrent (egl_renderer->edpy,
                       egl_display->dummy_surface,
                       egl_display->dummy_surface,
                       egl_display->egl_context))
    {
      error_message = "Unable to eglMakeCurrent with dummy surface";
      goto fail;
    }

  return TRUE;

 fail:
  g_set_error (error, COGL_WINSYS_ERROR,
               COGL_WINSYS_ERROR_CREATE_CONTEXT,
               "%s", error_message);
  return FALSE;
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

  if (wayland_display->wayland_egl_native_window)
    {
      wl_egl_window_destroy (wayland_display->wayland_egl_native_window);
      wayland_display->wayland_egl_native_window = NULL;
    }

  if (wayland_display->wayland_surface)
    {
      wl_surface_destroy (wayland_display->wayland_surface);
      wayland_display->wayland_surface = NULL;
    }
}

static CoglBool
_cogl_winsys_egl_context_init (CoglContext *context,
                               GError **error)
{
  context->feature_flags |= COGL_FEATURE_ONSCREEN_MULTIPLE;
  COGL_FLAGS_SET (context->features,
                  COGL_FEATURE_ID_ONSCREEN_MULTIPLE, TRUE);
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                  TRUE);

  return TRUE;
}

static CoglBool
_cogl_winsys_egl_onscreen_init (CoglOnscreen *onscreen,
                                EGLConfig egl_config,
                                GError **error)
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

  wayland_onscreen->wayland_surface =
    wl_compositor_create_surface (wayland_renderer->wayland_compositor);
  if (!wayland_onscreen->wayland_surface)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Error while creating wayland surface for CoglOnscreen");
      return FALSE;
    }

  wayland_onscreen->wayland_shell_surface =
    wl_shell_get_shell_surface (wayland_renderer->wayland_shell,
                                wayland_onscreen->wayland_surface);

  wayland_onscreen->wayland_egl_native_window =
    wl_egl_window_create (wayland_onscreen->wayland_surface,
                          cogl_framebuffer_get_width (framebuffer),
                          cogl_framebuffer_get_height (framebuffer));
  if (!wayland_onscreen->wayland_egl_native_window)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
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

  wl_shell_surface_set_toplevel (wayland_onscreen->wayland_shell_surface);

  return TRUE;
}

static void
_cogl_winsys_egl_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenWayland *wayland_onscreen = egl_onscreen->platform;

  if (wayland_onscreen->wayland_egl_native_window)
    {
      wl_egl_window_destroy (wayland_onscreen->wayland_egl_native_window);
      wayland_onscreen->wayland_egl_native_window = NULL;
    }

  if (wayland_onscreen->wayland_surface)
    {
      wl_surface_destroy (wayland_onscreen->wayland_surface);
      wayland_onscreen->wayland_surface = NULL;
    }

  g_slice_free (CoglOnscreenWayland, wayland_onscreen);
}

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  CoglFramebuffer *fb = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = fb->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererWayland *wayland_renderer = egl_renderer->platform;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenWayland *wayland_onscreen = egl_onscreen->platform;

  if (wayland_onscreen->has_pending)
    {
      wl_egl_window_resize (wayland_onscreen->wayland_egl_native_window,
                            wayland_onscreen->pending_width,
                            wayland_onscreen->pending_height,
                            wayland_onscreen->pending_dx,
                            wayland_onscreen->pending_dy);

      _cogl_framebuffer_winsys_update_size (fb,
                                            wayland_onscreen->pending_width,
                                            wayland_onscreen->pending_height);
      wayland_onscreen->has_pending = FALSE;
    }

  /* chain-up */
  parent_vtable->onscreen_swap_buffers (onscreen);

  /*
   * The implementation of eglSwapBuffers may do a flush however the semantics
   * of eglSwapBuffers on Wayland has changed in the past. So to be safe to
   * the implementation changing we should explicitly ensure all messages are
   * sent.
   */
  wl_display_flush (wayland_renderer->wayland_display);
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

void
cogl_wayland_renderer_set_foreign_compositor (CoglRenderer *renderer,
                                              struct wl_compositor *compositor)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));

  /* NB: Renderers are considered immutable once connected */
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->foreign_wayland_compositor = compositor;
}

struct wl_compositor *
cogl_wayland_renderer_get_compositor (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), NULL);

  if (renderer->foreign_wayland_compositor)
    return renderer->foreign_wayland_compositor;
  else if (renderer->connected)
    {
      CoglRendererEGL *egl_renderer = renderer->winsys;
      CoglRendererWayland *wayland_renderer = egl_renderer->platform;
      return wayland_renderer->wayland_compositor;
    }
  else
    return NULL;
}

void
cogl_wayland_renderer_set_foreign_shell (CoglRenderer *renderer,
                                         struct wl_shell *shell)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));

  /* NB: Renderers are considered immutable once connected */
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->foreign_wayland_shell = shell;
}

struct wl_shell *
cogl_wayland_renderer_get_shell (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), NULL);

  if (renderer->foreign_wayland_shell)
    return renderer->foreign_wayland_shell;
  else if (renderer->connected)
    {
      CoglRendererEGL *egl_renderer = renderer->winsys;
      CoglRendererWayland *wayland_renderer = egl_renderer->platform;
      return wayland_renderer->wayland_shell;
    }
  else
    return NULL;
}

struct wl_surface *
cogl_wayland_onscreen_get_surface (CoglOnscreen *onscreen)
{
  CoglFramebuffer *fb;

  fb = COGL_FRAMEBUFFER (onscreen);
  if (fb->allocated)
    {
      CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
      CoglOnscreenWayland *wayland_onscreen = egl_onscreen->platform;
      return wayland_onscreen->wayland_surface;
    }
  else
    return NULL;
}

struct wl_shell_surface *
cogl_wayland_onscreen_get_shell_surface (CoglOnscreen *onscreen)
{
  CoglFramebuffer *fb;

  fb = COGL_FRAMEBUFFER (onscreen);
  if (fb->allocated)
    {
      CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
      CoglOnscreenWayland *wayland_onscreen = egl_onscreen->platform;
      return wayland_onscreen->wayland_shell_surface;
    }
  else
    return NULL;
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

      vtable.onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers;

      vtable_inited = TRUE;
    }

  return &vtable;
}
