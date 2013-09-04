/*
 * Wayland Support
 *
 * Copyright (C) 2013 Intel Corporation
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/*
 * Copyright © 2010-2011 Intel Corporation
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2012 Collabora, Ltd.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* The file is based on src/input.c from Weston */

#define _GNU_SOURCE

#include "config.h"

#include <glib.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <clutter/evdev/clutter-evdev.h>

#include "meta-wayland-private.h"

static MetaWaylandSeat *
meta_wayland_keyboard_get_seat (MetaWaylandKeyboard *keyboard)
{
  MetaWaylandSeat *seat = wl_container_of (keyboard, seat, keyboard);

  return seat;
}

static int
create_anonymous_file (off_t size,
                       GError **error)
{
  static const char template[] = "mutter-shared-XXXXXX";
  char *path;
  int fd, flags;

  fd = g_file_open_tmp (template, &path, error);

  if (fd == -1)
    return -1;

  unlink (path);
  g_free (path);

  flags = fcntl (fd, F_GETFD);
  if (flags == -1)
    goto err;

  if (fcntl (fd, F_SETFD, flags | FD_CLOEXEC) == -1)
    goto err;

  if (ftruncate (fd, size) < 0)
    goto err;

  return fd;

 err:
  g_set_error_literal (error,
                       G_FILE_ERROR,
                       g_file_error_from_errno (errno),
                       strerror (errno));
  close (fd);

  return -1;
}

static gboolean
meta_wayland_xkb_info_new_keymap (MetaWaylandXkbInfo *xkb_info)
{
  GError *error = NULL;
  char *keymap_str;

  xkb_info->shift_mod =
    xkb_map_mod_get_index (xkb_info->keymap, XKB_MOD_NAME_SHIFT);
  xkb_info->caps_mod =
    xkb_map_mod_get_index (xkb_info->keymap, XKB_MOD_NAME_CAPS);
  xkb_info->ctrl_mod =
    xkb_map_mod_get_index (xkb_info->keymap, XKB_MOD_NAME_CTRL);
  xkb_info->alt_mod =
    xkb_map_mod_get_index (xkb_info->keymap, XKB_MOD_NAME_ALT);
  xkb_info->mod2_mod = xkb_map_mod_get_index (xkb_info->keymap, "Mod2");
  xkb_info->mod3_mod = xkb_map_mod_get_index (xkb_info->keymap, "Mod3");
  xkb_info->super_mod =
    xkb_map_mod_get_index (xkb_info->keymap, XKB_MOD_NAME_LOGO);
  xkb_info->mod5_mod = xkb_map_mod_get_index (xkb_info->keymap, "Mod5");

  keymap_str = xkb_map_get_as_string (xkb_info->keymap);
  if (keymap_str == NULL)
    {
      g_warning ("failed to get string version of keymap\n");
      return FALSE;
    }
  xkb_info->keymap_size = strlen (keymap_str) + 1;

  xkb_info->keymap_fd = create_anonymous_file (xkb_info->keymap_size, &error);
  if (xkb_info->keymap_fd < 0)
    {
      g_warning ("creating a keymap file for %lu bytes failed: %s\n",
                 (unsigned long) xkb_info->keymap_size,
                 error->message);
      g_clear_error (&error);
      goto err_keymap_str;
    }

  xkb_info->keymap_area = mmap (NULL, xkb_info->keymap_size,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED, xkb_info->keymap_fd, 0);
  if (xkb_info->keymap_area == MAP_FAILED)
    {
      g_warning ("failed to mmap() %lu bytes\n",
                 (unsigned long) xkb_info->keymap_size);
      goto err_dev_zero;
    }
  strcpy (xkb_info->keymap_area, keymap_str);
  free (keymap_str);

  return TRUE;

err_dev_zero:
  close (xkb_info->keymap_fd);
  xkb_info->keymap_fd = -1;
err_keymap_str:
  free (keymap_str);
  return FALSE;
}

static gboolean
meta_wayland_keyboard_build_global_keymap (struct xkb_context *xkb_context,
                                           struct xkb_rule_names *xkb_names,
                                           MetaWaylandXkbInfo *xkb_info)
{
  xkb_info->keymap = xkb_map_new_from_names (xkb_context,
                                             xkb_names,
                                             0 /* flags */);
  if (xkb_info->keymap == NULL)
    {
      g_warning ("failed to compile global XKB keymap\n"
                 "  tried rules %s, model %s, layout %s, variant %s, "
                 "options %s\n",
                 xkb_names->rules,
                 xkb_names->model,
                 xkb_names->layout,
                 xkb_names->variant,
                 xkb_names->options);
      return FALSE;
    }

  if (!meta_wayland_xkb_info_new_keymap (xkb_info))
    return FALSE;

  return TRUE;
}

