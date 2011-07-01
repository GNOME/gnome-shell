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

G_DEFINE_TYPE (ClutterBackendWin32, clutter_backend_win32,
	       CLUTTER_TYPE_BACKEND);

typedef int (WINAPI * SwapIntervalProc) (int interval);

/* singleton object */
static ClutterBackendWin32 *backend_singleton = NULL;

static gchar *clutter_vblank_name = NULL;

static HINSTANCE clutter_hinst = NULL;

/* various flags corresponding to pre init setup calls */
static gboolean _no_event_retrieval = FALSE;

const gchar *
_clutter_backend_win32_get_vblank (void)
{
  if (clutter_vblank_name && strcmp (clutter_vblank_name, "0") == 0)
    return "none";
  else
    return clutter_vblank_name;
}

gboolean
clutter_backend_win32_pre_parse (ClutterBackend  *backend,
				 GError         **error)
{
  const gchar *env_string;

  if ((env_string = g_getenv ("CLUTTER_VBLANK")))
    clutter_vblank_name = g_strdup (env_string);

  return TRUE;
}

static void
clutter_backend_win32_init_events (ClutterBackend *backend)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (backend);

  CLUTTER_NOTE (EVENT, "initialising the event loop");

  backend_win32->device_manager =
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

static const GOptionEntry entries[] =
  {
    {
      "vblank", 0, 
      0, 
      G_OPTION_ARG_STRING, &clutter_vblank_name,
      "VBlank method to be used (none, default or wgl)", "METHOD" 
    },
    { NULL }
  };

void
clutter_backend_win32_add_options (ClutterBackend *backend,
				   GOptionGroup   *group)
{
  g_option_group_add_entries (group, entries);
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

  /* Unrealize all shaders, since the GL context is going away */
  _clutter_shader_release_all ();

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
  ClutterFeatureFlags flags;

  parent_class = CLUTTER_BACKEND_CLASS (clutter_backend_win32_parent_class);

  flags = CLUTTER_FEATURE_STAGE_USER_RESIZE | CLUTTER_FEATURE_STAGE_CURSOR;

  if (cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN))
    {
      CLUTTER_NOTE (BACKEND, "Cogl supports multiple onscreen framebuffers");
      flags |= CLUTTER_FEATURE_STAGE_MULTIPLE;
    }
  else
    {
      CLUTTER_NOTE (BACKEND, "Cogl only supports one onscreen framebuffer");
      flags |= CLUTTER_FEATURE_STAGE_STATIC;
    }

  if (cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_THROTTLE))
    {
      CLUTTER_NOTE (BACKEND, "Cogl supports swap buffers throttling");
      flags |= CLUTTER_FEATURE_SYNC_TO_VBLANK;
    }
  else
    CLUTTER_NOTE (BACKEND, "Cogl doesn't support swap buffers throttling");

  CLUTTER_NOTE (BACKEND, "backend features checked");

  return flags;
}

static gboolean
clutter_backend_win32_create_context (ClutterBackend  *backend,
                                      GError         **error)
{
  CoglSwapChain *swap_chain;
  CoglOnscreenTemplate *onscreen_template;

  if (backend->cogl_context)
    return TRUE;

  backend->cogl_renderer = cogl_renderer_new ();
  if (!cogl_renderer_connect (backend->cogl_renderer, error))
    goto error;

  swap_chain = cogl_swap_chain_new ();

  onscreen_template = cogl_onscreen_template_new (swap_chain);
  cogl_object_unref (swap_chain);

  if (!cogl_renderer_check_onscreen_template (backend->cogl_renderer,
                                              onscreen_template,
                                              error))
    goto error;

  backend->cogl_display = cogl_display_new (backend->cogl_renderer,
                                            onscreen_template);
  cogl_object_unref (backend->cogl_renderer);
  cogl_object_unref (onscreen_template);

  if (!cogl_display_setup (backend->cogl_display, error))
    goto error;

  backend->cogl_context = cogl_context_new (backend->cogl_display, error);
  if (!backend->cogl_context)
    goto error;

  return TRUE;

error:
  if (backend->cogl_display)
    {
      cogl_object_unref (backend->cogl_display);
      backend->cogl_display = NULL;
    }

  if (onscreen_template)
    cogl_object_unref (onscreen_template);
  if (swap_chain)
    cogl_object_unref (swap_chain);

  if (backend->cogl_renderer)
    {
      cogl_object_unref (backend->cogl_renderer);
      backend->cogl_renderer = NULL;
    }
  return FALSE;
}

static void
clutter_backend_win32_ensure_context (ClutterBackend *backend, 
				      ClutterStage   *stage)
{
  ClutterStageWin32 *stage_win32 =
    CLUTTER_STAGE_WIN32 (_clutter_stage_get_window (stage));

  cogl_set_framebuffer (COGL_FRAMEBUFFER (stage_win32->onscreen));
}

static ClutterStageWindow *
clutter_backend_win32_create_stage (ClutterBackend  *backend,
				    ClutterStage    *wrapper,
				    GError         **error)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (backend);
  ClutterStageWin32 *stage_win32;
  ClutterStageWindow *stage;

  stage = g_object_new (CLUTTER_TYPE_STAGE_WIN32, NULL);

  /* copy backend data into the stage */
  stage_win32 = CLUTTER_STAGE_WIN32 (stage);
  stage_win32->backend = backend_win32;
  stage_win32->wrapper = wrapper;

  return stage;
}

static ClutterDeviceManager *
clutter_backend_win32_get_device_manager (ClutterBackend *backend)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (backend);

  if (G_UNLIKELY (backend_win32->device_manager == NULL))
    {
      backend_win32->device_manager =
        g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_WIN32,
                      "backend", backend_win32,
                      NULL);
    }

  return backend_win32->device_manager;
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

static void
clutter_backend_win32_class_init (ClutterBackendWin32Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_win32_constructor;
  gobject_class->dispose = clutter_backend_win32_dispose;
  gobject_class->finalize = clutter_backend_win32_finalize;

  backend_class->pre_parse        = clutter_backend_win32_pre_parse;
  backend_class->init_events      = clutter_backend_win32_init_events;
  backend_class->create_stage     = clutter_backend_win32_create_stage;
  backend_class->add_options      = clutter_backend_win32_add_options;
  backend_class->get_features     = clutter_backend_win32_get_features;
  backend_class->create_context   = clutter_backend_win32_create_context;
  backend_class->ensure_context   = clutter_backend_win32_ensure_context;
  backend_class->get_device_manager = clutter_backend_win32_get_device_manager;
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

GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_win32_get_type ();
}

BOOL WINAPI
DllMain (HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
  if (reason == DLL_PROCESS_ATTACH)
    /* Store the module handle so that we can use it to load resources */
    clutter_hinst = hinst;

  return TRUE;
}
