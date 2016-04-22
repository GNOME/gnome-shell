/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_MATERIAL_H__
#define __COGL_MATERIAL_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-matrix.h>
#include <cogl/cogl-depth-state.h>
#include <cogl/cogl-error.h>
#include <cogl/cogl-macros.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-material
 * @short_description: Fuctions for creating and manipulating materials
 *
 * COGL allows creating and manipulating materials used to fill in
 * geometry. Materials may simply be lighting attributes (such as an
 * ambient and diffuse colour) or might represent one or more textures
 * blended together.
 */

typedef struct _CoglMaterial	      CoglMaterial;
typedef struct _CoglMaterialLayer     CoglMaterialLayer;

#define COGL_MATERIAL(OBJECT) ((CoglMaterial *)OBJECT)

/**
 * CoglMaterialFilter:
 * @COGL_MATERIAL_FILTER_NEAREST: Measuring in manhatten distance from the,
 *   current pixel center, use the nearest texture texel
 * @COGL_MATERIAL_FILTER_LINEAR: Use the weighted average of the 4 texels
 *   nearest the current pixel center
 * @COGL_MATERIAL_FILTER_NEAREST_MIPMAP_NEAREST: Select the mimap level whose
 *   texel size most closely matches the current pixel, and use the
 *   %COGL_MATERIAL_FILTER_NEAREST criterion
 * @COGL_MATERIAL_FILTER_LINEAR_MIPMAP_NEAREST: Select the mimap level whose
 *   texel size most closely matches the current pixel, and use the
 *   %COGL_MATERIAL_FILTER_LINEAR criterion
 * @COGL_MATERIAL_FILTER_NEAREST_MIPMAP_LINEAR: Select the two mimap levels
 *   whose texel size most closely matches the current pixel, use
 *   the %COGL_MATERIAL_FILTER_NEAREST criterion on each one and take
 *   their weighted average
 * @COGL_MATERIAL_FILTER_LINEAR_MIPMAP_LINEAR: Select the two mimap levels
 *   whose texel size most closely matches the current pixel, use
 *   the %COGL_MATERIAL_FILTER_LINEAR criterion on each one and take
 *   their weighted average
 *
 * Texture filtering is used whenever the current pixel maps either to more
 * than one texture element (texel) or less than one. These filter enums
 * correspond to different strategies used to come up with a pixel color, by
 * possibly referring to multiple neighbouring texels and taking a weighted
 * average or simply using the nearest texel.
 */
typedef enum {
  COGL_MATERIAL_FILTER_NEAREST = 0x2600,
  COGL_MATERIAL_FILTER_LINEAR = 0x2601,
  COGL_MATERIAL_FILTER_NEAREST_MIPMAP_NEAREST = 0x2700,
  COGL_MATERIAL_FILTER_LINEAR_MIPMAP_NEAREST = 0x2701,
  COGL_MATERIAL_FILTER_NEAREST_MIPMAP_LINEAR = 0x2702,
  COGL_MATERIAL_FILTER_LINEAR_MIPMAP_LINEAR = 0x2703
} CoglMaterialFilter;
/* NB: these values come from the equivalents in gl.h */

/**
 * CoglMaterialWrapMode:
 * @COGL_MATERIAL_WRAP_MODE_REPEAT: The texture will be repeated. This
 *   is useful for example to draw a tiled background.
 * @COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE: The coordinates outside the
 *   range 0→1 will sample copies of the edge pixels of the
 *   texture. This is useful to avoid artifacts if only one copy of
 *   the texture is being rendered.
 * @COGL_MATERIAL_WRAP_MODE_AUTOMATIC: Cogl will try to automatically
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
 * anything but %COGL_MATERIAL_FILTER_NEAREST then texels outside the
 * range 0→1 might be used even when the coordinate is exactly 0 or 1
 * because OpenGL will try to sample neighbouring pixels. For example
 * if you are trying to render the full texture then you may get
 * artifacts around the edges when the pixels from the other side are
 * merged in if the wrap mode is set to repeat.
 *
 * Since: 1.4
 */
/* GL_ALWAYS is just used here as a value that is known not to clash
 * with any valid GL wrap modes
 *
 * XXX: keep the values in sync with the CoglMaterialWrapModeInternal
 * enum so no conversion is actually needed.
 */
typedef enum {
  COGL_MATERIAL_WRAP_MODE_REPEAT = 0x2901,
  COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE = 0x812F,
  COGL_MATERIAL_WRAP_MODE_AUTOMATIC = 0x0207
} CoglMaterialWrapMode;
/* NB: these values come from the equivalents in gl.h */

