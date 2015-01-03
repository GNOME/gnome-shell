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

#include "clutter-stage-win32.h"
#include "clutter-backend-win32.h"
#include "clutter-win32.h"

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-event-private.h"
#include "clutter-keysyms.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include <string.h>
#include <glib.h>
#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>

typedef struct _ClutterEventSource      ClutterEventSource;

struct _ClutterEventSource
{
  GSource source;

  ClutterBackend *backend;
  GPollFD event_poll_fd;
};

static gboolean clutter_event_prepare  (GSource     *source,
                                        gint        *timeout);
static gboolean clutter_event_check    (GSource     *source);
static gboolean clutter_event_dispatch (GSource     *source,
                                        GSourceFunc  callback,
                                        gpointer     user_data);

static GSourceFuncs event_funcs = {
  clutter_event_prepare,
  clutter_event_check,
  clutter_event_dispatch,
  NULL
};

/* Special mapping for some keys that don't have a direct Unicode
   value. Must be sorted by the numeric value of the Windows key
   virtual key code */
static const struct
{
  gushort win_sym, clutter_sym;
} clutter_win32_key_map[] =
  {
    { VK_CANCEL, CLUTTER_KEY_Cancel },
    { VK_BACK, CLUTTER_KEY_BackSpace },
    { VK_TAB, CLUTTER_KEY_Tab },
    { VK_CLEAR, CLUTTER_KEY_Clear },
    { VK_RETURN, CLUTTER_KEY_Return },
    { VK_MENU, CLUTTER_KEY_Menu },
    { VK_PAUSE, CLUTTER_KEY_Pause },
    { VK_HANGUL, CLUTTER_KEY_Hangul },
    { VK_KANJI, CLUTTER_KEY_Kanji },
    { VK_ESCAPE, CLUTTER_KEY_Escape },
    { VK_SPACE, CLUTTER_KEY_space },
    { VK_PRIOR, CLUTTER_KEY_Prior },
    { VK_NEXT, CLUTTER_KEY_Next },
    { VK_END, CLUTTER_KEY_End },
    { VK_HOME, CLUTTER_KEY_Home },
    { VK_LEFT, CLUTTER_KEY_Left },
    { VK_UP, CLUTTER_KEY_Up },
    { VK_RIGHT, CLUTTER_KEY_Right },
    { VK_DOWN, CLUTTER_KEY_Down },
    { VK_SELECT, CLUTTER_KEY_Select },
    { VK_PRINT, CLUTTER_KEY_Print },
    { VK_EXECUTE, CLUTTER_KEY_Execute },
    { VK_INSERT, CLUTTER_KEY_Insert },
    { VK_DELETE, CLUTTER_KEY_Delete },
    { VK_HELP, CLUTTER_KEY_Help },
    { VK_MULTIPLY, CLUTTER_KEY_multiply },
    { VK_F1, CLUTTER_KEY_F1 },
    { VK_F2, CLUTTER_KEY_F2 },
    { VK_F3, CLUTTER_KEY_F3 },
    { VK_F4, CLUTTER_KEY_F4 },
    { VK_F5, CLUTTER_KEY_F5 },
    { VK_F6, CLUTTER_KEY_F6 },
    { VK_F7, CLUTTER_KEY_F7 },
    { VK_F8, CLUTTER_KEY_F8 },
    { VK_F9, CLUTTER_KEY_F9 },
    { VK_F10, CLUTTER_KEY_F10 },
    { VK_F11, CLUTTER_KEY_F11 },
    { VK_F12, CLUTTER_KEY_F12 },
    { VK_F13, CLUTTER_KEY_F13 },
    { VK_F14, CLUTTER_KEY_F14 },
    { VK_F15, CLUTTER_KEY_F15 },
    { VK_F16, CLUTTER_KEY_F16 },
    { VK_F17, CLUTTER_KEY_F17 },
    { VK_F18, CLUTTER_KEY_F18 },
    { VK_F19, CLUTTER_KEY_F19 },
    { VK_F20, CLUTTER_KEY_F20 },
    { VK_F21, CLUTTER_KEY_F21 },
    { VK_F22, CLUTTER_KEY_F22 },
    { VK_F23, CLUTTER_KEY_F23 },
    { VK_F24, CLUTTER_KEY_F24 },
    { VK_LSHIFT, CLUTTER_KEY_Shift_L },
    { VK_RSHIFT, CLUTTER_KEY_Shift_R },
    { VK_LCONTROL, CLUTTER_KEY_Control_L },
    { VK_RCONTROL, CLUTTER_KEY_Control_R }
  };

