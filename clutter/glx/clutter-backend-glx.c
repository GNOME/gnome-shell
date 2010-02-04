/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gi18n-lib.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <GL/glx.h>
#include <GL/gl.h>

#include "clutter-backend-glx.h"
#include "clutter-stage-glx.h"
#include "clutter-glx.h"
#include "clutter-profile.h"

#include "../clutter-event.h"
#include "../clutter-main.h"
#include "../clutter-debug.h"
#include "../clutter-private.h"
#include "../clutter-version.h"

#include "cogl/cogl.h"


G_DEFINE_TYPE (ClutterBackendGLX, clutter_backend_glx, CLUTTER_TYPE_BACKEND_X11);

/* singleton object */
static ClutterBackendGLX *backend_singleton = NULL;

static gchar    *clutter_vblank_name = NULL;

#ifdef __linux__
#define DRM_VBLANK_RELATIVE 0x1;

struct drm_wait_vblank_request {
    int           type;
    unsigned int  sequence;
    unsigned long signal;
};

struct drm_wait_vblank_reply {
    int          type;
    unsigned int sequence;
    long         tval_sec;
    long         tval_usec;
};

typedef union drm_wait_vblank {
    struct drm_wait_vblank_request request;
    struct drm_wait_vblank_reply reply;
} drm_wait_vblank_t;

#define DRM_IOCTL_BASE                  'd'
#define DRM_IOWR(nr,type)               _IOWR(DRM_IOCTL_BASE,nr,type)
#define DRM_IOCTL_WAIT_VBLANK           DRM_IOWR(0x3a, drm_wait_vblank_t)

static int drm_wait_vblank(int fd, drm_wait_vblank_t *vbl)
{
    int ret, rc;

    do
      {
        ret = ioctl(fd, DRM_IOCTL_WAIT_VBLANK, vbl);
        vbl->request.type &= ~DRM_VBLANK_RELATIVE;
        rc = errno;
      }
    while (ret && rc == EINTR);

    return rc;
}

#endif

G_CONST_RETURN gchar*
clutter_backend_glx_get_vblank_method (void)
{
  return clutter_vblank_name;
}

static gboolean
clutter_backend_glx_pre_parse (ClutterBackend  *backend,
                               GError         **error)
{
  const gchar *env_string;

  env_string = g_getenv ("CLUTTER_VBLANK");
  if (env_string)
    {
      clutter_vblank_name = g_strdup (env_string);
      env_string = NULL;
    }

  return clutter_backend_x11_pre_parse (backend, error);
}

static gboolean
clutter_backend_glx_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  int glx_major, glx_minor;

  if (clutter_backend_x11_post_parse (backend, error))
    {
      if (!glXQueryVersion (backend_x11->xdpy, &glx_major, &glx_minor)
          || !(glx_major > 1 || glx_minor > 1))
        {
          g_set_error (error, CLUTTER_INIT_ERROR,
                       CLUTTER_INIT_ERROR_BACKEND,
                       "XServer appears to lack required GLX support");

          return FALSE;
        }
    }
  else
    return FALSE;

  return TRUE;
}

static const GOptionEntry entries[] =
{
  { "vblank", 0,
    0,
    G_OPTION_ARG_STRING, &clutter_vblank_name,
    N_("VBlank method to be used (none, dri or glx)"), "METHOD"
  },
  { NULL }
};

static void
clutter_backend_glx_add_options (ClutterBackend *backend,
                                 GOptionGroup   *group)
{
  g_option_group_add_entries (group, entries);
  clutter_backend_x11_add_options (backend, group);
}

static void
clutter_backend_glx_finalize (GObject *gobject)
{
  if (backend_singleton)
    backend_singleton = NULL;

  G_OBJECT_CLASS (clutter_backend_glx_parent_class)->finalize (gobject);
}

static void
clutter_backend_glx_dispose (GObject *gobject)
{
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (gobject);
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (gobject);

  /* Unrealize all shaders, since the GL context is going away */
  _clutter_shader_release_all ();

  if (backend_glx->gl_context)
    {
      glXMakeContextCurrent (backend_x11->xdpy, None, None, NULL);
      glXDestroyContext (backend_x11->xdpy, backend_glx->gl_context);
      backend_glx->gl_context = None;
    }

  if (backend_glx->dummy_xwin)
    {
      XDestroyWindow (backend_x11->xdpy, backend_glx->dummy_xwin);
      backend_glx->dummy_xwin = None;
    }

  G_OBJECT_CLASS (clutter_backend_glx_parent_class)->dispose (gobject);
}

