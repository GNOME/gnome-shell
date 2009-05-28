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
 * cogl_material_new:
 *
 * Allocates and initializes a blank white material
 *
 * Returns: a handle to the new material
 */
CoglHandle cogl_material_new (void);

/**
 * cogl_material_ref:
 * @handle: a @CoglHandle.
 *
 * Increment the reference count for a cogl material.
 *
 * Returns: the @handle.
 *
 * Since 1.0
 */
CoglHandle cogl_material_ref (CoglHandle handle);

/**
 * cogl_material_unref:
 * @handle: a @CoglHandle.
 *
 * Decrement the reference count for a cogl material.
 *
 * Since 1.0
 */
void cogl_material_unref (CoglHandle handle);

/**
 * cogl_is_material:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing material object.
 *
 * Returns: %TRUE if the handle references a #CoglMaterial,
 *   %FALSE otherwise
 */
gboolean cogl_is_material (CoglHandle handle);

/**
 * cogl_material_set_color:
 * @material: A CoglMaterial object
 * @color: The components of the color
 *
 * This is the basic color of the material, used when no lighting is enabled.
 *
 * The default value is (1.0, 1.0, 1.0, 1.0)
 *
 * Since 1.0
 */
void cogl_material_set_color (CoglHandle material, const CoglColor *color);

/**
 * cogl_material_set_color4ub:
 * @material: A CoglMaterial object
 * @red: The red component
 * @green: The green component
 * @blue: The blue component
 * @alpha: The alpha component
 *
 * This is the basic color of the material, used when no lighting is enabled.
 *
 * The default value is (0xff, 0xff, 0xff, 0xff)
 *
 * Since 1.0
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
 * This is the basic color of the material, used when no lighting is enabled.
 *
 * The default value is (1.0, 1.0, 1.0, 1.0)
 *
 * Since 1.0
 */
void cogl_material_set_color4f (CoglHandle material,
                                float      red,
                                float      green,
                                float      blue,
                                float      alpha);

/**
 * cogl_material_get_color:
 * @material: A CoglMaterial object
 * @color: The location to store the color
 *
 * This retrieves the current material color.
 *
 * Since 1.0
 */
void cogl_material_get_color (CoglHandle  material, CoglColor  *color);

/**
 * cogl_material_set_ambient:
 * @material: A CoglMaterial object
 * @ambient: The components of the desired ambient color
 *
 * Exposing the standard OpenGL lighting model; this function sets
 * the material's ambient color. The ambient color affects the overall
 * color of the object. Since the diffuse color will be intense when
 * the light hits the surface directly, the ambient will most aparent
 * where the light hits at a slant.
 *
 * The default value is (0.2, 0.2, 0.2, 1.0)
 *
 * Since 1.0
 */
void cogl_material_set_ambient (CoglHandle       material,
				const CoglColor *ambient);

/**
 * cogl_material_get_ambient:
 * @material: A CoglMaterial object
 * @ambient: The location to store the ambient color
 *
 * This retrieves the materials current ambient color.
 *
 * Since 1.0
 */
void cogl_material_get_ambient (CoglHandle  material, CoglColor  *ambient);

/**
 * cogl_material_set_diffuse:
 * @material: A CoglMaterial object
 * @diffuse: The components of the desired diffuse color
 *
 * Exposing the standard OpenGL lighting model; this function sets
 * the material's diffuse color. The diffuse color is most intense
 * where the light hits the surface directly; perpendicular to the
 * surface.
 *
 * The default value is (0.8, 0.8, 0.8, 1.0)
 *
 * Since 1.0
 */
void cogl_material_set_diffuse (CoglHandle       material,
				const CoglColor *diffuse);

/**
 * cogl_material_get_diffuse:
 * @material: A CoglMaterial object
 * @diffuse: The location to store the diffuse color
 *
 * This retrieves the materials current diffuse color.
 *
 * Since 1.0
 */
void cogl_material_get_diffuse (CoglHandle  material, CoglColor  *diffuse);

