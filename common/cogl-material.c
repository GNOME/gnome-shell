
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-handle.h"

#include "cogl-material-private.h"

#include <glib.h>

static void _cogl_material_free (CoglMaterial *tex);
static void _cogl_material_layer_free (CoglMaterialLayer *layer);

COGL_HANDLE_DEFINE (Material, material, material_handles);
COGL_HANDLE_DEFINE (MaterialLayer,
		    material_layer,
		    material_layer_handles);

CoglHandle
cogl_material_new (void)
{
  /* Create new - blank - material */
  CoglMaterial *material = g_new0 (CoglMaterial, 1);
  GLfloat *ambient = material->ambient;
  GLfloat *diffuse = material->diffuse;
  GLfloat *specular = material->specular;
  GLfloat *emission = material->emission;

  material->ref_count = 1;
  COGL_HANDLE_DEBUG_NEW (material, material);

  /* Use the same defaults as the GL spec... */
  ambient[0] = 0.2; ambient[1] = 0.2; ambient[2] = 0.2; ambient[3] = 1.0;
  diffuse[0] = 0.8; diffuse[1] = 0.8; diffuse[2] = 0.8; diffuse[3] = 1.0;
  specular[0] = 0; specular[1] = 0; specular[2] = 0; specular[3] = 1.0;
  emission[0] = 0; emission[1] = 0; emission[2] = 0; emission[3] = 1.0;

  /* Use the same defaults as the GL spec... */
  material->alpha_func = COGL_MATERIAL_ALPHA_FUNC_ALWAYS;
  material->alpha_func_reference = 0.0;

  /* Not the same as the GL default, but seems saner... */
  material->blend_src_factor = COGL_MATERIAL_BLEND_FACTOR_SRC_ALPHA;
  material->blend_dst_factor = COGL_MATERIAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

  material->layers = NULL;

  return _cogl_material_handle_new (material);
}

static void
_cogl_material_free (CoglMaterial *material)
{
  /* Frees material resources but its handle is not
     released! Do that separately before this! */

  g_list_foreach (material->layers,
		  (GFunc)cogl_material_layer_unref, NULL);
  g_free (material);
}

void
cogl_material_set_ambient (CoglHandle handle,
			   const CoglColor *ambient_color)
{
  CoglMaterial *material;
  GLfloat      *ambient;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  ambient = material->ambient;
  ambient[0] = cogl_color_get_red_float (ambient_color);
  ambient[1] = cogl_color_get_green_float (ambient_color);
  ambient[2] = cogl_color_get_blue_float (ambient_color);
  ambient[3] = cogl_color_get_alpha_float (ambient_color);

  material->flags |= COGL_MATERIAL_FLAG_DIRTY;
}

void
cogl_material_set_diffuse (CoglHandle handle,
			   const CoglColor *diffuse_color)
{
  CoglMaterial *material;
  GLfloat      *diffuse;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  diffuse = material->diffuse;
  diffuse[0] = cogl_color_get_red_float (diffuse_color);
  diffuse[1] = cogl_color_get_green_float (diffuse_color);
  diffuse[2] = cogl_color_get_blue_float (diffuse_color);
  diffuse[3] = cogl_color_get_alpha_float (diffuse_color);

  material->flags |= COGL_MATERIAL_FLAG_DIRTY;
}

void
cogl_material_set_ambient_and_diffuse (CoglHandle handle,
				       const CoglColor *color)
{
  cogl_material_set_ambient (handle, color);
  cogl_material_set_diffuse (handle, color);
}

void
cogl_material_set_specular (CoglHandle handle,
			    const CoglColor *specular_color)
{
  CoglMaterial *material;
  GLfloat      *specular;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  specular = material->specular;
  specular[0] = cogl_color_get_red_float (specular_color);
  specular[1] = cogl_color_get_green_float (specular_color);
  specular[2] = cogl_color_get_blue_float (specular_color);
  specular[3] = cogl_color_get_alpha_float (specular_color);

  material->flags |= COGL_MATERIAL_FLAG_DIRTY;
}

void
cogl_material_set_shininess (CoglHandle handle,
			     float shininess)
{
  CoglMaterial *material;

  g_return_if_fail (cogl_is_material (handle));

  if (shininess < 0.0 || shininess > 1.0)
    g_warning ("Out of range shininess %f supplied for material\n",
	       shininess);

  material = _cogl_material_pointer_from_handle (handle);

  material->shininess = (GLfloat)shininess * 128.0;

  material->flags |= COGL_MATERIAL_FLAG_DIRTY;
}

