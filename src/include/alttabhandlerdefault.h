/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Alt-Tab abstraction: default implementation */

/* 
 * Copyright (C) 2009 Red Hat, Inc.
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

#ifndef META_ALT_TAB_HANDLER_DEFAULT_H
#define META_ALT_TAB_HANDLER_DEFAULT_H

#include "alttabhandler.h"
#include "tabpopup.h"

#define META_TYPE_ALT_TAB_HANDLER_DEFAULT (meta_alt_tab_handler_default_get_type ())
#define META_ALT_TAB_HANDLER_DEFAULT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_ALT_TAB_HANDLER_DEFAULT, MetaAltTabHandlerDefault))

typedef struct _MetaAltTabHandlerDefault      MetaAltTabHandlerDefault;
typedef struct _MetaAltTabHandlerDefaultClass MetaAltTabHandlerDefaultClass;

struct _MetaAltTabHandlerDefault {
  GObject parent_instance;

  MetaScreen *screen;

  GArray *entries;
  gboolean immediate_mode;

  MetaTabPopup *tab_popup;
};

struct _MetaAltTabHandlerDefaultClass {
  GObjectClass parent_class;

};

GType              meta_alt_tab_handler_default_get_type (void);

#endif

