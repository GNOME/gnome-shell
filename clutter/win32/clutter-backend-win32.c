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

#include <windows.h>

#include "clutter-backend-win32.h"
#include "clutter-stage-win32.h"
#include "clutter-win32.h"

#include "../clutter-event.h"
#include "../clutter-main.h"
#include "../clutter-debug.h"
#include "../clutter-private.h"

#include "cogl.h"

G_DEFINE_TYPE (ClutterBackendWin32, clutter_backend_win32,
	       CLUTTER_TYPE_BACKEND);

typedef int (* SwapIntervalProc) (int interval);

/* singleton object */
static ClutterBackendWin32 *backend_singleton = NULL;

static gchar *clutter_vblank_name = NULL;

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
  CLUTTER_NOTE (EVENT, "initialising the event loop");

  _clutter_backend_win32_events_init (backend);
}

ClutterActor *
clutter_backend_win32_get_stage (ClutterBackend *backend)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (backend);

  return backend_win32->stage;
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

  G_OBJECT_CLASS (clutter_backend_win32_parent_class)->finalize (gobject);
}

static void
clutter_backend_win32_dispose (GObject *gobject)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (gobject);

  if (backend_win32->stage)
    {
      CLUTTER_NOTE (BACKEND, "Disposing the main stage");

      /* we unset the private flag on the stage so we can safely
       * destroy it without a warning from clutter_actor_destroy()
       */
      CLUTTER_UNSET_PRIVATE_FLAGS (backend_win32->stage,
                                   CLUTTER_ACTOR_IS_TOPLEVEL);
      clutter_actor_destroy (backend_win32->stage);
      backend_win32->stage = NULL;
    }

  CLUTTER_NOTE (BACKEND, "Removing the event source");
  _clutter_backend_win32_events_uninit (CLUTTER_BACKEND (backend_win32));

  G_OBJECT_CLASS (clutter_backend_win32_parent_class)->dispose (gobject);
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

static gboolean
check_vblank_env (const char *name)
{
  return clutter_vblank_name && !strcasecmp (clutter_vblank_name, name);
}

ClutterFeatureFlags
clutter_backend_win32_get_features (ClutterBackend *backend)
{
  ClutterFeatureFlags flags;
  const gchar *extensions;
  SwapIntervalProc swap_interval;

  /* FIXME: we really need to check if gl context is set */

  extensions = glGetString (GL_EXTENSIONS);

  CLUTTER_NOTE (BACKEND, "Checking features\n"
                "GL_VENDOR: %s\n"
                "GL_RENDERER: %s\n"
                "GL_VERSION: %s\n"
                "GL_EXTENSIONS: %s\n",
                glGetString (GL_VENDOR),
                glGetString (GL_RENDERER),
                glGetString (GL_VERSION),
                extensions);

  flags = CLUTTER_FEATURE_STAGE_USER_RESIZE | CLUTTER_FEATURE_STAGE_CURSOR;

  /* If the VBlank should be left at the default or it has been
     disabled elsewhere (eg NVIDIA) then don't bother trying to check
     for the swap control extension */
  if (getenv ("__GL_SYNC_TO_VBLANK") || check_vblank_env ("default"))
    CLUTTER_NOTE (BACKEND, "vblank sync: left at default at user request");
  else if (cogl_check_extension ("WGL_EXT_swap_control", extensions)
	   && (swap_interval = (SwapIntervalProc)
	       cogl_get_proc_address ("wglSwapIntervalEXT")))
    {
      /* According to the specification for the WGL_EXT_swap_control
	 extension the default swap interval is 1 anyway, so if no
	 vblank is requested then we should explicitly set it to
	 zero */
      if (check_vblank_env ("none"))
	{
	  if (swap_interval (0))
	    CLUTTER_NOTE (BACKEND, "vblank sync: successfully disabled");
	  else
	    CLUTTER_NOTE (BACKEND, "vblank sync: disabling failed");
	}
      else
	{
	  if (swap_interval (1))
	    {
	      flags |= CLUTTER_FEATURE_SYNC_TO_VBLANK;
	      CLUTTER_NOTE (BACKEND, "vblank sync: wglSwapIntervalEXT "
			    "vblank setup success");
	    }
	  else
	    CLUTTER_NOTE (BACKEND, "vblank sync: wglSwapIntervalEXT "
			  "vblank setup failed");
	}
    }
  else
    CLUTTER_NOTE (BACKEND, "no use-able vblank mechanism found");

  return flags;
}

static void
clutter_backend_win32_redraw (ClutterBackend *backend)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (backend);
  ClutterStageWin32 *stage_win32;

  stage_win32 = CLUTTER_STAGE_WIN32 (backend_win32->stage);

  clutter_actor_paint (CLUTTER_ACTOR (stage_win32));

  if (stage_win32->client_dc)
    SwapBuffers (stage_win32->client_dc);
}

static gboolean
clutter_backend_win32_init_stage (ClutterBackend  *backend,
				  GError         **error)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (backend);

  if (!backend_win32->stage)
    {
      ClutterStageWin32 *stage_win32;
      ClutterActor *stage;

      stage = g_object_new (CLUTTER_TYPE_STAGE_WIN32, NULL);

      /* copy backend data into the stage */
      stage_win32 = CLUTTER_STAGE_WIN32 (stage);
      stage_win32->backend = backend_win32;

      g_object_set_data (G_OBJECT (stage), "clutter-backend", backend);

      backend_win32->stage = g_object_ref_sink (stage);
    }

  clutter_actor_realize (backend_win32->stage);

  if (!CLUTTER_ACTOR_IS_REALIZED (backend_win32->stage))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_INTERNAL,
                   "Unable to realize the main stage");
      return FALSE;
    }

  return TRUE;
}

static void
clutter_backend_win32_class_init (ClutterBackendWin32Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_win32_constructor;
  gobject_class->dispose = clutter_backend_win32_dispose;
  gobject_class->finalize = clutter_backend_win32_finalize;

  backend_class->pre_parse    = clutter_backend_win32_pre_parse;
  backend_class->init_events  = clutter_backend_win32_init_events;
  backend_class->init_stage   = clutter_backend_win32_init_stage;
  backend_class->get_stage    = clutter_backend_win32_get_stage;
  backend_class->add_options  = clutter_backend_win32_add_options;
  backend_class->get_features = clutter_backend_win32_get_features;
  backend_class->redraw       = clutter_backend_win32_redraw;
}

static void
clutter_backend_win32_init (ClutterBackendWin32 *backend_win32)
{
  ClutterBackend *backend = CLUTTER_BACKEND (backend_win32);

  /* FIXME: get from GetSystemMetric? */
  clutter_backend_set_double_click_time (backend, 250);
  clutter_backend_set_double_click_distance (backend, 5);
  clutter_backend_set_resolution (backend, 96.0);
}

GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_win32_get_type ();
}