/**
 * cogl_material_new:
 *
 * Allocates and initializes a blank white material
 *
 * Return value: a pointer to a new #CoglMaterial
 * Deprecated: 1.16: Use cogl_pipeline_new() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_new)
CoglMaterial *
cogl_material_new (void);

/**
 * cogl_material_copy:
 * @source: a #CoglMaterial object to copy
 *
 * Creates a new material with the configuration copied from the
 * source material.
 *
 * We would strongly advise developers to always aim to use
 * cogl_material_copy() instead of cogl_material_new() whenever there will
 * be any similarity between two materials. Copying a material helps Cogl
 * keep track of a materials ancestry which we may use to help minimize GPU
 * state changes.
 *
 * Returns: a pointer to the newly allocated #CoglMaterial
 *
 * Since: 1.2
 * Deprecated: 1.16: Use cogl_pipeline_copy() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_copy)
CoglMaterial *
cogl_material_copy (CoglMaterial *source);

/**
 * cogl_material_ref:
 * @material: a #CoglMaterial object.
 *
 * Increment the reference count for a #CoglMaterial.
 *
 * Return value: the @material.
 *
 * Since: 1.0
 *
 * Deprecated: 1.2: Use cogl_object_ref() instead
 */
COGL_DEPRECATED
CoglHandle
cogl_material_ref (CoglHandle material);

/**
 * cogl_material_unref:
 * @material: a #CoglMaterial object.
 *
 * Decrement the reference count for a #CoglMaterial.
 *
 * Since: 1.0
 *
 * Deprecated: 1.2: Use cogl_object_unref() instead
 */
COGL_DEPRECATED
void
cogl_material_unref (CoglHandle material);

