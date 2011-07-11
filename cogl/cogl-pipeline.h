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

#ifndef __COGL_PIPELINE_H__
#define __COGL_PIPELINE_H__

G_BEGIN_DECLS

#include <cogl/cogl-types.h>
#include <cogl/cogl-matrix.h>

/**
 * SECTION:cogl-pipeline
 * @short_description: Functions for creating and manipulating the GPU
 *                     pipeline
 *
 * Cogl allows creating and manipulating objects representing the full
 * configuration of the GPU pipeline. In simplified terms the GPU
 * pipeline takes primitive geometry as the input, it first performs
 * vertex processing, allowing you to deform your geometry, then
 * rasterizes that (turning it from pure geometry into fragments) then
 * performs fragment processing including depth testing and texture
 * mapping. Finally it blends the result with the framebuffer.
 */

typedef struct _CoglPipeline	      CoglPipeline;

#define COGL_PIPELINE(OBJECT) ((CoglPipeline *)OBJECT)

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

/**
 * cogl_pipeline_new:
 *
 * Allocates and initializes a default simple pipeline that will color
 * a primitive white.
 *
 * Return value: a pointer to a new #CoglPipeline
 */
CoglPipeline *
cogl_pipeline_new (void);

/**
 * cogl_pipeline_copy:
 * @source: a #CoglPipeline object to copy
 *
 * Creates a new pipeline with the configuration copied from the
 * source pipeline.
 *
 * We would strongly advise developers to always aim to use
 * cogl_pipeline_copy() instead of cogl_pipeline_new() whenever there will
 * be any similarity between two pipelines. Copying a pipeline helps Cogl
 * keep track of a pipelines ancestry which we may use to help minimize GPU
 * state changes.
 *
 * Returns: a pointer to the newly allocated #CoglPipeline
 *
 * Since: 2.0
 */
CoglPipeline *
cogl_pipeline_copy (CoglPipeline *source);

/**
 * cogl_is_pipeline:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing pipeline object.
 *
 * Return value: %TRUE if the handle references a #CoglPipeline,
 *   %FALSE otherwise
 */
gboolean
cogl_is_pipeline (CoglHandle handle);

/**
 * cogl_pipeline_set_color:
 * @pipeline: A #CoglPipeline object
 * @color: The components of the color
 *
 * Sets the basic color of the pipeline, used when no lighting is enabled.
 *
 * Note that if you don't add any layers to the pipeline then the color
 * will be blended unmodified with the destination; the default blend
 * expects premultiplied colors: for example, use (0.5, 0.0, 0.0, 0.5) for
 * semi-transparent red. See cogl_color_premultiply().
 *
 * The default value is (1.0, 1.0, 1.0, 1.0)
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_color (CoglPipeline    *pipeline,
                         const CoglColor *color);

/**
 * cogl_pipeline_set_color4ub:
 * @pipeline: A #CoglPipeline object
 * @red: The red component
 * @green: The green component
 * @blue: The blue component
 * @alpha: The alpha component
 *
 * Sets the basic color of the pipeline, used when no lighting is enabled.
 *
 * The default value is (0xff, 0xff, 0xff, 0xff)
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_color4ub (CoglPipeline *pipeline,
			    guint8        red,
                            guint8        green,
                            guint8        blue,
                            guint8        alpha);

/**
 * cogl_pipeline_set_color4f:
 * @pipeline: A #CoglPipeline object
 * @red: The red component
 * @green: The green component
 * @blue: The blue component
 * @alpha: The alpha component
 *
 * Sets the basic color of the pipeline, used when no lighting is enabled.
 *
 * The default value is (1.0, 1.0, 1.0, 1.0)
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_color4f (CoglPipeline *pipeline,
                           float         red,
                           float         green,
                           float         blue,
                           float         alpha);

/**
 * cogl_pipeline_get_color:
 * @pipeline: A #CoglPipeline object
 * @color: (out): The location to store the color
 *
 * Retrieves the current pipeline color.
 *
 * Since: 2.0
 */
