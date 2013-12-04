/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010,2013 Intel Corporation.
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

#ifndef __COGL_CLIP_STATE_H__
#define __COGL_CLIP_STATE_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-macros.h>
#include <cogl/cogl-primitive.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-clipping
 * @short_description: Fuctions for manipulating a stack of clipping regions
 *
 * To support clipping your geometry to rectangles or paths Cogl exposes a
 * stack based API whereby each clip region you push onto the stack is
 * intersected with the previous region.
 */

/**
 * cogl_clip_push_window_rectangle:
 * @x_offset: left edge of the clip rectangle in window coordinates
 * @y_offset: top edge of the clip rectangle in window coordinates
 * @width: width of the clip rectangle
 * @height: height of the clip rectangle
 *
 * Specifies a rectangular clipping area for all subsequent drawing
 * operations. Any drawing commands that extend outside the rectangle
 * will be clipped so that only the portion inside the rectangle will
 * be displayed. The rectangle dimensions are not transformed by the
 * current model-view matrix.
 *
 * The rectangle is intersected with the current clip region. To undo
 * the effect of this function, call cogl_clip_pop().
 *
 * Since: 1.2
 * Deprecated: 1.16: Use cogl_framebuffer_push_scissor_clip() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_scissor_clip)
void
cogl_clip_push_window_rectangle (int x_offset,
                                 int y_offset,
                                 int width,
                                 int height);

/**
 * cogl_clip_push_window_rect:
 * @x_offset: left edge of the clip rectangle in window coordinates
 * @y_offset: top edge of the clip rectangle in window coordinates
 * @width: width of the clip rectangle
 * @height: height of the clip rectangle
 *
 * Specifies a rectangular clipping area for all subsequent drawing
 * operations. Any drawing commands that extend outside the rectangle
 * will be clipped so that only the portion inside the rectangle will
 * be displayed. The rectangle dimensions are not transformed by the
 * current model-view matrix.
 *
 * The rectangle is intersected with the current clip region. To undo
 * the effect of this function, call cogl_clip_pop().
 *
 * Deprecated: 1.16: Use cogl_framebuffer_push_scissor_clip() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_scissor_clip)
void
cogl_clip_push_window_rect (float x_offset,
                            float y_offset,
                            float width,
                            float height);

/**
 * cogl_clip_push_rectangle:
 * @x0: x coordinate for top left corner of the clip rectangle
 * @y0: y coordinate for top left corner of the clip rectangle
 * @x1: x coordinate for bottom right corner of the clip rectangle
 * @y1: y coordinate for bottom right corner of the clip rectangle
 *
 * Specifies a rectangular clipping area for all subsequent drawing
 * operations. Any drawing commands that extend outside the rectangle
 * will be clipped so that only the portion inside the rectangle will
 * be displayed. The rectangle dimensions are transformed by the
 * current model-view matrix.
 *
 * The rectangle is intersected with the current clip region. To undo
 * the effect of this function, call cogl_clip_pop().
 *
 * Since: 1.2
 * Deprecated: 1.16: Use cogl_framebuffer_push_rectangle_clip()
 *                   instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_rectangle_clip)
void
cogl_clip_push_rectangle (float x0,
                          float y0,
                          float x1,
                          float y1);

/**
 * cogl_clip_push:
 * @x_offset: left edge of the clip rectangle
 * @y_offset: top edge of the clip rectangle
 * @width: width of the clip rectangle
 * @height: height of the clip rectangle
 *
 * Specifies a rectangular clipping area for all subsequent drawing
 * operations. Any drawing commands that extend outside the rectangle
 * will be clipped so that only the portion inside the rectangle will
 * be displayed. The rectangle dimensions are transformed by the
 * current model-view matrix.
 *
 * The rectangle is intersected with the current clip region. To undo
 * the effect of this function, call cogl_clip_pop().
 *
 * Deprecated: 1.16: The x, y, width, height arguments are inconsistent
 *   with other API that specify rectangles in model space, and when used
 *   with a coordinate space that puts the origin at the center and y+
 *   extending up, it's awkward to use. Please use
 *   cogl_framebuffer_push_rectangle_clip()
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_rectangle_clip)
void
cogl_clip_push (float x_offset,
                float y_offset,
                float width,
                float height);

/**
 * cogl_clip_push_primitive:
 * @primitive: A #CoglPrimitive describing a flat 2D shape
 * @bounds_x1: x coordinate for the top-left corner of the primitives
 *             bounds
 * @bounds_y1: y coordinate for the top-left corner of the primitives
 *             bounds
 * @bounds_x2: x coordinate for the bottom-right corner of the primitives
 *             bounds
 * @bounds_y2: y coordinate for the bottom-right corner of the
 *             primitives bounds.
 *
 * Sets a new clipping area using a 2D shaped described with a
 * #CoglPrimitive. The shape must not contain self overlapping
 * geometry and must lie on a single 2D plane. A bounding box of the
 * 2D shape in local coordinates (the same coordinates used to
 * describe the shape) must be given. It is acceptable for the bounds
 * to be larger than the true bounds but behaviour is undefined if the
 * bounds are smaller than the true bounds.
 *
 * The primitive is transformed by the current model-view matrix and
 * the silhouette is intersected with the previous clipping area.  To
 * restore the previous clipping area, call
 * cogl_clip_pop().
 *
 * Since: 1.10
 * Stability: unstable
 * Deprecated: 1.16: Use cogl_framebuffer_push_primitive_clip()
 *                   instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_primitive_clip)
void
cogl_clip_push_primitive (CoglPrimitive *primitive,
                          float bounds_x1,
                          float bounds_y1,
                          float bounds_x2,
                          float bounds_y2);
/**
 * cogl_clip_pop:
 *
 * Reverts the clipping region to the state before the last call to
 * cogl_clip_push().
 *
 * Deprecated: 1.16: Use cogl_framebuffer_pop_clip() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_pop_clip)
void
cogl_clip_pop (void);

/**
 * cogl_clip_ensure:
 *
 * Ensures that the current clipping region has been set in GL. This
 * will automatically be called before any Cogl primitives but it
 * maybe be neccessary to call if you are using raw GL calls with
 * clipping.
 *
 * Deprecated: 1.2: Calling this function has no effect
 *
 * Since: 1.0
 */
