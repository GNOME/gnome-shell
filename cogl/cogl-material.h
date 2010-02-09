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

#ifndef __COGL_MATERIAL_H__
#define __COGL_MATERIAL_H__

G_BEGIN_DECLS

#include <cogl/cogl-types.h>
#include <cogl/cogl-matrix.h>

/**
 * SECTION:cogl-material
 * @short_description: Fuctions for creating and manipulating materials
 *
 * COGL allows creating and manipulating materials used to fill in
 * geometry. Materials may simply be lighting attributes (such as an
 * ambient and diffuse colour) or might represent one or more textures
 * blended together.
 */

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
  COGL_MATERIAL_FILTER_NEAREST = GL_NEAREST,
  COGL_MATERIAL_FILTER_LINEAR = GL_LINEAR,
  COGL_MATERIAL_FILTER_NEAREST_MIPMAP_NEAREST = GL_NEAREST_MIPMAP_NEAREST,
  COGL_MATERIAL_FILTER_LINEAR_MIPMAP_NEAREST = GL_LINEAR_MIPMAP_NEAREST,
  COGL_MATERIAL_FILTER_NEAREST_MIPMAP_LINEAR = GL_NEAREST_MIPMAP_LINEAR,
  COGL_MATERIAL_FILTER_LINEAR_MIPMAP_LINEAR = GL_LINEAR_MIPMAP_LINEAR
} CoglMaterialFilter;

/**
 * cogl_material_new:
 *
 * Allocates and initializes a blank white material
 *
 * Return value: a handle to the new material
 */
CoglHandle cogl_material_new (void);

/**
 * cogl_material_copy:
 *
 * Creates a new material handle with the configuration copied from the
 * source material.
 *
 * We would strongly advise developers to always aim to use
 * cogl_material_copy() instead of cogl_material_new() whenever there will
 * be any similarity between two materials. Copying a material helps Cogl
 * keep track of a materials ancestry which we may use to help minimize GPU
 * state changes.
 *
 * Returns: a handle to the new material
 *
 * Since: 1.2
 */
CoglHandle cogl_material_copy (CoglHandle source);

#ifndef COGL_DISABLE_DEPRECATED

/**
 * cogl_material_ref:
 * @handle: a @CoglHandle.
 *
 * Increment the reference count for a cogl material.
 *
 * Return value: the @handle.
 *
 * Since: 1.0
 *
 * Deprecated: 1.2: Use cogl_handle_ref() instead
 */
CoglHandle cogl_material_ref (CoglHandle handle);

/**
 * cogl_material_unref:
 * @handle: a @CoglHandle.
 *
 * Decrement the reference count for a cogl material.
 *
 * Since: 1.0
 *
 * Deprecated: 1.2: Use cogl_handle_unref() instead
 */
void cogl_material_unref (CoglHandle handle);

#endif /* COGL_DISABLE_DEPRECATED */

/**
 * cogl_is_material:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing material object.
 *
 * Return value: %TRUE if the handle references a #CoglMaterial,
 *   %FALSE otherwise
 */
gboolean cogl_is_material (CoglHandle handle);

/**
 * cogl_material_set_color:
 * @material: A CoglMaterial object
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
 */
void cogl_material_set_color (CoglHandle       material,
                              const CoglColor *color);

/**
 * cogl_material_set_color4ub:
 * @material: A CoglMaterial object
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
 */
void cogl_material_set_color4ub (CoglHandle material,
			         guint8     red,
                                 guint8     green,
                                 guint8     blue,
                                 guint8     alpha);

/**
 * cogl_material_set_color4f:
 * @material: A CoglMaterial object
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
 */
void cogl_material_set_color4f (CoglHandle material,
                                float      red,
                                float      green,
                                float      blue,
                                float      alpha);

/**
 * cogl_material_get_color:
 * @material: A CoglMaterial object
 * @color: (out): The location to store the color
 *
 * Retrieves the current material color.
 *
 * Since: 1.0
 */
void cogl_material_get_color (CoglHandle  material,
                              CoglColor  *color);

/**
 * cogl_material_set_ambient:
 * @material: A CoglMaterial object
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
 */
void cogl_material_set_ambient (CoglHandle       material,
				const CoglColor *ambient);

/**
 * cogl_material_get_ambient:
 * @material: A CoglMaterial object
 * @ambient: The location to store the ambient color
 *
 * Retrieves the current ambient color for @material
 *
 * Since: 1.0
 */
void cogl_material_get_ambient (CoglHandle  material,
                                CoglColor  *ambient);

/**
 * cogl_material_set_diffuse:
 * @material: A CoglMaterial object
 * @diffuse: The components of the desired diffuse color
 *
 * Sets the material's diffuse color, in the standard OpenGL lighting
 * model. The diffuse color is most intense where the light hits the
 * surface directly - perpendicular to the surface.
 *
 * The default value is (0.8, 0.8, 0.8, 1.0)
 *
 * Since: 1.0
 */
