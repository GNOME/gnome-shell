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

#include <glib/gi18n-lib.h>

#include <string.h>

#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>

#include <errno.h>

#include "clutter-backend-x11.h"
#include "clutter-device-manager-core-x11.h"
#include "clutter-device-manager-xi2.h"
#include "clutter-settings-x11.h"
#include "clutter-stage-x11.h"
#include "clutter-x11.h"

#include "xsettings/xsettings-common.h"

#if HAVE_XCOMPOSITE
#include <X11/extensions/Xcomposite.h>
#endif

#if HAVE_XINPUT_2
#include <X11/extensions/XInput2.h>
#endif

#include <cogl/cogl.h>
#include <cogl/cogl-xlib.h>

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-event-private.h"
#include "clutter-main.h"
#include "clutter-private.h"

#define clutter_backend_x11_get_type    _clutter_backend_x11_get_type

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

#define N_ATOM_NAMES G_N_ELEMENTS (atom_names)

/* various flags corresponding to pre init setup calls */
static gboolean _no_xevent_retrieval = FALSE;
static gboolean clutter_enable_xinput = TRUE;
static gboolean clutter_enable_argb = FALSE;
static Display  *_foreign_dpy = NULL;

/* options */
static gchar *clutter_display_name = NULL;
static gint clutter_screen = -1;
static gboolean clutter_synchronise = FALSE;

/* X error trap */
static int TrappedErrorCode = 0;
static int (* old_error_handler) (Display *, XErrorEvent *);

static ClutterX11FilterReturn
xsettings_filter (XEvent       *xevent,
                  ClutterEvent *event,
                  gpointer      data)
{
  ClutterBackendX11 *backend_x11 = data;

  _clutter_xsettings_client_process_event (backend_x11->xsettings, xevent);

  /* we always want the rest of the stack to get XSettings events, even
   * if Clutter already handled them
   */

  return CLUTTER_X11_FILTER_CONTINUE;
}

static ClutterX11FilterReturn
cogl_xlib_filter (XEvent       *xevent,
                  ClutterEvent *event,
                  gpointer      data)
{
  ClutterBackend *backend = data;
  ClutterX11FilterReturn retval;
  CoglFilterReturn ret;

  ret = cogl_xlib_renderer_handle_event (backend->cogl_renderer, xevent);
  switch (ret)
    {
    case COGL_FILTER_REMOVE:
      retval = CLUTTER_X11_FILTER_REMOVE;
      break;

    case COGL_FILTER_CONTINUE:
    default:
      retval = CLUTTER_X11_FILTER_CONTINUE;
      break;
    }

  return retval;
}

static void
clutter_backend_x11_xsettings_notify (const char       *name,
                                      XSettingsAction   action,
                                      XSettingsSetting *setting,
                                      void             *cb_data)
{
  ClutterSettings *settings = clutter_settings_get_default ();
  gint i;

  if (name == NULL || *name == '\0')
    return;

  if (setting == NULL)
    return;

  g_object_freeze_notify (G_OBJECT (settings));

  for (i = 0; i < _n_clutter_settings_map; i++)
    {
      if (g_strcmp0 (name, CLUTTER_SETTING_X11_NAME (i)) == 0)
        {
          GValue value = G_VALUE_INIT;

          switch (setting->type)
            {
            case XSETTINGS_TYPE_INT:
              g_value_init (&value, G_TYPE_INT);
              g_value_set_int (&value, setting->data.v_int);
              break;

            case XSETTINGS_TYPE_STRING:
              g_value_init (&value, G_TYPE_STRING);
              g_value_set_string (&value, setting->data.v_string);
              break;

            case XSETTINGS_TYPE_COLOR:
              {
                ClutterColor color;

                color.red   = (guint8) ((float) setting->data.v_color.red
                            / 65535.0 * 255);
                color.green = (guint8) ((float) setting->data.v_color.green
                            / 65535.0 * 255);
                color.blue  = (guint8) ((float) setting->data.v_color.blue
                            / 65535.0 * 255);
                color.alpha = (guint8) ((float) setting->data.v_color.alpha
                            / 65535.0 * 255);

                g_value_init (&value, G_TYPE_BOXED);
                clutter_value_set_color (&value, &color);
              }
              break;
            }

          CLUTTER_NOTE (BACKEND,
                        "Mapping XSETTING '%s' to 'ClutterSettings:%s'",
                        CLUTTER_SETTING_X11_NAME (i),
                        CLUTTER_SETTING_PROPERTY (i));

          g_object_set_property (G_OBJECT (settings),
                                 CLUTTER_SETTING_PROPERTY (i),
                                 &value);

          g_value_unset (&value);

          break;
        }
    }

  g_object_thaw_notify (G_OBJECT (settings));
}

