/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2014 Canonical Ltd.
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
 *   Marco Trevisan <marco.trevisan@canonical.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <mir_toolkit/mir_client_library.h>
#include <string.h>
#include <errno.h>

#include "cogl-winsys-egl-mir-private.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-renderer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-error-private.h"

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;

static const CoglWinsysVtable *parent_vtable;

typedef struct _CoglRendererMir
{
  MirConnection *mir_connection;
} CoglRendererMir;

typedef struct _CoglDisplayMir
{
  MirSurface *dummy_mir_surface;
} CoglDisplayMir;

typedef struct _CoglOnscreenMir
{
  MirSurface *mir_surface;
  MirSurfaceState last_state;
} CoglOnscreenMir;


static MirPixelFormat
_mir_connection_get_valid_format (MirConnection *connection)
{
  MirPixelFormat formats[mir_pixel_formats];
  guint valid_formats;
  guint f;

  mir_connection_get_available_surface_formats(connection, formats,
                                               mir_pixel_formats,
                                               &valid_formats);

  for (f = 0; f < valid_formats; f++)
    {
      switch (formats[f])
        {
          case mir_pixel_format_abgr_8888:
          case mir_pixel_format_xbgr_8888:
          case mir_pixel_format_argb_8888:
          case mir_pixel_format_xrgb_8888:
            return formats[f];
          default:
            continue;
        }
  }

  return mir_pixel_format_invalid;
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererMir *mir_renderer = egl_renderer->platform;

  if (egl_renderer->edpy)
    eglTerminate (egl_renderer->edpy);

  if (mir_connection_is_valid (mir_renderer->mir_connection))
    mir_connection_release (mir_renderer->mir_connection);

  g_slice_free (CoglRendererMir, egl_renderer->platform);
  g_slice_free (CoglRendererEGL, egl_renderer);
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               CoglError **error)
{
  CoglRendererEGL *egl_renderer;
  CoglRendererMir *mir_renderer;
  MirEGLNativeDisplayType mir_native_dpy;

  renderer->winsys = g_slice_new0 (CoglRendererEGL);
  egl_renderer = renderer->winsys;
  mir_renderer = g_slice_new0 (CoglRendererMir);
  egl_renderer->platform = mir_renderer;

  egl_renderer->platform_vtable = &_cogl_winsys_egl_vtable;

  mir_renderer->mir_connection = mir_connect_sync (NULL, __PRETTY_FUNCTION__);
  if (!mir_connection_is_valid (mir_renderer->mir_connection))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Failed to connect mir display");
      goto error;
    }

  mir_native_dpy =
    mir_connection_get_egl_native_display (mir_renderer->mir_connection);
  egl_renderer->edpy =
    eglGetDisplay (mir_native_dpy);

  if (!_cogl_winsys_egl_renderer_connect_common (renderer, error))
    goto error;

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
  CoglDisplayMir *mir_display;

  mir_display = g_slice_new0 (CoglDisplayMir);
  egl_display->platform = mir_display;

  return TRUE;
}

static void
_cogl_winsys_egl_display_destroy (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;

  g_slice_free (CoglDisplayMir, egl_display->platform);
}

static CoglBool
make_dummy_surface (CoglDisplay *display,
                    CoglError **error)
{
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererMir *mir_renderer = egl_renderer->platform;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayMir *mir_display = egl_display->platform;
  MirEGLNativeWindowType dummy_mir_egl_native_window;
  MirSurfaceParameters surfaceparm;
  const char *error_message;

  surfaceparm.name = "CoglDummySurface";
  surfaceparm.width = 1;
  surfaceparm.height = 1;
  surfaceparm.pixel_format = _mir_connection_get_valid_format (mir_renderer->mir_connection);
  surfaceparm.buffer_usage = mir_buffer_usage_hardware;
  surfaceparm.output_id = mir_display_output_id_invalid;

  mir_display->dummy_mir_surface =
    mir_connection_create_surface_sync (mir_renderer->mir_connection, &surfaceparm);
  if (!mir_display->dummy_mir_surface)
    {
      error_message= "Failed to create a dummy mir surface";
      goto fail;
    }

  dummy_mir_egl_native_window =
    mir_surface_get_egl_native_window (mir_display->dummy_mir_surface);
  if (!dummy_mir_egl_native_window)
    {
      error_message= "Failed to get a dummy mir native egl surface";
      goto fail;
    }

  egl_display->dummy_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (EGLNativeWindowType) dummy_mir_egl_native_window,
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
  CoglDisplayMir *mir_display = egl_display->platform;

  if (egl_display->dummy_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_display->dummy_surface);
      egl_display->dummy_surface = EGL_NO_SURFACE;
    }

  if (mir_display->dummy_mir_surface)
    {
      mir_surface_release (mir_display->dummy_mir_surface, NULL, NULL);
      mir_display->dummy_mir_surface = NULL;
    }
}

