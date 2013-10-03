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

#include <meta/display.h>
#include <meta/common.h>

#define META_TYPE_KEY_BINDING               (meta_key_binding_get_type ())

const char          *meta_key_binding_get_name      (MetaKeyBinding *binding);
MetaVirtualModifier  meta_key_binding_get_modifiers (MetaKeyBinding *binding);
guint                meta_key_binding_get_mask      (MetaKeyBinding *binding);
gboolean             meta_key_binding_is_builtin    (MetaKeyBinding *binding);

gboolean meta_keybindings_set_custom_handler (const gchar        *name,
					      MetaKeyHandlerFunc  handler,
					      gpointer            user_data,
					      GDestroyNotify      free_data);

void     meta_screen_ungrab_all_keys (MetaScreen *screen, guint32 timestamp);
gboolean meta_screen_grab_all_keys (MetaScreen *screen, guint32 timestamp);
#endif