static void
clutter_backend_x11_create_device_manager (ClutterBackendX11 *backend_x11)
{
  ClutterEventTranslator *translator;
  ClutterBackend *backend;

#ifdef HAVE_XINPUT_2
  if (clutter_enable_xinput)
    {
      int event_base, first_event, first_error;

      if (XQueryExtension (backend_x11->xdpy, "XInputExtension",
                           &event_base,
                           &first_event,
                           &first_error))
        {
          int major = 2;
          int minor = 3;

          if (XIQueryVersion (backend_x11->xdpy, &major, &minor) != BadRequest)
            {
              CLUTTER_NOTE (BACKEND, "Creating XI2 device manager");
              backend_x11->has_xinput = TRUE;
              backend_x11->device_manager =
                g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_XI2,
                              "backend", backend_x11,
                              "opcode", event_base,
                              NULL);

              backend_x11->xi_minor = minor;
            }
        }
    }

  if (backend_x11->device_manager == NULL)
#endif /* HAVE_XINPUT_2 */
    {
      CLUTTER_NOTE (BACKEND, "Creating Core device manager");
      backend_x11->has_xinput = FALSE;
      backend_x11->device_manager =
        g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_X11,
                      "backend", backend_x11,
                      NULL);

      backend_x11->xi_minor = -1;
    }

  backend = CLUTTER_BACKEND (backend_x11);
  backend->device_manager = backend_x11->device_manager;

  translator = CLUTTER_EVENT_TRANSLATOR (backend_x11->device_manager);
  _clutter_backend_add_event_translator (backend, translator);
}

static void
clutter_backend_x11_create_keymap (ClutterBackendX11 *backend_x11)
{
  if (backend_x11->keymap == NULL)
    {
      ClutterEventTranslator *translator;
      ClutterBackend *backend;

      backend_x11->keymap =
        g_object_new (CLUTTER_TYPE_KEYMAP_X11,
                      "backend", backend_x11,
                      NULL);

      backend = CLUTTER_BACKEND (backend_x11);
      translator = CLUTTER_EVENT_TRANSLATOR (backend_x11->keymap);
      _clutter_backend_add_event_translator (backend, translator);
    }
}

static gboolean
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

  env_string = g_getenv ("CLUTTER_DISABLE_XINPUT");
  if (env_string)
    {
      clutter_enable_xinput = FALSE;
      env_string = NULL;
    }

  return TRUE;
}

