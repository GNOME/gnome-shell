/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file screen-private.h  Screens which Mutter manages
 *
 * Managing X screens.
 * This file contains methods on this class which are available to
 * routines in core but not outside it.  (See screen.h for the routines
 * which the rest of the world is allowed to use.)
 */

/*
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef META_SCREEN_PRIVATE_H
#define META_SCREEN_PRIVATE_H

#include "display-private.h"
#include <meta/screen.h>
#include <X11/Xutil.h>
#include "stack-tracker.h"
#include "ui.h"
#include "meta-monitor-manager-private.h"

#define META_WIREFRAME_XOR_LINE_WIDTH 2

struct _MetaScreen
{
  GObject parent_instance;

  MetaDisplay *display;

  int closing;
};

struct _MetaScreenClass
{
  GObjectClass parent_class;
};

MetaScreen*   meta_screen_new                 (MetaDisplay                *display,
                                               guint32                     timestamp);
void          meta_screen_free                (MetaScreen                 *screen,
                                               guint32                     timestamp);

void          meta_screen_manage_all_windows  (MetaScreen                 *screen);

#endif
