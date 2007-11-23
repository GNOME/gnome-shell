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

#include "clutter-backend-x11.h"
#include "clutter-stage-x11.h"
#include "clutter-x11.h"

#include "../clutter-event.h"
#include "../clutter-main.h"
#include "../clutter-debug.h"
#include "../clutter-private.h"

#include "cogl.h"

G_DEFINE_TYPE (ClutterBackendX11, clutter_backend_x11, CLUTTER_TYPE_BACKEND);

/* atoms; remember to add the code that assigns the atom value to
 * the member of the ClutterBackendX11 structure if you add an
 * atom name here. do not change the order!
 */
static const gchar *atom_names[] = {
  "_NET_WM_PING",
  "_NET_WM_STATE",
  "_NET_WM_STATE_FULLSCREEN",
  "_NET_WM_USER_TIME",
  "WM_PROTOCOLS",
  "WM_DELETE_WINDOW",
  "_XEMBED",
  "_XEMBED_INFO",
  "_NET_WM_NAME",
  "UTF8_STRING",
};

static const guint n_atom_names = G_N_ELEMENTS (atom_names);

/* singleton object */
static ClutterBackendX11 *backend_singleton = NULL;

/* options */
static gchar *clutter_display_name = NULL;
static gint clutter_screen = 0;
static gboolean clutter_synchronise = FALSE;

/* X error trap */
static int TrappedErrorCode = 0;
static int (* old_error_handler) (Display *, XErrorEvent *);

gboolean
clutter_backend_x11_pre_parse (ClutterBackend  *backend,
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

gboolean
clutter_backend_x11_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (clutter_display_name)
    {
      CLUTTER_NOTE (BACKEND, "XOpenDisplay on `%s'", clutter_display_name);
      backend_x11->xdpy = XOpenDisplay (clutter_display_name);
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

  if (backend_x11->xdpy)
    {
      Atom atoms[n_atom_names];
      double dpi;

      CLUTTER_NOTE (BACKEND, "Getting the X screen");

      if (clutter_screen == 0)
        backend_x11->xscreen = DefaultScreenOfDisplay (backend_x11->xdpy);
      else
        backend_x11->xscreen = ScreenOfDisplay (backend_x11->xdpy,
                                                clutter_screen);
      
      backend_x11->xscreen_num = XScreenNumberOfScreen (backend_x11->xscreen);

      backend_x11->xwin_root = RootWindow (backend_x11->xdpy,
                                           backend_x11->xscreen_num);
      
      backend_x11->display_name = g_strdup (clutter_display_name);

      dpi = (((double) DisplayHeight (backend_x11->xdpy, backend_x11->xscreen_num) * 25.4)
            / (double) DisplayHeightMM (backend_x11->xdpy, backend_x11->xscreen_num));

      clutter_backend_set_resolution (backend, dpi);

      if (clutter_synchronise)
        XSynchronize (backend_x11->xdpy, True);

      XInternAtoms (backend_x11->xdpy,
                    (char **) atom_names, n_atom_names,
                    False, atoms);

      backend_x11->atom_NET_WM_PING = atoms[0];
      backend_x11->atom_NET_WM_STATE = atoms[1];
      backend_x11->atom_NET_WM_STATE_FULLSCREEN = atoms[2];
      backend_x11->atom_NET_WM_USER_TIME = atoms[3];
      backend_x11->atom_WM_PROTOCOLS = atoms[4];
      backend_x11->atom_WM_DELETE_WINDOW = atoms[5];
      backend_x11->atom_XEMBED = atoms[6];
      backend_x11->atom_XEMBED_INFO = atoms[7];
      backend_x11->atom_NET_WM_NAME = atoms[8];
      backend_x11->atom_UTF8_STRING = atoms[9];
    }

  g_free (clutter_display_name);
  
  CLUTTER_NOTE (BACKEND,
                "X Display `%s'[%p] opened (screen:%d, root:%u, dpi:%f)",
                backend_x11->display_name,
                backend_x11->xdpy,
                backend_x11->xscreen_num,
                (unsigned int) backend_x11->xwin_root,
                clutter_backend_get_resolution (backend));

  return TRUE;
}


static void
clutter_backend_x11_init_events (ClutterBackend *backend)
{
  CLUTTER_NOTE (EVENT, "initialising the event loop");

  _clutter_backend_x11_events_init (backend);
}

ClutterActor *
clutter_backend_x11_get_stage (ClutterBackend *backend)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);

  return backend_x11->stage;
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
  { "synch", 0,
    0,
    G_OPTION_ARG_NONE, &clutter_synchronise,
    "Make X calls synchronous", NULL,
  },
  { NULL }
};

void
clutter_backend_x11_add_options (ClutterBackend *backend,
                                 GOptionGroup   *group)
{
  g_option_group_add_entries (group, entries);
}

static void
clutter_backend_x11_finalize (GObject *gobject)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (gobject);

  g_free (backend_x11->display_name);

  XCloseDisplay (backend_x11->xdpy);

  if (backend_singleton)
    backend_singleton = NULL;

  G_OBJECT_CLASS (clutter_backend_x11_parent_class)->finalize (gobject);
}

static void
clutter_backend_x11_dispose (GObject *gobject)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (gobject);

  if (backend_x11->stage)
    {
      CLUTTER_NOTE (BACKEND, "Disposing the main stage");
      
      /* we unset the private flag on the stage so we can safely
       * destroy it without a warning from clutter_actor_destroy()
       */
      CLUTTER_UNSET_PRIVATE_FLAGS (backend_x11->stage,
                                   CLUTTER_ACTOR_IS_TOPLEVEL);
      clutter_actor_destroy (backend_x11->stage);
      backend_x11->stage = NULL;
    }
 
  CLUTTER_NOTE (BACKEND, "Removing the event source");
  _clutter_backend_x11_events_uninit (CLUTTER_BACKEND (backend_x11));

  G_OBJECT_CLASS (clutter_backend_x11_parent_class)->dispose (gobject);
}

