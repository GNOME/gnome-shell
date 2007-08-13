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

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <GL/glx.h>
#include <GL/gl.h>



#include "clutter-backend-glx.h"
#include "clutter-stage-glx.h"
#include "clutter-glx.h"

#include "../clutter-event.h"
#include "../clutter-main.h"
#include "../clutter-debug.h"
#include "../clutter-private.h"

#include "cogl.h"

G_DEFINE_TYPE (ClutterBackendGLX, clutter_backend_glx, CLUTTER_TYPE_BACKEND);

/* singleton object */
static ClutterBackendGLX *backend_singleton = NULL;

/* options */
static gchar *clutter_display_name = NULL;
static gint clutter_screen = 0;
static gboolean clutter_synchronise = FALSE;

/* X error trap */
static int TrappedErrorCode = 0;
static int (* old_error_handler) (Display *, XErrorEvent *);

static gchar *clutter_vblank_name = NULL;

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

  /* we don't fail here if DISPLAY is not set, as the user
   * might pass the --display command line switch
   */
  env_string = g_getenv ("DISPLAY");
  if (env_string)
    {
      clutter_display_name = g_strdup (env_string);
      env_string = NULL;
    }

  env_string = g_getenv ("CLUTTER_VBLANK");
  if (env_string)
    {
      clutter_vblank_name = g_strdup (env_string);
      env_string = NULL;
    }

  return TRUE;
}

static gboolean
clutter_backend_glx_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend);

  if (clutter_display_name)
    {
      CLUTTER_NOTE (BACKEND, "XOpenDisplay on `%s'", clutter_display_name);
      backend_glx->xdpy = XOpenDisplay (clutter_display_name);
    }
  else
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Unable to open display. You have to set the DISPLAY "
                   "environment variable, or use the --display command "
                   "line argument");
      return FALSE;
    }

  if (backend_glx->xdpy)
    {
      int glx_major, glx_minor;
      double dpi;

      CLUTTER_NOTE (BACKEND, "Getting the X screen");

      if (clutter_screen == 0)
        backend_glx->xscreen = DefaultScreenOfDisplay (backend_glx->xdpy);
      else
        backend_glx->xscreen = ScreenOfDisplay (backend_glx->xdpy,
                                                clutter_screen);
      
      backend_glx->xscreen_num = XScreenNumberOfScreen (backend_glx->xscreen);

      backend_glx->xwin_root = RootWindow (backend_glx->xdpy,
                                           backend_glx->xscreen_num);
      
      backend_glx->display_name = g_strdup (clutter_display_name);

      CLUTTER_NOTE (BACKEND, "Checking GLX info");

      if (!glXQueryVersion (backend_glx->xdpy, &glx_major, &glx_minor) 
	  || !(glx_major > 1 || glx_minor > 1)) 
	{
	  g_set_error (error, CLUTTER_INIT_ERROR,
		       CLUTTER_INIT_ERROR_BACKEND,
		       "XServer appears to lack required GLX support");
	  return 1;
	}

#if 0
      /* Prefer current GLX specs over current violations */
      if (!(glx_major > 1 || glx_minor > 2)) 
	{
	  
	  const char* exts = glXQueryExtensionsString (display, screen);
	  if (!exts || !strstr (exts, "GLX_SGIX_fbconfig"))
	    have_fbconfig = 0;
	}
#endif
      
      dpi = (((double) DisplayHeight (backend_glx->xdpy, backend_glx->xscreen_num) * 25.4)
            / (double) DisplayHeightMM (backend_glx->xdpy, backend_glx->xscreen_num));

      clutter_backend_set_resolution (backend, dpi);

      if (clutter_synchronise)
        XSynchronize (backend_glx->xdpy, True);
    }

  g_free (clutter_display_name);
  
  CLUTTER_NOTE (BACKEND,
                "X Display `%s'[%p] opened (screen:%d, root:%u, dpi:%f)",
                backend_glx->display_name,
                backend_glx->xdpy,
                backend_glx->xscreen_num,
                (unsigned int) backend_glx->xwin_root,
                clutter_backend_get_resolution (backend));

  return TRUE;
}


static gboolean
clutter_backend_glx_init_stage (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend);

  if (!backend_glx->stage)
    {
      ClutterStageGLX *stage_glx;
      ClutterActor *stage;

      stage = g_object_new (CLUTTER_TYPE_STAGE_GLX, NULL);

      /* copy backend data into the stage */
      stage_glx = CLUTTER_STAGE_GLX (stage);
      stage_glx->xdpy = backend_glx->xdpy;
      stage_glx->xwin_root = backend_glx->xwin_root;
      stage_glx->xscreen = backend_glx->xscreen_num;
      stage_glx->backend = backend_glx;

      CLUTTER_NOTE (MISC, "GLX stage created (display:%p, screen:%d, root:%u)",
                    stage_glx->xdpy,
                    stage_glx->xscreen,
                    (unsigned int) stage_glx->xwin_root);

      g_object_set_data (G_OBJECT (stage), "clutter-backend", backend);

      backend_glx->stage = g_object_ref_sink (stage);
    }

  clutter_actor_realize (backend_glx->stage);
  if (!CLUTTER_ACTOR_IS_REALIZED (backend_glx->stage))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_INTERNAL,
                   "Unable to realize the main stage");
      return FALSE;
    }

  return TRUE;
}