COGL_DEPRECATED
void
cogl_clip_ensure (void);

/**
 * cogl_clip_stack_save:
 *
 * Save the entire state of the clipping stack and then clear all
 * clipping. The previous state can be returned to with
 * cogl_clip_stack_restore(). Each call to cogl_clip_push() after this
 * must be matched by a call to cogl_clip_pop() before calling
 * cogl_clip_stack_restore().
 *
 * Deprecated: 1.2: This was originally added to allow us to save the
 *   clip stack when switching to an offscreen framebuffer, but it's
 *   not necessary anymore given that framebuffers now own separate
 *   clip stacks which will be automatically switched between when a
 *   new buffer is set. Calling this function has no effect
 *
 * Since: 0.8.2
 */
COGL_DEPRECATED
void
cogl_clip_stack_save (void);

/**
 * cogl_clip_stack_restore:
 *
 * Restore the state of the clipping stack that was previously saved
 * by cogl_clip_stack_save().
 *
 * Deprecated: 1.2: This was originally added to allow us to restore
 *   the clip stack when switching back from an offscreen framebuffer,
 *   but it's not necessary anymore given that framebuffers now own
 *   separate clip stacks which will be automatically switched between
 *   when a new buffer is set. Calling this function has no effect
 *
 * Since: 0.8.2
 */
COGL_DEPRECATED
void
cogl_clip_stack_restore (void);

COGL_END_DECLS

#endif /* __COGL_CLIP_STATE_H__ */
