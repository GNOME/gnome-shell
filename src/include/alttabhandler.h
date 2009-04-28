/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Alt-Tab abstraction */

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

#ifndef META_ALT_TAB_HANDLER_H
#define META_ALT_TAB_HANDLER_H

#include <glib-object.h>

#include "types.h"

#define META_TYPE_ALT_TAB_HANDLER               (meta_alt_tab_handler_get_type ())
#define META_ALT_TAB_HANDLER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_ALT_TAB_HANDLER, MetaAltTabHandler))
#define META_ALT_TAB_HANDLER_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), META_TYPE_ALT_TAB_HANDLER, MetaAltTabHandlerInterface))

typedef struct _MetaAltTabHandler          MetaAltTabHandler;
typedef struct _MetaAltTabHandlerInterface MetaAltTabHandlerInterface;

struct _MetaAltTabHandlerInterface {
  GTypeInterface g_iface;

  void         (*add_window)   (MetaAltTabHandler *handler,
                                MetaWindow        *window);

  void         (*show)         (MetaAltTabHandler *handler,
                                MetaWindow        *initial_selection);
  void         (*destroy)      (MetaAltTabHandler *handler);

  void         (*forward)      (MetaAltTabHandler *handler);
  void         (*backward)     (MetaAltTabHandler *handler);

  MetaWindow * (*get_selected) (MetaAltTabHandler *handler);
};

GType              meta_alt_tab_handler_get_type     (void);

void               meta_alt_tab_handler_register     (GType              type);
MetaAltTabHandler *meta_alt_tab_handler_new          (MetaScreen        *screen,
                                                      gboolean           immediate);

void               meta_alt_tab_handler_add_window   (MetaAltTabHandler *handler,
                                                      MetaWindow        *window);

void               meta_alt_tab_handler_show         (MetaAltTabHandler *handler,
                                                      MetaWindow        *initial_selection);
void               meta_alt_tab_handler_destroy      (MetaAltTabHandler *handler);

void               meta_alt_tab_handler_forward      (MetaAltTabHandler *handler);
void               meta_alt_tab_handler_backward     (MetaAltTabHandler *handler);

MetaWindow        *meta_alt_tab_handler_get_selected (MetaAltTabHandler *handler);

#endif

