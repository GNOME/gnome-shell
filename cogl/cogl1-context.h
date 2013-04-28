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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_1_CONTEXT_H__
#define __COGL_1_CONTEXT_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-texture.h>
#include <cogl/cogl-framebuffer.h>
#include <cogl/cogl-macros.h>

COGL_BEGIN_DECLS

/**
 * cogl_get_option_group:
 *
 * Retrieves the #GOptionGroup used by Cogl to parse the command
 * line options. Clutter uses this to handle the Cogl command line
 * options during its initialization process.
 *
 * Return value: a #GOptionGroup
 *
 * Since: 1.0
 * Deprecated: 1.16: Not replaced
 */
GOptionGroup *
cogl_get_option_group (void) COGL_DEPRECATED_IN_1_16;

/* Misc */
/**
 * cogl_get_features:
 *
 * Returns all of the features supported by COGL.
 *
 * Return value: A logical OR of all the supported COGL features.
 *
 * Since: 0.8
 * Deprecated: 1.10: Use cogl_foreach_feature() instead
 */
CoglFeatureFlags
cogl_get_features (void)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_foreach_feature);

/**
 * cogl_features_available:
 * @features: A bitmask of features to check for
 *
 * Checks whether the given COGL features are available. Multiple
 * features can be checked for by or-ing them together with the '|'
 * operator. %TRUE is only returned if all of the requested features
 * are available.
 *
 * Return value: %TRUE if the features are available, %FALSE otherwise.
 * Deprecated: 1.10: Use cogl_has_feature() instead
 */
CoglBool
cogl_features_available (CoglFeatureFlags features)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_has_feature);

/**
 * cogl_get_proc_address:
 * @name: the name of the function.
 *
 * Gets a pointer to a given GL or GL ES extension function. This acts
 * as a wrapper around glXGetProcAddress() or whatever is the
 * appropriate function for the current backend.
 *
 * <note>This function should not be used to query core opengl API
 * symbols since eglGetProcAddress for example doesn't allow this and
 * and may return a junk pointer if you do.</note>
 *
 * Return value: a pointer to the requested function or %NULL if the
 *   function is not available.
 */
CoglFuncPtr
cogl_get_proc_address (const char *name);

/**
 * cogl_check_extension:
 * @name: extension to check for
 * @ext: list of extensions
 *
 * Check whether @name occurs in list of extensions in @ext.
 *
 * Return value: %TRUE if the extension occurs in the list, %FALSE otherwise.
 *
 * Deprecated: 1.2: OpenGL is an implementation detail for Cogl and so it's
 *   not appropriate to expose OpenGL extensions through the Cogl API. This
 *   function can be replaced by the following equivalent code:
 * |[
 *   CoglBool retval = (strstr (ext, name) != NULL) ? TRUE : FALSE;
 * ]|
 */
CoglBool
cogl_check_extension (const char *name,
                      const char *ext) COGL_DEPRECATED;

/**
 * cogl_get_bitmasks:
 * @red: (out): Return location for the number of red bits or %NULL
 * @green: (out): Return location for the number of green bits or %NULL
 * @blue: (out): Return location for the number of blue bits or %NULL
 * @alpha: (out): Return location for the number of alpha bits or %NULL
 *
 * Gets the number of bitplanes used for each of the color components
 * in the color buffer. Pass %NULL for any of the arguments if the
 * value is not required.
 *
 * Deprecated: 1.8: Use cogl_framebuffer_get_red/green/blue/alpha_bits()
 *                  instead
 */
void
cogl_get_bitmasks (int *red,
                   int *green,
                   int *blue,
                   int *alpha)
    COGL_DEPRECATED_IN_1_8_FOR (cogl_framebuffer_get_red_OR_green_OR_blue_OR_alpha_bits);

/**
 * cogl_perspective:
 * @fovy: Vertical field of view angle in degrees.
 * @aspect: The (width over height) aspect ratio for display
 * @z_near: The distance to the near clipping plane (Must be positive)
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
 * Deprecated: 1.10: Use cogl_framebuffer_perspective() instead
 */
void
cogl_perspective (float fovy,
                  float aspect,
                  float z_near,
                  float z_far)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_perspective);

/**
 * cogl_frustum:
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
 * Since: 0.8.2
 * Deprecated: 1.10: Use cogl_framebuffer_frustum() instead
 */
