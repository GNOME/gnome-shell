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
			 CLUTTER_TYPE_GROUP,
			 G_IMPLEMENT_INTERFACE
			 (CLUTTER_TYPE_STAGE_WINDOW,
			  clutter_stage_window_iface_init));

static void
clutter_stage_win32_show (ClutterActor *actor)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (actor);

  if (stage_win32->hwnd)
    {
      /* Force a redraw so that the layout will be run and the correct
	 size will be allocated to the window before it is
	 shown. Otherwise a WM_SIZE message will be sent which will
	 override the user's chosen size */
      clutter_redraw (stage_win32->wrapper);

      ShowWindow (stage_win32->hwnd, SW_SHOW);
    }
}

static void
clutter_stage_win32_hide (ClutterActor *actor)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (actor);

  if (stage_win32->hwnd)
    ShowWindow (stage_win32->hwnd, SW_HIDE);
}

static void
clutter_stage_win32_get_preferred_width (ClutterActor  *self,
					 gfloat         for_height,
					 gfloat        *min_width_p,
					 gfloat        *natural_width_p)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (self);
  int width;

  /* If we're in fullscreen mode then return the size of the screen
     instead */
  if ((stage_win32->state & CLUTTER_STAGE_STATE_FULLSCREEN))
    width = CLUTTER_UNITS_FROM_DEVICE (stage_win32->fullscreen_rect.right
				       - stage_win32->fullscreen_rect.left);
  else
    width = CLUTTER_UNITS_FROM_DEVICE (stage_win32->win_width);

  if (min_width_p)
    *min_width_p = width;

  if (natural_width_p)
    *natural_width_p = width;
}

static void
clutter_stage_win32_get_preferred_height (ClutterActor  *self,
					  gfloat         for_width,
					  gfloat        *min_height_p,
					  gfloat        *natural_height_p)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (self);
  int height;

  /* If we're in fullscreen mode then return the size of the screen
     instead */
  if ((stage_win32->state & CLUTTER_STAGE_STATE_FULLSCREEN))
    height = CLUTTER_UNITS_FROM_DEVICE (stage_win32->fullscreen_rect.bottom
				       - stage_win32->fullscreen_rect.top);
  else
    height = CLUTTER_UNITS_FROM_DEVICE (stage_win32->win_height);

  if (min_height_p)
    *min_height_p = height;

  if (natural_height_p)
    *natural_height_p = height;
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
clutter_stage_win32_allocate (ClutterActor          *self,
			      const ClutterActorBox *box,
			      gboolean               origin_changed)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (self);
  gint new_width, new_height;

  new_width  = ABS (CLUTTER_UNITS_TO_INT (box->x2 - box->x1));
  new_height = ABS (CLUTTER_UNITS_TO_INT (box->y2 - box->y1)); 

  if (new_width != stage_win32->win_width
      || new_height != stage_win32->win_height)
    {
      /* Ignore size requests if we are in full screen mode */
      if ((stage_win32->state & CLUTTER_STAGE_STATE_FULLSCREEN) == 0)
	{
	  stage_win32->win_width = new_width;
	  stage_win32->win_height = new_height;

	  if (stage_win32->hwnd != NULL && !stage_win32->is_foreign_win)
	    {
	      int full_width, full_height;

	      get_full_window_size (stage_win32,
				    new_width, new_height,
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

  CLUTTER_ACTOR_CLASS (clutter_stage_win32_parent_class)
    ->allocate (self, box, origin_changed);
}

static void
clutter_stage_win32_set_title (ClutterStageWindow *stage_window,
			       const gchar        *title)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (stage_window);
  wchar_t *wtitle;

  /* Empty window titles not allowed, so set it to just a period. */
  if (title == NULL || !title[0])
    title = ".";
  
  wtitle = g_utf8_to_utf16 (title, -1, NULL, NULL, NULL);
  SetWindowTextW (stage_win32->hwnd, wtitle);
  g_free (wtitle);
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
clutter_stage_win32_check_gl_version ()
{
  const char *version_string, *major_end, *minor_end;
  int major = 0, minor = 0;
  
  /* Get the OpenGL version number */
  if ((version_string = (const char *) glGetString (GL_VERSION)) == NULL)
    return FALSE;

  /* Extract the major number */
  for (major_end = version_string; *major_end >= '0'
	 && *major_end <= '9'; major_end++)
    major = (major * 10) + *major_end - '0';
  /* If there were no digits or the major number isn't followed by a
     dot then it is invalid */
  if (major_end == version_string || *major_end != '.')
    return FALSE;
  
  /* Extract the minor number */
  for (minor_end = major_end + 1; *minor_end >= '0'
	 && *minor_end <= '9'; minor_end++)
    minor = (minor * 10) + *minor_end - '0';
  /* If there were no digits or there is an unexpected character then
     it is invalid */
  if (minor_end == major_end + 1
      || (*minor_end && *minor_end != ' ' && *minor_end != '.'))
    return FALSE;

  /* Accept OpenGL 1.2 or later */
  return major > 1 || (major == 1 && minor >= 2);
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

static void
clutter_stage_win32_realize (ClutterActor *actor)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (actor);
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
	  win_height = stage_win32->fullscreen_rect.left - win_ypos;
	}
      else
	{
	  win_xpos = win_ypos = CW_USEDEFAULT;

	  get_full_window_size (stage_win32,
				stage_win32->win_width,
				stage_win32->win_height,
				&win_width, &win_height);
	}

      stage_win32->hwnd = CreateWindowW ((LPWSTR) MAKEINTATOM (window_class),
					 L".",
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

      /* Make the context current so we can check the GL version */
      wglMakeCurrent (stage_win32->client_dc, backend_win32->gl_context);

      if (!clutter_stage_win32_check_gl_version ())
	{
	  g_critical ("OpenGL version number is too low");
	  goto fail;
	}
    }

  CLUTTER_NOTE (BACKEND, "Marking stage as realized");
  CLUTTER_ACTOR_SET_FLAGS (stage_win32, CLUTTER_ACTOR_REALIZED);

  return;

 fail:
  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
}

static void
clutter_stage_win32_unrealize (ClutterActor *actor)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (actor);

  CLUTTER_NOTE (BACKEND, "Unrealizing stage");

  if (CLUTTER_ACTOR_CLASS (clutter_stage_win32_parent_class)->unrealize != NULL)
    CLUTTER_ACTOR_CLASS (clutter_stage_win32_parent_class)->unrealize (actor);

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
      stage_win32->hwnd = NULL;
    }
}