static gboolean
clutter_backend_x11_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  ClutterSettings *settings;
  Atom atoms[N_ATOM_NAMES];
  double dpi;

  if (_foreign_dpy)
    backend_x11->xdpy = _foreign_dpy;

  /* Only open connection if not already set by prior call to
   * clutter_x11_set_display()
   */
  if (backend_x11->xdpy == NULL)
    {
      if (clutter_display_name != NULL &&
          *clutter_display_name != '\0')
	{
	  CLUTTER_NOTE (BACKEND, "XOpenDisplay on '%s'", clutter_display_name);

	  backend_x11->xdpy = XOpenDisplay (clutter_display_name);
          if (backend_x11->xdpy == NULL)
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
	  g_set_error_literal (error, CLUTTER_INIT_ERROR,
                               CLUTTER_INIT_ERROR_BACKEND,
                               "Unable to open display. You have to set the "
                               "DISPLAY environment variable, or use the "
                               "--display command line argument");
	  return FALSE;
	}
    }

  g_assert (backend_x11->xdpy != NULL);

  CLUTTER_NOTE (BACKEND, "Getting the X screen");

  settings = clutter_settings_get_default ();

  /* add event filter for Cogl events */
  clutter_x11_add_filter (cogl_xlib_filter, backend);

  if (clutter_screen == -1)
    backend_x11->xscreen = DefaultScreenOfDisplay (backend_x11->xdpy);
  else
    backend_x11->xscreen = ScreenOfDisplay (backend_x11->xdpy,
                                            clutter_screen);

  backend_x11->xscreen_num = XScreenNumberOfScreen (backend_x11->xscreen);
  backend_x11->xscreen_width = WidthOfScreen (backend_x11->xscreen);
  backend_x11->xscreen_height = HeightOfScreen (backend_x11->xscreen);

  backend_x11->xwin_root = RootWindow (backend_x11->xdpy,
                                       backend_x11->xscreen_num);

  backend_x11->display_name = g_strdup (clutter_display_name);

  dpi = (((double) DisplayHeight (backend_x11->xdpy, backend_x11->xscreen_num) * 25.4)
      / (double) DisplayHeightMM (backend_x11->xdpy, backend_x11->xscreen_num));

  g_object_set (settings, "font-dpi", (int) dpi * 1024, NULL);

  /* create XSETTINGS client */
  backend_x11->xsettings =
    _clutter_xsettings_client_new (backend_x11->xdpy,
                                   backend_x11->xscreen_num,
                                   clutter_backend_x11_xsettings_notify,
                                   NULL,
                                   backend_x11);

  /* add event filter for XSETTINGS events */
  clutter_x11_add_filter (xsettings_filter, backend_x11);

  if (clutter_synchronise)
    XSynchronize (backend_x11->xdpy, True);

  XInternAtoms (backend_x11->xdpy,
                (char **) atom_names, N_ATOM_NAMES,
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

void
_clutter_backend_x11_events_init (ClutterBackend *backend)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);

  CLUTTER_NOTE (EVENT, "initialising the event loop");

  /* the event source is optional */
  if (!_no_xevent_retrieval)
    {
      GSource *source;

      source = _clutter_x11_event_source_new (backend_x11);

      /* default priority for events
       *
       * XXX - at some point we'll have a common EventSource API that
       * is created by the backend, and this code will most likely go
       * into the default implementation of ClutterBackend
       */
      g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);

      /* attach the source to the default context, and transfer the
       * ownership to the GMainContext itself
       */
      g_source_attach (source, NULL);
      g_source_unref (source);

      backend_x11->event_source = source;
    }

  /* create the device manager; we need this because we can effectively
   * choose between core+XI1 and XI2 input events
   */
  clutter_backend_x11_create_device_manager (backend_x11);

  /* register keymap; unless we create a generic Keymap object, I'm
   * afraid this will have to stay
   */
  clutter_backend_x11_create_keymap (backend_x11);
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
#ifdef HAVE_XINPUT_2
  {
    "disable-xinput", 0,
    G_OPTION_FLAG_REVERSE,
    G_OPTION_ARG_NONE, &clutter_enable_xinput,
    N_("Disable XInput support"), NULL
  },
#endif /* HAVE_XINPUT_2 */
  { NULL }
};

static void
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

  clutter_x11_remove_filter (cogl_xlib_filter, gobject);

  clutter_x11_remove_filter (xsettings_filter, backend_x11);
  _clutter_xsettings_client_destroy (backend_x11->xsettings);

  XCloseDisplay (backend_x11->xdpy);

  G_OBJECT_CLASS (clutter_backend_x11_parent_class)->finalize (gobject);
}

static void
clutter_backend_x11_dispose (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_backend_x11_parent_class)->dispose (gobject);
}

static ClutterFeatureFlags
clutter_backend_x11_get_features (ClutterBackend *backend)
{
  ClutterFeatureFlags flags = CLUTTER_FEATURE_STAGE_USER_RESIZE
                            | CLUTTER_FEATURE_STAGE_CURSOR;

  flags |= CLUTTER_BACKEND_CLASS (clutter_backend_x11_parent_class)->get_features (backend);

  return flags;
}

