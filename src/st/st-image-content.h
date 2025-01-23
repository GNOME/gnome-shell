/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-image-content.h: A content image with scaling support
 *
 * Copyright 2019 Canonical, Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <clutter/clutter.h>

#define ST_TYPE_IMAGE_CONTENT (st_image_content_get_type ())
G_DECLARE_FINAL_TYPE (StImageContent, st_image_content,
                      ST, IMAGE_CONTENT, GObject)

ClutterContent *st_image_content_new_with_preferred_size (int width,
                                                          int height);

void st_image_content_set_preferred_width (StImageContent *content,
                                           int             width);
int  st_image_content_get_preferred_width (StImageContent *content);

void st_image_content_set_preferred_height (StImageContent *content,
                                            int             height);
int  st_image_content_get_preferred_height (StImageContent *content);

gboolean st_image_content_set_data (StImageContent  *content,
                                    CoglContext     *cogl_context,
                                    const guint8    *data,
                                    CoglPixelFormat  pixel_format,
                                    guint            width,
                                    guint            height,
                                    guint            row_stride,
                                    GError         **error);

gboolean st_image_content_set_bytes (StImageContent  *content,
                                     CoglContext     *cogl_context,
                                     GBytes          *data,
                                     CoglPixelFormat  pixel_format,
                                     guint            width,
                                     guint            height,
                                     guint            row_stride,
                                     GError         **error);

CoglTexture * st_image_content_get_texture (StImageContent *content);