static void
clutter_stage_win32_dispose (GObject *gobject)
{
  ClutterStageWin32 *stage_win32 = CLUTTER_STAGE_WIN32 (gobject);

  G_OBJECT_CLASS (clutter_stage_win32_parent_class)->dispose (gobject);
}

static void
clutter_stage_win32_class_init (ClutterStageWin32Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->dispose = clutter_stage_win32_dispose;
  
  actor_class->show = clutter_stage_win32_show;
  actor_class->hide = clutter_stage_win32_hide;
  actor_class->get_preferred_width = clutter_stage_win32_get_preferred_width;
  actor_class->get_preferred_height = clutter_stage_win32_get_preferred_height;
  actor_class->allocate = clutter_stage_win32_allocate;
  actor_class->realize = clutter_stage_win32_realize;
  actor_class->unrealize = clutter_stage_win32_unrealize;
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

  stage->wrapper = NULL;
  	 
  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_IS_TOPLEVEL);
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->get_wrapper = clutter_stage_win32_get_wrapper;
  iface->set_title = clutter_stage_win32_set_title;
  iface->set_fullscreen = clutter_stage_win32_set_fullscreen;
  iface->set_user_resizable = clutter_stage_win32_set_user_resize;
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
      ClutterMainContext  *context = clutter_context_get_default ();
      ClutterStageManager *stage_manager = context->stage_manager;
      GSList              *l;

      for (l = stage_manager->stages; l; l = l->next)
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
  CLUTTER_ACTOR_SET_FLAGS (stage_win32, CLUTTER_ACTOR_MAPPED);
  CLUTTER_ACTOR_SET_FLAGS (stage_win32->wrapper, CLUTTER_ACTOR_MAPPED);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (stage_win32->wrapper));
}

void
clutter_stage_win32_unmap (ClutterStageWin32 *stage_win32)
{
  CLUTTER_ACTOR_UNSET_FLAGS (stage_win32, CLUTTER_ACTOR_MAPPED);
  CLUTTER_ACTOR_UNSET_FLAGS (stage_win32->wrapper, CLUTTER_ACTOR_MAPPED);
}