void cogl_material_set_diffuse (CoglHandle       material,
				const CoglColor *diffuse);

/**
 * cogl_material_get_diffuse:
 * @material: A CoglMaterial object
 * @diffuse: The location to store the diffuse color
 *
 * Retrieves the current diffuse color for @material
 *
 * Since: 1.0
 */
void cogl_material_get_diffuse (CoglHandle  material,
                                CoglColor  *diffuse);

/**
 * cogl_material_set_ambient_and_diffuse:
 * @material: A CoglMaterial object
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
 */
void cogl_material_set_ambient_and_diffuse (CoglHandle       material,
					    const CoglColor *color);

/**
 * cogl_material_set_specular:
 * @material: A CoglMaterial object
 * @specular: The components of the desired specular color
 *
 * Sets the material's specular color, in the standard OpenGL lighting
 * model. The intensity of the specular color depends on the viewport
 * position, and is brightest along the lines of reflection.
 *
 * The default value is (0.0, 0.0, 0.0, 1.0)
 *
 * Since: 1.0
 */
void cogl_material_set_specular (CoglHandle       material,
				 const CoglColor *specular);

/**
 * cogl_material_get_specular:
 * @material: A CoglMaterial object
 * @specular: The location to store the specular color
 *
 * Retrieves the materials current specular color.
 *
 * Since: 1.0
 */
void cogl_material_get_specular (CoglHandle  material,
                                 CoglColor  *specular);

/**
 * cogl_material_set_shininess:
 * @material: A CoglMaterial object
 * @shininess: The desired shininess; range: [0.0, 1.0]
 *
 * Sets the materials shininess, in the standard OpenGL lighting model,
 * which determines how specular highlights are calculated. A higher
 * @shininess will produce smaller brigher highlights.
 *
 * The default value is 0.0
 *
 * Since: 1.0
 */
void cogl_material_set_shininess (CoglHandle material,
				  float      shininess);
/**
 * cogl_material_get_shininess:
 * @material: A CoglMaterial object
 *
 * Retrieves the materials current emission color.
 *
 * Return value: The materials current shininess value
 *
 * Since: 1.0
 */
float cogl_material_get_shininess (CoglHandle material);

/**
 * cogl_material_set_emission:
 * @material: A CoglMaterial object
 * @emission: The components of the desired emissive color
 *
 * Sets the material's emissive color, in the standard OpenGL lighting
 * model. It will look like the surface is a light source emitting this
 * color.
 *
 * The default value is (0.0, 0.0, 0.0, 1.0)
 *
 * Since: 1.0
 */
void cogl_material_set_emission (CoglHandle       material,
				 const CoglColor *emission);

/**
 * cogl_material_get_emission:
 * @material: A CoglMaterial object
 * @emission: The location to store the emission color
 *
 * Retrieves the materials current emission color.
 *
 * Since: 1.0
 */
void cogl_material_get_emission (CoglHandle material,
                                 CoglColor *emission);

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
  COGL_MATERIAL_ALPHA_FUNC_NEVER    = GL_NEVER,
  COGL_MATERIAL_ALPHA_FUNC_LESS	    = GL_LESS,
  COGL_MATERIAL_ALPHA_FUNC_EQUAL    = GL_EQUAL,
  COGL_MATERIAL_ALPHA_FUNC_LEQUAL   = GL_LEQUAL,
  COGL_MATERIAL_ALPHA_FUNC_GREATER  = GL_GREATER,
  COGL_MATERIAL_ALPHA_FUNC_NOTEQUAL = GL_NOTEQUAL,
  COGL_MATERIAL_ALPHA_FUNC_GEQUAL   = GL_GEQUAL,
  COGL_MATERIAL_ALPHA_FUNC_ALWAYS   = GL_ALWAYS
} CoglMaterialAlphaFunc;

/**
 * cogl_material_set_alpha_test_function:
 * @material: A CoglMaterial object
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
 */
void cogl_material_set_alpha_test_function (CoglHandle            material,
					    CoglMaterialAlphaFunc alpha_func,
					    float                 alpha_reference);

/**
 * cogl_material_set_blend:
 * @material: A CoglMaterial object
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
 */
gboolean cogl_material_set_blend (CoglHandle  material,
                                  const char *blend_string,
                                  GError    **error);

/**
 * cogl_material_set_blend_constant:
 * @material: A CoglMaterial object
 * @constant_color: The constant color you want
 *
 * When blending is setup to reference a CONSTANT blend factor then
 * blending will depend on the constant set with this function.
 *
 * Since: 1.0
 */
void cogl_material_set_blend_constant (CoglHandle material,
                                       CoglColor *constant_color);

