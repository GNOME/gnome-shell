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

#ifndef META_CURSOR_PRIVATE_H
#define META_CURSOR_PRIVATE_H

#include "meta-cursor.h"

#include <cogl/cogl.h>
#include <gbm.h>

typedef struct {
  CoglTexture2D *texture;
  struct gbm_bo *bo;
  int hot_x, hot_y;
} MetaCursorImage;

struct _MetaCursorReference {
  int ref_count;

  MetaCursorImage image;
};

CoglTexture *meta_cursor_reference_get_cogl_texture (MetaCursorReference *cursor,
                                                     int                 *hot_x,
                                                     int                 *hot_y);

struct gbm_bo *meta_cursor_reference_get_gbm_bo (MetaCursorReference *cursor,
                                                 int                 *hot_x,
                                                 int                 *hot_y);

#endif /* META_CURSOR_PRIVATE_H */
