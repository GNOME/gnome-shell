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

#include <X11/Xcursor/Xcursor.h>
#include <cogl/cogl.h>

#ifdef HAVE_NATIVE_BACKEND
#include <gbm.h>
#endif

typedef struct {
  CoglTexture2D *texture;
  int hot_x, hot_y;

#ifdef HAVE_NATIVE_BACKEND
  struct gbm_bo *bo;
#endif
} MetaCursorImage;

struct _MetaCursorSprite {
  int ref_count;

  int current_frame;
  XcursorImages *xcursor_images;
  MetaCursor cursor;
  MetaCursorImage image;
};

CoglTexture *meta_cursor_sprite_get_cogl_texture (MetaCursorSprite *self,
                                                  int              *hot_x,
                                                  int              *hot_y);

#ifdef HAVE_NATIVE_BACKEND
struct gbm_bo *meta_cursor_sprite_get_gbm_bo (MetaCursorSprite *self,
                                              int              *hot_x,
                                              int              *hot_y);
#endif

gboolean meta_cursor_sprite_is_animated            (MetaCursorSprite *self);
void     meta_cursor_sprite_tick_frame             (MetaCursorSprite *self);
guint    meta_cursor_sprite_get_current_frame_time (MetaCursorSprite *self);

#endif /* META_CURSOR_PRIVATE_H */
