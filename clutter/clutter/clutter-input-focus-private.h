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

#ifndef __CLUTTER_INPUT_FOCUS_PRIVATE_H__
#define __CLUTTER_INPUT_FOCUS_PRIVATE_H__

void clutter_input_focus_focus_in  (ClutterInputFocus  *focus,
                                    ClutterInputMethod *method);
void clutter_input_focus_focus_out (ClutterInputFocus  *focus);

void clutter_input_focus_commit (ClutterInputFocus *focus,
                                 const gchar       *text);
void clutter_input_focus_delete_surrounding (ClutterInputFocus *focus,
                                             guint              offset,
                                             guint              len);
void clutter_input_focus_request_surrounding (ClutterInputFocus *focus);

void clutter_input_focus_set_preedit_text (ClutterInputFocus *focus,
                                           const gchar       *preedit,
                                           guint              cursor);

#endif /* __CLUTTER_INPUT_FOCUS_PRIVATE_H__ */