static GObject *
clutter_backend_glx_constructor (GType                  gtype,
                                 guint                  n_params,
                                 GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  GObject *retval;

  if (!backend_singleton)
    {
      parent_class = G_OBJECT_CLASS (clutter_backend_glx_parent_class);
      retval = parent_class->constructor (gtype, n_params, params);

      backend_singleton = CLUTTER_BACKEND_GLX (retval);

      return retval;
    }

  g_warning ("Attempting to create a new backend object. This should "
             "never happen, so we return the singleton instance.");

  return g_object_ref (backend_singleton);
}

static gboolean
check_vblank_env (const char *name)
{
  if (clutter_vblank_name && !g_ascii_strcasecmp (clutter_vblank_name, name))
    return TRUE;

  return FALSE;
}

static ClutterFeatureFlags
clutter_backend_glx_get_features (ClutterBackend *backend)
{
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend);
  const gchar *glx_extensions = NULL;
  ClutterFeatureFlags flags;

  flags = clutter_backend_x11_get_features (backend);
  flags |= CLUTTER_FEATURE_STAGE_MULTIPLE;

  /* this will make sure that the GL context exists */
  g_assert (backend_glx->gl_context != None);
  g_assert (glXGetCurrentDrawable () != None);

  CLUTTER_NOTE (BACKEND,
                "Checking features\n"
                "  GL_VENDOR: %s\n"
                "  GL_RENDERER: %s\n"
                "  GL_VERSION: %s\n"
                "  GL_EXTENSIONS: %s\n",
                glGetString (GL_VENDOR),
                glGetString (GL_RENDERER),
                glGetString (GL_VERSION),
                glGetString (GL_EXTENSIONS));

  glx_extensions =
    glXQueryExtensionsString (clutter_x11_get_default_display (),
			      clutter_x11_get_default_screen ());

  CLUTTER_NOTE (BACKEND, "GLX Extensions: %s", glx_extensions);

  /* First check for explicit disabling or it set elsewhere (eg NVIDIA) */
  if (getenv ("__GL_SYNC_TO_VBLANK") || check_vblank_env ("none"))
    {
      CLUTTER_NOTE (BACKEND, "vblank sync: disabled at user request");
    }
  else
    {
      /* We try two GL vblank syncing mechanisms.
       * glXSwapIntervalSGI is tried first, then glXGetVideoSyncSGI.
       *
       * glXSwapIntervalSGI is known to work with Mesa and in particular
       * the Intel drivers. glXGetVideoSyncSGI has serious problems with
       * Intel drivers causing terrible frame rate so it only tried as a
       * fallback.
       *
       * How well glXGetVideoSyncSGI works with other driver (ATI etc) needs
       * to be investigated. glXGetVideoSyncSGI on ATI at least seems to have
       * no effect.
       */
      if (!check_vblank_env ("dri") &&
          cogl_check_extension ("GLX_SGI_swap_control", glx_extensions))
        {
          backend_glx->swap_interval =
            (SwapIntervalProc) cogl_get_proc_address ("glXSwapIntervalSGI");

          CLUTTER_NOTE (BACKEND, "attempting glXSwapIntervalSGI vblank setup");

          if (backend_glx->swap_interval != NULL)
            {
              if (backend_glx->swap_interval (1) == 0)
                {
                  backend_glx->vblank_type = CLUTTER_VBLANK_GLX_SWAP;
                  flags |= CLUTTER_FEATURE_SYNC_TO_VBLANK;
                  CLUTTER_NOTE (BACKEND, "glXSwapIntervalSGI setup success");
                }
            }

          if (!(flags & CLUTTER_FEATURE_SYNC_TO_VBLANK))
            CLUTTER_NOTE (BACKEND, "glXSwapIntervalSGI vblank setup failed");
        }

      if (!check_vblank_env ("dri") &&
          !(flags & CLUTTER_FEATURE_SYNC_TO_VBLANK) &&
          cogl_check_extension ("GLX_SGI_video_sync", glx_extensions))
        {
          CLUTTER_NOTE (BACKEND, "attempting glXGetVideoSyncSGI vblank setup");

          backend_glx->get_video_sync =
            (GetVideoSyncProc) cogl_get_proc_address ("glXGetVideoSyncSGI");

          backend_glx->wait_video_sync =
            (WaitVideoSyncProc) cogl_get_proc_address ("glXWaitVideoSyncSGI");

          if ((backend_glx->get_video_sync != NULL) &&
              (backend_glx->wait_video_sync != NULL))
            {
              CLUTTER_NOTE (BACKEND,
                            "glXGetVideoSyncSGI vblank setup success");

              backend_glx->vblank_type = CLUTTER_VBLANK_GLX;
              flags |= CLUTTER_FEATURE_SYNC_TO_VBLANK;
            }

          if (!(flags & CLUTTER_FEATURE_SYNC_TO_VBLANK))
            CLUTTER_NOTE (BACKEND, "glXGetVideoSyncSGI vblank setup failed");
        }

#ifdef __linux__
      /*
       * DRI is really an extreme fallback -rumoured to work with Via chipsets
       */
      if (!(flags & CLUTTER_FEATURE_SYNC_TO_VBLANK))
        {
          CLUTTER_NOTE (BACKEND, "attempting DRI vblank setup");
          backend_glx->dri_fd = open("/dev/dri/card0", O_RDWR);
          if (backend_glx->dri_fd >= 0)
            {
              CLUTTER_NOTE (BACKEND, "DRI vblank setup success");
              backend_glx->vblank_type = CLUTTER_VBLANK_DRI;
              flags |= CLUTTER_FEATURE_SYNC_TO_VBLANK;
            }

          if (!(flags & CLUTTER_FEATURE_SYNC_TO_VBLANK))
            CLUTTER_NOTE (BACKEND, "DRI vblank setup failed");
        }
#endif /* __linux__ */

      if (!(flags & CLUTTER_FEATURE_SYNC_TO_VBLANK))
        CLUTTER_NOTE (BACKEND, "no use-able vblank mechanism found");
    }

  CLUTTER_NOTE (BACKEND, "backend features checked");

  return flags;
}