static void
lose_keyboard_focus (struct wl_listener *listener, void *data)
{
  MetaWaylandKeyboard *keyboard =
    wl_container_of (listener, keyboard, focus_listener);

  keyboard->focus_resource = NULL;
}

static gboolean
default_grab_key (MetaWaylandKeyboardGrab *grab,
                  uint32_t time, uint32_t key, uint32_t state)
{
  MetaWaylandKeyboard *keyboard = grab->keyboard;
  struct wl_resource *resource;
  uint32_t serial;

  resource = keyboard->focus_resource;
  if (resource)
    {
      struct wl_client *client = wl_resource_get_client (resource);
      struct wl_display *display = wl_client_get_display (client);
      serial = wl_display_next_serial (display);
      wl_keyboard_send_key (resource, serial, time, key, state);
    }

  return resource != NULL;
}

static struct wl_resource *
find_resource_for_surface (struct wl_list *list, MetaWaylandSurface *surface)
{
  struct wl_client *client;

  if (!surface)
    return NULL;

  if (!surface->resource)
    return NULL;

  client = wl_resource_get_client (surface->resource);

  return wl_resource_find_for_client (list, client);
}

static void
default_grab_modifiers (MetaWaylandKeyboardGrab *grab, uint32_t serial,
                        uint32_t mods_depressed, uint32_t mods_latched,
                        uint32_t mods_locked, uint32_t group)
{
  MetaWaylandKeyboard *keyboard = grab->keyboard;
  MetaWaylandSeat *seat = meta_wayland_keyboard_get_seat (keyboard);
  MetaWaylandPointer *pointer = &seat->pointer;
  struct wl_resource *resource, *pr;

  resource = keyboard->focus_resource;
  if (!resource)
    return;

  wl_keyboard_send_modifiers (resource, serial, mods_depressed,
                              mods_latched, mods_locked, group);

  if (pointer && pointer->focus && pointer->focus != keyboard->focus)
    {
      pr = find_resource_for_surface (&keyboard->resource_list,
                                      pointer->focus);
      if (pr)
        {
          wl_keyboard_send_modifiers (pr, serial,
				      mods_depressed,
                                      mods_latched,
                                      mods_locked,
                                      group);
        }
    }
}

static const MetaWaylandKeyboardGrabInterface
  default_keyboard_grab_interface = {
  default_grab_key,
  default_grab_modifiers,
};

static gboolean
modal_key (MetaWaylandKeyboardGrab *grab,
	   uint32_t                 time,
	   uint32_t                 key,
	   uint32_t                 state)
{
  /* FALSE means: let the event through to clutter */
  return FALSE;
}

static void
modal_modifiers (MetaWaylandKeyboardGrab *grab,
		 uint32_t                 serial,
		 uint32_t                 mods_depressed,
		 uint32_t                 mods_latched,
		 uint32_t                 mods_locked,
		 uint32_t                 group)
{
}

static MetaWaylandKeyboardGrabInterface modal_grab = {
  modal_key,
  modal_modifiers,
};

gboolean
meta_wayland_keyboard_init (MetaWaylandKeyboard *keyboard,
                            struct wl_display   *display,
			    gboolean             is_evdev)
{
  ClutterDeviceManager *manager;

  memset (keyboard, 0, sizeof *keyboard);

  wl_list_init (&keyboard->resource_list);
  wl_array_init (&keyboard->keys);
  keyboard->focus_listener.notify = lose_keyboard_focus;
  keyboard->default_grab.interface = &default_keyboard_grab_interface;
  keyboard->default_grab.keyboard = keyboard;
  keyboard->grab = &keyboard->default_grab;
  wl_signal_init (&keyboard->focus_signal);

  keyboard->display = display;

  keyboard->xkb_context = xkb_context_new (0 /* flags */);

  meta_wayland_keyboard_build_global_keymap (keyboard->xkb_context,
					     &keyboard->xkb_names,
					     &keyboard->xkb_info);

  keyboard->is_evdev = is_evdev;
  if (is_evdev)
    {
      manager = clutter_device_manager_get_default ();

      clutter_evdev_set_keyboard_map (manager, keyboard->xkb_info.keymap);
    }

  return TRUE;
}

static void
meta_wayland_xkb_info_destroy (MetaWaylandXkbInfo *xkb_info)
{
  if (xkb_info->keymap)
    xkb_map_unref (xkb_info->keymap);

  if (xkb_info->keymap_area)
    munmap (xkb_info->keymap_area, xkb_info->keymap_size);
  if (xkb_info->keymap_fd >= 0)
    close (xkb_info->keymap_fd);
}

