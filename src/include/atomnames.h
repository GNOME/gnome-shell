/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002, 2003, 2004 Red Hat, Inc.
 * Copyright (C) 2003, 2004 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2008 Thomas Thurman
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

/**
 * \file atomnames.h  A list of atom names.
 *
 * This is a list of the names of all the X atoms that Mutter uses.
 * Each is wrapped in a macro "item()" which is undefined here; the
 * idea is that when you need to make a big list of all the X atoms,
 * you can define item(), include this file, and then undefine it
 * again.
 *
 * If you also define EWMH_ATOMS_ONLY then you will only get _NET_WM_*
 * atoms rather than all of them.
 */

#ifndef item
#error "item(x) must be defined when you include atomnames.h"
#endif

#ifndef EWMH_ATOMS_ONLY

item(WM_PROTOCOLS) /* MUST BE FIRST */
item(WM_TAKE_FOCUS)
item(WM_DELETE_WINDOW)
item(WM_STATE)
item(_MOTIF_WM_HINTS)
item(WM_CHANGE_STATE)
item(SM_CLIENT_ID)
item(WM_CLIENT_LEADER)
item(WM_WINDOW_ROLE)
item(UTF8_STRING)
item(WM_ICON_SIZE)
item(_KWM_WIN_ICON)
item(_MUTTER_RESTART_MESSAGE)
item(_MUTTER_RELOAD_THEME_MESSAGE)
item(_MUTTER_SET_KEYBINDINGS_MESSAGE)
item(_MUTTER_TOGGLE_VERBOSE)
item(_GNOME_WM_KEYBINDINGS)
item(_GNOME_PANEL_ACTION)
item(_GNOME_PANEL_ACTION_MAIN_MENU)
item(_GNOME_PANEL_ACTION_RUN_DIALOG)
item(_MUTTER_SENTINEL)
item(_MUTTER_VERSION)
item(WM_CLIENT_MACHINE)
item(MANAGER)
item(TARGETS)
item(MULTIPLE)
item(TIMESTAMP)
item(VERSION)
item(ATOM_PAIR)

/* Oddities: These are used, and we need atoms for them,
 * but when we need all _NET_WM hints (i.e. when we're making
 * lists of which _NET_WM hints we support in order to advertise
 * it) we haven't historically listed them.  I don't know what
 * the reason for this is.  It may be a bug.
 */
item(_NET_WM_SYNC_REQUEST)
item(_NET_WM_SYNC_REQUEST_COUNTER)
item(_NET_WM_VISIBLE_NAME)
item(_NET_WM_VISIBLE_ICON_NAME)
item(_NET_SUPPORTING_WM_CHECK)

/* But I suppose it's quite reasonable not to advertise using
 * _NET_SUPPORTED that we support _NET_SUPPORTED :)
 */
item(_NET_SUPPORTED)

#endif /* !EWMH_ATOMS_ONLY */

/**************************************************************************/

item(_NET_WM_NAME)
item(_NET_CLOSE_WINDOW)
item(_NET_WM_STATE)
item(_NET_WM_STATE_SHADED)
item(_NET_WM_STATE_MAXIMIZED_HORZ)
item(_NET_WM_STATE_MAXIMIZED_VERT)
item(_NET_WM_DESKTOP)
item(_NET_NUMBER_OF_DESKTOPS)
item(_NET_CURRENT_DESKTOP)
item(_NET_WM_WINDOW_TYPE)
item(_NET_WM_WINDOW_TYPE_DESKTOP)
item(_NET_WM_WINDOW_TYPE_DOCK)
item(_NET_WM_WINDOW_TYPE_TOOLBAR)
item(_NET_WM_WINDOW_TYPE_MENU)
item(_NET_WM_WINDOW_TYPE_UTILITY)
item(_NET_WM_WINDOW_TYPE_SPLASH)
item(_NET_WM_WINDOW_TYPE_DIALOG)
item(_NET_WM_WINDOW_TYPE_DROPDOWN_MENU)
item(_NET_WM_WINDOW_TYPE_POPUP_MENU)
item(_NET_WM_WINDOW_TYPE_TOOLTIP)
item(_NET_WM_WINDOW_TYPE_NOTIFICATION)
item(_NET_WM_WINDOW_TYPE_COMBO)
item(_NET_WM_WINDOW_TYPE_DND)
item(_NET_WM_WINDOW_TYPE_NORMAL)
item(_NET_WM_STATE_MODAL)
item(_NET_CLIENT_LIST)
item(_NET_CLIENT_LIST_STACKING)
item(_NET_WM_STATE_SKIP_TASKBAR)
item(_NET_WM_STATE_SKIP_PAGER)
item(_NET_WM_ICON_NAME)
item(_NET_WM_ICON)
item(_NET_WM_ICON_GEOMETRY)
item(_NET_WM_MOVERESIZE)
item(_NET_ACTIVE_WINDOW)
item(_NET_WM_STRUT)
item(_NET_WM_STATE_HIDDEN)
item(_NET_WM_STATE_FULLSCREEN)
item(_NET_WM_PING)
item(_NET_WM_PID)
item(_NET_WORKAREA)
item(_NET_SHOWING_DESKTOP)
item(_NET_DESKTOP_LAYOUT)
item(_NET_DESKTOP_NAMES)
item(_NET_WM_ALLOWED_ACTIONS)
item(_NET_WM_ACTION_MOVE)
item(_NET_WM_ACTION_RESIZE)
item(_NET_WM_ACTION_SHADE)
item(_NET_WM_ACTION_STICK)
item(_NET_WM_ACTION_MAXIMIZE_HORZ)
item(_NET_WM_ACTION_MAXIMIZE_VERT)
item(_NET_WM_ACTION_CHANGE_DESKTOP)
item(_NET_WM_ACTION_CLOSE)
item(_NET_WM_STATE_ABOVE)
item(_NET_WM_STATE_BELOW)
item(_NET_STARTUP_ID)
item(_NET_WM_STRUT_PARTIAL)
item(_NET_WM_ACTION_FULLSCREEN)
item(_NET_WM_ACTION_MINIMIZE)
item(_NET_FRAME_EXTENTS)
item(_NET_REQUEST_FRAME_EXTENTS)
item(_NET_WM_USER_TIME)
item(_NET_WM_STATE_DEMANDS_ATTENTION)
item(_NET_MOVERESIZE_WINDOW)
item(_NET_DESKTOP_GEOMETRY)
item(_NET_DESKTOP_VIEWPORT)
item(_NET_WM_USER_TIME_WINDOW)
item(_NET_WM_ACTION_ABOVE)
item(_NET_WM_ACTION_BELOW)
item(_NET_WM_STATE_STICKY)
item(_NET_WM_FULLSCREEN_MONITORS)

#if 0
/* We apparently never use: */
/* item(_NET_RESTACK_WINDOW) */
#endif

/* eof atomnames.h */

