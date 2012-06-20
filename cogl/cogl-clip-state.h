/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#ifndef __COGL_CLIP_STATE_H
#define __COGL_CLIP_STATE_H

#include <cogl/cogl-types.h>

G_BEGIN_DECLS

/**
 * cogl_clip_push_from_path:
 *
 * Sets a new clipping area using the current path. The current path
 * is then cleared. The clipping area is intersected with the previous
 * clipping area. To restore the previous clipping area, call
 * cogl_clip_pop().
 *
 * Since: 1.0
 */
void
cogl_clip_push_from_path (void);

G_END_DECLS

#endif /* __COGL_CLIP_STATE_H */
