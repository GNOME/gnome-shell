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

#include <android/native_window.h>

#include "cogl-winsys-egl-android-private.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-renderer-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-private.h"

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;

typedef struct _CoglDisplayAndroid
{
  int egl_surface_width;
  int egl_surface_height;
  CoglBool have_onscreen;
} CoglDisplayAndroid;

static ANativeWindow *android_native_window;

void
cogl_android_set_native_window (ANativeWindow *window)
{
  _cogl_init ();

  android_native_window = window;
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;

  eglTerminate (egl_renderer->edpy);

  g_slice_free (CoglRendererEGL, egl_renderer);
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               CoglError **error)
{
  CoglRendererEGL *egl_renderer;

  renderer->winsys = g_slice_new0 (CoglRendererEGL);
  egl_renderer = renderer->winsys;

  egl_renderer->platform_vtable = &_cogl_winsys_egl_vtable;

  egl_renderer->edpy = eglGetDisplay (EGL_DEFAULT_DISPLAY);

  if (!_cogl_winsys_egl_renderer_connect_common (renderer, error))
    goto error;

  return TRUE;

error:
  _cogl_winsys_renderer_disconnect (renderer);
  return FALSE;
}

static CoglBool
_cogl_winsys_egl_context_created (CoglDisplay *display,
                                  CoglError **error)
{
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayAndroid *android_display = egl_display->platform;
  const char *error_message;
  EGLint format;

  if (android_native_window == NULL)
    {
      error_message = "No ANativeWindow window specified with "
        "cogl_android_set_native_window()";
      goto fail;
    }

  /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
   * guaranteed to be accepted by ANativeWindow_setBuffersGeometry ().
   * As soon as we picked a EGLConfig, we can safely reconfigure the
   * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
  eglGetConfigAttrib (egl_renderer->edpy,
                      egl_display->egl_config,
                      EGL_NATIVE_VISUAL_ID, &format);

  ANativeWindow_setBuffersGeometry (android_native_window,
                                    0,
                                    0,
                                    format);

  egl_display->egl_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (NativeWindowType) android_native_window,
                            NULL);
  if (egl_display->egl_surface == EGL_NO_SURFACE)
    {
      error_message = "Unable to create EGL window surface";
      goto fail;
    }

  if (!_cogl_winsys_egl_make_current (display,
                                      egl_display->egl_surface,
                                      egl_display->egl_surface,
                                      egl_display->egl_context))
    {
      error_message = "Unable to eglMakeCurrent with egl surface";
      goto fail;
    }

  eglQuerySurface (egl_renderer->edpy,
                   egl_display->egl_surface,
                   EGL_WIDTH,
                   &android_display->egl_surface_width);

  eglQuerySurface (egl_renderer->edpy,
                   egl_display->egl_surface,
                   EGL_HEIGHT,
                   &android_display->egl_surface_height);

  return TRUE;

 fail:
  _cogl_set_error (error, COGL_WINSYS_ERROR,
               COGL_WINSYS_ERROR_CREATE_CONTEXT,
               "%s", error_message);
  return FALSE;
}

static CoglBool
_cogl_winsys_egl_display_setup (CoglDisplay *display,
                                CoglError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayAndroid *android_display;

  android_display = g_slice_new0 (CoglDisplayAndroid);
  egl_display->platform = android_display;

  return TRUE;
}

static void
_cogl_winsys_egl_display_destroy (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;

  g_slice_free (CoglDisplayAndroid, egl_display->platform);
}

static CoglBool
_cogl_winsys_egl_onscreen_init (CoglOnscreen *onscreen,
                                EGLConfig egl_config,
                                CoglError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayAndroid *android_display = egl_display->platform;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;

  if (android_display->have_onscreen)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "EGL platform only supports a single onscreen window");
      return FALSE;
    }

  egl_onscreen->egl_surface = egl_display->egl_surface;

  _cogl_framebuffer_winsys_update_size (framebuffer,
                                        android_display->egl_surface_width,
                                        android_display->egl_surface_height);

  android_display->have_onscreen = TRUE;

  return TRUE;
}

static const CoglWinsysEGLVtable
_cogl_winsys_egl_vtable =
  {
    .display_setup = _cogl_winsys_egl_display_setup,
    .display_destroy = _cogl_winsys_egl_display_destroy,
    .context_created = _cogl_winsys_egl_context_created,
    .onscreen_init = _cogl_winsys_egl_onscreen_init,
  };

const CoglWinsysVtable *
_cogl_winsys_egl_android_get_vtable (void)
{
  static CoglBool vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  if (!vtable_inited)
    {
      /* The EGL_ANDROID winsys is a subclass of the EGL winsys so we
         start by copying its vtable */

      vtable = *_cogl_winsys_egl_get_vtable ();

      vtable.id = COGL_WINSYS_ID_EGL_ANDROID;
      vtable.name = "EGL_ANDROID";

      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;

      vtable_inited = TRUE;
    }

  return &vtable;
}
