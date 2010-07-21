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
 *  Emmanuele Bassi
 *  Robert Bragg
 *  Neil Roberts
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>

#include "clutter-backend-egl.h"
#include "clutter-stage-egl.h"
#include "clutter-egl.h"

#include "../clutter-private.h"
#include "../clutter-main.h"
#include "../clutter-debug.h"
#include "../clutter-version.h"

static ClutterBackendEGL *backend_singleton = NULL;

static const gchar *clutter_fb_device = NULL;

#ifdef COGL_HAS_X11_SUPPORT
G_DEFINE_TYPE (ClutterBackendEGL, clutter_backend_egl, CLUTTER_TYPE_BACKEND_X11);
#else
G_DEFINE_TYPE (ClutterBackendEGL, clutter_backend_egl, CLUTTER_TYPE_BACKEND);
#endif

static void
clutter_backend_at_exit (void)
{
  if (backend_singleton)
    g_object_run_dispose (G_OBJECT (backend_singleton));
}

static gboolean
clutter_backend_egl_pre_parse (ClutterBackend  *backend,
                               GError         **error)
{
  const gchar *env_string;
#ifdef COGL_HAS_X11_SUPPORT
  ClutterBackendClass *backend_x11_class =
    CLUTTER_BACKEND_CLASS (clutter_backend_egl_parent_class);

  if (!backend_x11_class->pre_parse (backend, error))
    return FALSE;
#endif

  env_string = g_getenv ("CLUTTER_FB_DEVICE");
  if (env_string != NULL && env_string[0] != '\0')
    clutter_fb_device = g_strdup (env_string);

  return TRUE;
}

static gboolean
clutter_backend_egl_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);
#ifdef COGL_HAS_X11_SUPPORT
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  ClutterBackendClass *backend_x11_class =
    CLUTTER_BACKEND_CLASS (clutter_backend_egl_parent_class);
#endif
  EGLBoolean status;

#ifdef COGL_HAS_X11_SUPPORT
  if (!backend_x11_class->post_parse (backend, error))
    return FALSE;

#ifndef COGL_HAS_XLIB_SUPPORT
#error "Clutter's EGL on X11 support currently only works with xlib Displays"
#endif
  backend_egl->edpy =
    eglGetDisplay ((NativeDisplayType) backend_x11->xdpy);

  status = eglInitialize (backend_egl->edpy,
                          &backend_egl->egl_version_major,
                          &backend_egl->egl_version_minor);
#else
  backend_egl->edpy = eglGetDisplay (EGL_DEFAULT_DISPLAY);

  status = eglInitialize (backend_egl->edpy,
			  &backend_egl->egl_version_major,
			  &backend_egl->egl_version_minor);
#endif

  g_atexit (clutter_backend_at_exit);

  if (status != EGL_TRUE)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
		   CLUTTER_INIT_ERROR_BACKEND,
		   "Unable to Initialize EGL");
      return FALSE;
    }

  CLUTTER_NOTE (BACKEND, "EGL Reports version %i.%i",
		backend_egl->egl_version_major,
		backend_egl->egl_version_minor);

  return TRUE;
}

static gboolean
clutter_backend_egl_create_context (ClutterBackend  *backend,
                                    GError         **error)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);
#ifdef COGL_HAS_X11_SUPPORT
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
#endif
  EGLConfig          config;
  EGLint             config_count = 0;
  EGLBoolean         status;
  EGLint             cfg_attribs[] = {
    /* NB: This must be the first attribute, since we may
     * try and fallback to no stencil buffer */
    EGL_STENCIL_SIZE,    2,

    EGL_RED_SIZE,        1,
    EGL_GREEN_SIZE,      1,
    EGL_BLUE_SIZE,       1,
    EGL_ALPHA_SIZE,      EGL_DONT_CARE,

    EGL_DEPTH_SIZE,      1,

    EGL_BUFFER_SIZE,     EGL_DONT_CARE,

#if defined (HAVE_COGL_GL)
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
#elif defined (HAVE_COGL_GLES2)
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#else
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
#endif

    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,

    EGL_NONE
  };
  EGLDisplay edpy;
  gint retry_cookie = 0;
  const char *error_message = NULL;
#ifdef COGL_HAS_XLIB_SUPPORT
  XVisualInfo *xvisinfo;
  XSetWindowAttributes attrs;
#endif

  if (backend_egl->egl_context != EGL_NO_CONTEXT)
    return TRUE;

  edpy = clutter_egl_display ();

