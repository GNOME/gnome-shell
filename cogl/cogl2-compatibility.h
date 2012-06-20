/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL2_COMPATIBILITY_H__
#define __COGL2_COMPATIBILITY_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl2-path.h>

G_BEGIN_DECLS

#define cogl_clip_push_from_path cogl2_clip_push_from_path
/**
 * cogl_clip_push_from_path:
 * @path: The path to clip with.
 *
 * Sets a new clipping area using the silhouette of the specified,
 * filled @path.  The clipping area is intersected with the previous
 * clipping area. To restore the previous clipping area, call
 * call cogl_clip_pop().
 *
 * Since: 1.8
 * Stability: Unstable
 */
void
cogl_clip_push_from_path (CoglPath *path);

G_END_DECLS

#endif /* __COGL2_COMPATIBILITY_H__ */

