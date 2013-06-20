/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2011 Intel Corporation.
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

#ifndef __COGL_PIPELINE_STATE_H__
#define __COGL_PIPELINE_STATE_H__

#include <cogl/cogl-pipeline.h>
#include <cogl/cogl-color.h>
#include <cogl/cogl-depth-state.h>

COGL_BEGIN_DECLS

#ifdef COGL_ENABLE_EXPERIMENTAL_API

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
 * Stability: Unstable
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
 * Stability: Unstable
 */
void
cogl_pipeline_set_color4ub (CoglPipeline *pipeline,
			    uint8_t red,
                            uint8_t green,
                            uint8_t blue,
                            uint8_t alpha);

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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
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
 * Stability: Unstable
 */
float
cogl_pipeline_get_alpha_test_reference (CoglPipeline *pipeline);

/**
 * cogl_pipeline_set_blend:
 * @pipeline: A #CoglPipeline object
 * @blend_string: A <link linkend="cogl-Blend-Strings">Cogl blend string</link>
 *   describing the desired blend function.
 * @error: return location for a #CoglError that may report lack of driver
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
 * Stability: Unstable
 */
CoglBool
cogl_pipeline_set_blend (CoglPipeline *pipeline,
                         const char   *blend_string,
                         CoglError      **error);

/**
 * cogl_pipeline_set_blend_constant:
 * @pipeline: A #CoglPipeline object
 * @constant_color: The constant color you want
 *
 * When blending is setup to reference a CONSTANT blend factor then
 * blending will depend on the constant set with this function.
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_pipeline_set_blend_constant (CoglPipeline *pipeline,
                                  const CoglColor *constant_color);

/**
 * cogl_pipeline_set_point_size:
 * @pipeline: a #CoglPipeline pointer
 * @point_size: the new point size.
 *
 * Changes the size of points drawn when %COGL_VERTICES_MODE_POINTS is
 * used with the attribute buffer API. Note that typically the GPU
 * will only support a limited minimum and maximum range of point
 * sizes. If the chosen point size is outside that range then the
 * nearest value within that range will be used instead. The size of a
 * point is in screen space so it will be the same regardless of any
 * transformations.
 *
 * If the point size is set to 0.0 then drawing points with the
 * pipeline will have undefined results. This is the default value so
 * if an application wants to draw points it must make sure to use a
 * pipeline that has an explicit point size set on it.
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_pipeline_set_point_size (CoglPipeline *pipeline,
                              float point_size);

/**
 * cogl_pipeline_get_point_size:
 * @pipeline: a #CoglPipeline pointer
 *
 * Get the size of points drawn when %COGL_VERTICES_MODE_POINTS is
 * used with the vertex buffer API.
 *
 * Return value: the point size of the @pipeline.
 *
 * Since: 2.0
 * Stability: Unstable
 */
float
cogl_pipeline_get_point_size (CoglPipeline *pipeline);

/**
 * cogl_pipeline_set_per_vertex_point_size:
 * @pipeline: a #CoglPipeline pointer
 * @enable: whether to enable per-vertex point size
 * @error: a location to store a #CoglError if the change failed
 *
 * Sets whether to use a per-vertex point size or to use the value set
 * by cogl_pipeline_set_point_size(). If per-vertex point size is
 * enabled then the point size can be set for an individual point
 * either by drawing with a #CoglAttribute with the name
 * ‘cogl_point_size_in’ or by writing to the GLSL builtin
 * ‘cogl_point_size_out’ from a vertex shader snippet.
 *
 * If per-vertex point size is enabled and this attribute is not used
 * and cogl_point_size_out is not written to then the results are
 * undefined.
 *
 * Note that enabling this will only work if the
 * %COGL_FEATURE_ID_PER_VERTEX_POINT_SIZE feature is available. If
 * this is not available then the function will return %FALSE and set
 * a #CoglError.
 *
 * Since: 2.0
 * Stability: Unstable
 * Return value: %TRUE if the change suceeded or %FALSE otherwise
 */
CoglBool
cogl_pipeline_set_per_vertex_point_size (CoglPipeline *pipeline,
                                         CoglBool enable,
                                         CoglError **error);

/**
 * cogl_pipeline_get_per_vertex_point_size:
 * @pipeline: a #CoglPipeline pointer
 *
 * Since: 2.0
 * Stability: Unstable
 * Return value: %TRUE if the pipeline has per-vertex point size
 *   enabled or %FALSE otherwise. The per-vertex point size can be
 *   enabled with cogl_pipeline_set_per_vertex_point_size().
 */
CoglBool
cogl_pipeline_get_per_vertex_point_size (CoglPipeline *pipeline);

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
 * Stability: Unstable
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
 * Stability: Unstable
 */
void
cogl_pipeline_set_user_program (CoglPipeline *pipeline,
                                CoglHandle program);

