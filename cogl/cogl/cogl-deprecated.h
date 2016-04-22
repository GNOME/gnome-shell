/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

#ifndef COGL_DEPRECATED_H

#define cogl_color                      cogl_color_REPLACED_BY_cogl_set_source_color
#define cogl_enable_depth_test          cogl_enable_depth_test_RENAMED_TO_cogl_set_depth_test_enabled
#define cogl_enable_backface_culling    cogl_enable_backface_culling_RENAMED_TO_cogl_set_backface_culling_enabled

#define cogl_texture_rectangle          cogl_texture_rectangle_REPLACE_BY_cogl_set_source_texture_AND_cogl_rectangle_with_texture_coords

#define cogl_texture_multiple_rectangles        cogl_texture_multiple_rectangles_REPLACED_BY_cogl_set_source_texture_AND_cogl_rectangles_with_texture_coords

#define cogl_texture_polygon            cogl_texture_polygon_REPLACED_BY_cogl_set_source_texture_AND_cogl_polygon

#endif
