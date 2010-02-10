/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __COGL_ATLAS_H
#define __COGL_ATLAS_H

#include <glib.h>

typedef struct _CoglAtlas          CoglAtlas;
typedef struct _CoglAtlasRectangle CoglAtlasRectangle;

typedef void (* CoglAtlasCallback) (const CoglAtlasRectangle *rectangle,
                                    gpointer rectangle_data,
                                    gpointer user_data);

struct _CoglAtlasRectangle
{
  unsigned int x, y;
  unsigned int width, height;
};

CoglAtlas *
_cogl_atlas_new (unsigned int width,
                 unsigned int height,
                 GDestroyNotify value_destroy_func);

gboolean
_cogl_atlas_add_rectangle (CoglAtlas *atlas,
                           unsigned int width,
                           unsigned int height,
                           gpointer data,
                           CoglAtlasRectangle *rectangle);

void
_cogl_atlas_remove_rectangle (CoglAtlas *atlas,
                              const CoglAtlasRectangle *rectangle);

unsigned int
_cogl_atlas_get_width (CoglAtlas *atlas);

unsigned int
_cogl_atlas_get_height (CoglAtlas *atlas);

unsigned int
_cogl_atlas_get_remaining_space (CoglAtlas *atlas);

unsigned int
_cogl_atlas_get_n_rectangles (CoglAtlas *atlas);

void
_cogl_atlas_foreach (CoglAtlas *atlas,
                     CoglAtlasCallback callback,
                     gpointer data);

void
_cogl_atlas_free (CoglAtlas *atlas);

#endif /* __COGL_ATLAS_H */
