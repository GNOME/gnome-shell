/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_OFFSCREEN_H__
#define __COGL_OFFSCREEN_H__

#include <cogl/cogl-types.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-offscreen
 * @short_description: Fuctions for creating and manipulating offscreen
 *                     frame buffer objects
 *
 * Cogl allows creating and operating on offscreen render targets.
 */

/* Offscreen api */

/**
 * cogl_offscreen_new_to_texture:
 * @handle: A CoglHandle for a Cogl texture
 *
 * This creates an offscreen buffer object using the given texture as the
 * primary color buffer. It doesn't just initialize the contents of the
 * offscreen buffer with the texture; they are tightly bound so that
 * drawing to the offscreen buffer effectivly updates the contents of the
 * given texture. You don't need to destroy the offscreen buffer before
 * you can use the texture again.
 *
 * Note: This does not work with sliced Cogl textures.
 *
 * Returns: a #CoglHandle for the new offscreen buffer or %COGL_INVALID_HANDLE
 *          if it wasn't possible to create the buffer.
 */
CoglHandle      cogl_offscreen_new_to_texture (CoglHandle         handle);

/**
 * cogl_offscreen_ref:
 * @handle: A CoglHandle for an offscreen buffer
 *
 * Increments the reference count on the offscreen buffer.
 *
 * Returns: For convenience it returns the given CoglHandle
 */
CoglHandle      cogl_offscreen_ref            (CoglHandle          handle);

/**
 * cogl_is_offscreen:
 * @handle: A CoglHandle for an offscreen buffer
 *
 * Gets whether the given handle references an existing offscreen buffer
 * object.
 *
 * Returns: %TRUE if the handle references an offscreen buffer,
 *   %FALSE otherwise
 */
gboolean        cogl_is_offscreen             (CoglHandle          handle);

/**
 * cogl_offscreen_unref:
 * @handle: A CoglHandle for an offscreen buffer
 *
 * Decreases the reference count for the offscreen buffer and frees it when
 * the count reaches 0.
 */
void            cogl_offscreen_unref          (CoglHandle          handle);

G_END_DECLS

#endif /* __COGL_OFFSCREEN_H__ */
