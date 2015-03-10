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

#include "meta-cursor-private.h"

#include <meta/errors.h>

#include "display-private.h"
#include "screen-private.h"
#include "meta-backend-private.h"

#ifdef HAVE_NATIVE_BACKEND
#include "backends/native/meta-cursor-renderer-native.h"
#endif

#include <string.h>

#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xcursor/Xcursor.h>

#ifdef HAVE_WAYLAND
#include <cogl/cogl-wayland-server.h>
#endif

GType meta_cursor_sprite_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (MetaCursorSprite, meta_cursor_sprite, G_TYPE_OBJECT)

static void
meta_cursor_image_free (MetaCursorImage *image)
{
  if (image->texture)
    cogl_object_unref (image->texture);

#ifdef HAVE_NATIVE_BACKEND
  if (image->bo)
    gbm_bo_destroy (image->bo);
#endif
}

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

#ifdef HAVE_NATIVE_BACKEND
static void
get_hardware_cursor_size (uint64_t *cursor_width, uint64_t *cursor_height)
{
  MetaBackend *meta_backend = meta_get_backend ();
  MetaCursorRenderer *renderer = meta_backend_get_cursor_renderer (meta_backend);

  if (META_IS_CURSOR_RENDERER_NATIVE (renderer))
    {
      meta_cursor_renderer_native_get_cursor_size (META_CURSOR_RENDERER_NATIVE (renderer), cursor_width, cursor_height);
      return;
    }

  g_assert_not_reached ();
}
#endif

#ifdef HAVE_NATIVE_BACKEND
static void
meta_cursor_image_load_gbm_buffer (struct gbm_device *gbm,
                                   MetaCursorImage   *image,
                                   uint8_t           *pixels,
                                   uint               width,
                                   uint               height,
                                   int                rowstride,
                                   uint32_t           gbm_format)
{
  uint64_t cursor_width, cursor_height;
  get_hardware_cursor_size (&cursor_width, &cursor_height);

  if (width > cursor_width || height > cursor_height)
    {
      meta_warning ("Invalid theme cursor size (must be at most %ux%u)\n",
                    (unsigned int)cursor_width, (unsigned int)cursor_height);
      return;
    }

  if (gbm_device_is_format_supported (gbm, gbm_format,
                                      GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE))
    {
      uint8_t buf[4 * cursor_width * cursor_height];
      uint i;

      image->bo = gbm_bo_create (gbm, cursor_width, cursor_height,
                                 gbm_format, GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);

      memset (buf, 0, sizeof(buf));
      for (i = 0; i < height; i++)
        memcpy (buf + i * 4 * cursor_width, pixels + i * rowstride, width * 4);

      gbm_bo_write (image->bo, buf, cursor_width * cursor_height * 4);
    }
  else
    meta_warning ("HW cursor for format %d not supported\n", gbm_format);
}
#endif

#ifdef HAVE_NATIVE_BACKEND
static struct gbm_device *
get_gbm_device (void)
{
  MetaBackend *meta_backend = meta_get_backend ();
  MetaCursorRenderer *renderer = meta_backend_get_cursor_renderer (meta_backend);

  if (META_IS_CURSOR_RENDERER_NATIVE (renderer))
    return meta_cursor_renderer_native_get_gbm_device (META_CURSOR_RENDERER_NATIVE (renderer));
  else
    return NULL;
}
#endif

