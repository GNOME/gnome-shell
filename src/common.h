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
  META_FRAME_ALLOWS_DELETE            = 1 << 0,
  META_FRAME_ALLOWS_MENU              = 1 << 1,
  META_FRAME_ALLOWS_MINIMIZE          = 1 << 2,
  META_FRAME_ALLOWS_MAXIMIZE          = 1 << 3,
  META_FRAME_ALLOWS_VERTICAL_RESIZE   = 1 << 4,
  META_FRAME_ALLOWS_HORIZONTAL_RESIZE = 1 << 5,
  META_FRAME_HAS_FOCUS                = 1 << 6,
  META_FRAME_SHADED                   = 1 << 7,
  META_FRAME_STUCK                    = 1 << 8,
  META_FRAME_MAXIMIZED                = 1 << 9,
  META_FRAME_ALLOWS_SHADE             = 1 << 10,
  META_FRAME_ALLOWS_MOVE              = 1 << 11
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
  META_MENU_OP_WORKSPACES  = 1 << 8,
  META_MENU_OP_MOVE        = 1 << 9,
  META_MENU_OP_RESIZE      = 1 << 10
} MetaMenuOp;

typedef struct _MetaWindowMenu MetaWindowMenu;

typedef void (* MetaWindowMenuFunc) (MetaWindowMenu *menu,
                                     Display        *xdisplay,
                                     Window          client_xwindow,
                                     MetaMenuOp      op,
                                     int             workspace,
                                     gpointer        data);

/* when changing this enum, there are various switch statements
 * you have to update
 */
typedef enum
{
  META_GRAB_OP_NONE,

  /* Mouse ops */
  META_GRAB_OP_MOVING,
  META_GRAB_OP_RESIZING_SE,
  META_GRAB_OP_RESIZING_S,
  META_GRAB_OP_RESIZING_SW,
  META_GRAB_OP_RESIZING_N,
  META_GRAB_OP_RESIZING_NE,
  META_GRAB_OP_RESIZING_NW,
  META_GRAB_OP_RESIZING_W,
  META_GRAB_OP_RESIZING_E,

  /* Keyboard ops */
  META_GRAB_OP_KEYBOARD_MOVING,
  META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN,
  META_GRAB_OP_KEYBOARD_RESIZING_S,
  META_GRAB_OP_KEYBOARD_RESIZING_N,
  META_GRAB_OP_KEYBOARD_RESIZING_W,
  META_GRAB_OP_KEYBOARD_RESIZING_E,
  META_GRAB_OP_KEYBOARD_RESIZING_SE,
  META_GRAB_OP_KEYBOARD_RESIZING_NE,
  META_GRAB_OP_KEYBOARD_RESIZING_SW,
  META_GRAB_OP_KEYBOARD_RESIZING_NW,

  META_GRAB_OP_KEYBOARD_TABBING,
  
  /* Frame button ops */
  META_GRAB_OP_CLICKING_MINIMIZE,
  META_GRAB_OP_CLICKING_MAXIMIZE,
  META_GRAB_OP_CLICKING_UNMAXIMIZE,
  META_GRAB_OP_CLICKING_DELETE,
  META_GRAB_OP_CLICKING_MENU
} MetaGrabOp;


typedef enum
{
  META_CURSOR_DEFAULT,
  META_CURSOR_NORTH_RESIZE,
  META_CURSOR_SOUTH_RESIZE,
  META_CURSOR_WEST_RESIZE,
  META_CURSOR_EAST_RESIZE,
  META_CURSOR_SE_RESIZE,
  META_CURSOR_SW_RESIZE,
  META_CURSOR_NE_RESIZE,
  META_CURSOR_NW_RESIZE

} MetaCursor;

typedef enum
{
  META_FOCUS_MODE_CLICK,
  META_FOCUS_MODE_SLOPPY,
  META_FOCUS_MODE_MOUSE
} MetaFocusMode;

typedef enum
{
  META_FRAME_TYPE_NORMAL,
  META_FRAME_TYPE_DIALOG,
  META_FRAME_TYPE_MODAL_DIALOG,
  META_FRAME_TYPE_UTILITY,
  META_FRAME_TYPE_MENU,
  /* META_FRAME_TYPE_TOOLBAR, */
  META_FRAME_TYPE_LAST
} MetaFrameType;

/* should investigate changing these to whatever most apps use */
#define META_ICON_WIDTH 32
#define META_ICON_HEIGHT 32
#define META_MINI_ICON_WIDTH 16
#define META_MINI_ICON_HEIGHT 16

#endif




