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

#include "clutter-backend-win32.h"
#include "clutter-stage-win32.h"
#include "clutter-win32.h"

#include "clutter-actor-private.h"
#include "clutter-main.h"
#include "clutter-feature.h"
#include "clutter-event.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-stage-private.h"

#include "cogl/cogl.h"

#include <windows.h>

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

enum
{
  PROP_0,

  PROP_BACKEND,
  PROP_WRAPPER
};

G_DEFINE_TYPE_WITH_CODE (ClutterStageWin32,
			 clutter_stage_win32,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE
			 (CLUTTER_TYPE_STAGE_WINDOW,
			  clutter_stage_window_iface_init));

static void
clutter_stage_win32_show (ClutterStageWindow *stage_window,
                          gboolean            do_raise)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);

  if (stage_win32->hwnd)
    {
      ShowWindow (stage_win32->hwnd, do_raise ? SW_SHOW : SW_SHOWNA);

      if (stage_win32->accept_focus)
        SetForegroundWindow (stage_win32->hwnd);

      clutter_actor_map (CLUTTER_ACTOR (stage_win32->wrapper));
    }
}

static void
clutter_stage_win32_hide (ClutterStageWindow *stage_window)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);

  if (stage_win32->hwnd)
    {
      clutter_actor_unmap (CLUTTER_ACTOR (stage_win32->wrapper));
      ShowWindow (stage_win32->hwnd, SW_HIDE);
    }
}

static void
clutter_stage_win32_get_geometry (ClutterStageWindow    *stage_window,
                                  cairo_rectangle_int_t *geometry)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);

  if (_clutter_stage_is_fullscreen (stage_win32->wrapper))
    {
      geometry->width = stage_win32->fullscreen_rect.right
                      - stage_win32->fullscreen_rect.left;
      geometry->height = stage_win32->fullscreen_rect.bottom
                       - stage_win32->fullscreen_rect.top;
      return;
    }

  geometry->width = stage_win32->win_width;
  geometry->height = stage_win32->win_height;
}

static void
get_fullscreen_rect (ClutterStageWin32 *stage_win32)
{
  HMONITOR monitor;
  MONITORINFO monitor_info;

  /* If we already have a window then try to use the same monitor that
     is already on */
  if (stage_win32->hwnd)
    monitor = MonitorFromWindow (stage_win32->hwnd, MONITOR_DEFAULTTONEAREST);
  else
    {
      /* Otherwise just guess that they will want the monitor where
	 the cursor is */
      POINT cursor;
      GetCursorPos (&cursor);
      monitor = MonitorFromPoint (cursor, MONITOR_DEFAULTTONEAREST);
    }

  monitor_info.cbSize = sizeof (monitor_info);
  GetMonitorInfoW (monitor, &monitor_info);
  stage_win32->fullscreen_rect = monitor_info.rcMonitor;
}

static void
get_full_window_size (ClutterStageWin32 *stage_win32,
		      int width_in, int height_in,
		      int *width_out, int *height_out)
{
  gboolean resizable
    = clutter_stage_get_user_resizable (stage_win32->wrapper);
  /* The window size passed to CreateWindow includes the window
     decorations */
  gint frame_width, frame_height;

#if !defined (_MSC_VER) || (_MSC_VER < 1700)
  frame_width = GetSystemMetrics (resizable ? SM_CXSIZEFRAME : SM_CXFIXEDFRAME);
  frame_height = GetSystemMetrics (resizable ? SM_CYSIZEFRAME : SM_CYFIXEDFRAME);
#else
  /* MSVC 2012 and later returns wrong values from GetSystemMetrics()
   * http://connect.microsoft.com/VisualStudio/feedback/details/753224/regression-getsystemmetrics-delivers-different-values
   *
   * For AdjustWindowRectEx(), it doesn't matter much whether the Window is resizble.
   */

  RECT cxrect = {0, 0, 0, 0};
  AdjustWindowRectEx (&cxrect, WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_THICKFRAME | WS_DLGFRAME, FALSE, 0);

  frame_width = abs (cxrect.bottom);
  frame_height = abs (cxrect.left);
#endif
  *width_out = width_in + frame_width * 2;
  *height_out = height_in + frame_height * 2 + GetSystemMetrics (SM_CYCAPTION);
}

