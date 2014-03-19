/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter window group private header */

/*
 * Copyright (C) 2002 Red Hat Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_GROUP_H
#define META_GROUP_H

#include <X11/Xlib.h>
#include <glib.h>
#include <meta/types.h>

typedef struct _MetaGroup MetaGroup;

struct _MetaGroup
{
  int refcount;
  MetaDisplay *display;
  GSList *windows;
  Window group_leader;
  char *startup_id;
  char *wm_client_machine;
};

/* note, can return NULL */
MetaGroup* meta_window_get_group       (MetaWindow *window);
void       meta_window_compute_group   (MetaWindow *window);
void       meta_window_shutdown_group  (MetaWindow *window);

void       meta_window_group_leader_changed (MetaWindow *window);

/* note, can return NULL */
MetaGroup* meta_display_lookup_group   (MetaDisplay *display,
                                        Window       group_leader);

GSList*    meta_group_list_windows     (MetaGroup *group);

void       meta_group_update_layers    (MetaGroup *group);

const char* meta_group_get_startup_id  (MetaGroup *group);

int        meta_group_get_size         (MetaGroup *group);

gboolean meta_group_property_notify   (MetaGroup  *group,
                                       XEvent     *event);

#endif
