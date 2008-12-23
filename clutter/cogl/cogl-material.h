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
 * CoglMaterialBlendFactor:
 * @COGL_MATERIAL_BLEND_FACTOR_ZERO: (0, 0, 0, 0)
 * @COGL_MATERIAL_BLEND_FACTOR_ONE: (1, 1, 1, 1)
 * @COGL_MATERIAL_BLEND_FACTOR_SRC_COLOR: (Rs, Gs, Bs, As)
 * @COGL_MATERIAL_BLEND_FACTOR_DST_COLOR: (Rd, Gd, Bd, Ad)
 * @COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: (1-Rs, 1-Gs, 1-Bs, 1-As)
 * @COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_DST_COLOR: (1-Rd, 1-Gd, 1-Bd, 1-Ad)
 * @COGL_MATERIAL_BLEND_FACTOR_SRC_ALPHA: (As, As, As, As)
 * @COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: (1-As, 1-As, 1-As, 1-As)
 * @COGL_MATERIAL_BLEND_FACTOR_DST_ALPHA: (Ad, Ad, Ad, Ad)
 * @COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: (1-Ad, 1-Ad, 1-Ad, 1-Ad)
 * @COGL_MATERIAL_BLEND_FACTOR_SRC_ALPHA_SATURATE: (f,f,f,1) where f=MIN(As,1-Ad)
 *
 * Blending occurs after the alpha test function, and combines fragments with
 * the framebuffer.
 * <para>
 * A fixed function is used to determine the blended color, which is based on
 * the incoming source color of your fragment (Rs, Gs, Bs, As), a source
 * factor (Sr, Sg, Sb, Sa), a destination color (Rd, Rg, Rb, Ra) and
 * a destination factor (Dr, Dg, Db, Da), and is given by these equations:
 * </para>
 * <programlisting>
 * R = Rs*Sr + Rd*Dr
 * G = Gs*Sg + Gd*Dg
 * B = Bs*Sb + Bd*Db
 * A = As*Sa + Ad*Da
 * </programlisting>
 *
 * All factors have a range [0, 1]
 *
 * The factors are selected with the following constants:
 */
typedef enum _CoglMaterialBlendFactor
{
  COGL_MATERIAL_BLEND_FACTOR_ZERO		  = GL_ZERO,
  COGL_MATERIAL_BLEND_FACTOR_ONE		  = GL_ONE,
  COGL_MATERIAL_BLEND_FACTOR_SRC_COLOR		  = GL_SRC_COLOR,
  COGL_MATERIAL_BLEND_FACTOR_DST_COLOR		  = GL_DST_COLOR,
  COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR  = GL_ONE_MINUS_SRC_COLOR,
  COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_DST_COLOR  = GL_ONE_MINUS_DST_COLOR,
  COGL_MATERIAL_BLEND_FACTOR_SRC_ALPHA		  = GL_SRC_ALPHA,
  COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA  = GL_ONE_MINUS_SRC_ALPHA,
  COGL_MATERIAL_BLEND_FACTOR_DST_ALPHA		  = GL_DST_ALPHA,
  COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA  = GL_ONE_MINUS_DST_ALPHA,
  COGL_MATERIAL_BLEND_FACTOR_SRC_ALPHA_SATURATE	  = GL_SRC_ALPHA_SATURATE,
} CoglMaterialBlendFactor;

/**
 * cogl_material_set_blend_factors:
 * @material: A CoglMaterial object
 * @src_factor: Chooses the @CoglMaterialBlendFactor you want plugged in to
 * the blend equation.
 * @dst_factor: Chooses the @CoglMaterialBlendFactor you want plugged in to
 * the blend equation.
 *
 * This function lets you control how primitives using this material will get
 * blended with the contents of your framebuffer. The blended RGBA components
 * are calculated like this:
 *
 * (RsSr+RdDr, GsSg+GdDg, BsSb+BsSb, AsSa+AdDa)
 *
 * Where (Rs,Gs,Bs,As) represents your source - material- color,
 * (Rd,Gd,Bd,Ad) represents your destination - framebuffer - color,
 * (Sr,Sg,Sb,Sa) represents your source blend factor and
 * (Dr,Dg,Db,Da) represents you destination blend factor.
 *
 * All factors lie in the range [0,1] and incoming color components are also
 * normalized to the range [0,1]
 *
 * Since 1.0
 */