void
cogl_frustum (float left,
              float right,
              float bottom,
              float top,
              float z_near,
              float z_far)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_frustum);

/**
 * cogl_ortho:
 * @left: The coordinate for the left clipping plane
 * @right: The coordinate for the right clipping plane
 * @bottom: The coordinate for the bottom clipping plane
 * @top: The coordinate for the top clipping plane
 * @near: The <emphasis>distance</emphasis> to the near clipping
 *   plane (negative if the plane is behind the viewer)
 * @far: The <emphasis>distance</emphasis> for the far clipping
 *   plane (negative if the plane is behind the viewer)
 *
 * Replaces the current projection matrix with an orthographic projection
 * matrix. See <xref linkend="cogl-ortho-matrix"/> to see how the matrix is
 * calculated.
 *
 * <figure id="cogl-ortho-matrix">
 *   <title></title>
 *   <graphic fileref="cogl_ortho.png" format="PNG"/>
 * </figure>
 *
 * <note>This function copies the arguments from OpenGL's glOrtho() even
 * though they are unnecessarily confusing due to the z near and z far
 * arguments actually being a "distance" from the origin, where
 * negative values are behind the viewer, instead of coordinates for
 * the z clipping planes which would have been consistent with the
 * left, right bottom and top arguments.</note>
 *
 * Since: 1.0
 * Deprecated: 1.10: Use cogl_framebuffer_orthographic() instead
 */
void
cogl_ortho (float left,
            float right,
            float bottom,
            float top,
            float near,
            float far)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_orthographic);

/**
 * cogl_viewport:
 * @width: Width of the viewport
 * @height: Height of the viewport
 *
 * Replace the current viewport with the given values.
 *
 * Since: 0.8.2
 * Deprecated: 1.8: Use cogl_framebuffer_set_viewport instead
 */
void
cogl_viewport (unsigned int width,
	       unsigned int height)
     COGL_DEPRECATED_IN_1_8_FOR (cogl_framebuffer_set_viewport);

/**
 * cogl_set_viewport:
 * @x: X offset of the viewport
 * @y: Y offset of the viewport
 * @width: Width of the viewport
 * @height: Height of the viewport
 *
 * Replaces the current viewport with the given values.
 *
 * Since: 1.2
 * Deprecated: 1.8: Use cogl_framebuffer_set_viewport() instead
 */
void
cogl_set_viewport (int x,
                   int y,
                   int width,
                   int height)
     COGL_DEPRECATED_IN_1_8_FOR (cogl_framebuffer_set_viewport);

/**
 * cogl_push_matrix:
 *
 * Stores the current model-view matrix on the matrix stack. The matrix
 * can later be restored with cogl_pop_matrix().
 *
 * Deprecated: 1.10: Use cogl_framebuffer_push_matrix() instead
 */
void
cogl_push_matrix (void)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_push_matrix);

/**
 * cogl_pop_matrix:
 *
 * Restores the current model-view matrix from the matrix stack.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_pop_matrix() instead
 */
void
cogl_pop_matrix (void)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_push_matrix);

/**
 * cogl_scale:
 * @x: Amount to scale along the x-axis
 * @y: Amount to scale along the y-axis
 * @z: Amount to scale along the z-axis
 *
 * Multiplies the current model-view matrix by one that scales the x,
 * y and z axes by the given values.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_pop_matrix() instead
 */
void
cogl_scale (float x,
            float y,
            float z)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_scale);

/**
 * cogl_translate:
 * @x: Distance to translate along the x-axis
 * @y: Distance to translate along the y-axis
 * @z: Distance to translate along the z-axis
 *
 * Multiplies the current model-view matrix by one that translates the
 * model along all three axes according to the given values.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_translate() instead
 */
void
cogl_translate (float x,
                float y,
                float z)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_translate);

/**
 * cogl_rotate:
 * @angle: Angle in degrees to rotate.
 * @x: X-component of vertex to rotate around.
 * @y: Y-component of vertex to rotate around.
 * @z: Z-component of vertex to rotate around.
 *
 * Multiplies the current model-view matrix by one that rotates the
 * model around the vertex specified by @x, @y and @z. The rotation
 * follows the right-hand thumb rule so for example rotating by 10
 * degrees about the vertex (0, 0, 1) causes a small counter-clockwise
 * rotation.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_rotate() instead
 */
