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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include <gdk/gdk.h>
#include <cogl/cogl.h>

#ifndef GDK_WINDOWING_WIN32
#include <sys/ioctl.h>
#endif

#if defined(GDK_WINDOWING_X11) && defined(COGL_HAS_XLIB_SUPPORT)
#include <cogl/cogl-xlib.h>
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#endif

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

/* global for pre init setup calls */
static GdkDisplay  *_foreign_dpy = NULL;

static gboolean disable_event_retrieval = FALSE;

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
  ClutterBackend *backend = data;
  CoglFilterReturn ret;

  ret = cogl_xlib_renderer_handle_event (backend->cogl_renderer, (XEvent *) xevent);
  switch (ret)
    {
    case COGL_FILTER_REMOVE:
      return GDK_FILTER_REMOVE;

    case COGL_FILTER_CONTINUE:
    default:
      return GDK_FILTER_CONTINUE;
    }
#endif

  return GDK_FILTER_CONTINUE;
}

static gboolean
_clutter_backend_gdk_post_parse (ClutterBackend  *backend,
                                 GError         **error)
{
  ClutterBackendGdk *backend_gdk = CLUTTER_BACKEND_GDK (backend);

  if (_foreign_dpy != NULL)
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

  backend_gdk->screen = gdk_display_get_default_screen (backend_gdk->display);

  /* add event filter for Cogl events */
  gdk_window_add_filter (NULL, cogl_gdk_filter, backend_gdk);

  clutter_backend_gdk_init_settings (backend_gdk);

  CLUTTER_NOTE (BACKEND,
                "Gdk Display '%s' opened",
                gdk_display_get_name (backend_gdk->display));

  return TRUE;
}

static void
gdk_event_handler (GdkEvent *event,
		   gpointer  user_data)
{
  clutter_gdk_handle_event (event);
}

void
_clutter_backend_gdk_events_init (ClutterBackend *backend)
{
  ClutterBackendGdk *backend_gdk = CLUTTER_BACKEND_GDK (backend);

  CLUTTER_NOTE (EVENT, "initialising the event loop");

  backend->device_manager =
    g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_GDK,
                  "backend", backend,
                  "gdk-display", backend_gdk->display,
                  NULL);

  if (!disable_event_retrieval)
    gdk_event_handler_set (gdk_event_handler, NULL, NULL);
}

static void
clutter_backend_gdk_finalize (GObject *gobject)
{
  ClutterBackendGdk *backend_gdk = CLUTTER_BACKEND_GDK (gobject);

  gdk_window_remove_filter (NULL, cogl_gdk_filter, backend_gdk);
  g_object_unref (backend_gdk->display);

  G_OBJECT_CLASS (clutter_backend_gdk_parent_class)->finalize (gobject);
}

static void
clutter_backend_gdk_dispose (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_backend_gdk_parent_class)->dispose (gobject);
}

static ClutterFeatureFlags
clutter_backend_gdk_get_features (ClutterBackend *backend)
{
  ClutterBackendClass *parent_class;

  parent_class = CLUTTER_BACKEND_CLASS (clutter_backend_gdk_parent_class);

  return parent_class->get_features (backend)
        | CLUTTER_FEATURE_STAGE_USER_RESIZE
        | CLUTTER_FEATURE_STAGE_CURSOR;
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

static CoglRenderer *
clutter_backend_gdk_get_renderer (ClutterBackend  *backend,
                                  GError         **error)
{
  ClutterBackendGdk *backend_gdk = CLUTTER_BACKEND_GDK (backend);
  CoglRenderer *renderer = cogl_renderer_new ();

#if defined(GDK_WINDOWING_X11) && defined(COGL_HAS_XLIB_SUPPORT)
  if (GDK_IS_X11_DISPLAY (backend_gdk->display))
    {
      Display *xdisplay = gdk_x11_display_get_xdisplay (backend_gdk->display);

      cogl_xlib_renderer_set_foreign_display (renderer, xdisplay);
    }
  else
#endif
#if defined(GDK_WINDOWING_WIN32)
  if (GDK_IS_WIN32_DISPLAY (backend_gdk->display))
    {
      /* Force a WGL winsys on windows */
      cogl_renderer_set_winsys_id (renderer, COGL_WINSYS_ID_WGL);
    }
  else
#endif
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   _("Could not find a suitable CoglWinsys for a GdkDisplay of type %s"),
                   G_OBJECT_TYPE_NAME (backend_gdk->display));
      cogl_object_unref (renderer);

      return NULL;
    }

  return renderer;
}

static CoglDisplay *
clutter_backend_gdk_get_display (ClutterBackend  *backend,
                                 CoglRenderer    *renderer,
                                 CoglSwapChain   *swap_chain,
                                 GError         **error)
{
  ClutterBackendGdk *backend_gdk = CLUTTER_BACKEND_GDK (backend);
  CoglOnscreenTemplate *onscreen_template;
  GError *internal_error = NULL;
  CoglDisplay *display;
  gboolean has_rgba_visual;
  gboolean res;

  has_rgba_visual = gdk_screen_get_rgba_visual (backend_gdk->screen) == NULL;

  CLUTTER_NOTE (BACKEND, "Alpha on Cogl swap chain: %s",
                has_rgba_visual ? "enabled" : "disabled");

  cogl_swap_chain_set_has_alpha (swap_chain, has_rgba_visual);

  onscreen_template = cogl_onscreen_template_new (swap_chain);

  res = cogl_renderer_check_onscreen_template (renderer,
                                               onscreen_template,
                                               &internal_error);
  if (!res && has_rgba_visual)
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

static void
clutter_backend_gdk_class_init (ClutterBackendGdkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->dispose = clutter_backend_gdk_dispose;
  gobject_class->finalize = clutter_backend_gdk_finalize;

  backend_class->stage_window_type = CLUTTER_TYPE_STAGE_GDK;

  backend_class->post_parse = _clutter_backend_gdk_post_parse;

  backend_class->get_features = clutter_backend_gdk_get_features;
  backend_class->copy_event_data = clutter_backend_gdk_copy_event_data;
  backend_class->free_event_data = clutter_backend_gdk_free_event_data;

  backend_class->get_renderer = clutter_backend_gdk_get_renderer;
  backend_class->get_display = clutter_backend_gdk_get_display;
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
  ClutterBackend *backend = clutter_get_default_backend ();

  if (backend == NULL)
    {
      g_critical ("The Clutter backend has not been initialised");
      return NULL;
    }

  if (!CLUTTER_IS_BACKEND_GDK (backend))
    {
      g_critical ("The Clutter backend is not a GDK backend");
      return NULL;
    }

  return CLUTTER_BACKEND_GDK (backend)->display;
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

/**
 * clutter_gdk_disable_event_retrieval:
 *
 * Disable the event retrieval in Clutter.
 *
 * Callers of this function have to set up an event filter using the
 * GDK API, and call clutter_gdk_handle_event().
 *
 * This function should only be used when embedding Clutter into
 * a GDK based toolkit.
 *
 * Since: 1.10
 */
void
clutter_gdk_disable_event_retrieval (void)
{
  if (_clutter_context_is_initialized ())
    {
      g_warning ("%s() can only be used before calling clutter_init()",
                 G_STRFUNC);
      return;
    }

  disable_event_retrieval = TRUE;
}