void
cogl_material_set_emission (CoglHandle handle,
			    const CoglColor *emission_color)
{
  CoglMaterial *material;
  GLfloat      *emission;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);

  emission = material->emission;
  emission[0] = cogl_color_get_red_float (emission_color);
  emission[1] = cogl_color_get_green_float (emission_color);
  emission[2] = cogl_color_get_blue_float (emission_color);
  emission[3] = cogl_color_get_alpha_float (emission_color);

  material->flags |= COGL_MATERIAL_FLAG_DIRTY;
}

void
cogl_material_set_alpha_test_function (CoglHandle handle,
				       CoglMaterialAlphaFunc alpha_func,
				       float alpha_reference)
{
  CoglMaterial *material;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);
  material->alpha_func = alpha_func;
  material->alpha_func_reference = (GLfloat)alpha_reference;

  material->flags |= COGL_MATERIAL_FLAG_DIRTY;
}

void
cogl_material_set_blend_factors (CoglHandle handle,
				 CoglMaterialBlendFactor src_factor,
				 CoglMaterialBlendFactor dst_factor)
{
  CoglMaterial *material;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);
  material->blend_src_factor = src_factor;
  material->blend_dst_factor = dst_factor;

  material->flags |= COGL_MATERIAL_FLAG_DIRTY;
}

/* Asserts that a layer corresponding to the given index exists. If no
 * match is found, then a new empty layer is added.
 */
static CoglMaterialLayer *
_cogl_material_get_layer (CoglMaterial *material,
			  gint index,
			  gboolean create_if_not_found)
{
  CoglMaterialLayer *layer;
  GList		    *tmp;
  CoglHandle	     layer_handle;

  for (tmp = material->layers; tmp != NULL; tmp = tmp->next)
    {
      layer =
	_cogl_material_layer_pointer_from_handle ((CoglHandle)tmp->data);
      if (layer->index == index)
	return layer;

      /* The layers are always sorted, so at this point we know this layer
       * doesn't exist */
      if (layer->index > index)
	break;
    }
  /* NB: if we now insert a new layer before tmp, that will maintain order.
   */

  if (!create_if_not_found)
    return NULL;

  layer = g_new0 (CoglMaterialLayer, 1);

  layer->ref_count = 1;
  layer->index = index;
  layer->texture = COGL_INVALID_HANDLE;

  /* Choose the same default combine mode as OpenGL:
   * MODULATE(PREVIOUS[RGBA],TEXTURE[RGBA]) */
  layer->texture_combine_rgb_func = COGL_MATERIAL_LAYER_COMBINE_FUNC_MODULATE;
  layer->texture_combine_rgb_src[0] = COGL_MATERIAL_LAYER_COMBINE_SRC_PREVIOUS;
  layer->texture_combine_rgb_src[1] = COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE;
  layer->texture_combine_rgb_op[0] = COGL_MATERIAL_LAYER_COMBINE_OP_SRC_COLOR;
  layer->texture_combine_rgb_op[1] = COGL_MATERIAL_LAYER_COMBINE_OP_SRC_COLOR;
  layer->texture_combine_alpha_func =
    COGL_MATERIAL_LAYER_COMBINE_FUNC_MODULATE;
  layer->texture_combine_alpha_src[0] =
    COGL_MATERIAL_LAYER_COMBINE_SRC_PREVIOUS;
  layer->texture_combine_alpha_src[1] =
    COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE;
  layer->texture_combine_alpha_op[0] =
    COGL_MATERIAL_LAYER_COMBINE_OP_SRC_ALPHA;
  layer->texture_combine_alpha_op[1] =
    COGL_MATERIAL_LAYER_COMBINE_OP_SRC_ALPHA;

  layer_handle = _cogl_material_layer_handle_new (layer);
  /* Note: see comment after for() loop above */
  material->layers =
    g_list_insert_before (material->layers, tmp, layer_handle);

  return layer;
}

