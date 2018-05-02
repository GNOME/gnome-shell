/*
 * Copyright 2015, 2018 Red Hat, Inc.
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

#include "config.h"

#include "wayland/meta-cursor-sprite-wayland.h"

struct _MetaCursorSpriteWayland
{
  MetaCursorSprite parent;

  MetaWaylandSurface *surface;
};

G_DEFINE_TYPE (MetaCursorSpriteWayland,
               meta_cursor_sprite_wayland,
               META_TYPE_CURSOR_SPRITE)

static void
meta_cursor_sprite_wayland_realize_texture (MetaCursorSprite *sprite)
{
}

static gboolean
meta_cursor_sprite_wayland_is_animated (MetaCursorSprite *sprite)
{
  return FALSE;
}

MetaCursorSpriteWayland *
meta_cursor_sprite_wayland_new (MetaWaylandSurface *surface)
{
  MetaCursorSpriteWayland *sprite_wayland;

  sprite_wayland = g_object_new (META_TYPE_CURSOR_SPRITE_WAYLAND, NULL);
  sprite_wayland->surface = surface;

  return sprite_wayland;
}

MetaWaylandBuffer *
meta_cursor_sprite_wayland_get_buffer (MetaCursorSpriteWayland *sprite_wayland)
{
  return meta_wayland_surface_get_buffer (sprite_wayland->surface);
}

static void
meta_cursor_sprite_wayland_init (MetaCursorSpriteWayland *sprite_wayland)
{
}

static void
meta_cursor_sprite_wayland_class_init (MetaCursorSpriteWaylandClass *klass)
{
  MetaCursorSpriteClass *cursor_sprite_class = META_CURSOR_SPRITE_CLASS (klass);

  cursor_sprite_class->realize_texture =
    meta_cursor_sprite_wayland_realize_texture;
  cursor_sprite_class->is_animated = meta_cursor_sprite_wayland_is_animated;
}
