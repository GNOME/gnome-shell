/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010,2011 Intel Corporation.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"

#include "cogl-util.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-winsys-private.h"
#include "cogl-feature-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer.h"
#include "cogl-onscreen-private.h"
#include "cogl-swap-chain-private.h"
#include "cogl-renderer-private.h"
#include "cogl-onscreen-template-private.h"

#include "cogl-private.h"

#ifdef COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT
#include <android/native_window.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gi18n-lib.h>

#define MAX_EGL_CONFIG_ATTRIBS 30

/* Define a set of arrays containing the functions required from GL
   for each winsys feature */
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  egl_private_flags)                    \
  static const CoglFeatureFunction                                      \
  cogl_egl_feature_ ## name ## _funcs[] = {
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)                   \
  { G_STRINGIFY (name), G_STRUCT_OFFSET (CoglRendererEGL, pf_ ## name) },
#define COGL_WINSYS_FEATURE_END()               \
  { NULL, 0 },                                  \
    };
#include "cogl-winsys-egl-feature-functions.h"

/* Define an array of features */
#undef COGL_WINSYS_FEATURE_BEGIN
#define COGL_WINSYS_FEATURE_BEGIN(name, namespaces, extension_names,    \
                                  egl_private_flags)                    \
  { 255, 255, 0, namespaces, extension_names,                           \
      0, egl_private_flags,                                             \
      0,                                                                \
      cogl_egl_feature_ ## name ## _funcs },
#undef COGL_WINSYS_FEATURE_FUNCTION
#define COGL_WINSYS_FEATURE_FUNCTION(ret, name, args)
#undef COGL_WINSYS_FEATURE_END
#define COGL_WINSYS_FEATURE_END()

static const CoglFeatureData winsys_feature_data[] =
  {
#include "cogl-winsys-egl-feature-functions.h"
  };

static CoglFuncPtr
_cogl_winsys_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name)
{
  void *ptr;

  ptr = eglGetProcAddress (name);

  /* eglGetProcAddress doesn't support fetching core API so we need to
     get that separately with GModule */
  if (ptr == NULL)
    g_module_symbol (renderer->libgl_module, name, &ptr);

  return ptr;
}

#ifdef COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT
static ANativeWindow *android_native_window;

void
cogl_android_set_native_window (ANativeWindow *window)
{
  _cogl_init ();

  android_native_window = window;
}
#endif

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  if (renderer->winsys_vtable->id == COGL_WINSYS_ID_EGL_GDL &&
      egl_renderer->gdl_initialized)
    gdl_close ();
#endif

  eglTerminate (egl_renderer->edpy);

  g_slice_free (CoglRendererEGL, egl_renderer);
}

/* Updates all the function pointers */
static void
check_egl_extensions (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;
  const char *egl_extensions;
  int i;

  egl_extensions = eglQueryString (egl_renderer->edpy, EGL_EXTENSIONS);

  COGL_NOTE (WINSYS, "  EGL Extensions: %s", egl_extensions);

  egl_renderer->private_features = 0;
  for (i = 0; i < G_N_ELEMENTS (winsys_feature_data); i++)
    if (_cogl_feature_check (renderer,
                             "EGL", winsys_feature_data + i, 0, 0,
                             COGL_DRIVER_GL, /* the driver isn't used */
                             egl_extensions,
                             egl_renderer))
      {
        egl_renderer->private_features |=
          winsys_feature_data[i].feature_flags_private;
      }
}

gboolean
_cogl_winsys_egl_renderer_connect_common (CoglRenderer *renderer,
                                          GError **error)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;

  if (!eglInitialize (egl_renderer->edpy,
                      &egl_renderer->egl_version_major,
                      &egl_renderer->egl_version_minor))
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Couldn't initialize EGL");
      return FALSE;
    }

  check_egl_extensions (renderer);

  return TRUE;
}

static gboolean
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  CoglRendererEGL *egl_renderer;
#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  gdl_ret_t rc = GDL_SUCCESS;
  gdl_display_info_t gdl_display_info;