static void
clutter_backend_glx_init_events (ClutterBackend *backend)
{
  CLUTTER_NOTE (EVENT, "initialising the event loop");

  _clutter_backend_glx_events_init (backend);
}

static ClutterActor *
clutter_backend_glx_get_stage (ClutterBackend *backend)
{
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend);

  return backend_glx->stage;
}

static const GOptionEntry entries[] =
{
  {
    "display", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_STRING, &clutter_display_name,
    "X display to use", "DISPLAY"
  },
  {
    "screen", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_INT, &clutter_screen,
    "X screen to use", "SCREEN"
  },
  { "vblank", 0, 
    0, 
    G_OPTION_ARG_STRING, &clutter_vblank_name,
    "VBlank method to be used (none, dri or glx)", "METHOD" 
  },
  { "synch", 0,
    0,
    G_OPTION_ARG_NONE, &clutter_synchronise,
    "Make X calls synchronous", NULL,
  },
  { NULL }
};

static void
clutter_backend_glx_add_options (ClutterBackend *backend,
                                 GOptionGroup   *group)
{
  g_option_group_add_entries (group, entries);
}

static void
clutter_backend_glx_finalize (GObject *gobject)
{
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (gobject);

  g_free (backend_glx->display_name);

  XCloseDisplay (backend_glx->xdpy);

  if (backend_singleton)
    backend_singleton = NULL;

  G_OBJECT_CLASS (clutter_backend_glx_parent_class)->finalize (gobject);
}

static void
clutter_backend_glx_dispose (GObject *gobject)
{
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (gobject);

  if (backend_glx->stage)
    {
      CLUTTER_NOTE (BACKEND, "Disposing the main stage");

      clutter_actor_destroy (backend_glx->stage);
      backend_glx->stage = NULL;
    }
 
  CLUTTER_NOTE (BACKEND, "Removing the event source");
  _clutter_backend_glx_events_uninit (CLUTTER_BACKEND (backend_glx));

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
  if (clutter_vblank_name && !strcasecmp(clutter_vblank_name, name))
    return TRUE;

  return FALSE;
}


static ClutterFeatureFlags
clutter_backend_glx_get_features (ClutterBackend *backend)
{
  ClutterBackendGLX  *backend_glx = CLUTTER_BACKEND_GLX (backend);
  const gchar        *glx_extensions = NULL;
  ClutterFeatureFlags flags = 0;

  /* FIXME: we really need to check if gl context is set */

  flags = CLUTTER_FEATURE_STAGE_USER_RESIZE|CLUTTER_FEATURE_STAGE_CURSOR;

  CLUTTER_NOTE (BACKEND, "Checking features\n"
		"GL_VENDOR: %s\n"
		"GL_RENDERER: %s\n"
		"GL_VERSION: %s\n"
		"GL_EXTENSIONS: %s\n",
		glGetString (GL_VENDOR),
		glGetString (GL_RENDERER),
		glGetString (GL_VERSION),
		glGetString (GL_EXTENSIONS));

  glx_extensions = 
    glXQueryExtensionsString (clutter_glx_get_default_display (),
			      clutter_glx_get_default_screen ());

  CLUTTER_NOTE (BACKEND, "GLX Extensions: %s", glx_extensions);

  /* First check for explicit disabling or it set elsewhere (eg NVIDIA) */
  if (getenv("__GL_SYNC_TO_VBLANK") || check_vblank_env ("none"))
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
#endif
      if (!(flags & CLUTTER_FEATURE_SYNC_TO_VBLANK))
        {
          CLUTTER_NOTE (BACKEND,
                        "no use-able vblank mechanism found");
        }
    }

  CLUTTER_NOTE (MISC, "backend features checked");

  return flags;
}

static void
clutter_backend_glx_redraw (ClutterBackend *backend)
{
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend);
  ClutterStageGLX   *stage_glx;

  stage_glx = CLUTTER_STAGE_GLX(backend_glx->stage);

  clutter_actor_paint (CLUTTER_ACTOR (stage_glx));

  /* Why this paint is done in backend as likely GL windowing system
   * specific calls, like swapping buffers.
  */
  if (stage_glx->xwin)
    {
      clutter_backend_glx_wait_for_vblank (stage_glx->backend);
      glXSwapBuffers (stage_glx->xdpy, stage_glx->xwin);
    }
  else
    {
      /* offscreen */
      glXWaitGL ();
      CLUTTER_GLERR ();
    }

}

