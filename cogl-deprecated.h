/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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

#ifndef COGL_DEPRECATED_H

#define cogl_color                      cogl_color_REPLACED_BY_cogl_set_source_color
#define cogl_enable_depth_test          cogl_enable_depth_test_RENAMED_TO_cogl_set_depth_test_enabled
#define cogl_enable_backface_culling    cogl_enable_backface_culling_RENAMED_TO_cogl_set_backface_culling_enabled

#define cogl_texture_rectangle          cogl_texture_rectangle_REPLACE_BY_cogl_set_source_texture_AND_cogl_rectangle_with_texture_coords

#define cogl_texture_multiple_rectangles        cogl_texture_multiple_rectangles_REPLACED_BY_cogl_set_source_texture_AND_cogl_rectangles_with_texture_coords

#define cogl_texture_polygon            cogl_texture_polygon_REPLACED_BY_cogl_set_source_texture_AND_cogl_polygon

#endif
