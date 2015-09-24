/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-border-image.h: store information about an image with borders
 *
 * Copyright 2009, 2010 Red Hat, Inc.
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

#ifndef __ST_BORDER_IMAGE_H__
#define __ST_BORDER_IMAGE_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/* A StBorderImage encapsulates an image with specified unscaled borders on each edge.
 */

#define ST_TYPE_BORDER_IMAGE             (st_border_image_get_type ())
G_DECLARE_FINAL_TYPE (StBorderImage, st_border_image, ST, BORDER_IMAGE, GObject)

StBorderImage *st_border_image_new (GFile      *file,
                                    int         border_top,
                                    int         border_right,
                                    int         border_bottom,
                                    int         border_left,
                                    int         scale_factor);

GFile      *st_border_image_get_file     (StBorderImage *image);
void        st_border_image_get_borders  (StBorderImage *image,
                                          int           *border_top,
                                          int           *border_right,
                                          int           *border_bottom,
                                          int           *border_left);

gboolean st_border_image_equal (StBorderImage *image,
                                StBorderImage *other);

G_END_DECLS

#endif /* __ST_BORDER_IMAGE_H__ */
