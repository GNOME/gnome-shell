/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2014 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_FRAMEBUFFER_DEPRECATED_H__
#define __COGL_FRAMEBUFFER_DEPRECATED_H__

#include <cogl/cogl-macros.h>

/* XXX: Since this api was marked unstable, maybe we can just
 * remove this api if we can't find anyone is using it. */
/**
 * cogl_framebuffer_get_color_format:
 * @framebuffer: A #CoglFramebuffer framebuffer
 *
 * Queries the common #CoglPixelFormat of all color buffers attached
 * to this framebuffer. For an offscreen framebuffer created with
 * cogl_offscreen_new_with_texture() this will correspond to the format
 * of the texture.
 *
 * This API is deprecated because it is missleading to report a
 * #CoglPixelFormat for the internal format of the @framebuffer since
 * #CoglPixelFormat is such a precise format description and it's
 * only the set of components and the premultiplied alpha status
 * that is really known.
 *
 * Since: 1.8
 * Stability: unstable
 * Deprecated 1.18: Removed since it is misleading
 */
COGL_DEPRECATED_IN_1_18
CoglPixelFormat
cogl_framebuffer_get_color_format (CoglFramebuffer *framebuffer);

#endif /* __COGL_FRAMEBUFFER_DEPRECATED_H__ */
