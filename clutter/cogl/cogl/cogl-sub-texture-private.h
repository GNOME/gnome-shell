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

#ifndef __COGL_SUB_TEXTURE_H
#define __COGL_SUB_TEXTURE_H

#include "cogl-handle.h"
#include "cogl-texture-private.h"

#define COGL_SUB_TEXTURE(tex) ((CoglSubTexture *) tex)

typedef struct _CoglSubTexture CoglSubTexture;

struct _CoglSubTexture
{
  CoglTexture _parent;

  CoglHandle  full_texture;

  /* The region represented by this sub-texture */
  gint        sub_x;
  gint        sub_y;
  gint        sub_width;
  gint        sub_height;
};

GQuark
_cogl_handle_sub_texture_get_type (void);

CoglHandle
_cogl_sub_texture_new (CoglHandle full_texture,
                       gint sub_x, gint sub_y,
                       gint sub_width, gint sub_height);

#endif /* __COGL_SUB_TEXTURE_H */