static void
clutter_backend_x11_copy_event_data (ClutterBackend     *backend,
                                     const ClutterEvent *src,
                                     ClutterEvent       *dest)
{
  gpointer event_x11;

  event_x11 = _clutter_event_get_platform_data (src);
  if (event_x11 != NULL)
    _clutter_event_set_platform_data (dest, _clutter_event_x11_copy (event_x11));
}

static void
clutter_backend_x11_free_event_data (ClutterBackend *backend,
                                     ClutterEvent   *event)
{
  gpointer event_x11;

  event_x11 = _clutter_event_get_platform_data (event);
  if (event_x11 != NULL)
    _clutter_event_x11_free (event_x11);
}

static void
update_last_event_time (ClutterBackendX11 *backend_x11,
                        XEvent            *xevent)
{
  Time current_time = CurrentTime;
  Time last_time = backend_x11->last_event_time;

  switch (xevent->type)
    {
    case KeyPress:
    case KeyRelease:
      current_time = xevent->xkey.time;
      break;

    case ButtonPress:
    case ButtonRelease:
      current_time = xevent->xbutton.time;
      break;

    case MotionNotify:
      current_time = xevent->xmotion.time;
      break;

    case EnterNotify:
    case LeaveNotify:
      current_time = xevent->xcrossing.time;
      break;

    case PropertyNotify:
      current_time = xevent->xproperty.time;
      break;

    default:
      break;
    }

  /* only change the current event time if it's after the previous event
   * time, or if it is at least 30 seconds earlier - in case the system
   * clock was changed
   */
  if ((current_time != CurrentTime) &&
      (current_time > last_time || (last_time - current_time > (30 * 1000))))
    backend_x11->last_event_time = current_time;
}

static gboolean
clutter_backend_x11_translate_event (ClutterBackend *backend,
                                     gpointer        native,
                                     ClutterEvent   *event)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  ClutterBackendClass *parent_class;
  XEvent *xevent = native;

  /* X11 filter functions have a higher priority */
  if (backend_x11->event_filters != NULL)
    {
      GSList *node = backend_x11->event_filters;

      while (node != NULL)
        {
          ClutterX11EventFilter *filter = node->data;

          switch (filter->func (xevent, event, filter->data))
            {
            case CLUTTER_X11_FILTER_CONTINUE:
              break;

            case CLUTTER_X11_FILTER_TRANSLATE:
              return TRUE;

            case CLUTTER_X11_FILTER_REMOVE:
              return FALSE;

            default:
              break;
            }

          node = node->next;
        }
    }

  /* we update the event time only for events that can
   * actually reach Clutter's event queue
   */
  update_last_event_time (backend_x11, xevent);

  /* chain up to the parent implementation, which will handle
   * event translators
   */
  parent_class = CLUTTER_BACKEND_CLASS (clutter_backend_x11_parent_class);
  return parent_class->translate_event (backend, native, event);
}

static CoglRenderer *
clutter_backend_x11_get_renderer (ClutterBackend  *backend,
                                  GError         **error)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  Display *xdisplay = backend_x11->xdpy;
  CoglRenderer *renderer;

  CLUTTER_NOTE (BACKEND, "Creating a new Xlib renderer");

  renderer = cogl_renderer_new ();

  cogl_renderer_add_constraint (renderer, COGL_RENDERER_CONSTRAINT_USES_X11);

  /* set the display object we're using */
  cogl_xlib_renderer_set_foreign_display (renderer, xdisplay);

  return renderer;
}