static void
clutter_backend_glx_class_init (ClutterBackendGLXClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_glx_constructor;
  gobject_class->dispose = clutter_backend_glx_dispose;
  gobject_class->finalize = clutter_backend_glx_finalize;

  backend_class->pre_parse   = clutter_backend_glx_pre_parse;
  backend_class->post_parse   = clutter_backend_glx_post_parse;
  backend_class->init_stage   = clutter_backend_glx_init_stage;
  backend_class->init_events  = clutter_backend_glx_init_events;
  backend_class->get_stage    = clutter_backend_glx_get_stage;
  backend_class->add_options  = clutter_backend_glx_add_options;
  backend_class->get_features = clutter_backend_glx_get_features;
  backend_class->redraw       = clutter_backend_glx_redraw;
}

static void
clutter_backend_glx_init (ClutterBackendGLX *backend_glx)
{
  ClutterBackend *backend = CLUTTER_BACKEND (backend_glx);

  /* FIXME: get from xsettings */
  clutter_backend_set_double_click_time (backend, 250);
  clutter_backend_set_double_click_distance (backend, 5);
  clutter_backend_set_resolution (backend, 96.0);
}

/* every backend must implement this function */
GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_glx_get_type ();
}

static int
error_handler(Display     *xdpy,
	      XErrorEvent *error)
{
  TrappedErrorCode = error->error_code;
  return 0;
}

void
clutter_backend_glx_wait_for_vblank (ClutterBackendGLX *backend_glx)
{
  switch (backend_glx->vblank_type)
    {
    case CLUTTER_VBLANK_GLX_SWAP:
      {
	/* Nothing */
	break;
      }
    case CLUTTER_VBLANK_GLX:
      {
	unsigned int retraceCount;
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


/**
 * clutter_glx_trap_x_errors:
 *
 * FIXME
 *
 * Since: 0.4
 */
void
clutter_glx_trap_x_errors (void)
{
  TrappedErrorCode  = 0;
  old_error_handler = XSetErrorHandler (error_handler);
}

/**
 * clutter_glx_untrap_x_errors:
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.4
 */
gint
clutter_glx_untrap_x_errors (void)
{
  XSetErrorHandler (old_error_handler);

  return TrappedErrorCode;
}

/**
 * clutter_glx_get_default_display:
 * 
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.4
 */
Display *
clutter_glx_get_default_display (void)
{
  if (!backend_singleton)
    {
      g_critical ("GLX backend has not been initialised");
      return NULL;
    }

  return backend_singleton->xdpy;
}

/**
 * clutter_glx_get_default_screen:
 * 
 * Gets the pointer to the default X Screen object.
 *
 * Return value: FIXME
 *
 * Since: 0.4
 */
int
clutter_glx_get_default_screen (void)
{
  if (!backend_singleton)
    {
      g_critical ("GLX backend has not been initialised");
      return 0;
    }

  return backend_singleton->xscreen_num;
}

/**
 * clutter_glx_get_root_window:
 * 
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.4
 */
Window
clutter_glx_get_root_window (void)
{
  if (!backend_singleton)
    {
      g_critical ("GLX backend has not been initialised");
      return None;
    }

  return backend_singleton->xwin_root;
}

/**
 * clutter_glx_add_filter:
 * 
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.4
 */
void
clutter_glx_add_filter (ClutterGLXFilterFunc func, gpointer data)
{
  ClutterGLXEventFilter *filter;

  g_return_if_fail (func != NULL);

  if (!backend_singleton)
    {
      g_critical ("GLX backend has not been initialised");
      return;
    }

  filter = g_new0(ClutterGLXEventFilter, 1);
  filter->func = func;
  filter->data = data;

  backend_singleton->event_filters 
     = g_slist_append (backend_singleton->event_filters, filter);

  return;
}

/**
 * clutter_glx_remove_filter:
 * 
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.4
 */
void
clutter_glx_remove_filter (ClutterGLXFilterFunc func, gpointer data)
{
  GSList                *tmp_list, *this;
  ClutterGLXEventFilter *filter;

  g_return_if_fail (func == NULL);

  tmp_list = backend_singleton->event_filters;

  while (tmp_list)
    {
      filter = (ClutterGLXEventFilter *)tmp_list->data;
      this     =  tmp_list;
      tmp_list = tmp_list->next;

      if (filter->func == func && filter->data == data)
        {
	  backend_singleton->event_filters 
	      = g_slist_remove_link (backend_singleton->event_filters, this);

          g_slist_free_1 (this);
          g_free (filter);

          return;
        }
    }
}