/**
 * cogl_material_set_layer:
 * @material: A #CoglHandle for a material object
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
 */
void cogl_material_set_layer (CoglHandle material,
			      int        layer_index,
			      CoglHandle texture);

/**
 * cogl_material_remove_layer:
 * @material: A CoglMaterial object
 * @layer_index: Specifies the layer you want to remove
 *
 * This function removes a layer from your material
 */
void cogl_material_remove_layer (CoglHandle material,
				 gint       layer_index);


/**
 * cogl_material_set_layer_combine:
 * @material: A CoglMaterial object
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
 */
gboolean
cogl_material_set_layer_combine (CoglHandle   material,
				 gint         layer_index,
				 const char  *blend_string,
                                 GError     **error);

/**
 * cogl_material_set_layer_combine_constant:
 * @material: A CoglMaterial object
 * @layer_index: Specifies the layer you want to specify a constant used
 *               for texture combining
 * @constant: The constant color you want
 *
 * When you are using the 'CONSTANT' color source in a layer combine
 * description then you can use this function to define its value.
 *
 * Since: 1.0
 */
void cogl_material_set_layer_combine_constant (CoglHandle  material,
                                               int         layer_index,
                                               CoglColor  *constant);

/**
 * cogl_material_set_layer_matrix:
 * @material: A CoglMaterial object
 * @layer_index: the index for the layer inside @material
 * @matrix: the transformation matrix for the layer
 *
 * This function lets you set a matrix that can be used to e.g. translate
 * and rotate a single layer of a material used to fill your geometry.
 */
void cogl_material_set_layer_matrix (CoglHandle  material,
				     int         layer_index,
				     CoglMatrix *matrix);

/**
 * cogl_material_get_layers:
 * @material: a #CoglHandle for a material
 *
 * This function lets you access a materials internal list of layers
 * for iteration.
 *
 * Return value: (element-type Handle) (transfer none): A list of
 *   #CoglHandle<!-- -->'s that can be passed to the  cogl_material_layer_*
 *   functions. The list is owned by COGL and it  should not be modified or
 *   freed
 */
G_CONST_RETURN GList *cogl_material_get_layers (CoglHandle material);

/**
 * cogl_material_get_n_layers:
 * @material: a #CoglHandle for a material
 *
 * Retrieves the number of layers defined for the given @material
 *
 * Return value: the number of layers
 *
 * Since: 1.0
 */
int cogl_material_get_n_layers (CoglHandle material);

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
 * @layer: A #CoglHandle for a material layer
 *
 * Retrieves the type of the layer
 *
 * Currently there is only one type of layer defined:
 * %COGL_MATERIAL_LAYER_TYPE_TEXTURE, but considering we may add purely GLSL
 * based layers in the future, you should write code that checks the type
 * first.
 *
 * Return value: the type of the layer
 */
CoglMaterialLayerType cogl_material_layer_get_type (CoglHandle layer);

/**
 * cogl_material_layer_get_texture:
 * @layer: A #CoglHandle for a material layer
 *
 * Extracts a texture handle for a specific layer.
 *
 * <note>In the future Cogl may support purely GLSL based layers; for those
 * layers this function which will likely return %COGL_INVALID_HANDLE if you
 * try to get the texture handle from them. Considering this scenario, you
 * should call cogl_material_layer_get_type() first in order check it is of
 * type %COGL_MATERIAL_LAYER_TYPE_TEXTURE before calling this function.</note>
 *
 * Return value: a #CoglHandle for the texture inside the layer
 */
CoglHandle cogl_material_layer_get_texture (CoglHandle layer);

/**
 * cogl_material_layer_get_min_filter:
 * @layer: a #CoglHandle for a material layer
 *
 * Queries the currently set downscaling filter for a material layer
 *
 * Return value: the current downscaling filter
 */
CoglMaterialFilter cogl_material_layer_get_min_filter (CoglHandle layer);

/**
 * cogl_material_layer_get_mag_filter:
 * @layer: a #CoglHandle for a material layer
 *
 * Queries the currently set downscaling filter for a material later
 *
 * Return value: the current downscaling filter
 */
CoglMaterialFilter cogl_material_layer_get_mag_filter (CoglHandle layer);

/**
 * cogl_material_set_layer_filters:
 * @material: a #CoglHandle to a material.
 * @layer_index: the layer number to change.
 * @min_filter: the filter used when scaling a texture down.
 * @mag_filter: the filter used when magnifying a texture.
 *
 * Changes the decimation and interpolation filters used when a texture is
 * drawn at other scales than 100%.
 */
void cogl_material_set_layer_filters (CoglHandle         material,
                                      gint               layer_index,
                                      CoglMaterialFilter min_filter,
                                      CoglMaterialFilter mag_filter);

G_END_DECLS

#endif /* __COGL_MATERIAL_H__ */