void
cogl_rotate (float angle,
             float x,
             float y,
             float z)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_rotate);

/**
 * cogl_transform:
 * @matrix: the matrix to multiply with the current model-view
 *
 * Multiplies the current model-view matrix by the given matrix.
 *
 * Since: 1.4
 * Deprecated: 1.10: Use cogl_framebuffer_transform() instead
 */
void
cogl_transform (const CoglMatrix *matrix)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_transform);

/**
 * cogl_get_modelview_matrix:
 * @matrix: (out): return location for the model-view matrix
 *
 * Stores the current model-view matrix in @matrix.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_get_modelview_matrix()
 *                   instead
 */
void
cogl_get_modelview_matrix (CoglMatrix *matrix)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_get_modelview_matrix);

/**
 * cogl_set_modelview_matrix:
 * @matrix: the new model-view matrix
 *
 * Loads @matrix as the new model-view matrix.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_set_modelview_matrix()
 *                   instead
 */
void
cogl_set_modelview_matrix (CoglMatrix *matrix)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_set_modelview_matrix);

/**
 * cogl_get_projection_matrix:
 * @matrix: (out): return location for the projection matrix
 *
 * Stores the current projection matrix in @matrix.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_get_projection_matrix()
 *                   instead
 */
void
cogl_get_projection_matrix (CoglMatrix *matrix)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_get_projection_matrix);

/**
 * cogl_set_projection_matrix:
 * @matrix: the new projection matrix
 *
 * Loads matrix as the new projection matrix.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_set_projection_matrix()
 *                   instead
 */
void
cogl_set_projection_matrix (CoglMatrix *matrix)
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_set_projection_matrix);

/**
 * cogl_get_viewport:
 * @v: (out) (array fixed-size=4): pointer to a 4 element array
 *   of #float<!-- -->s to receive the viewport dimensions.
 *
 * Stores the current viewport in @v. @v[0] and @v[1] get the x and y
 * position of the viewport and @v[2] and @v[3] get the width and
 * height.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_get_viewport4fv()
 *                   instead
 */
void
cogl_get_viewport (float v[4])
     COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_get_viewport4fv);

/**
 * cogl_set_depth_test_enabled:
 * @setting: %TRUE to enable depth testing or %FALSE to disable.
 *
 * Sets whether depth testing is enabled. If it is disabled then the
 * order that actors are layered on the screen depends solely on the
 * order specified using clutter_actor_raise() and
 * clutter_actor_lower(), otherwise it will also take into account the
 * actor's depth. Depth testing is disabled by default.
 *
 * Deprecated: 1.16: Use cogl_pipeline_set_depth_state() instead
 */
void
cogl_set_depth_test_enabled (CoglBool setting)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_depth_state);

/**
 * cogl_get_depth_test_enabled:
 *
 * Queries if depth testing has been enabled via cogl_set_depth_test_enable()
 *
 * Return value: %TRUE if depth testing is enabled, and %FALSE otherwise
 *
 * Deprecated: 1.16: Use cogl_pipeline_set_depth_state() instead
 */
CoglBool
cogl_get_depth_test_enabled (void)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_depth_state);

/**
 * cogl_set_backface_culling_enabled:
 * @setting: %TRUE to enable backface culling or %FALSE to disable.
 *
 * Sets whether textures positioned so that their backface is showing
 * should be hidden. This can be used to efficiently draw two-sided
 * textures or fully closed cubes without enabling depth testing. This
 * only affects calls to the cogl_rectangle* family of functions and
 * cogl_vertex_buffer_draw*. Backface culling is disabled by default.
 *
 * Deprecated: 1.16: Use cogl_pipeline_set_cull_face_mode() instead
 */
void
cogl_set_backface_culling_enabled (CoglBool setting)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_cull_face_mode);

/**
 * cogl_get_backface_culling_enabled:
 *
 * Queries if backface culling has been enabled via
 * cogl_set_backface_culling_enabled()
 *
 * Return value: %TRUE if backface culling is enabled, and %FALSE otherwise
 *
 * Deprecated: 1.16: Use cogl_pipeline_get_cull_face_mode() instead
 */
CoglBool
cogl_get_backface_culling_enabled (void)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_get_cull_face_mode);