#define CLUTTER_WIN32_KEY_MAP_SIZE      (G_N_ELEMENTS (clutter_win32_key_map))

static GSource *
clutter_event_source_new (ClutterBackend *backend)
{
  GSource *source = g_source_new (&event_funcs, sizeof (ClutterEventSource));
  ClutterEventSource *event_source = (ClutterEventSource *) source;

  event_source->backend = backend;

  g_source_set_name (source, "Clutter Win32 Event Source");

  return source;
}

void
_clutter_backend_win32_events_init (ClutterBackend *backend)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (backend);
  GSource *source;
  ClutterEventSource *event_source;

  source = backend_win32->event_source = clutter_event_source_new (backend);
  event_source = (ClutterEventSource *) source;
  g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);

  event_source->event_poll_fd.fd = G_WIN32_MSG_HANDLE;
  event_source->event_poll_fd.events = G_IO_IN;

  g_source_add_poll (source, &event_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);
}

void
_clutter_backend_win32_events_uninit (ClutterBackend *backend)
{
  ClutterBackendWin32 *backend_win32 = CLUTTER_BACKEND_WIN32 (backend);

  if (backend_win32->event_source)
    {
      CLUTTER_NOTE (EVENT, "Destroying the event source");

      g_source_destroy (backend_win32->event_source);
      g_source_unref (backend_win32->event_source);
      backend_win32->event_source = NULL;
    }
}

static gboolean
check_msg_pending ()
{
  MSG msg;

  return PeekMessageW (&msg, NULL, 0, 0, PM_NOREMOVE) ? TRUE : FALSE;
}

static ClutterModifierType
get_modifier_state (WPARAM wparam)
{
  ClutterModifierType ret = 0;

  if ((wparam & MK_SHIFT))
    ret |= CLUTTER_SHIFT_MASK;
  if ((wparam & MK_CONTROL))
    ret |= CLUTTER_CONTROL_MASK;
  if ((wparam & MK_LBUTTON))
    ret |= CLUTTER_BUTTON1_MASK;
  if ((wparam & MK_MBUTTON))
    ret |= CLUTTER_BUTTON2_MASK;
  if ((wparam & MK_RBUTTON))
    ret |= CLUTTER_BUTTON3_MASK;

  return ret;
}

static inline void
take_and_queue_event (ClutterEvent *event)
{
  /* The event is added directly to the queue instead of using
     clutter_event_put so that it can avoid a copy. This takes
     ownership of the event */
  _clutter_event_push (event, FALSE);
}

static inline void
make_button_event (const MSG *msg,
                   ClutterStage *stage,
		   int button,
                   int click_count,
                   gboolean release,
                   ClutterInputDevice *device)
{
  ClutterEvent *event = clutter_event_new (release ?
                                           CLUTTER_BUTTON_RELEASE :
                                           CLUTTER_BUTTON_PRESS);

  event->any.stage = stage;

  event->button.time = msg->time;
  event->button.x = GET_X_LPARAM (msg->lParam);
  event->button.y = GET_Y_LPARAM (msg->lParam);
  event->button.modifier_state = get_modifier_state (msg->wParam);
  event->button.button = button;
  event->button.click_count = click_count;
  clutter_event_set_device (event, device);

  take_and_queue_event (event);
}

static gboolean
clutter_event_prepare (GSource *source,
                       gint    *timeout)
{
  gboolean retval;

  _clutter_threads_acquire_lock ();

  *timeout = -1;
  retval = (clutter_events_pending () || check_msg_pending ());

  _clutter_threads_release_lock ();

  return retval;
}

