/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_FRAMEBUFFER_H
#define __COGL_FRAMEBUFFER_H

#ifdef COGL_HAS_WIN32_SUPPORT
#include <windows.h>
#endif /* COGL_HAS_WIN32_SUPPORT */

/* We forward declare the CoglFramebuffer type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _CoglFramebuffer CoglFramebuffer;

#ifdef COGL_ENABLE_EXPERIMENTAL_2_0_API
#include <cogl/cogl2-path.h>
#else
#include <cogl/cogl-path.h>
#endif

#include <cogl/cogl-pipeline.h>
#include <cogl/cogl-indices.h>
#include <cogl/cogl-bitmap.h>
#ifdef COGL_ENABLE_EXPERIMENTAL_API
#include <cogl/cogl-quaternion.h>
#include <cogl/cogl-euler.h>
#include <cogl/cogl-texture.h>
#endif

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-framebuffer
 * @short_description: A common interface for manipulating framebuffers
 *
 * Framebuffers are a collection of buffers that can be rendered too.
 * A framebuffer may be comprised of one or more color buffers, an
 * optional depth buffer and an optional stencil buffer. Other
 * configuration parameters are associated with framebuffers too such
 * as whether the framebuffer supports multi-sampling (an anti-aliasing
 * technique) or dithering.
 *
 * There are two kinds of framebuffer in Cogl, #CoglOnscreen
 * framebuffers and #CoglOffscreen framebuffers. As the names imply
 * offscreen framebuffers are for rendering something offscreen
 * (perhaps to a texture which is bound as one of the color buffers).
 * The exact semantics of onscreen framebuffers depends on the window
 * system backend that you are using, but typically you can expect
 * rendering to a #CoglOnscreen framebuffer will be immediately
 * visible to the user.
 *
 * If you want to create a new framebuffer then you should start by
 * looking at the #CoglOnscreen and #CoglOffscreen constructor
 * functions, such as cogl_offscreen_new_to_texture() or
 * cogl_onscreen_new(). The #CoglFramebuffer interface deals with
 * all aspects that are common between those two types of framebuffer.
 *
 * Setup of a new CoglFramebuffer happens in two stages. There is a
 * configuration stage where you specify all the options and ancillary
 * buffers you want associated with your framebuffer and then when you
 * are happy with the configuration you can "allocate" the framebuffer
 * using cogl_framebuffer_allocate(). Technically explicitly calling
 * cogl_framebuffer_allocate() is optional for convenience and the
 * framebuffer will automatically be allocated when you first try to
 * draw to it, but if you do the allocation manually then you can
 * also catch any possible errors that may arise from your
 * configuration.
 */

#ifdef COGL_ENABLE_EXPERIMENTAL_API

#define COGL_FRAMEBUFFER(X) ((CoglFramebuffer *)(X))

/**
 * cogl_framebuffer_allocate:
 * @framebuffer: A #CoglFramebuffer
 * @error: A pointer to a #CoglError for returning exceptions.
 *
 * Explicitly allocates a configured #CoglFramebuffer allowing developers to
 * check and handle any errors that might arise from an unsupported
 * configuration so that fallback configurations may be tried.
 *
 * <note>Many applications don't support any fallback options at least when
 * they are initially developed and in that case the don't need to use this API
 * since Cogl will automatically allocate a framebuffer when it first gets
 * used.  The disadvantage of relying on automatic allocation is that the
 * program will abort with an error message if there is an error during
 * automatic allocation.</note>
 *
 * Return value: %TRUE if there were no error allocating the framebuffer, else %FALSE.
 * Since: 1.8
 * Stability: unstable
 */
CoglBool
cogl_framebuffer_allocate (CoglFramebuffer *framebuffer,
                           CoglError **error);

/**
 * cogl_framebuffer_get_width:
 * @framebuffer: A #CoglFramebuffer
 *
 * Queries the current width of the given @framebuffer.
 *
 * Return value: The width of @framebuffer.
 * Since: 1.8
 * Stability: unstable
 */