static void
meta_cursor_image_load_from_xcursor_image (MetaCursorImage   *image,
                                           XcursorImage      *xc_image)
{
  uint width, height, rowstride;
  CoglPixelFormat cogl_format;
  ClutterBackend *clutter_backend;
  CoglContext *cogl_context;

  width           = xc_image->width;
  height          = xc_image->height;
  rowstride       = width * 4;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  cogl_format = COGL_PIXEL_FORMAT_BGRA_8888;
#else
  cogl_format = COGL_PIXEL_FORMAT_ARGB_8888;
#endif

  image->hot_x = xc_image->xhot;
  image->hot_y = xc_image->yhot;

  clutter_backend = clutter_get_default_backend ();
  cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  image->texture = cogl_texture_2d_new_from_data (cogl_context,
                                                  width, height,
                                                  cogl_format,
                                                  rowstride,
                                                  (uint8_t *) xc_image->pixels,
                                                  NULL);

#ifdef HAVE_NATIVE_BACKEND
  struct gbm_device *gbm = get_gbm_device ();
  if (gbm)
    meta_cursor_image_load_gbm_buffer (gbm,
                                       image,
                                       (uint8_t *) xc_image->pixels,
                                       width, height, rowstride,
                                       GBM_FORMAT_ARGB8888);
#endif
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

  meta_cursor_image_free (&self->image);
  image = meta_cursor_sprite_get_current_frame_image (self);
  meta_cursor_image_load_from_xcursor_image (&self->image, image);
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

static void
load_cursor_image (MetaCursorSprite *self)
{
  XcursorImage *image;

  /* Either cursors are loaded from X cursors or buffers. Since
   * buffers are converted over immediately, we can make sure to
   * load this directly. */
  g_assert (self->cursor != META_CURSOR_NONE);

  if (!self->xcursor_images)
    {
      self->current_frame = 0;
      self->xcursor_images = load_cursor_on_client (self->cursor);
      if (!self->xcursor_images)
        meta_fatal ("Could not find cursor. Perhaps set XCURSOR_PATH?");
    }

  image = meta_cursor_sprite_get_current_frame_image (self);
  meta_cursor_image_load_from_xcursor_image (&self->image, image);
}

MetaCursorSprite *
meta_cursor_sprite_from_theme (MetaCursor cursor)
{
  MetaCursorSprite *self;

  self = g_object_new (META_TYPE_CURSOR_SPRITE, NULL);

  self->cursor = cursor;

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

  self->image.texture = texture;
  self->image.hot_x = hot_x;
  self->image.hot_y = hot_y;

  return self;
}

#ifdef HAVE_WAYLAND
static void
meta_cursor_image_load_from_buffer (MetaCursorImage    *image,
                                    struct wl_resource *buffer,
                                    int                 hot_x,
                                    int                 hot_y)
{
  ClutterBackend *backend;
  CoglContext *cogl_context;

  image->hot_x = hot_x;
  image->hot_y = hot_y;

  backend = clutter_get_default_backend ();
  cogl_context = clutter_backend_get_cogl_context (backend);

  image->texture = cogl_wayland_texture_2d_new_from_buffer (cogl_context, buffer, NULL);

#ifdef HAVE_NATIVE_BACKEND
  struct gbm_device *gbm = get_gbm_device ();
  if (gbm)
    {
      uint32_t gbm_format;
      uint64_t cursor_width, cursor_height;
      uint width, height;

      width = cogl_texture_get_width (COGL_TEXTURE (image->texture));
      height = cogl_texture_get_height (COGL_TEXTURE (image->texture));

      struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get (buffer);
      if (shm_buffer)
        {
          int rowstride = wl_shm_buffer_get_stride (shm_buffer);

          wl_shm_buffer_begin_access (shm_buffer);

          switch (wl_shm_buffer_get_format (shm_buffer))
            {
#if G_BYTE_ORDER == G_BIG_ENDIAN
            case WL_SHM_FORMAT_ARGB8888:
              gbm_format = GBM_FORMAT_ARGB8888;
              break;
            case WL_SHM_FORMAT_XRGB8888:
              gbm_format = GBM_FORMAT_XRGB8888;
              break;
#else
            case WL_SHM_FORMAT_ARGB8888:
              gbm_format = GBM_FORMAT_ARGB8888;
              break;
            case WL_SHM_FORMAT_XRGB8888:
              gbm_format = GBM_FORMAT_XRGB8888;
              break;
#endif
            default:
              g_warn_if_reached ();
              gbm_format = GBM_FORMAT_ARGB8888;
            }

          meta_cursor_image_load_gbm_buffer (gbm,
                                             image,
                                             (uint8_t *) wl_shm_buffer_get_data (shm_buffer),
                                             width, height, rowstride,
                                             gbm_format);

          wl_shm_buffer_end_access (shm_buffer);
        }
      else
        {
          /* HW cursors have a predefined size (at least 64x64), which usually is bigger than cursor theme
             size, so themed cursors must be padded with transparent pixels to fill the
             overlay. This is trivial if we have CPU access to the data, but it's not
             possible if the buffer is in GPU memory (and possibly tiled too), so if we
             don't get the right size, we fallback to GL.
          */
          get_hardware_cursor_size (&cursor_width, &cursor_height);

          if (width != cursor_width || height != cursor_height)
            {
              meta_warning ("Invalid cursor size (must be 64x64), falling back to software (GL) cursors\n");
              return;
            }

          image->bo = gbm_bo_import (gbm, GBM_BO_IMPORT_WL_BUFFER, buffer, GBM_BO_USE_CURSOR);
          if (!image->bo)
            meta_warning ("Importing HW cursor from wl_buffer failed\n");
        }
    }
#endif
}

MetaCursorSprite *
meta_cursor_sprite_from_buffer (struct wl_resource *buffer,
                                int                 hot_x,
                                int                 hot_y)
{
  MetaCursorSprite *self;

  self = g_object_new (META_TYPE_CURSOR_SPRITE, NULL);

  meta_cursor_image_load_from_buffer (&self->image, buffer, hot_x, hot_y);

  return self;
}
#endif

CoglTexture *
meta_cursor_sprite_get_cogl_texture (MetaCursorSprite *self,
                                     int              *hot_x,
                                     int              *hot_y)
{
  if (!self->image.texture)
    load_cursor_image (self);

  if (hot_x)
    *hot_x = self->image.hot_x;
  if (hot_y)
    *hot_y = self->image.hot_y;

  return COGL_TEXTURE (self->image.texture);
}

#ifdef HAVE_NATIVE_BACKEND
struct gbm_bo *
meta_cursor_sprite_get_gbm_bo (MetaCursorSprite *self,
                               int              *hot_x,
                               int              *hot_y)
{
  if (!self->image.bo)
    load_cursor_image (self);

  if (hot_x)
    *hot_x = self->image.hot_x;
  if (hot_y)
    *hot_y = self->image.hot_y;
  return self->image.bo;
}
#endif

MetaCursor
meta_cursor_sprite_get_meta_cursor (MetaCursorSprite *self)
{
  return self->cursor;
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
  meta_cursor_image_free (&self->image);

  G_OBJECT_CLASS (meta_cursor_sprite_parent_class)->finalize (object);
}

static void
meta_cursor_sprite_class_init (MetaCursorSpriteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_cursor_sprite_finalize;
}