/**
 * cogl_pipeline_set_depth_state:
 * @pipeline: A #CoglPipeline object
 * @state: A #CoglDepthState struct
 * @error: A #CoglError to report failures to setup the given @state.
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
CoglBool
cogl_pipeline_set_depth_state (CoglPipeline *pipeline,
                               const CoglDepthState *state,
                               CoglError **error);

/**
 * cogl_pipeline_get_depth_state
 * @pipeline: A #CoglPipeline object
 * @state_out: (out): A destination #CoglDepthState struct
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
 * CoglPipelineCullFaceMode:
 * @COGL_PIPELINE_CULL_FACE_MODE_NONE: Neither face will be
 *  culled. This is the default.
 * @COGL_PIPELINE_CULL_FACE_MODE_FRONT: Front faces will be culled.
 * @COGL_PIPELINE_CULL_FACE_MODE_BACK: Back faces will be culled.
 * @COGL_PIPELINE_CULL_FACE_MODE_BOTH: All faces will be culled.
 *
 * Specifies which faces should be culled. This can be set on a
 * pipeline using cogl_pipeline_set_cull_face_mode().
 */
typedef enum
{
  COGL_PIPELINE_CULL_FACE_MODE_NONE,
  COGL_PIPELINE_CULL_FACE_MODE_FRONT,
  COGL_PIPELINE_CULL_FACE_MODE_BACK,
  COGL_PIPELINE_CULL_FACE_MODE_BOTH
} CoglPipelineCullFaceMode;

/**
 * cogl_pipeline_set_cull_face_mode:
 * @pipeline: A #CoglPipeline
 * @cull_face_mode: The new mode to set
 *
 * Sets which faces will be culled when drawing. Face culling can be
 * used to increase efficiency by avoiding drawing faces that would
 * get overridden. For example, if a model has gaps so that it is
 * impossible to see the inside then faces which are facing away from
 * the screen will never be seen so there is no point in drawing
 * them. This can be acheived by setting the cull face mode to
 * %COGL_PIPELINE_CULL_FACE_MODE_BACK.
 *
 * Face culling relies on the primitives being drawn with a specific
 * order to represent which faces are facing inside and outside the
 * model. This order can be specified by calling
 * cogl_pipeline_set_front_face_winding().
 *
 * Status: Unstable
 * Since: 2.0
 */
void
cogl_pipeline_set_cull_face_mode (CoglPipeline *pipeline,
                                  CoglPipelineCullFaceMode cull_face_mode);

/**
 * cogl_pipeline_get_cull_face_mode:
 *
 * Return value: the cull face mode that was previously set with
 * cogl_pipeline_set_cull_face_mode().
 *
 * Status: Unstable
 * Since: 2.0
 */
CoglPipelineCullFaceMode
cogl_pipeline_get_cull_face_mode (CoglPipeline *pipeline);

/**
 * cogl_pipeline_set_front_face_winding:
 * @pipeline: a #CoglPipeline
 * @front_winding: the winding order
 *
 * The order of the vertices within a primitive specifies whether it
 * is considered to be front or back facing. This function specifies
 * which order is considered to be the front
 * faces. %COGL_WINDING_COUNTER_CLOCKWISE sets the front faces to
 * primitives with vertices in a counter-clockwise order and
 * %COGL_WINDING_CLOCKWISE sets them to be clockwise. The default is
 * %COGL_WINDING_COUNTER_CLOCKWISE.
 *
 * Status: Unstable
 * Since: 2.0
 */
void
cogl_pipeline_set_front_face_winding (CoglPipeline *pipeline,
                                      CoglWinding front_winding);

/**
 * cogl_pipeline_get_front_face_winding:
 * @pipeline: a #CoglPipeline
 *
 * The order of the vertices within a primitive specifies whether it
 * is considered to be front or back facing. This function specifies
 * which order is considered to be the front
 * faces. %COGL_WINDING_COUNTER_CLOCKWISE sets the front faces to
 * primitives with vertices in a counter-clockwise order and
 * %COGL_WINDING_CLOCKWISE sets them to be clockwise. The default is
 * %COGL_WINDING_COUNTER_CLOCKWISE.
 *
 * Returns: The @pipeline front face winding
 *
 * Status: Unstable
 * Since: 2.0
 */
CoglWinding
cogl_pipeline_get_front_face_winding (CoglPipeline *pipeline);