/* It seems the GLX spec never defined an invalid GLXFBConfig that
 * we could overload as an indication of error, so we have to return
 * an explicit boolean status. */
gboolean
_clutter_backend_glx_get_fbconfig (ClutterBackendGLX *backend_glx,
                                   GLXFBConfig       *config)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend_glx);
  GLXFBConfig *configs = NULL;
  gboolean use_argb = clutter_x11_get_use_argb_visual ();
  int n_configs, i;
  static const int attributes[] = {
    GLX_DRAWABLE_TYPE,    GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,      GLX_RGBA_BIT,
    GLX_DOUBLEBUFFER,     GL_TRUE,
    GLX_RED_SIZE,         1,
    GLX_GREEN_SIZE,       1,
    GLX_BLUE_SIZE,        1,
    GLX_ALPHA_SIZE,       1,
    GLX_DEPTH_SIZE,       1,
    GLX_STENCIL_SIZE,     1,
    None
  };

  if (backend_x11->xdpy == None || backend_x11->xscreen == None)
    return FALSE;

  /* If we don't already have a cached config then try to get one */
  if (!backend_glx->found_fbconfig)
    {
      CLUTTER_NOTE (BACKEND,
                    "Retrieving GL fbconfig, dpy: %p, xscreen; %p (%d)",
                    backend_x11->xdpy,
                    backend_x11->xscreen,
                    backend_x11->xscreen_num);

      configs = glXChooseFBConfig (backend_x11->xdpy,
                                   backend_x11->xscreen_num,
                                   attributes,
                                   &n_configs);
      if (configs)
        {
          if (use_argb)
            {
              for (i = 0; i < n_configs; i++)
                {
                  XVisualInfo *vinfo;

                  vinfo = glXGetVisualFromFBConfig (backend_x11->xdpy,
                                                    configs[i]);
                  if (vinfo == None)
                    continue;

                  if (vinfo->depth == 32 &&
                      (vinfo->red_mask   == 0xff0000 &&
                       vinfo->green_mask == 0x00ff00 &&
                       vinfo->blue_mask  == 0x0000ff))
                    {
                      CLUTTER_NOTE (BACKEND,
                                    "Found GLX visual ARGB [index:%d]", i);

                      backend_glx->found_fbconfig = TRUE;
                      backend_glx->fbconfig = configs[i];

                      goto out;
                    }
                }

              /* If we make it here then we didn't find an RGBA config so
                 we'll fall back to using an RGB config */
              CLUTTER_NOTE (BACKEND, "ARGB visual requested, but none found");
            }

          if (n_configs >= 1)
            {
              backend_glx->found_fbconfig = TRUE;
              backend_glx->fbconfig = configs[0];
            }

        out:
          XFree (configs);
        }
    }

  if (backend_glx->found_fbconfig)
    {
      *config = backend_glx->fbconfig;

      return TRUE;
    }
  else
    return FALSE;
}