static CoglBool
_cogl_winsys_egl_context_init (CoglContext *context,
                               CoglError **error)
{
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
                                CoglError **error)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenMir *mir_onscreen;
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererMir *mir_renderer = egl_renderer->platform;
  MirEGLNativeWindowType mir_egl_native_window;
  MirSurfaceParameters surfaceparm;

  mir_onscreen = g_slice_new0 (CoglOnscreenMir);
  egl_onscreen->platform = mir_onscreen;

  surfaceparm.name = "CoglSurface";
  surfaceparm.width = cogl_framebuffer_get_width (framebuffer);
  surfaceparm.height = cogl_framebuffer_get_height (framebuffer);
  surfaceparm.pixel_format = _mir_connection_get_valid_format (mir_renderer->mir_connection);
  surfaceparm.buffer_usage = mir_buffer_usage_hardware;
  surfaceparm.output_id = mir_display_output_id_invalid;

  mir_onscreen->mir_surface =
    mir_connection_create_surface_sync (mir_renderer->mir_connection, &surfaceparm);

  mir_onscreen->last_state = mir_surface_get_state (mir_onscreen->mir_surface);

  if (!mir_surface_is_valid (mir_onscreen->mir_surface))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                      "Error while creating mir surface for CoglOnscreen");
      return FALSE;
    }

  mir_egl_native_window = mir_surface_get_egl_native_window (mir_onscreen->mir_surface);
  if (!mir_egl_native_window)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Error while getting mir egl native window "
                       "for CoglOnscreen");
      return FALSE;
    }

  egl_onscreen->egl_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_config,
                            (EGLNativeWindowType) mir_egl_native_window,
                            NULL);

  return TRUE;
}

static void
_cogl_winsys_egl_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenMir *mir_onscreen = egl_onscreen->platform;

  if (mir_onscreen->mir_surface)
    {
      mir_surface_release (mir_onscreen->mir_surface, NULL, NULL);
      mir_onscreen->mir_surface = NULL;
    }

  mir_onscreen->last_state = mir_surface_state_unknown;

  g_slice_free (CoglOnscreenMir, mir_onscreen);
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      CoglBool visibility)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenMir *mir_onscreen = egl_onscreen->platform;
  MirSurfaceState new_state, current_state;

  current_state = mir_surface_get_state (mir_onscreen->mir_surface);

  if ((visibility && current_state != mir_surface_state_minimized) ||
      (!visibility && current_state == mir_surface_state_minimized))
    {
      return;
    }

  if (!visibility)
    {
      new_state = mir_surface_state_minimized;
      mir_onscreen->last_state = current_state;
    }
  else
    {
      new_state = mir_onscreen->last_state;
    }

  mir_surface_set_state (mir_onscreen->mir_surface, new_state);
}

MirSurface *
cogl_mir_onscreen_get_surface (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen;
  CoglOnscreenMir *mir_onscreen;

  cogl_framebuffer_allocate (COGL_FRAMEBUFFER (onscreen), NULL);

  egl_onscreen = onscreen->winsys;
  mir_onscreen = egl_onscreen->platform;

  return mir_onscreen->mir_surface;
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
_cogl_winsys_egl_mir_get_vtable (void)
{
  static CoglBool vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  if (!vtable_inited)
    {
      /* The EGL_MIR winsys is a subclass of the EGL winsys so we
         start by copying its vtable */

      parent_vtable = _cogl_winsys_egl_get_vtable ();
      vtable = *parent_vtable;

      vtable.id = COGL_WINSYS_ID_EGL_MIR;
      vtable.name = "EGL_MIR";

      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;

      vtable.onscreen_set_visibility =
        _cogl_winsys_onscreen_set_visibility;

      vtable_inited = TRUE;
    }

  return &vtable;
}