void
_clutter_stage_win32_get_min_max_info (ClutterStageWin32 *stage_win32,
				       MINMAXINFO *min_max_info)
{
  /* If the window isn't resizable then set the max and min size to
     the current size */
  if (!clutter_stage_get_user_resizable (CLUTTER_STAGE (stage_win32->wrapper)))
    {
      int full_width, full_height;
      get_full_window_size (stage_win32,
			    stage_win32->win_width, stage_win32->win_height,
			    &full_width, &full_height);
      min_max_info->ptMaxTrackSize.x = full_width;
      min_max_info->ptMinTrackSize.x = full_width;
      min_max_info->ptMaxTrackSize.y = full_height;
      min_max_info->ptMinTrackSize.y = full_height;
    }
}

static void
clutter_stage_win32_resize (ClutterStageWindow *stage_window,
                            gint                width,
                            gint                height)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);
  gboolean resize;

  resize = clutter_stage_get_user_resizable (stage_win32->wrapper);

  if (width != stage_win32->win_width || height != stage_win32->win_height)
    {
      /* Ignore size requests if we are in full screen mode */
      if (!_clutter_stage_is_fullscreen (stage_win32->wrapper))
        {
          stage_win32->win_width = width;
          stage_win32->win_height = height;

          if (stage_win32->hwnd != NULL && !stage_win32->is_foreign_win)
            {
              int full_width, full_height;

              get_full_window_size (stage_win32,
                                    width, height,
                                    &full_width, &full_height);

              SetWindowPos (stage_win32->hwnd, NULL,
                            0, 0,
                            full_width, full_height,
                            SWP_NOZORDER | SWP_NOMOVE);
            }
        }
    }
}

static void
clutter_stage_win32_set_title (ClutterStageWindow *stage_window,
			       const gchar        *title)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);

  /* Empty window titles not allowed, so set it to just a period. */
  if (title == NULL || !title[0])
    title = ".";
  
  if (stage_win32->wtitle != NULL)
    g_free (stage_win32->wtitle);
  stage_win32->wtitle = g_utf8_to_utf16 (title, -1, NULL, NULL, NULL);

  /* If the window is not yet created, the title will be set during the
     window creation */
  if (stage_win32->hwnd != NULL)
    SetWindowTextW (stage_win32->hwnd, stage_win32->wtitle);
}

void
_clutter_stage_win32_update_cursor (ClutterStageWin32 *stage_win32)
{
  HCURSOR cursor;

  if (stage_win32->is_cursor_visible)
    cursor = (HCURSOR) GetClassLongPtrW (stage_win32->hwnd, GCLP_HCURSOR);
  else
    {
      ClutterBackend *backend = clutter_get_default_backend ();
      /* The documentation implies that we can just use
         SetCursor(NULL) to get rid of the cursor but apparently this
         doesn't work very well so instead we create an invisible
         cursor */
      cursor = _clutter_backend_win32_get_invisible_cursor (backend);
    }

  SetCursor (cursor);
}

static void
clutter_stage_win32_set_cursor_visible (ClutterStageWindow *stage_window,
                                        gboolean            cursor_visible)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);

  if (stage_win32->is_cursor_visible != cursor_visible)
    {
      POINT cursor_pos;
      RECT client_rect;

      stage_win32->is_cursor_visible = cursor_visible;

      /* If the cursor is already over the client area of the window
         then we need to update it immediately */
      GetCursorPos (&cursor_pos);
      if (WindowFromPoint (cursor_pos) == stage_win32->hwnd &&
          ScreenToClient (stage_win32->hwnd, &cursor_pos) &&
          GetClientRect (stage_win32->hwnd, &client_rect) &&
          cursor_pos.x >= client_rect.left &&
          cursor_pos.y >= client_rect.top &&
          cursor_pos.x < client_rect.right &&
          cursor_pos.y < client_rect.bottom)
        _clutter_stage_win32_update_cursor (stage_win32);
    }
}

static LONG
get_requested_window_style (ClutterStageWin32 *stage_win32,
			    gboolean           want_fullscreen)
{
  ClutterStage *wrapper = stage_win32->wrapper;

  /* Fullscreen mode shouldn't have any borders */
  if (want_fullscreen)
    return WS_POPUP;
  /* Otherwise it's an overlapped window but if it isn't resizable
     then it shouldn't have a thick frame */
  else if (clutter_stage_get_user_resizable (wrapper))
    return WS_OVERLAPPEDWINDOW;
  else
    return WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
}

