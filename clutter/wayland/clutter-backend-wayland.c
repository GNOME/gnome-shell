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

 * Authors:
 *  Matthew Allum
 *  Robert Bragg
 *  Kristian Høgsberg
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>

#include <wayland-util.h>
#include <wayland-client.h>
#include <xf86drm.h>

#include "clutter-backend-wayland.h"
#include "clutter-stage-wayland.h"
#include "clutter-wayland.h"

#include "clutter-debug.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

static ClutterBackendWayland *backend_singleton = NULL;

G_DEFINE_TYPE (ClutterBackendWayland, _clutter_backend_wayland, CLUTTER_TYPE_BACKEND);

static void
clutter_backend_at_exit (void)
{
  if (backend_singleton)
    g_object_run_dispose (G_OBJECT (backend_singleton));
}

static gboolean
clutter_backend_wayland_pre_parse (ClutterBackend  *backend,
				   GError         **error)
{
  return TRUE;
}

static void
drm_handle_device (void *data, struct wl_drm *drm, const char *device)
{
  ClutterBackendWayland *backend_wayland = data;
  backend_wayland->device_name = g_strdup (device);
}

static void
drm_handle_authenticated (void *data, struct wl_drm *drm)
{
  ClutterBackendWayland *backend_wayland = data;
  backend_wayland->authenticated = 1;
}

static const struct wl_drm_listener drm_listener =
{
  drm_handle_device,
  drm_handle_authenticated
};

static void
display_handle_geometry (void *data,
			 struct wl_output *output,
			 int32_t x, int32_t y,
			 int32_t width, int32_t height)
{
  ClutterBackendWayland *backend_wayland = data;

  backend_wayland->screen_allocation.x = x;
  backend_wayland->screen_allocation.y = y;
  backend_wayland->screen_allocation.width = width;
  backend_wayland->screen_allocation.height = height;
}

static const struct wl_output_listener output_listener =
{
  display_handle_geometry,
};


static void
handle_configure (void *data, struct wl_shell *shell,
		  uint32_t timestamp, uint32_t edges,
		  struct wl_surface *surface,
		  int32_t width, int32_t height)
{
  ClutterStageWayland *stage_wayland;

  stage_wayland = wl_surface_get_user_data (surface);

  if ((stage_wayland->allocation.width != width) ||
      (stage_wayland->allocation.height != height))
    {
      clutter_actor_queue_relayout (CLUTTER_ACTOR (stage_wayland->wrapper));
    }

  stage_wayland->pending_allocation.width = width;
  stage_wayland->pending_allocation.height = height;
  stage_wayland->allocation = stage_wayland->pending_allocation;

  clutter_actor_set_size (CLUTTER_ACTOR (stage_wayland->wrapper),
			  width, height);

  /* the resize process is complete, so we can ask the stage
   * to set up the GL viewport with the new size
   */
  clutter_stage_ensure_viewport (stage_wayland->wrapper);
}

static const struct wl_shell_listener shell_listener = {
	handle_configure,
};

static void
display_handle_global (struct wl_display *display,
                       uint32_t id,
		       const char *interface,
                       uint32_t version,
                       void *data)
{
  ClutterBackendWayland *backend_wayland = data;

  if (strcmp (interface, "compositor") == 0)
    {
      backend_wayland->wayland_compositor = wl_compositor_create (display, id);
    }
  else if (strcmp (interface, "output") == 0)
    {
      backend_wayland->wayland_output = wl_output_create (display, id);
      wl_output_add_listener (backend_wayland->wayland_output,
                              &output_listener, backend_wayland);
    }
  else if (strcmp (interface, "input_device") == 0)
    {
      _clutter_backend_add_input_device (backend_wayland, id);
    }
  else if (strcmp (interface, "shell") == 0)
    {
      backend_wayland->wayland_shell = wl_shell_create (display, id);
      wl_shell_add_listener (backend_wayland->wayland_shell,
                             &shell_listener, backend_wayland);
    }
  else if (strcmp (interface, "drm") == 0)
    {
      backend_wayland->wayland_drm = wl_drm_create (display, id);
      wl_drm_add_listener (backend_wayland->wayland_drm,
                           &drm_listener, backend_wayland);
    }
  else if (strcmp (interface, "shm") == 0)
    {
      backend_wayland->wayland_shm = wl_shm_create (display, id);
    }
}

