/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
COGL_DEPRECATED_IN_1_16
GOptionGroup *
cogl_get_option_group (void);

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
COGL_DEPRECATED_IN_1_10_FOR (cogl_foreach_feature)
CoglFeatureFlags
cogl_get_features (void);

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
COGL_DEPRECATED_IN_1_10_FOR (cogl_has_feature)
CoglBool
cogl_features_available (CoglFeatureFlags features);

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
COGL_DEPRECATED
CoglBool
cogl_check_extension (const char *name,
                      const char *ext);

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
COGL_DEPRECATED_IN_1_8_FOR (cogl_framebuffer_get_red_OR_green_OR_blue_OR_alpha_bits)
void
cogl_get_bitmasks (int *red,
                   int *green,
                   int *blue,
                   int *alpha);

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
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_perspective)
void
cogl_perspective (float fovy,
                  float aspect,
                  float z_near,
                  float z_far);

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
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_frustum)
void
cogl_frustum (float left,
              float right,
              float bottom,
              float top,
              float z_near,
              float z_far);

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
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_orthographic)
void
cogl_ortho (float left,
            float right,
            float bottom,
            float top,
            float near,
            float far);

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
COGL_DEPRECATED_IN_1_8_FOR (cogl_framebuffer_set_viewport)
void
cogl_viewport (unsigned int width,
	       unsigned int height);

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
COGL_DEPRECATED_IN_1_8_FOR (cogl_framebuffer_set_viewport)
void
cogl_set_viewport (int x,
                   int y,
                   int width,
                   int height);

/**
 * cogl_push_matrix:
 *
 * Stores the current model-view matrix on the matrix stack. The matrix
 * can later be restored with cogl_pop_matrix().
 *
 * Deprecated: 1.10: Use cogl_framebuffer_push_matrix() instead
 */
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_push_matrix)
void
cogl_push_matrix (void);

/**
 * cogl_pop_matrix:
 *
 * Restores the current model-view matrix from the matrix stack.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_pop_matrix() instead
 */
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_push_matrix)
void
cogl_pop_matrix (void);

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
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_scale)
void
cogl_scale (float x,
            float y,
            float z);

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
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_translate)
void
cogl_translate (float x,
                float y,
                float z);

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
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_rotate)
void
cogl_rotate (float angle,
             float x,
             float y,
             float z);

/**
 * cogl_transform:
 * @matrix: the matrix to multiply with the current model-view
 *
 * Multiplies the current model-view matrix by the given matrix.
 *
 * Since: 1.4
 * Deprecated: 1.10: Use cogl_framebuffer_transform() instead
 */
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_transform)
void
cogl_transform (const CoglMatrix *matrix);

/**
 * cogl_get_modelview_matrix:
 * @matrix: (out): return location for the model-view matrix
 *
 * Stores the current model-view matrix in @matrix.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_get_modelview_matrix()
 *                   instead
 */
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_get_modelview_matrix)
void
cogl_get_modelview_matrix (CoglMatrix *matrix);

/**
 * cogl_set_modelview_matrix:
 * @matrix: the new model-view matrix
 *
 * Loads @matrix as the new model-view matrix.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_set_modelview_matrix()
 *                   instead
 */
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_set_modelview_matrix)
void
cogl_set_modelview_matrix (CoglMatrix *matrix);

/**
 * cogl_get_projection_matrix:
 * @matrix: (out): return location for the projection matrix
 *
 * Stores the current projection matrix in @matrix.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_get_projection_matrix()
 *                   instead
 */
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_get_projection_matrix)
void
cogl_get_projection_matrix (CoglMatrix *matrix);

/**
 * cogl_set_projection_matrix:
 * @matrix: the new projection matrix
 *
 * Loads matrix as the new projection matrix.
 *
 * Deprecated: 1.10: Use cogl_framebuffer_set_projection_matrix()
 *                   instead
 */
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_set_projection_matrix)
void
cogl_set_projection_matrix (CoglMatrix *matrix);

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
COGL_DEPRECATED_IN_1_10_FOR (cogl_framebuffer_get_viewport4fv)
void
cogl_get_viewport (float v[4]);

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
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_depth_state)
void
cogl_set_depth_test_enabled (CoglBool setting);

/**
 * cogl_get_depth_test_enabled:
 *
 * Queries if depth testing has been enabled via cogl_set_depth_test_enable()
 *
 * Return value: %TRUE if depth testing is enabled, and %FALSE otherwise
 *
 * Deprecated: 1.16: Use cogl_pipeline_set_depth_state() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_depth_state)
CoglBool
cogl_get_depth_test_enabled (void);

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
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_cull_face_mode)
void
cogl_set_backface_culling_enabled (CoglBool setting);

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
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_get_cull_face_mode)
CoglBool
cogl_get_backface_culling_enabled (void);

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
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_API)
void
cogl_set_fog (const CoglColor *fog_color,
              CoglFogMode mode,
              float density,
              float z_near,
              float z_far);

/**
 * cogl_disable_fog:
 *
 * This function disables fogging, so primitives drawn afterwards will not be
 * blended with any previously set fog color.
 *
 * Deprecated: 1.16: Use #CoglSnippet shader api for fog
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_API)
void
cogl_disable_fog (void);

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
COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_clear)
void
cogl_clear (const CoglColor *color,
            unsigned long buffers);

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
COGL_DEPRECATED_IN_1_16
void
cogl_set_source (void *material);

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
COGL_DEPRECATED_IN_1_16
void *
cogl_get_source (void);

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
COGL_DEPRECATED_IN_1_16
void
cogl_push_source (void *material);

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
COGL_DEPRECATED_IN_1_16
void
cogl_pop_source (void);

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
COGL_DEPRECATED_IN_1_16
void
cogl_set_source_color (const CoglColor *color);

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
COGL_DEPRECATED_IN_1_16
void
cogl_set_source_color4ub (uint8_t red,
                          uint8_t green,
                          uint8_t blue,
                          uint8_t alpha);

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
COGL_DEPRECATED_IN_1_16
void
cogl_set_source_color4f (float red,
                         float green,
                         float blue,
                         float alpha);

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
COGL_DEPRECATED_IN_1_16
void
cogl_set_source_texture (CoglTexture *texture);

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
COGL_DEPRECATED_IN_1_16_FOR (CoglGLES2Context_API)
void
cogl_begin_gl (void);

/**
 * cogl_end_gl:
 *
 * This is the counterpart to cogl_begin_gl() used to delimit blocks of drawing
 * code using raw OpenGL. Please refer to cogl_begin_gl() for full details.
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglGLES2Context api instead
 */
COGL_DEPRECATED_IN_1_16_FOR (CoglGLES2Context_API)
void
cogl_end_gl (void);

COGL_END_DECLS

#endif /* __COGL_1_CONTEXT_H__ */
