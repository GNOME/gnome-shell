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

#include "backends/meta-backend-private.h"

#include "meta-wayland-private.h"

static void meta_wayland_keyboard_update_xkb_state (MetaWaylandKeyboard *keyboard);
static void notify_modifiers (MetaWaylandKeyboard *keyboard);

static void
unbind_resource (struct wl_resource *resource)
{
  wl_list_remove (wl_resource_get_link (resource));
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

static void
inform_clients_of_new_keymap (MetaWaylandKeyboard *keyboard)
{
  struct wl_resource *keyboard_resource;

  wl_resource_for_each (keyboard_resource, &keyboard->resource_list)
    {
      wl_keyboard_send_keymap (keyboard_resource,
			       WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
			       keyboard->xkb_info.keymap_fd,
			       keyboard->xkb_info.keymap_size);
    }
  wl_resource_for_each (keyboard_resource, &keyboard->focus_resource_list)
    {
      wl_keyboard_send_keymap (keyboard_resource,
                               WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                               keyboard->xkb_info.keymap_fd,
                               keyboard->xkb_info.keymap_size);
    }
}

static void
meta_wayland_keyboard_take_keymap (MetaWaylandKeyboard *keyboard,
				   struct xkb_keymap   *keymap)
{
  MetaWaylandXkbInfo  *xkb_info = &keyboard->xkb_info;
  GError *error = NULL;
  char *keymap_str;
  size_t previous_size;

  if (keymap == NULL)
    {
      g_warning ("Attempting to set null keymap (compilation probably failed)");
      return;
    }

  xkb_keymap_unref (xkb_info->keymap);
  xkb_info->keymap = xkb_keymap_ref (keymap);

  meta_wayland_keyboard_update_xkb_state (keyboard);

  keymap_str = xkb_map_get_as_string (xkb_info->keymap);
  if (keymap_str == NULL)
    {
      g_warning ("failed to get string version of keymap");
      return;
    }
  previous_size = xkb_info->keymap_size;
  xkb_info->keymap_size = strlen (keymap_str) + 1;

  if (xkb_info->keymap_fd >= 0)
    close (xkb_info->keymap_fd);

  xkb_info->keymap_fd = create_anonymous_file (xkb_info->keymap_size, &error);
  if (xkb_info->keymap_fd < 0)
    {
      g_warning ("creating a keymap file for %lu bytes failed: %s",
                 (unsigned long) xkb_info->keymap_size,
                 error->message);
      g_clear_error (&error);
      goto err_keymap_str;
    }

  if (xkb_info->keymap_area)
    munmap (xkb_info->keymap_area, previous_size);

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

  inform_clients_of_new_keymap (keyboard);

  notify_modifiers (keyboard);

  return;

err_dev_zero:
  close (xkb_info->keymap_fd);
  xkb_info->keymap_fd = -1;
err_keymap_str:
  free (keymap_str);
  return;
}

static void
on_keymap_changed (MetaBackend *backend,
                   gpointer     data)
{
  MetaWaylandKeyboard *keyboard = data;

  meta_wayland_keyboard_take_keymap (keyboard, meta_backend_get_keymap (backend));
}

static void
on_keymap_layout_group_changed (MetaBackend *backend,
                                guint        idx,
                                gpointer     data)
{
  MetaWaylandKeyboard *keyboard = data;
  xkb_mod_mask_t depressed_mods;
  xkb_mod_mask_t latched_mods;
  xkb_mod_mask_t locked_mods;
  struct xkb_state *state;

  state = keyboard->xkb_info.state;

  depressed_mods = xkb_state_serialize_mods (state, XKB_STATE_MODS_DEPRESSED);
  latched_mods = xkb_state_serialize_mods (state, XKB_STATE_MODS_LATCHED);
  locked_mods = xkb_state_serialize_mods (state, XKB_STATE_MODS_LOCKED);

  xkb_state_update_mask (state, depressed_mods, latched_mods, locked_mods, 0, 0, idx);

  notify_modifiers (keyboard);
}

static void
keyboard_handle_focus_surface_destroy (struct wl_listener *listener, void *data)
{
  MetaWaylandKeyboard *keyboard = wl_container_of (listener, keyboard, focus_surface_listener);

  meta_wayland_keyboard_set_focus (keyboard, NULL);
}

static gboolean
notify_key (MetaWaylandKeyboard *keyboard,
            uint32_t time, uint32_t key, uint32_t state)
{
  struct wl_resource *resource;
  struct wl_list *l;

  l = &keyboard->focus_resource_list;
  if (!wl_list_empty (l))
    {
      struct wl_client *client = wl_resource_get_client (keyboard->focus_surface->resource);
      struct wl_display *display = wl_client_get_display (client);
      uint32_t serial = wl_display_next_serial (display);

      wl_resource_for_each (resource, l)
        {
          wl_keyboard_send_key (resource, serial, time, key, state);
        }
    }

  /* Eat the key events if we have a focused surface. */
  return (keyboard->focus_surface != NULL);
}

static void
notify_modifiers (MetaWaylandKeyboard *keyboard)
{
  struct xkb_state *state;
  struct wl_resource *resource;
  struct wl_list *l;

  state = keyboard->xkb_info.state;

  l = &keyboard->focus_resource_list;
  if (!wl_list_empty (l))
    {
      uint32_t serial = wl_display_next_serial (keyboard->display);

      wl_resource_for_each (resource, l)
        {
          wl_keyboard_send_modifiers (resource,
                                      serial,
                                      xkb_state_serialize_mods (state, XKB_STATE_MODS_DEPRESSED),
                                      xkb_state_serialize_mods (state, XKB_STATE_MODS_LATCHED),
                                      xkb_state_serialize_mods (state, XKB_STATE_MODS_LOCKED),
                                      xkb_state_serialize_layout (state, XKB_STATE_LAYOUT_EFFECTIVE));
        }
    }
}

static void
meta_wayland_keyboard_update_xkb_state (MetaWaylandKeyboard *keyboard)
{
  MetaWaylandXkbInfo *xkb_info = &keyboard->xkb_info;
  xkb_mod_mask_t latched, locked, group;

  /* Preserve latched/locked modifiers state */
  if (xkb_info->state)
    {
      latched = xkb_state_serialize_mods (xkb_info->state, XKB_STATE_MODS_LATCHED);
      locked = xkb_state_serialize_mods (xkb_info->state, XKB_STATE_MODS_LOCKED);
      group = xkb_state_serialize_layout (xkb_info->state, XKB_STATE_LAYOUT_EFFECTIVE);
      xkb_state_unref (xkb_info->state);
    }
  else
    latched = locked = group = 0;

  xkb_info->state = xkb_state_new (xkb_info->keymap);

  if (latched || locked || group)
    xkb_state_update_mask (xkb_info->state, 0, latched, locked, 0, 0, group);
}

static void
notify_key_repeat_for_resource (MetaWaylandKeyboard *keyboard,
                                struct wl_resource  *keyboard_resource)
{
  if (wl_resource_get_version (keyboard_resource) >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)
    {
      gboolean repeat;
      unsigned int delay, rate;

      repeat = g_settings_get_boolean (keyboard->settings, "repeat");

      if (repeat)
        {
          unsigned int interval;
          interval = g_settings_get_uint (keyboard->settings, "repeat-interval");
          /* Our setting is in the milliseconds between keys. "rate" is the number
           * of keys per second. */
          rate = (1000 / interval);
          delay = g_settings_get_uint (keyboard->settings, "delay");
        }
      else
        {
          rate = 0;
          delay = 0;
        }

      wl_keyboard_send_repeat_info (keyboard_resource, rate, delay);
    }
}

static void
notify_key_repeat (MetaWaylandKeyboard *keyboard)
{
  struct wl_resource *keyboard_resource;

  wl_resource_for_each (keyboard_resource, &keyboard->resource_list)
    {
      notify_key_repeat_for_resource (keyboard, keyboard_resource);
    }

  wl_resource_for_each (keyboard_resource, &keyboard->focus_resource_list)
    {
      notify_key_repeat_for_resource (keyboard, keyboard_resource);
    }
}

static void
settings_changed (GSettings           *settings,
                  const char          *key,
                  gpointer             data)
{
  MetaWaylandKeyboard *keyboard = data;

  notify_key_repeat (keyboard);
}

void
meta_wayland_keyboard_init (MetaWaylandKeyboard *keyboard,
                            struct wl_display   *display)
{
  MetaBackend *backend = meta_get_backend ();

  memset (keyboard, 0, sizeof *keyboard);

  keyboard->display = display;

  wl_list_init (&keyboard->resource_list);
  wl_list_init (&keyboard->focus_resource_list);

  keyboard->focus_surface_listener.notify = keyboard_handle_focus_surface_destroy;

  keyboard->xkb_info.keymap_fd = -1;

  keyboard->settings = g_settings_new ("org.gnome.desktop.peripherals.keyboard");
  g_signal_connect (keyboard->settings, "changed",
                    G_CALLBACK (settings_changed), keyboard);

  g_signal_connect (backend, "keymap-changed",
                    G_CALLBACK (on_keymap_changed), keyboard);
  g_signal_connect (backend, "keymap-layout-group-changed",
                    G_CALLBACK (on_keymap_layout_group_changed), keyboard);
  meta_wayland_keyboard_take_keymap (keyboard, meta_backend_get_keymap (backend));
}

static void
meta_wayland_xkb_info_destroy (MetaWaylandXkbInfo *xkb_info)
{
  xkb_keymap_unref (xkb_info->keymap);
  xkb_state_unref (xkb_info->state);

  if (xkb_info->keymap_area)
    munmap (xkb_info->keymap_area, xkb_info->keymap_size);
  if (xkb_info->keymap_fd >= 0)
    close (xkb_info->keymap_fd);
}

void
meta_wayland_keyboard_release (MetaWaylandKeyboard *keyboard)
{
  MetaBackend *backend = meta_get_backend ();

  g_signal_handlers_disconnect_by_func (backend, on_keymap_changed, keyboard);
  g_signal_handlers_disconnect_by_func (backend, on_keymap_layout_group_changed, keyboard);

  meta_wayland_keyboard_set_focus (keyboard, NULL);
  meta_wayland_xkb_info_destroy (&keyboard->xkb_info);

  /* XXX: What about keyboard->resource_list? */

  g_object_unref (keyboard->settings);

  keyboard->display = NULL;
}

static guint
evdev_code (const ClutterKeyEvent *event)
{
  /* clutter-xkb-utils.c adds a fixed offset of 8 to go into XKB's
   * range, so we do the reverse here. */
  return event->hardware_keycode - 8;
}

void
meta_wayland_keyboard_update (MetaWaylandKeyboard *keyboard,
                              const ClutterKeyEvent *event)
{
  gboolean is_press = event->type == CLUTTER_KEY_PRESS;

  keyboard->mods_changed = xkb_state_update_key (keyboard->xkb_info.state,
                                                 event->hardware_keycode,
                                                 is_press ? XKB_KEY_DOWN : XKB_KEY_UP);
}

gboolean
meta_wayland_keyboard_handle_event (MetaWaylandKeyboard *keyboard,
                                    const ClutterKeyEvent *event)
{
  gboolean is_press = event->type == CLUTTER_KEY_PRESS;
  gboolean handled;

  /* Synthetic key events are for autorepeat. Ignore those, as
   * autorepeat in Wayland is done on the client side. */
  if (event->flags & CLUTTER_EVENT_FLAG_SYNTHETIC)
    return FALSE;

  meta_verbose ("Handling key %s event code %d\n",
		is_press ? "press" : "release",
		event->hardware_keycode);

  handled = notify_key (keyboard, event->time, evdev_code (event), is_press);

  if (handled)
    meta_verbose ("Sent event to wayland client\n");
  else
    meta_verbose ("No wayland surface is focused, continuing normal operation\n");

  if (keyboard->mods_changed != 0)
    {
      notify_modifiers (keyboard);
      keyboard->mods_changed = 0;
    }

  return handled;
}

void
meta_wayland_keyboard_update_key_state (MetaWaylandKeyboard *keyboard,
                                        char                *key_vector,
                                        int                  key_vector_len,
                                        int                  offset)
{
  gboolean mods_changed = FALSE;

  int i;
  for (i = offset; i < key_vector_len * 8; i++)
    {
      gboolean set = (key_vector[i/8] & (1 << (i % 8))) != 0;

      /* The 'offset' parameter allows the caller to have the indices
       * into key_vector to either be X-style (base 8) or evdev (base 0), or
       * something else (unlikely). We subtract 'offset' to convert to evdev
       * style, then add 8 to convert the "evdev" style keycode back to
       * the X-style that xkbcommon expects.
       */
      mods_changed |= xkb_state_update_key (keyboard->xkb_info.state,
                                            i - offset + 8,
                                            set ? XKB_KEY_DOWN : XKB_KEY_UP);
    }

  if (mods_changed)
    notify_modifiers (keyboard);
}

static void
move_resources (struct wl_list *destination, struct wl_list *source)
{
  wl_list_insert_list (destination, source);
  wl_list_init (source);
}

static void
move_resources_for_client (struct wl_list *destination,
			   struct wl_list *source,
			   struct wl_client *client)
{
  struct wl_resource *resource, *tmp;
  wl_resource_for_each_safe (resource, tmp, source)
    {
      if (wl_resource_get_client (resource) == client)
        {
          wl_list_remove (wl_resource_get_link (resource));
          wl_list_insert (destination, wl_resource_get_link (resource));
        }
    }
}

static void
broadcast_focus (MetaWaylandKeyboard *keyboard,
                 struct wl_resource  *resource)
{
  struct wl_array fake_keys;
  struct xkb_state *state = keyboard->xkb_info.state;

  /* We never want to send pressed keys to wayland clients on
   * enter. The protocol says that we should send them, presumably so
   * that clients can trigger their own key repeat routine in case
   * they are given focus and a key is physically pressed.
   *
   * Unfortunately this causes some clients, in particular Xwayland,
   * to register key events that they really shouldn't handle,
   * e.g. on an Alt+Tab keybinding, where Alt is released before Tab,
   * clients would see Tab being pressed on enter followed by a key
   * release event for Tab, meaning that Tab would be processed by
   * the client when it really shouldn't.
   *
   * Since the use case for the pressed keys array on enter seems weak
   * to us, we'll just fake that there are no pressed keys instead
   * which should be spec compliant even if it might not be true.
   */
  wl_array_init (&fake_keys);

  wl_keyboard_send_modifiers (resource, keyboard->focus_serial,
                              xkb_state_serialize_mods (state, XKB_STATE_MODS_DEPRESSED),
                              xkb_state_serialize_mods (state, XKB_STATE_MODS_LATCHED),
                              xkb_state_serialize_mods (state, XKB_STATE_MODS_LOCKED),
                              xkb_state_serialize_layout (state, XKB_STATE_LAYOUT_EFFECTIVE));
  wl_keyboard_send_enter (resource, keyboard->focus_serial,
                          keyboard->focus_surface->resource,
                          &fake_keys);
}

void
meta_wayland_keyboard_set_focus (MetaWaylandKeyboard *keyboard,
                                 MetaWaylandSurface *surface)
{
  if (keyboard->display == NULL)
    return;

  if (keyboard->focus_surface == surface)
    return;

  if (keyboard->focus_surface != NULL)
    {
      struct wl_resource *resource;
      struct wl_list *l;

      l = &keyboard->focus_resource_list;
      if (!wl_list_empty (l))
        {
          struct wl_client *client = wl_resource_get_client (keyboard->focus_surface->resource);
          struct wl_display *display = wl_client_get_display (client);
          uint32_t serial = wl_display_next_serial (display);

          wl_resource_for_each (resource, l)
            {
              wl_keyboard_send_leave (resource, serial, keyboard->focus_surface->resource);
            }

          move_resources (&keyboard->resource_list, &keyboard->focus_resource_list);
        }

      wl_list_remove (&keyboard->focus_surface_listener.link);
      keyboard->focus_surface = NULL;
    }

  if (surface != NULL)
    {
      struct wl_resource *resource;
      struct wl_list *l;

      keyboard->focus_surface = surface;
      wl_resource_add_destroy_listener (keyboard->focus_surface->resource, &keyboard->focus_surface_listener);

      move_resources_for_client (&keyboard->focus_resource_list,
                                 &keyboard->resource_list,
                                 wl_resource_get_client (keyboard->focus_surface->resource));

      l = &keyboard->focus_resource_list;
      if (!wl_list_empty (l))
        {
          struct wl_client *client = wl_resource_get_client (keyboard->focus_surface->resource);
          struct wl_display *display = wl_client_get_display (client);
          keyboard->focus_serial = wl_display_next_serial (display);

          wl_resource_for_each (resource, l)
            {
              broadcast_focus (keyboard, resource);
            }
        }
    }
}

struct wl_client *
meta_wayland_keyboard_get_focus_client (MetaWaylandKeyboard *keyboard)
{
  if (keyboard->focus_surface)
    return wl_resource_get_client (keyboard->focus_surface->resource);
  else
    return NULL;
}

static void
keyboard_release (struct wl_client *client,
                  struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct wl_keyboard_interface keyboard_interface = {
  keyboard_release,
};

void
meta_wayland_keyboard_create_new_resource (MetaWaylandKeyboard *keyboard,
                                           struct wl_client    *client,
                                           struct wl_resource  *seat_resource,
                                           uint32_t id)
{
  struct wl_resource *cr;

  cr = wl_resource_create (client, &wl_keyboard_interface, wl_resource_get_version (seat_resource), id);
  wl_resource_set_implementation (cr, &keyboard_interface, keyboard, unbind_resource);

  wl_keyboard_send_keymap (cr,
                           WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                           keyboard->xkb_info.keymap_fd,
                           keyboard->xkb_info.keymap_size);

  notify_key_repeat_for_resource (keyboard, cr);

  if (keyboard->focus_surface && wl_resource_get_client (keyboard->focus_surface->resource) == client)
    {
      wl_list_insert (&keyboard->focus_resource_list, wl_resource_get_link (cr));
      broadcast_focus (keyboard, cr);
    }
  else
    {
      wl_list_insert (&keyboard->resource_list, wl_resource_get_link (cr));
    }
}
