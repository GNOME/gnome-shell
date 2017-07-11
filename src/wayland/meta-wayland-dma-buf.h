/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat Inc.
 * Copyright (C) 2017 Intel Corporation
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
 *     Daniel Stone <daniels@collabora.com>
 */

#ifndef META_WAYLAND_DMA_BUF_H
#define META_WAYLAND_DMA_BUF_H

#include <glib.h>
#include <glib-object.h>

#include "wayland/meta-wayland-types.h"

#define META_TYPE_WAYLAND_DMA_BUF_BUFFER (meta_wayland_dma_buf_buffer_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandDmaBufBuffer, meta_wayland_dma_buf_buffer,
                      META, WAYLAND_DMA_BUF_BUFFER, GObject);

typedef struct _MetaWaylandDmaBufBuffer MetaWaylandDmaBufBuffer;

gboolean meta_wayland_dma_buf_init (MetaWaylandCompositor *compositor);

gboolean
meta_wayland_dma_buf_buffer_attach (MetaWaylandBuffer *buffer,
                                    GError           **error);

MetaWaylandDmaBufBuffer *
meta_wayland_dma_buf_from_buffer (MetaWaylandBuffer *buffer);

#endif /* META_WAYLAND_DMA_BUF_H */