/**
 * cogl_is_material:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing material object.
 *
 * Return value: %TRUE if the handle references a #CoglMaterial,
 *   %FALSE otherwise
 * Deprecated: 1.16: Use cogl_is_pipeline() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_is_pipeline)
CoglBool
cogl_is_material (CoglHandle handle);

/**
 * cogl_material_set_color:
 * @material: A #CoglMaterial object
 * @color: The components of the color
 *
 * Sets the basic color of the material, used when no lighting is enabled.
 *
 * Note that if you don't add any layers to the material then the color
 * will be blended unmodified with the destination; the default blend
 * expects premultiplied colors: for example, use (0.5, 0.0, 0.0, 0.5) for
 * semi-transparent red. See cogl_color_premultiply().
 *
 * The default value is (1.0, 1.0, 1.0, 1.0)
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pipeline_set_color() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_color)
void
cogl_material_set_color (CoglMaterial    *material,
                         const CoglColor *color);

/**
 * cogl_material_set_color4ub:
 * @material: A #CoglMaterial object
 * @red: The red component
 * @green: The green component
 * @blue: The blue component
 * @alpha: The alpha component
 *
 * Sets the basic color of the material, used when no lighting is enabled.
 *
 * The default value is (0xff, 0xff, 0xff, 0xff)
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pipeline_set_color4ub() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_color4ub)
void
cogl_material_set_color4ub (CoglMaterial *material,
			    uint8_t red,
                            uint8_t green,
                            uint8_t blue,
                            uint8_t alpha);

/**
 * cogl_material_set_color4f:
 * @material: A #CoglMaterial object
 * @red: The red component
 * @green: The green component
 * @blue: The blue component
 * @alpha: The alpha component
 *
 * Sets the basic color of the material, used when no lighting is enabled.
 *
 * The default value is (1.0, 1.0, 1.0, 1.0)
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pipeline_set_color4f() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_color4f)
void
cogl_material_set_color4f (CoglMaterial *material,
                           float         red,
                           float         green,
                           float         blue,
                           float         alpha);

/**
 * cogl_material_get_color:
 * @material: A #CoglMaterial object
 * @color: (out): The location to store the color
 *
 * Retrieves the current material color.
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pipeline_get_color() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_get_color)
void
cogl_material_get_color (CoglMaterial *material,
                         CoglColor    *color);

/**
 * cogl_material_set_ambient:
 * @material: A #CoglMaterial object
 * @ambient: The components of the desired ambient color
 *
 * Sets the material's ambient color, in the standard OpenGL lighting
 * model. The ambient color affects the overall color of the object.
 *
 * Since the diffuse color will be intense when the light hits the surface
 * directly, the ambient will be most apparent where the light hits at a
 * slant.
 *
 * The default value is (0.2, 0.2, 0.2, 1.0)
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglSnippet shader api for lighting
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_material_set_ambient (CoglMaterial    *material,
			   const CoglColor *ambient);

/**
 * cogl_material_get_ambient:
 * @material: A #CoglMaterial object
 * @ambient: The location to store the ambient color
 *
 * Retrieves the current ambient color for @material
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglSnippet shader api for lighting
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_material_get_ambient (CoglMaterial *material,
                           CoglColor    *ambient);

/**
 * cogl_material_set_diffuse:
 * @material: A #CoglMaterial object
 * @diffuse: The components of the desired diffuse color
 *
 * Sets the material's diffuse color, in the standard OpenGL lighting
 * model. The diffuse color is most intense where the light hits the
 * surface directly - perpendicular to the surface.
 *
 * The default value is (0.8, 0.8, 0.8, 1.0)
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglSnippet shader api for lighting
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_material_set_diffuse (CoglMaterial    *material,
			   const CoglColor *diffuse);

/**
 * cogl_material_get_diffuse:
 * @material: A #CoglMaterial object
 * @diffuse: The location to store the diffuse color
 *
 * Retrieves the current diffuse color for @material
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglSnippet shader api for lighting
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_material_get_diffuse (CoglMaterial *material,
                           CoglColor    *diffuse);

/**
 * cogl_material_set_ambient_and_diffuse:
 * @material: A #CoglMaterial object
 * @color: The components of the desired ambient and diffuse colors
 *
 * Conveniently sets the diffuse and ambient color of @material at the same
 * time. See cogl_material_set_ambient() and cogl_material_set_diffuse().
 *
 * The default ambient color is (0.2, 0.2, 0.2, 1.0)
 *
 * The default diffuse color is (0.8, 0.8, 0.8, 1.0)
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglSnippet shader api for lighting
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_material_set_ambient_and_diffuse (CoglMaterial    *material,
				       const CoglColor *color);

/**
 * cogl_material_set_specular:
 * @material: A #CoglMaterial object
 * @specular: The components of the desired specular color
 *
 * Sets the material's specular color, in the standard OpenGL lighting
 * model. The intensity of the specular color depends on the viewport
 * position, and is brightest along the lines of reflection.
 *
 * The default value is (0.0, 0.0, 0.0, 1.0)
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglSnippet shader api for lighting
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_material_set_specular (CoglMaterial    *material,
			    const CoglColor *specular);

/**
 * cogl_material_get_specular:
 * @material: A #CoglMaterial object
 * @specular: The location to store the specular color
 *
 * Retrieves the materials current specular color.
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglSnippet shader api for lighting
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_material_get_specular (CoglMaterial *material,
                            CoglColor    *specular);

/**
 * cogl_material_set_shininess:
 * @material: A #CoglMaterial object
 * @shininess: The desired shininess; must be >= 0.0
 *
 * Sets the shininess of the material, in the standard OpenGL lighting
 * model, which determines the size of the specular highlights. A
 * higher @shininess will produce smaller highlights which makes the
 * object appear more shiny.
 *
 * The default value is 0.0
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglSnippet shader api for lighting
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_material_set_shininess (CoglMaterial *material,
			     float         shininess);

/**
 * cogl_material_get_shininess:
 * @material: A #CoglMaterial object
 *
 * Retrieves the materials current emission color.
 *
 * Return value: The materials current shininess value
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglSnippet shader api for lighting
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
float
cogl_material_get_shininess (CoglMaterial *material);

/**
 * cogl_material_set_emission:
 * @material: A #CoglMaterial object
 * @emission: The components of the desired emissive color
 *
 * Sets the material's emissive color, in the standard OpenGL lighting
 * model. It will look like the surface is a light source emitting this
 * color.
 *
 * The default value is (0.0, 0.0, 0.0, 1.0)
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglSnippet shader api for lighting
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_material_set_emission (CoglMaterial    *material,
			    const CoglColor *emission);

/**
 * cogl_material_get_emission:
 * @material: A #CoglMaterial object
 * @emission: The location to store the emission color
 *
 * Retrieves the materials current emission color.
 *
 * Since: 1.0
 * Deprecated: 1.16: Use the #CoglSnippet shader api for lighting
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_material_get_emission (CoglMaterial *material,
                            CoglColor    *emission);

/**
 * CoglMaterialAlphaFunc:
 * @COGL_MATERIAL_ALPHA_FUNC_NEVER: Never let the fragment through.
 * @COGL_MATERIAL_ALPHA_FUNC_LESS: Let the fragment through if the incoming
 *   alpha value is less than the reference alpha value
 * @COGL_MATERIAL_ALPHA_FUNC_EQUAL: Let the fragment through if the incoming
 *   alpha value equals the reference alpha value
 * @COGL_MATERIAL_ALPHA_FUNC_LEQUAL: Let the fragment through if the incoming
 *   alpha value is less than or equal to the reference alpha value
 * @COGL_MATERIAL_ALPHA_FUNC_GREATER: Let the fragment through if the incoming
 *   alpha value is greater than the reference alpha value
 * @COGL_MATERIAL_ALPHA_FUNC_NOTEQUAL: Let the fragment through if the incoming
 *   alpha value does not equal the reference alpha value
 * @COGL_MATERIAL_ALPHA_FUNC_GEQUAL: Let the fragment through if the incoming
 *   alpha value is greater than or equal to the reference alpha value.
 * @COGL_MATERIAL_ALPHA_FUNC_ALWAYS: Always let the fragment through.
 *
 * Alpha testing happens before blending primitives with the framebuffer and
 * gives an opportunity to discard fragments based on a comparison with the
 * incoming alpha value and a reference alpha value. The #CoglMaterialAlphaFunc
 * determines how the comparison is done.
 */
