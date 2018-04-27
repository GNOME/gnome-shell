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

#include "config.h"

#include "meta-cursor.h"

#include "backends/meta-backend-private.h"
#include "cogl/cogl.h"
#include "meta/common.h"

enum
{
  PREPARE_AT,
  TEXTURE_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct _MetaCursorSpritePrivate
{
  GObject parent;

  CoglTexture2D *texture;
  float texture_scale;
  int hot_x, hot_y;
} MetaCursorSpritePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaCursorSprite, meta_cursor_sprite, G_TYPE_OBJECT)

gboolean
meta_cursor_sprite_is_animated (MetaCursorSprite *sprite)
{
  MetaCursorSpriteClass *klass = META_CURSOR_SPRITE_GET_CLASS (sprite);

  if (klass->is_animated)
    return klass->is_animated (sprite);
  else
    return FALSE;
}

void
meta_cursor_sprite_tick_frame (MetaCursorSprite *sprite)
{
  return META_CURSOR_SPRITE_GET_CLASS (sprite)->tick_frame (sprite);
}

unsigned int
meta_cursor_sprite_get_current_frame_time (MetaCursorSprite *sprite)
{
  return META_CURSOR_SPRITE_GET_CLASS (sprite)->get_current_frame_time (sprite);
}

MetaCursorSprite *
meta_cursor_sprite_new (void)
{
  return g_object_new (META_TYPE_CURSOR_SPRITE, NULL);
}

void
meta_cursor_sprite_clear_texture (MetaCursorSprite *sprite)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  g_clear_pointer (&priv->texture, cogl_object_unref);
}

void
meta_cursor_sprite_set_texture (MetaCursorSprite *sprite,
                                CoglTexture      *texture,
                                int               hot_x,
                                int               hot_y)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  if (priv->texture == COGL_TEXTURE_2D (texture) &&
      priv->hot_x == hot_x &&
      priv->hot_y == hot_y)
    return;

  g_clear_pointer (&priv->texture, cogl_object_unref);
  if (texture)
    priv->texture = cogl_object_ref (texture);
  priv->hot_x = hot_x;
  priv->hot_y = hot_y;

  g_signal_emit (sprite, signals[TEXTURE_CHANGED], 0);
}

void
meta_cursor_sprite_set_texture_scale (MetaCursorSprite *sprite,
                                      float             scale)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  priv->texture_scale = scale;
}

CoglTexture *
meta_cursor_sprite_get_cogl_texture (MetaCursorSprite *sprite)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  return COGL_TEXTURE (priv->texture);
}

void
meta_cursor_sprite_get_hotspot (MetaCursorSprite *sprite,
                                int              *hot_x,
                                int              *hot_y)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  *hot_x = priv->hot_x;
  *hot_y = priv->hot_y;
}

float
meta_cursor_sprite_get_texture_scale (MetaCursorSprite *sprite)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  return priv->texture_scale;
}

void
meta_cursor_sprite_prepare_at (MetaCursorSprite *sprite,
                               int               x,
                               int               y)
{
  g_signal_emit (sprite, signals[PREPARE_AT], 0, x, y);
}

void
meta_cursor_sprite_realize_texture (MetaCursorSprite *sprite)
{
  MetaCursorSpriteClass *klass = META_CURSOR_SPRITE_GET_CLASS (sprite);

  if (klass->realize_texture)
    klass->realize_texture (sprite);
}

static void
meta_cursor_sprite_init (MetaCursorSprite *sprite)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  priv->texture_scale = 1.0f;
}

static void
meta_cursor_sprite_finalize (GObject *object)
{
  MetaCursorSprite *sprite = META_CURSOR_SPRITE (object);
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  g_clear_pointer (&priv->texture, cogl_object_unref);

  G_OBJECT_CLASS (meta_cursor_sprite_parent_class)->finalize (object);
}

static void
meta_cursor_sprite_class_init (MetaCursorSpriteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_cursor_sprite_finalize;

  signals[PREPARE_AT] = g_signal_new ("prepare-at",
                                      G_TYPE_FROM_CLASS (object_class),
                                      G_SIGNAL_RUN_LAST,
                                      0,
                                      NULL, NULL, NULL,
                                      G_TYPE_NONE, 2,
                                      G_TYPE_INT,
                                      G_TYPE_INT);
  signals[TEXTURE_CHANGED] = g_signal_new ("texture-changed",
                                           G_TYPE_FROM_CLASS (object_class),
                                           G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE, 0);
}