static XVisualInfo *
clutter_backend_glx_get_visual_info (ClutterBackendX11 *backend_x11)
{
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend_x11);
  GLXFBConfig config;

  if (!_clutter_backend_glx_get_fbconfig (backend_glx, &config))
    return NULL;

  return glXGetVisualFromFBConfig (backend_x11->xdpy, config);
}

static gboolean
clutter_backend_glx_create_context (ClutterBackend  *backend,
                                    GError         **error)
{
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend);
  GLXFBConfig config;
  gboolean is_direct;
  Window root_xwin;
  XSetWindowAttributes attrs;
  XVisualInfo *xvisinfo;
  Display *xdisplay;

  if (backend_glx->gl_context != None)
    return TRUE;

  xdisplay = clutter_x11_get_default_display ();
  root_xwin = clutter_x11_get_root_window ();

  if (!_clutter_backend_glx_get_fbconfig (backend_glx, &config))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Unable to find suitable fbconfig for the GLX context");
      return FALSE;
    }

  CLUTTER_NOTE (BACKEND, "Creating GLX Context (display: %p)", xdisplay);

  backend_glx->gl_context = glXCreateNewContext (xdisplay,
                                                 config,
                                                 GLX_RGBA_TYPE,
                                                 NULL,
                                                 True);
  if (backend_glx->gl_context == None)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Unable to create suitable GL context");
      return FALSE;
    }

  is_direct = glXIsDirect (xdisplay, backend_glx->gl_context);

  CLUTTER_NOTE (GL, "Setting %s context",
                is_direct ? "direct"
                          : "indirect");
  _cogl_set_indirect_context (!is_direct);

  /* COGL assumes that there is always a GL context selected; in order
   * to make sure that a GLX context exists and is made current, we use
   * a dummy, offscreen override-redirect window to which we can always
   * fall back if no stage is available
   *
   * XXX - we need to do this dance because GLX does not allow creating
   * a context and querying it for basic information (even the function
   * pointers) unless it's made current to a real Drawable. it should be
   * possible to avoid this in future releases of Mesa and X11, but right
   * now this is the best solution available.
   */
  xvisinfo = glXGetVisualFromFBConfig (xdisplay, config);
  if (xvisinfo == None)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Unable to retrieve the X11 visual");
      return FALSE;
    }

  clutter_x11_trap_x_errors ();

  attrs.override_redirect = True;
  attrs.colormap = XCreateColormap (xdisplay,
                                    root_xwin,
                                    xvisinfo->visual,
                                    AllocNone);

  backend_glx->dummy_xwin = XCreateWindow (xdisplay, root_xwin,
                                           -100, -100, 1, 1,
                                           0,
                                           xvisinfo->depth,
                                           CopyFromParent,
                                           xvisinfo->visual,
                                           CWOverrideRedirect | CWColormap,
                                           &attrs);

  CLUTTER_NOTE (BACKEND, "Selecting dummy 0x%x for the GLX context",
                (unsigned int) backend_glx->dummy_xwin);

  glXMakeContextCurrent (xdisplay,
                         backend_glx->dummy_xwin,
                         backend_glx->dummy_xwin,
                         backend_glx->gl_context);

  XFree (xvisinfo);

  if (clutter_x11_untrap_x_errors ())
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Unable to select the newly created GLX context");
      return FALSE;
    }

  return TRUE;
}