void cogl_material_set_blend_factors (CoglHandle              material,
				      CoglMaterialBlendFactor src_factor,
				      CoglMaterialBlendFactor dst_factor);

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
			      gint       layer_index,
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
 * CoglMaterialLayerCombineFunc:
 * @COGL_MATERIAL_LAYER_COMBINE_FUNC_REPLACE: Arg0
 * @COGL_MATERIAL_LAYER_COMBINE_FUNC_MODULATE: Arg0 x Arg1
 * @COGL_MATERIAL_LAYER_COMBINE_FUNC_ADD: Arg0 + Arg1
 * @COGL_MATERIAL_LAYER_COMBINE_FUNC_ADD_SIGNED: Arg0 + Arg1 - 0.5
 * @COGL_MATERIAL_LAYER_COMBINE_FUNC_INTERPOLATE: Arg0 x Arg + Arg1 x (1-Arg2)
 * @COGL_MATERIAL_LAYER_COMBINE_FUNC_SUBTRACT: Arg0 - Arg1
 * @COGL_MATERIAL_LAYER_COMBINE_FUNC_DOT3_RGB: 4 x ((Arg0r - 0.5) x (Arg1r - 0.5)) +
 * @COGL_MATERIAL_LAYER_COMBINE_FUNC_DOT3_RGBA: ((Arg0b - 0.5) x (Arg1b - 0.5)) +
 *
 * A material may comprise of 1 or more layers that can be combined using a
 * number of different functions. By default layers are modulated, which is
 * to say the components of the current source layer S are simply multipled
 * together with the combined results of the previous layer P like this:
 *
 * <programlisting>
 * (Rs*Rp, Gs*Gp, Bs*Bp, As*Ap)
 * </programlisting>
 *
 * For more advanced techniques, Cogl exposes the fixed function texture
 * combining capabilities of your GPU to give you greater control.
 */
typedef enum _CoglMaterialLayerCombineFunc
{
  COGL_MATERIAL_LAYER_COMBINE_FUNC_REPLACE    = GL_REPLACE,
  COGL_MATERIAL_LAYER_COMBINE_FUNC_MODULATE   = GL_MODULATE,
  COGL_MATERIAL_LAYER_COMBINE_FUNC_ADD	      = GL_ADD,
  COGL_MATERIAL_LAYER_COMBINE_FUNC_ADD_SIGNED = GL_ADD_SIGNED,
  COGL_MATERIAL_LAYER_COMBINE_FUNC_INTERPOLATE = GL_INTERPOLATE,
  COGL_MATERIAL_LAYER_COMBINE_FUNC_SUBTRACT   = GL_SUBTRACT,
  COGL_MATERIAL_LAYER_COMBINE_FUNC_DOT3_RGB   = GL_DOT3_RGB,
  COGL_MATERIAL_LAYER_COMBINE_FUNC_DOT3_RGBA  = GL_DOT3_RGBA
} CoglMaterialLayerCombineFunc;

/**
 * CoglMaterialLayerCombineChannels:
 * @COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB: Modify the function or argument
 *                                            src/op for the RGB components of a
 *                                            layer
 * @COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA: Modify the function or argument
 *                                              src/op for the Alpha component of a
 *                                              layer
 * @COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA: Modify the function or argument
 *                                             src/op for all the components of a
 *                                             layer
 *
 * Cogl optionally lets you describe 2 seperate combine modes for a single
 * layer; 1 for the RGB components, and 1 for the Alpha component, so in this
 * case you would repeat the 3 steps documented with the
 * @cogl_material_set_layer_combine_function function for each channel
 * selector.
 *
 * (Note: you can't have different modes for each channel, so if you need more
 *  control you will need to use a glsl fragment shader)
 */
typedef enum _CoglMaterialLayerCombineChannels
{
  COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB,
  COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA,
  COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA
} CoglMaterialLayerCombineChannels;