/**
 * cogl_material_set_ambient_and_diffuse:
 * @material: A CoglMaterial object
 * @color: The components of the desired ambient and diffuse colors
 *
 * This is a convenience for setting the diffuse and ambient color
 * of the material at the same time.
 *
 * The default ambient color is (0.2, 0.2, 0.2, 1.0)
 * The default diffuse color is (0.8, 0.8, 0.8, 1.0)
 *
 * Since 1.0
 */
void cogl_material_set_ambient_and_diffuse (CoglHandle       material,
					    const CoglColor *color);

/**
 * cogl_material_set_specular:
 * @material: A CoglMaterial object
 * @specular: The components of the desired specular color
 *
 * Exposing the standard OpenGL lighting model; this function sets
 * the material's specular color. The intensity of the specular color
 * depends on the viewport position, and is brightest along the lines
 * of reflection.
 *
 * The default value is (0.0, 0.0, 0.0, 1.0)
 *
 * Since 1.0
 */
void cogl_material_set_specular (CoglHandle       material,
				 const CoglColor *specular);

/**
 * cogl_material_get_specular:
 * @material: A CoglMaterial object
 * @specular: The location to store the specular color
 *
 * This retrieves the materials current specular color.
 *
 * Since 1.0
 */
void cogl_material_get_specular (CoglHandle  material, CoglColor  *specular);

/**
 * cogl_material_set_shininess:
 * @material: A CoglMaterial object
 * shininess: The desired shininess; range: [0.0, 1.0]
 *
 * This function sets the materials shininess which determines how
 * specular highlights are calculated. A higher shininess will produce
 * smaller brigher highlights.
 *
 * The default value is 0.0
 *
 * Since 1.0
 */
void cogl_material_set_shininess (CoglHandle material,
				  float      shininess);
/**
 * cogl_material_get_shininess:
 * @material: A CoglMaterial object
 *
 * This retrieves the materials current emission color.
 *
 * Return value: The materials current shininess value
 *
 * Since 1.0
 */
float cogl_material_get_shininess (CoglHandle material);

/**
 * cogl_material_set_emission:
 * @material: A CoglMaterial object
 * @emission: The components of the desired emissive color
 *
 * Exposing the standard OpenGL lighting model; this function sets
 * the material's emissive color. It will look like the surface is
 * a light source emitting this color.
 *
 * The default value is (0.0, 0.0, 0.0, 1.0)
 *
 * Since 1.0
 */
void cogl_material_set_emission (CoglHandle       material,
				 const CoglColor *emission);

/**
 * cogl_material_get_emission:
 * @material: A CoglMaterial object
 * @emission: The location to store the emission color
 *
 * This retrieves the materials current emission color.
 *
 * Since 1.0
 */
void cogl_material_get_emission (CoglHandle material, CoglColor *emission);

/**
 * CoglMaterialAlphaFunc:
 * @COGL_MATERIAL_ALPHA_FUNC_NEVER: Never let the fragment through.
 * @COGL_MATERIAL_ALPHA_FUNC_LESS: Let the fragment through if the incoming
 *                                 alpha value is less than the reference alpha
 *                                 value.
 * @COGL_MATERIAL_ALPHA_FUNC_EQUAL: Let the fragment through if the incoming
 *                                  alpha value equals the reference alpha
 *                                  value.
 * @COGL_MATERIAL_ALPHA_FUNC_LEQUAL: Let the fragment through if the incoming
 *                                   alpha value is less than or equal to the
 *                                   reference alpha value.
 * @COGL_MATERIAL_ALPHA_FUNC_GREATER: Let the fragment through if the incoming
 *                                    alpha value is greater than the reference
 *                                    alpha value.
 * @COGL_MATERIAL_ALPHA_FUNC_NOTEQUAL: Let the fragment through if the incoming
 *                                     alpha value does not equal the reference
 *                                     alpha value.
 * @COGL_MATERIAL_ALPHA_FUNC_GEQUAL: Let the fragment through if the incoming
 *                                   alpha value is greater than or equal to the
 *                                   reference alpha value.
 * @COGL_MATERIAL_ALPHA_FUNC_ALWAYS: Always let the fragment through.
 *
 * Alpha testing happens before blending primitives with the framebuffer and
 * gives an opportunity to discard fragments based on a comparison with the
 * incoming alpha value and a reference alpha value. The #CoglMaterialAlphaFunc
 * determines how the comparison is done.
 */
