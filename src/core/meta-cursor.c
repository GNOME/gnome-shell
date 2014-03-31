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
#include "meta-cursor-tracker-private.h" /* for tracker->gbm */

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

static void
translate_meta_cursor (MetaCursor   cursor,
                       guint       *glyph_out,
                       const char **name_out)
{
  guint glyph = XC_num_glyphs;
  const char *name = NULL;

  switch (cursor)
    {
    case META_CURSOR_DEFAULT:
      glyph = XC_left_ptr;
      break;
    case META_CURSOR_NORTH_RESIZE:
      glyph = XC_top_side;
      break;
    case META_CURSOR_SOUTH_RESIZE:
      glyph = XC_bottom_side;
      break;
    case META_CURSOR_WEST_RESIZE:
      glyph = XC_left_side;
      break;
    case META_CURSOR_EAST_RESIZE:
      glyph = XC_right_side;
      break;
    case META_CURSOR_SE_RESIZE:
      glyph = XC_bottom_right_corner;
      break;
    case META_CURSOR_SW_RESIZE:
      glyph = XC_bottom_left_corner;
      break;
    case META_CURSOR_NE_RESIZE:
      glyph = XC_top_right_corner;
      break;
    case META_CURSOR_NW_RESIZE:
      glyph = XC_top_left_corner;
      break;
    case META_CURSOR_MOVE_OR_RESIZE_WINDOW:
      glyph = XC_fleur;
      break;
    case META_CURSOR_BUSY:
      glyph = XC_watch;
      break;
    case META_CURSOR_DND_IN_DRAG:
      name = "dnd-none";
      break;
    case META_CURSOR_DND_MOVE:
      name = "dnd-move";
      break;
    case META_CURSOR_DND_COPY:
      name = "dnd-copy";
      break;
    case META_CURSOR_DND_UNSUPPORTED_TARGET:
      name = "dnd-none";
      break;
    case META_CURSOR_POINTING_HAND:
      glyph = XC_hand2;
      break;
    case META_CURSOR_CROSSHAIR:
      glyph = XC_crosshair;
      break;
    case META_CURSOR_IBEAM:
      glyph = XC_xterm;
      break;

    default:
      g_assert_not_reached ();
      glyph = 0; /* silence compiler */
      break;
    }

  *glyph_out = glyph;
  *name_out = name;
}

static Cursor
load_cursor_on_server (MetaDisplay *display,
                       MetaCursor   cursor)
{
  Cursor xcursor;
  guint glyph;
  const char *name;

  translate_meta_cursor (cursor, &glyph, &name);

  if (name != NULL)
    xcursor = XcursorLibraryLoadCursor (display->xdisplay, name);
  else
    xcursor = XCreateFontCursor (display->xdisplay, glyph);

  return xcursor;
}

Cursor
meta_display_create_x_cursor (MetaDisplay *display,
                              MetaCursor cursor)
{
  return load_cursor_on_server (display, cursor);
}

static XcursorImage *
load_cursor_on_client (MetaDisplay *display,
                       MetaCursor   cursor)
{
  XcursorImage *image;
  guint glyph;
  const char *name;
  const char *theme = XcursorGetTheme (display->xdisplay);
  int size = XcursorGetDefaultSize (display->xdisplay);

  translate_meta_cursor (cursor, &glyph, &name);

  if (name != NULL)
    image = XcursorLibraryLoadImage (name, theme, size);
  else
    image = XcursorShapeLoadImage (glyph, theme, size);

  return image;
}

static void
meta_cursor_image_load_from_xcursor_image (MetaCursorTracker *tracker,
                                           MetaCursorImage   *image,
                                           XcursorImage      *xc_image)
{
  int width, height, rowstride;
  CoglPixelFormat cogl_format;
  uint32_t gbm_format;
  ClutterBackend *clutter_backend;
  CoglContext *cogl_context;

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
                                                  (uint8_t*)xc_image->pixels,
                                                  NULL);

  if (tracker->gbm)
    {
      if (width > 64 || height > 64)
        {
          meta_warning ("Invalid theme cursor size (must be at most 64x64)\n");
          return;
        }

      if (gbm_device_is_format_supported (tracker->gbm, gbm_format,
                                          GBM_BO_USE_CURSOR_64X64 | GBM_BO_USE_WRITE))
        {
          uint32_t buf[64 * 64];
          int i;

          image->bo = gbm_bo_create (tracker->gbm, 64, 64,
                                     gbm_format, GBM_BO_USE_CURSOR_64X64 | GBM_BO_USE_WRITE);

          memset (buf, 0, sizeof(buf));
          for (i = 0; i < height; i++)
            memcpy (buf + i * 64, xc_image->pixels + i * width, width * 4);

          gbm_bo_write (image->bo, buf, 64 * 64 * 4);
        }
      else
        meta_warning ("HW cursor for format %d not supported\n", gbm_format);
    }
}

MetaCursorReference *
meta_cursor_reference_from_theme (MetaCursorTracker  *tracker,
                                  MetaCursor          cursor)
{
  MetaCursorReference *self;
  XcursorImage *image;

  if (tracker->theme_cursors[cursor])
    return meta_cursor_reference_ref (tracker->theme_cursors[cursor]);

  image = load_cursor_on_client (tracker->screen->display, cursor);
  if (!image)
    return NULL;

  self = g_slice_new0 (MetaCursorReference);
  self->ref_count = 1;
  meta_cursor_image_load_from_xcursor_image (tracker, &self->image, image);

  XcursorImageDestroy (image);
  return self;
}

static void
meta_cursor_image_load_from_buffer (MetaCursorTracker  *tracker,
                                    MetaCursorImage    *image,
                                    struct wl_resource *buffer,
                                    int                 hot_x,
                                    int                 hot_y)
{
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

      if (width > 64 || height > 64)
        {
          meta_warning ("Invalid cursor size (must be at most 64x64), falling back to software (GL) cursors\n");
          return;
        }

      if (tracker->gbm)
        {
          if (gbm_device_is_format_supported (tracker->gbm, gbm_format,
                                              GBM_BO_USE_CURSOR_64X64 | GBM_BO_USE_WRITE))
            {
              uint8_t *data;
              uint8_t buf[4 * 64 * 64];
              int i;

              image->bo = gbm_bo_create (tracker->gbm, 64, 64,
                                         gbm_format, GBM_BO_USE_CURSOR_64X64 | GBM_BO_USE_WRITE);

              data = wl_shm_buffer_get_data (shm_buffer);
              memset (buf, 0, sizeof(buf));
              for (i = 0; i < height; i++)
                memcpy (buf + i * 4 * 64, data + i * rowstride, 4 * width);

              gbm_bo_write (image->bo, buf, 64 * 64 * 4);
            }
          else
            meta_warning ("HW cursor for format %d not supported\n", gbm_format);
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

      if (tracker->gbm)
        {
          image->bo = gbm_bo_import (tracker->gbm, GBM_BO_IMPORT_WL_BUFFER,
                                     buffer, GBM_BO_USE_CURSOR_64X64);
          if (!image->bo)
            meta_warning ("Importing HW cursor from wl_buffer failed\n");
        }
    }
}

MetaCursorReference *
meta_cursor_reference_from_buffer (MetaCursorTracker  *tracker,
                                   struct wl_resource *buffer,
                                   int                 hot_x,
                                   int                 hot_y)
{
  MetaCursorReference *self;

  self = g_slice_new0 (MetaCursorReference);
  self->ref_count = 1;
  meta_cursor_image_load_from_buffer (tracker, &self->image, buffer, hot_x, hot_y);

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