typedef enum {
  COGL_MATERIAL_ALPHA_FUNC_NEVER    = 0x0200,
  COGL_MATERIAL_ALPHA_FUNC_LESS	    = 0x0201,
  COGL_MATERIAL_ALPHA_FUNC_EQUAL    = 0x0202,
  COGL_MATERIAL_ALPHA_FUNC_LEQUAL   = 0x0203,
  COGL_MATERIAL_ALPHA_FUNC_GREATER  = 0x0204,
  COGL_MATERIAL_ALPHA_FUNC_NOTEQUAL = 0x0205,
  COGL_MATERIAL_ALPHA_FUNC_GEQUAL   = 0x0206,
  COGL_MATERIAL_ALPHA_FUNC_ALWAYS   = 0x0207
} CoglMaterialAlphaFunc;

/**
 * cogl_material_set_alpha_test_function:
 * @material: A #CoglMaterial object
 * @alpha_func: A @CoglMaterialAlphaFunc constant
 * @alpha_reference: A reference point that the chosen alpha function uses
 *   to compare incoming fragments to.
 *
 * Before a primitive is blended with the framebuffer, it goes through an
 * alpha test stage which lets you discard fragments based on the current
 * alpha value. This function lets you change the function used to evaluate
 * the alpha channel, and thus determine which fragments are discarded
 * and which continue on to the blending stage.
 *
 * The default is %COGL_MATERIAL_ALPHA_FUNC_ALWAYS
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pipeline_set_alpha_test_function() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_alpha_test_function)
void
cogl_material_set_alpha_test_function (CoglMaterial         *material,
				       CoglMaterialAlphaFunc alpha_func,
				       float                 alpha_reference);

/**
 * cogl_material_set_blend:
 * @material: A #CoglMaterial object
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
 * <warning>The brackets around blend factors are currently not
 * optional!</warning>
 *
 * This is the list of source-names usable as blend factors:
 * <itemizedlist>
 *   <listitem><para>SRC_COLOR: The color of the in comming fragment</para></listitem>
 *   <listitem><para>DST_COLOR: The color of the framebuffer</para></listitem>
 *   <listitem><para>CONSTANT: The constant set via cogl_material_set_blend_constant()</para></listitem>
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
 * That gives normal alpha-blending when the calculated color for the material
 * is in premultiplied form.
 *
 * Return value: %TRUE if the blend string was successfully parsed, and the
 *   described blending is supported by the underlying driver/hardware. If
 *   there was an error, %FALSE is returned and @error is set accordingly (if
 *   present).
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pipeline_set_blend() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_blend)
CoglBool
cogl_material_set_blend (CoglMaterial *material,
                         const char   *blend_string,
                         CoglError   **error);

/**
 * cogl_material_set_blend_constant:
 * @material: A #CoglMaterial object
 * @constant_color: The constant color you want
 *
 * When blending is setup to reference a CONSTANT blend factor then
 * blending will depend on the constant set with this function.
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pipeline_set_blend_constant() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_blend_constant)
void
cogl_material_set_blend_constant (CoglMaterial *material,
                                  const CoglColor *constant_color);

/**
 * cogl_material_set_point_size:
 * @material: a material.
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
 * Since: 1.4
 * Deprecated: 1.16: Use cogl_pipeline_set_point_size() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_point_size)
void
cogl_material_set_point_size (CoglMaterial *material,
                              float         point_size);

/**
 * cogl_material_get_point_size:
 * @material: a #CoglHandle to a material.
 *
 * Get the size of points drawn when %COGL_VERTICES_MODE_POINTS is
 * used with the vertex buffer API.
 *
 * Return value: the point size of the material.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use cogl_pipeline_get_point_size() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_get_point_size)
float
cogl_material_get_point_size (CoglMaterial *material);

/**
 * cogl_material_get_user_program:
 * @material: a #CoglMaterial object.
 *
 * Queries what user program has been associated with the given
 * @material using cogl_material_set_user_program().
 *
 * Return value: (transfer none): The current user program
 *   or %COGL_INVALID_HANDLE.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use #CoglSnippet api instead instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
CoglHandle
cogl_material_get_user_program (CoglMaterial *material);

/**
 * cogl_material_set_user_program:
 * @material: a #CoglMaterial object.
 * @program: A #CoglHandle to a linked CoglProgram
 *
 * Associates a linked CoglProgram with the given material so that the
 * program can take full control of vertex and/or fragment processing.
 *
 * This is an example of how it can be used to associate an ARBfp
 * program with a #CoglMaterial:
 * |[
 * CoglHandle shader;
 * CoglHandle program;
 * CoglMaterial *material;
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
 * material = cogl_material_new ();
 * cogl_material_set_user_program (material, program);
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
 * Since: 1.4
 * Deprecated: 1.16: Use #CoglSnippet api instead instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_snippet_)
void
cogl_material_set_user_program (CoglMaterial *material,
                                CoglHandle program);

/**
 * cogl_material_set_layer:
 * @material: A #CoglMaterial object
 * @layer_index: the index of the layer
 * @texture: a #CoglHandle for the layer object
 *
 * In addition to the standard OpenGL lighting model a Cogl material may have
 * one or more layers comprised of textures that can be blended together in
 * order, with a number of different texture combine modes. This function
 * defines a new texture layer.
 *
 * The index values of multiple layers do not have to be consecutive; it is
 * only their relative order that is important.
 *
 * <note>In the future, we may define other types of material layers, such
 * as purely GLSL based layers.</note>
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pipeline_set_layer() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_layer)
void
cogl_material_set_layer (CoglMaterial *material,
			 int           layer_index,
			 CoglHandle    texture);

/**
 * cogl_material_remove_layer:
 * @material: A #CoglMaterial object
 * @layer_index: Specifies the layer you want to remove
 *
 * This function removes a layer from your material
 * Deprecated: 1.16: Use cogl_pipeline_remove_layer() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_remove_layer)
void
cogl_material_remove_layer (CoglMaterial *material,
			    int           layer_index);


/**
 * cogl_material_set_layer_combine:
 * @material: A #CoglMaterial object
 * @layer_index: Specifies the layer you want define a combine function for
 * @blend_string: A <link linkend="cogl-Blend-Strings">Cogl blend string</link>
 *    describing the desired texture combine function.
 * @error: A #CoglError that may report parse errors or lack of GPU/driver
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
 *     cogl_material_set_layer_constant()</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>PRIMARY</term>
 *     <listitem>Use the color of the material as set with
 *     cogl_material_set_color()</listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>PREVIOUS</term>
 *     <listitem>Either use the texture color from the previous layer, or
 *     if this is layer 0, use the color of the material as set with
 *     cogl_material_set_color()</listitem>
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
 *   color is given by calling cogl_material_set_layer_constant.</para>
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
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pipeline_set_layer_combine() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_layer_combine)
CoglBool
cogl_material_set_layer_combine (CoglMaterial *material,
				 int           layer_index,
				 const char   *blend_string,
                                 CoglError   **error);

/**
 * cogl_material_set_layer_combine_constant:
 * @material: A #CoglMaterial object
 * @layer_index: Specifies the layer you want to specify a constant used
 *               for texture combining
 * @constant: The constant color you want
 *
 * When you are using the 'CONSTANT' color source in a layer combine
 * description then you can use this function to define its value.
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pipeline_set_layer_combine_constant()
 * instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_layer_combine_constant)
void
cogl_material_set_layer_combine_constant (CoglMaterial    *material,
                                          int              layer_index,
                                          const CoglColor *constant);

/**
 * cogl_material_set_layer_matrix:
 * @material: A #CoglMaterial object
 * @layer_index: the index for the layer inside @material
 * @matrix: the transformation matrix for the layer
 *
 * This function lets you set a matrix that can be used to e.g. translate
 * and rotate a single layer of a material used to fill your geometry.
 * Deprecated: 1.16: Use cogl_pipeline_set_layer_matrix() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_layer_matrix)
void
cogl_material_set_layer_matrix (CoglMaterial     *material,
				int               layer_index,
				const CoglMatrix *matrix);

/**
 * cogl_material_get_layers:
 * @material: A #CoglMaterial object
 *
 * This function lets you access a material's internal list of layers
 * for iteration.
 *
 * <note>You should avoid using this API if possible since it was only
 * made public by mistake and will be deprecated when we have
 * suitable alternative.</note>
 *
 * <note>It's important to understand that the list returned may not
 * remain valid if you modify the material or any of the layers in any
 * way and so you would have to re-get the list in that
 * situation.</note>
 *
 * Return value: (element-type CoglMaterialLayer) (transfer none): A
 *    list of #CoglMaterialLayer<!-- -->'s that can be passed to the
 *    cogl_material_layer_* functions. The list is owned by Cogl and it
 *    should not be modified or freed
 *    Deprecated: 1.16: Use cogl_pipeline_get_layers() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_get_layers)
const GList *
cogl_material_get_layers (CoglMaterial *material);

/**
 * cogl_material_get_n_layers:
 * @material: A #CoglMaterial object
 *
 * Retrieves the number of layers defined for the given @material
 *
 * Return value: the number of layers
 *
 * Since: 1.0
 * Deprecated: 1.16: Use cogl_pipeline_get_n_layers() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_get_n_layers)
int
cogl_material_get_n_layers (CoglMaterial *material);

/**
 * CoglMaterialLayerType:
 * @COGL_MATERIAL_LAYER_TYPE_TEXTURE: The layer represents a
 *   <link linkend="cogl-Textures">texture</link>
 *
 * Available types of layers for a #CoglMaterial. This enumeration
 * might be expanded in later versions.
 *
 * Since: 1.0
 */