void
cogl_material_set_layer (CoglHandle material_handle,
			 gint layer_index,
			 CoglHandle texture_handle)
{
  CoglMaterial	    *material;
  CoglMaterialLayer *layer;
  int		     n_layers;

  g_return_if_fail (cogl_is_material (material_handle));
  g_return_if_fail (cogl_is_texture (texture_handle));

  material = _cogl_material_pointer_from_handle (material_handle);
  layer = _cogl_material_get_layer (material_handle, layer_index, TRUE);

  /* XXX: If we expose manual control over ENABLE_BLEND, we'll add
   * a flag to know when it's user configured, so we don't trash it */
  if (cogl_texture_get_format (texture_handle) & COGL_A_BIT)
    material->flags |= COGL_MATERIAL_FLAG_ENABLE_BLEND;

  n_layers = g_list_length (material->layers);
  if (n_layers >= CGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS)
    {
      if (!(material->flags & COGL_MATERIAL_FLAG_SHOWN_SAMPLER_WARNING))
	{
	  g_warning ("Your hardware doesnot have enough texture samplers"
		     "to handle this many texture layers");
	  material->flags |= COGL_MATERIAL_FLAG_SHOWN_SAMPLER_WARNING;
	}
      /* Note: We always make a best effort attempt to display as many
       * layers as possible, so this isn't an _error_ */
      /* Note: in the future we may support enabling/disabling layers
       * too, so it may become valid to add more than
       * MAX_COMBINED_TEXTURE_IMAGE_UNITS layers. */
    }

  if (layer->texture)
    cogl_texture_unref (layer->texture);

  cogl_texture_ref (texture_handle);
  layer->texture = texture_handle;
}

void
cogl_material_set_layer_combine_function (
				  CoglHandle handle,
				  gint layer_index,
				  CoglMaterialLayerCombineChannels channels,
				  CoglMaterialLayerCombineFunc func)
{
  CoglMaterial *material;
  CoglMaterialLayer *layer;
  gboolean set_alpha_func = FALSE;
  gboolean set_rgb_func = FALSE;

  g_return_if_fail (cogl_is_material (handle));

  material = _cogl_material_pointer_from_handle (handle);
  layer = _cogl_material_get_layer (material, layer_index, FALSE);
  if (!layer)
    return;

  if (channels == COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA)
    set_alpha_func = set_rgb_func = TRUE;
  else if (channels == COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB)
    set_rgb_func = TRUE;
  else if (channels == COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA)
    set_alpha_func = TRUE;

  if (set_rgb_func)
    layer->texture_combine_rgb_func = func;
  if (set_alpha_func)
    layer->texture_combine_alpha_func = func;
}

void
cogl_material_set_layer_combine_arg_src (
				  CoglHandle handle,
				  gint layer_index,
				  gint argument,
				  CoglMaterialLayerCombineChannels channels,
				  CoglMaterialLayerCombineSrc src)
{
  CoglMaterial	    *material;
  CoglMaterialLayer *layer;
  gboolean set_arg_alpha_src = FALSE;
  gboolean set_arg_rgb_src = FALSE;

  g_return_if_fail (cogl_is_material (handle));
  g_return_if_fail (argument >=0 && argument <= 3);

  material = _cogl_material_pointer_from_handle (handle);
  layer = _cogl_material_get_layer (material, layer_index, FALSE);
  if (!layer)
    return;

  if (channels == COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA)
    set_arg_alpha_src = set_arg_rgb_src = TRUE;
  else if (channels == COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB)
    set_arg_rgb_src = TRUE;
  else if (channels == COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA)
    set_arg_alpha_src = TRUE;

  if (set_arg_rgb_src)
    layer->texture_combine_rgb_src[argument] = src;
  if (set_arg_alpha_src)
    layer->texture_combine_alpha_src[argument] = src;
}

void
cogl_material_set_layer_combine_arg_op (
				    CoglHandle material_handle,
				    gint layer_index,
				    gint argument,
				    CoglMaterialLayerCombineChannels channels,
				    CoglMaterialLayerCombineOp op)
{
  CoglMaterial *material;
  CoglMaterialLayer *layer;
  gboolean set_arg_alpha_op = FALSE;
  gboolean set_arg_rgb_op = FALSE;

  g_return_if_fail (cogl_is_material (material_handle));
  g_return_if_fail (argument >=0 && argument <= 3);

  material = _cogl_material_pointer_from_handle (material_handle);
  layer = _cogl_material_get_layer (material, layer_index, FALSE);
  if (!layer)
    return;

  if (channels == COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA)
    set_arg_alpha_op = set_arg_rgb_op = TRUE;
  else if (channels == COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB)
    set_arg_rgb_op = TRUE;
  else if (channels == COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA)
    set_arg_alpha_op = TRUE;

  if (set_arg_rgb_op)
    layer->texture_combine_rgb_op[argument] = op;
  if (set_arg_alpha_op)
    layer->texture_combine_alpha_op[argument] = op;
}