/**
 * cogl_material_set_layer_combine_function:
 * @material: A CoglMaterial object
 * @layer_index: Specifies the layer whos combine mode you want to modify
 * @channels: Specifies which channels combine mode you want to modify
 *            (RGB, ALPHA, or RGBA)
 * @func: Specifies the function you want to use for combining fragments
 *        of the specified layer with the results of previously combined
 *        layers.
 *
 * There are three basic steps to describing how a layer should be combined:
 * <orderedlist>
 * <listitem>
 * Choose a function.
 * </listitem>
 * <listitem>
 * Specify the source color for each argument of the chosen function. (Note
 *    the functions don't all take the same number of arguments)
 * </listitem>
 * <listitem>
 * Specify an operator for each argument that can modify the corresponding
 *    source color before the function is applied.
 * </listitem>
 * </orderedlist>
 *
 * Cogl optionally lets you describe 2 seperate combine modes for a single
 * layer; 1 for the RGB components, and 1 for the Alpha component, so in this
 * case you would repeat the 3 steps for each channel selector.
 *
 * (Note: you can't have different modes for each channel, so if you need more
 *  control you will need to use a glsl fragment shader)
 *
 * For example here is how you could elect to use the ADD function for all
 * components of layer 1 in your material:
 * <programlisting>
 * //Step 1: Choose a function. Note the ADD function takes 2 arguments...
 * cogl_material_set_layer_combine_function (material,
 *                                           1,
 *                                           COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA)
 *                                           COGL_MATERIAL_LAYER_COMBINE_FUNC_ADD);
 * //Step 2: Specify the source color for the 2 ADD function arguments...
 * cogl_material_set_layer_combine_arg_src (material,
 *                                          1,//layer index
 *                                          0,//argument index
 *                                          COGL_MATERIAL_LAYER_COMBINE_SRC_PREVIOUS);
 * cogl_material_set_layer_combine_arg_src (material,
 *                                          1,//layer index
 *                                          1,//argument index
 *                                          COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA)
 *                                          COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE);
 * //Step 3: Specify the operators used to modify the arguments...
 * cogl_material_set_layer_combine_arg_op (material,
 *                                         1,//layer index
 *                                         0,//argument index
 *                                         COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA,
 *                                         COGL_MATERIAL_LAYER_COMBINE_OP_SRC_COLOR);
 * cogl_material_set_layer_combine_arg_op (material,
 *                                         1,//layer index
 *                                         1,//argument index
 *                                         COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA,
 *                                         COGL_MATERIAL_LAYER_COMBINE_OP_SRC_COLOR);
 * </programlisting>
 */
void cogl_material_set_layer_combine_function (CoglHandle                        material,
                                               gint                              layer_index,
                                               CoglMaterialLayerCombineChannels  channels,
                                               CoglMaterialLayerCombineFunc      func);

/**
 * CoglMaterialLayerCombineSrc:
 * @COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE: The fragment color of the current texture layer
 * @COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE0: The fragment color of texture unit 0
 * @COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE1: The fragment color of texture unit 1
 * @COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE2: The fragment color of texture unit 2..7
 * @COGL_MATERIAL_LAYER_COMBINE_SRC_CONSTANT: A fixed constant color (TODO: no API yet to specify the actual color!)
 * @COGL_MATERIAL_LAYER_COMBINE_SRC_PRIMARY_COLOR: The basic color of the primitive ignoring texturing
 * @COGL_MATERIAL_LAYER_COMBINE_SRC_PREVIOUS: The result of combining all previous layers
 *
 * Note for the constants @COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE0..n the
 * numbers may not correspond to the indices you choose for your layers since
 * your layer indices don't need to be contiguous. If you need to use these
 * it would probably be sensible to ensure the layer indices do infact
 * correspond.
 */
