/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corp.
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
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

#ifndef __CLUTTER_KEYMAP_X11_H__
#define __CLUTTER_KEYMAP_X11_H__

#include <glib-object.h>
#include <pango/pango.h>
#include <clutter/clutter-event.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_KEYMAP_X11         (_clutter_keymap_x11_get_type ())
#define CLUTTER_KEYMAP_X11(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_KEYMAP_X11, ClutterKeymapX11))
#define CLUTTER_IS_KEYMAP_X11(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_KEYMAP_X11))

typedef struct _ClutterKeymapX11        ClutterKeymapX11;

GType _clutter_keymap_x11_get_type (void) G_GNUC_CONST;

gint     _clutter_keymap_x11_get_key_group       (ClutterKeymapX11    *keymap,
                                                  ClutterModifierType  state);
gboolean _clutter_keymap_x11_get_num_lock_state  (ClutterKeymapX11    *keymap);
gboolean _clutter_keymap_x11_get_caps_lock_state (ClutterKeymapX11    *keymap);
gint     _clutter_keymap_x11_translate_key_state (ClutterKeymapX11    *keymap,
                                                  guint                hardware_keycode,
                                                  ClutterModifierType *modifier_state_p,
                                                  ClutterModifierType *mods_p);
gboolean _clutter_keymap_x11_get_is_modifier     (ClutterKeymapX11    *keymap,
                                                  gint                 keycode);

PangoDirection _clutter_keymap_x11_get_direction (ClutterKeymapX11    *keymap);

G_END_DECLS

#endif /* __CLUTTER_KEYMAP_X11_H__ */