static CoglDisplay *
clutter_backend_x11_get_display (ClutterBackend  *backend,
                                 CoglRenderer    *renderer,
                                 CoglSwapChain   *swap_chain,
                                 GError         **error)
{
  CoglOnscreenTemplate *onscreen_template;
  GError *internal_error = NULL;
  CoglDisplay *display;
  gboolean res;

  CLUTTER_NOTE (BACKEND, "Alpha on Cogl swap chain: %s",
                clutter_enable_argb ? "enabled" : "disabled");

  cogl_swap_chain_set_has_alpha (swap_chain, clutter_enable_argb);

  onscreen_template = cogl_onscreen_template_new (swap_chain);

  res = cogl_renderer_check_onscreen_template (renderer,
                                               onscreen_template,
                                               &internal_error);
  if (!res && clutter_enable_argb)
    {
      CLUTTER_NOTE (BACKEND,
                    "Creation of a context with a ARGB visual failed: %s",
                    internal_error != NULL ? internal_error->message
                                           : "Unknown reason");

      g_clear_error (&internal_error);

      /* It's possible that the current renderer doesn't support transparency
       * in a swap_chain so lets see if we can fallback to not having any
       * transparency...
       *
       * XXX: It might be nice to have a CoglRenderer feature we could
       * explicitly check for ahead of time.
       */
      clutter_enable_argb = FALSE;
      cogl_swap_chain_set_has_alpha (swap_chain, FALSE);
      res = cogl_renderer_check_onscreen_template (renderer,
                                                   onscreen_template,
                                                   &internal_error);
    }

  if (!res)
    {
      g_set_error_literal (error, CLUTTER_INIT_ERROR,
                           CLUTTER_INIT_ERROR_BACKEND,
                           internal_error->message);

      g_error_free (internal_error);
      cogl_object_unref (onscreen_template);

      return NULL;
    }

  display = cogl_display_new (renderer, onscreen_template);
  cogl_object_unref (onscreen_template);

  return display;
}

static ClutterStageWindow *
clutter_backend_x11_create_stage (ClutterBackend  *backend,
				  ClutterStage    *wrapper,
				  GError         **error)
{
  ClutterEventTranslator *translator;
  ClutterStageWindow *stage;

  stage = g_object_new (CLUTTER_TYPE_STAGE_X11,
			"backend", backend,
			"wrapper", wrapper,
			NULL);

  /* the X11 stage does event translation */
  translator = CLUTTER_EVENT_TRANSLATOR (stage);
  _clutter_backend_add_event_translator (backend, translator);

  CLUTTER_NOTE (MISC, "X11 stage created (display:%p, screen:%d, root:%u)",
                CLUTTER_BACKEND_X11 (backend)->xdpy,
                CLUTTER_BACKEND_X11 (backend)->xscreen_num,
                (unsigned int) CLUTTER_BACKEND_X11 (backend)->xwin_root);

  return stage;
}

static PangoDirection
clutter_backend_x11_get_keymap_direction (ClutterBackend *backend)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (G_UNLIKELY (backend_x11->keymap == NULL))
    return PANGO_DIRECTION_NEUTRAL;

  return _clutter_keymap_x11_get_direction (backend_x11->keymap);
}

static void
clutter_backend_x11_class_init (ClutterBackendX11Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->dispose = clutter_backend_x11_dispose;
  gobject_class->finalize = clutter_backend_x11_finalize;

  backend_class->stage_window_type = CLUTTER_TYPE_STAGE_X11;

  backend_class->pre_parse = clutter_backend_x11_pre_parse;
  backend_class->post_parse = clutter_backend_x11_post_parse;
  backend_class->add_options = clutter_backend_x11_add_options;
  backend_class->get_features = clutter_backend_x11_get_features;

  backend_class->copy_event_data = clutter_backend_x11_copy_event_data;
  backend_class->free_event_data = clutter_backend_x11_free_event_data;
  backend_class->translate_event = clutter_backend_x11_translate_event;

  backend_class->get_renderer = clutter_backend_x11_get_renderer;
  backend_class->get_display = clutter_backend_x11_get_display;
  backend_class->create_stage = clutter_backend_x11_create_stage;

  backend_class->get_keymap_direction = clutter_backend_x11_get_keymap_direction;
}