void
cogl_material_set_layer_matrix (CoglHandle material_handle,
				gint layer_index,
				CoglMatrix *matrix)
{
  CoglMaterial *material;
  CoglMaterialLayer *layer;

  g_return_if_fail (cogl_is_material (material_handle));

  material = _cogl_material_pointer_from_handle (material_handle);
  layer = _cogl_material_get_layer (material, layer_index, FALSE);
  if (!layer)
    return;

  layer->matrix = *matrix;
  layer->flags |= COGL_MATERIAL_LAYER_FLAG_USER_MATRIX;
}

static void
_cogl_material_layer_free (CoglMaterialLayer *layer)
{
  cogl_texture_unref (layer->texture);
  g_free (layer);
}

void
cogl_material_remove_layer (CoglHandle material_handle,
			    gint layer_index)
{
  CoglMaterial	     *material;
  CoglMaterialLayer  *layer;
  GList		     *tmp;

  g_return_if_fail (cogl_is_material (material_handle));

  material = _cogl_material_pointer_from_handle (material_handle);
  material->flags &= ~COGL_MATERIAL_FLAG_ENABLE_BLEND;
  for (tmp = material->layers; tmp != NULL; tmp = tmp->next)
    {
      layer = tmp->data;
      if (layer->index == layer_index)
	{
	  CoglHandle handle =
	    _cogl_material_layer_handle_from_pointer (layer);
	  cogl_material_layer_unref (handle);
	  material->layers = g_list_remove (material->layers, layer);
	  continue;
	}

      /* XXX: If we expose manual control over ENABLE_BLEND, we'll add
       * a flag to know when it's user configured, so we don't trash it */
      if (cogl_texture_get_format (layer->texture) & COGL_A_BIT)
	material->flags |= COGL_MATERIAL_FLAG_ENABLE_BLEND;
    }
}

/* XXX: This API is hopfully just a stop-gap solution. Ideally cogl_enable
 * will be replaced. */
gulong
cogl_material_get_cogl_enable_flags (CoglHandle material_handle)
{
  CoglMaterial	*material;
  gulong	 enable_flags = 0;

  _COGL_GET_CONTEXT (ctx, 0);

  g_return_val_if_fail (cogl_is_material (material_handle), 0);

  material = _cogl_material_pointer_from_handle (material_handle);

  /* Enable blending if the geometry has an associated alpha color,
   * or the material wants blending enabled. */
  if (material->flags & COGL_MATERIAL_FLAG_ENABLE_BLEND
      || ctx->color_alpha < 255)
    enable_flags |= COGL_ENABLE_BLEND;

  return enable_flags;
}

/* It's a bit out of the ordinary to return a const GList *, but it's
 * probably sensible to try and avoid list manipulation for every
 * primitive emitted in a scene, every frame.
 *
 * Alternativly; we could either add a _foreach function, or maybe
 * a function that gets a passed a buffer (that may be stack allocated)
 * by the caller.
 */
const GList *
cogl_material_get_layers (CoglHandle material_handle)
{
  CoglMaterial	*material;

  g_return_val_if_fail (cogl_is_material (material_handle), NULL);

  material = _cogl_material_pointer_from_handle (material_handle);

  return material->layers;
}

CoglMaterialLayerType
cogl_material_layer_get_type (CoglHandle layer_handle)
{
  return COGL_MATERIAL_LAYER_TYPE_TEXTURE;
}

CoglHandle
cogl_material_layer_get_texture (CoglHandle layer_handle)
{
  CoglMaterialLayer *layer;

  g_return_val_if_fail (cogl_is_material_layer (layer_handle),
			COGL_INVALID_HANDLE);

  layer = _cogl_material_layer_pointer_from_handle (layer_handle);
  return layer->texture;
}

static guint
get_n_args_for_combine_func (CoglMaterialLayerCombineFunc func)
{
  switch (func)
    {
    case COGL_MATERIAL_LAYER_COMBINE_FUNC_REPLACE:
      return 1;
    case COGL_MATERIAL_LAYER_COMBINE_FUNC_MODULATE:
    case COGL_MATERIAL_LAYER_COMBINE_FUNC_ADD:
    case COGL_MATERIAL_LAYER_COMBINE_FUNC_ADD_SIGNED:
    case COGL_MATERIAL_LAYER_COMBINE_FUNC_SUBTRACT:
    case COGL_MATERIAL_LAYER_COMBINE_FUNC_DOT3_RGB:
    case COGL_MATERIAL_LAYER_COMBINE_FUNC_DOT3_RGBA:
      return 2;
    case COGL_MATERIAL_LAYER_COMBINE_FUNC_INTERPOLATE:
      return 3;
    }
  return 0;
}

