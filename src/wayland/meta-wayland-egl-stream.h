/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat Inc.
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

#ifndef META_WAYLAND_EGL_STREAM_H
#define META_WAYLAND_EGL_STREAM_H

#include <glib.h>
#include <glib-object.h>

#include "cogl/cogl.h"
#include "wayland/meta-wayland-types.h"

gboolean meta_wayland_eglstream_controller_init (MetaWaylandCompositor *compositor);

#define META_TYPE_WAYLAND_EGL_STREAM (meta_wayland_egl_stream_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandEglStream, meta_wayland_egl_stream,
                      META, WAYLAND_EGL_STREAM, GObject);

gboolean meta_wayland_is_egl_stream_buffer (MetaWaylandBuffer *buffer);

MetaWaylandEglStream * meta_wayland_egl_stream_new (MetaWaylandBuffer *buffer,
                                                    GError           **error);

gboolean meta_wayland_egl_stream_attach (MetaWaylandEglStream *stream,
                                         GError              **error);

CoglTexture2D * meta_wayland_egl_stream_create_texture (MetaWaylandEglStream *stream,
                                                        GError              **error);
CoglSnippet * meta_wayland_egl_stream_create_snippet (void);

gboolean meta_wayland_egl_stream_is_y_inverted (MetaWaylandEglStream *stream);

#endif /* META_WAYLAND_EGL_STREAM_H */
