/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015-2017 Red Hat Inc.
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
 */

#ifndef META_SCREEN_CAST_STREAM_SRC_H
#define META_SCREEN_CAST_STREAM_SRC_H

#include <glib-object.h>

#include "clutter/clutter.h"
#include "mutter/meta/boxes.h"

typedef struct _MetaScreenCastStream MetaScreenCastStream;

#define META_TYPE_SCREEN_CAST_STREAM_SRC (meta_screen_cast_stream_src_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaScreenCastStreamSrc,
                          meta_screen_cast_stream_src,
                          META, SCREEN_CAST_STREAM_SRC,
                          GObject)

struct _MetaScreenCastStreamSrcClass
{
  GObjectClass parent_class;

  void (* get_specs) (MetaScreenCastStreamSrc *src,
                      int                     *width,
                      int                     *height,
                      float                   *frame_rate);
  void (* record_frame) (MetaScreenCastStreamSrc *src,
                         uint8_t                 *data);
};

void meta_screen_cast_stream_src_maybe_record_frame (MetaScreenCastStreamSrc *src);

MetaScreenCastStream * meta_screen_cast_stream_src_get_stream (MetaScreenCastStreamSrc *src);

#endif /* META_SCREEN_CAST_STREAM_SRC_H */