static gboolean
state_equal (MetaWaylandXkbState *one,
	     MetaWaylandXkbState *two)
{
  return one->mods_depressed == two->mods_depressed &&
    one->mods_latched == two->mods_latched &&
    one->mods_locked == two->mods_locked &&
    one->group == two->group;
}

static void
set_modifiers (MetaWaylandKeyboard *keyboard,
               guint32              serial,
               ClutterEvent        *event)
{
  MetaWaylandKeyboardGrab *grab = keyboard->grab;
  MetaWaylandXkbState new_state;
  guint effective_state;

  clutter_event_get_state_full (event,
				NULL,
				&new_state.mods_depressed,
				&new_state.mods_latched,
				&new_state.mods_locked,
				&effective_state);
  new_state.group = (effective_state >> 13) & 0x3;

  if (state_equal (&keyboard->modifier_state, &new_state))
    return;

  keyboard->modifier_state = new_state;

  grab->interface->modifiers (grab,
                              serial,
                              new_state.mods_depressed,
			      new_state.mods_latched,
			      new_state.mods_locked,
                              new_state.group);
}

static gboolean
update_pressed_keys (MetaWaylandKeyboard   *keyboard,
		     uint32_t               evdev_code,
		     gboolean               is_press)
{
  if (is_press)
    {
      uint32_t *end = (void *) ((char *) keyboard->keys.data +
                                keyboard->keys.size);
      uint32_t *k;

      /* We want to ignore events that are sent because of auto-repeat. In
	 the Clutter event stream these appear as a single key press
	 event. We can detect that because the key will already have been
	 pressed */
      for (k = keyboard->keys.data; k < end; k++)
        if (*k == evdev_code)
          return TRUE;

      /* Otherwise add the key to the list of pressed keys */
      k = wl_array_add (&keyboard->keys, sizeof (*k));
      *k = evdev_code;
    }
  else
    {
      uint32_t *end = (void *) ((char *) keyboard->keys.data +
                                keyboard->keys.size);
      uint32_t *k;

      /* Remove the key from the array */
      for (k = keyboard->keys.data; k < end; k++)
        if (*k == evdev_code)
          {
            *k = *(end - 1);
            keyboard->keys.size -= sizeof (*k);

            goto found;
          }

      g_warning ("unexpected key release event for key 0x%x", evdev_code);
      return FALSE;

    found:
      (void) 0;
    }

  return FALSE;
}

gboolean
meta_wayland_keyboard_handle_event (MetaWaylandKeyboard *keyboard,
                                    const ClutterKeyEvent *event)
{
  gboolean is_press = event->type == CLUTTER_KEY_PRESS;
  guint xkb_keycode, evdev_code;
  uint32_t serial;
  gboolean autorepeat;
  gboolean handled;

  xkb_keycode = event->hardware_keycode;
  if (event->device == NULL ||
      !clutter_input_device_keycode_to_evdev (event->device,
					      xkb_keycode, &evdev_code))
    evdev_code = xkb_keycode - 8; /* What everyone is doing in practice... */

  autorepeat = update_pressed_keys (keyboard, evdev_code, is_press);

  meta_verbose ("Handling key %s%s event code %d\n",
		is_press ? "press" : "release",
		autorepeat ? " (autorepeat)" : "",
		xkb_keycode);

  if (autorepeat)
    return FALSE;

  serial = wl_display_next_serial (keyboard->display);

  set_modifiers (keyboard, serial, (ClutterEvent*)event);

  handled = keyboard->grab->interface->key (keyboard->grab,
					    event->time,
					    evdev_code,
					    is_press);

  if (handled)
    meta_verbose ("Sent event to wayland client\n");
  else
    meta_verbose ("No wayland surface is focused, continuing normal operation\n");

  return handled;
}