static LONG
get_window_style (ClutterStageWin32 *stage_win32)
{
  ClutterStage *wrapper = stage_win32->wrapper;

  return get_requested_window_style (stage_win32, 
   _clutter_stage_is_fullscreen (wrapper));
}

static void
clutter_stage_win32_set_user_resize (ClutterStageWindow *stage_window,
				     gboolean            value)
{
  HWND hwnd = CLUTTER_STAGE_WIN32 (stage_window)->hwnd;
  LONG old_style = GetWindowLongW (hwnd, GWL_STYLE);

  /* Update the window style but preserve the visibility */
  SetWindowLongW (hwnd, GWL_STYLE,
		  get_window_style (CLUTTER_STAGE_WIN32 (stage_window))
		  | (old_style & WS_VISIBLE));
  /* Queue a redraw of the frame */
  RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
}

static void
clutter_stage_win32_set_accept_focus (ClutterStageWindow *stage_window,
                                      gboolean            accept_focus)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);

  accept_focus = !!accept_focus;

  stage_win32->accept_focus = accept_focus;
}

static ClutterActor *
clutter_stage_win32_get_wrapper (ClutterStageWindow *stage_window)
{
  return CLUTTER_ACTOR (CLUTTER_STAGE_WIN32 (stage_window)->wrapper);
}

static void
clutter_stage_win32_set_fullscreen (ClutterStageWindow *stage_window,
				    gboolean            value)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);
  HWND hwnd = CLUTTER_STAGE_WIN32 (stage_window)->hwnd;
  LONG old_style = GetWindowLongW (hwnd, GWL_STYLE);
  ClutterStageStateEvent event;

  if (hwnd)
    {
      /* Update the window style but preserve the visibility */
      SetWindowLongW (hwnd, GWL_STYLE,
		      get_requested_window_style (stage_win32, value)
		      | (old_style & WS_VISIBLE));
      /* Update the window size */
      if (value)
        {
          get_fullscreen_rect (stage_win32);
          SetWindowPos (hwnd, HWND_TOP,
                        stage_win32->fullscreen_rect.left,
                        stage_win32->fullscreen_rect.top,
                        stage_win32->fullscreen_rect.right
                        - stage_win32->fullscreen_rect.left,
                        stage_win32->fullscreen_rect.bottom
                        - stage_win32->fullscreen_rect.top,
                        0);
        }
      else
        {
          int full_width, full_height;

          get_full_window_size (stage_win32,
                                stage_win32->win_width,
                                stage_win32->win_height,
                                &full_width, &full_height);

          SetWindowPos (stage_win32->hwnd, NULL,
                        0, 0,
                        full_width, full_height,
                        SWP_NOZORDER | SWP_NOMOVE);
        }
    }

  /* Report the state change */
  if (value)
    {
      _clutter_stage_update_state (stage_win32->wrapper,
                                   0,
                                   CLUTTER_STAGE_STATE_FULLSCREEN);
    }
  else
    {
      _clutter_stage_update_state (stage_win32->wrapper,
                                   CLUTTER_STAGE_STATE_FULLSCREEN,
                                   0);
    }
}

static ATOM
clutter_stage_win32_get_window_class ()
{
  static ATOM klass = 0;

  if (klass == 0)
    {
      WNDCLASSW wndclass;
      memset (&wndclass, 0, sizeof (wndclass));
      wndclass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
      wndclass.lpfnWndProc = _clutter_stage_win32_window_proc;
      wndclass.cbWndExtra = sizeof (LONG_PTR);
      wndclass.hInstance = GetModuleHandleW (NULL);
      wndclass.hIcon = LoadIconW (NULL, (LPWSTR) IDI_APPLICATION);
      wndclass.hCursor = LoadCursorW (NULL, (LPWSTR) IDC_ARROW);
      wndclass.hbrBackground = NULL;
      wndclass.lpszMenuName = NULL;
      wndclass.lpszClassName = L"ClutterStageWin32";
      klass = RegisterClassW (&wndclass);
    }

  return klass;
}

