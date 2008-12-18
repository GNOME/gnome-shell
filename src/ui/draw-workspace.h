/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Draw a workspace */

/* This file should not be modified to depend on other files in
 * libwnck or metacity, since it's used in both of them
 */

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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef WNCK_DRAW_WORKSPACE_H
#define WNCK_DRAW_WORKSPACE_H

#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

typedef struct
{
  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;
  int x;
  int y;
  int width;
  int height;

  guint is_active : 1;
  
} WnckWindowDisplayInfo;

void wnck_draw_workspace (GtkWidget                   *widget,
                          GdkDrawable                 *drawable,
                          int                          x,
                          int                          y,
                          int                          width,
                          int                          height,
                          int                          screen_width,
                          int                          screen_height,
                          GdkPixbuf                   *workspace_background,
                          gboolean                     is_active,
                          const WnckWindowDisplayInfo *windows,
                          int                          n_windows);

#endif