void
meta_wayland_keyboard_set_focus (MetaWaylandKeyboard *keyboard,
                                 MetaWaylandSurface *surface)
{
  struct wl_resource *resource;
  uint32_t serial;

  if (keyboard->focus_resource && keyboard->focus != surface)
    {
      struct wl_display *display;
      struct wl_client *client;

      resource = keyboard->focus_resource;
      client = wl_resource_get_client (resource);
      display = wl_client_get_display (client);
      serial = wl_display_next_serial (display);
      wl_keyboard_send_leave (resource, serial, keyboard->focus->resource);
      wl_list_remove (&keyboard->focus_listener.link);
    }

  resource = find_resource_for_surface (&keyboard->resource_list, surface);
  if (resource &&
      (keyboard->focus != surface || keyboard->focus_resource != resource))
    {
      struct wl_client *client = wl_resource_get_client (resource);
      struct wl_display *display;

      display = wl_client_get_display (client);
      serial = wl_display_next_serial (display);

      /* If we're in a modal grab, the client is focused but doesn't see
	 modifiers or pressed keys (and fix that up when we exit the modal) */
      if (keyboard->grab->interface == &modal_grab)
	{
	  struct wl_array empty;
	  wl_array_init (&empty);

	  wl_keyboard_send_modifiers (resource, serial,
				      0, 0, 0, 0);
	  wl_keyboard_send_enter (resource, serial, surface->resource,
				  &empty);
	}
      else
	{
	  wl_keyboard_send_modifiers (resource, serial,
				      keyboard->modifier_state.mods_depressed,
				      keyboard->modifier_state.mods_latched,
				      keyboard->modifier_state.mods_locked,
				      keyboard->modifier_state.group);
	  wl_keyboard_send_enter (resource, serial, surface->resource,
				  &keyboard->keys);
	}
      wl_resource_add_destroy_listener (resource, &keyboard->focus_listener);
      keyboard->focus_serial = serial;
    }

  keyboard->focus_resource = resource;
  keyboard->focus = surface;
  wl_signal_emit (&keyboard->focus_signal, keyboard);
}

void
meta_wayland_keyboard_start_grab (MetaWaylandKeyboard *keyboard,
                                  MetaWaylandKeyboardGrab *grab)
{
  keyboard->grab = grab;
  grab->keyboard = keyboard;

  /* XXX focus? */
}

void
meta_wayland_keyboard_end_grab (MetaWaylandKeyboard *keyboard)
{
  keyboard->grab = &keyboard->default_grab;
}

void
meta_wayland_keyboard_release (MetaWaylandKeyboard *keyboard)
{
  g_free ((char *) keyboard->xkb_names.rules);
  g_free ((char *) keyboard->xkb_names.model);
  g_free ((char *) keyboard->xkb_names.layout);
  g_free ((char *) keyboard->xkb_names.variant);
  g_free ((char *) keyboard->xkb_names.options);

  meta_wayland_xkb_info_destroy (&keyboard->xkb_info);
  xkb_context_unref (keyboard->xkb_context);

  /* XXX: What about keyboard->resource_list? */
  if (keyboard->focus_resource)
    wl_list_remove (&keyboard->focus_listener.link);
  wl_array_release (&keyboard->keys);
}

gboolean
meta_wayland_keyboard_begin_modal (MetaWaylandKeyboard *keyboard,
				   guint32              timestamp)
{
  MetaWaylandKeyboardGrab *grab;
  uint32_t *end = (void *) ((char *) keyboard->keys.data +
			    keyboard->keys.size);
  uint32_t *k;
  uint32_t serial;

  meta_verbose ("Asked to acquire modal keyboard grab, timestamp %d\n", timestamp);

  if (keyboard->grab != &keyboard->default_grab)
    return FALSE;

  if (keyboard->focus)
    {
      /* Fake key release events for the focused app */
      serial = wl_display_next_serial (keyboard->display);
      keyboard->grab->interface->modifiers (keyboard->grab,
					    serial,
					    0, 0, 0, 0);

      for (k = keyboard->keys.data; k < end; k++)
	{
	  keyboard->grab->interface->key (keyboard->grab,
					  timestamp,
					  *k, 0);
	}
    }

  grab = g_slice_new0 (MetaWaylandKeyboardGrab);
  grab->interface = &modal_grab;
  meta_wayland_keyboard_start_grab (keyboard, grab);

  meta_verbose ("Acquired modal keyboard grab, timestamp %d\n", timestamp);

  return TRUE;
}

void
meta_wayland_keyboard_end_modal (MetaWaylandKeyboard *keyboard,
				 guint32              timestamp)
{
  MetaWaylandKeyboardGrab *grab;
  uint32_t *end = (void *) ((char *) keyboard->keys.data +
			    keyboard->keys.size);
  uint32_t *k;
  uint32_t serial;

  grab = keyboard->grab;

  g_assert (grab->interface == &modal_grab);

  meta_wayland_keyboard_end_grab (keyboard);
  g_slice_free (MetaWaylandKeyboardGrab, grab);

  if (keyboard->focus)
    {
      /* Fake key press events for the focused app */
      serial = wl_display_next_serial (keyboard->display);
      keyboard->grab->interface->modifiers (keyboard->grab,
					    serial,
					    keyboard->modifier_state.mods_depressed,
					    keyboard->modifier_state.mods_latched, 
					    keyboard->modifier_state.mods_locked,
					    keyboard->modifier_state.group);

      for (k = keyboard->keys.data; k < end; k++)
	{
	  keyboard->grab->interface->key (keyboard->grab,
					  timestamp,
					  *k, 1);
	}
    }

  meta_verbose ("Released modal keyboard grab, timestamp %d\n", timestamp);
}