/* XXX: we should get rid of this goto yukkyness, there is a fail:
 * goto at the end and this retry: goto at the top, but we should just
 * have a try_create_context() function and call it in a loop that
 * tries a different fallback each iteration */
retry:
  /* Here we can change the attributes depending on the fallback count... */

  /* Some GLES hardware can't support a stencil buffer: */
  if (retry_cookie == 1)
    {
      g_warning ("Trying with stencil buffer disabled...");
      cfg_attribs[1 /* EGL_STENCIL_SIZE */] = 0;
    }

  /* XXX: at this point we only have one fallback */

  status = eglChooseConfig (edpy,
                            cfg_attribs,
                            &config, 1,
                            &config_count);
  if (status != EGL_TRUE || config_count == 0)
    {
      error_message = "Unable to select a valid EGL configuration";
      goto fail;
    }

#ifdef HAVE_COGL_GL
  eglBindAPI (EGL_OPENGL_API);
#endif

  if (backend_egl->egl_context == EGL_NO_CONTEXT)
    {
#ifdef HAVE_COGL_GLES2
      static const EGLint attribs[] =
        { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

      backend_egl->egl_context = eglCreateContext (edpy,
                                                   config,
                                                   EGL_NO_CONTEXT,
                                                   attribs);

#else
      backend_egl->egl_context = eglCreateContext (edpy,
                                                   config,
                                                   EGL_NO_CONTEXT,
                                                   NULL);

#endif
      if (backend_egl->egl_context == EGL_NO_CONTEXT)
        {
          error_message = "Unable to create a suitable EGL context";
          goto fail;
        }

#ifdef COGL_HAS_XLIB_SUPPORT
      backend_egl->egl_config = config;
#endif
      CLUTTER_NOTE (GL, "Created EGL Context");
    }

#ifdef COGL_HAS_XLIB_SUPPORT
  /* COGL assumes that there is always a GL context selected; in order
   * to make sure that an EGL context exists and is made current, we use
   * a dummy, offscreen override-redirect window to which we can always
   * fall back if no stage is available */

  xvisinfo = clutter_backend_x11_get_visual_info (backend_x11);
  if (xvisinfo == NULL)
    {
      g_critical ("Unable to find suitable GL visual.");
      return FALSE;
    }

  attrs.override_redirect = True;
  attrs.colormap = XCreateColormap (backend_x11->xdpy,
                                    backend_x11->xwin_root,
                                    xvisinfo->visual,
                                    AllocNone);
  attrs.border_pixel = 0;

  backend_egl->dummy_xwin = XCreateWindow (backend_x11->xdpy,
                                           backend_x11->xwin_root,
                                           -100, -100, 1, 1,
                                           0,
                                           xvisinfo->depth,
                                           CopyFromParent,
                                           xvisinfo->visual,
                                           CWOverrideRedirect |
                                           CWColormap |
                                           CWBorderPixel,
                                           &attrs);

  XFree (xvisinfo);

  backend_egl->dummy_surface =
    eglCreateWindowSurface (edpy,
                            backend_egl->egl_config,
                            (NativeWindowType) backend_egl->dummy_xwin,
                            NULL);

  if (backend_egl->dummy_surface == EGL_NO_SURFACE)
    {
      g_critical ("Unable to create an EGL surface");
      return FALSE;
    }

  eglMakeCurrent (edpy,
                  backend_egl->dummy_surface,
                  backend_egl->dummy_surface,
                  backend_egl->egl_context);

#else /* COGL_HAS_XLIB_SUPPORT */

  if (clutter_fb_device != NULL)
    {
      int fd = open (clutter_fb_device, O_RDWR);

      if (fd < 0)
        {
          int errno_save = errno;

          g_set_error (error, CLUTTER_INIT_ERROR,
                       CLUTTER_INIT_ERROR_BACKEND,
                       "Unable to open the framebuffer device '%s': %s",
                       clutter_fb_device,
                       g_strerror (errno_save));

          return FALSE;
        }
      else
        backend_egl->fb_device_id = fd;

      backend_egl->egl_surface =
        eglCreateWindowSurface (edpy,
                                config,
                                (NativeWindowType) backend_egl->fb_device_id,
                                NULL);
    }
  else
    {
      backend_egl->egl_surface =
        eglCreateWindowSurface (edpy,
                                config,
                                (NativeWindowType) NULL,
                                NULL);
    }

  if (backend_egl->egl_surface == EGL_NO_SURFACE)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Unable to create EGL window surface");

      return FALSE;
    }

  CLUTTER_NOTE (BACKEND, "Setting context");

  /* Without X we assume we can have only one stage, so we
   * store the EGL surface in the backend itself, instead
   * of the StageWindow implementation, and we make it
   * current immediately to make sure the Cogl and Clutter
   * can query the EGL context for features.
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

#endif /* COGL_HAS_XLIB_SUPPORT */

  return TRUE;