/**
 * cogl_set_fog:
 * @fog_color: The color of the fog
 * @mode: A #CoglFogMode that determines the equation used to calculate the
 *   fogging blend factor.
 * @density: Used by %COGL_FOG_MODE_EXPONENTIAL and by
 *   %COGL_FOG_MODE_EXPONENTIAL_SQUARED equations.
 * @z_near: Position along Z axis where no fogging should be applied
 * @z_far: Position along Z axis where full fogging should be applied
 *
 * Enables fogging. Fogging causes vertices that are further away from the eye
 * to be rendered with a different color. The color is determined according to
 * the chosen fog mode; at it's simplest the color is linearly interpolated so
 * that vertices at @z_near are drawn fully with their original color and
 * vertices at @z_far are drawn fully with @fog_color. Fogging will remain
 * enabled until you call cogl_disable_fog().
 *
 * <note>The fogging functions only work correctly when primitives use
 * unmultiplied alpha colors. By default Cogl will premultiply textures
 * and cogl_set_source_color() will premultiply colors, so unless you
 * explicitly load your textures requesting an unmultiplied internal format
 * and use cogl_material_set_color() you can only use fogging with fully
 * opaque primitives. This might improve in the future when we can depend
 * on fragment shaders.</note>
 *
 * Deprecated: 1.16: Use #CoglSnippet shader api for fog
 */
void
cogl_set_fog (const CoglColor *fog_color,
              CoglFogMode mode,
              float density,
              float z_near,
              float z_far)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_API);

/**
 * cogl_disable_fog:
 *
 * This function disables fogging, so primitives drawn afterwards will not be
 * blended with any previously set fog color.
 *
 * Deprecated: 1.16: Use #CoglSnippet shader api for fog
 */
void
cogl_disable_fog (void)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_API);

/**
 * cogl_clear:
 * @color: Background color to clear to
 * @buffers: A mask of #CoglBufferBit<!-- -->'s identifying which auxiliary
 *   buffers to clear
 *
 * Clears all the auxiliary buffers identified in the @buffers mask, and if
 * that includes the color buffer then the specified @color is used.
 *
 * Deprecated: 1.16: Use cogl_framebuffer_clear() api instead
 */
void
cogl_clear (const CoglColor *color,
            unsigned long buffers)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_clear);

/**
 * cogl_set_source:
 * @material: A #CoglMaterial
 *
 * This function changes the material at the top of the source stack.
 * The material at the top of this stack defines the GPU state used to
 * process subsequent primitives, such as rectangles drawn with
 * cogl_rectangle() or vertices drawn using cogl_vertex_buffer_draw().
 *
 * Since: 1.0
 * Deprecated: 1.16: Latest drawing apis all take an explicit
 *                   #CoglPipeline argument so this stack of
 *                   #CoglMaterial<!-- -->s shouldn't be used.
 */
void
cogl_set_source (void *material) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_get_source:
 *
 * Returns the current source material as previously set using
 * cogl_set_source().
 *
 * <note>You should typically consider the returned material immutable
 * and not try to change any of its properties unless you own a
 * reference to that material. At times you may be able to get a
 * reference to an internally managed materials and the result of
 * modifying such materials is undefined.</note>
 *
 * Return value: The current source material.
 *
 * Since: 1.6
 * Deprecated: 1.16: Latest drawing apis all take an explicit
 *                   #CoglPipeline argument so this stack of
 *                   #CoglMaterial<!-- -->s shouldn't be used.
 */
void *
cogl_get_source (void) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_push_source:
 * @material: A #CoglMaterial
 *
 * Pushes the given @material to the top of the source stack. The
 * material at the top of this stack defines the GPU state used to
 * process later primitives as defined by cogl_set_source().
 *
 * Since: 1.6
 * Deprecated: 1.16: Latest drawing apis all take an explicit
 *                   #CoglPipeline argument so this stack of
 *                   #CoglMaterial<!-- -->s shouldn't be used.
 */
void
cogl_push_source (void *material) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_pop_source:
 *
 * Removes the material at the top of the source stack. The material
 * at the top of this stack defines the GPU state used to process
 * later primitives as defined by cogl_set_source().
 *
 * Since: 1.6
 * Deprecated: 1.16: Latest drawing apis all take an explicit
 *                   #CoglPipeline argument so this stack of
 *                   #CoglMaterial<!-- -->s shouldn't be used.
 */
