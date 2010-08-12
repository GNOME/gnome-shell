/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Tao Zhao <tao.zhao@intel.com>
 *   Damien Lespiau <damien.lespiau@intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "clutter-debug.h"
#include "clutter-main.h"

#include "clutter-backend-cex100.h"

static gdl_plane_id_t gdl_plane = GDL_PLANE_ID_UPP_C;

G_DEFINE_TYPE (ClutterBackendCex100,
               clutter_backend_cex100,
               CLUTTER_TYPE_BACKEND_EGL)

static gboolean
gdl_plane_init (gdl_display_id_t   dpy,
                gdl_plane_id_t     plane,
                gdl_pixel_format_t pixfmt)
{
  gboolean ret = TRUE;

  gdl_color_space_t   colorSpace = GDL_COLOR_SPACE_RGB;
  gdl_rectangle_t     dstRect;
  gdl_display_info_t  display_info;
  gdl_ret_t           rc = GDL_SUCCESS;

  if (GDL_DISPLAY_ID_0 != dpy && GDL_DISPLAY_ID_1 != dpy)
    {
      g_warning ("Invalid display ID, must be GDL_DISPLAY_ID_0 or "
                 "GDL_DISPLAY_ID_1.");
      return FALSE;
    }

  /* Init GDL library */
  rc = gdl_init (NULL);
  if (rc != GDL_SUCCESS)
    {
      g_warning ("GDL initialize failed. %s", gdl_get_error_string (rc));
      return FALSE;
    }

  rc = gdl_get_display_info (dpy, &display_info);
  if (rc != GDL_SUCCESS)
    {
      g_warning ("GDL failed to get display infomation: %s",
                 gdl_get_error_string (rc));
      gdl_close ();
      return FALSE;
    }

  dstRect.origin.x = 0;
  dstRect.origin.y = 0;
  dstRect.width = display_info.tvmode.width;
  dstRect.height = display_info.tvmode.height;

  /* Configure the plane attribute. */
  rc = gdl_plane_reset (plane);
  if (rc == GDL_SUCCESS)
    rc = gdl_plane_config_begin (plane);

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_set_attr (GDL_PLANE_SRC_COLOR_SPACE, &colorSpace);

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_set_attr (GDL_PLANE_PIXEL_FORMAT, &pixfmt);

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_set_attr (GDL_PLANE_DST_RECT, &dstRect);

#if 0
  /*
   * Change the number of back buffers for the eglWindowSurface, Default
   * value is 3, could be changed to 2, means one front buffer and one back
   * buffer.
   *
   * TODO: Make a new API to tune that;
   */
  if (rc == GDL_SUCCESS)
    rc = gdl_plane_set_uint (GDL_PLANE_NUM_GFX_SURFACES, 2);
#endif

  if (rc == GDL_SUCCESS)
    rc = gdl_plane_config_end (GDL_FALSE);
  else
    gdl_plane_config_end (GDL_TRUE);

  if (rc != GDL_SUCCESS)
    {
      g_warning ("GDL configuration failed: %s.", gdl_get_error_string (rc));
      ret = FALSE;
    }

  gdl_close ();

  return ret;
}

/*
 * ClutterBackendEGL implementation
 */

