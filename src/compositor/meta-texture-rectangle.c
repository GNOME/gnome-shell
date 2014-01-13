/*
 * texture rectangle
 *
 * A small utility function to help create a rectangle texture
 *
 * Authored By Neil Roberts  <neil@linux.intel.com>
 *
 * Copyright (C) 2011, 2012 Intel Corporation
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

#include <config.h>

#include <clutter/clutter.h>
#include "meta-texture-rectangle.h"

static void
texture_rectangle_check_cb (CoglTexture *sub_texture,
                            const float *sub_texture_coords,
                            const float *meta_coords,
                            void *user_data)
{
  gboolean *result = user_data;

  if (cogl_is_texture_rectangle (sub_texture))
    *result = TRUE;
}

/* Determines if the given texture is using a rectangle texture as its
 * primitive texture type. Eventually this function could be replaced
 * with cogl_texture_get_type if Cogl makes that public.
 *
 * http://git.gnome.org/browse/cogl/commit/?h=8012eee31
 */
gboolean
meta_texture_rectangle_check (CoglTexture *texture)
{
  gboolean result = FALSE;

  cogl_meta_texture_foreach_in_region (COGL_META_TEXTURE (texture),
                                       0.0f, 0.0f, /* tx_1 / ty_1 */
                                       1.0f, 1.0f, /* tx_2 / ty_2 */
                                       COGL_PIPELINE_WRAP_MODE_REPEAT,
                                       COGL_PIPELINE_WRAP_MODE_REPEAT,
                                       texture_rectangle_check_cb,
                                       &result);

  return result;
}
