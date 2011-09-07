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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_PIPELINE_LAYER_STATE_H__
#define __COGL_PIPELINE_LAYER_STATE_H__

#include <cogl/cogl-pipeline.h>
#include <cogl/cogl-color.h>
#include <cogl/cogl-matrix.h>
#include <cogl/cogl-texture.h>
#include <glib.h>

G_BEGIN_DECLS

#ifdef COGL_ENABLE_EXPERIMENTAL_API

/**
 * CoglPipelineFilter:
 * @COGL_PIPELINE_FILTER_NEAREST: Measuring in manhatten distance from the,
 *   current pixel center, use the nearest texture texel
 * @COGL_PIPELINE_FILTER_LINEAR: Use the weighted average of the 4 texels
 *   nearest the current pixel center
 * @COGL_PIPELINE_FILTER_NEAREST_MIPMAP_NEAREST: Select the mimap level whose
 *   texel size most closely matches the current pixel, and use the
 *   %COGL_PIPELINE_FILTER_NEAREST criterion
 * @COGL_PIPELINE_FILTER_LINEAR_MIPMAP_NEAREST: Select the mimap level whose
 *   texel size most closely matches the current pixel, and use the
 *   %COGL_PIPELINE_FILTER_LINEAR criterion
 * @COGL_PIPELINE_FILTER_NEAREST_MIPMAP_LINEAR: Select the two mimap levels
 *   whose texel size most closely matches the current pixel, use
 *   the %COGL_PIPELINE_FILTER_NEAREST criterion on each one and take
 *   their weighted average
 * @COGL_PIPELINE_FILTER_LINEAR_MIPMAP_LINEAR: Select the two mimap levels
 *   whose texel size most closely matches the current pixel, use
 *   the %COGL_PIPELINE_FILTER_LINEAR criterion on each one and take
 *   their weighted average
 *
 * Texture filtering is used whenever the current pixel maps either to more
 * than one texture element (texel) or less than one. These filter enums
 * correspond to different strategies used to come up with a pixel color, by
 * possibly referring to multiple neighbouring texels and taking a weighted
 * average or simply using the nearest texel.
 */
typedef enum {
  COGL_PIPELINE_FILTER_NEAREST = 0x2600,
  COGL_PIPELINE_FILTER_LINEAR = 0x2601,
  COGL_PIPELINE_FILTER_NEAREST_MIPMAP_NEAREST = 0x2700,
  COGL_PIPELINE_FILTER_LINEAR_MIPMAP_NEAREST = 0x2701,
  COGL_PIPELINE_FILTER_NEAREST_MIPMAP_LINEAR = 0x2702,
  COGL_PIPELINE_FILTER_LINEAR_MIPMAP_LINEAR = 0x2703
} CoglPipelineFilter;
/* NB: these values come from the equivalents in gl.h */

/**
 * CoglPipelineWrapMode:
 * @COGL_PIPELINE_WRAP_MODE_REPEAT: The texture will be repeated. This
 *   is useful for example to draw a tiled background.
 * @COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE: The coordinates outside the
 *   range 0→1 will sample copies of the edge pixels of the
 *   texture. This is useful to avoid artifacts if only one copy of
 *   the texture is being rendered.
 * @COGL_PIPELINE_WRAP_MODE_AUTOMATIC: Cogl will try to automatically
 *   decide which of the above two to use. For cogl_rectangle(), it
 *   will use repeat mode if any of the texture coordinates are
 *   outside the range 0→1, otherwise it will use clamp to edge. For
 *   cogl_polygon() it will always use repeat mode. For
 *   cogl_vertex_buffer_draw() it will use repeat mode except for
 *   layers that have point sprite coordinate generation enabled. This
 *   is the default value.
 *
 * The wrap mode specifies what happens when texture coordinates
 * outside the range 0→1 are used. Note that if the filter mode is
 * anything but %COGL_PIPELINE_FILTER_NEAREST then texels outside the
 * range 0→1 might be used even when the coordinate is exactly 0 or 1
 * because OpenGL will try to sample neighbouring pixels. For example
 * if you are trying to render the full texture then you may get
 * artifacts around the edges when the pixels from the other side are
 * merged in if the wrap mode is set to repeat.
 *
 * Since: 2.0
 */