static gboolean
clutter_stage_win32_realize (ClutterStageWindow *stage_window)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);
  ClutterBackend *backend;
  ClutterBackendWin32 *backend_win32;
  CoglFramebuffer *framebuffer;
  gfloat width;
  gfloat height;
  GError *error = NULL;

  CLUTTER_NOTE (MISC, "Realizing main stage");

  backend = CLUTTER_BACKEND (stage_win32->backend);
  backend_win32 = CLUTTER_BACKEND_WIN32 (backend);

  clutter_actor_get_size (CLUTTER_ACTOR (stage_win32->wrapper),
                          &width, &height);

  stage_win32->onscreen = cogl_onscreen_new (backend->cogl_context,
                                             width, height);

  if (stage_win32->hwnd == NULL)
    {
      ATOM window_class = clutter_stage_win32_get_window_class ();
      int win_xpos, win_ypos, win_width, win_height;

      if (window_class == 0)
        {
          g_critical ("Unable to register window class");
          goto fail;
        }

      /* If we're in fullscreen mode then use the fullscreen rect
         instead */
      if (_clutter_stage_is_fullscreen (stage_win32->wrapper))
        {
          get_fullscreen_rect (stage_win32);
          win_xpos = stage_win32->fullscreen_rect.left;
          win_ypos = stage_win32->fullscreen_rect.top;
          win_width = stage_win32->fullscreen_rect.right - win_xpos;
          win_height = stage_win32->fullscreen_rect.bottom - win_ypos;
        }
      else
        {
          win_xpos = win_ypos = CW_USEDEFAULT;

          get_full_window_size (stage_win32,
                                stage_win32->win_width,
                                stage_win32->win_height,
                                &win_width, &win_height);
        }

      if (stage_win32->wtitle == NULL)
        stage_win32->wtitle = g_utf8_to_utf16 (".", -1, NULL, NULL, NULL);

      stage_win32->hwnd = CreateWindowW ((LPWSTR) MAKEINTATOM (window_class),
					 stage_win32->wtitle,
					 get_window_style (stage_win32),
					 win_xpos,
					 win_ypos,
					 win_width,
					 win_height,
					 NULL, NULL,
					 GetModuleHandle (NULL),
					 NULL);

      if (stage_win32->hwnd == NULL)
        {
          g_critical ("Unable to create stage window");
          goto fail;
        }

      /* Store a pointer to the actor in the extra bytes of the window
         so we can quickly access it in the window procedure */
      SetWindowLongPtrW (stage_win32->hwnd, 0, (LONG_PTR) stage_win32);
    }

  cogl_win32_onscreen_set_foreign_window (stage_win32->onscreen,
                                          stage_win32->hwnd);

  cogl_onscreen_set_swap_throttled (stage_win32->onscreen,
                                    _clutter_get_sync_to_vblank ());

  framebuffer = COGL_FRAMEBUFFER (stage_win32->onscreen);
  if (!cogl_framebuffer_allocate (framebuffer, &error))
    {
      g_warning ("Failed to allocate stage: %s", error->message);
      g_error_free (error);
      cogl_object_unref (stage_win32->onscreen);
      stage_win32->onscreen = NULL;
      goto fail;
    }

  /* Create a context. This will be a no-op if we already have one */
  if (!_clutter_backend_create_context (CLUTTER_BACKEND (backend_win32),
                                        &error))
    {
      g_critical ("Unable to realize stage: %s", error->message);
      g_error_free (error);
      goto fail;
    }

  CLUTTER_NOTE (BACKEND, "Successfully realized stage");

  return TRUE;

 fail:
  return FALSE;
}

static void
clutter_stage_win32_unprepare_window (ClutterStageWin32 *stage_win32)
{
  if (!stage_win32->is_foreign_win && stage_win32->hwnd)
    {
      /* Drop the pointer to this stage in the window so that any
         further messages won't be processed. The stage might be being
         destroyed so otherwise the messages would be handled with an
         invalid stage instance */
      SetWindowLongPtrW (stage_win32->hwnd, 0, (LONG_PTR) 0);
      DestroyWindow (stage_win32->hwnd);
    }
}

static void
clutter_stage_win32_unrealize (ClutterStageWindow *stage_window)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);

  CLUTTER_NOTE (BACKEND, "Unrealizing stage");

  clutter_stage_win32_unprepare_window (stage_win32);
}

static void
clutter_stage_win32_redraw (ClutterStageWindow *stage_window)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);

  /* this will cause the stage implementation to be painted */
  _clutter_stage_do_paint (stage_win32->wrapper, NULL);
  cogl_flush ();

  if (stage_win32->onscreen)
    cogl_onscreen_swap_buffers (COGL_FRAMEBUFFER (stage_win32->onscreen));
}

