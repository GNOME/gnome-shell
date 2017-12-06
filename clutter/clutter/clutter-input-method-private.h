/*
 * Copyright (C) 2017,2018 Red Hat
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef __CLUTTER_INPUT_METHOD_PRIVATE_H__
#define __CLUTTER_INPUT_METHOD_PRIVATE_H__

ClutterInputFocus * clutter_input_method_get_focus (ClutterInputMethod *method);

void clutter_input_method_reset               (ClutterInputMethod *method);

void clutter_input_method_set_cursor_location (ClutterInputMethod *method,
                                               const ClutterRect  *rect);
void clutter_input_method_set_surrounding     (ClutterInputMethod *method,
                                               const gchar        *text,
                                               guint               cursor,
                                               guint               anchor);
void clutter_input_method_set_content_hints   (ClutterInputMethod           *method,
                                               ClutterInputContentHintFlags  hints);
void clutter_input_method_set_content_purpose (ClutterInputMethod         *method,
                                               ClutterInputContentPurpose  purpose);
void clutter_input_method_set_can_show_preedit (ClutterInputMethod *method,
                                                gboolean            can_show_preedit);
gboolean clutter_input_method_filter_key_event (ClutterInputMethod    *method,
                                                const ClutterKeyEvent *key);

void clutter_input_method_toggle_input_panel (ClutterInputMethod *method);

#endif /* __CLUTTER_INPUT_METHOD_PRIVATE_H__ */
