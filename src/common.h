/* Metacity common types shared by core.h and ui.h */

/* 
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef META_COMMON_H
#define META_COMMON_H

/* Don't include GTK or core headers here */
#include <X11/Xlib.h>
#include <glib.h>

typedef enum
{
  META_FRAME_ALLOWS_DELETE    = 1 << 0,
  META_FRAME_ALLOWS_MENU      = 1 << 1,
  META_FRAME_ALLOWS_MINIMIZE  = 1 << 2,
  META_FRAME_ALLOWS_MAXIMIZE  = 1 << 3, 
  META_FRAME_ALLOWS_RESIZE    = 1 << 4,
  META_FRAME_TRANSIENT        = 1 << 5,
  META_FRAME_HAS_FOCUS        = 1 << 6,
  META_FRAME_SHADED           = 1 << 7,
  META_FRAME_STUCK            = 1 << 8,
  META_FRAME_MAXIMIZED        = 1 << 9,
  META_FRAME_ALLOWS_SHADE     = 1 << 10
} MetaFrameFlags;

typedef enum
{
  META_MENU_OP_DELETE      = 1 << 0,
  META_MENU_OP_MINIMIZE    = 1 << 1,
  META_MENU_OP_UNMAXIMIZE  = 1 << 2,
  META_MENU_OP_MAXIMIZE    = 1 << 3,
  META_MENU_OP_UNSHADE     = 1 << 4,
  META_MENU_OP_SHADE       = 1 << 5,
  META_MENU_OP_UNSTICK     = 1 << 6,
  META_MENU_OP_STICK       = 1 << 7,
  META_MENU_OP_WORKSPACES  = 1 << 8
} MetaMenuOp;

typedef struct _MetaWindowMenu MetaWindowMenu;

typedef void (* MetaWindowMenuFunc) (MetaWindowMenu *menu,
                                     Display        *xdisplay,
                                     Window          client_xwindow,
                                     MetaMenuOp      op,
                                     int             workspace,
                                     gpointer        data);

#endif
