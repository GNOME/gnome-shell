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

#include "clutter-backend-glx.h"
#include "clutter-stage-glx.h"

#include "../clutter-event.h"
#include "../clutter-main.h"
#include "../clutter-debug.h"
#include "../clutter-private.h"

G_DEFINE_TYPE (ClutterBackendGlx, clutter_backend_glx, CLUTTER_TYPE_BACKEND);

/* singleton object */
static ClutterBackendGlx *backend_singleton = NULL;

/* options */
static gchar *clutter_display_name = NULL;
static gint clutter_screen = 0;

/* X error trap */
static int TrappedErrorCode = 0;
static int (* old_error_handler) (Display *, XErrorEvent *);

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

  return TRUE;
}

static gboolean
clutter_backend_glx_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendGlx *backend_glx = CLUTTER_BACKEND_GLX (backend);

  if (clutter_display_name)
    {
      CLUTTER_NOTE (MISC, "XOpenDisplay on `%s'", clutter_display_name);
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
      CLUTTER_NOTE (MISC, "Getting the X screen");

      if (clutter_screen == 0)
        backend_glx->xscreen = DefaultScreen (backend_glx->xdpy);
      else
        {
          Screen *xscreen;

          xscreen = ScreenOfDisplay (backend_glx->xdpy, clutter_screen);
          backend_glx->xscreen = XScreenNumberOfScreen (xscreen);
        }

      backend_glx->xwin_root = RootWindow (backend_glx->xdpy,
                                           backend_glx->xscreen);
      
      backend_glx->display_name = g_strdup (clutter_display_name);
    }

  g_free (clutter_display_name);
  
  CLUTTER_NOTE (MISC, "X Display `%s' [%p] opened (screen:%d, root:%u)",
                backend_glx->display_name,
                backend_glx->xdpy,
                backend_glx->xscreen,
                (unsigned int) backend_glx->xwin_root);

  return TRUE;
}

static gboolean 
is_gl_version_at_least_12 (void)
{     
#define NON_VENDOR_VERSION_MAX_LEN 32
  gchar non_vendor_version[NON_VENDOR_VERSION_MAX_LEN];
  const gchar *version;
  gint i = 0;

  version = (const gchar*) glGetString (GL_VERSION);

  while ( ((version[i] <= '9' && version[i] >= '0') || version[i] == '.') 
	  && i < NON_VENDOR_VERSION_MAX_LEN)
    {
      non_vendor_version[i] = version[i];
      i++;
    }

  non_vendor_version[i] = '\0';

  if (strstr (non_vendor_version, "1.0") == NULL &&
      strstr (non_vendor_version, "1.0") == NULL)
    return TRUE;

  return FALSE;
}


static gboolean
clutter_backend_glx_init_stage (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendGlx *backend_glx = CLUTTER_BACKEND_GLX (backend);

  if (!backend_glx->stage)
    {
      ClutterStageGlx *stage_glx;
      ClutterActor *stage;

      stage = g_object_new (CLUTTER_TYPE_STAGE_GLX, NULL);

      /* copy backend data into the stage */
      stage_glx = CLUTTER_STAGE_GLX (stage);
      stage_glx->xdpy = backend_glx->xdpy;
      stage_glx->xwin_root = backend_glx->xwin_root;
      stage_glx->xscreen = backend_glx->xscreen;

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

  /* At least GL 1.2 is needed for CLAMP_TO_EDGE */
  if (!is_gl_version_at_least_12 ())
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Clutter needs at least version 1.2 of OpenGL");
      return FALSE;
    }

  return TRUE;
}

static void
clutter_backend_glx_init_events (ClutterBackend *backend)
{
  CLUTTER_NOTE (EVENT, "initialising the event loop");
  _clutter_events_init (backend);
}

static ClutterActor *
clutter_backend_glx_get_stage (ClutterBackend *backend)
{
  ClutterBackendGlx *backend_glx = CLUTTER_BACKEND_GLX (backend);

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
  ClutterBackendGlx *backend_glx = CLUTTER_BACKEND_GLX (gobject);

  g_free (backend_glx->display_name);

  XCloseDisplay (backend_glx->xdpy);

  if (backend_singleton)
    backend_singleton = NULL;

  G_OBJECT_CLASS (clutter_backend_glx_parent_class)->finalize (gobject);
}

static void
clutter_backend_glx_dispose (GObject *gobject)
{
  ClutterBackendGlx *backend_glx = CLUTTER_BACKEND_GLX (gobject);

  _clutter_events_uninit (CLUTTER_BACKEND (backend_glx));

  if (backend_glx->stage)
    {
      g_object_unref (backend_glx->stage);
      backend_glx->stage = NULL;
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

static void
clutter_backend_glx_class_init (ClutterBackendGlxClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_glx_constructor;
  gobject_class->dispose = clutter_backend_glx_dispose;
  gobject_class->finalize = clutter_backend_glx_finalize;

  backend_class->pre_parse = clutter_backend_glx_pre_parse;
  backend_class->post_parse = clutter_backend_glx_post_parse;
  backend_class->init_stage = clutter_backend_glx_init_stage;
  backend_class->init_events = clutter_backend_glx_init_events;
  backend_class->get_stage = clutter_backend_glx_get_stage;
  backend_class->add_options = clutter_backend_glx_add_options;
}

static void
clutter_backend_glx_init (ClutterBackendGlx *backend_glx)
{
  ClutterBackend *backend = CLUTTER_BACKEND (backend_glx);
  backend->events_queue = g_queue_new ();

  backend->button_click_time[0] = backend->button_click_time[1] = 0;
  backend->button_number[0] = backend->button_number[1] = -1;
  backend->button_x[0] = backend->button_x[1] = 0;
  backend->button_y[0] = backend->button_y[1] = 0;

  /* FIXME - find a way to set this stuff from XSettings */
  backend->double_click_time = 250;
  backend->double_click_distance = 5;
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
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.4
 */
gint
clutter_glx_get_default_screen (void)
{
  if (!backend_singleton)
    {
      g_critical ("GLX backend has not been initialised");
      return -1;
    }

  return backend_singleton->xscreen;
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


