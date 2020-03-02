/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-entry.h: Plain entry actor
 *
 * Copyright 2008, 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#ifndef __ST_ENTRY_H__
#define __ST_ENTRY_H__

G_BEGIN_DECLS

#include <st/st-widget.h>

#define ST_TYPE_ENTRY (st_entry_get_type ())
G_DECLARE_DERIVABLE_TYPE (StEntry, st_entry, ST, ENTRY, StWidget)

struct _StEntryClass
{
  StWidgetClass parent_class;

  /* signals */
  void (*primary_icon_clicked)   (StEntry *entry);
  void (*secondary_icon_clicked) (StEntry *entry);
};

StWidget       *st_entry_new                (const gchar    *text);
const gchar    *st_entry_get_text           (StEntry        *entry);
void            st_entry_set_text           (StEntry        *entry,
                                             const gchar    *text);
ClutterActor   *st_entry_get_clutter_text   (StEntry        *entry);

void            st_entry_set_hint_text      (StEntry        *entry,
                                             const gchar    *text);
const gchar    *st_entry_get_hint_text      (StEntry        *entry);

void            st_entry_set_input_purpose  (StEntry                    *entry,
                                             ClutterInputContentPurpose  purpose);
void            st_entry_set_input_hints    (StEntry                      *entry,
                                             ClutterInputContentHintFlags  hints);

ClutterInputContentPurpose     st_entry_get_input_purpose  (StEntry *entry);
ClutterInputContentHintFlags   st_entry_get_input_hints    (StEntry *entry);

void            st_entry_set_primary_icon  (StEntry      *entry,
                                            ClutterActor *icon);
ClutterActor *  st_entry_get_primary_icon  (StEntry      *entry);

void            st_entry_set_secondary_icon (StEntry      *entry,
                                             ClutterActor *icon);
ClutterActor *  st_entry_get_secondary_icon (StEntry      *entry);

void            st_entry_set_hint_actor    (StEntry      *entry,
                                            ClutterActor *hint_actor);
ClutterActor *  st_entry_get_hint_actor    (StEntry      *entry);

typedef void (*StEntryCursorFunc) (StEntry *entry, gboolean use_ibeam, gpointer data);
void            st_entry_set_cursor_func    (StEntryCursorFunc func,
                                             gpointer          user_data);

G_END_DECLS

#endif /* __ST_ENTRY_H__ */