fail:

  /* NB: We currently only support a single fallback option */
  if (retry_cookie == 0)
    {
      retry_cookie = 1;
      goto retry;
    }

  g_set_error (error, CLUTTER_INIT_ERROR,
               CLUTTER_INIT_ERROR_BACKEND,
               "%s", error_message);

  return FALSE;
}

static void
clutter_backend_egl_ensure_context (ClutterBackend *backend,
                                    ClutterStage   *stage)
{
#ifndef COGL_HAS_XLIB_SUPPORT
  /* Without X we only have one EGL surface to worry about
   * so we can assume it is permanently made current and
   * don't have to do anything here. */
#else
  ClutterBackendEGL  *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterStageWindow *impl;

  if (stage == NULL ||
      CLUTTER_ACTOR_IN_DESTRUCTION (stage) ||
      ((impl = _clutter_stage_get_window (stage)) == NULL))
    {
      CLUTTER_NOTE (BACKEND, "Clearing EGL context");
      eglMakeCurrent (backend_egl->edpy,
                      EGL_NO_SURFACE,
                      EGL_NO_SURFACE,
                      EGL_NO_CONTEXT);
    }
  else
    {
      ClutterStageEGL    *stage_egl;
      ClutterStageX11    *stage_x11;

      g_assert (impl != NULL);

      CLUTTER_NOTE (MULTISTAGE, "Setting context for stage of type %s [%p]",
                    g_type_name (G_OBJECT_TYPE (impl)),
                    impl);

      stage_egl = CLUTTER_STAGE_EGL (impl);
      stage_x11 = CLUTTER_STAGE_X11 (impl);

      if (backend_egl->egl_context == EGL_NO_CONTEXT)
        return;

      clutter_x11_trap_x_errors ();

      /* we might get here inside the final dispose cycle, so we
       * need to handle this gracefully
       */
      if (stage_x11->xwin == None ||
          stage_egl->egl_surface == EGL_NO_SURFACE)
        {
          CLUTTER_NOTE (MULTISTAGE,
                        "Received a stale stage, clearing all context");

          if (backend_egl->dummy_surface == EGL_NO_SURFACE)
            eglMakeCurrent (backend_egl->edpy,
                            EGL_NO_SURFACE,
                            EGL_NO_SURFACE,
                            EGL_NO_CONTEXT);
          else
            eglMakeCurrent (backend_egl->edpy,
                            backend_egl->dummy_surface,
                            backend_egl->dummy_surface,
                            backend_egl->egl_context);
        }
      else
        {
          CLUTTER_NOTE (MULTISTAGE, "Setting real surface current");
          eglMakeCurrent (backend_egl->edpy,
                          stage_egl->egl_surface,
                          stage_egl->egl_surface,
                          backend_egl->egl_context);
        }

      if (clutter_x11_untrap_x_errors ())
        g_critical ("Unable to make the stage window 0x%x the current "
                    "EGLX drawable",
                    (int) stage_x11->xwin);
    }
#endif /* COGL_HAS_XLIB_SUPPORT */
}

static void
clutter_backend_egl_redraw (ClutterBackend *backend,
                            ClutterStage   *stage)
{
  ClutterStageWindow *impl;

  impl = _clutter_stage_get_window (stage);
  if (!impl)
    return;

  g_assert (CLUTTER_IS_STAGE_EGL (impl));

  clutter_stage_egl_redraw (CLUTTER_STAGE_EGL (impl), stage);
}

#ifdef HAVE_TSLIB
static void
clutter_backend_egl_init_events (ClutterBackend *backend)
{
  /* XXX: This should be renamed to _clutter_events_tslib_init */
  _clutter_events_egl_init (CLUTTER_BACKEND_EGL (backend));
}
#endif

static void
clutter_backend_egl_finalize (GObject *gobject)
{
  if (backend_singleton)
    backend_singleton = NULL;

  G_OBJECT_CLASS (clutter_backend_egl_parent_class)->finalize (gobject);
}

static void
clutter_backend_egl_dispose (GObject *gobject)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (gobject);
#ifdef COGL_HAS_X11_SUPPORT
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (gobject);
#else
  ClutterStageEGL   *stage_egl = CLUTTER_STAGE_EGL (backend_egl->stage);
