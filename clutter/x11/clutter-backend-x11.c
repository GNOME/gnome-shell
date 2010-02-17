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

#include <glib/gi18n-lib.h>

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
#include "clutter-device-manager-x11.h"
#include "clutter-input-device-x11.h"
#include "clutter-stage-x11.h"
#include "clutter-x11.h"

#include <X11/extensions/Xcomposite.h>

#ifdef HAVE_XINPUT
#include <X11/extensions/XInput.h>
#endif

#include "cogl/cogl.h"

#include "../clutter-debug.h"
#include "../clutter-device-manager.h"
#include "../clutter-event.h"
#include "../clutter-main.h"
#include "../clutter-private.h"

G_DEFINE_TYPE (ClutterBackendX11, clutter_backend_x11, CLUTTER_TYPE_BACKEND);

/* atoms; remember to add the code that assigns the atom value to
 * the member of the ClutterBackendX11 structure if you add an
 * atom name here. do not change the order!
 */
static const gchar *atom_names[] = {
  "_NET_WM_PID",
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

/* various flags corresponding to pre init setup calls */
static gboolean _no_xevent_retrieval = FALSE;
static gboolean clutter_enable_xinput = FALSE;
static gboolean clutter_enable_argb = FALSE;
static Display  *_foreign_dpy = NULL;

/* options */
static gchar *clutter_display_name = NULL;
static gint clutter_screen = -1;
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

  env_string = g_getenv ("CLUTTER_DISABLE_ARGB_VISUAL");
  if (env_string)
    {
      clutter_enable_argb = FALSE;
      env_string = NULL;
    }

  return TRUE;
}

gboolean
clutter_backend_x11_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (_foreign_dpy)
    backend_x11->xdpy = _foreign_dpy;
  /*
   * Only open connection if not already set by prior call to
   * clutter_x11_set_display()
   */
  if (!backend_x11->xdpy)
    {
      if (clutter_display_name)
	{
	  CLUTTER_NOTE (BACKEND, "XOpenDisplay on '%s'",
			clutter_display_name);
	  backend_x11->xdpy = XOpenDisplay (clutter_display_name);
          if (backend_x11->xdpy == None)
            {
              g_set_error (error, CLUTTER_INIT_ERROR,
                           CLUTTER_INIT_ERROR_BACKEND,
                           "Unable to open display '%s'",
                           clutter_display_name);
              return FALSE;
            }
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
    }

  if (backend_x11->xdpy)
    {
      Atom atoms[n_atom_names];
      double dpi;

      CLUTTER_NOTE (BACKEND, "Getting the X screen");

      if (clutter_screen == -1)
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

      /* register input devices */
      backend_x11->device_manager =
        g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_X11,
                      "use-xinput-1", clutter_enable_xinput,
                      "backend", backend_x11,
                      NULL);

      if (clutter_synchronise)
        XSynchronize (backend_x11->xdpy, True);

      XInternAtoms (backend_x11->xdpy,
                    (char **) atom_names, n_atom_names,
                    False, atoms);

      backend_x11->atom_NET_WM_PID = atoms[0];
      backend_x11->atom_NET_WM_PING = atoms[1];
      backend_x11->atom_NET_WM_STATE = atoms[2];
      backend_x11->atom_NET_WM_STATE_FULLSCREEN = atoms[3];
      backend_x11->atom_NET_WM_USER_TIME = atoms[4];
      backend_x11->atom_WM_PROTOCOLS = atoms[5];
      backend_x11->atom_WM_DELETE_WINDOW = atoms[6];
      backend_x11->atom_XEMBED = atoms[7];
      backend_x11->atom_XEMBED_INFO = atoms[8];
      backend_x11->atom_NET_WM_NAME = atoms[9];
      backend_x11->atom_UTF8_STRING = atoms[10];
    }

  g_free (clutter_display_name);

  CLUTTER_NOTE (BACKEND,
                "X Display '%s'[%p] opened (screen:%d, root:%u, dpi:%f)",
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

  if (!_no_xevent_retrieval)
    _clutter_backend_x11_events_init (backend);
}

static const GOptionEntry entries[] =
{
  {
    "display", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_STRING, &clutter_display_name,
    N_("X display to use"), "DISPLAY"
  },
  {
    "screen", 0,
    G_OPTION_FLAG_IN_MAIN,
    G_OPTION_ARG_INT, &clutter_screen,
    N_("X screen to use"), "SCREEN"
  },
  { "synch", 0,
    0,
    G_OPTION_ARG_NONE, &clutter_synchronise,
    N_("Make X calls synchronous"), NULL
  },
#ifdef HAVE_XINPUT
  {
    "enable-xinput", 0,
    0,
    G_OPTION_ARG_NONE, &clutter_enable_xinput,
    N_("Enable XInput support"), NULL
  },
#endif /* HAVE_XINPUT */
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
  ClutterBackendX11   *backend_x11 = CLUTTER_BACKEND_X11 (gobject);
  ClutterStageManager *stage_manager;

  CLUTTER_NOTE (BACKEND, "Disposing the of stages");
  stage_manager = clutter_stage_manager_get_default ();

  /* Destroy all of the stages. g_slist_foreach is used because the
     finalizer for the stages will remove the stage from the
     stage_manager's list and g_slist_foreach has some basic
     protection against this */
  g_slist_foreach (stage_manager->stages, (GFunc) clutter_actor_destroy, NULL);

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
  return CLUTTER_FEATURE_STAGE_USER_RESIZE | CLUTTER_FEATURE_STAGE_CURSOR;
}

