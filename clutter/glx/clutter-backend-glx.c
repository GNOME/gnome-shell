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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#include <glib/gi18n-lib.h>

#include <GL/glx.h>
#include <GL/glxext.h>
#include <GL/gl.h>

#include "clutter-backend-glx.h"
#include "clutter-event-glx.h"
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
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend);
  ClutterBackendClass *backend_class =
    CLUTTER_BACKEND_CLASS (clutter_backend_glx_parent_class);
  int glx_major, glx_minor;

  if (!backend_class->post_parse (backend, error))
    return FALSE;

  if (!glXQueryExtension (backend_x11->xdpy,
                          &backend_glx->error_base,
                          &backend_glx->event_base))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "XServer appears to lack required GLX support");

      return FALSE;
    }

  /* XXX: Technically we should require >= GLX 1.3 support but for a long
   * time Mesa has exported a hybrid GLX, exporting extensions specified
   * to require GLX 1.3, but still reporting 1.2 via glXQueryVersion. */
  if (!glXQueryVersion (backend_x11->xdpy, &glx_major, &glx_minor)
      || !(glx_major == 1 && glx_minor >= 2))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "XServer appears to lack required GLX 1.2 support");
      return FALSE;
    }

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
      backend_glx->gl_context = NULL;
    }

  if (backend_glx->dummy_glxwin)
    {
      glXDestroyWindow (backend_x11->xdpy, backend_glx->dummy_glxwin);
      backend_glx->dummy_glxwin = None;
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
  const gchar *gl_extensions = NULL;
  ClutterFeatureFlags flags;
  gboolean use_dri = FALSE;

  flags = clutter_backend_x11_get_features (backend);
  flags |= CLUTTER_FEATURE_STAGE_MULTIPLE;

  /* this will make sure that the GL context exists */
  g_assert (backend_glx->gl_context != NULL);
  g_assert (glXGetCurrentDrawable () != None);

  CLUTTER_NOTE (BACKEND,
                "Checking features\n"
                "  GL_VENDOR: %s\n"
                "  GL_RENDERER: %s\n"
                "  GL_VERSION: %s\n"
                "  GL_EXTENSIONS: %s",
                glGetString (GL_VENDOR),
                glGetString (GL_RENDERER),
                glGetString (GL_VERSION),
                glGetString (GL_EXTENSIONS));

  glx_extensions =
    glXQueryExtensionsString (clutter_x11_get_default_display (),
                              clutter_x11_get_default_screen ());

  CLUTTER_NOTE (BACKEND, "  GLX Extensions: %s", glx_extensions);

  gl_extensions = (const gchar *)glGetString (GL_EXTENSIONS);

  /* When using glBlitFramebuffer or glXCopySubBufferMESA for sub stage
   * redraws, we cannot rely on glXSwapIntervalSGI to throttle the blits
   * so we need to resort to manually synchronizing with the vblank so we
   * always check for the video_sync extension...
   */
  if (_cogl_check_extension ("GLX_SGI_video_sync", glx_extensions) &&
      /* Note: the GLX_SGI_video_sync spec explicitly states this extension
       * only works for direct contexts. */
      glXIsDirect (clutter_x11_get_default_display (),
                   backend_glx->gl_context))
    {
      backend_glx->get_video_sync =
        (GetVideoSyncProc) cogl_get_proc_address ("glXGetVideoSyncSGI");

      backend_glx->wait_video_sync =
        (WaitVideoSyncProc) cogl_get_proc_address ("glXWaitVideoSyncSGI");
    }

  use_dri = check_vblank_env ("dri");

  /* First check for explicit disabling or it set elsewhere (eg NVIDIA) */
  if (check_vblank_env ("none"))
    {
      CLUTTER_NOTE (BACKEND, "vblank sync: disabled at user request");
      goto vblank_setup_done;
    }

  if (g_getenv ("__GL_SYNC_TO_VBLANK") != NULL)
    {
      backend_glx->vblank_type = CLUTTER_VBLANK_GLX_SWAP;
      flags |= CLUTTER_FEATURE_SYNC_TO_VBLANK;

      CLUTTER_NOTE (BACKEND, "Using __GL_SYNC_TO_VBLANK hint");
      goto vblank_setup_done;
    }

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
  if (!use_dri &&
      _cogl_check_extension ("GLX_SGI_swap_control", glx_extensions))
    {
      backend_glx->swap_interval =
        (SwapIntervalProc) cogl_get_proc_address ("glXSwapIntervalSGI");

      CLUTTER_NOTE (BACKEND, "attempting glXSwapIntervalSGI vblank setup");

      if (backend_glx->swap_interval != NULL &&
          backend_glx->swap_interval (1) == 0)
        {
          backend_glx->vblank_type = CLUTTER_VBLANK_GLX_SWAP;
          flags |= CLUTTER_FEATURE_SYNC_TO_VBLANK;

          CLUTTER_NOTE (BACKEND, "glXSwapIntervalSGI setup success");

#ifdef GLX_INTEL_swap_event
          /* GLX_INTEL_swap_event allows us to avoid blocking the CPU
           * while we wait for glXSwapBuffers to complete, and instead
           * we get an X event notifying us of completion...
           */
          if (!(clutter_paint_debug_flags & CLUTTER_DEBUG_DISABLE_SWAP_EVENTS) &&
              _cogl_check_extension ("GLX_INTEL_swap_event", glx_extensions))
            {
              flags |= CLUTTER_FEATURE_SWAP_EVENTS;
            }
#endif /* GLX_INTEL_swap_event */

          goto vblank_setup_done;
        }

      CLUTTER_NOTE (BACKEND, "glXSwapIntervalSGI vblank setup failed");
    }

  if (!use_dri &&
      !(flags & CLUTTER_FEATURE_SYNC_TO_VBLANK) &&
      _cogl_check_extension ("GLX_SGI_video_sync", glx_extensions))
    {
      CLUTTER_NOTE (BACKEND, "attempting glXGetVideoSyncSGI vblank setup");

      if ((backend_glx->get_video_sync != NULL) &&
          (backend_glx->wait_video_sync != NULL))
        {
          CLUTTER_NOTE (BACKEND, "glXGetVideoSyncSGI vblank setup success");

          backend_glx->vblank_type = CLUTTER_VBLANK_GLX;
          flags |= CLUTTER_FEATURE_SYNC_TO_VBLANK;

          goto vblank_setup_done;
        }

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

          goto vblank_setup_done;
        }

      CLUTTER_NOTE (BACKEND, "DRI vblank setup failed");
    }