void
cogl_material_layer_flush_gl_sampler_state (CoglHandle layer_handle)
{
  CoglMaterialLayer *layer;
  int n_rgb_func_args;
  int n_alpha_func_args;

  g_return_if_fail (cogl_is_material_layer (layer_handle));

  layer = _cogl_material_layer_pointer_from_handle (layer_handle);

  /* XXX: We really want some kind of cache/dirty flag mechanism
   * somewhere here so we can avoid as much mucking about with
   * the texture units per primitive as possible!
   *
   * E.g. some recent profiling of clutter-actor suggested that
   * validating/updating the texture environment may currently
   * be a significant bottleneck. Given that all the actors should
   * have the same texture environment, that implies we could do a
   * much better job of avoiding redundant glTexEnv calls.
   */

  GE (glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE));

  /* Set the combiner functions... */
  GE (glTexEnvi (GL_TEXTURE_ENV,
		 GL_COMBINE_RGB,
		 layer->texture_combine_rgb_func));
  GE (glTexEnvi (GL_TEXTURE_ENV,
		 GL_COMBINE_ALPHA,
		 layer->texture_combine_alpha_func));

  /*
   * Setup the function arguments...
   */

  /* For the RGB components... */
  n_rgb_func_args =
    get_n_args_for_combine_func (layer->texture_combine_rgb_func);

  GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB,
		 layer->texture_combine_rgb_src[0]));
  GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB,
		 layer->texture_combine_rgb_op[0]));
  if (n_rgb_func_args > 1)
    {
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB,
		     layer->texture_combine_rgb_src[1]));
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB,
		     layer->texture_combine_rgb_op[1]));
    }
  if (n_rgb_func_args > 2)
    {
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC2_RGB,
		     layer->texture_combine_rgb_src[2]));
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_RGB,
		     layer->texture_combine_rgb_op[2]));
    }

  /* For the Alpha component */
  n_alpha_func_args =
    get_n_args_for_combine_func (layer->texture_combine_alpha_func);

  GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA,
		 layer->texture_combine_alpha_src[0]));
  GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,
		 layer->texture_combine_alpha_op[0]));
  if (n_alpha_func_args > 1)
    {
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA,
		     layer->texture_combine_alpha_src[1]));
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA,
		     layer->texture_combine_alpha_op[1]));
    }
  if (n_alpha_func_args > 2)
    {
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC2_ALPHA,
		     layer->texture_combine_alpha_src[2]));
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_ALPHA,
		     layer->texture_combine_alpha_op[2]));
    }

  if (layer->flags & COGL_MATERIAL_LAYER_FLAG_USER_MATRIX)
    {
      GE (glMatrixMode (GL_TEXTURE));
      GE (glLoadMatrixf ((GLfloat *)&layer->matrix));
      GE (glMatrixMode (GL_MODELVIEW));
    }
}

/* TODO: Should go in cogl.c, but that implies duplication which is also
 * not ideal. */
void
cogl_set_source (CoglHandle material_handle)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_is_material (material_handle));

  if (ctx->source_material == material_handle)
    return;

  if (ctx->source_material)
    cogl_material_unref (ctx->source_material);

  cogl_material_ref (material_handle);
  ctx->source_material = material_handle;
}
/* TODO: add cogl_set_front_source (), and cogl_set_back_source () */

void
cogl_flush_material_gl_state (void)
{
  CoglMaterial *material;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  material = _cogl_material_pointer_from_handle (ctx->source_material);

  if (ctx->source_material == ctx->current_material
      && !(material->flags & COGL_MATERIAL_FLAG_DIRTY))
    return;

  /* FIXME - we only need to set these if lighting is enabled... */
  GE (glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, material->ambient));
  GE (glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, material->diffuse));
  GE (glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, material->specular));
  GE (glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, material->emission));
  GE (glMaterialfv (GL_FRONT_AND_BACK, GL_SHININESS, &material->shininess));

  /* NB: Currently the Cogl defines are compatible with the GL ones: */
  GE (glAlphaFunc (material->alpha_func, material->alpha_func_reference));

  GE (glBlendFunc (material->blend_src_factor, material->blend_dst_factor));

  ctx->current_material = ctx->source_material;
}

