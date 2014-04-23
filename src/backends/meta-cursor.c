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
#include "meta-backend.h"
#include "backends/native/meta-cursor-renderer-native.h"

#include <string.h>

#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xcursor/Xcursor.h>

#include <cogl/cogl-wayland-server.h>

MetaCursorReference *
meta_cursor_reference_ref (MetaCursorReference *self)
{
  g_assert (self->ref_count > 0);
  self->ref_count++;

  return self;
}

static void
meta_cursor_image_free (MetaCursorImage *image)
{
  cogl_object_unref (image->texture);
  if (image->bo)
    gbm_bo_destroy (image->bo);
}

static void
meta_cursor_reference_free (MetaCursorReference *self)
{
  meta_cursor_image_free (&self->image);
  g_slice_free (MetaCursorReference, self);
}

void
meta_cursor_reference_unref (MetaCursorReference *self)
{
  self->ref_count--;

  if (self->ref_count == 0)
    meta_cursor_reference_free (self);
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

static XcursorImage *
load_cursor_on_client (MetaCursor cursor)
{
  return XcursorLibraryLoadImage (translate_meta_cursor (cursor),
                                  meta_prefs_get_cursor_theme (),
                                  meta_prefs_get_cursor_size ());
}

static void
meta_cursor_image_load_gbm_buffer (struct gbm_device *gbm,
                                   MetaCursorImage   *image,
                                   uint8_t           *pixels,
                                   int                width,
                                   int                height,
                                   int                rowstride,
                                   uint32_t           gbm_format)
{
  if (width > 64 || height > 64)
    {
      meta_warning ("Invalid theme cursor size (must be at most 64x64)\n");
      return;
    }

  if (gbm_device_is_format_supported (gbm, gbm_format,
                                      GBM_BO_USE_CURSOR_64X64 | GBM_BO_USE_WRITE))
    {
      uint8_t buf[4 * 64 * 64];
      int i;

      image->bo = gbm_bo_create (gbm, 64, 64,
                                 gbm_format, GBM_BO_USE_CURSOR_64X64 | GBM_BO_USE_WRITE);

      memset (buf, 0, sizeof(buf));
      for (i = 0; i < height; i++)
        memcpy (buf + i * 4 * 64, pixels + i * rowstride, width * 4);

      gbm_bo_write (image->bo, buf, 64 * 64 * 4);
    }
  else
    meta_warning ("HW cursor for format %d not supported\n", gbm_format);
}

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

static void
meta_cursor_image_load_from_xcursor_image (MetaCursorImage   *image,
                                           XcursorImage      *xc_image)
{
  int width, height, rowstride;
  CoglPixelFormat cogl_format;
  uint32_t gbm_format;
  ClutterBackend *clutter_backend;
  CoglContext *cogl_context;
  struct gbm_device *gbm;

  width           = xc_image->width;
  height          = xc_image->height;
  rowstride       = width * 4;

  gbm_format = GBM_FORMAT_ARGB8888;
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

  gbm = get_gbm_device ();
  if (gbm)
    meta_cursor_image_load_gbm_buffer (gbm,
                                       image,
                                       (uint8_t *) xc_image->pixels,
                                       width, height, rowstride,
                                       gbm_format);
}

MetaCursorReference *
meta_cursor_reference_from_theme (MetaCursor cursor)
{
  MetaCursorReference *self;
  XcursorImage *image;

  image = load_cursor_on_client (cursor);
  if (!image)
    return NULL;

  self = g_slice_new0 (MetaCursorReference);
  self->ref_count = 1;
  meta_cursor_image_load_from_xcursor_image (&self->image, image);

  XcursorImageDestroy (image);
  return self;
}

static void
meta_cursor_image_load_from_buffer (MetaCursorImage    *image,
                                    struct wl_resource *buffer,
                                    int                 hot_x,
                                    int                 hot_y)
{
  struct gbm_device *gbm = get_gbm_device ();

  ClutterBackend *backend;
  CoglContext *cogl_context;
  struct wl_shm_buffer *shm_buffer;
  uint32_t gbm_format;
  int width, height;

  image->hot_x = hot_x;
  image->hot_y = hot_y;

  backend = clutter_get_default_backend ();
  cogl_context = clutter_backend_get_cogl_context (backend);

  image->texture = cogl_wayland_texture_2d_new_from_buffer (cogl_context, buffer, NULL);

  width = cogl_texture_get_width (COGL_TEXTURE (image->texture));
  height = cogl_texture_get_height (COGL_TEXTURE (image->texture));

  shm_buffer = wl_shm_buffer_get (buffer);
  if (shm_buffer)
    {
      if (gbm)
        {
          int rowstride = wl_shm_buffer_get_stride (shm_buffer);

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
        }
    }
  else
    {
      /* HW cursors must be 64x64, but 64x64 is huge, and no cursor theme actually uses
         that, so themed cursors must be padded with transparent pixels to fill the
         overlay. This is trivial if we have CPU access to the data, but it's not
         possible if the buffer is in GPU memory (and possibly tiled too), so if we
         don't get the right size, we fallback to GL.
      */
      if (width != 64 || height != 64)
        {
          meta_warning ("Invalid cursor size (must be 64x64), falling back to software (GL) cursors\n");
          return;
        }

      if (gbm)
        {
          image->bo = gbm_bo_import (gbm, GBM_BO_IMPORT_WL_BUFFER,
                                     buffer, GBM_BO_USE_CURSOR_64X64);
          if (!image->bo)
            meta_warning ("Importing HW cursor from wl_buffer failed\n");
        }
    }
}

MetaCursorReference *
meta_cursor_reference_from_buffer (struct wl_resource *buffer,
                                   int                 hot_x,
                                   int                 hot_y)
{
  MetaCursorReference *self;

  self = g_slice_new0 (MetaCursorReference);
  self->ref_count = 1;
  meta_cursor_image_load_from_buffer (&self->image, buffer, hot_x, hot_y);

  return self;
}

CoglTexture *
meta_cursor_reference_get_cogl_texture (MetaCursorReference *cursor,
                                        int                 *hot_x,
                                        int                 *hot_y)
{
  if (hot_x)
    *hot_x = cursor->image.hot_x;
  if (hot_y)
    *hot_y = cursor->image.hot_y;
  return COGL_TEXTURE (cursor->image.texture);
}

struct gbm_bo *
meta_cursor_reference_get_gbm_bo (MetaCursorReference *cursor,
                                  int                 *hot_x,
                                  int                 *hot_y)
{
  if (hot_x)
    *hot_x = cursor->image.hot_x;
  if (hot_y)
    *hot_y = cursor->image.hot_y;
  return cursor->image.bo;
}