#endif

  renderer->winsys = g_slice_new0 (CoglRendererEGL);

  egl_renderer = renderer->winsys;

  switch (renderer->winsys_vtable->id)
    {
    default:
      g_warn_if_reached ();
      goto error;

    case COGL_WINSYS_ID_EGL_GDL:
    case COGL_WINSYS_ID_EGL_ANDROID:
    case COGL_WINSYS_ID_EGL_NULL:
      egl_renderer->edpy = eglGetDisplay (EGL_DEFAULT_DISPLAY);
      break;
    }

  if (!_cogl_winsys_egl_renderer_connect_common (renderer, error))
    goto error;

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  if (renderer->winsys_vtable->id == COGL_WINSYS_ID_EGL_GDL)
    {
      /* Check we can talk to the GDL library */

      rc = gdl_init (NULL);
      if (rc != GDL_SUCCESS)
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "GDL initialize failed. %s",
                       gdl_get_error_string (rc));
          goto error;
        }

      rc = gdl_get_display_info (GDL_DISPLAY_ID_0, &gdl_display_info);
      if (rc != GDL_SUCCESS)
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "GDL failed to get display information: %s",
                       gdl_get_error_string (rc));
          gdl_close ();
          goto error;
        }

      gdl_close ();
    }
#endif

  return TRUE;

error:
  _cogl_winsys_renderer_disconnect (renderer);
  return FALSE;
}

static void
egl_attributes_from_framebuffer_config (CoglDisplay *display,
                                        CoglFramebufferConfig *config,
                                        gboolean needs_stencil_override,
                                        EGLint *attributes)
{
  CoglRenderer *renderer = display->renderer;
  int i = 0;

  attributes[i++] = EGL_STENCIL_SIZE;
  attributes[i++] = needs_stencil_override ? 2 : 0;

  attributes[i++] = EGL_RED_SIZE;
  attributes[i++] = 1;
  attributes[i++] = EGL_GREEN_SIZE;
  attributes[i++] = 1;
  attributes[i++] = EGL_BLUE_SIZE;
  attributes[i++] = 1;

  attributes[i++] = EGL_ALPHA_SIZE;
  attributes[i++] = config->swap_chain->has_alpha ? 1 : EGL_DONT_CARE;

  attributes[i++] = EGL_DEPTH_SIZE;
  attributes[i++] = 1;

  /* XXX: Why does the GDL platform choose these by default? */
#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  if (renderer->winsys_vtable->id == COGL_WINSYS_ID_EGL_GDL)
    {
      attributes[i++] = EGL_BIND_TO_TEXTURE_RGBA;
      attributes[i++] = EGL_TRUE;
      attributes[i++] = EGL_BIND_TO_TEXTURE_RGB;
      attributes[i++] = EGL_TRUE;
    }
#endif

  attributes[i++] = EGL_BUFFER_SIZE;
  attributes[i++] = EGL_DONT_CARE;

  attributes[i++] = EGL_RENDERABLE_TYPE;
  attributes[i++] = (renderer->driver == COGL_DRIVER_GL ?
                      EGL_OPENGL_BIT :
                      renderer->driver == COGL_DRIVER_GLES1 ?
                      EGL_OPENGL_ES_BIT :
                      EGL_OPENGL_ES2_BIT);

  attributes[i++] = EGL_SURFACE_TYPE;
  attributes[i++] = EGL_WINDOW_BIT;

  if (config->samples_per_pixel)
    {
       attributes[i++] = EGL_SAMPLE_BUFFERS;
       attributes[i++] = 1;
       attributes[i++] = EGL_SAMPLES;
       attributes[i++] = config->samples_per_pixel;
    }

  attributes[i++] = EGL_NONE;

  g_assert (i < MAX_EGL_CONFIG_ATTRIBS);
}

