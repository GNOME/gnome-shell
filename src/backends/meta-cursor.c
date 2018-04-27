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

#include <string.h>
#include <X11/Xcursor/Xcursor.h>

#include "backends/meta-backend-private.h"
#include "backends/meta-cursor-renderer.h"
#include "clutter/clutter.h"
#include "cogl/cogl.h"
#include "meta/common.h"
#include "meta/prefs.h"

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

  MetaCursor cursor;

  CoglTexture2D *texture;
  float texture_scale;
  int hot_x, hot_y;

  int current_frame;
  XcursorImages *xcursor_images;

  int theme_scale;
  gboolean theme_dirty;
} MetaCursorSpritePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaCursorSprite, meta_cursor_sprite, G_TYPE_OBJECT)

static const char *
translate_meta_cursor (MetaCursor cursor)
{
  switch (cursor)
    {
    case META_CURSOR_DEFAULT:
      return "left_ptr";
    case META_CURSOR_NORTH_RESIZE:
      return "top_side";
    case META_CURSOR_SOUTH_RESIZE:
      return "bottom_side";
    case META_CURSOR_WEST_RESIZE:
      return "left_side";
    case META_CURSOR_EAST_RESIZE:
      return "right_side";
    case META_CURSOR_SE_RESIZE:
      return "bottom_right_corner";
    case META_CURSOR_SW_RESIZE:
      return "bottom_left_corner";
    case META_CURSOR_NE_RESIZE:
      return "top_right_corner";
    case META_CURSOR_NW_RESIZE:
      return "top_left_corner";
    case META_CURSOR_MOVE_OR_RESIZE_WINDOW:
      return "fleur";
    case META_CURSOR_BUSY:
      return "watch";
    case META_CURSOR_DND_IN_DRAG:
      return "dnd-none";
    case META_CURSOR_DND_MOVE:
      return "dnd-move";
    case META_CURSOR_DND_COPY:
      return "dnd-copy";
    case META_CURSOR_DND_UNSUPPORTED_TARGET:
      return "dnd-none";
    case META_CURSOR_POINTING_HAND:
      return "hand2";
    case META_CURSOR_CROSSHAIR:
      return "crosshair";
    case META_CURSOR_IBEAM:
      return "xterm";
    default:
      break;
    }

  g_assert_not_reached ();
}

Cursor
meta_create_x_cursor (Display    *xdisplay,
                      MetaCursor  cursor)
{
  return XcursorLibraryLoadCursor (xdisplay, translate_meta_cursor (cursor));
}

static XcursorImages *
load_cursor_on_client (MetaCursor cursor, int scale)
{
  return XcursorLibraryLoadImages (translate_meta_cursor (cursor),
                                   meta_prefs_get_cursor_theme (),
                                   meta_prefs_get_cursor_size () * scale);
}

static void
meta_cursor_sprite_load_from_xcursor_image (MetaCursorSprite *sprite,
                                            XcursorImage     *xc_image)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);
  MetaBackend *backend = meta_get_backend ();
  MetaCursorRenderer *renderer = meta_backend_get_cursor_renderer (backend);
  uint width, height, rowstride;
  CoglPixelFormat cogl_format;
  ClutterBackend *clutter_backend;
  CoglContext *cogl_context;
  CoglTexture2D *texture;
  CoglError *error = NULL;

  g_assert (priv->texture == NULL);

  width           = xc_image->width;
  height          = xc_image->height;
  rowstride       = width * 4;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  cogl_format = COGL_PIXEL_FORMAT_BGRA_8888;
#else
  cogl_format = COGL_PIXEL_FORMAT_ARGB_8888;
