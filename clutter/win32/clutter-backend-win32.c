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

#include <windows.h>

#include "clutter-backend-win32.h"
#include "clutter-stage-win32.h"
#include "clutter-win32.h"
#include "clutter-device-manager-win32.h"

#include "clutter-event.h"
#include "clutter-main.h"
#include "clutter-device-manager-private.h"
#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include "cogl/cogl.h"

/* prototype decleration for DllMain to satisfy compiler checking in 
 * maintainer mode build.
 */
BOOL WINAPI DllMain (HINSTANCE hinst, DWORD reason, LPVOID reserved);

G_DEFINE_TYPE (ClutterBackendWin32, clutter_backend_win32,
	       CLUTTER_TYPE_BACKEND);

typedef int (WINAPI * SwapIntervalProc) (int interval);

/* singleton object */
static ClutterBackendWin32 *backend_singleton = NULL;

static HINSTANCE clutter_hinst = NULL;

/* various flags corresponding to pre init setup calls */
static gboolean _no_event_retrieval = FALSE;

static void
clutter_backend_win32_init_events (ClutterBackend *backend)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (backend);

  CLUTTER_NOTE (EVENT, "initialising the event loop");

  backend->device_manager =
    g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_WIN32,
                  "backend", backend_win32,
                  NULL);

  if (!_no_event_retrieval)
    _clutter_backend_win32_events_init (backend);
}

HCURSOR
_clutter_backend_win32_get_invisible_cursor (ClutterBackend *backend)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (backend);

  if (backend_win32->invisible_cursor == NULL)
    backend_win32->invisible_cursor =
      LoadCursor (clutter_hinst, MAKEINTRESOURCE (42));

  return backend_win32->invisible_cursor;
}

static void
clutter_backend_win32_finalize (GObject *gobject)
{
  backend_singleton = NULL;

  timeEndPeriod (1);

  G_OBJECT_CLASS (clutter_backend_win32_parent_class)->finalize (gobject);
}

static void
clutter_backend_win32_dispose (GObject *gobject)
{
  ClutterBackend *backend = CLUTTER_BACKEND (gobject);
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (gobject);
  ClutterStageManager *stage_manager;

  CLUTTER_NOTE (BACKEND, "Disposing the of stages");
  stage_manager = clutter_stage_manager_get_default ();

  g_object_unref (stage_manager);

  CLUTTER_NOTE (BACKEND, "Removing the event source");
  _clutter_backend_win32_events_uninit (CLUTTER_BACKEND (backend_win32));

  G_OBJECT_CLASS (clutter_backend_win32_parent_class)->dispose (gobject);

  if (backend->cogl_context)
    {
      cogl_object_unref (backend->cogl_context);
      backend->cogl_context = NULL;
    }
}

static GObject *
clutter_backend_win32_constructor (GType                  gtype,
				   guint                  n_params,
				   GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  GObject *retval;

  if (!backend_singleton)
    {
      parent_class = G_OBJECT_CLASS (clutter_backend_win32_parent_class);
      retval = parent_class->constructor (gtype, n_params, params);

      backend_singleton = CLUTTER_BACKEND_WIN32 (retval);

      return retval;
    }

  g_warning ("Attempting to create a new backend object. This should "
             "never happen, so we return the singleton instance.");

  return g_object_ref (backend_singleton);
}

ClutterFeatureFlags
clutter_backend_win32_get_features (ClutterBackend *backend)
{
  ClutterBackendClass *parent_class;

  parent_class = CLUTTER_BACKEND_CLASS (clutter_backend_win32_parent_class);

  return parent_class->get_features (backend)
    | CLUTTER_FEATURE_STAGE_USER_RESIZE
    | CLUTTER_FEATURE_STAGE_CURSOR;
}

/**
 * clutter_win32_disable_event_retrieval
 *
 * Disables retrieval of Windows messages in the main loop. Use to
 * create event-less canvas.
 *
 * This function can only be called before calling clutter_init().
 *
 * Since: 0.8
 */
void
clutter_win32_disable_event_retrieval (void)
{
  if (_clutter_context_is_initialized ())
    {
      g_warning ("clutter_win32_disable_event_retrieval() can only be "
                 "called before clutter_init()");
      return;
    }

  _no_event_retrieval = TRUE;
}

static CoglRenderer *
clutter_backend_win32_get_renderer (ClutterBackend  *backend,
                                    GError         **error)
{
  CoglRenderer *renderer;

  CLUTTER_NOTE (BACKEND, "Creating a new WGL renderer");

  renderer = cogl_renderer_new ();
  cogl_renderer_set_winsys_id (renderer, COGL_WINSYS_ID_WGL);

  /* We don't want Cogl to install its default event handler because
   * we'll handle them manually */
  cogl_win32_renderer_set_event_retrieval_enabled (renderer, FALSE);

  return renderer;
}

static void
clutter_backend_win32_class_init (ClutterBackendWin32Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_win32_constructor;
  gobject_class->dispose = clutter_backend_win32_dispose;
  gobject_class->finalize = clutter_backend_win32_finalize;

  backend_class->stage_window_type = CLUTTER_TYPE_STAGE_WIN32;

  backend_class->init_events = clutter_backend_win32_init_events;
  backend_class->get_features = clutter_backend_win32_get_features;
  backend_class->get_renderer = clutter_backend_win32_get_renderer;
}

static void
clutter_backend_win32_init (ClutterBackendWin32 *backend_win32)
{
  backend_win32->invisible_cursor   = NULL;

  /* FIXME: get from GetSystemMetric?
  clutter_backend_set_double_click_time (backend, 250);
  clutter_backend_set_double_click_distance (backend, 5);
  clutter_backend_set_resolution (backend, 96.0);
  */

  /* Set the maximum precision for Windows time functions. Without
     this glib will not be able to sleep accurately enough to give a
     reasonable frame rate */
  timeBeginPeriod (1);
}

BOOL WINAPI
DllMain (HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
  if (reason == DLL_PROCESS_ATTACH)
    /* Store the module handle so that we can use it to load resources */
    clutter_hinst = hinst;

  return TRUE;
}