void
cogl_pipeline_get_color (CoglPipeline *pipeline,
                         CoglColor    *color);

/**
 * cogl_pipeline_set_ambient:
 * @pipeline: A #CoglPipeline object
 * @ambient: The components of the desired ambient color
 *
 * Sets the pipeline's ambient color, in the standard OpenGL lighting
 * model. The ambient color affects the overall color of the object.
 *
 * Since the diffuse color will be intense when the light hits the surface
 * directly, the ambient will be most apparent where the light hits at a
 * slant.
 *
 * The default value is (0.2, 0.2, 0.2, 1.0)
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_ambient (CoglPipeline    *pipeline,
			   const CoglColor *ambient);

/**
 * cogl_pipeline_get_ambient:
 * @pipeline: A #CoglPipeline object
 * @ambient: The location to store the ambient color
 *
 * Retrieves the current ambient color for @pipeline
 *
 * Since: 2.0
 */
void
cogl_pipeline_get_ambient (CoglPipeline *pipeline,
                           CoglColor    *ambient);

/**
 * cogl_pipeline_set_diffuse:
 * @pipeline: A #CoglPipeline object
 * @diffuse: The components of the desired diffuse color
 *
 * Sets the pipeline's diffuse color, in the standard OpenGL lighting
 * model. The diffuse color is most intense where the light hits the
 * surface directly - perpendicular to the surface.
 *
 * The default value is (0.8, 0.8, 0.8, 1.0)
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_diffuse (CoglPipeline    *pipeline,
			   const CoglColor *diffuse);

/**
 * cogl_pipeline_get_diffuse:
 * @pipeline: A #CoglPipeline object
 * @diffuse: The location to store the diffuse color
 *
 * Retrieves the current diffuse color for @pipeline
 *
 * Since: 2.0
 */
void
cogl_pipeline_get_diffuse (CoglPipeline *pipeline,
                           CoglColor    *diffuse);

/**
 * cogl_pipeline_set_ambient_and_diffuse:
 * @pipeline: A #CoglPipeline object
 * @color: The components of the desired ambient and diffuse colors
 *
 * Conveniently sets the diffuse and ambient color of @pipeline at the same
 * time. See cogl_pipeline_set_ambient() and cogl_pipeline_set_diffuse().
 *
 * The default ambient color is (0.2, 0.2, 0.2, 1.0)
 *
 * The default diffuse color is (0.8, 0.8, 0.8, 1.0)
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_ambient_and_diffuse (CoglPipeline    *pipeline,
				       const CoglColor *color);

/**
 * cogl_pipeline_set_specular:
 * @pipeline: A #CoglPipeline object
 * @specular: The components of the desired specular color
 *
 * Sets the pipeline's specular color, in the standard OpenGL lighting
 * model. The intensity of the specular color depends on the viewport
 * position, and is brightest along the lines of reflection.
 *
 * The default value is (0.0, 0.0, 0.0, 1.0)
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_specular (CoglPipeline    *pipeline,
			    const CoglColor *specular);

/**
 * cogl_pipeline_get_specular:
 * @pipeline: A #CoglPipeline object
 * @specular: The location to store the specular color
 *
 * Retrieves the pipelines current specular color.
 *
 * Since: 2.0
 */
void
cogl_pipeline_get_specular (CoglPipeline *pipeline,
                            CoglColor    *specular);

/**
 * cogl_pipeline_set_shininess:
 * @pipeline: A #CoglPipeline object
 * @shininess: The desired shininess; must be >= 0.0
 *
 * Sets the shininess of the pipeline, in the standard OpenGL lighting
 * model, which determines the size of the specular highlights. A
 * higher @shininess will produce smaller highlights which makes the
 * object appear more shiny.
 *
 * The default value is 0.0
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_shininess (CoglPipeline *pipeline,
			     float         shininess);

/**
 * cogl_pipeline_get_shininess:
 * @pipeline: A #CoglPipeline object
 *
 * Retrieves the pipelines current emission color.
 *
 * Return value: The pipelines current shininess value
 *
 * Since: 2.0
 */
