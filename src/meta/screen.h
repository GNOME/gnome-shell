/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2008 Iain Holmes
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

#ifndef META_SCREEN_H
#define META_SCREEN_H

#include <X11/Xlib.h>
#include <glib-object.h>
#include <meta/types.h>
#include <meta/workspace.h>

#define META_TYPE_SCREEN            (meta_screen_get_type ())
#define META_SCREEN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_SCREEN, MetaScreen))
#define META_SCREEN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_SCREEN, MetaScreenClass))
#define META_IS_SCREEN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_SCREEN))
#define META_IS_SCREEN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_SCREEN))
#define META_SCREEN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_SCREEN, MetaScreenClass))

typedef struct _MetaScreenClass   MetaScreenClass;

GType meta_screen_get_type (void);

MetaDisplay *meta_screen_get_display (MetaScreen *screen);

#endif