gboolean
clutter_backend_x11_handle_event (ClutterBackendX11 *backend_x11,
                                  XEvent *xevent)
{
  return FALSE;
}

static ClutterDeviceManager *
clutter_backend_x11_get_device_manager (ClutterBackend *backend)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (G_UNLIKELY (backend_x11->device_manager == NULL))
    {
      backend_x11->device_manager =
        g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_X11,
                      "use-xinput-1", clutter_enable_xinput,
                      "backend", backend_x11,
                      NULL);
    }

  return backend_x11->device_manager;
}

static void
clutter_backend_x11_class_init (ClutterBackendX11Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);
  ClutterBackendX11Class *backendx11_class = CLUTTER_BACKEND_X11_CLASS (klass);

  gobject_class->constructor = clutter_backend_x11_constructor;
  gobject_class->dispose = clutter_backend_x11_dispose;
  gobject_class->finalize = clutter_backend_x11_finalize;

  backend_class->pre_parse = clutter_backend_x11_pre_parse;
  backend_class->post_parse = clutter_backend_x11_post_parse;
  backend_class->init_events = clutter_backend_x11_init_events;
  backend_class->add_options = clutter_backend_x11_add_options;
  backend_class->get_features = clutter_backend_x11_get_features;
  backend_class->get_device_manager = clutter_backend_x11_get_device_manager;

  backendx11_class->handle_event = clutter_backend_x11_handle_event;
}

