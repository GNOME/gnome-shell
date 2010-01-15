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

#include "clutter-backend-win32.h"
#include "clutter-stage-win32.h"
#include "clutter-win32.h"

#include "../clutter-stage-window.h"
#include "../clutter-main.h"
#include "../clutter-feature.h"
#include "../clutter-color.h"
#include "../clutter-util.h"
#include "../clutter-event.h"
#include "../clutter-enum-types.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-units.h"
#include "../clutter-shader.h"
#include "../clutter-stage.h"

#include "cogl/cogl.h"

#include <windows.h>

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

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

      clutter_stage_ensure_viewport (CLUTTER_STAGE (stage_win32->wrapper));
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
clutter_stage_win32_get_geometry (ClutterStageWindow *stage_window,
                                  ClutterGeometry    *geometry)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);

  if ((stage_win32->state & CLUTTER_STAGE_STATE_FULLSCREEN))
    {
      geometry->width = (stage_win32->fullscreen_rect.right
                         - stage_win32->fullscreen_rect.left);
      geometry->height = (stage_win32->fullscreen_rect.bottom
                          - stage_win32->fullscreen_rect.top);
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
  *width_out = width_in + GetSystemMetrics (resizable ? SM_CXSIZEFRAME
					    : SM_CXFIXEDFRAME) * 2;
  *height_out = height_in + GetSystemMetrics (resizable ? SM_CYSIZEFRAME
					      : SM_CYFIXEDFRAME) * 2
    + GetSystemMetrics (SM_CYCAPTION);
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
      if ((stage_win32->state & CLUTTER_STAGE_STATE_FULLSCREEN) == 0)
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

      CLUTTER_SET_PRIVATE_FLAGS (stage_win32->wrapper,
                                 CLUTTER_ACTOR_SYNC_MATRICES);
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
    cursor = (HCURSOR) GetClassLongPtrW (stage_win32->hwnd, GCL_HCURSOR);
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
get_window_style (ClutterStageWin32 *stage_win32)
{
  ClutterStage *wrapper = stage_win32->wrapper;

  /* Fullscreen mode shouldn't have any borders */
  if ((stage_win32->state & CLUTTER_STAGE_STATE_FULLSCREEN))
    return WS_POPUP;
  /* Otherwise it's an overlapped window but if it isn't resizable
     then it shouldn't have a thick frame */
  else if (clutter_stage_get_user_resizable (wrapper))
    return WS_OVERLAPPEDWINDOW;
  else
    return WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
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

  if (value)
    stage_win32->state |= CLUTTER_STAGE_STATE_FULLSCREEN;
  else
    stage_win32->state &= ~CLUTTER_STAGE_STATE_FULLSCREEN;

  if (hwnd)
    {
      /* Update the window style but preserve the visibility */
      SetWindowLongW (hwnd, GWL_STYLE,
		      get_window_style (stage_win32)
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

      CLUTTER_SET_PRIVATE_FLAGS (stage_win32->wrapper,
				 CLUTTER_ACTOR_SYNC_MATRICES);
    }

  /* Report the state change */
  memset (&event, 0, sizeof (event));
  event.type = CLUTTER_STAGE_STATE;
  event.stage = CLUTTER_STAGE (stage_win32->wrapper);
  event.new_state = stage_win32->state;
  event.changed_mask = CLUTTER_STAGE_STATE_FULLSCREEN;
  clutter_event_put ((ClutterEvent *) &event);
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
clutter_stage_win32_pixel_format_is_better (const PIXELFORMATDESCRIPTOR *pfa,
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
clutter_stage_win32_choose_pixel_format (HDC dc, PIXELFORMATDESCRIPTOR *pfd)
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
	      || clutter_stage_win32_pixel_format_is_better (&best_pfd, pfd)))
        {
          best_pf = i;
          best_pfd = *pfd;
        }
    }

  *pfd = best_pfd;

  return best_pf;
}

