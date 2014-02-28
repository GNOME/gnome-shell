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

#ifndef __META_WAYLAND_KEYBOARD_H__
#define __META_WAYLAND_KEYBOARD_H__

#include <clutter/clutter.h>
#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>

struct _MetaWaylandKeyboardGrabInterface
{
  gboolean (*key) (MetaWaylandKeyboardGrab * grab, uint32_t time,
		   uint32_t key, uint32_t state);
  void (*modifiers) (MetaWaylandKeyboardGrab * grab, uint32_t serial,
                     uint32_t mods_depressed, uint32_t mods_latched,
                     uint32_t mods_locked, uint32_t group);
};

struct _MetaWaylandKeyboardGrab
{
  const MetaWaylandKeyboardGrabInterface *interface;
  MetaWaylandKeyboard *keyboard;
  MetaWaylandSurface *focus;
  uint32_t key;
};

typedef struct
{
  struct xkb_keymap *keymap;
  int keymap_fd;
  size_t keymap_size;
  char *keymap_area;
  xkb_mod_index_t shift_mod;
  xkb_mod_index_t caps_mod;
  xkb_mod_index_t ctrl_mod;
  xkb_mod_index_t alt_mod;
  xkb_mod_index_t mod2_mod;
  xkb_mod_index_t mod3_mod;
  xkb_mod_index_t super_mod;
  xkb_mod_index_t mod5_mod;
} MetaWaylandXkbInfo;

typedef struct
{
  uint32_t mods_depressed;
  uint32_t mods_latched;
  uint32_t mods_locked;
  uint32_t group;
} MetaWaylandXkbState;

struct _MetaWaylandKeyboard
{
  struct wl_list resource_list;

  MetaWaylandSurface *focus_surface;
  struct wl_listener focus_surface_listener;
  struct wl_resource *focus_resource;
  struct wl_listener focus_resource_listener;
  uint32_t focus_serial;

  MetaWaylandKeyboardGrab *grab;
  MetaWaylandKeyboardGrab default_grab;
  uint32_t grab_key;
  uint32_t grab_serial;
  uint32_t grab_time;

  struct wl_array keys;

  MetaWaylandXkbState modifier_state;

  struct wl_display *display;

  struct xkb_context *xkb_context;
  MetaWaylandXkbInfo xkb_info;

  MetaWaylandKeyboardGrab input_method_grab;
  struct wl_resource *input_method_resource;
};

gboolean
meta_wayland_keyboard_init (MetaWaylandKeyboard *keyboard,
                            struct wl_display   *display);

typedef enum {
  META_WAYLAND_KEYBOARD_SKIP_XCLIENTS = 1,
} MetaWaylandKeyboardSetKeymapFlags;

void
meta_wayland_keyboard_set_keymap_names (MetaWaylandKeyboard *keyboard,
					const char          *rules,
					const char          *model,
					const char          *layout,
					const char          *variant,
					const char          *options,
					int                  flags);
gboolean
meta_wayland_keyboard_handle_event (MetaWaylandKeyboard *keyboard,
                                    const ClutterKeyEvent *event);

void
meta_wayland_keyboard_set_focus (MetaWaylandKeyboard *keyboard,
                                 MetaWaylandSurface *surface);

void
meta_wayland_keyboard_start_grab (MetaWaylandKeyboard *device,
                                  MetaWaylandKeyboardGrab *grab);

void
meta_wayland_keyboard_end_grab (MetaWaylandKeyboard *keyboard);

gboolean
meta_wayland_keyboard_begin_modal (MetaWaylandKeyboard *keyboard,
				   guint32              timestamp);
void
meta_wayland_keyboard_end_modal   (MetaWaylandKeyboard *keyboard,
				   guint32              timestamp);

void
meta_wayland_keyboard_release (MetaWaylandKeyboard *keyboard);

#endif /* __META_WAYLAND_KEYBOARD_H__ */
