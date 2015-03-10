/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright 2013 Red Hat, Inc.
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
 * Author: Giovanni Campagna <gcampagn@redhat.com>
 */

#ifndef META_CURSOR_H
#define META_CURSOR_H

#include <meta/common.h>

typedef struct _MetaCursorSprite MetaCursorSprite;

#define META_TYPE_CURSOR_SPRITE (meta_cursor_sprite_get_type ())
G_DECLARE_FINAL_TYPE (MetaCursorSprite,
                      meta_cursor_sprite,
                      META, CURSOR_SPRITE,
                      GObject);

MetaCursorSprite * meta_cursor_sprite_from_theme  (MetaCursor          cursor);

#ifdef HAVE_WAYLAND
#include <wayland-server.h>
MetaCursorSprite * meta_cursor_sprite_from_buffer (struct wl_resource *buffer,
                                                   int                 hot_x,
                                                   int                 hot_y);
#endif

MetaCursor meta_cursor_sprite_get_meta_cursor (MetaCursorSprite *self);

MetaCursorSprite * meta_cursor_sprite_from_texture (CoglTexture2D *texture,
                                                    int            hot_x,
                                                    int            hot_y);

Cursor meta_cursor_create_x_cursor (Display    *xdisplay,
                                    MetaCursor  cursor);

#endif /* META_CURSOR_H */