static gboolean
clutter_event_check (GSource *source)
{
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  gboolean retval;

  _clutter_threads_acquire_lock ();

  if ((event_source->event_poll_fd.revents & G_IO_IN))
    retval = (clutter_events_pending () || check_msg_pending ());
  else
    retval = FALSE;

  _clutter_threads_release_lock ();

  return retval;
}

static gboolean
clutter_event_dispatch (GSource     *source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  ClutterEvent *event;
  MSG msg;

  _clutter_threads_acquire_lock ();

  /* Process Windows messages until we've got one that translates into
     the clutter event queue */
  while (!clutter_events_pending () && PeekMessageW (&msg, NULL,
						     0, 0, PM_REMOVE))
      DispatchMessageW (&msg);

  /* Pop an event off the queue if any */
  if ((event = clutter_event_get ()))
    {
      /* forward the event into clutter for emission etc. */
      if (event->any.stage)
        _clutter_stage_queue_event (event->any.stage, event, FALSE);
    }

  _clutter_threads_release_lock ();

  return TRUE;
}

static ClutterModifierType
get_key_modifier_state (const BYTE *key_states)
{
  ClutterModifierType ret = 0;

  if ((key_states[VK_SHIFT] & 0x80)
      || (key_states[VK_LSHIFT] & 0x80)
      || (key_states[VK_RSHIFT] & 0x80))
    ret |= CLUTTER_SHIFT_MASK;
  if ((key_states[VK_CONTROL] & 0x80)
      || (key_states[VK_LCONTROL] & 0x80)
      || (key_states[VK_RCONTROL] & 0x80))
    ret |= CLUTTER_CONTROL_MASK;
  if ((key_states[VK_MENU] & 0x80)
      || (key_states[VK_LMENU] & 0x80)
      || (key_states[VK_RMENU] & 0x80))
    ret |= CLUTTER_MOD1_MASK;
  if (key_states[VK_CAPITAL])
    ret |= CLUTTER_LOCK_MASK;

  return ret;
}

/**
 * clutter_win32_handle_event:
 * @msg: A pointer to a structure describing a Win32 message.
 *
 * This function processes a single Win32 message. It can be used to
 * hook into external windows message processing (for example, a GDK
 * filter function).
 *
 * If clutter_win32_disable_event_retrieval() has been called, you must
 * let this function process events to update Clutter's internal state.
 *
 * Return value: %TRUE if the message was handled entirely by Clutter
 * and no further processing (such as calling the default window
 * procedure) should take place. %FALSE is returned if is the message
 * was not handled at all or if Clutter expects processing to take
 * place.
 *
 * Since: 1.6
 */