static CoglFramebuffer *
clutter_stage_win32_get_active_framebuffer (ClutterStageWindow *stage_window)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);

  return COGL_FRAMEBUFFER (stage_win32->onscreen);
}

static void
clutter_stage_win32_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ClutterStageWin32 *self = CLUTTER_STAGE_WIN32 (gobject);

  switch (prop_id)
    {
    case PROP_BACKEND:
      self->backend = g_value_get_object (value);
      break;

    case PROP_WRAPPER:
      self->wrapper = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_stage_win32_dispose (GObject *gobject)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (gobject);

  /* Make sure that context and window are destroyed in case unrealize
   * hasn't been called yet.
   */
  if (stage_win32->hwnd)
    clutter_stage_win32_unprepare_window (stage_win32);

  if (stage_win32->wtitle)
    {
      g_free (stage_win32->wtitle);
      stage_win32->wtitle = NULL;
    }

  G_OBJECT_CLASS (clutter_stage_win32_parent_class)->dispose (gobject);
}

static void
clutter_stage_win32_class_init (ClutterStageWin32Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = clutter_stage_win32_set_property;
  gobject_class->dispose = clutter_stage_win32_dispose;

  g_object_class_override_property (gobject_class, PROP_BACKEND, "backend");
  g_object_class_override_property (gobject_class, PROP_WRAPPER, "wrapper");
}

static void
clutter_stage_win32_init (ClutterStageWin32 *stage)
{
  stage->hwnd = NULL;
  stage->win_width = 640;
  stage->win_height = 480;
  stage->backend = NULL;
  stage->scroll_pos = 0;
  stage->wtitle = NULL;
  stage->wrapper = NULL;

  stage->is_foreign_win = FALSE;
  stage->is_cursor_visible = TRUE;
  stage->accept_focus = TRUE;
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->get_wrapper = clutter_stage_win32_get_wrapper;
  iface->set_title = clutter_stage_win32_set_title;
  iface->set_fullscreen = clutter_stage_win32_set_fullscreen;
  iface->set_cursor_visible = clutter_stage_win32_set_cursor_visible;
  iface->set_user_resizable = clutter_stage_win32_set_user_resize;
  iface->set_accept_focus = clutter_stage_win32_set_accept_focus;
  iface->show = clutter_stage_win32_show;
  iface->hide = clutter_stage_win32_hide;
  iface->resize = clutter_stage_win32_resize;
  iface->get_geometry = clutter_stage_win32_get_geometry;
  iface->realize = clutter_stage_win32_realize;
  iface->unrealize = clutter_stage_win32_unrealize;
  iface->redraw = clutter_stage_win32_redraw;
  iface->get_active_framebuffer = clutter_stage_win32_get_active_framebuffer;
}

/**
 * clutter_win32_get_stage_window:
 * @stage: a #ClutterStage
 *
 * Gets the stage's window handle
 *
 * Return value: An HWND for the stage window.
 *
 * Since: 0.8
 */
HWND
clutter_win32_get_stage_window (ClutterStage *stage)
{
  ClutterStageWindow *impl;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  impl = _clutter_stage_get_window (stage);

  g_return_val_if_fail (CLUTTER_IS_STAGE_WIN32 (impl), NULL);

  return CLUTTER_STAGE_WIN32 (impl)->hwnd;
}

/**
 * clutter_win32_get_stage_from_window:
 * @hwnd: a window handle
 *
 * Gets the stage for a particular window.
 *
 * Return value: The stage or NULL if a stage does not exist for the
 * window.
 *
 * Since: 0.8
 */
ClutterStage *
clutter_win32_get_stage_from_window (HWND hwnd)
{
  /* Check whether the window handle is an instance of the stage
     window class */
  if ((ATOM) GetClassLongPtrW (hwnd, GCW_ATOM)
      == clutter_stage_win32_get_window_class ())
    /* If it is there should be a pointer to the stage in the window
       extra data */
    return CLUTTER_STAGE_WIN32 (GetWindowLongPtrW (hwnd, 0))->wrapper;
  else
    {
      /* Otherwise it might be a foreign window so we should check the
	 stage list */
      ClutterStageManager *stage_manager;
      const GSList        *stages, *l;

      stage_manager = clutter_stage_manager_get_default ();
      stages = clutter_stage_manager_peek_stages (stage_manager);

      for (l = stages; l != NULL; l = l->next)
	{
	  ClutterStage *stage = l->data;
	  ClutterStageWindow *impl;

	  impl = _clutter_stage_get_window (stage);
	  g_assert (CLUTTER_IS_STAGE_WIN32 (impl));

	  if (CLUTTER_STAGE_WIN32 (impl)->hwnd == hwnd)
	    return stage;
	}
    }

  return NULL;
}