#endif

  /* We chain up before disposing our own resources so that
     ClutterBackendX11 will destroy all of the stages before we
     destroy the egl context. Otherwise the actors may try to make GL
     calls during destruction which causes a crash */
  G_OBJECT_CLASS (clutter_backend_egl_parent_class)->dispose (gobject);

#ifdef HAVE_TSLIB
  /* XXX: This should be renamed to _clutter_events_tslib_uninit */
  _clutter_events_egl_uninit (backend_egl);
#endif

#ifdef COGL_HAS_XLIB_SUPPORT
  if (backend_egl->dummy_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (backend_egl->edpy, backend_egl->dummy_surface);
      backend_egl->dummy_surface = EGL_NO_SURFACE;
    }

  if (backend_egl->dummy_xwin)
    {
      XDestroyWindow (backend_x11->xdpy, backend_egl->dummy_xwin);
      backend_egl->dummy_xwin = None;
    }

#else /* COGL_HAS_XLIB_SUPPORT */

  if (backend_egl->egl_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (backend_egl->edpy, backend_egl->egl_surface);
      backend_egl->egl_surface = EGL_NO_SURFACE;
    }

  if (backend_egl->stage != NULL)
    {
      clutter_actor_destroy (CLUTTER_ACTOR (stage_egl->wrapper));
      backend_egl->stage = NULL;
    }

  if (backend_egl->fb_device_id != -1)
    {
      close (backend_egl->fb_device_id);
      backend_egl->fb_device_id = -1;
    }

#endif /* COGL_HAS_XLIB_SUPPORT */

  if (backend_egl->egl_context)
    {
      eglDestroyContext (backend_egl->edpy, backend_egl->egl_context);
      backend_egl->egl_context = NULL;
    }

  if (backend_egl->edpy)
    {
      eglTerminate (backend_egl->edpy);
      backend_egl->edpy = 0;
    }

#ifdef HAVE_TSLIB
  if (backend_egl->event_timer != NULL)
    {
      g_timer_destroy (backend_egl->event_timer);
      backend_egl->event_timer = NULL;
    }
#endif
}

static GObject *
clutter_backend_egl_constructor (GType                  gtype,
                                 guint                  n_params,
                                 GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  GObject *retval;

  if (!backend_singleton)
    {
      parent_class = G_OBJECT_CLASS (clutter_backend_egl_parent_class);
      retval = parent_class->constructor (gtype, n_params, params);

      backend_singleton = CLUTTER_BACKEND_EGL (retval);

      return retval;
    }

  g_warning ("Attempting to create a new backend object. This should "
             "never happen, so we return the singleton instance.");

  return g_object_ref (backend_singleton);
}

static ClutterFeatureFlags
clutter_backend_egl_get_features (ClutterBackend *backend)
{
  ClutterBackendEGL  *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterFeatureFlags flags;

  g_assert (backend_egl->egl_context != NULL);

#ifdef COGL_HAS_XLIB_SUPPORT
  flags = clutter_backend_x11_get_features (backend);
  flags |= CLUTTER_FEATURE_STAGE_MULTIPLE;
#else
  flags = CLUTTER_FEATURE_STAGE_STATIC;
#endif

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
                eglQueryString (backend_egl->edpy, EGL_VENDOR),
                eglQueryString (backend_egl->edpy, EGL_VERSION),
                eglQueryString (backend_egl->edpy, EGL_EXTENSIONS));

  return flags;
}

static ClutterStageWindow *
clutter_backend_egl_create_stage (ClutterBackend  *backend,
                                  ClutterStage    *wrapper,
                                  GError         **error)
{
#ifdef COGL_HAS_XLIB_SUPPORT
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  ClutterStageWindow *stage;
  ClutterStageX11 *stage_x11;

  CLUTTER_NOTE (BACKEND, "Creating stage of type '%s'",
                g_type_name (CLUTTER_STAGE_TYPE));

  stage = g_object_new (CLUTTER_TYPE_STAGE_EGL, NULL);

  /* copy backend data into the stage */
  stage_x11 = CLUTTER_STAGE_X11 (stage);
  stage_x11->wrapper = wrapper;

  CLUTTER_NOTE (MISC, "EGLX stage created (display:%p, screen:%d, root:%u)",
                backend_x11->xdpy,
                backend_x11->xscreen_num,
                (unsigned int) backend_x11->xwin_root);

#else /* COGL_HAS_XLIB_SUPPORT */

  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterStageWindow *stage;
  ClutterStageEGL *stage_egl;

  if (G_UNLIKELY (backend_egl->stage != NULL))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "The EGL native backend does not support multiple stages");
      return backend_egl->stage;
    }

  stage = g_object_new (CLUTTER_TYPE_STAGE_EGL, NULL);

  stage_egl = CLUTTER_STAGE_EGL (stage);
  stage_egl->backend = backend_egl;
  stage_egl->wrapper = wrapper;

  backend_egl->stage = stage;