typedef enum _CoglMaterialLayerCombineSrc
{
  COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE = GL_TEXTURE,

  /* Can we find a nicer way... */
  COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE0 = GL_TEXTURE0,
  COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE1 = GL_TEXTURE1,
  COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE2 = GL_TEXTURE2,
  COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE3 = GL_TEXTURE3,
  COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE4 = GL_TEXTURE4,
  COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE5 = GL_TEXTURE5,
  COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE6 = GL_TEXTURE6,
  COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE7 = GL_TEXTURE7,
  /* .. who would ever need more than 8 texture layers.. :-) */

  COGL_MATERIAL_LAYER_COMBINE_SRC_CONSTANT	= GL_CONSTANT,
  COGL_MATERIAL_LAYER_COMBINE_SRC_PRIMARY_COLOR = GL_PRIMARY_COLOR,
  COGL_MATERIAL_LAYER_COMBINE_SRC_PREVIOUS	= GL_PREVIOUS
} CoglMaterialLayerCombineSrc;

/**
 * cogl_material_set_layer_combine_arg_src:
 * @material: A CoglMaterial object
 * @layer_index:
 * @argument:
 * @channels:
 * @src:
 *
 */
void cogl_material_set_layer_combine_arg_src (CoglHandle                       material,
                                              gint                             layer_index,
                                              gint                             argument,
                                              CoglMaterialLayerCombineChannels channels,
                                              CoglMaterialLayerCombineSrc      src);

typedef enum _CoglMaterialLayerCombineOp
{
  COGL_MATERIAL_LAYER_COMBINE_OP_SRC_COLOR	     = GL_SRC_COLOR,
  COGL_MATERIAL_LAYER_COMBINE_OP_ONE_MINUS_SRC_COLOR = GL_ONE_MINUS_SRC_COLOR,

  COGL_MATERIAL_LAYER_COMBINE_OP_SRC_ALPHA	     = GL_SRC_ALPHA,
  COGL_MATERIAL_LAYER_COMBINE_OP_ONE_MINUS_SRC_ALPHA = GL_ONE_MINUS_SRC_ALPHA
} CoglMaterialLayerCombineOp;

/**
 * cogl_material_set_layer_combine_arg_op:
 * @material: A CoglMaterial object
 * @layer_index:
 * @argument:
 * @channels:
 * @op:
 *
 */
void cogl_material_set_layer_combine_arg_op (CoglHandle                       material,
                                             gint                             layer_index,
                                             gint                             argument,
                                             CoglMaterialLayerCombineChannels channels,
                                             CoglMaterialLayerCombineOp       op);

/* TODO: */
#if 0
 I think it would be be really neat to support a simple string description
 of the fixed function texture combine modes exposed above. I think we can
 consider this stuff to be set in stone from the POV that more advanced
 texture combine functions are catered for with GLSL, so it seems reasonable
 to find a concise string representation that can represent all the above
 modes in a *much* more readable/useable fashion. I think somthing like
 this would be quite nice:

  "MODULATE(TEXTURE[RGB], PREVIOUS[A])"
  "ADD(TEXTURE[A],PREVIOUS[RGB])"
  "INTERPOLATE(TEXTURE[1-A], PREVIOUS[RGB])"

void cogl_material_set_layer_rgb_combine (CoglHandle material
					  gint layer_index,
					  const char *combine_description);
void cogl_material_set_layer_alpha_combine (CoglHandle material
					    gint layer_index,
					    const char *combine_description);
#endif

/**
 * cogl_material_set_layer_matrix:
 * @material: A CoglMaterial object
 *
 * This function lets you set a matrix that can be used to e.g. translate
 * and rotate a single layer of a material used to fill your geometry.
 */
void cogl_material_set_layer_matrix (CoglHandle  material,
				     gint        layer_index,
				     CoglMatrix *matrix);

/**
 * cogl_material_get_cogl_enable_flags:
 * @material: A CoglMaterial object
 *
 * This determines what flags need to be passed to cogl_enable before
 * this material can be used. Normally you shouldn't need to use this
 * function directly since Cogl will do this internally, but if you are
 * developing custom primitives directly with OpenGL you may want to use
 * this.
 *
 * Note: This API is hopfully just a stop-gap solution. Ideally
 * cogl_enable will be replaced.
 */