float
cogl_pipeline_get_shininess (CoglPipeline *pipeline);

/**
 * cogl_pipeline_set_emission:
 * @pipeline: A #CoglPipeline object
 * @emission: The components of the desired emissive color
 *
 * Sets the pipeline's emissive color, in the standard OpenGL lighting
 * model. It will look like the surface is a light source emitting this
 * color.
 *
 * The default value is (0.0, 0.0, 0.0, 1.0)
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_emission (CoglPipeline    *pipeline,
			    const CoglColor *emission);

/**
 * cogl_pipeline_get_emission:
 * @pipeline: A #CoglPipeline object
 * @emission: The location to store the emission color
 *
 * Retrieves the pipelines current emission color.
 *
 * Since: 2.0
 */
void
cogl_pipeline_get_emission (CoglPipeline *pipeline,
                            CoglColor    *emission);

/**
 * CoglPipelineAlphaFunc:
 * @COGL_PIPELINE_ALPHA_FUNC_NEVER: Never let the fragment through.
 * @COGL_PIPELINE_ALPHA_FUNC_LESS: Let the fragment through if the incoming
 *   alpha value is less than the reference alpha value
 * @COGL_PIPELINE_ALPHA_FUNC_EQUAL: Let the fragment through if the incoming
 *   alpha value equals the reference alpha value
 * @COGL_PIPELINE_ALPHA_FUNC_LEQUAL: Let the fragment through if the incoming
 *   alpha value is less than or equal to the reference alpha value
 * @COGL_PIPELINE_ALPHA_FUNC_GREATER: Let the fragment through if the incoming
 *   alpha value is greater than the reference alpha value
 * @COGL_PIPELINE_ALPHA_FUNC_NOTEQUAL: Let the fragment through if the incoming
 *   alpha value does not equal the reference alpha value
 * @COGL_PIPELINE_ALPHA_FUNC_GEQUAL: Let the fragment through if the incoming
 *   alpha value is greater than or equal to the reference alpha value.
 * @COGL_PIPELINE_ALPHA_FUNC_ALWAYS: Always let the fragment through.
 *
 * Alpha testing happens before blending primitives with the framebuffer and
 * gives an opportunity to discard fragments based on a comparison with the
 * incoming alpha value and a reference alpha value. The #CoglPipelineAlphaFunc
 * determines how the comparison is done.
 */
typedef enum {
  COGL_PIPELINE_ALPHA_FUNC_NEVER    = 0x0200,
  COGL_PIPELINE_ALPHA_FUNC_LESS	    = 0x0201,
  COGL_PIPELINE_ALPHA_FUNC_EQUAL    = 0x0202,
  COGL_PIPELINE_ALPHA_FUNC_LEQUAL   = 0x0203,
  COGL_PIPELINE_ALPHA_FUNC_GREATER  = 0x0204,
  COGL_PIPELINE_ALPHA_FUNC_NOTEQUAL = 0x0205,
  COGL_PIPELINE_ALPHA_FUNC_GEQUAL   = 0x0206,
  COGL_PIPELINE_ALPHA_FUNC_ALWAYS   = 0x0207
} CoglPipelineAlphaFunc;
/* NB: these values come from the equivalents in gl.h */

