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

#include "../clutter-event.h"
#include "../clutter-main.h"
#include "../clutter-input-device.h"
#include "../clutter-debug.h"
#include "../clutter-private.h"
#include "../clutter-version.h"

#include "cogl/cogl.h"

G_DEFINE_TYPE (ClutterBackendWin32, clutter_backend_win32,
	       CLUTTER_TYPE_BACKEND);

typedef int (WINAPI * SwapIntervalProc) (int interval);

/* singleton object */
static ClutterBackendWin32 *backend_singleton = NULL;

static gchar *clutter_vblank_name = NULL;

static HINSTANCE clutter_hinst = NULL;

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
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (gobject);
  ClutterStageManager *stage_manager;

  CLUTTER_NOTE (BACKEND, "Disposing the of stages");

  stage_manager = clutter_stage_manager_get_default ();

  /* Destroy all of the stages. g_slist_foreach is used because the
     finalizer for the stages will remove the stage from the
     stage_manager's list and g_slist_foreach has some basic
     protection against this */
  g_slist_foreach (stage_manager->stages, (GFunc) clutter_actor_destroy, NULL);

  CLUTTER_NOTE (BACKEND, "Removing the event source");
  _clutter_backend_win32_events_uninit (CLUTTER_BACKEND (backend_win32));

  /* Unrealize all shaders, since the GL context is going away */
  _clutter_shader_release_all ();

  if (backend_win32->gl_context)
    {
      wglMakeCurrent (NULL, NULL);
      wglDeleteContext (backend_win32->gl_context);
      backend_win32->gl_context = NULL;
    }

  if (backend_win32->dummy_dc)
    {
      ReleaseDC (backend_win32->dummy_hwnd, backend_win32->dummy_dc);
      backend_win32->dummy_dc = NULL;
    }

  if (backend_win32->dummy_hwnd)
    {
      DestroyWindow (backend_win32->dummy_hwnd);
      backend_win32->dummy_hwnd = NULL;
    }

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
  return clutter_vblank_name && !g_ascii_strcasecmp (clutter_vblank_name, name);
}