#endif /* COGL_HAS_XLIB_SUPPORT */

  return stage;
}

#ifdef COGL_HAS_XLIB_SUPPORT
static XVisualInfo *
clutter_backend_egl_get_visual_info (ClutterBackendX11 *backend_x11)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend_x11);
  XVisualInfo visinfo_template;
  int template_mask = 0;
  XVisualInfo *visinfo = NULL;
  int visinfos_count;
  EGLint visualid, red_size, green_size, blue_size, alpha_size;

  if (!clutter_backend_egl_create_context (CLUTTER_BACKEND (backend_x11), NULL))
    return NULL;

  visinfo_template.screen = backend_x11->xscreen_num;
  template_mask |= VisualScreenMask;

  eglGetConfigAttrib (backend_egl->edpy, backend_egl->egl_config,
                      EGL_NATIVE_VISUAL_ID, &visualid);

  if (visualid != 0)
    {
      visinfo_template.visualid = visualid;
      template_mask |= VisualIDMask;
    }
  else
    {
      /* some EGL drivers don't implement the EGL_NATIVE_VISUAL_ID
       * attribute, so attempt to find the closest match. */

      eglGetConfigAttrib (backend_egl->edpy, backend_egl->egl_config,
                          EGL_RED_SIZE, &red_size);
      eglGetConfigAttrib (backend_egl->edpy, backend_egl->egl_config,
                          EGL_GREEN_SIZE, &green_size);
      eglGetConfigAttrib (backend_egl->edpy, backend_egl->egl_config,
                          EGL_BLUE_SIZE, &blue_size);
      eglGetConfigAttrib (backend_egl->edpy, backend_egl->egl_config,
                          EGL_ALPHA_SIZE, &alpha_size);

      visinfo_template.depth = red_size + green_size + blue_size + alpha_size;
      template_mask |= VisualDepthMask;
    }

  visinfo = XGetVisualInfo (backend_x11->xdpy,
                            template_mask,
                            &visinfo_template,
                            &visinfos_count);

  return visinfo;
}
#endif

static void
clutter_backend_egl_class_init (ClutterBackendEGLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);
#ifdef COGL_HAS_X11_SUPPORT
  ClutterBackendX11Class *backendx11_class = CLUTTER_BACKEND_X11_CLASS (klass);
#endif

  gobject_class->constructor = clutter_backend_egl_constructor;
  gobject_class->dispose     = clutter_backend_egl_dispose;
  gobject_class->finalize    = clutter_backend_egl_finalize;

  backend_class->pre_parse        = clutter_backend_egl_pre_parse;
  backend_class->post_parse       = clutter_backend_egl_post_parse;
  backend_class->get_features     = clutter_backend_egl_get_features;
#ifdef HAVE_TSLIB
  backend_class->init_events      = clutter_backend_egl_init_events;
#endif
  backend_class->create_stage     = clutter_backend_egl_create_stage;
  backend_class->create_context   = clutter_backend_egl_create_context;
  backend_class->ensure_context   = clutter_backend_egl_ensure_context;
  backend_class->redraw           = clutter_backend_egl_redraw;

#ifdef COGL_HAS_XLIB_SUPPORT
  backendx11_class->get_visual_info = clutter_backend_egl_get_visual_info;
#endif
}

static void
clutter_backend_egl_init (ClutterBackendEGL *backend_egl)
{
#ifndef COGL_HAS_XLIB_SUPPORT
  ClutterBackend *backend = CLUTTER_BACKEND (backend_egl);

#ifdef HAVE_TSLIB
  backend_egl->event_timer = g_timer_new ();
#endif

  backend_egl->fb_device_id = -1;

#else

  backend_egl->egl_context = EGL_NO_CONTEXT;
  backend_egl->dummy_surface = EGL_NO_SURFACE;

#endif
}

GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_egl_get_type ();
}

#ifdef COGL_HAS_XLIB_SUPPORT
EGLDisplay
clutter_eglx_display (void)
{
  return backend_singleton->edpy;
}
#endif /* COGL_HAS_XLIB_SUPPORT */

EGLDisplay
clutter_egl_display (void)
{
  return backend_singleton->edpy;
}

