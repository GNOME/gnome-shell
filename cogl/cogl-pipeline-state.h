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

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_PIPELINE_STATE_H__
#define __COGL_PIPELINE_STATE_H__

#include <cogl/cogl-pipeline.h>
#include <cogl/cogl-color.h>
#include <glib.h>

G_BEGIN_DECLS

#ifdef COGL_ENABLE_EXPERIMENTAL_API

#define cogl_pipeline_set_color cogl_pipeline_set_color_EXP
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

#define cogl_pipeline_set_color4ub cogl_pipeline_set_color4ub_EXP
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
			    guint8        red,
                            guint8        green,
                            guint8        blue,
                            guint8        alpha);

#define cogl_pipeline_set_color4f cogl_pipeline_set_color4f_EXP
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

#define cogl_pipeline_get_color cogl_pipeline_get_color_EXP
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

#define cogl_pipeline_set_ambient cogl_pipeline_set_ambient_EXP
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

#define cogl_pipeline_get_ambient cogl_pipeline_get_ambient_EXP
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

#define cogl_pipeline_set_diffuse cogl_pipeline_set_diffuse_EXP
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

#define cogl_pipeline_get_diffuse cogl_pipeline_get_diffuse_EXP
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

#define cogl_pipeline_set_ambient_and_diffuse \
  cogl_pipeline_set_ambient_and_diffuse_EXP
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

#define cogl_pipeline_set_specular cogl_pipeline_set_specular_EXP
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

#define cogl_pipeline_get_specular cogl_pipeline_get_specular_EXP
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

#define cogl_pipeline_set_shininess cogl_pipeline_set_shininess_EXP
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

#define cogl_pipeline_get_shininess cogl_pipeline_get_shininess_EXP
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

#define cogl_pipeline_set_emission cogl_pipeline_set_emission_EXP
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

#define cogl_pipeline_get_emission cogl_pipeline_get_emission_EXP
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

#define cogl_pipeline_set_alpha_test_function \
  cogl_pipeline_set_alpha_test_function_EXP
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

#define cogl_pipeline_get_alpha_test_function \
  cogl_pipeline_get_alpha_test_function_EXP
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

#define cogl_pipeline_get_alpha_test_reference \
  cogl_pipeline_get_alpha_test_reference_EXP
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

#define cogl_pipeline_set_blend cogl_pipeline_set_blend_EXP
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
gboolean
cogl_pipeline_set_blend (CoglPipeline *pipeline,
                         const char   *blend_string,
                         GError      **error);

#define cogl_pipeline_set_blend_constant cogl_pipeline_set_blend_constant_EXP
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

#define cogl_pipeline_set_point_size cogl_pipeline_set_point_size_EXP
/**
 * cogl_pipeline_set_point_size:
 * @pipeline: a #CoglPipeline pointer
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
 * Stability: Unstable
 */
void
cogl_pipeline_set_point_size (CoglPipeline *pipeline,
                              float point_size);

#define cogl_pipeline_get_point_size cogl_pipeline_get_point_size_EXP
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

#define cogl_pipeline_get_user_program cogl_pipeline_get_user_program_EXP
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

#define cogl_pipeline_set_user_program cogl_pipeline_set_user_program_EXP
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

#define cogl_pipeline_set_depth_state cogl_pipeline_set_depth_state_EXP
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

#define cogl_pipeline_get_depth_state cogl_pipeline_get_depth_state_EXP
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

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

G_END_DECLS

#endif /* __COGL_PIPELINE_STATE_H__ */