ClutterFeatureFlags
clutter_backend_win32_get_features (ClutterBackend *backend)
{
  ClutterFeatureFlags  flags;
  const gchar         *extensions;
  SwapIntervalProc     swap_interval;
  ClutterBackendWin32 *backend_win32;

  /* this will make sure that the GL context exists and is bound to a
     drawable */
  backend_win32 = CLUTTER_BACKEND_WIN32 (backend);
  g_return_val_if_fail (backend_win32->gl_context != NULL, 0);
  g_return_val_if_fail (wglGetCurrentDC () != NULL, 0);

  extensions = (const gchar *) glGetString (GL_EXTENSIONS);

  CLUTTER_NOTE (BACKEND,
                "Checking features\n"
                "  GL_VENDOR: %s\n"
                "  GL_RENDERER: %s\n"
                "  GL_VERSION: %s\n"
                "  GL_EXTENSIONS: %s\n",
                glGetString (GL_VENDOR),
                glGetString (GL_RENDERER),
                glGetString (GL_VERSION),
                extensions);

  flags = CLUTTER_FEATURE_STAGE_USER_RESIZE
    | CLUTTER_FEATURE_STAGE_CURSOR
    | CLUTTER_FEATURE_STAGE_MULTIPLE;

  /* If the VBlank should be left at the default or it has been
     disabled elsewhere (eg NVIDIA) then don't bother trying to check
     for the swap control extension */
  if (getenv ("__GL_SYNC_TO_VBLANK") || check_vblank_env ("default"))
    CLUTTER_NOTE (BACKEND, "vblank sync: left at default at user request");
  else if (_cogl_check_extension ("WGL_EXT_swap_control", extensions)
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

  CLUTTER_NOTE (BACKEND, "backend features checked");

  return flags;
}

static ATOM
clutter_backend_win32_get_dummy_window_class ()
{
  static ATOM klass = 0;

  if (klass == 0)
    {
      WNDCLASSW wndclass;
      memset (&wndclass, 0, sizeof (wndclass));
      wndclass.lpfnWndProc = DefWindowProc;
      wndclass.hInstance = GetModuleHandleW (NULL);
      wndclass.lpszClassName = L"ClutterBackendWin32DummyWindow";
      klass = RegisterClassW (&wndclass);
    }

  return klass;
}

static gboolean
clutter_backend_win32_pixel_format_is_better (const PIXELFORMATDESCRIPTOR *pfa,
                                              const PIXELFORMATDESCRIPTOR *pfb)
{
  /* Always prefer a format with a stencil buffer */
  if (pfa->cStencilBits == 0)
    {
      if (pfb->cStencilBits > 0)
        return TRUE;
    }
  else if (pfb->cStencilBits == 0)
    return FALSE;

  /* Prefer a bigger color buffer */
  if (pfb->cColorBits > pfa->cColorBits)
    return TRUE;
  else if (pfb->cColorBits < pfa->cColorBits)
    return FALSE;

  /* Prefer a bigger depth buffer */
  return pfb->cDepthBits > pfa->cDepthBits;
}

static int
clutter_backend_win32_choose_pixel_format (HDC dc, PIXELFORMATDESCRIPTOR *pfd)
{
  int i, num_formats, best_pf = 0;
  PIXELFORMATDESCRIPTOR best_pfd;

  num_formats = DescribePixelFormat (dc, 0, sizeof (best_pfd), NULL);

  for (i = 1; i <= num_formats; i++)
    {
      memset (pfd, 0, sizeof (*pfd));

      if (DescribePixelFormat (dc, i, sizeof (best_pfd), pfd)
          /* Check whether this format is useable by Clutter */
          && ((pfd->dwFlags & (PFD_SUPPORT_OPENGL
                               | PFD_DRAW_TO_WINDOW
                               | PFD_DOUBLEBUFFER
                               | PFD_GENERIC_FORMAT))
              == (PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW))
          && pfd->iPixelType == PFD_TYPE_RGBA
          && pfd->cColorBits >= 16 && pfd->cColorBits <= 32
          && pfd->cDepthBits >= 16 && pfd->cDepthBits <= 32
          /* Check whether this is a better format than one we've
             already found */
          && (best_pf == 0
              || clutter_backend_win32_pixel_format_is_better (&best_pfd, pfd)))
        {
          best_pf = i;
          best_pfd = *pfd;
        }
    }

  *pfd = best_pfd;

  return best_pf;
}

static gboolean
clutter_backend_win32_create_context (ClutterBackend  *backend,
                                      GError         **error)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (backend);

  /* COGL assumes that there is always a GL context selected; in order
   * to make sure that a WGL context exists and is made current, we
   * use a small dummy window that never gets shown to which we can
   * always fall back if no stage is available
   */
  if (backend_win32->dummy_hwnd == NULL)
    {
      ATOM window_class = clutter_backend_win32_get_dummy_window_class ();

      if (window_class == 0)
        {
          g_critical ("Unable to register window class");
          return FALSE;
        }

      backend_win32->dummy_hwnd =
        CreateWindowW ((LPWSTR) MAKEINTATOM (window_class),
                       L".",
                       WS_OVERLAPPEDWINDOW,
                       CW_USEDEFAULT,
                       CW_USEDEFAULT,
                       1, 1,
                       NULL, NULL,
                       GetModuleHandle (NULL),
                       NULL);

      if (backend_win32->dummy_hwnd == NULL)
        {
          g_critical ("Unable to create dummy window");
          return FALSE;
        }
    }

  if (backend_win32->dummy_dc == NULL)
    {
      PIXELFORMATDESCRIPTOR pfd;
      int pf;

      backend_win32->dummy_dc = GetDC (backend_win32->dummy_hwnd);

      pf = clutter_backend_win32_choose_pixel_format (backend_win32->dummy_dc,
                                                      &pfd);

      if (pf == 0 || !SetPixelFormat (backend_win32->dummy_dc, pf, &pfd))
        {
          g_critical ("Unable to find suitable GL pixel format");
          ReleaseDC (backend_win32->dummy_hwnd, backend_win32->dummy_dc);
          backend_win32->dummy_dc = NULL;
          return FALSE;
        }
    }

  if (backend_win32->gl_context == NULL)
    {
      backend_win32->gl_context = wglCreateContext (backend_win32->dummy_dc);

      if (backend_win32->gl_context == NULL)
        {
          g_critical ("Unable to create suitable GL context");
          return FALSE;
        }
    }

  CLUTTER_NOTE (BACKEND, "Selecting dummy 0x%x for the WGL context",
                (unsigned int) backend_win32->dummy_hwnd);

  wglMakeCurrent (backend_win32->dummy_dc, backend_win32->gl_context);

  return TRUE;
}

