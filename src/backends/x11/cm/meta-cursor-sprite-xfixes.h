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

#ifndef META_CURSOR_SPRITE_XFIXES_H
#define META_CURSOR_SPRITE_XFIXES_H

#include <glib-object.h>

#include "backends/meta-cursor.h"
#include "meta/types.h"

#define META_TYPE_CURSOR_SPRITE_XFIXES (meta_cursor_sprite_xfixes_get_type ())
G_DECLARE_FINAL_TYPE (MetaCursorSpriteXfixes,
                      meta_cursor_sprite_xfixes,
                      META, CURSOR_SPRITE_XFIXES,
                      MetaCursorSprite)

MetaCursorSpriteXfixes * meta_cursor_sprite_xfixes_new (MetaDisplay  *display,
                                                        GError      **error);

#endif /* META_CURSOR_SPRITE_XFIXES_H */