typedef enum {
  COGL_MATERIAL_LAYER_TYPE_TEXTURE
} CoglMaterialLayerType;


/**
 * cogl_material_layer_get_type:
 * @layer: A #CoglMaterialLayer object
 *
 * Retrieves the type of the layer
 *
 * Currently there is only one type of layer defined:
 * %COGL_MATERIAL_LAYER_TYPE_TEXTURE, but considering we may add purely GLSL
 * based layers in the future, you should write code that checks the type
 * first.
 *
 * Return value: the type of the layer
 * Deprecated: 1.16: No replacement
 */
COGL_DEPRECATED_IN_1_16
CoglMaterialLayerType
cogl_material_layer_get_type (CoglMaterialLayer *layer);

/**
 * cogl_material_layer_get_texture:
 * @layer: A #CoglMaterialLayer object
 *
 * Extracts a texture handle for a specific layer.
 *
 * <note>In the future Cogl may support purely GLSL based layers; for those
 * layers this function which will likely return %COGL_INVALID_HANDLE if you
 * try to get the texture handle from them. Considering this scenario, you
 * should call cogl_material_layer_get_type() first in order check it is of
 * type %COGL_MATERIAL_LAYER_TYPE_TEXTURE before calling this function.</note>
 *
 * Return value: (transfer none): a #CoglHandle for the texture inside the layer
 * Deprecated: 1.16: No replacement
 */
