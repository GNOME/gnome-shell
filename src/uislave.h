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
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct _MetaUISlave MetaUISlave;

struct _MetaUISlave
{
  char *display_name;
};

MetaUISlave* meta_ui_slave_new (const char *display_name);


#endif
