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
#include <meta/boxes.h>

#define META_TYPE_CURSOR_SPRITE (meta_cursor_sprite_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaCursorSprite,
                          meta_cursor_sprite,
                          META, CURSOR_SPRITE,
                          GObject)

struct _MetaCursorSpriteClass
{
  GObjectClass parent_class;

  void (* realize_texture) (MetaCursorSprite *sprite);
  gboolean (* is_animated) (MetaCursorSprite *sprite);
  void (* tick_frame) (MetaCursorSprite *sprite);
  unsigned int (* get_current_frame_time) (MetaCursorSprite *sprite);
};

MetaCursorSprite * meta_cursor_sprite_new (void);

void meta_cursor_sprite_prepare_at (MetaCursorSprite *sprite,
                                    int               x,
                                    int               y);

void meta_cursor_sprite_realize_texture (MetaCursorSprite *sprite);

void meta_cursor_sprite_clear_texture (MetaCursorSprite *sprite);

void meta_cursor_sprite_set_texture (MetaCursorSprite *sprite,
                                     CoglTexture      *texture,
                                     int               hot_x,
                                     int               hot_y);

void meta_cursor_sprite_set_texture_scale (MetaCursorSprite *sprite,
                                           float             scale);

CoglTexture *meta_cursor_sprite_get_cogl_texture (MetaCursorSprite *sprite);

void meta_cursor_sprite_get_hotspot (MetaCursorSprite *sprite,
                                     int              *hot_x,
                                     int              *hot_y);

float meta_cursor_sprite_get_texture_scale (MetaCursorSprite *sprite);

gboolean meta_cursor_sprite_is_animated (MetaCursorSprite *sprite);

void meta_cursor_sprite_tick_frame (MetaCursorSprite *sprite);

unsigned int meta_cursor_sprite_get_current_frame_time (MetaCursorSprite *sprite);

#endif /* META_CURSOR_H */
