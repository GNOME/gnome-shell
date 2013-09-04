/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

 * Authors:
 *  Kristian Høgsberg
 *  Damien Lespiau <damien.lespiau@intel.com>
 */

#include "clutter-keysyms.h"
#include "clutter-event-private.h"
#include "clutter-xkb-utils.h"

/*
 * _clutter_event_new_from_evdev: Create a new Clutter ClutterKeyEvent
 * @device: a ClutterInputDevice
 * @stage: the stage the event should be delivered to
 * @xkb: XKB rules to translate the event
 * @_time: timestamp of the event
 * @key: a key code coming from a Linux input device
 * @state: TRUE if a press event, FALSE if a release event
 * @modifer_state: in/out
 *
 * Translate @key to a #ClutterKeyEvent using rules from xbbcommon.
 *
 * Return value: the new #ClutterEvent
 */
ClutterEvent *
_clutter_key_event_new_from_evdev (ClutterInputDevice *device,
				   ClutterInputDevice *core_device,
                                   ClutterStage       *stage,
                                   struct xkb_state   *xkb_state,
				   uint32_t            button_state,
                                   uint32_t            _time,
                                   xkb_keycode_t       key,
                                   uint32_t            state)
{
  ClutterEvent *event;
  xkb_keysym_t sym;
  const xkb_keysym_t *syms;
  char buffer[8];
  int n;

  if (state)
    event = clutter_event_new (CLUTTER_KEY_PRESS);
  else
    event = clutter_event_new (CLUTTER_KEY_RELEASE);

  /* We use a fixed offset of 8 because evdev starts KEY_* numbering from
   * 0, whereas X11's minimum keycode, for really stupid reasons, is 8.
   * So the evdev XKB rules are based on the keycodes all being shifted
   * upwards by 8. */
  key += 8;

  n = xkb_key_get_syms (xkb_state, key, &syms);
  if (n == 1)
    sym = syms[0];
  else
    sym = XKB_KEY_NoSymbol;

  event->key.device = core_device;
  event->key.stage = stage;
  event->key.time = _time;
  _clutter_xkb_translate_state (event, xkb_state, button_state);
  event->key.hardware_keycode = key;
  event->key.keyval = sym;
  clutter_event_set_source_device (event, device);

  n = xkb_keysym_to_utf8 (sym, buffer, sizeof (buffer));

  if (n == 0)
    {
      /* not printable */
      event->key.unicode_value = (gunichar) '\0';
    }
  else
    {
      event->key.unicode_value = g_utf8_get_char_validated (buffer, n);
      if (event->key.unicode_value == -1 || event->key.unicode_value == -2)
        event->key.unicode_value = (gunichar) '\0';
    }

  return event;
}

/*
 * _clutter_xkb_state_new:
 *
 * Create a new xkbcommon keymap and state object.
 *
 * FIXME: We need a way to override the layout here, a fixed or runtime
 * detected layout is provided by the backend calling _clutter_xkb_state_new();
 */
struct xkb_state *
_clutter_xkb_state_new (const gchar *model,
                        const gchar *layout,
                        const gchar *variant,
                        const gchar *options)
{
  struct xkb_context *ctx;
  struct xkb_keymap *keymap;
  struct xkb_state *state;
  struct xkb_rule_names names;

  ctx = xkb_context_new(0);
  if (!ctx)
    return NULL;

  names.rules = "evdev";
  if (model)
    names.model = model;
  else
    names.model = "pc105";
  names.layout = layout;
  names.variant = variant;
  names.options = options;

  keymap = xkb_map_new_from_names(ctx, &names, 0);
  xkb_context_unref(ctx);
  if (!keymap)
    return NULL;

  state = xkb_state_new(keymap);
  xkb_map_unref(keymap);

  return state;
}

void
_clutter_xkb_translate_state (ClutterEvent     *event,
			      struct xkb_state *state,
			      uint32_t          button_state)
{
  _clutter_event_set_state_full (event,
				 button_state,
				 xkb_state_serialize_mods (state, XKB_STATE_MODS_DEPRESSED),
				 xkb_state_serialize_mods (state, XKB_STATE_MODS_LATCHED),
				 xkb_state_serialize_mods (state, XKB_STATE_MODS_LOCKED),
				 xkb_state_serialize_mods (state, XKB_STATE_MODS_EFFECTIVE) | button_state);
}