static gboolean
try_get_display (ClutterBackendWayland *backend_wayland, GError **error)
{
  EGLDisplay edpy = EGL_NO_DISPLAY;
  int drm_fd;

  drm_fd = open (backend_wayland->device_name, O_RDWR);

  backend_wayland->get_drm_display =
    (PFNEGLGETDRMDISPLAYMESA) eglGetProcAddress ("eglGetDRMDisplayMESA");

  if (backend_wayland->get_drm_display != NULL && drm_fd >= 0)
     edpy = backend_wayland->get_drm_display (drm_fd);

  if (edpy == EGL_NO_DISPLAY)
      edpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  if (edpy == EGL_NO_DISPLAY)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
		   CLUTTER_INIT_ERROR_BACKEND,
		   "Failed to open EGLDisplay");
      return FALSE;
    }

  backend_wayland->edpy   = edpy;
  backend_wayland->drm_fd = drm_fd;

  return TRUE;
}

static gboolean
try_enable_drm (ClutterBackendWayland *backend_wayland, GError **error)
{
  drm_magic_t magic;
  const gchar *exts, *glexts;

  if (backend_wayland->drm_fd < 0)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
		   CLUTTER_INIT_ERROR_BACKEND,
		   "Failed to open drm device");
      return FALSE;
    }

  glexts = glGetString(GL_EXTENSIONS);
  exts = eglQueryString (backend_wayland->edpy, EGL_EXTENSIONS);

  if (!cogl_clutter_check_extension ("EGL_KHR_image_base", exts) ||
      !cogl_clutter_check_extension ("EGL_MESA_drm_image", exts) ||
      !cogl_clutter_check_extension ("GL_OES_EGL_image", glexts))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
		   CLUTTER_INIT_ERROR_BACKEND,
		   "Missing EGL extensions");
      return FALSE;
    }

  backend_wayland->create_drm_image =
    (PFNEGLCREATEDRMIMAGEMESA) eglGetProcAddress ("eglCreateDRMImageMESA");
  backend_wayland->destroy_image =
    (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress ("eglDestroyImageKHR");
  backend_wayland->export_drm_image =
    (PFNEGLEXPORTDRMIMAGEMESA) eglGetProcAddress ("eglExportDRMImageMESA");
  backend_wayland->image_target_texture_2d =
    (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress ("glEGLImageTargetTexture2DOES");

  if (backend_wayland->create_drm_image == NULL ||
      backend_wayland->destroy_image == NULL ||
      backend_wayland->export_drm_image == NULL ||
      backend_wayland->image_target_texture_2d == NULL)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
		   CLUTTER_INIT_ERROR_BACKEND,
		   "Missing EGL extensions");
      return FALSE;
    }

  if (drmGetMagic (backend_wayland->drm_fd, &magic))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
		   CLUTTER_INIT_ERROR_BACKEND,
		   "Failed to get drm magic");
      return FALSE;
    }

  wl_drm_authenticate (backend_wayland->wayland_drm, magic);
  wl_display_iterate (backend_wayland->wayland_display, WL_DISPLAY_WRITABLE);
  while (!backend_wayland->authenticated)
    wl_display_iterate (backend_wayland->wayland_display, WL_DISPLAY_READABLE);

  return TRUE;
};