/**
 * cogl_pipeline_set_alpha_test_function:
 * @pipeline: A #CoglPipeline object
 * @alpha_func: A @CoglPipelineAlphaFunc constant
 * @alpha_reference: A reference point that the chosen alpha function uses
 *   to compare incoming fragments to.
 *
 * Before a primitive is blended with the framebuffer, it goes through an
 * alpha test stage which lets you discard fragments based on the current
 * alpha value. This function lets you change the function used to evaluate
 * the alpha channel, and thus determine which fragments are discarded
 * and which continue on to the blending stage.
 *
 * The default is %COGL_PIPELINE_ALPHA_FUNC_ALWAYS
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_alpha_test_function (CoglPipeline         *pipeline,
				       CoglPipelineAlphaFunc alpha_func,
				       float                 alpha_reference);

/**
 * cogl_pipeline_get_alpha_test_function:
 * @pipeline: A #CoglPipeline object
 *
 * Return value: The alpha test function of @pipeline.
 *
 * Since: 2.0
 */
CoglPipelineAlphaFunc
cogl_pipeline_get_alpha_test_function (CoglPipeline *pipeline);

/**
 * cogl_pipeline_get_alpha_test_reference:
 * @pipeline: A #CoglPipeline object
 *
 * Return value: The alpha test reference value of @pipeline.
 *
 * Since: 2.0
 */
float
cogl_pipeline_get_alpha_test_reference (CoglPipeline *pipeline);

/**
 * cogl_pipeline_set_blend:
 * @pipeline: A #CoglPipeline object
 * @blend_string: A <link linkend="cogl-Blend-Strings">Cogl blend string</link>
 *   describing the desired blend function.
 * @error: return location for a #GError that may report lack of driver
 *   support if you give separate blend string statements for the alpha
 *   channel and RGB channels since some drivers, or backends such as
 *   GLES 1.1, don't support this feature. May be %NULL, in which case a
 *   warning will be printed out using GLib's logging facilities if an
 *   error is encountered.
 *
 * If not already familiar; please refer <link linkend="cogl-Blend-Strings">here</link>
 * for an overview of what blend strings are, and their syntax.
 *
 * Blending occurs after the alpha test function, and combines fragments with
 * the framebuffer.

 * Currently the only blend function Cogl exposes is ADD(). So any valid
 * blend statements will be of the form:
 *
 * |[
 *   &lt;channel-mask&gt;=ADD(SRC_COLOR*(&lt;factor&gt;), DST_COLOR*(&lt;factor&gt;))
 * ]|
 *
 * <warning>The brackets around blend factors are currently not
 * optional!</warning>
 *
 * This is the list of source-names usable as blend factors:
 * <itemizedlist>
 *   <listitem><para>SRC_COLOR: The color of the in comming fragment</para></listitem>
 *   <listitem><para>DST_COLOR: The color of the framebuffer</para></listitem>
 *   <listitem><para>CONSTANT: The constant set via cogl_pipeline_set_blend_constant()</para></listitem>
 * </itemizedlist>
 *
 * The source names can be used according to the
 * <link linkend="cogl-Blend-String-syntax">color-source and factor syntax</link>,
 * so for example "(1-SRC_COLOR[A])" would be a valid factor, as would
 * "(CONSTANT[RGB])"
 *
 * These can also be used as factors:
 * <itemizedlist>
 *   <listitem>0: (0, 0, 0, 0)</listitem>
 *   <listitem>1: (1, 1, 1, 1)</listitem>
 *   <listitem>SRC_ALPHA_SATURATE_FACTOR: (f,f,f,1) where f = MIN(SRC_COLOR[A],1-DST_COLOR[A])</listitem>
 * </itemizedlist>
 *
 * <note>Remember; all color components are normalized to the range [0, 1]
 * before computing the result of blending.</note>
 *
 * <example id="cogl-Blend-Strings-blend-unpremul">
 *   <title>Blend Strings/1</title>
 *   <para>Blend a non-premultiplied source over a destination with
 *   premultiplied alpha:</para>
 *   <programlisting>
 * "RGB = ADD(SRC_COLOR*(SRC_COLOR[A]), DST_COLOR*(1-SRC_COLOR[A]))"
 * "A   = ADD(SRC_COLOR, DST_COLOR*(1-SRC_COLOR[A]))"
 *   </programlisting>
 * </example>
 *
 * <example id="cogl-Blend-Strings-blend-premul">
 *   <title>Blend Strings/2</title>
 *   <para>Blend a premultiplied source over a destination with
 *   premultiplied alpha</para>
 *   <programlisting>
 * "RGBA = ADD(SRC_COLOR, DST_COLOR*(1-SRC_COLOR[A]))"
 *   </programlisting>
 * </example>
 *
 * The default blend string is:
 * |[
 *    RGBA = ADD (SRC_COLOR, DST_COLOR*(1-SRC_COLOR[A]))
 * ]|
 *
 * That gives normal alpha-blending when the calculated color for the pipeline
 * is in premultiplied form.
 *
 * Return value: %TRUE if the blend string was successfully parsed, and the
 *   described blending is supported by the underlying driver/hardware. If
 *   there was an error, %FALSE is returned and @error is set accordingly (if
 *   present).
 *
 * Since: 2.0
 */