typedef struct {
  ClutterStageWin32 *stage_win32;
  cairo_rectangle_int_t geom;
  HWND hwnd;
  guint destroy_old_hwnd : 1;
} ForeignWindowData;

static void
set_foreign_window_callback (ClutterActor *actor,
                             void         *data)
{
  ForeignWindowData *fwd = data;

  CLUTTER_NOTE (BACKEND, "Setting foreign window (0x%x)",
                (guint) fwd->hwnd);

  if (fwd->destroy_old_hwnd && fwd->stage_win32->hwnd != NULL)
    {
      CLUTTER_NOTE (BACKEND, "Destroying previous window (0x%x)",
                    (guint) fwd->stage_win32->hwnd);
      DestroyWindow (fwd->stage_win32->hwnd);
    }

  fwd->stage_win32->hwnd = fwd->hwnd;
  fwd->stage_win32->is_foreign_win = TRUE;

  fwd->stage_win32->win_width = fwd->geom.width;
  fwd->stage_win32->win_height = fwd->geom.height;

  clutter_actor_set_size (actor, fwd->geom.width, fwd->geom.height);

  /* calling this with the stage unrealized will unset the stage
   * from the GL context; once the stage is realized the GL context
   * will be set again
   */
  clutter_stage_ensure_current (CLUTTER_STAGE (actor));
}

/**
 * clutter_win32_set_stage_foreign:
 * @stage: a #ClutterStage
 * @hwnd: an existing window handle
 *
 * Target the #ClutterStage to use an existing external window handle.
 *
 * Return value: %TRUE if foreign window is valid
 *
 * Since: 0.8
 */
gboolean
clutter_win32_set_stage_foreign (ClutterStage *stage,
				 HWND          hwnd)
{
  ClutterStageWin32 *stage_win32;
  ClutterStageWindow *impl;
  ClutterActor *actor;
  RECT client_rect;
  ForeignWindowData fwd;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);
  g_return_val_if_fail (hwnd != NULL, FALSE);

  impl = _clutter_stage_get_window (stage);
  if (!CLUTTER_IS_STAGE_WIN32 (impl))
    {
      g_critical ("The Clutter backend is not a Windows backend");
      return FALSE;
    }

  stage_win32 = CLUTTER_STAGE_WIN32 (impl);

  if (!GetClientRect (hwnd, &client_rect))
    {
      g_warning ("Unable to retrieve the new window geometry");
      return FALSE;
    }

  fwd.stage_win32 = stage_win32;
  fwd.hwnd = hwnd;

  /* destroy the old HWND, if we have one and it's ours */
  if (stage_win32->hwnd != NULL && !stage_win32->is_foreign_win)
    fwd.destroy_old_hwnd = TRUE;
  else
    fwd.destroy_old_hwnd = FALSE;

  fwd.geom.x = 0;
  fwd.geom.y = 0;
  fwd.geom.width = client_rect.right - client_rect.left;
  fwd.geom.height = client_rect.bottom - client_rect.top;

  actor = CLUTTER_ACTOR (stage);

  _clutter_actor_rerealize (actor,
                            set_foreign_window_callback,
                            &fwd);

  /* Queue a relayout - so the stage will be allocated the new
   * window size.
   *
   * Note also that when the stage gets allocated the new
   * window size that will result in the stage's
   * priv->viewport being changed, which will in turn result
   * in the Cogl viewport changing when _clutter_do_redraw
   * calls _clutter_stage_maybe_setup_viewport().
   */
  clutter_actor_queue_relayout (CLUTTER_ACTOR (stage));

  return TRUE;
}

void
clutter_stage_win32_map (ClutterStageWin32 *stage_win32)
{
  clutter_actor_map (CLUTTER_ACTOR (stage_win32->wrapper));

  clutter_actor_queue_relayout (CLUTTER_ACTOR (stage_win32->wrapper));
}

void
clutter_stage_win32_unmap (ClutterStageWin32 *stage_win32)
{
  clutter_actor_unmap (CLUTTER_ACTOR (stage_win32->wrapper));
}