static GObject *
clutter_backend_x11_constructor (GType                  gtype,
                                 guint                  n_params,
                                 GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  GObject *retval;

  if (!backend_singleton)
    {
      parent_class = G_OBJECT_CLASS (clutter_backend_x11_parent_class);
      retval = parent_class->constructor (gtype, n_params, params);

      backend_singleton = CLUTTER_BACKEND_X11 (retval);

      return retval;
    }

  g_warning ("Attempting to create a new backend object. This should "
             "never happen, so we return the singleton instance.");
  
  return g_object_ref (backend_singleton);
}

ClutterFeatureFlags
clutter_backend_x11_get_features (ClutterBackend *backend)
{
  ClutterFeatureFlags flags = 0;

  /* FIXME: we really need to check if gl context is set */

  flags = CLUTTER_FEATURE_STAGE_USER_RESIZE|CLUTTER_FEATURE_STAGE_CURSOR;

  return flags;
}

static void
clutter_backend_x11_class_init (ClutterBackendX11Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_x11_constructor;
  gobject_class->dispose = clutter_backend_x11_dispose;
  gobject_class->finalize = clutter_backend_x11_finalize;

  backend_class->pre_parse   = clutter_backend_x11_pre_parse;
  backend_class->post_parse   = clutter_backend_x11_post_parse;
  backend_class->init_events  = clutter_backend_x11_init_events;
  backend_class->get_stage    = clutter_backend_x11_get_stage;
  backend_class->add_options  = clutter_backend_x11_add_options;
  backend_class->get_features = clutter_backend_x11_get_features;
}

static void
clutter_backend_x11_init (ClutterBackendX11 *backend_x11)
{
  ClutterBackend *backend = CLUTTER_BACKEND (backend_x11);

  /* FIXME: get from xsettings */
  clutter_backend_set_double_click_time (backend, 250);
  clutter_backend_set_double_click_distance (backend, 5);
  clutter_backend_set_resolution (backend, 96.0);
}

static int
error_handler(Display     *xdpy,
	      XErrorEvent *error)
{
  TrappedErrorCode = error->error_code;
  return 0;
}

/**
 * clutter_x11_trap_x_errors:
 *
 * Traps every X error until clutter_x11_untrap_x_errors() is called.
 *
 * Since: 0.6
 */
void
clutter_x11_trap_x_errors (void)
{
  TrappedErrorCode  = 0;
  old_error_handler = XSetErrorHandler (error_handler);
}

/**
 * clutter_x11_untrap_x_errors:
 *
 * Removes the X error trap and returns the current status.
 *
 * Return value: the trapped error code, or 0 for success
 *
 * Since: 0.4
 */
gint
clutter_x11_untrap_x_errors (void)
{
  XSetErrorHandler (old_error_handler);

  return TrappedErrorCode;
}

/**
 * clutter_x11_get_default_display:
 * 
 * Retrieves the pointer to the default display.
 *
 * Return value: the default display
 *
 * Since: 0.6
 */
Display *
clutter_x11_get_default_display (void)
{
  if (!backend_singleton)
    {
      g_critical ("X11 backend has not been initialised");
      return NULL;
    }

  return backend_singleton->xdpy;
}

/**
 * clutter_x11_get_default_screen:
 * 
 * Gets the number of the default X Screen object.
 *
 * Return value: the number of the default screen
 *
 * Since: 0.6
 */
int
clutter_x11_get_default_screen (void)
{
  if (!backend_singleton)
    {
      g_critical ("X11 backend has not been initialised");
      return 0;
    }

  return backend_singleton->xscreen_num;
}

/**
 * clutter_x11_get_root_window:
 * 
 * Retrieves the root window.
 *
 * Return value: the id of the root window
 *
 * Since: 0.6
 */
Window
clutter_x11_get_root_window (void)
{
  if (!backend_singleton)
    {
      g_critical ("X11 backend has not been initialised");
      return None;
    }

  return backend_singleton->xwin_root;
}

/**
 * clutter_x11_add_filter:
 * @func: a filter function
 * @data: user data to be passed to the filter function, or %NULL
 *
 * Adds an event filter function.
 *
 * Since: 0.6
 */
void
clutter_x11_add_filter (ClutterX11FilterFunc func,
                        gpointer             data)
{
  ClutterX11EventFilter *filter;

  g_return_if_fail (func != NULL);

  if (!backend_singleton)
    {
      g_critical ("X11 backend has not been initialised");
      return;
    }

  filter = g_new0 (ClutterX11EventFilter, 1);
  filter->func = func;
  filter->data = data;

  backend_singleton->event_filters =
    g_slist_append (backend_singleton->event_filters, filter);

  return;
}

/**
 * clutter_x11_remove_filter:
 * @func: a filter function
 * @data: user data to be passed to the filter function, or %NULL
 *
 * Removes the given filter function.
 *
 * Since: 0.6
 */
void
clutter_x11_remove_filter (ClutterX11FilterFunc func,
                           gpointer             data)
{
  GSList                *tmp_list, *this;
  ClutterX11EventFilter *filter;

  g_return_if_fail (func == NULL);

  tmp_list = backend_singleton->event_filters;

  while (tmp_list)
    {
      filter   = tmp_list->data;
      this     =  tmp_list;
      tmp_list = tmp_list->next;

      if (filter->func == func && filter->data == data)
        {
	  backend_singleton->event_filters =
	    g_slist_remove_link (backend_singleton->event_filters, this);

          g_slist_free_1 (this);
          g_free (filter);

          return;
        }
    }
}