void
cogl_pop_source (void) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_set_source_color:
 * @color: a #CoglColor
 *
 * This is a convenience function for creating a solid fill source material
 * from the given color. This color will be used for any subsequent drawing
 * operation.
 *
 * The color will be premultiplied by Cogl, so the color should be
 * non-premultiplied. For example: use (1.0, 0.0, 0.0, 0.5) for
 * semi-transparent red.
 *
 * See also cogl_set_source_color4ub() and cogl_set_source_color4f()
 * if you already have the color components.
 *
 * Since: 1.0
 * Deprecated: 1.16: Latest drawing apis all take an explicit
 *                   #CoglPipeline argument so this stack of
 *                   #CoglMaterial<!-- -->s shouldn't be used.
 */
void
cogl_set_source_color (const CoglColor *color) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_set_source_color4ub:
 * @red: value of the red channel, between 0 and 255
 * @green: value of the green channel, between 0 and 255
 * @blue: value of the blue channel, between 0 and 255
 * @alpha: value of the alpha channel, between 0 and 255
 *
 * This is a convenience function for creating a solid fill source material
 * from the given color using unsigned bytes for each component. This
 * color will be used for any subsequent drawing operation.
 *
 * The value for each component is an unsigned byte in the range
 * between 0 and 255.
 *
 * Since: 1.0
 * Deprecated: 1.16: Latest drawing apis all take an explicit
 *                   #CoglPipeline argument so this stack of
 *                   #CoglMaterial<!-- -->s shouldn't be used.
 */
void
cogl_set_source_color4ub (uint8_t red,
                          uint8_t green,
                          uint8_t blue,
                          uint8_t alpha) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_set_source_color4f:
 * @red: value of the red channel, between 0 and %1.0
 * @green: value of the green channel, between 0 and %1.0
 * @blue: value of the blue channel, between 0 and %1.0
 * @alpha: value of the alpha channel, between 0 and %1.0
 *
 * This is a convenience function for creating a solid fill source material
 * from the given color using normalized values for each component. This color
 * will be used for any subsequent drawing operation.
 *
 * The value for each component is a fixed point number in the range
 * between 0 and %1.0. If the values passed in are outside that
 * range, they will be clamped.
 *
 * Since: 1.0
 * Deprecated: 1.16: Latest drawing apis all take an explicit
 *                   #CoglPipeline argument so this stack of
 *                   #CoglMaterial<!-- -->s shouldn't be used.
 */
void
cogl_set_source_color4f (float red,
                         float green,
                         float blue,
                         float alpha) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_set_source_texture:
 * @texture: The #CoglTexture you want as your source
 *
 * This is a convenience function for creating a material with the first
 * layer set to @texture and setting that material as the source with
 * cogl_set_source.
 *
 * Note: There is no interaction between calls to cogl_set_source_color
 * and cogl_set_source_texture. If you need to blend a texture with a color then
 * you can create a simple material like this:
 * <programlisting>
 * material = cogl_material_new ();
 * cogl_material_set_color4ub (material, 0xff, 0x00, 0x00, 0x80);
 * cogl_material_set_layer (material, 0, tex_handle);
 * cogl_set_source (material);
 * </programlisting>
 *
 * Since: 1.0
 * Deprecated: 1.16: Latest drawing apis all take an explicit
 *                   #CoglPipeline argument so this stack of
 *                   #CoglMaterial<!-- -->s shouldn't be used.
 */
void
cogl_set_source_texture (CoglTexture *texture) COGL_DEPRECATED_IN_1_16;


/**
 * SECTION:cogl-clipping
 * @short_description: Fuctions for manipulating a stack of clipping regions
 *
 * To support clipping your geometry to rectangles or paths Cogl exposes a
 * stack based API whereby each clip region you push onto the stack is
 * intersected with the previous region.
 */

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
void
cogl_clip_push_window_rect (float x_offset,
                            float y_offset,
                            float width,
                            float height)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_scissor_clip);

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
void
cogl_clip_push_window_rectangle (int x_offset,
                                 int y_offset,
                                 int width,
                                 int height)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_scissor_clip);

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
void
cogl_clip_push (float x_offset,
                float y_offset,
                float width,
                float height)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_rectangle_clip);

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
void
cogl_clip_push_rectangle (float x0,
                          float y0,
                          float x1,
                          float y1)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_rectangle_clip);