static void
clutter_backend_glx_ensure_context (ClutterBackend *backend,
                                    ClutterStage   *stage)
{
  ClutterStageWindow *impl;

  /* if there is no stage, the stage is being destroyed or it has no
   * implementation attached to it then we clear the GL context
   */
  if (stage == NULL ||
      (CLUTTER_PRIVATE_FLAGS (stage) & CLUTTER_ACTOR_IN_DESTRUCTION) ||
      ((impl = _clutter_stage_get_window (stage)) == NULL))
    {
      ClutterBackendX11 *backend_x11;

      backend_x11 = CLUTTER_BACKEND_X11 (backend);
      CLUTTER_NOTE (MULTISTAGE, "Clearing all context");

      glXMakeContextCurrent (backend_x11->xdpy, None, None, NULL);
    }
  else
    {
      ClutterBackendGLX *backend_glx;
      ClutterBackendX11 *backend_x11;
      ClutterStageGLX   *stage_glx;
      ClutterStageX11   *stage_x11;

      g_assert (impl != NULL);

      stage_glx = CLUTTER_STAGE_GLX (impl);
      stage_x11 = CLUTTER_STAGE_X11 (impl);
      backend_glx = CLUTTER_BACKEND_GLX (backend);
      backend_x11 = CLUTTER_BACKEND_X11 (backend);

      CLUTTER_NOTE (BACKEND,
                    "Setting context for stage of type %s, window: 0x%x",
                    G_OBJECT_TYPE_NAME (impl),
                    (unsigned int) stage_x11->xwin);

      /* no GL context to set */
      if (backend_glx->gl_context == None)
        return;

      clutter_x11_trap_x_errors ();

      /* we might get here inside the final dispose cycle, so we
       * need to handle this gracefully
       */
      if (stage_x11->xwin == None)
        {
          CLUTTER_NOTE (BACKEND,
                        "Received a stale stage, clearing all context");

          if (backend_glx->dummy_xwin != None)
            {
              glXMakeContextCurrent (backend_x11->xdpy,
                                     backend_glx->dummy_xwin,
                                     backend_glx->dummy_xwin,
                                     backend_glx->gl_context);
            }
          else
            glXMakeContextCurrent (backend_x11->xdpy, None, None, NULL);
        }
      else
        {
          CLUTTER_NOTE (BACKEND,
                        "MakeContextCurrent dpy: %p, window: 0x%x (%s), context: %p",
                        backend_x11->xdpy,
                        (unsigned int) stage_x11->xwin,
                        stage_x11->is_foreign_xwin ? "foreign" : "native",
                        backend_glx->gl_context);

          glXMakeContextCurrent (backend_x11->xdpy,
                                 stage_x11->xwin,
                                 stage_x11->xwin,
                                 backend_glx->gl_context);
        }

      if (clutter_x11_untrap_x_errors ())
        g_critical ("Unable to make the stage window 0x%x the current "
                    "GLX drawable",
                    (unsigned int) stage_x11->xwin);
    }
}

static void
glx_wait_for_vblank (ClutterBackendGLX *backend_glx)
{
  /* If we are going to wait for VBLANK manually, we not only need
   * to flush out pending drawing to the GPU before we sleep, we
   * need to wait for it to finish. Otherwise, we may end up with
   * the situation:
   *
   *        - We finish drawing      - GPU drawing continues
   *        - We go to sleep         - GPU drawing continues
   * VBLANK - We call glXSwapBuffers - GPU drawing continues
   *                                 - GPU drawing continues
   *                                 - Swap buffers happens
   *
   * Producing a tear. Calling glFinish() first will cause us to properly
   * wait for the next VBLANK before we swap. This obviously does not
   * happen when we use GLX_SWAP and let the driver do the right thing
   */

  switch (backend_glx->vblank_type)
    {
    case CLUTTER_VBLANK_GLX_SWAP:
      /* Nothing */
      break;

    case CLUTTER_VBLANK_GLX:
      {
        unsigned int retraceCount;

        glFinish ();

        backend_glx->get_video_sync (&retraceCount);
        backend_glx->wait_video_sync (2,
                                      (retraceCount + 1) % 2,
                                      &retraceCount);
      }
      break;

    case CLUTTER_VBLANK_DRI:
#ifdef __linux__
      {
        drm_wait_vblank_t blank;

        glFinish ();

        blank.request.type     = DRM_VBLANK_RELATIVE;
        blank.request.sequence = 1;
        blank.request.signal   = 0;
        drm_wait_vblank (backend_glx->dri_fd, &blank);
      }
#endif
      break;

    case CLUTTER_VBLANK_NONE:
    default:
      break;
    }
}

