/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015 Red Hat
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jonas Ådahl <jadahl@gmail.com>
 */

#include "config.h"

#include "backends/x11/nested/meta-cursor-renderer-x11-nested.h"

#include <X11/Xcursor/Xcursor.h>

#include "backends/x11/meta-backend-x11.h"

struct _MetaCursorRendererX11Nested
{
  MetaCursorRenderer parent;
};

G_DEFINE_TYPE (MetaCursorRendererX11Nested, meta_cursor_renderer_x11_nested,
               META_TYPE_CURSOR_RENDERER);

static gboolean
meta_cursor_renderer_x11_nested_update_cursor (MetaCursorRenderer *renderer,
                                               MetaCursorSprite   *cursor_sprite)
{
  if (cursor_sprite)
    meta_cursor_sprite_realize_texture (cursor_sprite);
  return FALSE;
}

static Cursor
create_empty_cursor (Display *xdisplay)
{
  XcursorImage *image;
  XcursorPixel *pixels;
  Cursor xcursor;

  image = XcursorImageCreate (1, 1);
  if (image == NULL)
    return None;

  image->xhot = 0;
  image->yhot = 0;

  pixels = image->pixels;
  pixels[0] = 0;

  xcursor = XcursorImageLoadCursor (xdisplay, image);
  XcursorImageDestroy (image);

  return xcursor;
}

static void
meta_cursor_renderer_x11_nested_init (MetaCursorRendererX11Nested *x11_nested)
{
  MetaBackendX11 *backend = META_BACKEND_X11 (meta_get_backend ());
  Window xwindow = meta_backend_x11_get_xwindow (backend);
  Display *xdisplay = meta_backend_x11_get_xdisplay (backend);

  Cursor empty_xcursor = create_empty_cursor (xdisplay);
  XDefineCursor (xdisplay, xwindow, empty_xcursor);
  XFreeCursor (xdisplay, empty_xcursor);
}

static void
meta_cursor_renderer_x11_nested_class_init (MetaCursorRendererX11NestedClass *klass)
{
  MetaCursorRendererClass *renderer_class = META_CURSOR_RENDERER_CLASS (klass);

  renderer_class->update_cursor = meta_cursor_renderer_x11_nested_update_cursor;
}