#endif

  clutter_backend = clutter_get_default_backend ();
  cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  texture = cogl_texture_2d_new_from_data (cogl_context,
                                           width, height,
                                           cogl_format,
                                           rowstride,
                                           (uint8_t *) xc_image->pixels,
                                           &error);

  if (error)
    {
      meta_warning ("Failed to allocate cursor texture: %s\n", error->message);
      cogl_error_free (error);
    }

  meta_cursor_sprite_set_texture (sprite, COGL_TEXTURE (texture),
                                  xc_image->xhot, xc_image->yhot);

  if (texture)
    cogl_object_unref (texture);

  meta_cursor_renderer_realize_cursor_from_xcursor (renderer, sprite, xc_image);
}

static XcursorImage *
meta_cursor_sprite_get_current_frame_image (MetaCursorSprite *sprite)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  return priv->xcursor_images->images[priv->current_frame];
}

void
meta_cursor_sprite_tick_frame (MetaCursorSprite *sprite)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);
  XcursorImage *image;

  if (!meta_cursor_sprite_is_animated (sprite))
    return;

  priv->current_frame++;

  if (priv->current_frame >= priv->xcursor_images->nimage)
    priv->current_frame = 0;

  image = meta_cursor_sprite_get_current_frame_image (sprite);

  g_clear_pointer (&priv->texture, cogl_object_unref);
  meta_cursor_sprite_load_from_xcursor_image (sprite, image);
}

guint
meta_cursor_sprite_get_current_frame_time (MetaCursorSprite *sprite)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  if (!meta_cursor_sprite_is_animated (sprite))
    return 0;

  return priv->xcursor_images->images[priv->current_frame]->delay;
}

gboolean
meta_cursor_sprite_is_animated (MetaCursorSprite *sprite)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  return (priv->xcursor_images &&
          priv->xcursor_images->nimage > 1);
}

MetaCursorSprite *
meta_cursor_sprite_new (void)
{
  return g_object_new (META_TYPE_CURSOR_SPRITE, NULL);
}

static void
meta_cursor_sprite_load_from_theme (MetaCursorSprite *sprite)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);
  XcursorImage *image;

  g_assert (priv->cursor != META_CURSOR_NONE);

  priv->theme_dirty = FALSE;

  /* We might be reloading with a different scale. If so clear the old data. */
  if (priv->xcursor_images)
    {
      g_clear_pointer (&priv->texture, cogl_object_unref);
      XcursorImagesDestroy (priv->xcursor_images);
    }

  priv->current_frame = 0;
  priv->xcursor_images = load_cursor_on_client (priv->cursor,
                                                priv->theme_scale);
  if (!priv->xcursor_images)
    meta_fatal ("Could not find cursor. Perhaps set XCURSOR_PATH?");

  image = meta_cursor_sprite_get_current_frame_image (sprite);
  meta_cursor_sprite_load_from_xcursor_image (sprite, image);
}

MetaCursorSprite *
meta_cursor_sprite_from_theme (MetaCursor cursor)
{
  MetaCursorSprite *sprite;
  MetaCursorSpritePrivate *priv;

  sprite = meta_cursor_sprite_new ();
  priv = meta_cursor_sprite_get_instance_private (sprite);

  priv->cursor = cursor;
  priv->theme_dirty = TRUE;

  return sprite;
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

void
meta_cursor_sprite_set_theme_scale (MetaCursorSprite *sprite,
                                    int               theme_scale)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  if (priv->theme_scale != theme_scale)
    priv->theme_dirty = TRUE;
  priv->theme_scale = theme_scale;
}

CoglTexture *
meta_cursor_sprite_get_cogl_texture (MetaCursorSprite *sprite)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  return COGL_TEXTURE (priv->texture);
}

MetaCursor
meta_cursor_sprite_get_meta_cursor (MetaCursorSprite *sprite)
{
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  return priv->cursor;
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
  MetaCursorSpritePrivate *priv =
    meta_cursor_sprite_get_instance_private (sprite);

  if (priv->theme_dirty)
    meta_cursor_sprite_load_from_theme (sprite);
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

  if (priv->xcursor_images)
    XcursorImagesDestroy (priv->xcursor_images);

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