static gboolean
clutter_stage_win32_realize (ClutterStageWindow *stage_window)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);
  ClutterBackendWin32 *backend_win32;
  PIXELFORMATDESCRIPTOR pfd;
  int pf;

  CLUTTER_NOTE (MISC, "Realizing main stage");

  backend_win32 = CLUTTER_BACKEND_WIN32 (clutter_get_default_backend ());

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
      if ((stage_win32->state & CLUTTER_STAGE_STATE_FULLSCREEN))
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

  if (stage_win32->client_dc)
    ReleaseDC (stage_win32->hwnd, stage_win32->client_dc);

  stage_win32->client_dc = GetDC (stage_win32->hwnd);

  pf = clutter_stage_win32_choose_pixel_format (stage_win32->client_dc, &pfd);
  
  if (pf == 0 || !SetPixelFormat (stage_win32->client_dc, pf, &pfd))
    {
      g_critical ("Unable to find suitable GL pixel format");
      goto fail;
    }

  if (backend_win32->gl_context == NULL)
    {
      backend_win32->gl_context = wglCreateContext (stage_win32->client_dc);
      
      if (backend_win32->gl_context == NULL)
        {
          g_critical ("Unable to create suitable GL context");
          goto fail;
        }
    }

  CLUTTER_NOTE (BACKEND, "Successfully realized stage");

  return TRUE;

 fail:
  return FALSE;
}

static void
clutter_stage_win32_unprepare_window (ClutterStageWin32 *stage_win32)
{
  if (stage_win32->client_dc)
    {
      ReleaseDC (stage_win32->hwnd, stage_win32->client_dc);
      stage_win32->client_dc = NULL;
    }

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
clutter_stage_win32_dispose (GObject *gobject)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (gobject);

  /* Make sure that context and window are destroyed in case unrealize
   * hasn't been called yet.
   */
  if (stage_win32->hwnd)
    clutter_stage_win32_unprepare_window (stage_win32);

  if (stage_win32->wtitle)
    g_free (stage_win32->wtitle);

  G_OBJECT_CLASS (clutter_stage_win32_parent_class)->dispose (gobject);
}

static void
clutter_stage_win32_class_init (ClutterStageWin32Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_stage_win32_dispose;
}

static void
clutter_stage_win32_init (ClutterStageWin32 *stage)
{
  stage->hwnd = NULL;
  stage->client_dc = NULL;
  stage->win_width = 640;
  stage->win_height = 480;
  stage->backend = NULL;
  stage->scroll_pos = 0;
  stage->is_foreign_win = FALSE;
  stage->wtitle = NULL;
  stage->is_cursor_visible = TRUE;
  stage->wrapper = NULL;
  	 
  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_IS_TOPLEVEL);
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->get_wrapper = clutter_stage_win32_get_wrapper;
  iface->set_title = clutter_stage_win32_set_title;
  iface->set_fullscreen = clutter_stage_win32_set_fullscreen;
  iface->set_cursor_visible = clutter_stage_win32_set_cursor_visible;
  iface->set_user_resizable = clutter_stage_win32_set_user_resize;
  iface->show = clutter_stage_win32_show;
  iface->hide = clutter_stage_win32_hide;
  iface->resize = clutter_stage_win32_resize;
  iface->get_geometry = clutter_stage_win32_get_geometry;
  iface->realize = clutter_stage_win32_realize;
  iface->unrealize = clutter_stage_win32_unrealize;
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
  POINT window_pos;
  ClutterGeometry geom;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);
  g_return_val_if_fail (hwnd != NULL, FALSE);

  actor = CLUTTER_ACTOR (stage);

  impl = _clutter_stage_get_window (stage);
  stage_win32 = CLUTTER_STAGE_WIN32 (impl);

  /* FIXME this needs updating to use _clutter_actor_rerealize(),
   * see the analogous code in x11 backend. Probably best if
   * win32 maintainer does it so they can be sure it compiles
   * and works.
   */

  clutter_actor_unrealize (actor);

  if (!GetClientRect (hwnd, &client_rect))
    {
      g_warning ("Unable to retrieve the new window geometry");
      return FALSE;
    }
  window_pos.x = client_rect.left;
  window_pos.y = client_rect.right;
  ClientToScreen (hwnd, &window_pos);

  CLUTTER_NOTE (BACKEND, "Setting foreign window (0x%x)", (int) hwnd);

  stage_win32->hwnd = hwnd;
  stage_win32->is_foreign_win = TRUE;

  geom.x = 0;
  geom.y = 0;
  geom.width = client_rect.right - client_rect.left;
  geom.height = client_rect.bottom - client_rect.top;

  clutter_actor_set_geometry (actor, &geom);
  clutter_actor_realize (actor);

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
