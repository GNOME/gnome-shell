/*
 * Copyright (C) 2009 Intel Corporation.
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

#ifndef META_KEYBINDINGS_H
#define META_KEYBINDINGS_H

#include "display.h"
#include "common.h"

typedef void (* MetaKeyHandlerFunc) (MetaDisplay    *display,
                                     MetaScreen     *screen,
                                     MetaWindow     *window,
                                     XEvent         *event,
                                     MetaKeyBinding *binding,
				     gpointer        user_data);

typedef struct
{
  const char *name;
  MetaKeyHandlerFunc func;
  MetaKeyHandlerFunc default_func;
  gint data, flags;
  gpointer user_data;
  GDestroyNotify user_data_free_func;
} MetaKeyHandler;

struct _MetaKeyBinding
{
  const char *name;
  KeySym keysym;
  KeyCode keycode;
  unsigned int mask;
  MetaVirtualModifier modifiers;
  MetaKeyHandler *handler;
};


gboolean meta_keybindings_set_custom_handler (const gchar        *name,
					      MetaKeyHandlerFunc  handler,
					      gpointer            user_data,
					      GDestroyNotify      free_data);

void meta_keybindings_switch_window (MetaDisplay    *display,
				     MetaScreen     *screen,
				     MetaWindow     *event_window,
				     XEvent         *event,
				     MetaKeyBinding *binding);


void     meta_screen_ungrab_all_keys (MetaScreen *screen, guint32 timestamp);
gboolean meta_screen_grab_all_keys (MetaScreen *screen, guint32 timestamp);
#endif