/**
 * cogl_clip_push_primitive:
 * @primitive: A #CoglPrimitive describing a flat 2D shape
 * @bounds_x1: x coordinate for the top-left corner of the primitives
 *             bounds
 * @bounds_y1: y coordinate for the top-left corner of the primitives
 *             bounds
 * @bounds_x2: x coordinate for the top-left corner of the primitives
 *             bounds
 * @bounds_y2: x coordinate for the bottom-right corner of the
 *             primitives bounds.
 * @bounds_x1: y coordinate for the bottom-right corner of the
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
void
cogl_clip_push_primitive (CoglPrimitive *primitive,
                          float bounds_x1,
                          float bounds_y1,
                          float bounds_x2,
                          float bounds_y2)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_push_primitive_clip);

/**
 * cogl_clip_pop:
 *
 * Reverts the clipping region to the state before the last call to
 * cogl_clip_push().
 *
 * Deprecated: 1.16: Use cogl_framebuffer_pop_clip() instead
 */
void
cogl_clip_pop (void)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_pop_clip);

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
void
cogl_clip_ensure (void) COGL_DEPRECATED;

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
void
cogl_clip_stack_save (void) COGL_DEPRECATED;

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
void
cogl_clip_stack_restore (void) COGL_DEPRECATED;

/**
 * cogl_set_framebuffer:
 * @buffer: A #CoglFramebuffer object, either onscreen or offscreen.
 *
 * This redirects all subsequent drawing to the specified framebuffer. This can
 * either be an offscreen buffer created with cogl_offscreen_new_to_texture ()
 * or in the future it may be an onscreen framebuffers too.
 *
 * Since: 1.2
 * Deprecated: 1.16: The latest drawing apis take explicit
 *                   #CoglFramebuffer arguments so this stack of
 *                   framebuffers shouldn't be used anymore.
 */
void
cogl_set_framebuffer (CoglFramebuffer *buffer) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_push_framebuffer:
 * @buffer: A #CoglFramebuffer object, either onscreen or offscreen.
 *
 * Redirects all subsequent drawing to the specified framebuffer. This can
 * either be an offscreen buffer created with cogl_offscreen_new_to_texture ()
 * or in the future it may be an onscreen framebuffer too.
 *
 * You should understand that a framebuffer owns the following state:
 * <itemizedlist>
 *  <listitem><simpara>The projection matrix</simpara></listitem>
 *  <listitem><simpara>The modelview matrix stack</simpara></listitem>
 *  <listitem><simpara>The viewport</simpara></listitem>
 *  <listitem><simpara>The clip stack</simpara></listitem>
 * </itemizedlist>
 * So these items will automatically be saved and restored when you
 * push and pop between different framebuffers.
 *
 * Also remember a newly allocated framebuffer will have an identity matrix for
 * the projection and modelview matrices which gives you a coordinate space
 * like OpenGL with (-1, -1) corresponding to the top left of the viewport,
 * (1, 1) corresponding to the bottom right and +z coming out towards the
 * viewer.
 *
 * If you want to set up a coordinate space like Clutter does with (0, 0)
 * corresponding to the top left and (framebuffer_width, framebuffer_height)
 * corresponding to the bottom right you can do so like this:
 *
 * |[
 * static void
 * setup_viewport (unsigned int width,
 *                 unsigned int height,
 *                 float fovy,
 *                 float aspect,
 *                 float z_near,
 *                 float z_far)
 * {
 *   float z_camera;
 *   CoglMatrix projection_matrix;
 *   CoglMatrix mv_matrix;
 *
 *   cogl_set_viewport (0, 0, width, height);
 *   cogl_perspective (fovy, aspect, z_near, z_far);
 *
 *   cogl_get_projection_matrix (&amp;projection_matrix);
 *   z_camera = 0.5 * projection_matrix.xx;
 *
 *   cogl_matrix_init_identity (&amp;mv_matrix);
 *   cogl_matrix_translate (&amp;mv_matrix, -0.5f, -0.5f, -z_camera);
 *   cogl_matrix_scale (&amp;mv_matrix, 1.0f / width, -1.0f / height, 1.0f / width);
 *   cogl_matrix_translate (&amp;mv_matrix, 0.0f, -1.0 * height, 0.0f);
 *   cogl_set_modelview_matrix (&amp;mv_matrix);
 * }
 *
 * static void
 * my_init_framebuffer (ClutterStage *stage,
 *                      CoglFramebuffer *framebuffer,
 *                      unsigned int framebuffer_width,
 *                      unsigned int framebuffer_height)
 * {
 *   ClutterPerspective perspective;
 *
 *   clutter_stage_get_perspective (stage, &perspective);
 *
 *   cogl_push_framebuffer (framebuffer);
 *   setup_viewport (framebuffer_width,
 *                   framebuffer_height,
 *                   perspective.fovy,
 *                   perspective.aspect,
 *                   perspective.z_near,
 *                   perspective.z_far);
 * }
 * ]|
 *
 * The previous framebuffer can be restored by calling cogl_pop_framebuffer()
 *
 * Since: 1.2
 * Deprecated: 1.16: The latest drawing apis take explicit
 *                   #CoglFramebuffer arguments so this stack of
 *                   framebuffers shouldn't be used anymore.
 */