static gboolean
clutter_backend_wayland_post_parse (ClutterBackend  *backend,
				    GError         **error)
{
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  EGLBoolean status;

  g_atexit (clutter_backend_at_exit);

  /* TODO: expose environment variable/commandline option for this... */
  backend_wayland->wayland_display = wl_display_connect (NULL);
  if (!backend_wayland->wayland_display)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
		   CLUTTER_INIT_ERROR_BACKEND,
		   "Failed to open Wayland display socket");
      return FALSE;
    }

  backend_wayland->wayland_source =
    _clutter_event_source_wayland_new (backend_wayland->wayland_display);
  g_source_attach (backend_wayland->wayland_source, NULL);

  /* Set up listener so we'll catch all events. */
  wl_display_add_global_listener (backend_wayland->wayland_display,
                                  display_handle_global,
                                  backend_wayland);

  /* Process connection events. */
  wl_display_iterate (backend_wayland->wayland_display, WL_DISPLAY_READABLE);

  if (!try_get_display(backend_wayland, error))
    return FALSE;

  status = eglInitialize (backend_wayland->edpy,
			  &backend_wayland->egl_version_major,
			  &backend_wayland->egl_version_minor);
  if (status != EGL_TRUE)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
		   CLUTTER_INIT_ERROR_BACKEND,
		   "Unable to Initialize EGL");
      return FALSE;
    }

  CLUTTER_NOTE (BACKEND, "EGL Reports version %i.%i",
		backend_wayland->egl_version_major,
		backend_wayland->egl_version_minor);

  backend_wayland->drm_enabled = try_enable_drm(backend_wayland, error);

  if (!backend_wayland->drm_enabled) {
    if (backend_wayland->wayland_shm == NULL)
      return FALSE;

    g_debug("Could not enable DRM buffers, falling back to SHM buffers");
    g_clear_error(error);
  }

  return TRUE;
}

#if defined(HAVE_COGL_GL)
#define _COGL_RENDERABLE_BIT EGL_OPENGL_BIT
#elif defined(HAVE_COGL_GLES2)
#define _COGL_GLES_VERSION 2
#define _COGL_RENDERABLE_BIT EGL_OPENGL_ES2_BIT
#elif defined(HAVE_COGL_GLES)
#define _COGL_GLES_VERSION 1
#define _COGL_RENDERABLE_BIT EGL_OPENGL_ES_BIT
#endif

static gboolean
make_dummy_surface (ClutterBackendWayland *backend_wayland)
{
  static const EGLint attrs[] = {
    EGL_WIDTH, 1,
    EGL_HEIGHT, 1,
    EGL_RENDERABLE_TYPE, _COGL_RENDERABLE_BIT,
    EGL_NONE };
  EGLint num_configs;

  eglGetConfigs(backend_wayland->edpy,
                &backend_wayland->egl_config, 1, &num_configs);
  if (num_configs < 1)
    return FALSE;

  backend_wayland->egl_surface =
    eglCreatePbufferSurface(backend_wayland->edpy,
                            backend_wayland->egl_config,
                            attrs);

  if (backend_wayland->egl_surface == EGL_NO_SURFACE)
    return FALSE;

  return TRUE;
}

static gboolean
try_create_context (ClutterBackend  *backend,
                    int retry_cookie,
                    gboolean *try_fallback,
                    GError **error)
{
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  const char *error_message;

  if (backend_wayland->egl_context == EGL_NO_CONTEXT)
    {
#if defined(HAVE_COGL_GL)
      static const EGLint *attribs = NULL;
#else
      static const EGLint attribs[] =
        { EGL_CONTEXT_CLIENT_VERSION, _COGL_GLES_VERSION, EGL_NONE };
#endif

      backend_wayland->egl_context =
        eglCreateContext (backend_wayland->edpy,
                          backend_wayland->egl_config,
                          EGL_NO_CONTEXT,
                          attribs);
      if (backend_wayland->egl_context == EGL_NO_CONTEXT)
        {
          error_message = "Unable to create a suitable EGL context";
          goto fail;
        }

      CLUTTER_NOTE (GL, "Created EGL Context");
    }

  if (!eglMakeCurrent (backend_wayland->edpy,
                       backend_wayland->egl_surface,
                       backend_wayland->egl_surface,
                       backend_wayland->egl_context))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Unable to MakeCurrent");
      return FALSE;
    }

  return TRUE;

