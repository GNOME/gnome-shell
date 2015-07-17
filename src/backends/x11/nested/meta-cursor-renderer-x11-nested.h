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

#ifndef META_CURSOR_RENDERER_X11_NESTED_NESTED_H
#define META_CURSOR_RENDERER_X11_NESTED_NESTED_H

#include <glib-object.h>

#include "backends/meta-cursor-renderer.h"

#define META_TYPE_CURSOR_RENDERER_X11_NESTED (meta_cursor_renderer_x11_nested_get_type ())
G_DECLARE_FINAL_TYPE (MetaCursorRendererX11Nested,
                      meta_cursor_renderer_x11_nested,
                      META, CURSOR_RENDERER_X11_NESTED,
                      MetaCursorRenderer);

#endif /* META_CURSOR_RENDERER_X11_NESTED_NESTED_H */
