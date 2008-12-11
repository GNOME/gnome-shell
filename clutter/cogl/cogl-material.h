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

G_END_DECLS

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

typedef enum _CoglMaterialBlendFactor
{
  COGL_MATERIAL_BLEND_FACTOR_ZERO		  = GL_ZERO,
  COGL_MATERIAL_BLEND_FACTOR_ONE		  = GL_ONE,
  COGL_MATERIAL_BLEND_FACTOR_DST_COLOR		  = GL_DST_COLOR,
  COGL_MATERIAL_BLEND_FACTOR_SRC_COLOR		  = GL_SRC_COLOR,
  COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_DST_COLOR  = GL_ONE_MINUS_DST_COLOR,
  COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR  = GL_ONE_MINUS_SRC_COLOR,
  COGL_MATERIAL_BLEND_FACTOR_SRC_ALPHA		  = GL_SRC_ALPHA,
  COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA  = GL_ONE_MINUS_SRC_ALPHA,
  COGL_MATERIAL_BLEND_FACTOR_DST_ALPHA		  = GL_DST_ALPHA,
  COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA  = GL_ONE_MINUS_DST_ALPHA,
  COGL_MATERIAL_BLEND_FACTOR_SRC_ALPHA_SATURATE	  = GL_SRC_ALPHA_SATURATE,
  COGL_MATERIAL_BLEND_FACTOR_CONSTANT_COLOR	  = GL_CONSTANT_COLOR,
  COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR =
    GL_ONE_MINUS_CONSTANT_COLOR,
  COGL_MATERIAL_BLEND_FACTOR_CONSTANT_ALPHA	  = GL_CONSTANT_ALPHA,
  COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA =
    GL_ONE_MINUS_CONSTANT_ALPHA
} CoglMaterialBlendFactor;

typedef enum _CoglMaterialLayerType
{
  COGL_MATERIAL_LAYER_TYPE_TEXTURE
} CoglMaterialLayerType;

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

typedef enum _CoglMaterialLayerCombineChannels
{
  COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB,
  COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA,
  COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA
} CoglMaterialLayerCombineChannels;

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

typedef enum _CoglMaterialLayerCombineOp
{
  COGL_MATERIAL_LAYER_COMBINE_OP_SRC_COLOR	     = GL_SRC_COLOR,
  COGL_MATERIAL_LAYER_COMBINE_OP_ONE_MINUS_SRC_COLOR = GL_ONE_MINUS_SRC_COLOR,

  COGL_MATERIAL_LAYER_COMBINE_OP_SRC_ALPHA	     = GL_SRC_ALPHA,
  COGL_MATERIAL_LAYER_COMBINE_OP_ONE_MINUS_SRC_ALPHA = GL_ONE_MINUS_SRC_ALPHA
} CoglMaterialLayerCombineOp;

/**
 * cogl_material_new:
 *
 * Allocates and initializes blank white material
 */
CoglHandle cogl_material_new (void);

/**
 * cogl_material_ref:
 * @handle: a @CoglHandle.
 *
 * Increment the reference count for a cogl material.
 *
 * Returns: the @handle.
 */
CoglHandle cogl_material_ref (CoglHandle handle);

/**
 * cogl_material_unref:
 * @handle: a @CoglHandle.
 *
 * Deccrement the reference count for a cogl material.
 */
void cogl_material_unref (CoglHandle handle);

/**
 * cogl_material_set_diffuse:
 * @material: A CoglMaterial object
 * @diffuse: The components of the desired diffuse color
 *
 * Exposing the standard OpenGL lighting model; this function sets
 * the material's diffuse color. The diffuse color is most intense
 * where the light hits the surface directly; perpendicular to the
 * surface.
 */
void cogl_material_set_diffuse (CoglHandle material,
				const CoglColor *diffuse);

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
 */
void cogl_material_set_ambient (CoglHandle material,
				const CoglColor *ambient);

/**
 * cogl_material_set_ambient_and_diffuse:
 * @material: A CoglMaterial object
 * @color: The components of the desired ambient and diffuse colors
 *
 * This is a convenience for setting the diffuse and ambient color
 * of the material at the same time.
 */