static gboolean
clutter_backend_cex100_create_context (ClutterBackend  *backend,
				       GError         **error)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);
  EGLConfig configs[2];
  EGLint config_count;
  EGLBoolean status;

  if (backend_egl->egl_context != EGL_NO_CONTEXT)
    return TRUE;

  /* Start by initializing the GDL plane */
  if (!gdl_plane_init (GDL_DISPLAY_ID_0, gdl_plane, GDL_PF_ARGB_32))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Could not initialize the GDL plane");
      return FALSE;
    }

  NativeWindowType window = (NativeWindowType) gdl_plane;
  EGLint cfg_attribs[] = {
      EGL_BUFFER_SIZE,     EGL_DONT_CARE,
      EGL_RED_SIZE,        8,
      EGL_GREEN_SIZE,      8,
      EGL_BLUE_SIZE,       8,
      EGL_DEPTH_SIZE,      16,
      EGL_ALPHA_SIZE,      8,
      EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE,
      EGL_BIND_TO_TEXTURE_RGB, EGL_TRUE,
#ifdef HAVE_COGL_GLES2
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#else /* HAVE_COGL_GLES2 */
      EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
#endif /* HAVE_COGL_GLES2 */
      EGL_NONE
  };

  status = eglGetConfigs (backend_egl->edpy,
                          configs,
                          2,
                          &config_count);

  if (status != EGL_TRUE)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "No EGL configurations found");
      return FALSE;
    }

  status = eglChooseConfig (backend_egl->edpy,
                            cfg_attribs,
                            configs,
                            G_N_ELEMENTS (configs),
                            &config_count);

  if (status != EGL_TRUE)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Unable to select a valid EGL configuration");
      return FALSE;
    }

  CLUTTER_NOTE (BACKEND, "Got %i configs", config_count);

  if (status != EGL_TRUE)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Unable to Make Current Context for NULL");
      return FALSE;
    }

  if (G_UNLIKELY (backend_egl->egl_surface != EGL_NO_SURFACE))
    {
      eglDestroySurface (backend_egl->edpy, backend_egl->egl_surface);
      backend_egl->egl_surface = EGL_NO_SURFACE;
    }

  if (G_UNLIKELY (backend_egl->egl_context != NULL))
    {
      eglDestroyContext (backend_egl->edpy, backend_egl->egl_context);
      backend_egl->egl_context = NULL;
    }

  backend_egl->egl_surface = eglCreateWindowSurface (backend_egl->edpy,
                                                     configs[0],
                                                     window,
                                                     NULL);

  if (backend_egl->egl_surface == EGL_NO_SURFACE)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Unable to create EGL window surface");

      return FALSE;
    }

#ifdef HAVE_COGL_GLES2
    {
      static const EGLint attribs[3] = {
          EGL_CONTEXT_CLIENT_VERSION, 2,
          EGL_NONE
      };

      backend_egl->egl_context = eglCreateContext (backend_egl->edpy,
                                                   configs[0],
                                                   EGL_NO_CONTEXT,
                                                   attribs);
    }
#else
  /* Seems some GLES implementations 1.x do not like attribs... */
  backend_egl->egl_context = eglCreateContext (backend_egl->edpy,
                                               configs[0],
                                               EGL_NO_CONTEXT,
                                               NULL);
#endif

  if (backend_egl->egl_context == EGL_NO_CONTEXT)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Unable to create a suitable EGL context");
      return FALSE;
    }

  CLUTTER_NOTE (GL, "Created EGL Context");

  CLUTTER_NOTE (BACKEND, "Setting context");

  /*
   * eglnative can have only one stage, so we store the EGL surface in the
   * backend itself, instead of the StageWindow implementation, and we make it
   * current immediately to make sure the Cogl and Clutter can query the EGL
   * context for features.
   */
  status = eglMakeCurrent (backend_egl->edpy,
                           backend_egl->egl_surface,
                           backend_egl->egl_surface,
                           backend_egl->egl_context);

  eglQuerySurface (backend_egl->edpy,
                   backend_egl->egl_surface,
                   EGL_WIDTH,
                   &backend_egl->surface_width);

  eglQuerySurface (backend_egl->edpy,
                   backend_egl->egl_surface,
                   EGL_HEIGHT,
                   &backend_egl->surface_height);

  CLUTTER_NOTE (BACKEND, "EGL surface is %ix%i",
                backend_egl->surface_width,
                backend_egl->surface_height);

  /*
   * For EGL backend, it needs to clear all the back buffers of the window
   * surface before drawing anything, otherwise the image will be blinking
   * heavily.  The default eglWindowSurface has 3 gdl surfaces as the back
   * buffer, that's why glClear should be called 3 times.
   */
  glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
  glClear (GL_COLOR_BUFFER_BIT);
  eglSwapBuffers (backend_egl->edpy, backend_egl->egl_surface);

  glClear (GL_COLOR_BUFFER_BIT);
  eglSwapBuffers (backend_egl->edpy, backend_egl->egl_surface);

  glClear (GL_COLOR_BUFFER_BIT);
  eglSwapBuffers (backend_egl->edpy, backend_egl->egl_surface);

  return TRUE;
}

/*
 * GObject implementation
 */

static void
clutter_backend_cex100_class_init (ClutterBackendCex100Class *klass)
{
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  backend_class->create_context   = clutter_backend_cex100_create_context;
}

static void
clutter_backend_cex100_init (ClutterBackendCex100 *self)
{
}

/* every backend must implement this function */
GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_cex100_get_type ();
}