typedef enum _CoglMaterialAlphaFunc
{
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
 *                   to compare incoming fragments to.
 *
 * Before a primitive is blended with the framebuffer, it goes through an
 * alpha test stage which lets you discard fragments based on the current
 * alpha value. This function lets you change the function used to evaluate
 * the alpha channel, and thus determine which fragments are discarded
 * and which continue on to the blending stage.
 *
 * The default is COGL_MATERIAL_ALPHA_FUNC_ALWAYS
 *
 * Since 1.0
 */
void cogl_material_set_alpha_test_function (CoglHandle            material,
					    CoglMaterialAlphaFunc alpha_func,
					    float                 alpha_reference);

/**
 * cogl_material_set_blend:
 * @material: A CoglMaterial object
 * @blend_string: A <link linkend="cogl-Blend-Strings">Cogl blend string</link>
 *                describing the desired blend function.
 * @error: A GError that may report lack of driver support if you give
 *         separate blend string statements for the alpha channel and RGB
 *         channels since some drivers or backends such as GLES 1.1 dont
 *         support this.
 *
 * If not already familiar; please refer
 * <link linkend="cogl-Blend-Strings">here</link> for an overview of what blend
 * strings are and there syntax.
 *
 * Blending occurs after the alpha test function, and combines fragments with
 * the framebuffer.

 * Currently the only blend function Cogl exposes is ADD(). So any valid
 * blend statements will be of the form:
 *
 * <programlisting>
 * &lt;channel-mask&gt;=ADD(SRC_COLOR*(&lt;factor&gt;), DST_COLOR*(&lt;factor&gt;))
 * </programlisting>
 *
 * <b>NOTE: The brackets around blend factors are currently not optional!</b>
 *
 * This is the list of source-names usable as blend factors:
 * <itemizedlist>
 * <listitem>SRC_COLOR: The color of the in comming fragment</listitem>
 * <listitem>DST_COLOR: The color of the framebuffer</listitem>
 * <listitem>
 * CONSTANT: The constant set via cogl_material_set_blend_constant()</listitem>
 * </itemizedlist>
 * The source names can be used according to the
 * <link linkend="cogl-Blend-String-syntax">color-source and factor syntax</link>,
 * so for example "(1-SRC_COLOR[A])" would be a valid factor, as would
 * "(CONSTANT[RGB])"
 *
 * These can also be used as factors:
 * <itemizedlist>
 * <listitem>0: (0, 0, 0, 0)</listitem>
 * <listitem>1: (1, 1, 1, 1)</listitem>
 * <listitem>SRC_ALPHA_SATURATE_FACTOR: (f,f,f,1)
 * where f=MIN(SRC_COLOR[A],1-DST_COLOR[A])</listitem>
 * </itemizedlist>
 * <para>
 * Remember; all color components are normalized to the range [0, 1] before
 * computing the result of blending.
 * </para>
 * <section>
 * <title>Examples</title>
 * Blend a non-premultiplied source over a destination with
 * premultiplied alpha:
 * <programlisting>
 * "RGB = ADD(SRC_COLOR*(SRC_COLOR[A]), DST_COLOR*(1-SRC_COLOR[A]))"
 * "A   = ADD(SRC_COLOR, DST_COLOR*(1-SRC_COLOR[A]))"
 * </programlisting>
 * Blend a premultiplied source over a destination with premultiplied alpha:
 * <programlisting>
 * "RGBA = ADD(SRC_COLOR, DST_COLOR*(1-SRC_COLOR[A]))"
 * </programlisting>
 * </section>
 *
 * Returns: TRUE if the blend string was successfully parsed, and the described
 *          blending is supported by the underlying driver/hardware. If there
 *          was an error, it returns FALSE.
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
void cogl_material_set_blend_constant (CoglHandle             material,
                                       CoglColor              *constant_color);

/**
 * cogl_material_set_layer:
 * @material: A CoglMaterial object
 *
 * In addition to the standard OpenGL lighting model a Cogl material may have
 * one or more layers comprised of textures that can be blended together in
 * order, with a number of different texture combine modes. This function
 * defines a new texture layer.
 *
 * The index values of multiple layers do not have to be consecutive; it is
 * only their relative order that is important.
 *
 * XXX: In the future, we may define other types of material layers, such
 * as purely GLSL based layers.
 *
 * Since 1.0
 */
void cogl_material_set_layer (CoglHandle material,
			      int        layer_index,
			      CoglHandle texture);

/**
 * cogl_material_add_texture:
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
 *                describing the desired texture combine function.
 * @error: A GError that may report parse errors or lack of GPU/driver support.
 *
 * If not already familiar; you can refer
 * <link linkend="cogl-Blend-Strings">here</link> for an overview of what blend
 * strings are and there syntax.
 *
 * These are all the functions available for texture combining:
 * <itemizedlist>
 * <listitem>REPLACE(arg0) = arg0</listitem>
 * <listitem>MODULATE(arg0, arg1) = arg0 x arg1</listitem>
 * <listitem>ADD(arg0, arg1) = arg0 + arg1</listitem>
 * <listitem>ADD_SIGNED(arg0, arg1) = arg0 + arg1 - 0.5</listitem>
 * <listitem>INTERPOLATE(arg0, arg1, arg2) =
 * arg0 x arg2 + arg1 x (1 - arg2)</listitem>
 * <listitem>SUBTRACT(arg0, arg1) = arg0 - arg1</listitem>
 * <listitem>
 * DOT3_RGB(arg0, arg1) =
 * <programlisting>
 * 4 x ((arg0[R] - 0.5)) * (arg1[R] - 0.5) +
 *      (arg0[G] - 0.5)) * (arg1[G] - 0.5) +
 *      (arg0[B] - 0.5)) * (arg1[B] - 0.5))
 * </programlisting>
 * </listitem>
 * <listitem>DOT3_RGBA(arg0, arg1) =
 * <programlisting>
 * 4 x ((arg0[R] - 0.5)) * (arg1[R] - 0.5) +
 *      (arg0[G] - 0.5)) * (arg1[G] - 0.5) +
 *      (arg0[B] - 0.5)) * (arg1[B] - 0.5))
 * </programlisting>
 * </listitem>
 * </itemizedlist>
 *
 * Refer to the
 * <link linkend="cogl-Blend-String-syntax">color-source syntax</link> for
 * describing the arguments. The valid source names for texture combining
 * are:
 * <itemizedlist>
 * <listitem>
 * TEXTURE: Use the color from the current texture layer
 * </listitem>
 * <listitem>
 * TEXTURE_0, TEXTURE_1, etc: Use the color from the specified texture layer
 * </listitem>
 * <listitem>
 * CONSTANT: Use the color from the constant given with
 * cogl_material_set_layer_constant()
 * </listitem>
 * <listitem>
 * PRIMARY: Use the color of the material as set with cogl_material_set_color()
 * </listitem>
 * <listitem>
 * PREVIOUS: Either use the texture color from the previous layer, or if this
 * is layer 0, use the color of the material as set with
 * cogl_material_set_color()
 * </listitem>
 * </itemizedlist>
 * <section>
 * <title>Example</title>
 * This is effectively what the default blending is:
 * <programlisting>
 * "RGBA = MODULATE (PREVIOUS, TEXTURE)"
 * </programlisting>
 * This could be used to cross-fade between two images, using the alpha
 * component of a constant as the interpolator. The constant color
 * is given by calling cogl_material_set_layer_constant.
 * <programlisting>
 * RGBA = INTERPOLATE (PREVIOUS, TEXTURE, CONSTANT[A])
 * </programlisting>
 * </section>
 * <b>Note: you can't give a multiplication factor for arguments as you can
 * with blending.</b>
 *
 * Returns: TRUE if the blend string was successfully parsed, and the described
 *          texture combining is supported by the underlying driver/hardware.
 *          If there was an error, it returns FALSE.
 *
 * Since: 1.0
 */
gboolean
cogl_material_set_layer_combine (CoglHandle material,
				 gint layer_index,
				 const char *blend_string,
                                 GError **error);

/**
 * cogl_material_set_layer_combine_constant:
 * @material: A CoglMaterial object
 * @layer_index: Specifies the layer you want to specify a constant used
 *               for texture combining
 * @color_constant: The constant color you want
 *
 * When you are using the 'CONSTANT' color source in a layer combine
 * description then you can use this function to define its value.
 *
 * Since 1.0
 */
void cogl_material_set_layer_combine_constant (CoglHandle                     material,
                                               int                            layer_index,
                                               CoglColor                     *constant);

/**
 * cogl_material_set_layer_matrix:
 * @material: A CoglMaterial object
 *
 * This function lets you set a matrix that can be used to e.g. translate
 * and rotate a single layer of a material used to fill your geometry.
 */
void cogl_material_set_layer_matrix (CoglHandle  material,
				     int         layer_index,
				     CoglMatrix *matrix);

/**
 * cogl_material_get_layers:
 * @material: A CoglMaterial object
 *
 * This function lets you access a materials internal list of layers
 * for iteration.
 *
 * Returns: A list of #CoglHandle<!-- -->'s that can be passed to the
 *          cogl_material_layer_* functions.
 */
const GList *cogl_material_get_layers (CoglHandle material_handle);

/**
 * CoglMaterialLayerType:
 * @COGL_MATERIAL_LAYER_TYPE_TEXTURE: The layer represents a
 * <link linkend="cogl-Textures">Cogl texture</link>
 *
 * Available types of layers for a #CoglMaterial. This enumeration
 * might be expanded in later versions.
 *
 * Since: 1.0
 */
typedef enum _CoglMaterialLayerType
{
  COGL_MATERIAL_LAYER_TYPE_TEXTURE
} CoglMaterialLayerType;

/**
 * cogl_material_layer_get_type:
 * @layer_handle: A Cogl material layer handle
 *
 * Currently there is only one type of layer defined:
 * COGL_MATERIAL_LAYER_TYPE_TEXTURE, but considering we may add purely GLSL
 * based layers in the future, you should write code that checks the type
 * first.
 */
CoglMaterialLayerType cogl_material_layer_get_type (CoglHandle layer_handle);

/**
 * cogl_material_layer_get_texture:
 * @layer_handle: A CoglMaterialLayer handle
 *
 * This lets you extract a CoglTexture handle for a specific layer.
 *
 * Note: In the future, we may support purely GLSL based layers which will
 *       likely return COGL_INVALID_HANDLE if you try to get the texture.
 *       Considering this, you can call cogl_material_layer_get_type first,
 *       to check it is of type COGL_MATERIAL_LAYER_TYPE_TEXTURE.
 *
 * Note: It is possible for a layer object of type
 *       COGL_MATERIAL_LAYER_TYPE_TEXTURE to be realized before a texture
 *       object has been associated with the layer. For example this happens
 *       if you setup layer combining for a given layer index before calling
 *       cogl_material_set_layer for that index.
 *
 * Returns: A CoglHandle to the layers texture object or COGL_INVALID_HANDLE
 *          if a texture has not been set yet.
 */
CoglHandle cogl_material_layer_get_texture (CoglHandle layer_handle);


G_END_DECLS

#endif /* __COGL_MATERIAL_H__ */

