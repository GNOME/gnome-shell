/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* 
 * Copyright (C) 2013 Red Hat Inc.
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

/* This file is shared between mutter (src/core/meta-xrandr-shared.h)
   and gnome-desktop (libgnome-desktop/meta-xrandr-shared.h).

   The canonical place for all changes is mutter.

   There should be no includes in this file.
*/

#ifndef META_XRANDR_SHARED_H
#define META_XRANDR_SHARED_H

typedef enum {
  META_POWER_SAVE_UNSUPPORTED = -1,
  META_POWER_SAVE_ON = 0,
  META_POWER_SAVE_STANDBY,
  META_POWER_SAVE_SUSPEND,
  META_POWER_SAVE_OFF,
} MetaPowerSave;

#endif