COGL_DEPRECATED_IN_1_16
CoglHandle
cogl_material_layer_get_texture (CoglMaterialLayer *layer);

/**
 * cogl_material_layer_get_min_filter:
 * @layer: a #CoglHandle for a material layer
 *
 * Queries the currently set downscaling filter for a material layer
 *
 * Return value: the current downscaling filter
 * Deprecated: 1.16: No replacement
 */
COGL_DEPRECATED_IN_1_16
CoglMaterialFilter
cogl_material_layer_get_min_filter (CoglMaterialLayer *layer);

/**
 * cogl_material_layer_get_mag_filter:
 * @layer: A #CoglMaterialLayer object
 *
 * Queries the currently set downscaling filter for a material later
 *
 * Return value: the current downscaling filter
 * Deprecated: 1.16: No replacement
 */
COGL_DEPRECATED_IN_1_16
CoglMaterialFilter
cogl_material_layer_get_mag_filter (CoglMaterialLayer *layer);

/**
 * cogl_material_set_layer_filters:
 * @material: A #CoglMaterial object
 * @layer_index: the layer number to change.
 * @min_filter: the filter used when scaling a texture down.
 * @mag_filter: the filter used when magnifying a texture.
 *
 * Changes the decimation and interpolation filters used when a texture is
 * drawn at other scales than 100%.
 * Deprecated: 1.16: Use cogl_pipeline_set_layer_filters() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_layer_filters)
void
cogl_material_set_layer_filters (CoglMaterial      *material,
                                 int                layer_index,
                                 CoglMaterialFilter min_filter,
                                 CoglMaterialFilter mag_filter);

/**
 * cogl_material_set_layer_point_sprite_coords_enabled:
 * @material: a #CoglHandle to a material.
 * @layer_index: the layer number to change.
 * @enable: whether to enable point sprite coord generation.
 * @error: A return location for a CoglError, or NULL to ignore errors.
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
 * Since: 1.4
 * Deprecated: 1.16: Use cogl_pipeline_set_layer_point_sprite_coords_enabled()
 *                  instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_layer_point_sprite_coords_enabled)
CoglBool
cogl_material_set_layer_point_sprite_coords_enabled (CoglMaterial *material,
                                                     int           layer_index,
                                                     CoglBool      enable,
                                                     CoglError   **error);

/**
 * cogl_material_get_layer_point_sprite_coords_enabled:
 * @material: a #CoglHandle to a material.
 * @layer_index: the layer number to check.
 *
 * Gets whether point sprite coordinate generation is enabled for this
 * texture layer.
 *
 * Return value: whether the texture coordinates will be replaced with
 * point sprite coordinates.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use cogl_pipeline_get_layer_point_sprite_coords_enabled()
 *                  instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_get_layer_point_sprite_coords_enabled)
CoglBool
cogl_material_get_layer_point_sprite_coords_enabled (CoglMaterial *material,
                                                     int           layer_index);

/**
 * cogl_material_get_layer_wrap_mode_s:
 * @material: A #CoglMaterial object
 * @layer_index: the layer number to change.
 *
 * Returns the wrap mode for the 's' coordinate of texture lookups on this
 * layer.
 *
 * Return value: the wrap mode for the 's' coordinate of texture lookups on
 * this layer.
 *
 * Since: 1.6
 * Deprecated: 1.16: Use cogl_pipeline_get_layer_wrap_mode_s() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_get_layer_wrap_mode_s)
CoglMaterialWrapMode
cogl_material_get_layer_wrap_mode_s (CoglMaterial *material,
                                     int           layer_index);

/**
 * cogl_material_set_layer_wrap_mode_s:
 * @material: A #CoglMaterial object
 * @layer_index: the layer number to change.
 * @mode: the new wrap mode
 *
 * Sets the wrap mode for the 's' coordinate of texture lookups on this layer.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use cogl_pipeline_set_layer_wrap_mode_s() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_layer_wrap_mode_s)
void
cogl_material_set_layer_wrap_mode_s (CoglMaterial        *material,
                                     int                  layer_index,
                                     CoglMaterialWrapMode mode);

/**
 * cogl_material_get_layer_wrap_mode_t:
 * @material: A #CoglMaterial object
 * @layer_index: the layer number to change.
 *
 * Returns the wrap mode for the 't' coordinate of texture lookups on this
 * layer.
 *
 * Return value: the wrap mode for the 't' coordinate of texture lookups on
 * this layer.
 *
 * Since: 1.6
 * Deprecated: 1.16: Use cogl_pipeline_get_layer_wrap_mode_t() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_get_layer_wrap_mode_t)
CoglMaterialWrapMode
cogl_material_get_layer_wrap_mode_t (CoglMaterial *material,
                                     int           layer_index);


/**
 * cogl_material_set_layer_wrap_mode_t:
 * @material: A #CoglMaterial object
 * @layer_index: the layer number to change.
 * @mode: the new wrap mode
 *
 * Sets the wrap mode for the 't' coordinate of texture lookups on this layer.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use cogl_pipeline_set_layer_wrap_mode_t() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_layer_wrap_mode_t)
void
cogl_material_set_layer_wrap_mode_t (CoglMaterial        *material,
                                     int                  layer_index,
                                     CoglMaterialWrapMode mode);

/**
 * cogl_material_get_layer_wrap_mode_p:
 * @material: A #CoglMaterial object
 * @layer_index: the layer number to change.
 *
 * Returns the wrap mode for the 'p' coordinate of texture lookups on this
 * layer.
 *
 * Return value: the wrap mode for the 'p' coordinate of texture lookups on
 * this layer.
 *
 * Since: 1.6
 * Deprecated: 1.16: Use cogl_pipeline_get_layer_wrap_mode_p() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_get_layer_wrap_mode_p)
CoglMaterialWrapMode
cogl_material_get_layer_wrap_mode_p (CoglMaterial *material,
                                     int           layer_index);

/**
 * cogl_material_set_layer_wrap_mode_p:
 * @material: A #CoglMaterial object
 * @layer_index: the layer number to change.
 * @mode: the new wrap mode
 *
 * Sets the wrap mode for the 'p' coordinate of texture lookups on
 * this layer. 'p' is the third coordinate.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use cogl_pipeline_set_layer_wrap_mode_p() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_layer_wrap_mode_p)
void
cogl_material_set_layer_wrap_mode_p (CoglMaterial        *material,
                                     int                  layer_index,
                                     CoglMaterialWrapMode mode);

/**
 * cogl_material_set_layer_wrap_mode:
 * @material: A #CoglMaterial object
 * @layer_index: the layer number to change.
 * @mode: the new wrap mode
 *
 * Sets the wrap mode for all three coordinates of texture lookups on
 * this layer. This is equivalent to calling
 * cogl_material_set_layer_wrap_mode_s(),
 * cogl_material_set_layer_wrap_mode_t() and
 * cogl_material_set_layer_wrap_mode_p() separately.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use cogl_pipeline_set_layer_wrap_mode() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_layer_wrap_mode)
void
cogl_material_set_layer_wrap_mode (CoglMaterial        *material,
                                   int                  layer_index,
                                   CoglMaterialWrapMode mode);

/**
 * cogl_material_layer_get_wrap_mode_s:
 * @layer: A #CoglMaterialLayer object
 *
 * Gets the wrap mode for the 's' coordinate of texture lookups on this layer.
 *
 * Return value: the wrap mode value for the s coordinate.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use cogl_pipeline_layer_get_wrap_mode_s() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_layer_get_wrap_mode_s)
CoglMaterialWrapMode
cogl_material_layer_get_wrap_mode_s (CoglMaterialLayer *layer);

/**
 * cogl_material_layer_get_wrap_mode_t:
 * @layer: A #CoglMaterialLayer object
 *
 * Gets the wrap mode for the 't' coordinate of texture lookups on this layer.
 *
 * Return value: the wrap mode value for the t coordinate.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use cogl_pipeline_layer_get_wrap_mode_t() instead
 */

COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_layer_get_wrap_mode_t)
CoglMaterialWrapMode
cogl_material_layer_get_wrap_mode_t (CoglMaterialLayer *layer);

/**
 * cogl_material_layer_get_wrap_mode_p:
 * @layer: A #CoglMaterialLayer object
 *
 * Gets the wrap mode for the 'p' coordinate of texture lookups on
 * this layer. 'p' is the third coordinate.
 *
 * Return value: the wrap mode value for the p coordinate.
 *
 * Since: 1.4
 * Deprecated: 1.16: Use cogl_pipeline_layer_get_wrap_mode_p() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_layer_get_wrap_mode_p)
CoglMaterialWrapMode
cogl_material_layer_get_wrap_mode_p (CoglMaterialLayer *layer);

#ifdef COGL_ENABLE_EXPERIMENTAL_API

/**
 * cogl_material_set_depth_state:
 * @material: A #CoglMaterial object
 * @state: A #CoglDepthState struct
 * @error: A #CoglError to report failures to setup the given @state.
 *
 * This commits all the depth state configured in @state struct to the
 * given @material. The configuration values are copied into the
 * material so there is no requirement to keep the #CoglDepthState
 * struct around if you don't need it any more.
 *
 * Note: Since some platforms do not support the depth range feature
 * it is possible for this function to fail and report an @error.
 *
 * Returns: TRUE if the GPU supports all the given @state else %FALSE
 *          and returns an @error.
 *
 * Since: 1.8
 * Stability: Unstable
 * Deprecated: 1.16: Use cogl_pipeline_set_depth_state() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_set_depth_state)
CoglBool
cogl_material_set_depth_state (CoglMaterial *material,
                               const CoglDepthState *state,
                               CoglError **error);

/**
 * cogl_material_get_depth_state:
 * @material: A #CoglMaterial object
 * @state_out: A destination #CoglDepthState struct
 *
 * Retrieves the current depth state configuration for the given
 * @pipeline as previously set using cogl_pipeline_set_depth_state().
 *
 * Since: 2.0
 * Stability: Unstable
 * Deprecated: 1.16: Use cogl_pipeline_get_depth_state() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_pipeline_get_depth_state)
void
cogl_material_get_depth_state (CoglMaterial *material,
                               CoglDepthState *state_out);

/**
 * CoglMaterialLayerCallback:
 * @material: The #CoglMaterial whos layers are being iterated
 * @layer_index: The current layer index
 * @user_data: The private data passed to cogl_material_foreach_layer()
 *
 * The callback prototype used with cogl_material_foreach_layer() for
 * iterating all the layers of a @material.
 *
 * Since: 1.4
 * Stability: Unstable
 * Deprecated: 1.16
 */
typedef CoglBool (*CoglMaterialLayerCallback) (CoglMaterial *material,
                                               int layer_index,
                                               void *user_data);

/**
 * cogl_material_foreach_layer:
 * @material: A #CoglMaterial object
 * @callback: A #CoglMaterialLayerCallback to be called for each layer
 *            index
 * @user_data: Private data that will be passed to the callback
 *
 * Iterates all the layer indices of the given @material.
 *
 * Since: 1.4
 * Stability: Unstable
 * Deprecated: 1.16: No replacement
 */
COGL_DEPRECATED_IN_1_16
void
cogl_material_foreach_layer (CoglMaterial *material,
                             CoglMaterialLayerCallback callback,
                             void *user_data);

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

G_END_DECLS

#endif /* __COGL_MATERIAL_H__ */