void cogl_material_set_ambient_and_diffuse (CoglHandle material,
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
 */
void cogl_material_set_specular (CoglHandle material,
				 const CoglColor *specular);

/**
 * cogl_material_set_shininess:
 * @material: A CoglMaterial object
 * shininess: The desired shininess; range: [0.0, 1.0]
 *
 * This function sets the materials shininess which determines how
 * specular highlights are calculated. A higher shininess will produce
 * smaller brigher highlights.
 */
void cogl_material_set_shininess (CoglHandle material,
				  float shininess);

/**
 * cogl_material_set_emission:
 * @material: A CoglMaterial object
 * @emission: The components of the desired emissive color
 *
 * Exposing the standard OpenGL lighting model; this function sets
 * the material's emissive color. It will look like the surface is
 * a light source emitting this color.
 */
void cogl_material_set_emission (CoglHandle material,
				 const CoglColor *emission);

/**
 * cogl_set_source:
 * @material: A CoglMaterial object
 *
 * This function sets the source material that will be used to fill
 * subsequent geometry emitted via the cogl API.
 *
 * XXX: This doesn't really belong to the cogl-material API, it should
 * move to cogl.h
 */
void cogl_set_source (CoglHandle material);

/**
 * cogl_material_set_alpha_test_func:
 * @material: A CoglMaterial object
 *
 * Before a primitive is blended with the framebuffer, it goes through an
 * alpha test stage which lets you discard fragments based on the current
 * alpha value. This function lets you change the function used to evaluate
 * the alpha channel, and thus determine which fragments are discarded
 * and which continue on to the blending stage.
 * TODO: expand on this
 */
void cogl_material_set_alpha_test_func (CoglHandle material,
					CoglMaterialAlphaFunc alpha_func,
					float alpha_reference);

/**
 * cogl_material_set_blend_function:
 * @material: A CoglMaterial object
 * @src_factor: Chooses the source factor you want plugged in to the blend
 * equation. 
 * @dst_factor: Chooses the source factor you want plugged in to the blend
 * equation.
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
 * The factors are selected with the following constants:
 *    <itemizedlist>
 *    <listitem>COGL_MATERIAL_BLEND_FACTOR_ZERO: (0,0,0,0)</listitem>
 *    <listitem>COGL_MATERIAL_BLEND_FACTOR_ONE: (1,1,1,1)</listitem>
 *    <listitem>COGL_MATERIAL_BLEND_FACTOR_DST_COLOR: (Rd,Gd,Bd,Ad)</listitem>
 *    <listitem>COGL_MATERIAL_BLEND_FACTOR_SRC_COLOR: (Rs,Gs,Bs,As)</listitem>
 *    <listitem>COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
 *		(1,1,1,1)-(Rd,Gd,Bd,Ad) [Only valid for src_factor]</listitem>
 *    <listitem>COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
 *		(1,1,1,1)-(Rs,Gs,Bs,As)</listitem>
 *    <listitem>COGL_MATERIAL_BLEND_FACTOR_SRC_ALPHA: (As,As,As,As)</listitem>
 *    <listitem>COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
 *		(1,1,1,1)-(As,As,As,As) [Only valid for dst_factor]</listitem>
 *    <listitem>COGL_MATERIAL_BLEND_FACTOR_DST_ALPHA: (Ad,Ad,Ad,Ad)</listitem>
 *    <listitem>COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
 *		(1,1,1,1)-(Ad,Ad,Ad,Ad)</listitem>
 *    <listitem>COGL_MATERIAL_BLEND_FACTOR_SRC_ALPHA_SATURATE:
 *		(f,f,f,1) where f=MIN(As,1-Ad)</listitem>
 *    </itemizedlist>
 */
void cogl_material_set_blend_function (CoglHandle material,
				       CoglMaterialBlendFactor src_factor,
				       CoglMaterialBlendFactor dst_factor);

/**
 * cogl_material_set_layer:
 * @material: A CoglMaterial object
 *
 * In addition to the standard OpenGL lighting model a Cogl material may have
 * one or more layers comprised of textures that can be blended together in
 * order with a number of different texture combine modes. This function
 * defines a new texture layer.
 *
 * The index values of multiple layers do not have to be consecutive; it is
 * only their relative order that is important.
 *
 * XXX: In the future, we may define other types of material layers, such
 * as purely GLSL based layers.
 */
void cogl_material_set_layer (CoglHandle material,
			      gint layer_index,
			      CoglHandle texture);

/**
 * cogl_material_add_texture:
 * @material: A CoglMaterial object
 *
 *
 */
void cogl_material_remove_layer (CoglHandle material,
				 gint layer_index);

/**
 * cogl_material_set_layer_alpha_combine_func:
 * @material: A CoglMaterial object
 *
 * TODO: Brew, a nice hot cup of tea, and document these functions...
 */
void cogl_material_set_layer_combine_func (
				    CoglHandle material,
				    gint layer_index,
				    CoglMaterialLayerCombineChannels channels,
				    CoglMaterialLayerCombineFunc func);

void cogl_material_set_layer_combine_arg_src (
				    CoglHandle material,
				    gint layer_index,
				    gint argument,
				    CoglMaterialLayerCombineChannels channels,
				    CoglMaterialLayerCombineSrc src);

void cogl_material_set_layer_combine_arg_op (
				    CoglHandle material,
				    gint layer_index,
				    gint argument,
				    CoglMaterialLayerCombineChannels channels,
				    CoglMaterialLayerCombineOp op);

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
void cogl_material_set_layer_matrix (CoglHandle material,
				     gint layer_index,
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
gulong
cogl_material_get_cogl_enable_flags (CoglHandle handle);

/**
 * cogl_material_flush_gl_material_state:
 * @material: A CoglMaterial object
 *
 * This commits the glMaterial state to the OpenGL driver. Normally you
 * shouldn't need to use this function directly, since Cogl will do this
 * internally, but if you are developing custom primitives directly with
 * OpenGL you may want to use this.
 */
void cogl_material_flush_gl_material_state (CoglHandle material_handle);

/**
 * cogl_material_flush_gl_alpha_func:
 * @material: A CoglMaterial object
 *
 */
void cogl_material_flush_gl_alpha_func (CoglHandle material_handle);

/**
 * cogl_material_flush_gl_blend_func:
 * @material: A CoglMaterial object
 *
 */
void cogl_material_flush_gl_blend_func (CoglHandle material_handle);

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

#endif /* __COGL_MATERIAL_H__ */