static void
clutter_backend_x11_init (ClutterBackendX11 *backend_x11)
{
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
 * Return value: (transfer none): the default display
 *
 * Since: 0.6
 */
Display *
clutter_x11_get_default_display (void)
{
  ClutterBackend *backend = clutter_get_default_backend ();

  if (backend == NULL)
    {
      g_critical ("The Clutter backend has not been initialised");
      return NULL;
    }

  if (!CLUTTER_IS_BACKEND_X11 (backend))
    {
      g_critical ("The Clutter backend is not a X11 backend");
      return NULL;
    }

  return CLUTTER_BACKEND_X11 (backend)->xdpy;
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
      g_warning ("%s() can only be used before calling clutter_init()",
                 G_STRFUNC);
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
 *
 * Deprecated: 1.14: This function does not do anything; XInput support
 *   is enabled by default in Clutter. Use the CLUTTER_DISABLE_XINPUT
 *   environment variable to disable XInput support and use Xlib core
 *   events instead.
 */
void
clutter_x11_enable_xinput (void)
{
}

/**
 * clutter_x11_disable_event_retrieval:
 *
 * Disables the internal polling of X11 events in the main loop.
 *
 * Libraries or applications calling this function will be responsible of
 * polling all X11 events.
 *
 * You also must call clutter_x11_handle_event() to let Clutter process
 * events and maintain its internal state.
 *
 * <warning>This function can only be called before calling
 * clutter_init().</warning>
 *
 * <note>Even with event handling disabled, Clutter will still select
 * all the events required to maintain its internal state on the stage
 * Window; compositors using Clutter and input regions to pass events
 * through to application windows should not rely on an empty input
 * region, and should instead clear it themselves explicitly using the
 * XFixes extension.</note>
 *
 * This function should not be normally used by applications.
 *
 * Since: 0.8
 */
void
clutter_x11_disable_event_retrieval (void)
{
  if (_clutter_context_is_initialized ())
    {
      g_warning ("%s() can only be used before calling clutter_init()",
                 G_STRFUNC);
      return;
    }

  _no_xevent_retrieval = TRUE;
}

/**
 * clutter_x11_has_event_retrieval:
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
 ClutterBackend *backend = clutter_get_default_backend ();

  if (backend == NULL)
    {
      g_critical ("The Clutter backend has not been initialised");
      return 0;
    }

  if (!CLUTTER_IS_BACKEND_X11 (backend))
    {
      g_critical ("The Clutter backend is not a X11 backend");
      return 0;
    }

  return CLUTTER_BACKEND_X11 (backend)->xscreen_num;
}

/**
 * clutter_x11_get_root_window: (skip)
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
 ClutterBackend *backend = clutter_get_default_backend ();

  if (backend == NULL)
    {
      g_critical ("The Clutter backend has not been initialised");
      return None;
    }

  if (!CLUTTER_IS_BACKEND_X11 (backend))
    {
      g_critical ("The Clutter backend is not a X11 backend");
      return None;
    }

  return CLUTTER_BACKEND_X11 (backend)->xwin_root;
}

/**
 * clutter_x11_add_filter: (skip)
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
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;

  g_return_if_fail (func != NULL);

  if (backend == NULL)
    {
      g_critical ("The Clutter backend has not been initialised");
      return;
    }

  if (!CLUTTER_IS_BACKEND_X11 (backend))
    {
      g_critical ("The Clutter backend is not a X11 backend");
      return;
    }

  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  filter = g_new0 (ClutterX11EventFilter, 1);
  filter->func = func;
  filter->data = data;

  backend_x11->event_filters =
    g_slist_append (backend_x11->event_filters, filter);

  return;
}

/**
 * clutter_x11_remove_filter: (skip)
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
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;

  g_return_if_fail (func != NULL);

  if (backend == NULL)
    {
      g_critical ("The Clutter backend has not been initialised");
      return;
    }

  if (!CLUTTER_IS_BACKEND_X11 (backend))
    {
      g_critical ("The Clutter backend is not a X11 backend");
      return;
    }

  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  tmp_list = backend_x11->event_filters;

  while (tmp_list)
    {
      filter   = tmp_list->data;
      this     =  tmp_list;
      tmp_list = tmp_list->next;

      if (filter->func == func && filter->data == data)
        {
          backend_x11->event_filters =
            g_slist_remove_link (backend_x11->event_filters, this);

          g_slist_free_1 (this);
          g_free (filter);

          return;
        }
    }
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
 * Return value: (transfer none) (element-type Clutter.InputDevice): a
 *   pointer to the internal list of input devices; the returned list is
 *   owned by Clutter and should not be modified or freed
 */
const GSList *
clutter_x11_get_input_devices (void)
{
  ClutterDeviceManager *manager;

  manager = clutter_device_manager_get_default ();
  if (manager == NULL)
    return NULL;

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
#ifdef HAVE_XINPUT_2
 ClutterBackend *backend = clutter_get_default_backend ();

  if (backend == NULL)
    {
      g_critical ("The Clutter backend has not been initialised");
      return FALSE;
    }

  if (!CLUTTER_IS_BACKEND_X11 (backend))
    {
      g_critical ("The Clutter backend is not a X11 backend.");
      return FALSE;
    }

  return CLUTTER_BACKEND_X11 (backend)->has_xinput;
#else
  return FALSE;
#endif
}

/**
 * clutter_x11_has_composite_extension:
 *
 * Retrieves whether Clutter is running on an X11 server with the
 * XComposite extension
 *
 * Return value: %TRUE if the XComposite extension is available
 */
gboolean
clutter_x11_has_composite_extension (void)
{
#if HAVE_XCOMPOSITE
  static gboolean have_composite = FALSE, done_check = FALSE;
  int error = 0, event = 0;
  Display *dpy;

  if (done_check)
    return have_composite;

  if (!_clutter_context_is_initialized ())
    {
      g_critical ("X11 backend has not been initialised");
      return FALSE;
    }

  dpy = clutter_x11_get_default_display();
  if (dpy == NULL)
    return FALSE;

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
#else
  return FALSE;
#endif /* HAVE_XCOMPOSITE */
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
  if (_clutter_context_is_initialized ())
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
_clutter_backend_x11_get_visual_info (ClutterBackendX11 *backend_x11)
{
  return cogl_clutter_winsys_xlib_get_visual_info ();
}

/**
 * clutter_x11_get_visual_info: (skip)
 *
 * Retrieves the <structname>XVisualInfo</structname> used by the Clutter X11
 * backend.
 *
 * Return value: (transfer full): a <structname>XVisualInfo</structname>, or
 *   <varname>None</varname>. The returned value should be freed using XFree()
 *   when done
 *
 * Since: 1.2
 */
XVisualInfo *
clutter_x11_get_visual_info (void)
{
  ClutterBackendX11 *backend_x11;
  ClutterBackend *backend;

  backend = clutter_get_default_backend ();
  if (!CLUTTER_IS_BACKEND_X11 (backend))
    {
      g_critical ("The Clutter backend is not a X11 backend.");
      return NULL;
    }

  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  return _clutter_backend_x11_get_visual_info (backend_x11);
}

gboolean
_clutter_x11_input_device_translate_screen_coord (ClutterInputDevice *device,
                                                  gint                stage_root_x,
                                                  gint                stage_root_y,
                                                  guint               index_,
                                                  gdouble             value,
                                                  gdouble            *axis_value)
{
  ClutterAxisInfo *info;
  ClutterBackendX11 *backend_x11;
  gdouble width, scale, offset;
  
  backend_x11 = CLUTTER_BACKEND_X11 (device->backend);

  if (device->axes == NULL || index_ >= device->axes->len)
    return FALSE;

  info = &g_array_index (device->axes, ClutterAxisInfo, index_);
  if (!(info->axis == CLUTTER_INPUT_AXIS_X || info->axis == CLUTTER_INPUT_AXIS_Y))
    return FALSE;

  width = info->max_value - info->min_value;

  if (info->axis == CLUTTER_INPUT_AXIS_X)
    {
      if (width > 0)
        scale = backend_x11->xscreen_width / width;
      else
        scale = 1;

      offset = - stage_root_x;
    }
  else
    {
      if (width > 0)
        scale = backend_x11->xscreen_height / width;
      else
        scale = 1;

      offset = - stage_root_y;
    }

  if (axis_value)
    *axis_value = offset + scale * (value - info->min_value);

  return TRUE;
}