gboolean
cogl_pipeline_set_blend (CoglPipeline *pipeline,
                         const char   *blend_string,
                         GError      **error);

/**
 * cogl_pipeline_set_blend_constant:
 * @pipeline: A #CoglPipeline object
 * @constant_color: The constant color you want
 *
 * When blending is setup to reference a CONSTANT blend factor then
 * blending will depend on the constant set with this function.
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_blend_constant (CoglPipeline *pipeline,
                                  const CoglColor *constant_color);

/**
 * cogl_pipeline_set_point_size:
 * @pipeline: a #CoglHandle to a pipeline.
 * @point_size: the new point size.
 *
 * Changes the size of points drawn when %COGL_VERTICES_MODE_POINTS is
 * used with the vertex buffer API. Note that typically the GPU will
 * only support a limited minimum and maximum range of point sizes. If
 * the chosen point size is outside that range then the nearest value
 * within that range will be used instead. The size of a point is in
 * screen space so it will be the same regardless of any
 * transformations. The default point size is 1.0.
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_point_size (CoglHandle pipeline,
                              float      point_size);

/**
 * cogl_pipeline_get_point_size:
 * @pipeline: a #CoglHandle to a pipeline.
 *
 * Get the size of points drawn when %COGL_VERTICES_MODE_POINTS is
 * used with the vertex buffer API.
 *
 * Return value: the point size of the pipeline.
 *
 * Since: 2.0
 */
float
cogl_pipeline_get_point_size (CoglHandle  pipeline);

#define cogl_pipeline_get_color_mask cogl_pipeline_get_color_mask_EXP
/**
 * cogl_pipeline_get_color_mask:
 * @pipeline: a #CoglPipeline object.
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
cogl_pipeline_get_color_mask (CoglPipeline *pipeline);

#define cogl_pipeline_set_color_mask cogl_pipeline_set_color_mask_EXP
/**
 * cogl_pipeline_set_color_mask:
 * @pipeline: a #CoglPipeline object.
 * @color_mask: A #CoglColorMask of which color channels to write to
 *              the current framebuffer.
 *
 * Defines a bit mask of which color channels should be written to the
 * current framebuffer. If a bit is set in @color_mask that means that
 * color will be written.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_pipeline_set_color_mask (CoglPipeline *pipeline,
                              CoglColorMask color_mask);

/**
 * cogl_pipeline_get_user_program:
 * @pipeline: a #CoglPipeline object.
 *
 * Queries what user program has been associated with the given
 * @pipeline using cogl_pipeline_set_user_program().
 *
 * Return value: The current user program or %COGL_INVALID_HANDLE.
 *
 * Since: 2.0
 */
CoglHandle
cogl_pipeline_get_user_program (CoglPipeline *pipeline);

