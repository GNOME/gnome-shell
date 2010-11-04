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
 */

#include "clutter-keysyms.h"
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
                                   ClutterStage       *stage,
                                   struct xkb_desc    *xkb,
                                   uint32_t            _time,
                                   uint32_t            key,
                                   uint32_t            state,
                                   uint32_t           *modifier_state)
{
  ClutterEvent *event;
  uint32_t code, sym, level;
  char buffer[128];
  int n;

  if (state)
    event = clutter_event_new (CLUTTER_KEY_PRESS);
  else
    event = clutter_event_new (CLUTTER_KEY_RELEASE);

  code = key + xkb->min_key_code;
  level = 0;

  if (*modifier_state & CLUTTER_SHIFT_MASK &&
      XkbKeyGroupWidth (xkb, code, 0) > 1)
    level = 1;

  sym = XkbKeySymEntry (xkb, code, level, 0);
  if (state)
    *modifier_state |= xkb->map->modmap[code];
  else
    *modifier_state &= ~xkb->map->modmap[code];

  event->key.device = device;
  event->key.stage = stage;
  event->key.time = _time;
  event->key.modifier_state = *modifier_state;
  event->key.hardware_keycode = key;
  event->key.keyval = sym;
  event->key.unicode_value = sym;

  return event;
}

/*
 * _clutter_xkb_desc_new:
 *
 * Create a new xkbcommon keymap.
 *
 * FIXME: We need a way to override the layout here, a fixed or runtime
 * detected layout is provided by the backend calling _clutter_xkb_desc_new();
 */
struct xkb_desc *
_clutter_xkb_desc_new (const gchar *model,
                       const gchar *layout,
                       const gchar *variant,
                       const gchar *options)
{
  struct xkb_rule_names names;

  names.rules = "evdev";
  if (model)
    names.model = model;
  else
    names.model = "pc105";
  names.layout = layout;
  names.variant = variant;
  names.options = options;

  return xkb_compile_keymap_from_rules (&names);
}
