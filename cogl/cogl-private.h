/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_PRIVATE_H__
#define __COGL_PRIVATE_H__

G_BEGIN_DECLS

gboolean
_cogl_gl_update_features (CoglContext *context,
                          GError **error);

gboolean
_cogl_gles_update_features (CoglContext *context,
                            GError **error);

gboolean
_cogl_check_extension (const char *name, const char *ext);

void
_cogl_clear (const CoglColor *color, unsigned long buffers);

void
_cogl_read_pixels_with_rowstride (int x,
                                  int y,
                                  int width,
                                  int height,
                                  CoglReadPixelsFlags source,
                                  CoglPixelFormat format,
                                  guint8 *pixels,
                                  int rowstride);

void
_cogl_init (void);

G_END_DECLS

#endif /* __COGL_PRIVATE_H__ */