/**
 * cogl_pipeline_set_user_program:
 * @pipeline: a #CoglPipeline object.
 * @program: A #CoglHandle to a linked CoglProgram
 *
 * Associates a linked CoglProgram with the given pipeline so that the
 * program can take full control of vertex and/or fragment processing.
 *
 * This is an example of how it can be used to associate an ARBfp
 * program with a #CoglPipeline:
 * |[
 * CoglHandle shader;
 * CoglHandle program;
 * CoglPipeline *pipeline;
 *
 * shader = cogl_create_shader (COGL_SHADER_TYPE_FRAGMENT);
 * cogl_shader_source (shader,
 *                     "!!ARBfp1.0\n"
 *                     "MOV result.color,fragment.color;\n"
 *                     "END\n");
 * cogl_shader_compile (shader);
 *
 * program = cogl_create_program ();
 * cogl_program_attach_shader (program, shader);
 * cogl_program_link (program);
 *
 * pipeline = cogl_pipeline_new ();
 * cogl_pipeline_set_user_program (pipeline, program);
 *
 * cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
 * cogl_rectangle (0, 0, 100, 100);
 * ]|
 *
 * It is possibly worth keeping in mind that this API is not part of
 * the long term design for how we want to expose shaders to Cogl
 * developers (We are planning on deprecating the cogl_program and
 * cogl_shader APIs in favour of a "snippet" framework) but in the
 * meantime we hope this will handle most practical GLSL and ARBfp
 * requirements.
 *
 * Also remember you need to check for either the
 * %COGL_FEATURE_SHADERS_GLSL or %COGL_FEATURE_SHADERS_ARBFP before
 * using the cogl_program or cogl_shader API.
 *
 * Since: 2.0
 */
void
cogl_pipeline_set_user_program (CoglPipeline *pipeline,
                                CoglHandle program);

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

#ifdef COGL_ENABLE_EXPERIMENTAL_API

/**
 * cogl_pipeline_set_depth_state:
 * @pipeline: A #CoglPipeline object
 * @state: A #CoglDepthState struct
 * @error: A #GError to report failures to setup the given @state.
 *
 * This commits all the depth state configured in @state struct to the
 * given @pipeline. The configuration values are copied into the
 * pipeline so there is no requirement to keep the #CoglDepthState
 * struct around if you don't need it any more.
 *
 * Note: Since some platforms do not support the depth range feature
 * it is possible for this function to fail and report an @error.
 *
 * Returns: TRUE if the GPU supports all the given @state else %FALSE
 *          and returns an @error.
 *
 * Since: 2.0
 * Stability: Unstable
 */
gboolean
cogl_pipeline_set_depth_state (CoglPipeline *pipeline,
                               const CoglDepthState *state,
                               GError **error);

/**
 * cogl_pipeline_get_depth_state
 * @pipeline: A #CoglPipeline object
 * @state: A destination #CoglDepthState struct
 *
 * Retrieves the current depth state configuration for the given
 * @pipeline as previously set using cogl_pipeline_set_depth_state().
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_pipeline_get_depth_state (CoglPipeline *pipeline,
                               CoglDepthState *state_out);

/**
 * CoglPipelineLayerCallback:
 * @pipeline: The #CoglPipeline whos layers are being iterated
 * @layer_index: The current layer index
 * @user_data: The private data passed to cogl_pipeline_foreach_layer()
 *
 * The callback prototype used with cogl_pipeline_foreach_layer() for
 * iterating all the layers of a @pipeline.
 *
 * Since: 2.0
 * Stability: Unstable
 */
typedef gboolean (*CoglPipelineLayerCallback) (CoglPipeline *pipeline,
                                               int layer_index,
                                               void *user_data);

/**
 * cogl_pipeline_foreach_layer:
 * @pipeline: A #CoglPipeline object
 * @callback: A #CoglPipelineLayerCallback to be called for each layer
 *            index
 * @user_data: Private data that will be passed to the callback
 *
 * Iterates all the layer indices of the given @pipeline.
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_pipeline_foreach_layer (CoglPipeline *pipeline,
                             CoglPipelineLayerCallback callback,
                             void *user_data);

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

G_END_DECLS

#endif /* __COGL_PIPELINE_H__ */