gboolean
clutter_win32_handle_event (const MSG *msg)
{
  ClutterBackend       *backend;
  ClutterBackendWin32  *backend_win32;
  ClutterStageWin32    *stage_win32;
  ClutterDeviceManager *manager;
  ClutterInputDevice   *core_pointer, *core_keyboard;
  ClutterStage         *stage;
  ClutterStageWindow   *impl;
  gboolean              return_value = FALSE;

  stage = clutter_win32_get_stage_from_window (msg->hwnd);

  /* Ignore any messages for windows which we don't have a stage for */
  if (stage == NULL)
    return FALSE;

  impl = _clutter_stage_get_window (stage);
  stage_win32 = CLUTTER_STAGE_WIN32 (impl);
  backend_win32 = stage_win32->backend;
  backend = CLUTTER_BACKEND (backend_win32);

  /* Give Cogl a chance to handle the message first */
  if (backend->cogl_renderer != NULL &&
      cogl_win32_renderer_handle_event (backend->cogl_renderer,
                                        (void *) msg) == COGL_FILTER_REMOVE)
    return TRUE;

  manager = clutter_device_manager_get_default ();
  if (manager == NULL)
    return FALSE;

  core_pointer =
    clutter_device_manager_get_core_device (manager, CLUTTER_POINTER_DEVICE);
  core_keyboard =
    clutter_device_manager_get_core_device (manager, CLUTTER_KEYBOARD_DEVICE);

  switch (msg->message)
    {
    case WM_SIZE:
      if (!stage_win32->is_foreign_win
	  /* Ignore size changes resulting from the stage being
	     minimized - otherwise the window size will be set to
	     0,0 */
	  && msg->wParam != SIZE_MINIMIZED)
	{
	  WORD new_width = LOWORD (msg->lParam);
	  WORD new_height = HIWORD (msg->lParam);
	  gfloat old_width, old_height;

	  clutter_actor_get_size (CLUTTER_ACTOR (stage),
				  &old_width, &old_height);

	  if (new_width != old_width || new_height != old_height)
	    clutter_actor_set_size (CLUTTER_ACTOR (stage),
				    new_width, new_height);
	}
      break;

    case WM_SHOWWINDOW:
      if (msg->wParam)
	clutter_stage_win32_map (stage_win32);
      else
	clutter_stage_win32_unmap (stage_win32);
      break;

    case WM_ACTIVATE:
      if (msg->wParam == WA_INACTIVE)
        {
          if (_clutter_stage_is_activated (stage_win32->wrapper))
            {
              _clutter_stage_update_state (stage_win32->wrapper,
                                           CLUTTER_STAGE_STATE_ACTIVATED,
                                           0);
            }
        }
      else if (!_clutter_stage_is_activated (stage_win32->wrapper))
        {
          _clutter_stage_update_state (stage_win32->wrapper,
                                       0,
                                       CLUTTER_STAGE_STATE_ACTIVATED);
        }
      break;

    case WM_PAINT:
      CLUTTER_NOTE (BACKEND, "expose for stage:%p, redrawing", stage);
      clutter_stage_ensure_redraw (stage);
      break;

    case WM_DESTROY:
      {
        ClutterEvent *event = clutter_event_new (CLUTTER_DESTROY_NOTIFY);

        CLUTTER_NOTE (EVENT, "WM_DESTROY");

        event->any.stage = stage;

        take_and_queue_event (event);
      }
      break;

    case WM_CLOSE:
      {
        ClutterEvent *event = clutter_event_new (CLUTTER_DELETE);

        CLUTTER_NOTE (EVENT, "WM_CLOSE");

        event->any.stage = stage;

        take_and_queue_event (event);

        /* The default window proc will destroy the window so we want to
           prevent this to allow applications to optionally destroy the
           window themselves */
        return_value = TRUE;
      }
      break;

    case WM_LBUTTONDOWN:
      make_button_event (msg, stage, 1, 1, FALSE, core_pointer);
      break;

    case WM_MBUTTONDOWN:
      make_button_event (msg, stage, 2, 1, FALSE, core_pointer);
      break;

    case WM_RBUTTONDOWN:
      make_button_event (msg, stage, 3, 1, FALSE, core_pointer);
      break;

    case WM_LBUTTONUP:
      make_button_event (msg, stage, 1, 1, TRUE, core_pointer);
      break;

    case WM_MBUTTONUP:
      make_button_event (msg, stage, 2, 1, TRUE, core_pointer);
      break;

    case WM_RBUTTONUP:
      make_button_event (msg, stage, 3, 1, TRUE, core_pointer);
      break;

    case WM_LBUTTONDBLCLK:
      make_button_event (msg, stage, 1, 2, FALSE, core_pointer);
      break;

    case WM_MBUTTONDBLCLK:
      make_button_event (msg, stage, 2, 2, FALSE, core_pointer);
      break;

    case WM_RBUTTONDBLCLK:
      make_button_event (msg, stage, 3, 2, FALSE, core_pointer);
      break;

    case WM_MOUSEWHEEL:
      stage_win32->scroll_pos += (SHORT) HIWORD (msg->wParam);

      while (abs (stage_win32->scroll_pos) >= WHEEL_DELTA)
        {
          ClutterEvent *event = clutter_event_new (CLUTTER_SCROLL);
          POINT pt;

          event->scroll.time = msg->time;
          event->scroll.modifier_state =
            get_modifier_state (LOWORD (msg->wParam));
          event->any.stage = stage;

          clutter_event_set_device (event, core_pointer);

          /* conversion to window coordinates is required */
          pt.x = GET_X_LPARAM (msg->lParam);
          pt.y = GET_Y_LPARAM (msg->lParam);
          ScreenToClient (msg->hwnd, &pt);
          event->scroll.x = pt.x;
          event->scroll.y = pt.y;

          if (stage_win32->scroll_pos > 0)
            {
              event->scroll.direction = CLUTTER_SCROLL_UP;
              stage_win32->scroll_pos -= WHEEL_DELTA;
            }
          else
            {
              event->scroll.direction = CLUTTER_SCROLL_DOWN;
              stage_win32->scroll_pos += WHEEL_DELTA;
            }

          take_and_queue_event (event);
        }
      break;

    case WM_MOUSEMOVE:
      {
        ClutterEvent *event = clutter_event_new (CLUTTER_MOTION);

        event->motion.time = msg->time;
        event->motion.x = GET_X_LPARAM (msg->lParam);
        event->motion.y = GET_Y_LPARAM (msg->lParam);
        event->motion.modifier_state = get_modifier_state (msg->wParam);
        event->any.stage = stage;

        clutter_event_set_device (event, core_pointer);

        /* We need to start tracking when the mouse enters the stage if
           we're not already */
        if (!stage_win32->tracking_mouse)
          {
            ClutterEvent *crossing = clutter_event_new (CLUTTER_ENTER);
            TRACKMOUSEEVENT tmevent;

            tmevent.cbSize = sizeof (tmevent);
            tmevent.dwFlags = TME_LEAVE;
            tmevent.hwndTrack = stage_win32->hwnd;
            TrackMouseEvent (&tmevent);

            event->crossing.time = msg->time;
            event->crossing.x = event->motion.x;
            event->crossing.y = event->motion.y;
            event->crossing.stage = stage;
            event->crossing.source = CLUTTER_ACTOR (stage);
            event->crossing.related = NULL;

            clutter_event_set_device (event, core_pointer);

            /* we entered the stage */
            _clutter_input_device_set_stage (core_pointer, stage);

            take_and_queue_event (crossing);

            stage_win32->tracking_mouse = TRUE;
          }

        take_and_queue_event (event);
      }
      break;

    case WM_MOUSELEAVE:
      {
        ClutterEvent *event = clutter_event_new (CLUTTER_LEAVE);

        event->crossing.time = msg->time;
        event->crossing.x = msg->pt.x;
        event->crossing.y = msg->pt.y;
        event->crossing.stage = stage;
        event->crossing.source = CLUTTER_ACTOR (stage);
        event->crossing.related = NULL;

        clutter_event_set_device (event, core_pointer);

        /* we left the stage */
        _clutter_input_device_set_stage (core_pointer, NULL);

        /* When we get a leave message the mouse tracking is
           automatically cancelled so we'll need to start it again when
           the mouse next enters the window */
        stage_win32->tracking_mouse = FALSE;

        take_and_queue_event (event);
      }
      break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
      {
        ClutterEvent *event = clutter_event_new (CLUTTER_EVENT_NONE);
	int scan_code = (msg->lParam >> 16) & 0xff;
	int min = 0, max = CLUTTER_WIN32_KEY_MAP_SIZE, mid;
	BYTE key_states[256];

	/* Get the keyboard modifier states. GetKeyboardState
	   conveniently gets the key state that was current when the
	   last keyboard message read was generated */
	GetKeyboardState(key_states);

	/* Binary chop to check if we have a direct mapping for this
	   key code */
	while (min < max)
	  {
	    mid = (min + max) / 2;
	    if (clutter_win32_key_map[mid].win_sym == msg->wParam)
	      {
		event->key.keyval = clutter_win32_key_map[mid].clutter_sym;
		event->key.unicode_value = 0;
		break;
	      }
	    else if (clutter_win32_key_map[mid].win_sym < msg->wParam)
	      min = mid + 1;
	    else
	      max = mid;
	  }

	/* If we don't have a direct mapping then try getting the
	   unicode value of the key sym */
	if (min >= max)
	  {
	    WCHAR ch;
	    BYTE shift_state[256];

	    /* Translate to a Unicode value, but only take into
	       account the shift key. That way Ctrl+Shift+C will
	       generate a capital C virtual key code with a zero
	       unicode value for example */
	    memset (shift_state, 0, 256);
	    shift_state[VK_SHIFT] = key_states[VK_SHIFT];
	    shift_state[VK_LSHIFT] = key_states[VK_LSHIFT];
	    shift_state[VK_RSHIFT] = key_states[VK_RSHIFT];
	    shift_state[VK_CAPITAL] = key_states[VK_CAPITAL];

	    if (ToUnicode (msg->wParam, scan_code,
			   shift_state, &ch, 1, 0) == 1
		/* The codes in this range directly match the Latin 1
		   codes so we can just use the Unicode value as the
		   key sym */
		&& ch >= 0x20 && ch <= 0xff)
	      event->key.keyval = ch;
	    else
	      /* Otherwise we don't know what the key means but the
		 application might be able to do something with the
		 scan code so we might as well still generate the
		 event */
	      event->key.keyval = CLUTTER_KEY_VoidSymbol;

	    /* Get the unicode value of the keypress again using the
	       full modifier state */
	    if (ToUnicode (msg->wParam, scan_code,
			   key_states, &ch, 1, 0) == 1)
		event->key.unicode_value = ch;
	    else
		event->key.unicode_value = 0;
	  }

	event->key.type = msg->message == WM_KEYDOWN
	  || msg->message == WM_SYSKEYDOWN
	  ? CLUTTER_KEY_PRESS : CLUTTER_KEY_RELEASE;
	event->key.time = msg->time;
	event->key.modifier_state = get_key_modifier_state (key_states);
	event->key.hardware_keycode = scan_code;
        event->any.stage = stage;

        clutter_event_set_device (event, core_keyboard);

        take_and_queue_event (event);
      }
      break;

    case WM_GETMINMAXINFO:
      {
	MINMAXINFO *min_max_info = (MINMAXINFO *) msg->lParam;
	_clutter_stage_win32_get_min_max_info (stage_win32, min_max_info);
        return_value = TRUE;
      }
      break;

    case WM_SETCURSOR:
      /* If the cursor is in the window's client area and the stage's
         cursor should be invisible then we'll set a blank cursor
         instead */
      if (LOWORD (msg->lParam) == HTCLIENT && !stage_win32->is_cursor_visible)
        {
          return_value = TRUE;
          _clutter_stage_win32_update_cursor (stage_win32);
        }
      break;
    }

  return return_value;
}