static void
clutter_backend_glx_redraw (ClutterBackend *backend,
                            ClutterStage   *stage)
{
  ClutterBackendX11 *backend_x11;
  ClutterStageGLX *stage_glx;
  ClutterStageX11 *stage_x11;
  ClutterStageWindow *impl;
  CLUTTER_STATIC_TIMER (painting_timer,
                        "Redrawing", /* parent */
                        "Painting actors",
                        "The time spent painting actors",
                        0 /* no application private data */);
  CLUTTER_STATIC_TIMER (swapbuffers_timer,
                        "Redrawing", /* parent */
                        "glXSwapBuffers",
                        "The time spent blocked by glXSwapBuffers",
                        0 /* no application private data */);

  impl = _clutter_stage_get_window (stage);
  if (G_UNLIKELY (impl == NULL))
    {
      CLUTTER_NOTE (BACKEND, "Stage [%p] has no implementation", stage);
      return;
    }

  g_assert (CLUTTER_IS_STAGE_GLX (impl));

  backend_x11 = CLUTTER_BACKEND_X11 (backend);
  stage_x11 = CLUTTER_STAGE_X11 (impl);
  stage_glx = CLUTTER_STAGE_GLX (impl);

  CLUTTER_TIMER_START (_clutter_uprof_context, painting_timer);
  /* this will cause the stage implementation to be painted */
  clutter_actor_paint (CLUTTER_ACTOR (stage));
  cogl_flush ();
  CLUTTER_TIMER_STOP (_clutter_uprof_context, painting_timer);

  if (stage_x11->xwin != None)
    {
      /* wait for the next vblank */
      CLUTTER_NOTE (BACKEND, "Waiting for vblank");
      glx_wait_for_vblank (CLUTTER_BACKEND_GLX (backend));

      /* push on the screen */
      CLUTTER_NOTE (BACKEND, "glXSwapBuffers (display: %p, window: 0x%lx)",
                    backend_x11->xdpy,
                    (unsigned long) stage_x11->xwin);

      CLUTTER_TIMER_START (_clutter_uprof_context, swapbuffers_timer);
      glXSwapBuffers (backend_x11->xdpy, stage_x11->xwin);
      CLUTTER_TIMER_STOP (_clutter_uprof_context, swapbuffers_timer);
    }
}

static ClutterStageWindow *
clutter_backend_glx_create_stage (ClutterBackend  *backend,
                                  ClutterStage    *wrapper,
                                  GError         **error)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  ClutterStageWindow *stage_window;
  ClutterStageX11 *stage_x11;

  CLUTTER_NOTE (BACKEND, "Creating stage of type '%s'",
                g_type_name (CLUTTER_STAGE_TYPE));

  stage_window = g_object_new (CLUTTER_TYPE_STAGE_GLX, NULL);

  /* copy backend data into the stage */
  stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  stage_x11->wrapper = wrapper;

  CLUTTER_NOTE (BACKEND,
                "GLX stage created[%p] (dpy:%p, screen:%d, root:%u, wrap:%p)",
                stage_window,
                backend_x11->xdpy,
                backend_x11->xscreen_num,
                (unsigned int) backend_x11->xwin_root,
                wrapper);

  return stage_window;
}

static void
clutter_backend_glx_class_init (ClutterBackendGLXClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);
  ClutterBackendX11Class *backendx11_class = CLUTTER_BACKEND_X11_CLASS (klass);

  gobject_class->constructor = clutter_backend_glx_constructor;
  gobject_class->dispose     = clutter_backend_glx_dispose;
  gobject_class->finalize    = clutter_backend_glx_finalize;

  backend_class->pre_parse      = clutter_backend_glx_pre_parse;
  backend_class->post_parse     = clutter_backend_glx_post_parse;
  backend_class->create_stage   = clutter_backend_glx_create_stage;
  backend_class->add_options    = clutter_backend_glx_add_options;
  backend_class->get_features   = clutter_backend_glx_get_features;
  backend_class->redraw         = clutter_backend_glx_redraw;
  backend_class->create_context = clutter_backend_glx_create_context;
  backend_class->ensure_context = clutter_backend_glx_ensure_context;

  backendx11_class->get_visual_info = clutter_backend_glx_get_visual_info;
}

static void
clutter_backend_glx_init (ClutterBackendGLX *backend_glx)
{

}

/* every backend must implement this function */
GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_glx_get_type ();
}