static gboolean
try_create_context (CoglDisplay *display,
                    gboolean with_stencil_buffer,
                    GError **error)
{
  CoglRenderer *renderer = display->renderer;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  EGLDisplay edpy;
  EGLConfig config;
  EGLint config_count = 0;
  EGLBoolean status;
  EGLint attribs[3];
  EGLint cfg_attribs[MAX_EGL_CONFIG_ATTRIBS];
  const char *error_message;

  _COGL_RETURN_VAL_IF_FAIL (egl_display->egl_context == NULL, TRUE);

  if (renderer->driver == COGL_DRIVER_GL)
    eglBindAPI (EGL_OPENGL_API);

  if (display->renderer->driver == COGL_DRIVER_GLES2)
    {
      attribs[0] = EGL_CONTEXT_CLIENT_VERSION;
      attribs[1] = 2;
      attribs[2] = EGL_NONE;
    }
  else
    attribs[0] = EGL_NONE;

  /* Divert to the platform implementation if one is defined */
  if (egl_renderer->platform_vtable &&
      egl_renderer->platform_vtable->try_create_context)
    return egl_renderer->platform_vtable->
      try_create_context (display, attribs, error);

  egl_attributes_from_framebuffer_config (display,
                                          &display->onscreen_template->config,
                                          with_stencil_buffer,
                                          cfg_attribs);

  edpy = egl_renderer->edpy;

  status = eglChooseConfig (edpy,
                            cfg_attribs,
                            &config, 1,
                            &config_count);
  if (status != EGL_TRUE || config_count == 0)
    {
      error_message = "Unable to find a usable EGL configuration";
      goto fail;
    }

  egl_display->egl_config = config;

  egl_display->egl_context = eglCreateContext (edpy,
                                               config,
                                               EGL_NO_CONTEXT,
                                               attribs);
  if (egl_display->egl_context == EGL_NO_CONTEXT)
    {
      error_message = "Unable to create a suitable EGL context";
      goto fail;
    }

  switch (renderer->winsys_vtable->id)
    {
    default:
      if (egl_renderer->platform_vtable &&
          egl_renderer->platform_vtable->context_created &&
          !egl_renderer->platform_vtable->context_created (display, error))
        return FALSE;
      break;

#ifdef COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT
    case COGL_WINSYS_ID_EGL_ANDROID:
      {
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
        eglGetConfigAttrib (edpy, config, EGL_NATIVE_VISUAL_ID, &format);

        ANativeWindow_setBuffersGeometry (android_native_window,
                                          0,
                                          0,
                                          format);

        egl_display->egl_surface =
          eglCreateWindowSurface (edpy,
                                  config,
                                  (NativeWindowType) android_native_window,
                                  NULL);
        if (egl_display->egl_surface == EGL_NO_SURFACE)
          {
            error_message = "Unable to create EGL window surface";
            goto fail;
          }

        if (!eglMakeCurrent (egl_renderer->edpy,
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
                         &egl_display->egl_surface_width);

        eglQuerySurface (egl_renderer->edpy,
                         egl_display->egl_surface,
                         EGL_HEIGHT,
                         &egl_display->egl_surface_height);
      }
      break;
#endif

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
    case COGL_WINSYS_ID_EGL_GDL:
      egl_display->egl_surface =
        eglCreateWindowSurface (edpy,
                                config,
                                (NativeWindowType) display->gdl_plane,
                                NULL);

      if (egl_display->egl_surface == EGL_NO_SURFACE)
        {
          error_message = "Unable to create EGL window surface";
          goto fail;
        }

      if (!eglMakeCurrent (egl_renderer->edpy,
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
                       &egl_display->egl_surface_width);

      eglQuerySurface (egl_renderer->edpy,
                       egl_display->egl_surface,
                       EGL_HEIGHT,
                       &egl_display->egl_surface_height);
      break;
#endif

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT
    case COGL_WINSYS_ID_EGL_NULL:
      egl_display->egl_surface =
        eglCreateWindowSurface (edpy,
                                config,
                                (NativeWindowType) NULL,
                                NULL);
      if (egl_display->egl_surface == EGL_NO_SURFACE)
        {
          error_message = "Unable to create EGL window surface";
          goto fail;
        }

      if (!eglMakeCurrent (egl_renderer->edpy,
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
                       &egl_display->egl_surface_width);

      eglQuerySurface (egl_renderer->edpy,
                       egl_display->egl_surface,
                       EGL_HEIGHT,
                       &egl_display->egl_surface_height);
      break;
#endif
    }

  return TRUE;

fail:
  g_set_error (error, COGL_WINSYS_ERROR,
               COGL_WINSYS_ERROR_CREATE_CONTEXT,
               "%s", error_message);
  return FALSE;
}

static void
cleanup_context (CoglDisplay *display)
{
  CoglRenderer *renderer = display->renderer;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglRendererEGL *egl_renderer = renderer->winsys;

  if (egl_display->egl_context != EGL_NO_CONTEXT)
    {
      eglMakeCurrent (egl_renderer->edpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
                      EGL_NO_CONTEXT);
      eglDestroyContext (egl_renderer->edpy, egl_display->egl_context);
      egl_display->egl_context = EGL_NO_CONTEXT;
    }

  if (egl_renderer->platform_vtable &&
      egl_renderer->platform_vtable->cleanup_context)
    egl_renderer->platform_vtable->cleanup_context (display);

  switch (renderer->winsys_vtable->id)
    {
    default:
      break;

#if defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT) ||    \
    defined (COGL_HAS_EGL_PLATFORM_GDL_SUPPORT)
    case COGL_WINSYS_ID_EGL_NULL:
    case COGL_WINSYS_ID_EGL_GDL:
      if (egl_display->egl_surface != EGL_NO_SURFACE)
        {
          eglDestroySurface (egl_renderer->edpy, egl_display->egl_surface);
          egl_display->egl_surface = EGL_NO_SURFACE;
        }
      break;
#endif
    }
}

static gboolean
create_context (CoglDisplay *display, GError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;

  /* Note: we don't just rely on eglChooseConfig to correctly
   * report that the driver doesn't support a stencil buffer
   * because we've seen PVR drivers that claim stencil buffer
   * support according to the EGLConfig but then later fail
   * when trying to create a context with such a config.
   */
  if (try_create_context (display, TRUE, error))
    {
      egl_display->stencil_disabled = FALSE;
      return TRUE;
    }
  else
    {
      g_clear_error (error);
      cleanup_context (display);
      egl_display->stencil_disabled = TRUE;
      return try_create_context (display, FALSE, error);
    }
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  CoglDisplayEGL *egl_display = display->winsys;

  _COGL_RETURN_IF_FAIL (egl_display != NULL);

  cleanup_context (display);

  if (egl_renderer->platform_vtable &&
      egl_renderer->platform_vtable->display_destroy)
    egl_renderer->platform_vtable->display_destroy (display);

  g_slice_free (CoglDisplayEGL, display->winsys);
  display->winsys = NULL;
}

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
static gboolean
gdl_plane_init (CoglDisplay *display, GError **error)
{
  gboolean ret = TRUE;
  gdl_color_space_t colorSpace = GDL_COLOR_SPACE_RGB;
  gdl_pixel_format_t pixfmt = GDL_PF_ARGB_32;
  gdl_rectangle_t dstRect;
  gdl_display_info_t display_info;
  gdl_ret_t rc = GDL_SUCCESS;

  if (!display->gdl_plane)
    {
      g_set_error (error, COGL_WINSYS_ERROR, COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "No GDL plane specified with "
                   "cogl_gdl_display_set_plane");
      return FALSE;
    }

  rc = gdl_init (NULL);
  if (rc != GDL_SUCCESS)
    {
      g_set_error (error, COGL_WINSYS_ERROR, COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "GDL initialize failed. %s", gdl_get_error_string (rc));
      return FALSE;
    }

  rc = gdl_get_display_info (GDL_DISPLAY_ID_0, &display_info);
  if (rc != GDL_SUCCESS)
    {
      g_set_error (error, COGL_WINSYS_ERROR, COGL_WINSYS_ERROR_CREATE_CONTEXT,
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
      if (display->onscreen_template->swap_chain &&
          display->onscreen_template->swap_chain->length != -1)
        rc = gdl_plane_set_uint (GDL_PLANE_NUM_GFX_SURFACES,
                                 display->onscreen_template->swap_chain->length);
      else
        rc = gdl_plane_set_uint (GDL_PLANE_NUM_GFX_SURFACES, 3);
    }

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_config_end (GDL_FALSE);
  else
    gdl_plane_config_end (GDL_TRUE);

  if (rc != GDL_SUCCESS)
    {
      g_set_error (error, COGL_WINSYS_ERROR, COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "GDL configuration failed: %s.", gdl_get_error_string (rc));
      ret = FALSE;
    }

  gdl_close ();

  return TRUE;
}
#endif

static gboolean
_cogl_winsys_display_setup (CoglDisplay *display,
                            GError **error)
{
  CoglDisplayEGL *egl_display;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;

  _COGL_RETURN_VAL_IF_FAIL (display->winsys == NULL, FALSE);

  egl_display = g_slice_new0 (CoglDisplayEGL);
  display->winsys = egl_display;

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  if (renderer->winsys_vtable->id == COGL_WINSYS_ID_EGL_GDL &&
      !gdl_plane_init (display, error))
    goto error;
#endif

#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
  if (display->wayland_compositor_display)
    {
      struct wl_display *wayland_display = display->wayland_compositor_display;
      CoglRendererEGL *egl_renderer = display->renderer->winsys;

      egl_renderer->pf_eglBindWaylandDisplay (egl_renderer->edpy,
                                              wayland_display);
    }
#endif

  if (egl_renderer->platform_vtable &&
      egl_renderer->platform_vtable->display_setup &&
      !egl_renderer->platform_vtable->display_setup (display, error))
    goto error;

  if (!create_context (display, error))
    goto error;

  egl_display->found_egl_config = TRUE;

  return TRUE;

error:
  _cogl_winsys_display_destroy (display);
  return FALSE;
}

static gboolean
_cogl_winsys_context_init (CoglContext *context, GError **error)
{
  CoglRenderer *renderer = context->display->renderer;
  CoglDisplayEGL *egl_display = context->display->winsys;
  CoglRendererEGL *egl_renderer = renderer->winsys;

  context->winsys = g_new0 (CoglContextEGL, 1);

  _COGL_RETURN_VAL_IF_FAIL (egl_display->egl_context, FALSE);

  memset (context->winsys_features, 0, sizeof (context->winsys_features));

  check_egl_extensions (renderer);

  if (!_cogl_context_update_features (context, error))
    return FALSE;

  if (egl_renderer->private_features & COGL_EGL_WINSYS_FEATURE_SWAP_REGION)
    {
      COGL_FLAGS_SET (context->winsys_features,
                      COGL_WINSYS_FEATURE_SWAP_REGION, TRUE);
      COGL_FLAGS_SET (context->winsys_features,
                      COGL_WINSYS_FEATURE_SWAP_REGION_THROTTLE, TRUE);
    }

  if (egl_renderer->platform_vtable &&
      egl_renderer->platform_vtable->context_init &&
      !egl_renderer->platform_vtable->context_init (context, error))
    return FALSE;

  return TRUE;
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;

  if (egl_renderer->platform_vtable &&
      egl_renderer->platform_vtable->context_deinit)
    egl_renderer->platform_vtable->context_deinit (context);

  g_free (context->winsys);
}

static gboolean
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            GError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglOnscreenEGL *egl_onscreen;
  EGLint attributes[MAX_EGL_CONFIG_ATTRIBS];
  EGLConfig egl_config;
  EGLint config_count = 0;
  EGLBoolean status;
  gboolean need_stencil =
    egl_display->stencil_disabled ? FALSE : framebuffer->config.need_stencil;

  _COGL_RETURN_VAL_IF_FAIL (egl_display->egl_context, FALSE);

  egl_attributes_from_framebuffer_config (display,
                                          &framebuffer->config,
                                          need_stencil,
                                          attributes);

  status = eglChooseConfig (egl_renderer->edpy,
                            attributes,
                            &egl_config, 1,
                            &config_count);
  if (status != EGL_TRUE || config_count == 0)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to find a suitable EGL configuration");
      return FALSE;
    }

  /* Update the real number of samples_per_pixel now that we have
   * found an egl_config... */
  if (framebuffer->config.samples_per_pixel)
    {
      EGLint samples;
      status = eglGetConfigAttrib (egl_renderer->edpy,
                                   egl_config,
                                   EGL_SAMPLES, &samples);
      g_return_val_if_fail (status == EGL_TRUE, TRUE);
      framebuffer->samples_per_pixel = samples;
    }

  onscreen->winsys = g_slice_new0 (CoglOnscreenEGL);
  egl_onscreen = onscreen->winsys;

  switch (renderer->winsys_vtable->id)
    {
    default:
      if (egl_renderer->platform_vtable &&
          egl_renderer->platform_vtable->onscreen_init &&
          !egl_renderer->platform_vtable->onscreen_init (onscreen,
                                                         egl_config,
                                                         error))
        {
          g_slice_free (CoglOnscreenEGL, onscreen->winsys);
          return FALSE;
        }
      break;

#if defined (COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT) || \
    defined (COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT)      || \
    defined (COGL_HAS_EGL_PLATFORM_GDL_SUPPORT)
    case COGL_WINSYS_ID_EGL_NULL:
    case COGL_WINSYS_ID_EGL_ANDROID:
    case COGL_WINSYS_ID_EGL_GDL:

      if (egl_display->have_onscreen)
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "EGL platform only supports a single onscreen window");
          return FALSE;
        }

      egl_onscreen->egl_surface = egl_display->egl_surface;

      _cogl_framebuffer_winsys_update_size (framebuffer,
                                            egl_display->egl_surface_width,
                                            egl_display->egl_surface_height);
      egl_display->have_onscreen = TRUE;
      break;
#endif
    }

  return TRUE;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT
  CoglDisplayEGL *egl_display = context->display->winsys;
#endif
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;

  /* If we never successfully allocated then there's nothing to do */
  if (egl_onscreen == NULL)
    return;
  if (egl_onscreen->egl_surface != EGL_NO_SURFACE)
    {
      if (eglDestroySurface (egl_renderer->edpy, egl_onscreen->egl_surface)
          == EGL_FALSE)
        g_warning ("Failed to destroy EGL surface");
      egl_onscreen->egl_surface = EGL_NO_SURFACE;
    }

  if (egl_renderer->platform_vtable &&
      egl_renderer->platform_vtable->onscreen_deinit)
    egl_renderer->platform_vtable->onscreen_deinit (onscreen);

#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT
  egl_display->have_onscreen = FALSE;
#endif

  g_slice_free (CoglOnscreenEGL, onscreen->winsys);
  onscreen->winsys = NULL;
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglDisplayEGL *egl_display = context->display->winsys;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglContextEGL *egl_context = context->winsys;

  if (egl_context->current_surface == egl_onscreen->egl_surface)
    return;

  eglMakeCurrent (egl_renderer->edpy,
                  egl_onscreen->egl_surface,
                  egl_onscreen->egl_surface,
                  egl_display->egl_context);
  egl_context->current_surface = egl_onscreen->egl_surface;

  if (onscreen->swap_throttled)
    eglSwapInterval (egl_renderer->edpy, 1);
  else
    eglSwapInterval (egl_renderer->edpy, 0);
}