/* GL_ALWAYS is just used here as a value that is known not to clash
 * with any valid GL wrap modes
 *
 * XXX: keep the values in sync with the CoglPipelineWrapModeInternal
 * enum so no conversion is actually needed.
 */
typedef enum {
  COGL_PIPELINE_WRAP_MODE_REPEAT = 0x2901,
  COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE = 0x812F,
  COGL_PIPELINE_WRAP_MODE_AUTOMATIC = 0x0207
} CoglPipelineWrapMode;
/* NB: these values come from the equivalents in gl.h */

#define cogl_pipeline_set_layer_texture cogl_pipeline_set_layer_texture_EXP
/**
 * cogl_pipeline_set_layer:
 * @pipeline: A #CoglPipeline object
 * @layer_index: the index of the layer
 * @texture: a #CoglHandle for the layer object
 *
 * In addition to the standard OpenGL lighting model a Cogl pipeline may have
 * one or more layers comprised of textures that can be blended together in
 * order, with a number of different texture combine modes. This function
 * defines a new texture layer.
 *
 * The index values of multiple layers do not have to be consecutive; it is
 * only their relative order that is important.
 *
 * <note>In the future, we may define other types of pipeline layers, such
 * as purely GLSL based layers.</note>
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_layer_texture (CoglPipeline *pipeline,
                                 int           layer_index,
                                 CoglHandle    texture);

#define cogl_pipeline_remove_layer cogl_pipeline_remove_layer_EXP
/**
 * cogl_pipeline_remove_layer:
 * @pipeline: A #CoglPipeline object
 * @layer_index: Specifies the layer you want to remove
 *
 * This function removes a layer from your pipeline
 */
void
cogl_pipeline_remove_layer (CoglPipeline *pipeline,
			    int           layer_index);

