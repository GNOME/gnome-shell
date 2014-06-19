/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002, 2003, 2004 Red Hat, Inc.
 * Copyright (C) 2003, 2004 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "events.h"

#include "display-private.h"
#include "window-private.h"
#include "backends/meta-backend.h"
#include "backends/x11/meta-backend-x11.h"

#ifdef HAVE_NATIVE_BACKEND
#include "backends/native/meta-backend-native.h"
#include "backends/native/meta-idle-monitor-native.h"
#endif

#include "x11/events.h"
#include "wayland/meta-wayland-private.h"
#include "meta-surface-actor.h"

static MetaWindow *
get_window_for_event (MetaDisplay        *display,
                      const ClutterEvent *event)
{
  ClutterActor *source;

  if (display->grab_op != META_GRAB_OP_NONE)
    return display->grab_window;

  /* Always use the key focused window for key events. */
  switch (event->type)
    {
    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
      return display->focus_window;
    default:
      break;
    }

  source = clutter_event_get_source (event);
  if (META_IS_SURFACE_ACTOR (source))
    return meta_surface_actor_get_window (META_SURFACE_ACTOR (source));

  return NULL;
}

static void
handle_idletime_for_event (const ClutterEvent *event)
{
#ifdef HAVE_NATIVE_BACKEND
  /* This is handled by XSync under X11. */
  MetaBackend *backend = meta_get_backend ();

  if (META_IS_BACKEND_NATIVE (backend))
    {
      ClutterInputDevice *device, *source_device;
      MetaIdleMonitor *core_monitor, *device_monitor;
      int device_id;

      device = clutter_event_get_device (event);
      if (device == NULL)
        return;

      device_id = clutter_input_device_get_device_id (device);

      core_monitor = meta_idle_monitor_get_core ();
      device_monitor = meta_idle_monitor_get_for_device (device_id);

      meta_idle_monitor_native_reset_idletime (core_monitor);
      meta_idle_monitor_native_reset_idletime (device_monitor);

      source_device = clutter_event_get_source_device (event);
      if (source_device != device)
        {
          device_id = clutter_input_device_get_device_id (device);
          device_monitor = meta_idle_monitor_get_for_device (device_id);
          meta_idle_monitor_native_reset_idletime (device_monitor);
        }
    }
#endif /* HAVE_NATIVE_BACKEND */
}

static gboolean
meta_display_handle_event (MetaDisplay        *display,
                           const ClutterEvent *event)
{
  MetaWindow *window;
  gboolean bypass_clutter = FALSE, bypass_wayland = FALSE;
  MetaWaylandCompositor *compositor = NULL;

  if (meta_is_wayland_compositor ())
    {
      compositor = meta_wayland_compositor_get_default ();
      meta_wayland_compositor_update (compositor, event);
    }

  handle_idletime_for_event (event);

  window = get_window_for_event (display, event);

  display->current_time = event->any.time;

  if (window && !window->override_redirect &&
      (event->type == CLUTTER_KEY_PRESS ||
       event->type == CLUTTER_BUTTON_PRESS ||
       event->type == CLUTTER_TOUCH_BEGIN))
    {
      if (CurrentTime == display->current_time)
        {
          /* We can't use missing (i.e. invalid) timestamps to set user time,
           * nor do we want to use them to sanity check other timestamps.
           * See bug 313490 for more details.
           */
          meta_warning ("Event has no timestamp! You may be using a broken "
                        "program such as xse.  Please ask the authors of that "
                        "program to fix it.\n");
        }
      else
        {
          meta_window_set_user_time (window, display->current_time);
          meta_display_sanity_check_timestamps (display, display->current_time);
        }
    }

  if (display->grab_window == window &&
      meta_grab_op_is_moving_or_resizing (display->grab_op))
    {
      if (meta_window_handle_mouse_grab_op_event (window, event))
        {
          bypass_clutter = TRUE;
          bypass_wayland = TRUE;
          goto out;
        }
    }

  /* For key events, it's important to enforce single-handling, or
   * we can get into a confused state. So if a keybinding is
   * handled (because it's one of our hot-keys, or because we are
   * in a keyboard-grabbed mode like moving a window, we don't
   * want to pass the key event to the compositor or Wayland at all.
   */
  if (meta_keybindings_process_event (display, window, event))
    {
      bypass_clutter = TRUE;
      bypass_wayland = TRUE;
      goto out;
    }

  if (window)
    {
      /* Swallow all events on windows that come our way. */
      bypass_clutter = TRUE;

      /* Under X11, we have a Sync grab and in order to send it back to
       * clients, we have to explicitly replay it.
       *
       * Under Wayland, we retrieve all events and we have to make sure
       * to filter them out from Wayland clients.
       */
      if (meta_window_handle_ungrabbed_event (window, event))
        {
          bypass_wayland = TRUE;
        }
      else
        {
          MetaBackend *backend = meta_get_backend ();
          if (META_IS_BACKEND_X11 (backend))
            {
              Display *xdisplay = meta_backend_x11_get_xdisplay (META_BACKEND_X11 (backend));
              meta_verbose ("Allowing events time %u\n",
                            (unsigned int)event->button.time);
              XIAllowEvents (xdisplay, clutter_event_get_device_id (event),
                             XIReplayDevice, event->button.time);
            }
        }

      goto out;
    }

 out:
  /* If the compositor has a grab, don't pass that through to Wayland */
  if (display->grab_op == META_GRAB_OP_COMPOSITOR)
    bypass_wayland = TRUE;

  /* If a Wayland client has a grab, don't pass that through to Clutter */
  if (display->grab_op == META_GRAB_OP_WAYLAND_POPUP)
    bypass_clutter = TRUE;

  if (compositor && !bypass_wayland)
    {
      if (meta_wayland_compositor_handle_event (compositor, event))
        bypass_clutter = TRUE;
    }

  display->current_time = CurrentTime;
  return bypass_clutter;
}

static gboolean
event_callback (const ClutterEvent *event,
                gpointer            data)
{
  MetaDisplay *display = data;

  return meta_display_handle_event (display, event);
}

void
meta_display_init_events (MetaDisplay *display)
{
  meta_display_init_events_x11 (display);
  display->clutter_event_filter = clutter_event_add_filter (NULL,
                                                            event_callback,
                                                            NULL,
                                                            display);
}

void
meta_display_free_events (MetaDisplay *display)
{
  meta_display_free_events_x11 (display);
  clutter_event_remove_filter (display->clutter_event_filter);
  display->clutter_event_filter = 0;
}