int
cogl_framebuffer_get_width (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_height:
 * @framebuffer: A #CoglFramebuffer
 *
 * Queries the current height of the given @framebuffer.
 *
 * Return value: The height of @framebuffer.
 * Since: 1.8
 * Stability: unstable
 */
int
cogl_framebuffer_get_height (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_set_viewport:
 * @framebuffer: A #CoglFramebuffer
 * @x: The top-left x coordinate of the viewport origin (only integers
 *     supported currently)
 * @y: The top-left y coordinate of the viewport origin (only integers
 *     supported currently)
 * @width: The width of the viewport (only integers supported currently)
 * @height: The height of the viewport (only integers supported currently)
 *
 * Defines a scale and offset for everything rendered relative to the
 * top-left of the destination framebuffer.
 *
 * By default the viewport has an origin of (0,0) and width and height
 * that match the framebuffer's size. Assuming a default projection and
 * modelview matrix then you could translate the contents of a window
 * down and right by leaving the viewport size unchanged by moving the
 * offset to (10,10). The viewport coordinates are measured in pixels.
 * If you left the x and y origin as (0,0) you could scale the windows
 * contents down by specify and width and height that's half the real
 * size of the framebuffer.
 *
 * <note>Although the function takes floating point arguments, existing
 * drivers only allow the use of integer values. In the future floating
 * point values will be exposed via a checkable feature.</note>
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_set_viewport (CoglFramebuffer *framebuffer,
                               float x,
                               float y,
                               float width,
                               float height);

/**
 * cogl_framebuffer_get_viewport_x:
 * @framebuffer: A #CoglFramebuffer
 *
 * Queries the x coordinate of the viewport origin as set using cogl_framebuffer_set_viewport()
 * or the default value which is 0.
 *
 * Return value: The x coordinate of the viewport origin.
 * Since: 1.8
 * Stability: unstable
 */
float
cogl_framebuffer_get_viewport_x (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_viewport_y:
 * @framebuffer: A #CoglFramebuffer
 *
 * Queries the y coordinate of the viewport origin as set using cogl_framebuffer_set_viewport()
 * or the default value which is 0.
 *
 * Return value: The y coordinate of the viewport origin.
 * Since: 1.8
 * Stability: unstable
 */
float
cogl_framebuffer_get_viewport_y (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_viewport_width:
 * @framebuffer: A #CoglFramebuffer
 *
 * Queries the width of the viewport as set using cogl_framebuffer_set_viewport()
 * or the default value which is the width of the framebuffer.
 *
 * Return value: The width of the viewport.
 * Since: 1.8
 * Stability: unstable
 */
float
cogl_framebuffer_get_viewport_width (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_viewport_height:
 * @framebuffer: A #CoglFramebuffer
 *
 * Queries the height of the viewport as set using cogl_framebuffer_set_viewport()
 * or the default value which is the height of the framebuffer.
 *
 * Return value: The height of the viewport.
 * Since: 1.8
 * Stability: unstable
 */
float
cogl_framebuffer_get_viewport_height (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_viewport4fv:
 * @framebuffer: A #CoglFramebuffer
 * @viewport: (out caller-allocates) (array fixed-size=4): A pointer to an
 *            array of 4 floats to receive the (x, y, width, height)
 *            components of the current viewport.
 *
 * Queries the x, y, width and height components of the current viewport as set
 * using cogl_framebuffer_set_viewport() or the default values which are 0, 0,
 * framebuffer_width and framebuffer_height.  The values are written into the
 * given @viewport array.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_get_viewport4fv (CoglFramebuffer *framebuffer,
                                  float *viewport);

/**
 * cogl_framebuffer_push_matrix:
 * @framebuffer: A #CoglFramebuffer pointer
 *
 * Copies the current model-view matrix onto the matrix stack. The matrix
 * can later be restored with cogl_framebuffer_pop_matrix().
 *
 * Since: 1.10
 */
void
cogl_framebuffer_push_matrix (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_pop_matrix:
 * @framebuffer: A #CoglFramebuffer pointer
 *
 * Restores the model-view matrix on the top of the matrix stack.
 *
 * Since: 1.10
 */
void
cogl_framebuffer_pop_matrix (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_identity_matrix:
 * @framebuffer: A #CoglFramebuffer pointer
 *
 * Resets the current model-view matrix to the identity matrix.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_identity_matrix (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_scale:
 * @framebuffer: A #CoglFramebuffer pointer
 * @x: Amount to scale along the x-axis
 * @y: Amount to scale along the y-axis
 * @z: Amount to scale along the z-axis
 *
 * Multiplies the current model-view matrix by one that scales the x,
 * y and z axes by the given values.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_scale (CoglFramebuffer *framebuffer,
                        float x,
                        float y,
                        float z);

/**
 * cogl_framebuffer_translate:
 * @framebuffer: A #CoglFramebuffer pointer
 * @x: Distance to translate along the x-axis
 * @y: Distance to translate along the y-axis
 * @z: Distance to translate along the z-axis
 *
 * Multiplies the current model-view matrix by one that translates the
 * model along all three axes according to the given values.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_translate (CoglFramebuffer *framebuffer,
                            float x,
                            float y,
                            float z);

/**
 * cogl_framebuffer_rotate:
 * @framebuffer: A #CoglFramebuffer pointer
 * @angle: Angle in degrees to rotate.
 * @x: X-component of vertex to rotate around.
 * @y: Y-component of vertex to rotate around.
 * @z: Z-component of vertex to rotate around.
 *
 * Multiplies the current model-view matrix by one that rotates the
 * model around the axis-vector specified by @x, @y and @z. The
 * rotation follows the right-hand thumb rule so for example rotating
 * by 10 degrees about the axis-vector (0, 0, 1) causes a small
 * counter-clockwise rotation.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_rotate (CoglFramebuffer *framebuffer,
                         float angle,
                         float x,
                         float y,
                         float z);

#ifdef COGL_ENABLE_EXPERIMENTAL_API

/**
 * cogl_framebuffer_rotate_quaternion:
 * @framebuffer: A #CoglFramebuffer pointer
 * @quaternion: A #CoglQuaternion
 *
 * Multiplies the current model-view matrix by one that rotates
 * according to the rotation described by @quaternion.
 *
 * Since: 2.0
 * Stability: unstable
 */
void
cogl_framebuffer_rotate_quaternion (CoglFramebuffer *framebuffer,
                                    const CoglQuaternion *quaternion);

/**
 * cogl_framebuffer_rotate_euler:
 * @framebuffer: A #CoglFramebuffer pointer
 * @euler: A #CoglEuler
 *
 * Multiplies the current model-view matrix by one that rotates
 * according to the rotation described by @euler.
 *
 * Since: 2.0
 * Stability: unstable
 */
void
cogl_framebuffer_rotate_euler (CoglFramebuffer *framebuffer,
                               const CoglEuler *euler);

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

/**
 * cogl_framebuffer_transform:
 * @framebuffer: A #CoglFramebuffer pointer
 * @matrix: the matrix to multiply with the current model-view
 *
 * Multiplies the current model-view matrix by the given matrix.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_transform (CoglFramebuffer *framebuffer,
                            const CoglMatrix *matrix);

/**
 * cogl_framebuffer_get_modelview_matrix:
 * @framebuffer: A #CoglFramebuffer pointer
 * @matrix: (out): return location for the model-view matrix
 *
 * Stores the current model-view matrix in @matrix.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_get_modelview_matrix (CoglFramebuffer *framebuffer,
                                       CoglMatrix *matrix);

/**
 * cogl_framebuffer_set_modelview_matrix:
 * @framebuffer: A #CoglFramebuffer pointer
 * @matrix: the new model-view matrix
 *
 * Sets @matrix as the new model-view matrix.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_set_modelview_matrix (CoglFramebuffer *framebuffer,
                                       const CoglMatrix *matrix);

/**
 * cogl_framebuffer_perspective:
 * @framebuffer: A #CoglFramebuffer pointer
 * @fov_y: Vertical field of view angle in degrees.
 * @aspect: The (width over height) aspect ratio for display
 * @z_near: The distance to the near clipping plane (Must be positive,
 *   and must not be 0)
 * @z_far: The distance to the far clipping plane (Must be positive)
 *
 * Replaces the current projection matrix with a perspective matrix
 * based on the provided values.
 *
 * <note>You should be careful not to have to great a @z_far / @z_near
 * ratio since that will reduce the effectiveness of depth testing
 * since there wont be enough precision to identify the depth of
 * objects near to each other.</note>
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_perspective (CoglFramebuffer *framebuffer,
                              float fov_y,
                              float aspect,
                              float z_near,
                              float z_far);

/**
 * cogl_framebuffer_frustum:
 * @framebuffer: A #CoglFramebuffer pointer
 * @left: X position of the left clipping plane where it
 *   intersects the near clipping plane
 * @right: X position of the right clipping plane where it
 *   intersects the near clipping plane
 * @bottom: Y position of the bottom clipping plane where it
 *   intersects the near clipping plane
 * @top: Y position of the top clipping plane where it intersects
 *   the near clipping plane
 * @z_near: The distance to the near clipping plane (Must be positive)
 * @z_far: The distance to the far clipping plane (Must be positive)
 *
 * Replaces the current projection matrix with a perspective matrix
 * for a given viewing frustum defined by 4 side clip planes that
 * all cross through the origin and 2 near and far clip planes.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_frustum (CoglFramebuffer *framebuffer,
                          float left,
                          float right,
                          float bottom,
                          float top,
                          float z_near,
                          float z_far);

/**
 * cogl_framebuffer_orthographic:
 * @framebuffer: A #CoglFramebuffer pointer
 * @x_1: The x coordinate for the first vertical clipping plane
 * @y_1: The y coordinate for the first horizontal clipping plane
 * @x_2: The x coordinate for the second vertical clipping plane
 * @y_2: The y coordinate for the second horizontal clipping plane
 * @near: The <emphasis>distance</emphasis> to the near clipping
 *   plane (will be <emphasis>negative</emphasis> if the plane is
 *   behind the viewer)
 * @far: The <emphasis>distance</emphasis> to the far clipping
 *   plane (will be <emphasis>negative</emphasis> if the plane is
 *   behind the viewer)
 *
 * Replaces the current projection matrix with an orthographic projection
 * matrix.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_orthographic (CoglFramebuffer *framebuffer,
                               float x_1,
                               float y_1,
                               float x_2,
                               float y_2,
                               float near,
                               float far);

/**
 * cogl_framebuffer_get_projection_matrix:
 * @framebuffer: A #CoglFramebuffer pointer
 * @matrix: (out): return location for the projection matrix
 *
 * Stores the current projection matrix in @matrix.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_get_projection_matrix (CoglFramebuffer *framebuffer,
                                        CoglMatrix *matrix);

/**
 * cogl_framebuffer_set_projection_matrix:
 * @framebuffer: A #CoglFramebuffer pointer
 * @matrix: the new projection matrix
 *
 * Sets @matrix as the new projection matrix.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_set_projection_matrix (CoglFramebuffer *framebuffer,
                                        const CoglMatrix *matrix);

/**
 * cogl_framebuffer_push_scissor_clip:
 * @framebuffer: A #CoglFramebuffer pointer
 * @x: left edge of the clip rectangle in window coordinates
 * @y: top edge of the clip rectangle in window coordinates
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
 * the effect of this function, call cogl_framebuffer_pop_clip().
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_push_scissor_clip (CoglFramebuffer *framebuffer,
                                    int x,
                                    int y,
                                    int width,
                                    int height);

/**
 * cogl_framebuffer_push_rectangle_clip:
 * @framebuffer: A #CoglFramebuffer pointer
 * @x_1: x coordinate for top left corner of the clip rectangle
 * @y_1: y coordinate for top left corner of the clip rectangle
 * @x_2: x coordinate for bottom right corner of the clip rectangle
 * @y_2: y coordinate for bottom right corner of the clip rectangle
 *
 * Specifies a modelview transformed rectangular clipping area for all
 * subsequent drawing operations. Any drawing commands that extend
 * outside the rectangle will be clipped so that only the portion
 * inside the rectangle will be displayed. The rectangle dimensions
 * are transformed by the current model-view matrix.
 *
 * The rectangle is intersected with the current clip region. To undo
 * the effect of this function, call cogl_framebuffer_pop_clip().
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_push_rectangle_clip (CoglFramebuffer *framebuffer,
                                      float x_1,
                                      float y_1,
                                      float x_2,
                                      float y_2);

/**
 * cogl_framebuffer_push_path_clip:
 * @framebuffer: A #CoglFramebuffer pointer
 * @path: The path to clip with.
 *
 * Sets a new clipping area using the silhouette of the specified,
 * filled @path.  The clipping area is intersected with the previous
 * clipping area. To restore the previous clipping area, call
 * cogl_framebuffer_pop_clip().
 *
 * Since: 1.0
 * Stability: unstable
 */
void
cogl_framebuffer_push_path_clip (CoglFramebuffer *framebuffer,
                                 CoglPath *path);

/**
 * cogl_framebuffer_push_primitive_clip:
 * @framebuffer: A #CoglFramebuffer pointer
 * @primitive: A #CoglPrimitive describing a flat 2D shape
 * @bounds_x1: x coordinate for the top-left corner of the primitives
 *             bounds
 * @bounds_y1: y coordinate for the top-left corner of the primitives
 *             bounds
 * @bounds_x2: x coordinate for the bottom-right corner of the
 *             primitives bounds.
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
 * cogl_framebuffer_pop_clip().
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_push_primitive_clip (CoglFramebuffer *framebuffer,
                                      CoglPrimitive *primitive,
                                      float bounds_x1,
                                      float bounds_y1,
                                      float bounds_x2,
                                      float bounds_y2);

/**
 * cogl_framebuffer_pop_clip:
 * @framebuffer: A #CoglFramebuffer pointer
 *
 * Reverts the clipping region to the state before the last call to
 * cogl_framebuffer_push_scissor_clip(), cogl_framebuffer_push_rectangle_clip()
 * cogl_framebuffer_push_path_clip(), or cogl_framebuffer_push_primitive_clip().
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_pop_clip (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_red_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of red bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 1.8
 * Stability: unstable
 */
int
cogl_framebuffer_get_red_bits (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_green_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of green bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 1.8
 * Stability: unstable
 */
int
cogl_framebuffer_get_green_bits (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_blue_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of blue bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 1.8
 * Stability: unstable
 */
int
cogl_framebuffer_get_blue_bits (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_alpha_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of alpha bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 1.8
 * Stability: unstable
 */
int
cogl_framebuffer_get_alpha_bits (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_depth_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of depth bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 2.0
 * Stability: unstable
 */
int
cogl_framebuffer_get_depth_bits (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_dither_enabled:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Returns whether dithering has been requested for the given @framebuffer.
 * See cogl_framebuffer_set_dither_enabled() for more details about dithering.
 *
 * <note>This may return %TRUE even when the underlying @framebuffer
 * display pipeline does not support dithering. This value only represents
 * the user's request for dithering.</note>
 *
 * Return value: %TRUE if dithering has been requested or %FALSE if not.
 * Since: 1.8
 * Stability: unstable
 */
CoglBool
cogl_framebuffer_get_dither_enabled (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_set_dither_enabled:
 * @framebuffer: a pointer to a #CoglFramebuffer
 * @dither_enabled: %TRUE to enable dithering or %FALSE to disable
 *
 * Enables or disabled dithering if supported by the hardware.
 *
 * Dithering is a hardware dependent technique to increase the visible
 * color resolution beyond what the underlying hardware supports by playing
 * tricks with the colors placed into the framebuffer to give the illusion
 * of other colors. (For example this can be compared to half-toning used
 * by some news papers to show varying levels of grey even though their may
 * only be black and white are available).
 *
 * If the current display pipeline for @framebuffer does not support dithering
 * then this has no affect.
 *
 * Dithering is enabled by default.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_set_dither_enabled (CoglFramebuffer *framebuffer,
                                     CoglBool dither_enabled);

/**
 * cogl_framebuffer_get_color_mask:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Gets the current #CoglColorMask of which channels would be written to the
 * current framebuffer. Each bit set in the mask means that the
 * corresponding color would be written.
 *
 * Returns: A #CoglColorMask
 * Since: 1.8
 * Stability: unstable
 */
CoglColorMask
cogl_framebuffer_get_color_mask (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_set_color_mask:
 * @framebuffer: a pointer to a #CoglFramebuffer
 * @color_mask: A #CoglColorMask of which color channels to write to
 *              the current framebuffer.
 *
 * Defines a bit mask of which color channels should be written to the
 * given @framebuffer. If a bit is set in @color_mask that means that
 * color will be written.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_set_color_mask (CoglFramebuffer *framebuffer,
                                 CoglColorMask color_mask);

/**
 * cogl_framebuffer_get_color_format:
 * @framebuffer: A #CoglFramebuffer framebuffer
 *
 * Queries the common #CoglPixelFormat of all color buffers attached
 * to this framebuffer. For an offscreen framebuffer created with
 * cogl_offscreen_new_to_texture() this will correspond to the format
 * of the texture.
 *
 * Since: 1.8
 * Stability: unstable
 */
CoglPixelFormat
cogl_framebuffer_get_color_format (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_set_depth_texture_enabled:
 * @framebuffer: A #CoglFramebuffer
 * @enabled: TRUE or FALSE
 *
 * If @enabled is #TRUE, the depth buffer used when rendering to @framebuffer
 * is available as a texture. You can retrieve the texture with
 * cogl_framebuffer_get_depth_texture().
 *
 * <note>It's possible that your GPU does not support depth textures. You
 * should check the %COGL_FEATURE_ID_DEPTH_TEXTURE feature before using this
 * function.</note>
 * <note>It's not valid to call this function after the framebuffer has been
 * allocated as the creation of the depth texture is done at allocation time.
 * </note>
 *
 * Since: 1.14
 * Stability: unstable
 */
void
cogl_framebuffer_set_depth_texture_enabled (CoglFramebuffer *framebuffer,
                                            CoglBool enabled);

/**
 * cogl_framebuffer_get_depth_texture_enabled:
 * @framebuffer: A #CoglFramebuffer
 *
 * Queries whether texture based depth buffer has been enabled via
 * cogl_framebuffer_set_depth_texture_enabled().
 *
 * Return value: %TRUE if a depth texture has been enabled, else
 *               %FALSE.
 *
 * Since: 1.14
 * Stability: unstable
 */
CoglBool
cogl_framebuffer_get_depth_texture_enabled (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_depth_texture:
 * @framebuffer: A #CoglFramebuffer
 *
 * Retrieves the depth buffer of @framebuffer as a #CoglTexture. You need to
 * call cogl_framebuffer_get_depth_texture(fb, TRUE); before using this
 * function.
 *
 * <note>Calling this function implicitely allocates the framebuffer.</note>
 * <note>The texture returned stays valid as long as the framebuffer stays
 * valid.</note>
 *
 * Returns: (transfer none): the depth texture
 *
 * Since: 1.14
 * Stability: unstable
 */
CoglTexture *
cogl_framebuffer_get_depth_texture (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_set_samples_per_pixel:
 * @framebuffer: A #CoglFramebuffer framebuffer
 * @samples_per_pixel: The minimum number of samples per pixel
 *
 * Requires that when rendering to @framebuffer then @n point samples
 * should be made per pixel which will all contribute to the final
 * resolved color for that pixel. The idea is that the hardware aims
 * to get quality similar to what you would get if you rendered
 * everything twice as big (for 4 samples per pixel) and then scaled
 * that image back down with filtering. It can effectively remove the
 * jagged edges of polygons and should be more efficient than if you
 * were to manually render at a higher resolution and downscale
 * because the hardware is often able to take some shortcuts. For
 * example the GPU may only calculate a single texture sample for all
 * points of a single pixel, and for tile based architectures all the
 * extra sample data (such as depth and stencil samples) may be
 * handled on-chip and so avoid increased demand on system memory
 * bandwidth.
 *
 * By default this value is usually set to 0 and that is referred to
 * as "single-sample" rendering. A value of 1 or greater is referred
 * to as "multisample" rendering.
 *
 * <note>There are some semantic differences between single-sample
 * rendering and multisampling with just 1 point sample such as it
 * being redundant to use the cogl_framebuffer_resolve_samples() and
 * cogl_framebuffer_resolve_samples_region() apis with single-sample
 * rendering.</note>
 *
 * <note>It's recommended that
 * cogl_framebuffer_resolve_samples_region() be explicitly used at the
 * end of rendering to a point sample buffer to minimize the number of
 * samples that get resolved. By default Cogl will implicitly resolve
 * all framebuffer samples but if only a small region of a
 * framebuffer has changed this can lead to redundant work being
 * done.</note>
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_set_samples_per_pixel (CoglFramebuffer *framebuffer,
                                        int samples_per_pixel);

/**
 * cogl_framebuffer_get_samples_per_pixel:
 * @framebuffer: A #CoglFramebuffer framebuffer
 *
 * Gets the number of points that are sampled per-pixel when
 * rasterizing geometry. Usually by default this will return 0 which
 * means that single-sample not multisample rendering has been chosen.
 * When using a GPU supporting multisample rendering it's possible to
 * increase the number of samples per pixel using
 * cogl_framebuffer_set_samples_per_pixel().
 *
 * Calling cogl_framebuffer_get_samples_per_pixel() before the
 * framebuffer has been allocated will simply return the value set
 * using cogl_framebuffer_set_samples_per_pixel(). After the
 * framebuffer has been allocated the value will reflect the actual
 * number of samples that will be made by the GPU.
 *
 * Returns: The number of point samples made per pixel when
 *          rasterizing geometry or 0 if single-sample rendering
 *          has been chosen.
 *
 * Since: 1.10
 * Stability: unstable
 */
int
cogl_framebuffer_get_samples_per_pixel (CoglFramebuffer *framebuffer);


/**
 * cogl_framebuffer_resolve_samples:
 * @framebuffer: A #CoglFramebuffer framebuffer
 *
 * When point sample rendering (also known as multisample rendering)
 * has been enabled via cogl_framebuffer_set_samples_per_pixel()
 * then you can optionally call this function (or
 * cogl_framebuffer_resolve_samples_region()) to explicitly resolve
 * the point samples into values for the final color buffer.
 *
 * Some GPUs will implicitly resolve the point samples during
 * rendering and so this function is effectively a nop, but with other
 * architectures it is desirable to defer the resolve step until the
 * end of the frame.
 *
 * Since Cogl will automatically ensure samples are resolved if the
 * target color buffer is used as a source this API only needs to be
 * used if explicit control is desired - perhaps because you want to
 * ensure that the resolve is completed in advance to avoid later
 * having to wait for the resolve to complete.
 *
 * If you are performing incremental updates to a framebuffer you
 * should consider using cogl_framebuffer_resolve_samples_region()
 * instead to avoid resolving redundant pixels.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_resolve_samples (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_resolve_samples_region:
 * @framebuffer: A #CoglFramebuffer framebuffer
 * @x: top-left x coordinate of region to resolve
 * @y: top-left y coordinate of region to resolve
 * @width: width of region to resolve
 * @height: height of region to resolve
 *
 * When point sample rendering (also known as multisample rendering)
 * has been enabled via cogl_framebuffer_set_samples_per_pixel()
 * then you can optionally call this function (or
 * cogl_framebuffer_resolve_samples()) to explicitly resolve the point
 * samples into values for the final color buffer.
 *
 * Some GPUs will implicitly resolve the point samples during
 * rendering and so this function is effectively a nop, but with other
 * architectures it is desirable to defer the resolve step until the
 * end of the frame.
 *
 * Use of this API is recommended if incremental, small updates to
 * a framebuffer are being made because by default Cogl will
 * implicitly resolve all the point samples of the framebuffer which
 * can result in redundant work if only a small number of samples have
 * changed.
 *
 * Because some GPUs implicitly resolve point samples this function
 * only guarantees that at-least the region specified will be resolved
 * and if you have rendered to a larger region then it's possible that
 * other samples may be implicitly resolved.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_resolve_samples_region (CoglFramebuffer *framebuffer,
                                         int x,
                                         int y,
                                         int width,
                                         int height);

/**
 * cogl_framebuffer_get_context:
 * @framebuffer: A #CoglFramebuffer
 *
 * Can be used to query the #CoglContext a given @framebuffer was
 * instantiated within. This is the #CoglContext that was passed to
 * cogl_onscreen_new() for example.
 *
 * Return value: The #CoglContext that the given @framebuffer was
 *               instantiated within.
 * Since: 1.8
 * Stability: unstable
 */
CoglContext *
cogl_framebuffer_get_context (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_clear:
 * @framebuffer: A #CoglFramebuffer
 * @buffers: A mask of #CoglBufferBit<!-- -->'s identifying which auxiliary
 *   buffers to clear
 * @color: The color to clear the color buffer too if specified in
 *         @buffers.
 *
 * Clears all the auxiliary buffers identified in the @buffers mask, and if
 * that includes the color buffer then the specified @color is used.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_clear (CoglFramebuffer *framebuffer,
                        unsigned long buffers,
                        const CoglColor *color);

/**
 * cogl_framebuffer_clear4f:
 * @framebuffer: A #CoglFramebuffer
 * @buffers: A mask of #CoglBufferBit<!-- -->'s identifying which auxiliary
 *   buffers to clear
 * @red: The red component of color to clear the color buffer too if
 *       specified in @buffers.
 * @green: The green component of color to clear the color buffer too if
 *         specified in @buffers.
 * @blue: The blue component of color to clear the color buffer too if
 *        specified in @buffers.
 * @alpha: The alpha component of color to clear the color buffer too if
 *         specified in @buffers.
 *
 * Clears all the auxiliary buffers identified in the @buffers mask, and if
 * that includes the color buffer then the specified @color is used.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_clear4f (CoglFramebuffer *framebuffer,
                          unsigned long buffers,
                          float red,
                          float green,
                          float blue,
                          float alpha);

/**
 * cogl_framebuffer_draw_primitive:
 * @framebuffer: A destination #CoglFramebuffer
 * @pipeline: A #CoglPipeline state object
 * @primitive: A #CoglPrimitive geometry object
 *
 * Draws the given @primitive geometry to the specified destination
 * @framebuffer using the graphics processing state described by @pipeline.
 *
 * This drawing api doesn't support high-level meta texture types such
 * as #CoglTexture2DSliced so it is the user's responsibility to
 * ensure that only low-level textures that can be directly sampled by
 * a GPU such as #CoglTexture2D, #CoglTextureRectangle or #CoglTexture3D
 * are associated with layers of the given @pipeline.
 *
 * <note>This api doesn't support any of the legacy global state options such
 * as cogl_set_depth_test_enabled(), cogl_set_backface_culling_enabled() or
 * cogl_program_use()</note>
 *
 * Stability: unstable
 * Since: 1.10
 */
void
cogl_framebuffer_draw_primitive (CoglFramebuffer *framebuffer,
                                 CoglPipeline *pipeline,
                                 CoglPrimitive *primitive);

/**
 * cogl_framebuffer_draw_rectangle:
 * @framebuffer: A destination #CoglFramebuffer
 * @pipeline: A #CoglPipeline state object
 * @x_1: X coordinate of the top-left corner
 * @y_1: Y coordinate of the top-left corner
 * @x_2: X coordinate of the bottom-right corner
 * @y_2: Y coordinate of the bottom-right corner
 *
 * Draws a rectangle to @framebuffer with the given @pipeline state
 * and with the top left corner positioned at (@x_1, @y_1) and the
 * bottom right corner positioned at (@x_2, @y_2).
 *
 * <note>The position is the position before the rectangle has been
 * transformed by the model-view matrix and the projection
 * matrix.</note>
 *
 * <note>If you want to describe a rectangle with a texture mapped on
 * it then you can use
 * cogl_framebuffer_draw_textured_rectangle().</note>
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_draw_rectangle (CoglFramebuffer *framebuffer,
                                 CoglPipeline *pipeline,
                                 float x_1,
                                 float y_1,
                                 float x_2,
                                 float y_2);

/**
 * cogl_framebuffer_draw_textured_rectangle:
 * @framebuffer: A destination #CoglFramebuffer
 * @pipeline: A #CoglPipeline state object
 * @x_1: x coordinate upper left on screen.
 * @y_1: y coordinate upper left on screen.
 * @x_2: x coordinate lower right on screen.
 * @y_2: y coordinate lower right on screen.
 * @s_1: S texture coordinate of the top-left coorner
 * @t_1: T texture coordinate of the top-left coorner
 * @s_2: S texture coordinate of the bottom-right coorner
 * @t_2: T texture coordinate of the bottom-right coorner
 *
 * Draws a textured rectangle to @framebuffer using the given
 * @pipeline state with the top left corner positioned at (@x_1, @y_1)
 * and the bottom right corner positioned at (@x_2, @y_2). The top
 * left corner will have texture coordinates of (@s_1, @t_1) and the
 * bottom right corner will have texture coordinates of (@s_2, @t_2).
 *
 * <note>The position is the position before the rectangle has been
 * transformed by the model-view matrix and the projection
 * matrix.</note>
 *
 * This is a high level drawing api that can handle any kind of
 * #CoglMetaTexture texture such as #CoglTexture2DSliced textures
 * which may internally be comprised of multiple low-level textures.
 * This is unlike low-level drawing apis such as cogl_primitive_draw()
 * which only support low level texture types that are directly
 * supported by GPUs such as #CoglTexture2D.
 *
 * <note>The given texture coordinates will only be used for the first
 * texture layer of the pipeline and if your pipeline has more than
 * one layer then all other layers will have default texture
 * coordinates of @s_1=0.0 @t_1=0.0 @s_2=1.0 @t_2=1.0 </note>
 *
 * The given texture coordinates should always be normalized such that
 * (0, 0) corresponds to the top left and (1, 1) corresponds to the
 * bottom right. To map an entire texture across the rectangle pass
 * in @s_1=0, @t_1=0, @s_2=1, @t_2=1.
 *
 * <note>Even if you have associated a #CoglTextureRectangle texture
 * with one of your @pipeline layers which normally implies working
 * with non-normalized texture coordinates this api should still be
 * passed normalized texture coordinates.</note>
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_draw_textured_rectangle (CoglFramebuffer *framebuffer,
                                          CoglPipeline *pipeline,
                                          float x_1,
                                          float y_1,
                                          float x_2,
                                          float y_2,
                                          float s_1,
                                          float t_1,
                                          float s_2,
                                          float t_2);

/**
 * cogl_framebuffer_draw_multitextured_rectangle:
 * @framebuffer: A destination #CoglFramebuffer
 * @pipeline: A #CoglPipeline state object
 * @x_1: x coordinate upper left on screen.
 * @y_1: y coordinate upper left on screen.
 * @x_2: x coordinate lower right on screen.
 * @y_2: y coordinate lower right on screen.
 * @tex_coords: (in) (array) (transfer none): An array containing groups of
 *   4 float values: [s_1, t_1, s_2, t_2] that are interpreted as two texture
 *   coordinates; one for the top left texel, and one for the bottom right
 *   texel. Each value should be between 0.0 and 1.0, where the coordinate
 *   (0.0, 0.0) represents the top left of the texture, and (1.0, 1.0) the
 *   bottom right.
 * @tex_coords_len: The length of the @tex_coords array. (For one layer
 *   and one group of texture coordinates, this would be 4)
 *
 * Draws a textured rectangle to @framebuffer with the given @pipeline
 * state with the top left corner positioned at (@x_1, @y_1) and the
 * bottom right corner positioned at (@x_2, @y_2). As a pipeline may
 * contain multiple texture layers this interface lets you supply
 * texture coordinates for each layer of the pipeline.
 *
 * <note>The position is the position before the rectangle has been
 * transformed by the model-view matrix and the projection
 * matrix.</note>
 *
 * This is a high level drawing api that can handle any kind of
 * #CoglMetaTexture texture for the first layer such as
 * #CoglTexture2DSliced textures which may internally be comprised of
 * multiple low-level textures.  This is unlike low-level drawing apis
 * such as cogl_primitive_draw() which only support low level texture
 * types that are directly supported by GPUs such as #CoglTexture2D.
 *
 * <note>This api can not currently handle multiple high-level meta
 * texture layers. The first layer may be a high level meta texture
 * such as #CoglTexture2DSliced but all other layers much be low
 * level textures such as #CoglTexture2D and additionally they
 * should be textures that can be sampled using normalized coordinates
 * (so not #CoglTextureRectangle textures).</note>
 *
 * The top left texture coordinate for layer 0 of any pipeline will be
 * (tex_coords[0], tex_coords[1]) and the bottom right coordinate will
 * be (tex_coords[2], tex_coords[3]). The coordinates for layer 1
 * would be (tex_coords[4], tex_coords[5]) (tex_coords[6],
 * tex_coords[7]) and so on...
 *
 * The given texture coordinates should always be normalized such that
 * (0, 0) corresponds to the top left and (1, 1) corresponds to the
 * bottom right. To map an entire texture across the rectangle pass
 * in tex_coords[0]=0, tex_coords[1]=0, tex_coords[2]=1,
 * tex_coords[3]=1.
 *
 * <note>Even if you have associated a #CoglTextureRectangle texture
 * which normally implies working with non-normalized texture
 * coordinates this api should still be passed normalized texture
 * coordinates.</note>
 *
 * The first pair of coordinates are for the first layer (with the
 * smallest layer index) and if you supply less texture coordinates
 * than there are layers in the current source material then default
 * texture coordinates (0.0, 0.0, 1.0, 1.0) are generated.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_draw_multitextured_rectangle (CoglFramebuffer *framebuffer,
                                               CoglPipeline *pipeline,
                                               float x_1,
                                               float y_1,
                                               float x_2,
                                               float y_2,
                                               const float *tex_coords,
                                               int tex_coords_len);

/**
 * cogl_framebuffer_draw_rectangles:
 * @framebuffer: A destination #CoglFramebuffer
 * @pipeline: A #CoglPipeline state object
 * @coordinates: (in) (array) (transfer none): an array of coordinates
 *   containing groups of 4 float values: [x_1, y_1, x_2, y_2] that are
 *   interpreted as two position coordinates; one for the top left of
 *   the rectangle (x1, y1), and one for the bottom right of the
 *   rectangle (x2, y2).
 * @n_rectangles: number of rectangles defined in @coordinates.
 *
 * Draws a series of rectangles to @framebuffer with the given
 * @pipeline state in the same way that
 * cogl_framebuffer_draw_rectangle() does.
 *
 * The top left corner of the first rectangle is positioned at
 * (coordinates[0], coordinates[1]) and the bottom right corner is
 * positioned at (coordinates[2], coordinates[3]). The positions for
 * the second rectangle are (coordinates[4], coordinates[5]) and
 * (coordinates[6], coordinates[7]) and so on...
 *
 * <note>The position is the position before the rectangle has been
 * transformed by the model-view matrix and the projection
 * matrix.</note>
 *
 * As a general rule for better performance its recommended to use
 * this this API instead of calling
 * cogl_framebuffer_draw_textured_rectangle() separately for multiple
 * rectangles if all of the rectangles will be drawn together with the
 * same @pipeline state.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_draw_rectangles (CoglFramebuffer *framebuffer,
                                  CoglPipeline *pipeline,
                                  const float *coordinates,
                                  unsigned int n_rectangles);

/**
 * cogl_framebuffer_draw_textured_rectangles:
 * @framebuffer: A destination #CoglFramebuffer
 * @pipeline: A #CoglPipeline state object
 * @coordinates: (in) (array) (transfer none): an array containing
 *   groups of 8 float values: [x_1, y_1, x_2, y_2, s_1, t_1, s_2, t_2]
 *   that have the same meaning as the arguments for
 *   cogl_framebuffer_draw_textured_rectangle().
 * @n_rectangles: number of rectangles to @coordinates to draw
 *
 * Draws a series of rectangles to @framebuffer with the given
 * @pipeline state in the same way that
 * cogl_framebuffer_draw_textured_rectangle() does.
 *
 * <note>The position is the position before the rectangle has been
 * transformed by the model-view matrix and the projection
 * matrix.</note>
 *
 * This is a high level drawing api that can handle any kind of
 * #CoglMetaTexture texture such as #CoglTexture2DSliced textures
 * which may internally be comprised of multiple low-level textures.
 * This is unlike low-level drawing apis such as cogl_primitive_draw()
 * which only support low level texture types that are directly
 * supported by GPUs such as #CoglTexture2D.
 *
 * The top left corner of the first rectangle is positioned at
 * (coordinates[0], coordinates[1]) and the bottom right corner is
 * positioned at (coordinates[2], coordinates[3]). The top left
 * texture coordinate is (coordinates[4], coordinates[5]) and the
 * bottom right texture coordinate is (coordinates[6],
 * coordinates[7]). The coordinates for subsequent rectangles
 * are defined similarly by the subsequent coordinates.
 *
 * As a general rule for better performance its recommended to use
 * this this API instead of calling
 * cogl_framebuffer_draw_textured_rectangle() separately for multiple
 * rectangles if all of the rectangles will be drawn together with the
 * same @pipeline state.
 *
 * The given texture coordinates should always be normalized such that
 * (0, 0) corresponds to the top left and (1, 1) corresponds to the
 * bottom right. To map an entire texture across the rectangle pass
 * in tex_coords[0]=0, tex_coords[1]=0, tex_coords[2]=1,
 * tex_coords[3]=1.
 *
 * <note>Even if you have associated a #CoglTextureRectangle texture
 * which normally implies working with non-normalized texture
 * coordinates this api should still be passed normalized texture
 * coordinates.</note>
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_framebuffer_draw_textured_rectangles (CoglFramebuffer *framebuffer,
                                           CoglPipeline *pipeline,
                                           const float *coordinates,
                                           unsigned int n_rectangles);

/**
 * cogl_framebuffer_fill_path:
 * @framebuffer: A #CoglFramebuffer
 * @pipeline: A #CoglPipeline to render with
 * @path: The #CoglPath to fill
 *
 * Fills the interior of the path using the fragment operations
 * defined by the pipeline.
 *
 * The interior of the shape is determined using the fill rule of the
 * path. See %CoglPathFillRule for details.
 *
 * <note>The result of referencing sliced textures in your current
 * pipeline when filling a path are undefined. You should pass
 * the %COGL_TEXTURE_NO_SLICING flag when loading any texture you will
 * use while filling a path.</note>
 *
 * Since: 2.0
 */
void
cogl_framebuffer_fill_path (CoglFramebuffer *framebuffer,
                            CoglPipeline *pipeline,
                            CoglPath *path);

/**
 * cogl_framebuffer_stroke_path:
 * @framebuffer: A #CoglFramebuffer
 * @pipeline: A #CoglPipeline to render with
 * @path: The #CoglPath to stroke
 *
 * Strokes the edge of the path using the fragment operations defined
 * by the pipeline. The stroke line will have a width of 1 pixel
 * regardless of the current transformation matrix.
 *
 * Since: 2.0
 */
void
cogl_framebuffer_stroke_path (CoglFramebuffer *framebuffer,
                              CoglPipeline *pipeline,
                              CoglPath *path);

/* XXX: Should we take an n_buffers + buffer id array instead of using
 * the CoglBufferBits type which doesn't seem future proof? */
/**
 * cogl_framebuffer_discard_buffers:
 * @framebuffer: A #CoglFramebuffer
 * @buffers: A #CoglBufferBit mask of which ancillary buffers you want
 *           to discard.
 *
 * Declares that the specified @buffers no longer need to be referenced
 * by any further rendering commands. This can be an important
 * optimization to avoid subsequent frames of rendering depending on
 * the results of a previous frame.
 *
 * For example; some tile-based rendering GPUs are able to avoid allocating and
 * accessing system memory for the depth and stencil buffer so long as these
 * buffers are not required as input for subsequent frames and that can save a
 * significant amount of memory bandwidth used to save and restore their
 * contents to system memory between frames.
 *
 * It is currently considered an error to try and explicitly discard the color
 * buffer by passing %COGL_BUFFER_BIT_COLOR. This is because the color buffer is
 * already implicitly discard when you finish rendering to a #CoglOnscreen
 * framebuffer, and it's not meaningful to try and discard the color buffer of
 * a #CoglOffscreen framebuffer since they are single-buffered.
 *
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_discard_buffers (CoglFramebuffer *framebuffer,
                                  unsigned long buffers);

/**
 * cogl_framebuffer_finish:
 * @framebuffer: A #CoglFramebuffer pointer
 *
 * This blocks the CPU until all pending rendering associated with the
 * specified framebuffer has completed. It's very rare that developers should
 * ever need this level of synchronization with the GPU and should never be
 * used unless you clearly understand why you need to explicitly force
 * synchronization.
 *
 * One example might be for benchmarking purposes to be sure timing
 * measurements reflect the time that the GPU is busy for not just the time it
 * takes to queue rendering commands.
 *
 * Stability: unstable
 * Since: 1.10
 */
void
cogl_framebuffer_finish (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_read_pixels_into_bitmap:
 * @framebuffer: A #CoglFramebuffer
 * @x: The x position to read from
 * @y: The y position to read from
 * @source: Identifies which auxillary buffer you want to read
 *          (only COGL_READ_PIXELS_COLOR_BUFFER supported currently)
 * @bitmap: The bitmap to store the results in.
 *
 * This reads a rectangle of pixels from the given framebuffer where
 * position (0, 0) is the top left. The pixel at (x, y) is the first
 * read, and a rectangle of pixels with the same size as the bitmap is
 * read right and downwards from that point.
 *
 * Currently Cogl assumes that the framebuffer is in a premultiplied
 * format so if the format of @bitmap is non-premultiplied it will
 * convert it. To read the pixel values without any conversion you
 * should either specify a format that doesn't use an alpha channel or
 * use one of the formats ending in PRE.
 *
 * Return value: %TRUE if the read succeeded or %FALSE otherwise. The
 *  function is only likely to fail if the bitmap points to a pixel
 *  buffer and it could not be mapped.
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_framebuffer_read_pixels_into_bitmap (CoglFramebuffer *framebuffer,
                                          int x,
                                          int y,
                                          CoglReadPixelsFlags source,
                                          CoglBitmap *bitmap);

/**
 * cogl_framebuffer_read_pixels:
 * @framebuffer: A #CoglFramebuffer
 * @x: The x position to read from
 * @y: The y position to read from
 * @width: The width of the region of rectangles to read
 * @height: The height of the region of rectangles to read
 * @format: The pixel format to store the data in
 * @pixels: The address of the buffer to store the data in
 *
 * This is a convenience wrapper around
 * cogl_framebuffer_read_pixels_into_bitmap() which allocates a
 * temporary #CoglBitmap to read pixel data directly into the given
 * buffer. The rowstride of the buffer is assumed to be the width of
 * the region times the bytes per pixel of the format. The source for
 * the data is always taken from the color buffer. If you want to use
 * any other rowstride or source, please use the
 * cogl_framebuffer_read_pixels_into_bitmap() function directly.
 *
 * The implementation of the function looks like this:
 *
 * |[
 * bitmap = cogl_bitmap_new_for_data (context,
 *                                    width, height,
 *                                    format,
 *                                    /<!-- -->* rowstride *<!-- -->/
 *                                    bpp * width,
 *                                    pixels);
 * cogl_framebuffer_read_pixels_into_bitmap (framebuffer,
 *                                           x, y,
 *                                           COGL_READ_PIXELS_COLOR_BUFFER,
 *                                           bitmap);
 * cogl_object_unref (bitmap);
 * ]|
 *
 * Return value: %TRUE if the read succeeded or %FALSE otherwise.
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_framebuffer_read_pixels (CoglFramebuffer *framebuffer,
                              int x,
                              int y,
                              int width,
                              int height,
                              CoglPixelFormat format,
                              uint8_t *pixels);

/**
 * cogl_get_draw_framebuffer:
 *
 * Gets the current #CoglFramebuffer as set using
 * cogl_push_framebuffer()
 *
 * Return value: The current #CoglFramebuffer
 * Stability: unstable
 * Since: 1.8
 */
CoglFramebuffer *
cogl_get_draw_framebuffer (void);

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

/* XXX: Note these are defined outside the COGL_ENABLE_EXPERIMENTAL_API guard since
 * otherwise the glib-mkenums stuff will get upset. */

uint32_t
cogl_framebuffer_error_quark (void);

/**
 * COGL_FRAMEBUFFER_ERROR:
 *
 * An error domain for reporting #CoglFramebuffer exceptions
 */
#define COGL_FRAMEBUFFER_ERROR (cogl_framebuffer_error_quark ())

typedef enum { /*< prefix=COGL_FRAMEBUFFER_ERROR >*/
  COGL_FRAMEBUFFER_ERROR_ALLOCATE
} CoglFramebufferError;

/**
 * cogl_is_framebuffer:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a #CoglFramebuffer.
 *
 * Return value: %TRUE if the object references a #CoglFramebuffer
 *   and %FALSE otherwise.
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_is_framebuffer (void *object);

COGL_END_DECLS

#endif /* __COGL_FRAMEBUFFER_H */

