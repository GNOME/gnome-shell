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

typedef struct _MetaDisplay MetaDisplay;
typedef struct _MetaFrame   MetaFrame;
typedef struct _MetaScreen  MetaScreen;
typedef struct _MetaWindow  MetaWindow;

struct _MetaDisplay
{
  char *name;
  Display *xdisplay;
  
  MetaEventQueue *events;
  GSList *screens;
  GHashTable *window_ids;
  GSList *error_traps;
  int server_grab_count;
};

gboolean    meta_display_open            (const char  *name);
void        meta_display_close           (MetaDisplay *display);
MetaScreen* meta_display_screen_for_root (MetaDisplay *display,
                                          Window       xroot);
void        meta_display_grab            (MetaDisplay *display);
void        meta_display_ungrab          (MetaDisplay *display);

/* A given MetaWindow may have various X windows that "belong"
 * to it, such as the frame window.
 */
MetaWindow* meta_display_lookup_x_window     (MetaDisplay *display,
                                              Window       xwindow);
void        meta_display_register_x_window   (MetaDisplay *display,
                                              Window      *xwindowp,
                                              MetaWindow  *window);
void        meta_display_unregister_x_window (MetaDisplay *display,
                                              Window       xwindow);



MetaDisplay* meta_display_for_x_display  (Display     *xdisplay);
GSList*      meta_displays_list          (void);

#endif