static void
_cogl_winsys_onscreen_swap_region (CoglOnscreen *onscreen,
                                   const int *user_rectangles,
                                   int n_rectangles)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  int framebuffer_height  = cogl_framebuffer_get_height (framebuffer);
  int *rectangles = g_alloca (sizeof (int) * n_rectangles * 4);
  int i;

  /* eglSwapBuffersRegion expects rectangles relative to the
   * bottom left corner but we are given rectangles relative to
   * the top left so we need to flip them... */
  memcpy (rectangles, user_rectangles, sizeof (int) * n_rectangles * 4);
  for (i = 0; i < n_rectangles; i++)
    {
      int *rect = &rectangles[4 * i];
      rect[1] = framebuffer_height - rect[1] - rect[3];
    }

  /* At least for eglSwapBuffers the EGL spec says that the surface to
     swap must be bound to the current context. It looks like Mesa
     also validates that this is the case for eglSwapBuffersRegion so
     we must bind here too */
  _cogl_framebuffer_flush_state (COGL_FRAMEBUFFER (onscreen),
                                 COGL_FRAMEBUFFER (onscreen),
                                 COGL_FRAMEBUFFER_STATE_BIND);

  if (egl_renderer->pf_eglSwapBuffersRegion (egl_renderer->edpy,
                                             egl_onscreen->egl_surface,
                                             n_rectangles,
                                             rectangles) == EGL_FALSE)
    g_warning ("Error reported by eglSwapBuffersRegion");
}

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;

  /* The specification for EGL (at least in 1.4) says that the surface
     needs to be bound to the current context for the swap to work
     although it may change in future. Mesa explicitly checks for this
     and just returns an error if this is not the case so we can't
     just pretend this isn't in the spec. */
  _cogl_framebuffer_flush_state (COGL_FRAMEBUFFER (onscreen),
                                 COGL_FRAMEBUFFER (onscreen),
                                 COGL_FRAMEBUFFER_STATE_BIND);

  eglSwapBuffers (egl_renderer->edpy, egl_onscreen->egl_surface);
}