#define cogl_pipeline_set_layer_combine cogl_pipeline_set_layer_combine_EXP
/**
 * cogl_pipeline_set_layer_combine:
 * @pipeline: A #CoglPipeline object
 * @layer_index: Specifies the layer you want define a combine function for
 * @blend_string: A <link linkend="cogl-Blend-Strings">Cogl blend string</link>
 *    describing the desired texture combine function.
 * @error: A #GError that may report parse errors or lack of GPU/driver
 *   support. May be %NULL, in which case a warning will be printed out if an
 *   error is encountered.
 *
 * If not already familiar; you can refer
 * <link linkend="cogl-Blend-Strings">here</link> for an overview of what blend
 * strings are and there syntax.
 *
 * These are all the functions available for texture combining:
 * <itemizedlist>
 *   <listitem>REPLACE(arg0) = arg0</listitem>
 *   <listitem>MODULATE(arg0, arg1) = arg0 x arg1</listitem>
 *   <listitem>ADD(arg0, arg1) = arg0 + arg1</listitem>
 *   <listitem>ADD_SIGNED(arg0, arg1) = arg0 + arg1 - 0.5</listitem>
 *   <listitem>INTERPOLATE(arg0, arg1, arg2) = arg0 x arg2 + arg1 x (1 - arg2)</listitem>
 *   <listitem>SUBTRACT(arg0, arg1) = arg0 - arg1</listitem>
 *   <listitem>
 *     <programlisting>
 *  DOT3_RGB(arg0, arg1) = 4 x ((arg0[R] - 0.5)) * (arg1[R] - 0.5) +
 *                              (arg0[G] - 0.5)) * (arg1[G] - 0.5) +
 *                              (arg0[B] - 0.5)) * (arg1[B] - 0.5))
 *     </programlisting>
 *   </listitem>
 *   <listitem>
 *     <programlisting>
 *  DOT3_RGBA(arg0, arg1) = 4 x ((arg0[R] - 0.5)) * (arg1[R] - 0.5) +
 *                               (arg0[G] - 0.5)) * (arg1[G] - 0.5) +
 *                               (arg0[B] - 0.5)) * (arg1[B] - 0.5))
 *     </programlisting>
 *   </listitem>
 * </itemizedlist>
 *
 * Refer to the
 * <link linkend="cogl-Blend-String-syntax">color-source syntax</link> for
 * describing the arguments. The valid source names for texture combining
 * are:
 * <variablelist>
 *   <varlistentry>
 *     <term>TEXTURE</term>
 *     <listitem>Use the color from the current texture layer</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>TEXTURE_0, TEXTURE_1, etc</term>
 *     <listitem>Use the color from the specified texture layer</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>CONSTANT</term>
 *     <listitem>Use the color from the constant given with
 *     cogl_pipeline_set_layer_constant()</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>PRIMARY</term>
 *     <listitem>Use the color of the pipeline as set with
 *     cogl_pipeline_set_color()</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>PREVIOUS</term>
 *     <listitem>Either use the texture color from the previous layer, or
 *     if this is layer 0, use the color of the pipeline as set with
 *     cogl_pipeline_set_color()</listitem>
 *   </varlistentry>
 * </variablelist>
 *
 * <refsect2 id="cogl-Layer-Combine-Examples">
 *   <title>Layer Combine Examples</title>
 *   <para>This is effectively what the default blending is:</para>
 *   <informalexample><programlisting>
 *   RGBA = MODULATE (PREVIOUS, TEXTURE)
 *   </programlisting></informalexample>
 *   <para>This could be used to cross-fade between two images, using
 *   the alpha component of a constant as the interpolator. The constant
 *   color is given by calling cogl_pipeline_set_layer_constant.</para>
 *   <informalexample><programlisting>
 *   RGBA = INTERPOLATE (PREVIOUS, TEXTURE, CONSTANT[A])
 *   </programlisting></informalexample>
 * </refsect2>
 *
 * <note>You can't give a multiplication factor for arguments as you can
 * with blending.</note>
 *
 * Return value: %TRUE if the blend string was successfully parsed, and the
 *   described texture combining is supported by the underlying driver and
 *   or hardware. On failure, %FALSE is returned and @error is set
 *
 * Since: 2.0
 */
gboolean
cogl_pipeline_set_layer_combine (CoglPipeline *pipeline,
				 int           layer_index,
				 const char   *blend_string,
                                 GError      **error);

#define cogl_pipeline_set_layer_combine_constant \
  cogl_pipeline_set_layer_combine_constant_EXP
/**
 * cogl_pipeline_set_layer_combine_constant:
 * @pipeline: A #CoglPipeline object
 * @layer_index: Specifies the layer you want to specify a constant used
 *               for texture combining
 * @constant: The constant color you want
 *
 * When you are using the 'CONSTANT' color source in a layer combine
 * description then you can use this function to define its value.
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_layer_combine_constant (CoglPipeline    *pipeline,
                                          int              layer_index,
                                          const CoglColor *constant);

#define cogl_pipeline_set_layer_matrix cogl_pipeline_set_layer_matrix_EXP
/**
 * cogl_pipeline_set_layer_matrix:
 * @pipeline: A #CoglPipeline object
 * @layer_index: the index for the layer inside @pipeline
 * @matrix: the transformation matrix for the layer
 *
 * This function lets you set a matrix that can be used to e.g. translate
 * and rotate a single layer of a pipeline used to fill your geometry.
 */
void
cogl_pipeline_set_layer_matrix (CoglPipeline     *pipeline,
				int               layer_index,
				const CoglMatrix *matrix);

