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

#include <gdk/gdk.h>
#include <cogl/cogl.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
/* other backends not yet supported */

#include "clutter-backend-gdk.h"
#include "clutter-device-manager-gdk.h"
#include "clutter-settings-gdk.h"
#include "clutter-stage-gdk.h"
#include "clutter-gdk.h"

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-event-private.h"
#include "clutter-main.h"
#include "clutter-private.h"

#define clutter_backend_gdk_get_type _clutter_backend_gdk_get_type
G_DEFINE_TYPE (ClutterBackendGdk, clutter_backend_gdk, CLUTTER_TYPE_BACKEND);

/* singleton object */
static ClutterBackendGdk *backend_singleton = NULL;

/* global for pre init setup calls */
static GdkDisplay  *_foreign_dpy = NULL;

static void
clutter_backend_gdk_init_settings (ClutterBackendGdk *backend_gdk)
{
  ClutterSettings *settings = clutter_settings_get_default ();
  int i;

  for (i = 0; i < G_N_ELEMENTS (_clutter_settings_map); i++)
    {
      GValue val = G_VALUE_INIT;

      g_value_init (&val, CLUTTER_SETTING_TYPE(i));
      gdk_screen_get_setting (backend_gdk->screen,
			      CLUTTER_SETTING_GDK_NAME(i),
			      &val);
      g_object_set_property (G_OBJECT (settings),
			     CLUTTER_SETTING_PROPERTY(i),
			     &val);
      g_value_unset (&val);
    }
}

void
_clutter_backend_gdk_update_setting (ClutterBackendGdk *backend_gdk,
				     const gchar       *setting_name)
{
  ClutterSettings *settings = clutter_settings_get_default ();
  int i;

  for (i = 0; i < G_N_ELEMENTS (_clutter_settings_map); i++)
    {
      if (g_strcmp0 (CLUTTER_SETTING_GDK_NAME (i), setting_name) == 0)
	{
	  GValue val = G_VALUE_INIT;

	  g_value_init (&val, CLUTTER_SETTING_TYPE (i));
	  gdk_screen_get_setting (backend_gdk->screen,
				  CLUTTER_SETTING_GDK_NAME (i),
				  &val);
	  g_object_set_property (G_OBJECT (settings),
				 CLUTTER_SETTING_PROPERTY (i),
				 &val);
	  g_value_unset (&val);

	  break;
	}
    }
}

static GdkFilterReturn
cogl_gdk_filter (GdkXEvent  *xevent,
		 GdkEvent   *event,
		 gpointer    data)
{
#ifdef GDK_WINDOWING_X11
  CoglFilterReturn ret;

  ret = cogl_xlib_handle_event ((XEvent*)xevent);
  switch (ret)
    {
    case COGL_FILTER_REMOVE:
      return GDK_FILTER_REMOVE;

    case COGL_FILTER_CONTINUE:
    default:
      return GDK_FILTER_CONTINUE;
    }
#endif
}

static gboolean
_clutter_backend_gdk_pre_parse (ClutterBackend  *backend,
				GError         **error)
{
  /* nothing to do here */
  return TRUE;
}

static gboolean
_clutter_backend_gdk_post_parse (ClutterBackend  *backend,
                                 GError         **error)
{
  ClutterBackendGdk *backend_gdk = CLUTTER_BACKEND_GDK (backend);

  if (_foreign_dpy)
    backend_gdk->display = _foreign_dpy;

  /* Init Gdk, if outside code did not already */
  if (!gdk_init_check (NULL, NULL))
    return FALSE;

  /*
   * Only open connection if not already set by prior call to
   * clutter_gdk_set_display()
   */
  if (backend_gdk->display == NULL)
    backend_gdk->display = g_object_ref (gdk_display_get_default ());

  g_assert (backend_gdk->display != NULL);

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (backend_gdk->display))
    {
      /* Cogl needs to know the Xlib display connection for
	 CoglTexturePixmapX11 */
      cogl_xlib_set_display (gdk_x11_display_get_xdisplay (backend_gdk->display));
    }
#endif

  backend_gdk->screen = gdk_display_get_default_screen (backend_gdk->display);

  /* add event filter for Cogl events */
  gdk_window_add_filter (NULL, cogl_gdk_filter, NULL);

  clutter_backend_gdk_init_settings (backend_gdk);

  CLUTTER_NOTE (BACKEND,
                "Gdk Display '%s' opened",
                gdk_display_get_name (backend_gdk->display));

  return TRUE;
}


static void
clutter_backend_gdk_init_events (ClutterBackend *backend)
{
  CLUTTER_NOTE (EVENT, "initialising the event loop");

  _clutter_backend_gdk_events_init (backend);
}

static void
clutter_backend_gdk_finalize (GObject *gobject)
{
  ClutterBackendGdk *backend_gdk = CLUTTER_BACKEND_GDK (gobject);

  gdk_window_remove_filter (NULL, cogl_gdk_filter, NULL);
  g_object_unref (backend_gdk->display);

  if (backend_singleton)
    backend_singleton = NULL;

  G_OBJECT_CLASS (clutter_backend_gdk_parent_class)->finalize (gobject);
}

