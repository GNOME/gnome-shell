/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter window icons */

/* 
 * Copyright (C) 2002 Havoc Pennington
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

#ifndef META_ICON_CACHE_H
#define META_ICON_CACHE_H

#include "screen-private.h"

typedef struct _MetaIconCache MetaIconCache;

typedef enum
{
  /* These MUST be in ascending order of preference;
   * i.e. if we get _NET_WM_ICON and already have
   * WM_HINTS, we prefer _NET_WM_ICON
   */
  USING_NO_ICON,
  USING_FALLBACK_ICON,
  USING_KWM_WIN_ICON,
  USING_WM_HINTS,
  USING_NET_WM_ICON
} IconOrigin;

struct _MetaIconCache
{
  int origin;
  Pixmap prev_pixmap;
  Pixmap prev_mask;
  guint want_fallback : 1;
  /* TRUE if these props have changed */
  guint wm_hints_dirty : 1;
  guint kwm_win_icon_dirty : 1;
  guint net_wm_icon_dirty : 1;
};

void           meta_icon_cache_init                 (MetaIconCache *icon_cache);
void           meta_icon_cache_free                 (MetaIconCache *icon_cache);
void           meta_icon_cache_property_changed     (MetaIconCache *icon_cache,
                                                     MetaDisplay   *display,
                                                     Atom           atom);
gboolean       meta_icon_cache_get_icon_invalidated (MetaIconCache *icon_cache);

gboolean meta_read_icons         (MetaScreen     *screen,
                                  Window          xwindow,
                                  MetaIconCache  *icon_cache,
                                  Pixmap          wm_hints_pixmap,
                                  Pixmap          wm_hints_mask,
                                  GdkPixbuf     **iconp,
                                  int             ideal_width,
                                  int             ideal_height,
                                  GdkPixbuf     **mini_iconp,
                                  int             ideal_mini_width,
                                  int             ideal_mini_height);

#endif