#define cogl_pipeline_get_n_layers cogl_pipeline_get_n_layers_EXP
/**
 * cogl_pipeline_get_n_layers:
 * @pipeline: A #CoglPipeline object
 *
 * Retrieves the number of layers defined for the given @pipeline
 *
 * Return value: the number of layers
 *
 * Since: 2.0
 */
int
cogl_pipeline_get_n_layers (CoglPipeline *pipeline);

#define cogl_pipeline_set_layer_filters cogl_pipeline_set_layer_filters_EXP
/**
 * cogl_pipeline_set_layer_filters:
 * @pipeline: A #CoglPipeline object
 * @layer_index: the layer number to change.
 * @min_filter: the filter used when scaling a texture down.
 * @mag_filter: the filter used when magnifying a texture.
 *
 * Changes the decimation and interpolation filters used when a texture is
 * drawn at other scales than 100%.
 */
void
cogl_pipeline_set_layer_filters (CoglPipeline      *pipeline,
                                 int                layer_index,
                                 CoglPipelineFilter min_filter,
                                 CoglPipelineFilter mag_filter);

#define cogl_pipeline_set_layer_point_sprite_coords_enabled \
  cogl_pipeline_set_layer_point_sprite_coords_enabled_EXP
/**
 * cogl_pipeline_set_layer_point_sprite_coords_enabled:
 * @pipeline: a #CoglHandle to a pipeline.
 * @layer_index: the layer number to change.
 * @enable: whether to enable point sprite coord generation.
 * @error: A return location for a GError, or NULL to ignore errors.
 *
 * When rendering points, if @enable is %TRUE then the texture
 * coordinates for this layer will be replaced with coordinates that
 * vary from 0.0 to 1.0 across the primitive. The top left of the
 * point will have the coordinates 0.0,0.0 and the bottom right will
 * have 1.0,1.0. If @enable is %FALSE then the coordinates will be
 * fixed for the entire point.
 *
 * This function will only work if %COGL_FEATURE_POINT_SPRITE is
 * available. If the feature is not available then the function will
 * return %FALSE and set @error.
 *
 * Return value: %TRUE if the function succeeds, %FALSE otherwise.
 * Since: 2.0
 */
gboolean
cogl_pipeline_set_layer_point_sprite_coords_enabled (CoglPipeline *pipeline,
                                                     int           layer_index,
                                                     gboolean      enable,
                                                     GError      **error);

#define cogl_pipeline_get_layer_point_sprite_coords_enabled \
  cogl_pipeline_get_layer_point_sprite_coords_enabled_EXP
/**
 * cogl_pipeline_get_layer_point_sprite_coords_enabled:
 * @pipeline: a #CoglHandle to a pipeline.
 * @layer_index: the layer number to check.
 *
 * Gets whether point sprite coordinate generation is enabled for this
 * texture layer.
 *
 * Return value: whether the texture coordinates will be replaced with
 * point sprite coordinates.
 *
 * Since: 2.0
 */
gboolean
cogl_pipeline_get_layer_point_sprite_coords_enabled (CoglPipeline *pipeline,
                                                     int           layer_index);

#define cogl_pipeline_get_layer_wrap_mode_s \
  cogl_pipeline_get_layer_wrap_mode_s_EXP
/**
 * cogl_pipeline_get_layer_wrap_mode_s:
 * @pipeline: A #CoglPipeline object
 * @layer_index: the layer number to change.
 *
 * Returns the wrap mode for the 's' coordinate of texture lookups on this
 * layer.
 *
 * Return value: the wrap mode for the 's' coordinate of texture lookups on
 * this layer.
 *
 * Since: 1.6
 */
CoglPipelineWrapMode
cogl_pipeline_get_layer_wrap_mode_s (CoglPipeline *pipeline,
                                     int           layer_index);

#define cogl_pipeline_set_layer_wrap_mode_s \
  cogl_pipeline_set_layer_wrap_mode_s_EXP