static void
clutter_backend_win32_ensure_context (ClutterBackend *backend, 
				      ClutterStage   *stage)
{
  ClutterStageWindow  *impl;

  if (stage == NULL ||
      (CLUTTER_PRIVATE_FLAGS (stage) & CLUTTER_ACTOR_IN_DESTRUCTION) ||
      ((impl = _clutter_stage_get_window (stage)) == NULL))
    {
      CLUTTER_NOTE (MULTISTAGE, "Clearing all context");

      wglMakeCurrent (NULL, NULL);
    }
  else
    {
      ClutterBackendWin32 *backend_win32;
      ClutterStageWin32   *stage_win32;

      g_return_if_fail (impl != NULL);
      
      CLUTTER_NOTE (MULTISTAGE, "Setting context for stage of type %s [%p]",
		    g_type_name (G_OBJECT_TYPE (impl)),
		    impl);

      backend_win32 = CLUTTER_BACKEND_WIN32 (backend);
      stage_win32 = CLUTTER_STAGE_WIN32 (impl);
      
      /* no GL context to set */
      if (backend_win32->gl_context == NULL)
        return;

      /* we might get here inside the final dispose cycle, so we
       * need to handle this gracefully
       */
      if (stage_win32->client_dc == NULL)
        {
          CLUTTER_NOTE (MULTISTAGE,
                        "Received a stale stage, clearing all context");

          if (backend_win32->dummy_dc != NULL)
            wglMakeCurrent (backend_win32->dummy_dc,
                            backend_win32->gl_context);
          else
            wglMakeCurrent (NULL, NULL);
        }
      else
        {
          CLUTTER_NOTE (BACKEND,
			"MakeCurrent window %p (%s), context %p",
			stage_win32->hwnd,
			stage_win32->is_foreign_win ? "foreign" : "native",
			backend_win32->gl_context);
          wglMakeCurrent (stage_win32->client_dc,
			  backend_win32->gl_context);
        }
    }
}

static void
clutter_backend_win32_redraw (ClutterBackend *backend,
			      ClutterStage   *stage)
{
  ClutterStageWin32  *stage_win32;
  ClutterStageWindow *impl;

  impl = _clutter_stage_get_window (stage);
  if (impl == NULL)
    return;

  g_return_if_fail (CLUTTER_IS_STAGE_WIN32 (impl));

  stage_win32 = CLUTTER_STAGE_WIN32 (impl);

  /* this will cause the stage implementation to be painted */
  clutter_actor_paint (CLUTTER_ACTOR (stage));
  cogl_flush ();

  if (stage_win32->client_dc)
    SwapBuffers (stage_win32->client_dc);
}

static ClutterStageWindow *
clutter_backend_win32_create_stage (ClutterBackend  *backend,
				    ClutterStage    *wrapper,
				    GError         **error)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (backend);
  ClutterStageWin32 *stage_win32;
  ClutterStageWindow *stage;

  CLUTTER_NOTE (BACKEND, "Creating stage of type '%s'",
		g_type_name (CLUTTER_STAGE_TYPE));

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
  backend_class->redraw           = clutter_backend_win32_redraw;
  backend_class->create_context   = clutter_backend_win32_create_context;
  backend_class->ensure_context   = clutter_backend_win32_ensure_context;
  backend_class->get_device_manager = clutter_backend_win32_get_device_manager;
}

static void
clutter_backend_win32_init (ClutterBackendWin32 *backend_win32)
{
  ClutterBackend *backend = CLUTTER_BACKEND (backend_win32);

  backend_win32->gl_context         = NULL;
  backend_win32->no_event_retrieval = FALSE;
  backend_win32->invisible_cursor   = NULL;

  /* FIXME: get from GetSystemMetric? */
  clutter_backend_set_double_click_time (backend, 250);
  clutter_backend_set_double_click_distance (backend, 5);
  clutter_backend_set_resolution (backend, 96.0);

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
