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

#include "cogl-winsys-egl-gdl-private.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-renderer-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-swap-chain-private.h"

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;

typedef struct _CoglRendererGDL
{
  CoglBool gdl_initialized;
} CoglRendererGDL;

typedef struct _CoglDisplayGDL
{
  int egl_surface_width;
  int egl_surface_height;
  CoglBool have_onscreen;
} CoglDisplayGDL;

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererGDL *gdl_renderer = egl_renderer->platform;

  if (gdl_renderer->gdl_initialized)
    gdl_close ();

  eglTerminate (egl_renderer->edpy);

  g_slice_free (CoglRendererEGL, egl_renderer);
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               CoglError **error)
{
  CoglRendererEGL *egl_renderer;
  CoglRendererGDL *gdl_renderer;
  gdl_ret_t rc = GDL_SUCCESS;
  gdl_display_info_t gdl_display_info;

  renderer->winsys = g_slice_new0 (CoglRendererEGL);
  egl_renderer = renderer->winsys;

  gdl_renderer = g_slice_new0 (CoglRendererGDL);
  egl_renderer->platform = gdl_renderer;

  egl_renderer->platform_vtable = &_cogl_winsys_egl_vtable;

  egl_renderer->edpy = eglGetDisplay (EGL_DEFAULT_DISPLAY);

  if (!_cogl_winsys_egl_renderer_connect_common (renderer, error))
    goto error;

  /* Check we can talk to the GDL library */
  rc = gdl_init (NULL);
  if (rc != GDL_SUCCESS)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "GDL initialize failed. %s",
                   gdl_get_error_string (rc));
      goto error;
    }

  rc = gdl_get_display_info (GDL_DISPLAY_ID_0, &gdl_display_info);
  if (rc != GDL_SUCCESS)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "GDL failed to get display information: %s",
                   gdl_get_error_string (rc));
      gdl_close ();
      goto error;
    }

  gdl_close ();

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
  CoglDisplayGDL *gdl_display = egl_display->platform;
  const char *error_message;

  egl_display->egl_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (NativeWindowType) display->gdl_plane,
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
                   &gdl_display->egl_surface_width);

  eglQuerySurface (egl_renderer->edpy,
                   egl_display->egl_surface,
                   EGL_HEIGHT,
                   &gdl_display->egl_surface_height);

  return TRUE;

 fail:
  _cogl_set_error (error, COGL_WINSYS_ERROR,
               COGL_WINSYS_ERROR_CREATE_CONTEXT,
               "%s", error_message);
  return FALSE;
}

static CoglBool
gdl_plane_init (CoglDisplay *display, CoglError **error)
{
  CoglBool ret = TRUE;
  gdl_color_space_t colorSpace = GDL_COLOR_SPACE_RGB;
  gdl_pixel_format_t pixfmt = GDL_PF_ARGB_32;
  gdl_rectangle_t dstRect;
  gdl_display_info_t display_info;
  gdl_ret_t rc = GDL_SUCCESS;

  if (!display->gdl_plane)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR, COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "No GDL plane specified with "
                   "cogl_gdl_display_set_plane");
      return FALSE;
    }

  rc = gdl_init (NULL);
  if (rc != GDL_SUCCESS)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR, COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "GDL initialize failed. %s", gdl_get_error_string (rc));
      return FALSE;
    }

  rc = gdl_get_display_info (GDL_DISPLAY_ID_0, &display_info);
  if (rc != GDL_SUCCESS)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR, COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "GDL failed to get display infomation: %s",
                   gdl_get_error_string (rc));
      gdl_close ();
      return FALSE;
    }

  dstRect.origin.x = 0;
  dstRect.origin.y = 0;
  dstRect.width = display_info.tvmode.width;
  dstRect.height = display_info.tvmode.height;

  /* Configure the plane attribute. */
  rc = gdl_plane_reset (display->gdl_plane);
  if (rc == GDL_SUCCESS)
    rc = gdl_plane_config_begin (display->gdl_plane);

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_set_attr (GDL_PLANE_SRC_COLOR_SPACE, &colorSpace);

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_set_attr (GDL_PLANE_PIXEL_FORMAT, &pixfmt);

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_set_attr (GDL_PLANE_DST_RECT, &dstRect);

  /* Default to triple buffering if the swap_chain doesn't have an explicit
   * length */
  if (rc == GDL_SUCCESS)
    {
      if (display->onscreen_template->config.swap_chain &&
          display->onscreen_template->config.swap_chain->length != -1)
        rc = gdl_plane_set_uint (GDL_PLANE_NUM_GFX_SURFACES,
                                 display->onscreen_template->
                                 config.swap_chain->length);
      else
        rc = gdl_plane_set_uint (GDL_PLANE_NUM_GFX_SURFACES, 3);
    }

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_config_end (GDL_FALSE);
  else
    gdl_plane_config_end (GDL_TRUE);

  if (rc != GDL_SUCCESS)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR, COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "GDL configuration failed: %s.", gdl_get_error_string (rc));
      ret = FALSE;
    }

  gdl_close ();

  return ret;
}

