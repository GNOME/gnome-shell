/*
 * Copyright 2013, 2018 Red Hat, Inc.
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
 *
 */

#ifndef META_CURSOR_SPRITE_XCURSOR_H
#define META_CURSOR_SPRITE_XCURSOR_H

#include <glib-object.h>
#include <X11/Xcursor/Xcursor.h>

#include "backends/meta-cursor.h"

#define META_TYPE_CURSOR_SPRITE_XCURSOR meta_cursor_sprite_xcursor_get_type ()
G_DECLARE_FINAL_TYPE (MetaCursorSpriteXcursor, meta_cursor_sprite_xcursor,
                      META, CURSOR_SPRITE_XCURSOR, MetaCursorSprite)

MetaCursorSpriteXcursor * meta_cursor_sprite_xcursor_new (MetaCursor cursor);

void meta_cursor_sprite_xcursor_set_theme_scale (MetaCursorSpriteXcursor *sprite_xcursor,
                                                 int                      scale);

MetaCursor meta_cursor_sprite_xcursor_get_cursor (MetaCursorSpriteXcursor *sprite_xcusror);

XcursorImage * meta_cursor_sprite_xcursor_get_current_image (MetaCursorSpriteXcursor *sprite_xcursor);

Cursor meta_create_x_cursor (Display    *xdisplay,
                             MetaCursor  cursor);

#endif /* META_CURSOR_SPRITE_XCURSOR_H */
