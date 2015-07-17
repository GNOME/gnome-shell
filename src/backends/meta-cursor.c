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

#include <meta/errors.h>

#include "display-private.h"
#include "screen-private.h"
#include "meta-backend-private.h"

#include <string.h>

#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xcursor/Xcursor.h>

#ifdef HAVE_WAYLAND
#include <cogl/cogl-wayland-server.h>
#endif

struct _MetaCursorSprite
{
  GObject parent;

  MetaCursor cursor;

  CoglTexture2D *texture;
  int hot_x, hot_y;

  int current_frame;
  XcursorImages *xcursor_images;
};

GType meta_cursor_sprite_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (MetaCursorSprite, meta_cursor_sprite, G_TYPE_OBJECT)

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
meta_cursor_create_x_cursor (Display    *xdisplay,
                             MetaCursor  cursor)
{
  return XcursorLibraryLoadCursor (xdisplay, translate_meta_cursor (cursor));
}

static XcursorImages *
load_cursor_on_client (MetaCursor cursor)
{
  return XcursorLibraryLoadImages (translate_meta_cursor (cursor),
                                   meta_prefs_get_cursor_theme (),
                                   meta_prefs_get_cursor_size ());
}

static void
meta_cursor_sprite_load_from_xcursor_image (MetaCursorSprite *self,
                                            XcursorImage     *xc_image)
{
  MetaBackend *meta_backend = meta_get_backend ();
  MetaCursorRenderer *renderer = meta_backend_get_cursor_renderer (meta_backend);
  uint width, height, rowstride;
  CoglPixelFormat cogl_format;
  ClutterBackend *clutter_backend;
  CoglContext *cogl_context;

  g_assert (self->texture == NULL);

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
  self->texture = cogl_texture_2d_new_from_data (cogl_context,
                                                 width, height,
                                                 cogl_format,
                                                 rowstride,
                                                 (uint8_t *) xc_image->pixels,
                                                 NULL);
  self->hot_x = xc_image->xhot;
  self->hot_y = xc_image->yhot;

  meta_cursor_renderer_realize_cursor_from_xcursor (renderer, self, xc_image);
}

static XcursorImage *
meta_cursor_sprite_get_current_frame_image (MetaCursorSprite *self)
{
  return self->xcursor_images->images[self->current_frame];
}

void
meta_cursor_sprite_tick_frame (MetaCursorSprite *self)
{
  XcursorImage *image;

  if (!meta_cursor_sprite_is_animated (self))
    return;

  self->current_frame++;

  if (self->current_frame >= self->xcursor_images->nimage)
    self->current_frame = 0;

  image = meta_cursor_sprite_get_current_frame_image (self);

  g_clear_pointer (&self->texture, cogl_object_unref);
  meta_cursor_sprite_load_from_xcursor_image (self, image);
}

guint
meta_cursor_sprite_get_current_frame_time (MetaCursorSprite *self)
{
  if (!meta_cursor_sprite_is_animated (self))
    return 0;

  return self->xcursor_images->images[self->current_frame]->delay;
}

gboolean
meta_cursor_sprite_is_animated (MetaCursorSprite *self)
{
  return (self->xcursor_images &&
          self->xcursor_images->nimage > 1);
}

MetaCursorSprite *
meta_cursor_sprite_from_theme (MetaCursor cursor)
{
  MetaCursorSprite *self;
  XcursorImage *image;

  self = g_object_new (META_TYPE_CURSOR_SPRITE, NULL);

  self->cursor = cursor;
  self->current_frame = 0;
  self->xcursor_images = load_cursor_on_client (self->cursor);
  if (!self->xcursor_images)
    meta_fatal ("Could not find cursor. Perhaps set XCURSOR_PATH?");

  image = meta_cursor_sprite_get_current_frame_image (self);
  meta_cursor_sprite_load_from_xcursor_image (self, image);

  return self;
}

MetaCursorSprite *
meta_cursor_sprite_from_texture (CoglTexture2D *texture,
                                 int            hot_x,
                                 int            hot_y)
{
  MetaCursorSprite *self;

  self = g_object_new (META_TYPE_CURSOR_SPRITE, NULL);

  cogl_object_ref (texture);

  self->texture = texture;
  self->hot_x = hot_x;
  self->hot_y = hot_y;

  return self;
}

#ifdef HAVE_WAYLAND
static void
meta_cursor_sprite_load_from_buffer (MetaCursorSprite   *self,
                                     struct wl_resource *buffer,
                                     int                 hot_x,
                                     int                 hot_y)
{
  MetaBackend *meta_backend = meta_get_backend ();
  MetaCursorRenderer *renderer =
    meta_backend_get_cursor_renderer (meta_backend);
  ClutterBackend *backend;
  CoglContext *cogl_context;

  self->hot_x = hot_x;
  self->hot_y = hot_y;

  backend = clutter_get_default_backend ();
  cogl_context = clutter_backend_get_cogl_context (backend);

  self->texture = cogl_wayland_texture_2d_new_from_buffer (cogl_context, buffer, NULL);

  meta_cursor_renderer_realize_cursor_from_wl_buffer (renderer, self, buffer);
}

MetaCursorSprite *
meta_cursor_sprite_from_buffer (struct wl_resource *buffer,
                                int                 hot_x,
                                int                 hot_y)
{
  MetaCursorSprite *self;

  self = g_object_new (META_TYPE_CURSOR_SPRITE, NULL);

  meta_cursor_sprite_load_from_buffer (self, buffer, hot_x, hot_y);

  return self;
}
#endif

CoglTexture *
meta_cursor_sprite_get_cogl_texture (MetaCursorSprite *self,
                                     int              *hot_x,
                                     int              *hot_y)
{
  if (hot_x)
    *hot_x = self->hot_x;
  if (hot_y)
    *hot_y = self->hot_y;

  return COGL_TEXTURE (self->texture);
}

MetaCursor
meta_cursor_sprite_get_meta_cursor (MetaCursorSprite *self)
{
  return self->cursor;
}

void
meta_cursor_sprite_get_hotspot (MetaCursorSprite *self,
                                int              *hot_x,
                                int              *hot_y)
{
  *hot_x = self->hot_x;
  *hot_y = self->hot_y;
}

guint
meta_cursor_sprite_get_width (MetaCursorSprite *self)
{
  return cogl_texture_get_width (COGL_TEXTURE (self->texture));
}

guint
meta_cursor_sprite_get_height (MetaCursorSprite *self)
{
  return cogl_texture_get_height (COGL_TEXTURE (self->texture));
}

static void
meta_cursor_sprite_init (MetaCursorSprite *self)
{
}

static void
meta_cursor_sprite_finalize (GObject *object)
{
  MetaCursorSprite *self = META_CURSOR_SPRITE (object);

  if (self->xcursor_images)
    XcursorImagesDestroy (self->xcursor_images);

  g_clear_pointer (&self->texture, cogl_object_unref);

  G_OBJECT_CLASS (meta_cursor_sprite_parent_class)->finalize (object);
}

static void
meta_cursor_sprite_class_init (MetaCursorSpriteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_cursor_sprite_finalize;
}