static void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextEGL *egl_context = context->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;

  if (egl_context->current_surface != egl_onscreen->egl_surface)
    return;

  egl_context->current_surface = EGL_NO_SURFACE;

  _cogl_winsys_onscreen_bind (onscreen);
}

static EGLDisplay
_cogl_winsys_context_egl_get_egl_display (CoglContext *context)
{
  CoglRendererEGL *egl_renderer = context->display->renderer->winsys;

  return egl_renderer->edpy;
}

static CoglWinsysVtable _cogl_winsys_vtable =
  {
    /* This winsys is only used as a base for the EGL-platform
       winsys's so it does not have an ID or a name */

    .renderer_get_proc_address = _cogl_winsys_renderer_get_proc_address,
    .renderer_connect = _cogl_winsys_renderer_connect,
    .renderer_disconnect = _cogl_winsys_renderer_disconnect,
    .display_setup = _cogl_winsys_display_setup,
    .display_destroy = _cogl_winsys_display_destroy,
    .context_init = _cogl_winsys_context_init,
    .context_deinit = _cogl_winsys_context_deinit,
    .context_egl_get_egl_display =
      _cogl_winsys_context_egl_get_egl_display,
    .onscreen_init = _cogl_winsys_onscreen_init,
    .onscreen_deinit = _cogl_winsys_onscreen_deinit,
    .onscreen_bind = _cogl_winsys_onscreen_bind,
    .onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers,
    .onscreen_swap_region = _cogl_winsys_onscreen_swap_region,
    .onscreen_update_swap_throttled =
      _cogl_winsys_onscreen_update_swap_throttled,
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
_cogl_winsys_egl_get_vtable (void)
{
  return &_cogl_winsys_vtable;
}

#ifdef EGL_KHR_image_base
EGLImageKHR
_cogl_egl_create_image (CoglContext *ctx,
                        EGLenum target,
                        EGLClientBuffer buffer,
                        const EGLint *attribs)
{
  CoglDisplayEGL *egl_display = ctx->display->winsys;
  CoglRendererEGL *egl_renderer = ctx->display->renderer->winsys;
  EGLContext egl_ctx;

  _COGL_RETURN_VAL_IF_FAIL (egl_renderer->pf_eglCreateImage, EGL_NO_IMAGE_KHR);

  /* The EGL_KHR_image_pixmap spec explicitly states that EGL_NO_CONTEXT must
   * always be used in conjunction with the EGL_NATIVE_PIXMAP_KHR target */
#ifdef EGL_KHR_image_pixmap
  if (target == EGL_NATIVE_PIXMAP_KHR)
    egl_ctx = EGL_NO_CONTEXT;
  else
#endif
    egl_ctx = egl_display->egl_context;

  return egl_renderer->pf_eglCreateImage (egl_renderer->edpy,
                                          egl_ctx,
                                          target,
                                          buffer,
                                          attribs);
}

void
_cogl_egl_destroy_image (CoglContext *ctx,
                         EGLImageKHR image)
{
  CoglRendererEGL *egl_renderer = ctx->display->renderer->winsys;

  _COGL_RETURN_IF_FAIL (egl_renderer->pf_eglDestroyImage);

  egl_renderer->pf_eglDestroyImage (egl_renderer->edpy, image);
}
#endif
