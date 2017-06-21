/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat Inc.
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

#ifndef META_SCREEN_CAST_STREAM_H
#define META_SCREEN_CAST_STREAM_H

#include <glib-object.h>

#include "backends/meta-screen-cast-stream-src.h"
#include "meta-dbus-screen-cast.h"

#define META_TYPE_SCREEN_CAST_STREAM (meta_screen_cast_stream_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaScreenCastStream, meta_screen_cast_stream,
                          META, SCREEN_CAST_STREAM,
                          MetaDBusScreenCastStreamSkeleton)

struct _MetaScreenCastStreamClass
{
  MetaDBusScreenCastStreamSkeletonClass parent_class;

  MetaScreenCastStreamSrc * (* create_src) (MetaScreenCastStream *stream,
                                            const char           *stream_id,
                                            GError              **error);
};

gboolean meta_screen_cast_stream_start (MetaScreenCastStream *stream,
                                        GError              **error);

void meta_screen_cast_stream_close (MetaScreenCastStream *stream);

char * meta_screen_cast_stream_get_object_path (MetaScreenCastStream *stream);

#endif /* META_SCREEN_CAST_STREAM_H */