/**
 * cogl_pipeline_set_uniform_1f:
 * @pipeline: A #CoglPipeline object
 * @uniform_location: The uniform's location identifier
 * @value: The new value for the uniform
 *
 * Sets a new value for the uniform at @uniform_location. If this
 * pipeline has a user program attached and is later used as a source
 * for drawing, the given value will be assigned to the uniform which
 * can be accessed from the shader's source. The value for
 * @uniform_location should be retrieved from the string name of the
 * uniform by calling cogl_pipeline_get_uniform_location().
 *
 * This function should be used to set uniforms that are of type
 * float. It can also be used to set a single member of a float array
 * uniform.
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_pipeline_set_uniform_1f (CoglPipeline *pipeline,
                              int uniform_location,
                              float value);

/**
 * cogl_pipeline_set_uniform_1i:
 * @pipeline: A #CoglPipeline object
 * @uniform_location: The uniform's location identifier
 * @value: The new value for the uniform
 *
 * Sets a new value for the uniform at @uniform_location. If this
 * pipeline has a user program attached and is later used as a source
 * for drawing, the given value will be assigned to the uniform which
 * can be accessed from the shader's source. The value for
 * @uniform_location should be retrieved from the string name of the
 * uniform by calling cogl_pipeline_get_uniform_location().
 *
 * This function should be used to set uniforms that are of type
 * int. It can also be used to set a single member of a int array
 * uniform or a sampler uniform.
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_pipeline_set_uniform_1i (CoglPipeline *pipeline,
                              int uniform_location,
                              int value);

/**
 * cogl_pipeline_set_uniform_float:
 * @pipeline: A #CoglPipeline object
 * @uniform_location: The uniform's location identifier
 * @n_components: The number of components in the corresponding uniform's type
 * @count: The number of values to set
 * @value: Pointer to the new values to set
 *
 * Sets new values for the uniform at @uniform_location. If this
 * pipeline has a user program attached and is later used as a source
 * for drawing, the given values will be assigned to the uniform which
 * can be accessed from the shader's source. The value for
 * @uniform_location should be retrieved from the string name of the
 * uniform by calling cogl_pipeline_get_uniform_location().
 *
 * This function can be used to set any floating point type uniform,
 * including float arrays and float vectors. For example, to set a
 * single vec4 uniform you would use 4 for @n_components and 1 for
 * @count. To set an array of 8 float values, you could use 1 for
 * @n_components and 8 for @count.
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_pipeline_set_uniform_float (CoglPipeline *pipeline,
                                 int uniform_location,
                                 int n_components,
                                 int count,
                                 const float *value);

/**
 * cogl_pipeline_set_uniform_int:
 * @pipeline: A #CoglPipeline object
 * @uniform_location: The uniform's location identifier
 * @n_components: The number of components in the corresponding uniform's type
 * @count: The number of values to set
 * @value: Pointer to the new values to set
 *
 * Sets new values for the uniform at @uniform_location. If this
 * pipeline has a user program attached and is later used as a source
 * for drawing, the given values will be assigned to the uniform which
 * can be accessed from the shader's source. The value for
 * @uniform_location should be retrieved from the string name of the
 * uniform by calling cogl_pipeline_get_uniform_location().
 *
 * This function can be used to set any integer type uniform,
 * including int arrays and int vectors. For example, to set a single
 * ivec4 uniform you would use 4 for @n_components and 1 for
 * @count. To set an array of 8 int values, you could use 1 for
 * @n_components and 8 for @count.
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_pipeline_set_uniform_int (CoglPipeline *pipeline,
                               int uniform_location,
                               int n_components,
                               int count,
                               const int *value);

/**
 * cogl_pipeline_set_uniform_matrix:
 * @pipeline: A #CoglPipeline object
 * @uniform_location: The uniform's location identifier
 * @dimensions: The size of the matrix
 * @count: The number of values to set
 * @transpose: Whether to transpose the matrix
 * @value: Pointer to the new values to set
 *
 * Sets new values for the uniform at @uniform_location. If this
 * pipeline has a user program attached and is later used as a source
 * for drawing, the given values will be assigned to the uniform which
 * can be accessed from the shader's source. The value for
 * @uniform_location should be retrieved from the string name of the
 * uniform by calling cogl_pipeline_get_uniform_location().
 *
 * This function can be used to set any matrix type uniform, including
 * matrix arrays. For example, to set a single mat4 uniform you would
 * use 4 for @dimensions and 1 for @count. To set an array of 8
 * mat3 values, you could use 3 for @dimensions and 8 for @count.
 *
 * If @transpose is %FALSE then the matrix is expected to be in
 * column-major order or if it is %TRUE then the matrix is in
 * row-major order. You can pass a #CoglMatrix by calling by passing
 * the result of cogl_matrix_get_array() in @value and setting
 * @transpose to %FALSE.
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_pipeline_set_uniform_matrix (CoglPipeline *pipeline,
                                  int uniform_location,
                                  int dimensions,
                                  int count,
                                  CoglBool transpose,
                                  const float *value);

/**
 * cogl_pipeline_add_snippet:
 * @pipeline: A #CoglPipeline
 * @snippet: The #CoglSnippet to add to the vertex processing hook
 *
 * Adds a shader snippet to @pipeline. The snippet will wrap around or
 * replace some part of the pipeline as defined by the hook point in
 * @snippet. Note that some hook points are specific to a layer and
 * must be added with cogl_pipeline_add_layer_snippet() instead.
 *
 * Since: 1.10
 * Stability: Unstable
 */
void
cogl_pipeline_add_snippet (CoglPipeline *pipeline,
                           CoglSnippet *snippet);

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

COGL_END_DECLS

#endif /* __COGL_PIPELINE_STATE_H__ */