/**
 * cogl_pipeline_set_layer_wrap_mode_s:
 * @pipeline: A #CoglPipeline object
 * @layer_index: the layer number to change.
 * @mode: the new wrap mode
 *
 * Sets the wrap mode for the 's' coordinate of texture lookups on this layer.
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_layer_wrap_mode_s (CoglPipeline        *pipeline,
                                     int                  layer_index,
                                     CoglPipelineWrapMode mode);

#define cogl_pipeline_get_layer_wrap_mode_t \
  cogl_pipeline_get_layer_wrap_mode_t_EXP
/**
 * cogl_pipeline_get_layer_wrap_mode_t:
 * @pipeline: A #CoglPipeline object
 * @layer_index: the layer number to change.
 *
 * Returns the wrap mode for the 't' coordinate of texture lookups on this
 * layer.
 *
 * Return value: the wrap mode for the 't' coordinate of texture lookups on
 * this layer.
 *
 * Since: 1.6
 */
CoglPipelineWrapMode
cogl_pipeline_get_layer_wrap_mode_t (CoglPipeline *pipeline,
                                     int           layer_index);


#define cogl_pipeline_set_layer_wrap_mode_t \
  cogl_pipeline_set_layer_wrap_mode_t_EXP
/**
 * cogl_pipeline_set_layer_wrap_mode_t:
 * @pipeline: A #CoglPipeline object
 * @layer_index: the layer number to change.
 * @mode: the new wrap mode
 *
 * Sets the wrap mode for the 't' coordinate of texture lookups on this layer.
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_layer_wrap_mode_t (CoglPipeline        *pipeline,
                                     int                  layer_index,
                                     CoglPipelineWrapMode mode);

#define cogl_pipeline_get_layer_wrap_mode_p \
  cogl_pipeline_get_layer_wrap_mode_p_EXP
/**
 * cogl_pipeline_get_layer_wrap_mode_p:
 * @pipeline: A #CoglPipeline object
 * @layer_index: the layer number to change.
 *
 * Returns the wrap mode for the 'p' coordinate of texture lookups on this
 * layer.
 *
 * Return value: the wrap mode for the 'p' coordinate of texture lookups on
 * this layer.
 *
 * Since: 1.6
 */
CoglPipelineWrapMode
cogl_pipeline_get_layer_wrap_mode_p (CoglPipeline *pipeline,
                                     int           layer_index);

#define cogl_pipeline_set_layer_wrap_mode_p \
  cogl_pipeline_set_layer_wrap_mode_p_EXP
/**
 * cogl_pipeline_set_layer_wrap_mode_p:
 * @pipeline: A #CoglPipeline object
 * @layer_index: the layer number to change.
 * @mode: the new wrap mode
 *
 * Sets the wrap mode for the 'p' coordinate of texture lookups on
 * this layer. 'p' is the third coordinate.
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_layer_wrap_mode_p (CoglPipeline        *pipeline,
                                     int                  layer_index,
                                     CoglPipelineWrapMode mode);

#define cogl_pipeline_set_layer_wrap_mode \
  cogl_pipeline_set_layer_wrap_mode_EXP
/**
 * cogl_pipeline_set_layer_wrap_mode:
 * @pipeline: A #CoglPipeline object
 * @layer_index: the layer number to change.
 * @mode: the new wrap mode
 *
 * Sets the wrap mode for all three coordinates of texture lookups on
 * this layer. This is equivalent to calling
 * cogl_pipeline_set_layer_wrap_mode_s(),
 * cogl_pipeline_set_layer_wrap_mode_t() and
 * cogl_pipeline_set_layer_wrap_mode_p() separately.
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_layer_wrap_mode (CoglPipeline        *pipeline,
                                   int                  layer_index,
                                   CoglPipelineWrapMode mode);

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

G_END_DECLS

#endif /* __COGL_PIPELINE_LAYER_STATE_H__ */