fail:
    {
      *try_fallback = FALSE;
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "%s", error_message);
      return FALSE;
    }
}

#if defined(HAVE_COGL_GL)
#define _COGL_SURFACELESS_EXTENSION "EGL_KHR_surfaceless_opengl"
#elif defined(HAVE_COGL_GLES)
#define _COGL_SURFACELESS_EXTENSION "EGL_KHR_surfaceless_gles1"
#elif defined(HAVE_COGL_GLES2)
#define _COGL_SURFACELESS_EXTENSION "EGL_KHR_surfaceless_gles2"
#endif

static gboolean
clutter_backend_wayland_create_context (ClutterBackend  *backend,
					GError         **error)
{
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  const gchar *egl_extensions = NULL;
  gboolean status;
  int retry_cookie;
  gboolean try_fallback;
  GError *try_error = NULL;

  if (backend_wayland->egl_context != EGL_NO_CONTEXT)
    return TRUE;

#if defined(HAVE_COGL_GL)
  eglBindAPI (EGL_OPENGL_API);
#else
  eglBindAPI (EGL_OPENGL_ES_API);
#endif
  egl_extensions = eglQueryString (backend_wayland->edpy, EGL_EXTENSIONS);

  if (!cogl_clutter_check_extension (_COGL_SURFACELESS_EXTENSION, egl_extensions))
    {
      g_debug("Could not find the " _COGL_SURFACELESS_EXTENSION
              " extension; falling back to binding a dummy surface");
      if (!make_dummy_surface(backend_wayland))
        {
          g_set_error (error, CLUTTER_INIT_ERROR,
                       CLUTTER_INIT_ERROR_BACKEND,
                       "Could not create dummy surface");
          return FALSE;
        }
    }
  else
    {
      backend_wayland->egl_config = NULL;
      backend_wayland->egl_surface = EGL_NO_SURFACE;
    }

  retry_cookie = 0;
  while (!(status = try_create_context (backend,
                                        retry_cookie,
                                        &try_fallback,
                                        &try_error)) &&
         try_fallback)
    {
      g_warning ("Failed to create context: %s\nWill try fallback...",
                 try_error->message);
      g_error_free (try_error);
      try_error = NULL;
      retry_cookie++;
    }
  if (!status)
    g_propagate_error (error, try_error);

  return status;
}

static void
clutter_backend_wayland_ensure_context (ClutterBackend *backend,
					ClutterStage   *stage)
{
}

static void
clutter_backend_wayland_redraw (ClutterBackend *backend,
				ClutterStage   *stage)
{
  ClutterStageWindow *impl;

  impl = _clutter_stage_get_window (stage);
  if (!impl)
    return;

  g_assert (CLUTTER_IS_STAGE_WAYLAND (impl));

  _clutter_stage_wayland_redraw (CLUTTER_STAGE_WAYLAND (impl), stage);
}

static void
clutter_backend_wayland_init_events (ClutterBackend *backend)
{
}

static void
clutter_backend_wayland_finalize (GObject *gobject)
{
  if (backend_singleton)
    backend_singleton = NULL;

  G_OBJECT_CLASS (_clutter_backend_wayland_parent_class)->finalize (gobject);
}

static void
clutter_backend_wayland_dispose (GObject *gobject)
{
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (gobject);

  /* We chain up before disposing our own resources so that
     ClutterBackend will destroy all of the stages before we destroy
     the egl context. Otherwise the actors may try to make GL calls
     during destruction which causes a crash */
  G_OBJECT_CLASS (_clutter_backend_wayland_parent_class)->dispose (gobject);

  if (backend_wayland->egl_context)
    {
      eglDestroyContext (backend_wayland->edpy, backend_wayland->egl_context);
      backend_wayland->egl_context = NULL;
    }

  if (backend_wayland->edpy)
    {
      eglTerminate (backend_wayland->edpy);
      backend_wayland->edpy = 0;
    }

  if (backend_wayland->drm_fd != -1)
    {
      close (backend_wayland->drm_fd);
      backend_wayland->drm_fd = -1;
    }
}