void
cogl_push_framebuffer (CoglFramebuffer *buffer) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_pop_framebuffer:
 *
 * Restores the framebuffer that was previously at the top of the stack.
 * All subsequent drawing will be redirected to this framebuffer.
 *
 * Since: 1.2
 * Deprecated: 1.16: The latest drawing apis take explicit
 *                   #CoglFramebuffer arguments so this stack of
 *                   framebuffers shouldn't be used anymore.
 */
void
cogl_pop_framebuffer (void) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_set_draw_buffer:
 * @target: A #CoglBufferTarget that specifies what kind of framebuffer you
 *          are setting as the render target.
 * @offscreen: If you are setting a framebuffer of type COGL_OFFSCREEN_BUFFER
 *             then this is a CoglHandle for the offscreen buffer.
 *
 * Redirects all subsequent drawing to the specified framebuffer. This
 * can either be an offscreen buffer created with
 * cogl_offscreen_new_to_texture () or you can revert to your original
 * on screen window buffer.
 *
 * Deprecated: 1.16: The latest drawing apis take explicit
 *                   #CoglFramebuffer arguments so this stack of
 *                   framebuffers shouldn't be used anymore.
 */
void
cogl_set_draw_buffer (CoglBufferTarget target,
                      CoglHandle offscreen) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_push_draw_buffer:
 *
 * Save cogl_set_draw_buffer() state.
 *
 * Deprecated: 1.16: The latest drawing apis take explicit
 *                   #CoglFramebuffer arguments so this stack of
 *                   framebuffers shouldn't be used anymore.
 */
void
cogl_push_draw_buffer (void) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_pop_draw_buffer:
 *
 * Restore cogl_set_draw_buffer() state.
 *
 * Deprecated: 1.16: The latest drawing apis take explicit
 *                   #CoglFramebuffer arguments so this stack of
 *                   framebuffers shouldn't be used anymore.
 */
void
cogl_pop_draw_buffer (void) COGL_DEPRECATED_IN_1_16;

/**
 * cogl_read_pixels:
 * @x: The window x position to start reading from
 * @y: The window y position to start reading from
 * @width: The width of the rectangle you want to read
 * @height: The height of the rectangle you want to read
 * @source: Identifies which auxillary buffer you want to read
 *          (only COGL_READ_PIXELS_COLOR_BUFFER supported currently)
 * @format: The pixel format you want the result in
 *          (only COGL_PIXEL_FORMAT_RGBA_8888 supported currently)
 * @pixels: The location to write the pixel data.
 *
 * This reads a rectangle of pixels from the current framebuffer where
 * position (0, 0) is the top left. The pixel at (x, y) is the first
 * read, and the data is returned with a rowstride of (width * 4).
 *
 * Currently Cogl assumes that the framebuffer is in a premultiplied
 * format so if @format is non-premultiplied it will convert it. To
 * read the pixel values without any conversion you should either
 * specify a format that doesn't use an alpha channel or use one of
 * the formats ending in PRE.
 *
 * Deprecated: 1.16: Use cogl_framebuffer_read_pixels() instead
 */