static void
clutter_backend_x11_init (ClutterBackendX11 *backend_x11)
{
  ClutterBackend *backend = CLUTTER_BACKEND (backend_x11);

  /* FIXME: get from xsettings */
  clutter_backend_set_double_click_time (backend, 250);
  clutter_backend_set_double_click_distance (backend, 5);
  clutter_backend_set_resolution (backend, 96.0);

  backend_x11->last_event_time = CurrentTime;
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
 * clutter_x11_set_display:
 * @xdpy: pointer to a X display connection.
 *
 * Sets the display connection Clutter should use; must be called
 * before clutter_init(), clutter_init_with_args() or other functions
 * pertaining Clutter's initialization process.
 *
 * If you are parsing the command line arguments by retrieving Clutter's
 * #GOptionGroup with clutter_get_option_group() and calling
 * g_option_context_parse() yourself, you should also call
 * clutter_x11_set_display() before g_option_context_parse().
 *
 * Since: 0.8
 */
void
clutter_x11_set_display (Display *xdpy)
{
  if (_clutter_context_is_initialized ())
    {
      g_critical ("Display connection already exists. You can only call "
		  "clutter_x11_set_display() before clutter_init()");
      return;
    }

  _foreign_dpy= xdpy;
}

/**
 * clutter_x11_enable_xinput:
 *
 * Enables the use of the XInput extension if present on connected
 * XServer and support built into Clutter. XInput allows for multiple
 * pointing devices to be used.
 *
 * This function must be called before clutter_init().
 *
 * Since XInput might not be supported by the X server, you might
 * want to use clutter_x11_has_xinput() to see if support was enabled.
 *
 * Since: 0.8
 */
void
clutter_x11_enable_xinput (void)
{
  if (_clutter_context_is_initialized ())
    {
      g_critical ("clutter_x11_enable_xinput() can only be called "
                  "before clutter_init()");
      return;
    }

  clutter_enable_xinput = TRUE;
}

/**
 * clutter_x11_disable_event_retrieval
 *
 * Disables retrieval of X events in the main loop. Use to create event-less
 * canvas or in conjunction with clutter_x11_handle_event.
 *
 * This function can only be called before calling clutter_init().
 *
 * Since: 0.8
 */
void
clutter_x11_disable_event_retrieval (void)
{
  if (_clutter_context_is_initialized ())
    {
      g_warning  ("clutter_x11_disable_event_retrieval() can only be "
                  "called before clutter_init()");
      return;
    }

  _no_xevent_retrieval = TRUE;
}

/**
 * clutter_x11_has_event_retrieval
 *
 * Queries the X11 backend to check if event collection has been disabled.
 *
 * Return value: TRUE if event retrival has been disabled. FALSE otherwise.
 *
 * Since: 0.8
 */
gboolean
clutter_x11_has_event_retrieval (void)
{
  return !_no_xevent_retrieval;
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

  g_return_if_fail (func != NULL);

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

void
_clutter_x11_select_events (Window xwin)
{
#ifdef HAVE_XINPUT
  ClutterDeviceManager *manager;
  const GSList *l;

  if (G_UNLIKELY (backend_singleton == NULL))
    {
      g_critical ("X11 backend has not been initialised");

      return;
    }

  manager = clutter_device_manager_get_default ();

  for (l = clutter_device_manager_peek_devices (manager);
       l != NULL;
       l = l->next)
  {
    ClutterInputDevice *device = l->data;

    _clutter_input_device_x11_select_events (device, backend_singleton, xwin);
  }
#endif /* HAVE_XINPUT */
}

ClutterInputDevice *
_clutter_x11_get_device_for_xid (XID id)
{
  ClutterDeviceManager *manager;

  manager = clutter_device_manager_get_default ();

  return clutter_device_manager_get_device (manager, (gint) id);
}

/**
 * clutter_x11_get_input_devices:
 *
 * Retrieves a pointer to the list of input devices
 *
 * Deprecated: 1.2: Use clutter_device_manager_peek_devices() instead
 *
 * Since: 0.8
 *
 * Return value: a pointer to the internal list of input devices; the
 *   returned list is owned by Clutter and should not be modified or
 *   freed
 */
G_CONST_RETURN GSList *
clutter_x11_get_input_devices (void)
{
  ClutterDeviceManager *manager;

  manager = clutter_device_manager_get_default ();

  return clutter_device_manager_peek_devices (manager);
}

/**
 * clutter_x11_has_xinput:
 *
 * Gets whether Clutter has XInput support.
 *
 * Return value: %TRUE if Clutter was compiled with XInput support
 *   and XInput support is available at run time.
 *
 * Since: 0.8
 */
gboolean
clutter_x11_has_xinput (void)
{
#ifdef HAVE_XINPUT
  if (backend_singleton == NULL)
    {
      g_critical ("X11 backend has not been initialised");
      return FALSE;
    }

  return backend_singleton->have_xinput;
#else
  return FALSE;
#endif
}

gboolean
clutter_x11_has_composite_extension (void)
{
  static gboolean have_composite = FALSE, done_check = FALSE;
  int error = 0, event = 0;
  Display *dpy;

  if (done_check)
    return have_composite;

  if (!backend_singleton)
    {
      g_critical ("X11 backend has not been initialised");
      return FALSE;
    }

  dpy = clutter_x11_get_default_display();

  if (XCompositeQueryExtension (dpy, &event, &error))
    {
      int major = 0, minor = 0;
      if (XCompositeQueryVersion (dpy, &major, &minor))
        {
          if (major >= 0 && minor >= 3)
            have_composite = TRUE;
        }
    }

  done_check = TRUE;

  return have_composite;
}

/**
 * clutter_x11_set_use_argb_visual:
 * @use_argb: %TRUE if ARGB visuals should be requested by default
 *
 * Sets whether the Clutter X11 backend should request ARGB visuals by default
 * or not.
 *
 * By default, Clutter requests RGB visuals.
 *
 * <note>If no ARGB visuals are found, the X11 backend will fall back to
 * requesting a RGB visual instead.</note>
 *
 * ARGB visuals are required for the #ClutterStage:use-alpha property to work.
 *
 * <note>This function can only be called once, and before clutter_init() is
 * called.</note>
 *
 * Since: 1.2
 */
void
clutter_x11_set_use_argb_visual (gboolean use_argb)
{
  if (backend_singleton != NULL)
    {
      g_warning ("%s() can only be used before calling clutter_init()",
                 G_STRFUNC);
      return;
    }

  CLUTTER_NOTE (BACKEND, "ARGB visuals are %s",
                use_argb ? "enabled" : "disabled");

  clutter_enable_argb = use_argb;
}

/**
 * clutter_x11_get_use_argb_visual:
 *
 * Retrieves whether the Clutter X11 backend is using ARGB visuals by default
 *
 * Return value: %TRUE if ARGB visuals are queried by default
 *
 * Since: 1.2
 */
gboolean
clutter_x11_get_use_argb_visual (void)
{
  return clutter_enable_argb;
}

XVisualInfo *
clutter_backend_x11_get_visual_info (ClutterBackendX11 *backend_x11)
{
  ClutterBackendX11Class *klass;

  g_return_val_if_fail (CLUTTER_IS_BACKEND_X11 (backend_x11), NULL);

  klass = CLUTTER_BACKEND_X11_GET_CLASS (backend_x11);
  if (klass->get_visual_info)
    return klass->get_visual_info (backend_x11);

  return NULL;
}

/**
 * clutter_x11_get_visual_info:
 *
 * Retrieves the <structname>XVisualInfo</structname> used by the Clutter X11
 * backend.
 *
 * Return value: a <structname>XVisualInfo</structname>, or
 *   <varname>None</varname>. The returned value should be freed using XFree()
 *   when done
 *
 * Since: 1.2
 */
XVisualInfo *
clutter_x11_get_visual_info (void)
{
  ClutterBackendX11 *backend_x11;

  backend_x11 = CLUTTER_BACKEND_X11 (clutter_get_default_backend ());

  return clutter_backend_x11_get_visual_info (backend_x11);
}