static CoglBool
_cogl_winsys_egl_display_setup (CoglDisplay *display,
                                CoglError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayGDL *gdl_display;

  gdl_display = g_slice_new0 (CoglDisplayGDL);
  egl_display->platform = gdl_display;

  if (!gdl_plane_init (display, error))
    return FALSE;

  return TRUE;
}

static void
_cogl_winsys_egl_display_destroy (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;

  g_slice_free (CoglDisplayGDL, egl_display->platform);
}

static void
_cogl_winsys_egl_cleanup_context (CoglDisplay *display)
{
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglDisplayEGL *egl_display = display->winsys;

  if (egl_display->egl_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_display->egl_surface);
      egl_display->egl_surface = EGL_NO_SURFACE;
    }
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
  CoglDisplayGDL *gdl_display = egl_display->platform;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;

  if (gdl_display->have_onscreen)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "EGL platform only supports a single onscreen window");
      return FALSE;
    }

  egl_onscreen->egl_surface = egl_display->egl_surface;

  _cogl_framebuffer_winsys_update_size (framebuffer,
                                        gdl_display->egl_surface_width,
                                        gdl_display->egl_surface_height);
  gdl_display->have_onscreen = TRUE;

  return TRUE;
}

static int
_cogl_winsys_egl_add_config_attributes (CoglDisplay *display,
                                        CoglFramebufferConfig *config,
                                        EGLint *attributes)
{
  int i = 0;

  /* XXX: Why does the GDL platform choose these by default? */
  attributes[i++] = EGL_BIND_TO_TEXTURE_RGBA;
  attributes[i++] = EGL_TRUE;
  attributes[i++] = EGL_BIND_TO_TEXTURE_RGB;
  attributes[i++] = EGL_TRUE;

  return i;
}

static const CoglWinsysEGLVtable
_cogl_winsys_egl_vtable =
  {
    .display_setup = _cogl_winsys_egl_display_setup,
    .display_destroy = _cogl_winsys_egl_display_destroy,
    .context_created = _cogl_winsys_egl_context_created,
    .cleanup_context = _cogl_winsys_egl_cleanup_context,
    .onscreen_init = _cogl_winsys_egl_onscreen_init,
    .add_config_attributes = _cogl_winsys_egl_add_config_attributes
  };

const CoglWinsysVtable *
_cogl_winsys_egl_gdl_get_vtable (void)
{
  static CoglBool vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  if (!vtable_inited)
    {
      /* The EGL_GDL winsys is a subclass of the EGL winsys so we
         start by copying its vtable */

      vtable = *_cogl_winsys_egl_get_vtable ();

      vtable.id = COGL_WINSYS_ID_EGL_GDL;
      vtable.name = "EGL_GDL";

      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;

      vtable_inited = TRUE;
    }

  return &vtable;
}
