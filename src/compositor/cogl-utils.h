/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Utilities for use with Cogl
 *
 * Copyright 2010 Red Hat, Inc.
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
 */

#ifndef __META_COGL_UTILS_H__
#define __META_COGL_UTILS_H__

#include <cogl/cogl.h>

CoglTexture * meta_create_color_texture_4ub (guint8           red,
                                             guint8           green,
                                             guint8           blue,
                                             guint8           alpha,
                                             CoglTextureFlags flags);
CoglPipeline * meta_create_texture_pipeline (CoglTexture *texture);

#endif /* __META_COGL_UTILS_H__ */