static void
clutter_backend_gdk_dispose (GObject *gobject)
{
  ClutterBackendGdk   *backend_gdk = CLUTTER_BACKEND_GDK (gobject);
  ClutterStageManager *stage_manager;

  CLUTTER_NOTE (BACKEND, "Disposing the of stages");
  stage_manager = clutter_stage_manager_get_default ();

  g_object_unref (stage_manager);

  CLUTTER_NOTE (BACKEND, "Removing the event source");
  _clutter_backend_gdk_events_uninit (CLUTTER_BACKEND (backend_gdk));

  G_OBJECT_CLASS (clutter_backend_gdk_parent_class)->dispose (gobject);
}

static GObject *
clutter_backend_gdk_constructor (GType                  gtype,
                                 guint                  n_params,
                                 GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  GObject *retval;

  if (backend_singleton == NULL)
    {
      parent_class = G_OBJECT_CLASS (clutter_backend_gdk_parent_class);
      retval = parent_class->constructor (gtype, n_params, params);

      backend_singleton = CLUTTER_BACKEND_GDK (retval);

      return retval;
    }

  g_critical ("Attempting to create a new backend object. This should "
	      "never happen, so we return the singleton instance.");

  return g_object_ref (backend_singleton);
}

static ClutterFeatureFlags
clutter_backend_gdk_get_features (ClutterBackend *backend)
{
  return CLUTTER_FEATURE_STAGE_USER_RESIZE | CLUTTER_FEATURE_STAGE_CURSOR;
}

static void
clutter_backend_gdk_copy_event_data (ClutterBackend     *backend,
                                     const ClutterEvent *src,
                                     ClutterEvent       *dest)
{
  GdkEvent *gdk_event;

  gdk_event = _clutter_event_get_platform_data (src);
  if (gdk_event != NULL)
    _clutter_event_set_platform_data (dest, gdk_event_copy (gdk_event));
}

static void
clutter_backend_gdk_free_event_data (ClutterBackend *backend,
                                     ClutterEvent   *event)
{
  GdkEvent *gdk_event;

  gdk_event = _clutter_event_get_platform_data (event);
  if (gdk_event != NULL)
    gdk_event_free (gdk_event);
}

static ClutterDeviceManager *
clutter_backend_gdk_get_device_manager (ClutterBackend *backend)
{
  ClutterBackendGdk *backend_gdk = CLUTTER_BACKEND_GDK (backend);

  if (G_UNLIKELY (backend_gdk->device_manager == NULL))
    {
      backend_gdk->device_manager = g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_GDK,
						  "backend", backend_gdk,
						  "gdk-display", backend_gdk->display,
						  NULL);
    }

  return backend_gdk->device_manager;
}

static void
clutter_backend_gdk_class_init (ClutterBackendGdkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_gdk_constructor;
  gobject_class->dispose = clutter_backend_gdk_dispose;
  gobject_class->finalize = clutter_backend_gdk_finalize;

  backend_class->pre_parse = _clutter_backend_gdk_pre_parse;
  backend_class->post_parse = _clutter_backend_gdk_post_parse;
  backend_class->init_events = clutter_backend_gdk_init_events;
  backend_class->get_features = clutter_backend_gdk_get_features;
  backend_class->get_device_manager = clutter_backend_gdk_get_device_manager;
  backend_class->copy_event_data = clutter_backend_gdk_copy_event_data;
  backend_class->free_event_data = clutter_backend_gdk_free_event_data;
}

static void
clutter_backend_gdk_init (ClutterBackendGdk *backend_gdk)
{
  /* nothing to do here */
}

/**
 * clutter_gdk_get_default_display:
 *
 * Retrieves the pointer to the default display.
 *
 * Return value: (transfer none): the default display
 *
 * Since: 0.6
 */
GdkDisplay *
clutter_gdk_get_default_display (void)
{
  if (!backend_singleton)
    {
      g_critical ("GDK backend has not been initialised");
      return NULL;
    }

  return backend_singleton->display;
}

/**
 * clutter_gdk_set_display:
 * @display: pointer to a GDK display connection.
 *
 * Sets the display connection Clutter should use; must be called
 * before clutter_init(), clutter_init_with_args() or other functions
 * pertaining Clutter's initialization process.
 *
 * If you are parsing the command line arguments by retrieving Clutter's
 * #GOptionGroup with clutter_get_option_group() and calling
 * g_option_context_parse() yourself, you should also call
 * clutter_gdk_set_display() before g_option_context_parse().
 *
 * Since: 0.8
 */
void
clutter_gdk_set_display (GdkDisplay *display)
{
  if (_clutter_context_is_initialized ())
    {
      g_warning ("%s() can only be used before calling clutter_init()",
                 G_STRFUNC);
      return;
    }

  _foreign_dpy = g_object_ref (display);
}