/* TODO: find a nicer solution! */
gulong
cogl_material_get_cogl_enable_flags (CoglHandle handle);

/**
 * cogl_material_get_layers:
 * @material: A CoglMaterial object
 *
 * This function lets you access a materials internal list of layers
 * for iteration.
 *
 * Note: Normally you shouldn't need to use this function directly since
 * Cogl will do this internally, but if you are developing custom primitives
 * directly with OpenGL, you will need to iterate the layers that you want
 * to texture with.
 *
 * Note: This function may return more layers than OpenGL can use at once
 * so it's your responsability limit yourself to
 * CGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS.
 *
 * Note: It's a bit out of the ordinary to return a const GList *, but it
 * was considered sensible to try and avoid list manipulation for every
 * primitive emitted in a scene, every frame.
 */
const GList *cogl_material_get_layers (CoglHandle material_handle);

/**
 * CoglMaterialLayerType:
 * @COGL_MATERIAL_LAYER_TYPE_TEXTURE: The layer represents a CoglTexture
 */
typedef enum _CoglMaterialLayerType
{
  COGL_MATERIAL_LAYER_TYPE_TEXTURE
} CoglMaterialLayerType;

/**
 * cogl_material_layer_get_type:
 * @material: A CoglMaterial object
 *
 * Currently there is only one type of layer defined:
 * COGL_MATERIAL_LAYER_TYPE_TEXTURE, but considering we may add purely GLSL
 * based layers in the future, you should write code that checks the type
 * first.
 *
 * Note: Normally you shouldn't need to use this function directly since
 * Cogl will do this internally, but if you are developing custom primitives
 * directly with OpenGL, you will need to iterate the layers that you want
 * to texture with, and thus should be checking the layer types.
 */
CoglMaterialLayerType cogl_material_layer_get_type (CoglHandle layer_handle);

/**
 * cogl_material_layer_get_texture:
 * @material: A CoglMaterial object
 *
 * This lets you extract a CoglTexture handle for a specific layer. Normally
 * you shouldn't need to use this function directly since Cogl will do this
 * internally, but if you are developing custom primitives directly with
 * OpenGL you may need this.
 *
 * Note: In the future, we may support purely GLSL based layers which will
 * likley return COGL_INVALID_HANDLE if you try to get the texture.
 * Considering this, you should always call cogl_material_layer_get_type
 * first, to check it is of type COGL_MATERIAL_LAYER_TYPE_TEXTURE.
 */
CoglHandle cogl_material_layer_get_texture (CoglHandle layer_handle);

/**
 * cogl_material_layer_flush_gl_sampler_state:
 * @material: A CoglMaterial object
 *
 * This commits the sampler state for a single material layer to the OpenGL
 * driver. Normally you shouldn't need to use this function directly since
 * Cogl will do this internally, but if you are developing custom primitives
 * directly with OpenGL you may want to use this.
 *
 * Note: It assumes you have already activated the appropriate sampler
 * by calling glActiveTexture ();
 */
void cogl_material_layer_flush_gl_sampler_state (CoglHandle layer_handle);

/**
 * cogl_set_source:
 * @material: A CoglMaterial object
 *
 * This function sets the source material that will be used to fill
 * subsequent geometry emitted via the cogl API.
 *
 * Note: in the future we may add the ability to set a front facing
 * material, and a back facing material, in which case this function
 * will set both to the same.
 *
 * Since 1.0
 */
/* XXX: This doesn't really belong to the cogl-material API, it should
 * move to cogl.h */
void cogl_set_source (CoglHandle material);

/**
 * cogl_flush_material_gl_state:
 *
 * This function commits all the state of the source CoglMaterial - not
 * including the per-layer state - to the OpenGL[ES] driver.
 *
 * Normally you shouldn't need to use this function directly, but if you
 * are developing a custom primitive using raw OpenGL that works with
 * CoglMaterials, then you may want to use this function.
 *
 * Since 1.0
 */
/* XXX: This should be moved with cogl_set_source to cogl.h */
void cogl_flush_material_gl_state (void);

G_END_DECLS

#endif /* __COGL_MATERIAL_H__ */

