/* Metacity X display handler */

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

#ifndef META_DISPLAY_H
#define META_DISPLAY_H

#include <glib.h>
#include <Xlib.h>
#include "eventqueue.h"

typedef struct _MetaWindow  MetaWindow;
typedef struct _MetaScreen  MetaScreen;
typedef struct _MetaDisplay MetaDisplay;

struct _MetaDisplay
{
  char *name;
  Display *xdisplay;
  
  /*< private >*/
  MetaEventQueue *events;
  GSList *screens;
  GHashTable *window_ids;
};

gboolean    meta_display_open            (const char  *name);
void        meta_display_close           (MetaDisplay *display);
MetaScreen* meta_display_screen_for_root (MetaDisplay *display,
                                          Window       xroot);

MetaWindow* meta_display_lookup_window   (MetaDisplay *display,
                                          Window       xwindow);
void        meta_display_register_window (MetaDisplay *display,
                                          MetaWindow  *window);



#endif
