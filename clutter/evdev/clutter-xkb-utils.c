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
#include "clutter-xkb-utils.h"

/*
 * print_key_sym: Translate a symbol to its printable form if any
 * @symbol: the symbol to translate
 * @buffer: the buffer where to put the translated string
 * @len: size of the buffer
 *
 * Translates @symbol into a printable representation in @buffer, if possible.
 *
 * Return value: The number of bytes of the translated string, 0 if the
 *               symbol can't be printed
 *
 * Note: The code is derived from libX11's src/KeyBind.c
 *       Copyright 1985, 1987, 1998  The Open Group
 *
 * Note: This code works for Latin-1 symbols. clutter_keysym_to_unicode()
 *       does the work for the other keysyms.
 */
static int
print_keysym (uint32_t  symbol,
               char     *buffer,
               int       len)
{
  unsigned long high_bytes;
  unsigned char c;

  high_bytes = symbol >> 8;
  if (!(len &&
        ((high_bytes == 0) ||
         ((high_bytes == 0xFF) &&
          (((symbol >= CLUTTER_KEY_BackSpace) &&
            (symbol <= CLUTTER_KEY_Clear)) ||
           (symbol == CLUTTER_KEY_Return) ||
           (symbol == CLUTTER_KEY_Escape) ||
           (symbol == CLUTTER_KEY_KP_Space) ||
           (symbol == CLUTTER_KEY_KP_Tab) ||
           (symbol == CLUTTER_KEY_KP_Enter) ||
           ((symbol >= CLUTTER_KEY_KP_Multiply) &&
            (symbol <= CLUTTER_KEY_KP_9)) ||
           (symbol == CLUTTER_KEY_KP_Equal) ||
           (symbol == CLUTTER_KEY_Delete))))))
    return 0;

  /* if X keysym, convert to ascii by grabbing low 7 bits */
  if (symbol == CLUTTER_KEY_KP_Space)
    c = CLUTTER_KEY_space & 0x7F; /* patch encoding botch */
  else if (high_bytes == 0xFF)
    c = symbol & 0x7F;
  else
    c = symbol & 0xFF;

  buffer[0] = c;
  return 1;
}

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

  /* unicode_value is the printable representation */
  n = print_keysym (sym, buffer, sizeof (buffer));

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