LRESULT CALLBACK
_clutter_stage_win32_window_proc (HWND hwnd, UINT umsg,
				  WPARAM wparam, LPARAM lparam)
{
  gboolean message_handled = FALSE;

  /* Windows sends some messages such as WM_GETMINMAXINFO and
     WM_CREATE before returning from CreateWindow. The stage point
     will not yet be stored in the Window structure in this case so we
     need to skip these messages because we can't know which stage the
     window belongs to */
  if (GetWindowLongPtrW (hwnd, 0))
    {
      MSG msg;
      DWORD message_pos = GetMessagePos ();

      /* Convert the parameters to a MSG struct */
      msg.hwnd = hwnd;
      msg.message = umsg;
      msg.wParam = wparam;
      msg.lParam = lparam;
      msg.time = GetMessageTime ();
      /* Neither MAKE_POINTS nor GET_[XY]_LPARAM is defined in MinGW
	 headers so we need to convert to a signed type explicitly */
      msg.pt.x = (SHORT) LOWORD (message_pos);
      msg.pt.y = (SHORT) HIWORD (message_pos);

      /* Process the message */
      message_handled = clutter_win32_handle_event (&msg);
    }

  if (!message_handled)
    return DefWindowProcW (hwnd, umsg, wparam, lparam);
  else
    return 0;
}