void
cogl_read_pixels (int x,
                  int y,
                  int width,
                  int height,
                  CoglReadPixelsFlags source,
                  CoglPixelFormat format,
                  uint8_t *pixels)
     COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_read_pixels);

/**
 * cogl_flush:
 *
 * This function should only need to be called in exceptional circumstances.
 *
 * As an optimization Cogl drawing functions may batch up primitives
 * internally, so if you are trying to use raw GL outside of Cogl you stand a
 * better chance of being successful if you ask Cogl to flush any batched
 * geometry before making your state changes.
 *
 * It only ensure that the underlying driver is issued all the commands
 * necessary to draw the batched primitives. It provides no guarantees about
 * when the driver will complete the rendering.
 *
 * This provides no guarantees about the GL state upon returning and to avoid
 * confusing Cogl you should aim to restore any changes you make before
 * resuming use of Cogl.
 *
 * If you are making state changes with the intention of affecting Cogl drawing
 * primitives you are 100% on your own since you stand a good chance of
 * conflicting with Cogl internals. For example clutter-gst which currently
 * uses direct GL calls to bind ARBfp programs will very likely break when Cogl
 * starts to use ARBfb programs itself for the material API.
 *
 * Since: 1.0
 */
void
cogl_flush (void);

/**
 * cogl_begin_gl:
 *
 * We do not advise nor reliably support the interleaving of raw GL drawing and
 * Cogl drawing functions, but if you insist, cogl_begin_gl() and cogl_end_gl()
 * provide a simple mechanism that may at least give you a fighting chance of
 * succeeding.
 *
 * Note: this doesn't help you modify the behaviour of Cogl drawing functions
 * through the modification of GL state; that will never be reliably supported,
 * but if you are trying to do something like:
 *
 * |[
 * {
 *    - setup some OpenGL state.
 *    - draw using OpenGL (e.g. glDrawArrays() )
 *    - reset modified OpenGL state.
 *    - continue using Cogl to draw
 * }
 * ]|
 *
 * You should surround blocks of drawing using raw GL with cogl_begin_gl()
 * and cogl_end_gl():
 *
 * |[
 * {
 *    cogl_begin_gl ();
 *    - setup some OpenGL state.
 *    - draw using OpenGL (e.g. glDrawArrays() )
 *    - reset modified OpenGL state.
 *    cogl_end_gl ();
 *    - continue using Cogl to draw
 * }
 * ]|
 *
 * Don't ever try and do:
 *
 * |[
 * {
 *    - setup some OpenGL state.
 *    - use Cogl to draw
 *    - reset modified OpenGL state.
 * }
 * ]|
 *
 * When the internals of Cogl evolves, this is very liable to break.
 *
 * This function will flush all batched primitives, and subsequently flush
 * all internal Cogl state to OpenGL as if it were going to draw something
 * itself.
 *
 * The result is that the OpenGL modelview matrix will be setup; the state
 * corresponding to the current source material will be set up and other world
 * state such as backface culling, depth and fogging enabledness will be sent
 * to OpenGL.
 *
 * <note>No special material state is flushed, so if you want Cogl to setup a
 * simplified material state it is your responsibility to set a simple source
 * material before calling cogl_begin_gl(). E.g. by calling
 * cogl_set_source_color4ub().</note>
 *
 * <note>It is your responsibility to restore any OpenGL state that you modify
 * to how it was after calling cogl_begin_gl() if you don't do this then the
 * result of further Cogl calls is undefined.</note>
 *
 * <note>You can not nest begin/end blocks.</note>
 *
 * Again we would like to stress, we do not advise the use of this API and if
 * possible we would prefer to improve Cogl than have developers require raw
 * OpenGL.
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglGLES2Context api instead
 */
void
cogl_begin_gl (void) COGL_DEPRECATED_IN_1_16_FOR (CoglGLES2Context_API);

/**
 * cogl_end_gl:
 *
 * This is the counterpart to cogl_begin_gl() used to delimit blocks of drawing
 * code using raw OpenGL. Please refer to cogl_begin_gl() for full details.
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglGLES2Context api instead
 */
void
cogl_end_gl (void) COGL_DEPRECATED_IN_1_16_FOR (CoglGLES2Context_API);

COGL_END_DECLS

#endif /* __COGL_1_CONTEXT_H__ */
