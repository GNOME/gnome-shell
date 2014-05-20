/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington, Anders Carlsson
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
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

#include "window-private.h"

#ifndef META_WINDOW_X11_PRIVATE_H
#define META_WINDOW_X11_PRIVATE_H

G_BEGIN_DECLS

typedef struct _MetaWindowX11Private MetaWindowX11Private;

struct _MetaWindowX11
{
  MetaWindow parent;

  MetaWindowX11Private *priv;
};

struct _MetaWindowX11Private
{
  /* TRUE if the client forced these on */
  guint wm_state_skip_taskbar : 1;
  guint wm_state_skip_pager : 1;

  /* Weird "_NET_WM_STATE_MODAL" flag */
  guint wm_state_modal : 1;

  /* Info on which props we got our attributes from */
  guint using_net_wm_name              : 1; /* vs. plain wm_name */
  guint using_net_wm_visible_name      : 1; /* tracked so we can clear it */

  Atom type_atom;

  /* Requested geometry */
  int border_width;

  MetaResizePopup *grab_resize_popup;

  /* These are in server coordinates. If we have a frame, it's
   * relative to the frame. */
  MetaRectangle client_rect;
};

G_END_DECLS

#endif
