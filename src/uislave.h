/* Metacity UI Slave */

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

#ifndef META_UI_SLAVE_H
#define META_UI_SLAVE_H

#include "util.h"
#include "uislave/messages.h"
#include "messagequeue.h"
#include "display.h"

typedef void (* MetaUISlaveFunc) (MetaUISlave *uislave,
                                  MetaMessage *message,
                                  gpointer     data);

struct _MetaUISlave
{
  char *display_name;
  int child_pid;
  int in_pipe;
  int out_pipe;

  MetaMessageQueue *mq;

  MetaUISlaveFunc func;
  gpointer data;

  int serial;
  
  /* if we determine that our available slave is hosed,
   * set this bit.
   */
  guint no_respawn : 1;
};

MetaUISlave* meta_ui_slave_new     (const char      *display_name,
                                    MetaUISlaveFunc  func,
                                    gpointer         data);
void         meta_ui_slave_free    (MetaUISlave     *uislave);
void         meta_ui_slave_disable (MetaUISlave     *uislave);

void         meta_ui_slave_show_tip (MetaUISlave    *uislave,
                                     int             root_x,
                                     int             root_y,
                                     const char     *markup_text);
void         meta_ui_slave_hide_tip (MetaUISlave    *uislave);

void meta_ui_slave_show_window_menu (MetaUISlave             *uislave,
                                     MetaWindow              *window,
                                     int                      root_x,
                                     int                      root_y,
                                     int                      button,
                                     MetaMessageWindowMenuOps ops,
                                     MetaMessageWindowMenuOps insensitive,
                                     Time                     timestamp);
void meta_ui_slave_hide_window_menu (MetaUISlave             *uislave);

#endif