static GObject *
clutter_backend_wayland_constructor (GType                  gtype,
				     guint                  n_params,
				     GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  GObject *retval;

  if (!backend_singleton)
    {
      parent_class = G_OBJECT_CLASS (_clutter_backend_wayland_parent_class);
      retval = parent_class->constructor (gtype, n_params, params);

      backend_singleton = CLUTTER_BACKEND_WAYLAND (retval);

      return retval;
    }

  g_warning ("Attempting to create a new backend object. This should "
             "never happen, so we return the singleton instance.");

  return g_object_ref (backend_singleton);
}

static ClutterFeatureFlags
clutter_backend_wayland_get_features (ClutterBackend *backend)
{
  ClutterBackendWayland  *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  ClutterFeatureFlags flags = 0;

  g_assert (backend_wayland->egl_context != NULL);

  flags |=
    CLUTTER_FEATURE_STAGE_MULTIPLE |
    CLUTTER_FEATURE_SWAP_EVENTS |
    CLUTTER_FEATURE_SYNC_TO_VBLANK;

  CLUTTER_NOTE (BACKEND, "Checking features\n"
                "GL_VENDOR: %s\n"
                "GL_RENDERER: %s\n"
                "GL_VERSION: %s\n"
                "EGL_VENDOR: %s\n"
                "EGL_VERSION: %s\n"
                "EGL_EXTENSIONS: %s\n",
                glGetString (GL_VENDOR),
                glGetString (GL_RENDERER),
                glGetString (GL_VERSION),
                eglQueryString (backend_wayland->edpy, EGL_VENDOR),
                eglQueryString (backend_wayland->edpy, EGL_VERSION),
                eglQueryString (backend_wayland->edpy, EGL_EXTENSIONS));

  return flags;
}

static ClutterStageWindow *
clutter_backend_wayland_create_stage (ClutterBackend  *backend,
				      ClutterStage    *wrapper,
				      GError         **error)
{
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  ClutterStageWindow *stage;
  ClutterStageWayland *stage_wayland;

  stage = g_object_new (CLUTTER_TYPE_STAGE_WAYLAND, NULL);

  stage_wayland = CLUTTER_STAGE_WAYLAND (stage);
  stage_wayland->backend = backend_wayland;
  stage_wayland->wrapper = wrapper;

  return stage;
}

static void
_clutter_backend_wayland_class_init (ClutterBackendWaylandClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_wayland_constructor;
  gobject_class->dispose     = clutter_backend_wayland_dispose;
  gobject_class->finalize    = clutter_backend_wayland_finalize;

  backend_class->pre_parse        = clutter_backend_wayland_pre_parse;
  backend_class->post_parse       = clutter_backend_wayland_post_parse;
  backend_class->get_features     = clutter_backend_wayland_get_features;
  backend_class->init_events      = clutter_backend_wayland_init_events;
  backend_class->create_stage     = clutter_backend_wayland_create_stage;
  backend_class->create_context   = clutter_backend_wayland_create_context;
  backend_class->ensure_context   = clutter_backend_wayland_ensure_context;
  backend_class->redraw           = clutter_backend_wayland_redraw;
}

static void
_clutter_backend_wayland_init (ClutterBackendWayland *backend_wayland)
{
  backend_wayland->edpy = EGL_NO_DISPLAY;
  backend_wayland->egl_context = EGL_NO_CONTEXT;

  backend_wayland->drm_fd = -1;
}

GType
_clutter_backend_impl_get_type (void)
{
  return _clutter_backend_wayland_get_type ();
}

EGLDisplay
clutter_wayland_get_egl_display (void)
{
  return backend_singleton->edpy;
}