#endif /* __linux__ */

  CLUTTER_NOTE (BACKEND, "no use-able vblank mechanism found");

vblank_setup_done:

  if (_cogl_check_extension ("GLX_MESA_copy_sub_buffer", glx_extensions))
    {
      backend_glx->copy_sub_buffer =
        (CopySubBufferProc) cogl_get_proc_address ("glXCopySubBufferMESA");
      backend_glx->can_blit_sub_buffer = TRUE;
      backend_glx->blit_sub_buffer_is_synchronized = TRUE;
    }
  else if (_cogl_check_extension ("GL_EXT_framebuffer_blit", gl_extensions))
    {
      CLUTTER_NOTE (BACKEND,
                    "Using glBlitFramebuffer fallback for sub_buffer copies");
      backend_glx->blit_framebuffer =
        (BlitFramebufferProc) cogl_get_proc_address ("glBlitFramebuffer");
      backend_glx->can_blit_sub_buffer = TRUE;
      backend_glx->blit_sub_buffer_is_synchronized = FALSE;
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

  if (backend_x11->xdpy == NULL || backend_x11->xscreen == NULL)
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
                  if (vinfo == NULL)
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

void
_clutter_backend_glx_blit_sub_buffer (ClutterBackendGLX *backend_glx,
                                      GLXDrawable drawable,
                                      int x, int y, int width, int height)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend_glx);

  if (backend_glx->copy_sub_buffer)
    {
      backend_glx->copy_sub_buffer (backend_x11->xdpy, drawable,
                                    x, y, width, height);
    }
  else if (backend_glx->blit_framebuffer)
    {
      glDrawBuffer (GL_FRONT);
      backend_glx->blit_framebuffer (x, y, x + width, y + height,
                                     x, y, x + width, y + height,
                                     GL_COLOR_BUFFER_BIT, GL_NEAREST);
      glDrawBuffer (GL_BACK);
      glFlush();
    }
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
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  GLXFBConfig config;
  gboolean is_direct;
  Window root_xwin;
  XSetWindowAttributes attrs;
  XVisualInfo *xvisinfo;
  Display *xdisplay;
  int major;
  int minor;
  GLXDrawable dummy_drawable;

  if (backend_glx->gl_context != NULL)
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
  if (backend_glx->gl_context == NULL)
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
  if (xvisinfo == NULL)
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
  attrs.border_pixel = 0;

  backend_glx->dummy_xwin = XCreateWindow (xdisplay, root_xwin,
                                           -100, -100, 1, 1,
                                           0,
                                           xvisinfo->depth,
                                           CopyFromParent,
                                           xvisinfo->visual,
                                           CWOverrideRedirect | CWColormap | CWBorderPixel,
                                           &attrs);

  /* Try and create a GLXWindow to use with extensions dependent on
   * GLX versions >= 1.3 that don't accept regular X Windows as GLX
   * drawables. */
  if (glXQueryVersion (backend_x11->xdpy, &major, &minor) &&
      major == 1 && minor >= 3)
    {
      backend_glx->dummy_glxwin = glXCreateWindow (backend_x11->xdpy,
                                                   config,
                                                   backend_glx->dummy_xwin,
                                                   NULL);
    }

  if (backend_glx->dummy_glxwin)
    dummy_drawable = backend_glx->dummy_glxwin;
  else
    dummy_drawable = backend_glx->dummy_xwin;

  CLUTTER_NOTE (BACKEND, "Selecting dummy 0x%x for the GLX context",
                (unsigned int) dummy_drawable);

  glXMakeContextCurrent (xdisplay,
                         dummy_drawable,
                         dummy_drawable,
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

/* TODO: remove this interface in favour of
 * _clutter_stage_window_make_current () */
static void
clutter_backend_glx_ensure_context (ClutterBackend *backend,
                                    ClutterStage   *stage)
{
  ClutterStageWindow *impl;

  /* if there is no stage, the stage is being destroyed or it has no
   * implementation attached to it then we clear the GL context
   */
  if (stage == NULL ||
      CLUTTER_ACTOR_IN_DESTRUCTION (stage) ||
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
      GLXDrawable        drawable;

      g_assert (impl != NULL);

      stage_glx = CLUTTER_STAGE_GLX (impl);
      stage_x11 = CLUTTER_STAGE_X11 (impl);
      backend_glx = CLUTTER_BACKEND_GLX (backend);
      backend_x11 = CLUTTER_BACKEND_X11 (backend);

      drawable = stage_glx->glxwin ? stage_glx->glxwin : stage_x11->xwin;

      CLUTTER_NOTE (BACKEND,
                    "Setting context for stage of type %s, window: 0x%x",
                    G_OBJECT_TYPE_NAME (impl),
                    (unsigned int) drawable);

      /* no GL context to set */
      if (backend_glx->gl_context == NULL)
        return;

      clutter_x11_trap_x_errors ();

      /* we might get here inside the final dispose cycle, so we
       * need to handle this gracefully
       */
      if (drawable == None)
        {
          GLXDrawable dummy_drawable;

          CLUTTER_NOTE (BACKEND,
                        "Received a stale stage, clearing all context");

          if (backend_glx->dummy_glxwin)
            dummy_drawable = backend_glx->dummy_glxwin;
          else
            dummy_drawable = backend_glx->dummy_xwin;

          if (dummy_drawable == None)
            glXMakeContextCurrent (backend_x11->xdpy, None, None, NULL);
          else
            {
              glXMakeContextCurrent (backend_x11->xdpy,
                                     dummy_drawable,
                                     dummy_drawable,
                                     backend_glx->gl_context);
            }
        }
      else
        {
          CLUTTER_NOTE (BACKEND,
                        "MakeContextCurrent dpy: %p, window: 0x%x (%s), context: %p",
                        backend_x11->xdpy,
                        (unsigned int) drawable,
                        stage_x11->is_foreign_xwin ? "foreign" : "native",
                        backend_glx->gl_context);

          glXMakeContextCurrent (backend_x11->xdpy,
                                 drawable,
                                 drawable,
                                 backend_glx->gl_context);
          /*
           * In case we are using GLX_SGI_swap_control for vblank syncing we need call
           * glXSwapIntervalSGI here to make sure that it affects the current drawable.
           */
          if (backend_glx->vblank_type == CLUTTER_VBLANK_GLX_SWAP && backend_glx->swap_interval != NULL)
            backend_glx->swap_interval (1);
        }

      if (clutter_x11_untrap_x_errors ())
        g_critical ("Unable to make the stage window 0x%x the current "
                    "GLX drawable",
                    (unsigned int) drawable);
    }
}

/*
 * FIXME: we should remove backend_class->redraw() and just
 * have stage_window_iface->redraw()
 */
static void
clutter_backend_glx_redraw (ClutterBackend *backend,
                            ClutterStage   *stage)
{
  ClutterStageWindow *impl = _clutter_stage_get_window (stage);

  if (G_UNLIKELY (impl == NULL))
    {
      CLUTTER_NOTE (BACKEND, "Stage [%p] has no implementation", stage);
      return;
    }

  g_assert (CLUTTER_IS_STAGE_GLX (impl));

  clutter_stage_glx_redraw (CLUTTER_STAGE_GLX (impl),
                            stage);
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
  backendx11_class->handle_event = clutter_backend_glx_handle_event;
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
