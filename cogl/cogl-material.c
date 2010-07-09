/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-object.h"

#include "cogl-material-private.h"
#include "cogl-texture-private.h"
#include "cogl-blend-string.h"
#include "cogl-journal-private.h"
#include "cogl-color-private.h"
#include "cogl-profile.h"
#ifndef HAVE_COGL_GLES
#include "cogl-program.h"
#endif

#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>

/*
 * GL/GLES compatability defines for material thingies:
 */

#ifdef HAVE_COGL_GLES2
#include "../gles/cogl-gles2-wrapper.h"
#endif

#ifdef HAVE_COGL_GL
#define glActiveTexture ctx->drv.pf_glActiveTexture
#define glClientActiveTexture ctx->drv.pf_glClientActiveTexture
#define glBlendFuncSeparate ctx->drv.pf_glBlendFuncSeparate
#define glBlendEquation ctx->drv.pf_glBlendEquation
#define glBlendColor ctx->drv.pf_glBlendColor
#define glBlendEquationSeparate ctx->drv.pf_glBlendEquationSeparate

#define glProgramString ctx->drv.pf_glProgramString
#define glBindProgram ctx->drv.pf_glBindProgram
#define glDeletePrograms ctx->drv.pf_glDeletePrograms
#define glGenPrograms ctx->drv.pf_glGenPrograms
#define glProgramLocalParameter4fv ctx->drv.pf_glProgramLocalParameter4fv
#define glUseProgram ctx->drv.pf_glUseProgram
#endif

/* These aren't defined in the GLES headers */
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif
#ifndef GL_COORD_REPLACE
#define GL_COORD_REPLACE 0x8862
#endif
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812d
#endif

#define COGL_MATERIAL_LAYER(X) ((CoglMaterialLayer *)(X))

typedef gboolean (*CoglMaterialStateComparitor) (CoglMaterial *authority0,
                                                 CoglMaterial *authority1);

static CoglMaterialLayer *_cogl_material_layer_copy (CoglMaterialLayer *layer);

static void _cogl_material_free (CoglMaterial *tex);
static void _cogl_material_layer_free (CoglMaterialLayer *layer);
static void _cogl_material_add_layer_difference (CoglMaterial *material,
                                                 CoglMaterialLayer *layer,
                                                 gboolean inc_n_layers);
static void handle_automatic_blend_enable (CoglMaterial *material,
                                           CoglMaterialState changes);
static void recursively_free_layer_caches (CoglMaterial *material);

static const CoglMaterialBackend *backends[COGL_MATERIAL_N_BACKENDS];

#ifdef COGL_MATERIAL_BACKEND_GLSL
#include "cogl-material-glsl-private.h"
#endif
#ifdef COGL_MATERIAL_BACKEND_ARBFP
#include "cogl-material-arbfp-private.h"
#endif
#ifdef COGL_MATERIAL_BACKEND_FIXED
#include "cogl-material-fixed-private.h"
#endif

COGL_OBJECT_DEFINE (Material, material);
COGL_OBJECT_DEFINE_DEPRECATED_REF_COUNTING (material);
/* This type was made deprecated before the cogl_is_material_layer
   function was ever exposed in the public headers so there's no need
   to make the cogl_is_material_layer function public. We use INTERNAL
   so that the cogl_is_* function won't get defined */
COGL_OBJECT_INTERNAL_DEFINE (MaterialLayer, material_layer);

static void
texture_unit_init (CoglTextureUnit *unit, int index_)
{
  unit->index = index_;
  unit->enabled = FALSE;
  unit->current_gl_target = 0;
  unit->gl_texture = 0;
  unit->is_foreign = FALSE;
  unit->dirty_gl_texture = FALSE;
  unit->matrix_stack = _cogl_matrix_stack_new ();

  unit->layer = NULL;
  unit->layer_changes_since_flush = 0;
  unit->texture_storage_changed = FALSE;
}

static void
texture_unit_free (CoglTextureUnit *unit)
{
  if (unit->layer)
    cogl_object_unref (unit->layer);
  _cogl_matrix_stack_destroy (unit->matrix_stack);
}

CoglTextureUnit *
_cogl_get_texture_unit (int index_)
{
  _COGL_GET_CONTEXT (ctx, NULL);

  if (ctx->texture_units->len < (index_ + 1))
    {
      int i;
      int prev_len = ctx->texture_units->len;
      ctx->texture_units = g_array_set_size (ctx->texture_units, index_ + 1);
      for (i = prev_len; i <= index_; i++)
        {
          CoglTextureUnit *unit =
            &g_array_index (ctx->texture_units, CoglTextureUnit, i);

          texture_unit_init (unit, i);
        }
    }

  return &g_array_index (ctx->texture_units, CoglTextureUnit, index_);
}

void
_cogl_destroy_texture_units (void)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);
      texture_unit_free (unit);
    }
  g_array_free (ctx->texture_units, TRUE);
}

void
_cogl_set_active_texture_unit (int unit_index)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->active_texture_unit != unit_index)
    {
      GE (glActiveTexture (GL_TEXTURE0 + unit_index));
      ctx->active_texture_unit = unit_index;
    }
}

/* Note: _cogl_bind_gl_texture_transient conceptually has slightly
 * different semantics to OpenGL's glBindTexture because Cogl never
 * cares about tracking multiple textures bound to different targets
 * on the same texture unit.
 *
 * glBindTexture lets you bind multiple textures to a single texture
 * unit if they are bound to different targets. So it does something
 * like:
 *   unit->current_texture[target] = texture;
 *
 * Cogl only lets you associate one texture with the currently active
 * texture unit, so the target is basically a redundant parameter
 * that's implicitly set on that texture.
 *
 * Technically this is just a thin wrapper around glBindTexture so
 * actually it does have the GL semantics but it seems worth
 * mentioning the conceptual difference in case anyone wonders why we
 * don't associate the gl_texture with a gl_target in the
 * CoglTextureUnit.
 */
void
_cogl_bind_gl_texture_transient (GLenum gl_target,
                                 GLuint gl_texture,
                                 gboolean is_foreign)
{
  CoglTextureUnit *unit;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We choose to always make texture unit 1 active for transient
   * binds so that in the common case where multitexturing isn't used
   * we can simply ignore the state of this texture unit. Notably we
   * didn't use a large texture unit (.e.g. (GL_MAX_TEXTURE_UNITS - 1)
   * in case the driver doesn't have a sparse data structure for
   * texture units.
   */
  _cogl_set_active_texture_unit (1);
  unit = _cogl_get_texture_unit (1);

  /* NB: If we have previously bound a foreign texture to this texture
   * unit we don't know if that texture has since been deleted and we
   * are seeing the texture name recycled */
  if (unit->gl_texture == gl_texture &&
      !unit->dirty_gl_texture &&
      !unit->is_foreign)
    return;

  GE (glBindTexture (gl_target, gl_texture));

  unit->dirty_gl_texture = TRUE;
  unit->is_foreign = is_foreign;
}

void
_cogl_delete_gl_texture (GLuint gl_texture)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);

      if (unit->gl_texture == gl_texture)
        {
          unit->gl_texture = 0;
          unit->dirty_gl_texture = FALSE;
        }
    }

  GE (glDeleteTextures (1, &gl_texture));
}

/* Whenever the underlying GL texture storage of a CoglTexture is
 * changed (e.g. due to migration out of a texture atlas) then we are
 * notified. This lets us ensure that we reflush that texture's state
 * if it reused again with the same texture unit.
 */
void
_cogl_material_texture_storage_change_notify (CoglHandle texture)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);

      if (unit->layer &&
          unit->layer->texture == texture)
        unit->texture_storage_changed = TRUE;

      /* NB: the texture may be bound to multiple texture units so
       * we continue to check the rest */
    }
}

GQuark
_cogl_material_error_quark (void)
{
  return g_quark_from_static_string ("cogl-material-error-quark");
}

static void
_cogl_material_node_init (CoglMaterialNode *node)
{
  node->parent = NULL;
  node->has_children = FALSE;
}

static void
_cogl_material_node_set_parent_real (CoglMaterialNode *node,
                                     CoglMaterialNode *parent,
                                     CoglMaterialNodeUnparentVFunc unparent)
{
  /* NB: the old parent may indirectly be keeping the new parent alive so we
   * have to ref the new parent before unrefing the old */
  cogl_object_ref (parent);

  if (node->parent)
    unparent (node);

  if (G_UNLIKELY (parent->has_children))
    parent->children = g_list_prepend (parent->children, node);
  else
    {
      parent->has_children = TRUE;
      parent->first_child = node;
      parent->children = NULL;
    }

  node->parent = parent;
}

static void
_cogl_material_node_unparent_real (CoglMaterialNode *node)
{
  CoglMaterialNode *parent = node->parent;

  if (parent == NULL)
    return;

  g_return_if_fail (parent->has_children);

  if (parent->first_child == node)
    {
      if (parent->children)
        {
          parent->first_child = parent->children->data;
          parent->children =
            g_list_delete_link (parent->children, parent->children);
        }
      else
        parent->has_children = FALSE;
    }
  else
    parent->children = g_list_remove (parent->children, node);

  cogl_object_unref (parent);

  node->parent = NULL;
}

void
_cogl_material_node_foreach_child (CoglMaterialNode *node,
                                   CoglMaterialNodeChildCallback callback,
                                   void *user_data)
{
  if (node->has_children)
    {
      callback (node->first_child, user_data);
      g_list_foreach (node->children, (GFunc)callback, user_data);
    }
}

/*
 * This initializes the first material owned by the Cogl context. All
 * subsequently instantiated materials created via the cogl_material_new()
 * API will initially be a copy of this material.
 *
 * The default material is the topmost ancester for all materials.
 */
void
_cogl_material_init_default_material (void)
{
  /* Create new - blank - material */
  CoglMaterial *material = g_slice_new0 (CoglMaterial);
  CoglMaterialBigState *big_state = g_slice_new0 (CoglMaterialBigState);
  CoglMaterialLightingState *lighting_state = &big_state->lighting_state;
  CoglMaterialAlphaFuncState *alpha_state = &big_state->alpha_state;
  CoglMaterialBlendState *blend_state = &big_state->blend_state;
  CoglMaterialDepthState *depth_state = &big_state->depth_state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Take this opportunity to setup the fragment processing backends... */
#ifdef COGL_MATERIAL_BACKEND_GLSL
  backends[COGL_MATERIAL_BACKEND_GLSL] = &_cogl_material_glsl_backend;
#endif
#ifdef COGL_MATERIAL_BACKEND_ARBFP
  backends[COGL_MATERIAL_BACKEND_ARBFP] = &_cogl_material_arbfp_backend;
#endif
#ifdef COGL_MATERIAL_BACKEND_FIXED
  backends[COGL_MATERIAL_BACKEND_FIXED] = &_cogl_material_fixed_backend;
#endif

  _cogl_material_node_init (COGL_MATERIAL_NODE (material));

  material->is_weak = FALSE;
  material->journal_ref_count = 0;
  material->backend = COGL_MATERIAL_BACKEND_UNDEFINED;
  material->differences = COGL_MATERIAL_STATE_ALL_SPARSE;

  material->real_blend_enable = FALSE;

  material->blend_enable = COGL_MATERIAL_BLEND_ENABLE_AUTOMATIC;
  material->layer_differences = NULL;
  material->n_layers = 0;

  material->big_state = big_state;
  material->has_big_state = TRUE;

  material->static_breadcrumb = "default material";
  material->has_static_breadcrumb = TRUE;

  material->age = 0;

  /* Use the same defaults as the GL spec... */
  cogl_color_init_from_4ub (&material->color, 0xff, 0xff, 0xff, 0xff);

  /* Use the same defaults as the GL spec... */
  lighting_state->ambient[0] = 0.2;
  lighting_state->ambient[1] = 0.2;
  lighting_state->ambient[2] = 0.2;
  lighting_state->ambient[3] = 1.0;

  lighting_state->diffuse[0] = 0.8;
  lighting_state->diffuse[1] = 0.8;
  lighting_state->diffuse[2] = 0.8;
  lighting_state->diffuse[3] = 1.0;

  lighting_state->specular[0] = 0;
  lighting_state->specular[1] = 0;
  lighting_state->specular[2] = 0;
  lighting_state->specular[3] = 1.0;

  lighting_state->emission[0] = 0;
  lighting_state->emission[1] = 0;
  lighting_state->emission[2] = 0;
  lighting_state->emission[3] = 1.0;

  /* Use the same defaults as the GL spec... */
  alpha_state->alpha_func = COGL_MATERIAL_ALPHA_FUNC_ALWAYS;
  alpha_state->alpha_func_reference = 0.0;

  /* Not the same as the GL default, but seems saner... */
#ifndef HAVE_COGL_GLES
  blend_state->blend_equation_rgb = GL_FUNC_ADD;
  blend_state->blend_equation_alpha = GL_FUNC_ADD;
  blend_state->blend_src_factor_alpha = GL_ONE;
  blend_state->blend_dst_factor_alpha = GL_ONE_MINUS_SRC_ALPHA;
  cogl_color_init_from_4ub (&blend_state->blend_constant,
                            0x00, 0x00, 0x00, 0x00);
#endif
  blend_state->blend_src_factor_rgb = GL_ONE;
  blend_state->blend_dst_factor_rgb = GL_ONE_MINUS_SRC_ALPHA;

  big_state->user_program = COGL_INVALID_HANDLE;

  /* The same as the GL defaults */
  depth_state->depth_test_enabled = FALSE;
  depth_state->depth_test_function = COGL_DEPTH_TEST_FUNCTION_LESS;
  depth_state->depth_writing_enabled = TRUE;
  depth_state->depth_range_near = 0;
  depth_state->depth_range_far = 1;

  big_state->point_size = 1.0f;

  ctx->default_material = _cogl_material_object_new (material);
}

static void
_cogl_material_unparent (CoglMaterialNode *material)
{
  /* Chain up */
  _cogl_material_node_unparent_real (material);
}

static gboolean
recursively_free_layer_caches_cb (CoglMaterialNode *node,
                                  void *user_data)
{
  recursively_free_layer_caches (COGL_MATERIAL (node));
  return TRUE;
}

/* This recursively frees the layers_cache of a material and all of
 * its descendants.
 *
 * For instance if we change a materials ->layer_differences list
 * then that material and all of its descendants may now have
 * incorrect layer caches. */
static void
recursively_free_layer_caches (CoglMaterial *material)
{
  /* Note: we maintain the invariable that if a material already has a
   * dirty layers_cache then so do all of its descendants. */
  if (material->layers_cache_dirty)
    return;

  if (G_UNLIKELY (material->layers_cache != material->short_layers_cache))
    g_slice_free1 (sizeof (CoglMaterialLayer *) * material->n_layers,
                   material->layers_cache);
  material->layers_cache_dirty = TRUE;

  _cogl_material_node_foreach_child (COGL_MATERIAL_NODE (material),
                                     recursively_free_layer_caches_cb,
                                     NULL);
}

static void
_cogl_material_set_parent (CoglMaterial *material, CoglMaterial *parent)
{
  /* Chain up */
  _cogl_material_node_set_parent_real (COGL_MATERIAL_NODE (material),
                                       COGL_MATERIAL_NODE (parent),
                                       _cogl_material_unparent);

  /* Since we just changed the ancestry of the material its cache of
   * layers could now be invalid so free it... */
  if (material->differences & COGL_MATERIAL_STATE_LAYERS)
    recursively_free_layer_caches (material);

  /* If the fragment processing backend is also caching state along
   * with the material that depends on the material's ancestry then it
   * may be notified here...
   */
  if (material->backend != COGL_MATERIAL_BACKEND_UNDEFINED &&
      backends[material->backend]->material_set_parent_notify)
    backends[material->backend]->material_set_parent_notify (material);
}

/* XXX: Always have an eye out for opportunities to lower the cost of
 * cogl_material_copy. */
CoglMaterial *
cogl_material_copy (CoglMaterial *src)
{
  CoglMaterial *material = g_slice_new (CoglMaterial);

  _cogl_material_node_init (COGL_MATERIAL_NODE (material));

  material->is_weak = FALSE;

  material->journal_ref_count = 0;

  material->differences = 0;

  material->has_big_state = FALSE;

  /* NB: real_blend_enable isn't a sparse property, it's valid for
   * every material node so we have fast access to it. */
  material->real_blend_enable = src->real_blend_enable;

  /* XXX:
   * consider generalizing the idea of "cached" properties. These
   * would still have an authority like other sparse properties but
   * you wouldn't have to walk up the ancestry to find the authority
   * because the value would be cached directly in each material.
   */

  material->layers_cache_dirty = TRUE;
  material->deprecated_get_layers_list_dirty = TRUE;

  material->backend = src->backend;
  material->backend_priv_set_mask = 0;

  material->has_static_breadcrumb = FALSE;

  material->age = 0;

  _cogl_material_set_parent (material, src);

  return _cogl_material_object_new (material);
}

/* XXX: we should give this more thought before making anything like
 * this API public! */
CoglMaterial *
_cogl_material_weak_copy (CoglMaterial *material)
{
  CoglMaterial *copy;
  CoglMaterial *copy_material;

  /* If we make a public API we might want want to allow weak copies
   * of weak material? */
  g_return_val_if_fail (!material->is_weak, NULL);

  copy = cogl_material_copy (material);
  copy_material = COGL_MATERIAL (copy);
  copy_material->is_weak = TRUE;

  return copy;
}

CoglMaterial *
cogl_material_new (void)
{
  CoglMaterial *new;

  _COGL_GET_CONTEXT (ctx, NULL);

  new = cogl_material_copy (ctx->default_material);
  _cogl_material_set_static_breadcrumb (new, "new");
  return new;
}

static void
_cogl_material_backend_free_priv (CoglMaterial *material)
{
  if (material->backend != COGL_MATERIAL_BACKEND_UNDEFINED &&
      backends[material->backend]->free_priv)
    backends[material->backend]->free_priv (material);
}

static void
_cogl_material_free (CoglMaterial *material)
{
  _cogl_material_backend_free_priv (material);

  _cogl_material_unparent (COGL_MATERIAL_NODE (material));

  if (material->differences & COGL_MATERIAL_STATE_USER_SHADER &&
      material->big_state->user_program)
    cogl_handle_unref (material->big_state->user_program);

  if (material->differences & COGL_MATERIAL_STATE_NEEDS_BIG_STATE)
    g_slice_free (CoglMaterialBigState, material->big_state);

  if (material->differences & COGL_MATERIAL_STATE_LAYERS)
    {
      g_list_foreach (material->layer_differences,
                      (GFunc)cogl_object_unref, NULL);
      g_list_free (material->layer_differences);
    }

  g_slice_free (CoglMaterial, material);
}

gboolean
_cogl_material_get_real_blend_enabled (CoglMaterial *material)
{
  g_return_val_if_fail (cogl_is_material (material), FALSE);

  return material->real_blend_enable;
}

inline CoglMaterial *
_cogl_material_get_parent (CoglMaterial *material)
{
  CoglMaterialNode *parent_node = COGL_MATERIAL_NODE (material)->parent;
  return COGL_MATERIAL (parent_node);
}

CoglMaterial *
_cogl_material_get_authority (CoglMaterial *material,
                              unsigned long difference)
{
  CoglMaterial *authority = material;
  while (!(authority->differences & difference))
    authority = _cogl_material_get_parent (authority);
  return authority;
}

/* XXX: Think twice before making this non static since it is used
 * heavily and we expect the compiler to inline it...
 */
static CoglMaterialLayer *
_cogl_material_layer_get_parent (CoglMaterialLayer *layer)
{
  CoglMaterialNode *parent_node = COGL_MATERIAL_NODE (layer)->parent;
  return COGL_MATERIAL_LAYER (parent_node);
}

CoglMaterialLayer *
_cogl_material_layer_get_authority (CoglMaterialLayer *layer,
                                    unsigned long difference)
{
  CoglMaterialLayer *authority = layer;
  while (!(authority->differences & difference))
    authority = _cogl_material_layer_get_parent (authority);
  return authority;
}

int
_cogl_material_layer_get_unit_index (CoglMaterialLayer *layer)
{
  CoglMaterialLayer *authority =
    _cogl_material_layer_get_authority (layer, COGL_MATERIAL_LAYER_STATE_UNIT);
  return authority->unit_index;
}

static void
_cogl_material_update_layers_cache (CoglMaterial *material)
{
  /* Note: we assume this material is a _LAYERS authority */
  int n_layers;
  CoglMaterial *current;
  int layers_found;

  if (G_LIKELY (!material->layers_cache_dirty) ||
      material->n_layers == 0)
    return;

  material->layers_cache_dirty = FALSE;

  n_layers = material->n_layers;
  if (G_LIKELY (n_layers < G_N_ELEMENTS (material->short_layers_cache)))
    {
      material->layers_cache = material->short_layers_cache;
      memset (material->layers_cache, 0,
              sizeof (CoglMaterialLayer *) *
              G_N_ELEMENTS (material->short_layers_cache));
    }
  else
    {
      material->layers_cache =
        g_slice_alloc0 (sizeof (CoglMaterialLayer *) * n_layers);
    }

  /* Notes:
   *
   * Each material doesn't have to contain a complete list of the layers
   * it depends on, some of them are indirectly referenced through the
   * material's ancestors.
   *
   * material->layer_differences only contains a list of layers that
   * have changed in relation to its parent.
   *
   * material->layer_differences is not maintained sorted, but it
   * won't contain multiple layers corresponding to a particular
   * ->unit_index.
   *
   * Some of the ancestor materials may reference layers with
   * ->unit_index values >= n_layers so we ignore them.
   *
   * As we ascend through the ancestors we are searching for any
   * CoglMaterialLayers corresponding to the texture ->unit_index
   * values in the range [0,n_layers-1]. As soon as a pointer is found
   * we ignore layers of further ancestors with the same ->unit_index
   * values.
   */

  layers_found = 0;
  for (current = material;
       _cogl_material_get_parent (current);
       current = _cogl_material_get_parent (current))
    {
      GList *l;

      if (!(current->differences & COGL_MATERIAL_STATE_LAYERS))
        continue;

      for (l = current->layer_differences; l; l = l->next)
        {
          CoglMaterialLayer *layer = l->data;
          int unit_index = _cogl_material_layer_get_unit_index (layer);

          if (unit_index < n_layers && !material->layers_cache[unit_index])
            {
              material->layers_cache[unit_index] = layer;
              layers_found++;
              if (layers_found == n_layers)
                return;
            }
        }
    }

  g_warn_if_reached ();
}

/* TODO: add public cogl_material_foreach_layer but instead of passing
 * a CoglMaterialLayer pointer to the callback we should pass a
 * layer_index instead. */

void
_cogl_material_foreach_layer (CoglMaterial *material,
                              CoglMaterialLayerCallback callback,
                              void *user_data)
{
  CoglMaterial *authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_LAYERS);
  int n_layers;
  int i;
  gboolean cont;

  n_layers = authority->n_layers;
  if (n_layers == 0)
    return;

  _cogl_material_update_layers_cache (authority);

  for (i = 0, cont = TRUE; i < n_layers && cont == TRUE; i++)
    cont = callback (authority->layers_cache[i], user_data);
}

static gboolean
layer_has_alpha_cb (CoglMaterialLayer *layer, void *data)
{
  CoglMaterialLayer *combine_authority =
    _cogl_material_layer_get_authority (layer,
                                        COGL_MATERIAL_LAYER_STATE_COMBINE);
  CoglMaterialLayerBigState *big_state = combine_authority->big_state;
  CoglMaterialLayer *tex_authority;
  gboolean *has_alpha = data;

  /* has_alpha maintains the alpha status for the GL_PREVIOUS layer */

  /* For anything but the default texture combine we currently just
   * assume it may result in an alpha value < 1
   *
   * FIXME: we could do better than this. */
  if (big_state->texture_combine_alpha_func != GL_MODULATE ||
      big_state->texture_combine_alpha_src[0] != GL_PREVIOUS ||
      big_state->texture_combine_alpha_op[0] != GL_SRC_ALPHA ||
      big_state->texture_combine_alpha_src[0] != GL_TEXTURE ||
      big_state->texture_combine_alpha_op[0] != GL_SRC_ALPHA)
    {
      *has_alpha = TRUE;
      /* return FALSE to stop iterating layers... */
      return FALSE;
    }

  /* NB: A layer may have a combine mode set on it but not yet
   * have an associated texture which would mean we'd fallback
   * to the default texture which doesn't have an alpha component
   */
  tex_authority =
    _cogl_material_layer_get_authority (layer,
                                        COGL_MATERIAL_LAYER_STATE_TEXTURE);
  if (tex_authority->texture &&
      cogl_texture_get_format (tex_authority->texture) & COGL_A_BIT)
    {
      *has_alpha = TRUE;
      /* return FALSE to stop iterating layers... */
      return FALSE;
    }

  *has_alpha = FALSE;
  /* return FALSE to continue iterating layers... */
  return TRUE;
}

static CoglMaterial *
_cogl_material_get_user_program (CoglMaterial *material)
{
  CoglMaterial *authority;

  g_return_val_if_fail (cogl_is_material (material), NULL);

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_USER_SHADER);

  return authority->big_state->user_program;
}

static gboolean
_cogl_material_needs_blending_enabled (CoglMaterial    *material,
                                       unsigned long    changes,
                                       const CoglColor *override_color)
{
  CoglMaterial *enable_authority;
  CoglMaterial *blend_authority;
  CoglMaterialBlendState *blend_state;
  CoglMaterialBlendEnable enabled;
  unsigned long other_state;

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DISABLE_BLENDING))
    return FALSE;

  enable_authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_BLEND_ENABLE);

  enabled = enable_authority->blend_enable;
  if (enabled != COGL_MATERIAL_BLEND_ENABLE_AUTOMATIC)
    return enabled == COGL_MATERIAL_BLEND_ENABLE_ENABLED ? TRUE : FALSE;

  blend_authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_BLEND);

  blend_state = &blend_authority->big_state->blend_state;

  /* We are trying to identify awkward cases that are equivalent to
   * blending being disable, where the output is simply GL_SRC_COLOR.
   *
   * Note: we assume that all OpenGL drivers will identify the simple
   * case of ADD (ONE, ZERO) as equivalent to blending being disabled.
   *
   * We should update this when we add support for more blend
   * functions...
   */

#ifndef HAVE_COGL_GLES
  /* GLES 1 can't change the function or have separate alpha factors */
  if (blend_state->blend_equation_rgb != GL_FUNC_ADD ||
      blend_state->blend_equation_alpha != GL_FUNC_ADD)
    return TRUE;

  if (blend_state->blend_src_factor_alpha != GL_ONE ||
      blend_state->blend_dst_factor_alpha != GL_ONE_MINUS_SRC_ALPHA)
    return TRUE;
#endif

  if (blend_state->blend_src_factor_rgb != GL_ONE ||
      blend_state->blend_dst_factor_rgb != GL_ONE_MINUS_SRC_ALPHA)
    return TRUE;

  /* Given the above constraints, it's now a case of finding any
   * SRC_ALPHA that != 1 */

  /* In the case of a layer state change we need to check everything
   * else first since they contribute to the has_alpha status of the
   * GL_PREVIOUS layer. */
  if (changes & COGL_MATERIAL_STATE_LAYERS)
    changes = COGL_MATERIAL_STATE_AFFECTS_BLENDING;

  /* XXX: we don't currently handle specific changes in an optimal way*/
  changes = COGL_MATERIAL_STATE_AFFECTS_BLENDING;

  if ((override_color && cogl_color_get_alpha_byte (override_color) != 0xff))
    return TRUE;

  if (changes & COGL_MATERIAL_STATE_COLOR)
    {
      CoglColor tmp;
      cogl_material_get_color (material, &tmp);
      if (cogl_color_get_alpha_byte (&tmp) != 0xff)
        return TRUE;
    }

  /* We can't make any assumptions about the alpha channel if the user
   * is using an unknown fragment shader.
   *
   * TODO: check that it isn't just a vertex shader!
   */
  if (changes & COGL_MATERIAL_STATE_USER_SHADER)
    {
      if (_cogl_material_get_user_program (material) != COGL_INVALID_HANDLE)
        return TRUE;
    }

  /* XXX: we should only need to look at these if lighting is enabled
   */
  if (changes & COGL_MATERIAL_STATE_LIGHTING)
    {
      CoglColor tmp;

      cogl_material_get_ambient (material, &tmp);
      if (cogl_color_get_alpha_byte (&tmp) != 0xff)
        return TRUE;
      cogl_material_get_diffuse (material, &tmp);
      if (cogl_color_get_alpha_byte (&tmp) != 0xff)
        return TRUE;
      cogl_material_get_specular (material, &tmp);
      if (cogl_color_get_alpha_byte (&tmp) != 0xff)
        return TRUE;
      cogl_material_get_emission (material, &tmp);
      if (cogl_color_get_alpha_byte (&tmp) != 0xff)
        return TRUE;
    }

  if (changes & COGL_MATERIAL_STATE_LAYERS)
    {
      /* has_alpha tracks the alpha status of the GL_PREVIOUS layer.
       * To start with that's defined by the material color which
       * must be fully opaque if we got this far. */
      gboolean has_alpha = FALSE;
      _cogl_material_foreach_layer (material,
                                    layer_has_alpha_cb,
                                    &has_alpha);
      if (has_alpha)
        return TRUE;
    }

  /* So far we have only checked the property that has been changed so
   * we now need to check all the other properties too. */
  other_state = COGL_MATERIAL_STATE_AFFECTS_BLENDING & ~changes;
  if (other_state &&
      _cogl_material_needs_blending_enabled (material,
                                             other_state,
                                             NULL))
    return TRUE;

  return FALSE;
}

static void
_cogl_material_set_backend (CoglMaterial *material, int backend)
{
  _cogl_material_backend_free_priv (material);
  material->backend = backend;
}

static void
_cogl_material_copy_differences (CoglMaterial *dest,
                                 CoglMaterial *src,
                                 unsigned long differences)
{
  CoglMaterialBigState *big_state;

  if (differences & COGL_MATERIAL_STATE_COLOR)
    dest->color = src->color;

  if (differences & COGL_MATERIAL_STATE_BLEND_ENABLE)
    dest->blend_enable = src->blend_enable;

  if (differences & COGL_MATERIAL_STATE_LAYERS)
    {
      GList *l;

      if (dest->differences & COGL_MATERIAL_STATE_LAYERS &&
          dest->layer_differences)
        {
          g_list_foreach (dest->layer_differences,
                          (GFunc)cogl_object_unref,
                          NULL);
          g_list_free (dest->layer_differences);
        }

      dest->n_layers = src->n_layers;
      dest->layer_differences = g_list_copy (src->layer_differences);

      for (l = src->layer_differences; l; l = l->next)
        {
          /* NB: a layer can't have more than one ->owner so we can't
           * simply take a references on each of the original
           * layer_differences, we have to derive new layers from the
           * originals instead. */
          CoglMaterialLayer *copy = _cogl_material_layer_copy (l->data);
          _cogl_material_add_layer_difference (dest, copy, FALSE);
          cogl_object_unref (copy);
        }
    }

  if (differences & COGL_MATERIAL_STATE_NEEDS_BIG_STATE)
    {
      if (!dest->has_big_state)
        {
          dest->big_state = g_slice_new (CoglMaterialBigState);
          dest->has_big_state = TRUE;
        }
      big_state = dest->big_state;
    }
  else
    goto check_for_blending_change;

  if (differences & COGL_MATERIAL_STATE_LIGHTING)
    {
      memcpy (&big_state->lighting_state,
              &src->big_state->lighting_state,
              sizeof (CoglMaterialLightingState));
    }

  if (differences & COGL_MATERIAL_STATE_ALPHA_FUNC)
    {
      memcpy (&big_state->alpha_state,
              &src->big_state->alpha_state,
              sizeof (CoglMaterialAlphaFuncState));
    }

  if (differences & COGL_MATERIAL_STATE_BLEND)
    {
      memcpy (&big_state->blend_state,
              &src->big_state->blend_state,
              sizeof (CoglMaterialBlendState));
    }

  if (differences & COGL_MATERIAL_STATE_USER_SHADER)
    {
      if (src->big_state->user_program)
        big_state->user_program =
          cogl_handle_ref (src->big_state->user_program);
      else
        big_state->user_program = COGL_INVALID_HANDLE;
    }

  if (differences & COGL_MATERIAL_STATE_DEPTH)
    {
      memcpy (&big_state->depth_state,
              &src->big_state->depth_state,
              sizeof (CoglMaterialDepthState));
    }

  if (differences & COGL_MATERIAL_STATE_POINT_SIZE)
    big_state->point_size = src->big_state->point_size;

  /* XXX: we shouldn't bother doing this in most cases since
   * _copy_differences is typically used to initialize material state
   * by copying it from the current authority, so it's not actually
   * *changing* anything.
   */
check_for_blending_change:
  if (differences & COGL_MATERIAL_STATE_AFFECTS_BLENDING)
    handle_automatic_blend_enable (dest, differences);

  dest->differences |= differences;
}

static void
_cogl_material_initialize_state (CoglMaterial *dest,
                                 CoglMaterial *src,
                                 CoglMaterialState state)
{
  if (dest == src)
    return;

  if (state != COGL_MATERIAL_STATE_LAYERS)
    _cogl_material_copy_differences (dest, src, state);
  else
    {
      dest->n_layers = src->n_layers;
      dest->layer_differences = NULL;
    }
}

static gboolean
check_if_strong_cb (CoglMaterialNode *node, void *user_data)
{
  CoglMaterial *material = COGL_MATERIAL (node);
  gboolean *has_strong_child = user_data;

  if (!material->is_weak)
    {
      *has_strong_child = TRUE;
      return FALSE;
    }
  return TRUE;
}

static gboolean
has_strong_children (CoglMaterial *material)
{
  gboolean has_strong_child = FALSE;
  _cogl_material_node_foreach_child (COGL_MATERIAL_NODE (material),
                                     check_if_strong_cb,
                                     &has_strong_child);
  return has_strong_child;
}

static gboolean
reparent_strong_children_cb (CoglMaterialNode *node,
                             void *user_data)
{
  CoglMaterial *material = COGL_MATERIAL (node);
  CoglMaterial *parent = user_data;

  if (material->is_weak)
    return TRUE;

  _cogl_material_set_parent (material, parent);

  return TRUE;
}

static void
_cogl_material_pre_change_notify (CoglMaterial     *material,
                                  CoglMaterialState change,
                                  const CoglColor  *new_color)
{
  CoglMaterial *authority;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* If primitives have been logged in the journal referencing the
   * current state of this material we need to flush the journal
   * before we can modify it... */
  if (material->journal_ref_count)
    {
      gboolean skip_journal_flush = FALSE;

      /* XXX: We don't usually need to flush the journal just due to
       * color changes since material colors are logged in the
       * journal's vertex buffer. The exception is when the change in
       * color enables or disables the need for blending. */
      if (change == COGL_MATERIAL_STATE_COLOR)
        {
          gboolean will_need_blending =
            _cogl_material_needs_blending_enabled (material,
                                                   change,
                                                   new_color);
          gboolean blend_enable = material->real_blend_enable ? TRUE : FALSE;

          if (will_need_blending == blend_enable)
            skip_journal_flush = TRUE;
        }

      if (!skip_journal_flush)
        _cogl_journal_flush ();
    }

  /* The fixed function backend has no private state and can't
   * do anything special to handle small material changes so we may as
   * well try to find a better backend whenever the material changes.
   *
   * The programmable backends may be able to cache a lot of the code
   * they generate and only need to update a small section of that
   * code in response to a material change therefore we don't want to
   * try searching for another backend when the material changes.
   */
  if (material->backend == COGL_MATERIAL_BACKEND_FIXED)
    _cogl_material_set_backend (material, COGL_MATERIAL_BACKEND_UNDEFINED);

  if (material->backend != COGL_MATERIAL_BACKEND_UNDEFINED &&
      backends[material->backend]->material_pre_change_notify)
    backends[material->backend]->material_pre_change_notify (material,
                                                             change,
                                                             new_color);

  /*
   * There is an arbitrary tree of descendants of this material; any of
   * which may indirectly depend on this material as the authority for
   * some set of properties. (Meaning for example that one of its
   * descendants derives its color or blending state from this
   * material.)
   *
   * We can't modify any property that this material is the authority
   * for unless we create another material to take its place first and
   * make sure descendants reference this new material instead.
   */
  if (has_strong_children (material))
    {
      CoglMaterial *new_authority;

      COGL_STATIC_COUNTER (material_copy_on_write_counter,
                           "material copy on write counter",
                           "Increments each time a material "
                           "must be copied to allow modification",
                           0 /* no application private data */);

      COGL_COUNTER_INC (_cogl_uprof_context, material_copy_on_write_counter);

      new_authority =
        cogl_material_copy (_cogl_material_get_parent (material));
      _cogl_material_set_static_breadcrumb (new_authority,
                                            "pre_change_notify:copy-on-write");

      /* We could explicitly walk the descendants, OR together the set
       * of differences that we determine this material is the
       * authority on and only copy those differences copied across.
       *
       * Or, if we don't explicitly walk the descendants we at least
       * know that material->differences represents the largest set of
       * differences that this material could possibly be an authority
       * on.
       *
       * We do the later just because it's simplest, but we might need
       * to come back to this later...
       */
      _cogl_material_copy_differences (new_authority, material,
                                       material->differences);

      /* Reparent the strong children of material to be children of
       * new_authority instead... */
      _cogl_material_node_foreach_child (COGL_MATERIAL_NODE (material),
                                         reparent_strong_children_cb,
                                         new_authority);

      /* The children will keep the new authority alive so drop the
       * reference we got when copying... */
      cogl_object_unref (new_authority);
    }

  /* At this point we know we have a material with no strong
   * dependants (though we may have some weak children) so we are now
   * free to modify the material. */

  material->age++;

  /* If the material isn't already an authority for the state group
   * being modified then we need to initialize the corresponding
   * state. */
  if (change & COGL_MATERIAL_STATE_ALL_SPARSE)
    authority = _cogl_material_get_authority (material, change);
  else
    authority = material;
  _cogl_material_initialize_state (material, authority, change);

  /* Each material has a sorted cache of the layers it depends on
   * which will need updating via _cogl_material_update_layers_cache
   * if a material's layers are changed. */
  if (change == COGL_MATERIAL_STATE_LAYERS)
    recursively_free_layer_caches (material);

  /* If the material being changed is the same as the last material we
   * flushed then we keep a track of the changes so we can try to
   * minimize redundant OpenGL calls if the same material is flushed
   * again.
   */
  if (ctx->current_material == material)
    ctx->current_material_changes_since_flush |= change;
}


static void
_cogl_material_add_layer_difference (CoglMaterial *material,
                                     CoglMaterialLayer *layer,
                                     gboolean inc_n_layers)
{
  g_return_if_fail (layer->owner == NULL);

  layer->owner = material;
  cogl_object_ref (layer);

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_STATE_LAYERS,
                                    NULL);

  material->differences |= COGL_MATERIAL_STATE_LAYERS;

  material->layer_differences =
    g_list_prepend (material->layer_differences, layer);

  if (inc_n_layers)
    material->n_layers++;
}

/* NB: If you are calling this it's your responsibility to have
 * already called:
 *   _cogl_material_pre_change_notify (m, _CHANGE_LAYERS, NULL);
 */
static void
_cogl_material_remove_layer_difference (CoglMaterial *material,
                                        CoglMaterialLayer *layer,
                                        gboolean dec_n_layers)
{
  g_return_if_fail (layer->owner == material);

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material,
                                    COGL_MATERIAL_STATE_LAYERS,
                                    NULL);

  layer->owner = NULL;
  cogl_object_unref (layer);

  material->differences |= COGL_MATERIAL_STATE_LAYERS;

  material->layer_differences =
    g_list_remove (material->layer_differences, layer);

  if (dec_n_layers)
    material->n_layers--;
}

static void
_cogl_material_try_reverting_layers_authority (CoglMaterial *authority,
                                               CoglMaterial *old_authority)
{
  if (authority->layer_differences == NULL &&
      _cogl_material_get_parent (authority))
    {
      /* If the previous _STATE_LAYERS authority has the same
       * ->n_layers then we can revert to that being the authority
       *  again. */
      if (!old_authority)
        {
          old_authority =
            _cogl_material_get_authority (_cogl_material_get_parent (authority),
                                          COGL_MATERIAL_STATE_LAYERS);
        }

      if (old_authority->n_layers == authority->n_layers)
        authority->differences &= ~COGL_MATERIAL_STATE_LAYERS;
    }
}


static void
handle_automatic_blend_enable (CoglMaterial *material,
                               CoglMaterialState change)
{
  gboolean blend_enable =
    _cogl_material_needs_blending_enabled (material, change, NULL);

  if (blend_enable != material->real_blend_enable)
    {
      /* - Flush journal primitives referencing the current state.
       * - Make sure the material has no dependants so it may be
       *   modified.
       * - If the material isn't currently an authority for the state
       *   being changed, then initialize that state from the current
       *   authority.
       */
      _cogl_material_pre_change_notify (material,
                                        COGL_MATERIAL_STATE_REAL_BLEND_ENABLE,
                                        NULL);
      material->real_blend_enable = blend_enable;
    }
}

typedef struct
{
  int keep_n;
  int current_pos;
  gboolean needs_pruning;
  int first_index_to_prune;
} CoglMaterialPruneLayersInfo;

static gboolean
update_prune_layers_info_cb (CoglMaterialLayer *layer, void *user_data)
{
  CoglMaterialPruneLayersInfo *state = user_data;

  if (state->current_pos == state->keep_n)
    {
      state->needs_pruning = TRUE;
      state->first_index_to_prune = layer->index;
      return FALSE;
    }
  state->current_pos++;
  return TRUE;
}

void
_cogl_material_prune_to_n_layers (CoglMaterial *material, int n)
{
  CoglMaterialPruneLayersInfo state;
  gboolean notified_change = TRUE;
  GList *l;
  GList *next;

  state.keep_n = n;
  state.current_pos = 0;
  state.needs_pruning = FALSE;
  _cogl_material_foreach_layer (material,
                                update_prune_layers_info_cb,
                                &state);

  material->n_layers = n;

  if (!state.needs_pruning)
    return;

  if (!(material->differences & COGL_MATERIAL_STATE_LAYERS))
    return;

  /* It's possible that this material owns some of the layers being
   * discarded, so we'll need to unlink them... */
  for (l = material->layer_differences; l; l = next)
    {
      CoglMaterialLayer *layer = l->data;
      next = l->next; /* we're modifying the list we're iterating */

      if (layer->index > state.first_index_to_prune)
        {
          if (!notified_change)
            {
              /* - Flush journal primitives referencing the current
               *   state.
               * - Make sure the material has no dependants so it may
               *   be modified.
               * - If the material isn't currently an authority for
               *   the state being changed, then initialize that state
               *   from the current authority.
               */
              _cogl_material_pre_change_notify (material,
                                                COGL_MATERIAL_STATE_LAYERS,
                                                NULL);
              notified_change = TRUE;
            }

          material->layer_differences =
            g_list_delete_link (material->layer_differences, l);
        }
    }
}

static void
_cogl_material_backend_layer_change_notify (CoglMaterialLayer *layer,
                                            CoglMaterialLayerState change)
{
  int i;

  /* NB: layers may be used by multiple materials which may be using
   * different backends, therefore we determine which backends to
   * notify based on the private state pointers for each backend...
   */
  for (i = 0; i < COGL_MATERIAL_N_BACKENDS; i++)
    {
      if (layer->backend_priv[i] && backends[i]->layer_pre_change_notify)
        backends[i]->layer_pre_change_notify (layer, change);
    }
}

unsigned int
_cogl_get_n_args_for_combine_func (GLint func)
{
  switch (func)
    {
    case GL_REPLACE:
      return 1;
    case GL_MODULATE:
    case GL_ADD:
    case GL_ADD_SIGNED:
    case GL_SUBTRACT:
    case GL_DOT3_RGB:
    case GL_DOT3_RGBA:
      return 2;
    case GL_INTERPOLATE:
      return 3;
    }
  return 0;
}

static void
_cogl_material_layer_initialize_state (CoglMaterialLayer *dest,
                                       CoglMaterialLayer *src,
                                       unsigned long differences)
{
  CoglMaterialLayerBigState *big_state;

  dest->differences |= differences;

  if (differences & COGL_MATERIAL_LAYER_STATE_UNIT)
    dest->unit_index = src->unit_index;

  if (differences & COGL_MATERIAL_LAYER_STATE_TEXTURE)
    dest->texture = src->texture;

  if (differences & COGL_MATERIAL_LAYER_STATE_FILTERS)
    {
      dest->min_filter = src->min_filter;
      dest->mag_filter = src->mag_filter;
    }

  if (differences & COGL_MATERIAL_LAYER_STATE_WRAP_MODES)
    {
      dest->wrap_mode_s = src->wrap_mode_s;
      dest->wrap_mode_t = src->wrap_mode_t;
      dest->wrap_mode_r = src->wrap_mode_r;
    }

  if (differences & COGL_MATERIAL_LAYER_STATE_NEEDS_BIG_STATE)
    {
      if (!dest->has_big_state)
        {
          dest->big_state = g_slice_new (CoglMaterialLayerBigState);
          dest->has_big_state = TRUE;
        }
      big_state = dest->big_state;
    }
  else
    return;

  if (differences & COGL_MATERIAL_LAYER_STATE_COMBINE)
    {
      int n_args;
      int i;
      GLint func = src->big_state->texture_combine_rgb_func;
      big_state->texture_combine_rgb_func = func;
      n_args = _cogl_get_n_args_for_combine_func (func);
      for (i = 0; i < n_args; i++)
        {
          big_state->texture_combine_rgb_src[i] =
            src->big_state->texture_combine_rgb_src[i];
          big_state->texture_combine_rgb_op[i] =
            src->big_state->texture_combine_rgb_op[i];
        }

      func = src->big_state->texture_combine_alpha_func;
      big_state->texture_combine_alpha_func = func;
      n_args = _cogl_get_n_args_for_combine_func (func);
      for (i = 0; i < n_args; i++)
        {
          big_state->texture_combine_alpha_src[i] =
            src->big_state->texture_combine_alpha_src[i];
          big_state->texture_combine_alpha_op[i] =
            src->big_state->texture_combine_alpha_op[i];
        }
    }

  if (differences & COGL_MATERIAL_LAYER_STATE_COMBINE_CONSTANT)
    memcpy (dest->big_state->texture_combine_constant,
            src->big_state->texture_combine_constant,
            sizeof (float) * 4);

  if (differences & COGL_MATERIAL_LAYER_STATE_USER_MATRIX)
    dest->big_state->matrix = src->big_state->matrix;

  if (differences & COGL_MATERIAL_LAYER_STATE_POINT_SPRITE_COORDS)
    dest->big_state->point_sprite_coords = src->big_state->point_sprite_coords;
}

/* NB: This function will allocate a new derived layer if you are
 * trying to change the state of a layer with dependants so you must
 * always check the return value.
 *
 * If a new layer is returned it will be owned by required_owner.
 *
 * required_owner can only by NULL for new, currently unowned layers
 * with no dependants.
 */
static CoglMaterialLayer *
_cogl_material_layer_pre_change_notify (CoglMaterial *required_owner,
                                        CoglMaterialLayer *layer,
                                        CoglMaterialLayerState change)
{
  CoglTextureUnit *unit;
  CoglMaterialLayer *authority;

  /* Identify the case where the layer is new with no owner or
   * dependants and so we don't need to do anything. */
  if (COGL_MATERIAL_NODE (layer)->has_children == FALSE &&
      layer->owner == NULL)
    goto init_layer_state;

  /* We only allow a NULL required_owner for new layers */
  g_return_val_if_fail (required_owner != NULL, layer);

  /* Unlike materials; layers are simply considered immutable once
   * they have dependants - either children or another material owner.
   */
  if (COGL_MATERIAL_NODE (layer)->has_children ||
      layer->owner != required_owner)
    {
      CoglMaterialLayer *new = _cogl_material_layer_copy (layer);
      _cogl_material_add_layer_difference (required_owner, new, FALSE);
      cogl_object_unref (new);
      layer = new;
      goto init_layer_state;
    }

  /* Note: At this point we know there is only one material dependant on
   * this layer (required_owner), and there are no other layers
   * dependant on this layer so it's ok to modify it. */

  if (required_owner->journal_ref_count)
    _cogl_journal_flush ();

  _cogl_material_backend_layer_change_notify (layer, change);

  /* If the layer being changed is the same as the last layer we
   * flushed to the corresponding texture unit then we keep a track of
   * the changes so we can try to minimize redundant OpenGL calls if
   * the same layer is flushed again.
   */
  unit = _cogl_get_texture_unit (_cogl_material_layer_get_unit_index (layer));
  if (unit->layer == layer)
    unit->layer_changes_since_flush |= change;

init_layer_state:

  if (required_owner)
    required_owner->age++;

  /* If the material isn't already an authority for the state group
   * being modified then we need to initialize the corresponding
   * state. */
  authority = _cogl_material_layer_get_authority (layer, change);
  _cogl_material_layer_initialize_state (layer, authority, change);

  return layer;
}

static void
_cogl_material_layer_unparent (CoglMaterialNode *layer)
{
  /* Chain up */
  _cogl_material_node_unparent_real (layer);
}

static void
_cogl_material_layer_set_parent (CoglMaterialLayer *layer,
                                 CoglMaterialLayer *parent)
{
  /* Chain up */
  _cogl_material_node_set_parent_real (COGL_MATERIAL_NODE (layer),
                                       COGL_MATERIAL_NODE (parent),
                                       _cogl_material_layer_unparent);
}

/* XXX: This is duplicated logic; the same as for
 * _cogl_material_prune_redundant_ancestry it would be nice to find a
 * way to consolidate these functions! */
static void
_cogl_material_layer_prune_redundant_ancestry (CoglMaterialLayer *layer)
{
  CoglMaterialLayer *new_parent = _cogl_material_layer_get_parent (layer);

  /* walk up past ancestors that are now redundant and potentially
   * reparent the layer. */
  while (_cogl_material_layer_get_parent (new_parent) &&
         (new_parent->differences | layer->differences) ==
         layer->differences)
    new_parent = _cogl_material_layer_get_parent (new_parent);

  _cogl_material_layer_set_parent (layer, new_parent);
}

/*
 * XXX: consider special casing layer->unit_index so it's not a sparse
 * property so instead we can assume it's valid for all layer
 * instances.
 * - We would need to initialize ->unit_index in
 *   _cogl_material_layer_copy ().
 *
 * XXX: If you use this API you should consider that the given layer
 * might not be writeable and so a new derived layer will be allocated
 * and modified instead. The layer modified will be returned so you
 * can identify when this happens.
 */
static CoglMaterialLayer *
_cogl_material_set_layer_unit (CoglMaterial *required_owner,
                               CoglMaterialLayer *layer,
                               int unit_index)
{
  CoglMaterialLayerState change = COGL_MATERIAL_LAYER_STATE_UNIT;
  CoglMaterialLayer *authority =
    _cogl_material_layer_get_authority (layer, change);
  CoglMaterialLayer *new;

  if (authority->unit_index == unit_index)
    return layer;

  new =
    _cogl_material_layer_pre_change_notify (required_owner,
                                            layer,
                                            change);
  if (new != layer)
    layer = new;
  else
    {
      /* If the layer we found is currently the authority on the state
       * we are changing see if we can revert to one of our ancestors
       * being the authority. */
      if (layer == authority &&
          _cogl_material_layer_get_parent (authority) != NULL)
        {
          CoglMaterialLayer *parent =
            _cogl_material_layer_get_parent (authority);
          CoglMaterialLayer *old_authority =
            _cogl_material_layer_get_authority (parent, change);

          if (old_authority->unit_index == unit_index)
            {
              layer->differences &= ~change;
              return layer;
            }
        }
    }

  layer->unit_index = unit_index;

  /* If we weren't previously the authority on this state then we need
   * to extended our differences mask and so it's possible that some
   * of our ancestry will now become redundant, so we aim to reparent
   * ourselves if that's true... */
  if (layer != authority)
    {
      layer->differences |= change;
      _cogl_material_layer_prune_redundant_ancestry (layer);
    }

  return layer;
}

typedef struct
{
  /* The layer we are trying to find */
  int                         layer_index;

  /* The layer we find or untouched if not found */
  CoglMaterialLayer          *layer;

  /* If the layer can't be found then a new layer should be
   * inserted after this texture unit index... */
  int                         insert_after;

  /* When adding a layer we need the list of layers to shift up
   * to a new texture unit. When removing we need the list of
   * layers to shift down.
   *
   * Note: the list isn't sorted */
  CoglMaterialLayer         **layers_to_shift;
  int                         n_layers_to_shift;

  /* When adding a layer we don't need a complete list of
   * layers_to_shift if we find a layer already corresponding to the
   * layer_index.  */
  gboolean                    ignore_shift_layers_if_found;

} CoglMaterialLayerInfo;

/* Returns TRUE once we know there is nothing more to update */
static gboolean
update_layer_info (CoglMaterialLayer *layer,
                   CoglMaterialLayerInfo *layer_info)
{
  if (layer->index == layer_info->layer_index)
    {
      layer_info->layer = layer;
      if (layer_info->ignore_shift_layers_if_found)
        return TRUE;
    }
  else if (layer->index < layer_info->layer_index)
    {
      int unit_index = _cogl_material_layer_get_unit_index (layer);
      layer_info->insert_after = unit_index;
    }
  else
    layer_info->layers_to_shift[layer_info->n_layers_to_shift++] =
      layer;

  return FALSE;
}

/* Returns FALSE to break out of a _foreach_layer () iteration */
static gboolean
update_layer_info_cb (CoglMaterialLayer *layer,
                      void *user_data)
{
  CoglMaterialLayerInfo *layer_info = user_data;

  if (update_layer_info (layer, layer_info))
    return FALSE; /* break */
  else
    return TRUE; /* continue */
}

static void
_cogl_material_get_layer_info (CoglMaterial *material,
                               CoglMaterialLayerInfo *layer_info)
{
  /* Note: we are assuming this material is a _STATE_LAYERS authority */
  int n_layers = material->n_layers;
  int i;

  /* FIXME: _cogl_material_foreach_layer now calls
   * _cogl_material_update_layers_cache anyway so this codepath is
   * pointless! */
  if (layer_info->ignore_shift_layers_if_found &&
      material->layers_cache_dirty)
    {
      /* The expectation is that callers of
       * _cogl_material_get_layer_info are likely to be modifying the
       * list of layers associated with a material so in this case
       * where we don't have a cache of the layers and we don't
       * necessarily have to iterate all the layers of the material we
       * use a foreach_layer callback instead of updating the cache
       * and iterating that as below. */
      _cogl_material_foreach_layer (material,
                                    update_layer_info_cb,
                                    layer_info);
      return;
    }

  _cogl_material_update_layers_cache (material);
  for (i = 0; i < n_layers; i++)
    {
      CoglMaterialLayer *layer = material->layers_cache[i];

      if (update_layer_info (layer, layer_info))
        return;
    }
}

static CoglMaterialLayer *
_cogl_material_get_layer (CoglMaterial *material,
                          int layer_index)
{
  CoglMaterial *authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_LAYERS);
  CoglMaterialLayerInfo layer_info;
  CoglMaterialLayer *layer;
  int unit_index;
  int i;

  _COGL_GET_CONTEXT (ctx, NULL);

  /* The layer index of the layer we want info about */
  layer_info.layer_index = layer_index;

  /* If a layer already exists with the given index this will be
   * updated. */
  layer_info.layer = NULL;

  /* If a layer isn't found for the given index we'll need to know
   * where to insert a new layer. */
  layer_info.insert_after = -1;

  /* If a layer can't be found then we'll need to insert a new layer
   * and bump up the texture unit for all layers with an index
   * > layer_index. */
  layer_info.layers_to_shift =
    g_alloca (sizeof (CoglMaterialLayer *) * authority->n_layers);
  layer_info.n_layers_to_shift = 0;

  /* If an exact match is found though we don't need a complete
   * list of layers with indices > layer_index... */
  layer_info.ignore_shift_layers_if_found = TRUE;

  _cogl_material_get_layer_info (authority, &layer_info);

  if (layer_info.layer)
    return layer_info.layer;

  unit_index = layer_info.insert_after + 1;
  if (unit_index == 0)
    layer = _cogl_material_layer_copy (ctx->default_layer_0);
  else
    {
      CoglMaterialLayer *new;
      layer = _cogl_material_layer_copy (ctx->default_layer_n);
      new = _cogl_material_set_layer_unit (NULL, layer, unit_index);
      /* Since we passed a newly allocated layer we wouldn't expect
       * _set_layer_unit() to have to allocate *another* layer. */
      g_assert (new == layer);
    }
  layer->index = layer_index;

  for (i = 0; i < layer_info.n_layers_to_shift; i++)
    {
      CoglMaterialLayer *shift_layer = layer_info.layers_to_shift[i];

      unit_index = _cogl_material_layer_get_unit_index (shift_layer);
      _cogl_material_set_layer_unit (material, shift_layer, unit_index + 1);
      /* NB: shift_layer may not be writeable so _set_layer_unit()
       * will allocate a derived layer internally which will become
       * owned by material. Check the return value if we need to do
       * anything else with this layer. */
    }

  _cogl_material_add_layer_difference (material, layer, TRUE);

  cogl_object_unref (layer);

  return layer;
}

CoglHandle
_cogl_material_layer_get_texture (CoglMaterialLayer *layer)
{
  CoglMaterialLayer *authority =
    _cogl_material_layer_get_authority (layer,
                                        COGL_MATERIAL_LAYER_STATE_TEXTURE);

  return authority->texture;
}

static void
_cogl_material_prune_empty_layer_difference (CoglMaterial *layers_authority,
                                             CoglMaterialLayer *layer)
{
  /* Find the GList link that references the empty layer */
  GList *link = g_list_find (layers_authority->layer_differences, layer);
  /* No material directly owns the root node layer so this is safe... */
  CoglMaterialLayer *layer_parent = _cogl_material_layer_get_parent (layer);
  CoglMaterialLayerInfo layer_info;
  CoglMaterial *old_layers_authority;

  g_return_if_fail (link != NULL);

  /* If the layer's parent doesn't have an owner then we can simply
   * take ownership ourselves and drop our reference on the empty
   * layer.
   */
  if (layer_parent->index == layer->index && layer_parent->owner == NULL)
    {
      cogl_object_ref (layer_parent);
      link->data = _cogl_material_layer_get_parent (layer);
      cogl_object_unref (layer);
      recursively_free_layer_caches (layers_authority);
      return;
    }

  /* Now we want to find the layer that would become the authority for
   * layer->index if we were to remove layer from
   * layers_authority->layer_differences
   */

  /* The layer index of the layer we want info about */
  layer_info.layer_index = layer->index;

  /* If a layer already exists with the given index this will be
   * updated. */
  layer_info.layer = NULL;

  /* If a layer can't be found then we'll need to insert a new layer
   * and bump up the texture unit for all layers with an index
   * > layer_index. */
  layer_info.layers_to_shift =
    g_alloca (sizeof (CoglMaterialLayer *) * layers_authority->n_layers);
  layer_info.n_layers_to_shift = 0;

  /* If an exact match is found though we don't need a complete
   * list of layers with indices > layer_index... */
  layer_info.ignore_shift_layers_if_found = TRUE;

  /* We know the default/root material isn't a LAYERS authority so it's
   * safe to use the result of _cogl_material_get_parent (layers_authority)
   * without checking it.
   */
  old_layers_authority =
    _cogl_material_get_authority (_cogl_material_get_parent (layers_authority),
                                  COGL_MATERIAL_STATE_LAYERS);

  _cogl_material_get_layer_info (old_layers_authority, &layer_info);

  /* If layer is the defining layer for the corresponding ->index then
   * we can't get rid of it. */
  if (!layer_info.layer)
    return;

  /* If the layer that would become the authority for layer->index is
   * _cogl_material_layer_get_parent (layer) then we can simply remove the
   * layer difference. */
  if (layer_info.layer == _cogl_material_layer_get_parent (layer))
    {
      _cogl_material_remove_layer_difference (layers_authority, layer, FALSE);
      _cogl_material_try_reverting_layers_authority (layers_authority,
                                                     old_layers_authority);
    }
}

static void
_cogl_material_set_layer_texture (CoglMaterial *material,
                                  int layer_index,
                                  CoglHandle texture,
                                  gboolean overriden,
                                  GLuint slice_gl_texture,
                                  GLenum slice_gl_target)
{
  CoglMaterialLayerState change = COGL_MATERIAL_LAYER_STATE_TEXTURE;
  CoglMaterialLayer *layer;
  CoglMaterialLayer *authority;
  CoglMaterialLayer *new;

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);

  /* Now find the ancestor of the layer that is the authority for the
   * state we want to change */
  authority = _cogl_material_layer_get_authority (layer, change);

  if (authority->texture_overridden == overriden &&
      authority->texture == texture &&
      (authority->texture_overridden == FALSE ||
       (authority->slice_gl_texture == slice_gl_texture &&
        authority->slice_gl_target == slice_gl_target)))
    return;

  new = _cogl_material_layer_pre_change_notify (material, layer, change);
  if (new != layer)
    layer = new;
  else
    {
      /* If the original layer we found is currently the authority on
       * the state we are changing see if we can revert to one of our
       * ancestors being the authority. */
      if (layer == authority &&
          _cogl_material_layer_get_parent (authority) != NULL)
        {
          CoglMaterialLayer *parent =
            _cogl_material_layer_get_parent (authority);
          CoglMaterialLayer *old_authority =
            _cogl_material_layer_get_authority (parent, change);

          if (old_authority->texture_overridden == overriden &&
              old_authority->texture == texture &&
              (old_authority->texture_overridden == FALSE ||
               (old_authority->slice_gl_texture == slice_gl_texture &&
                old_authority->slice_gl_target == slice_gl_target)))
            {
              layer->differences &= ~change;

              g_assert (layer->owner == material);
              if (layer->differences == 0)
                _cogl_material_prune_empty_layer_difference (material,
                                                             layer);
              goto changed;
            }
        }
    }

  if (texture != COGL_INVALID_HANDLE)
    cogl_handle_ref (texture);
  if (layer == authority &&
      layer->texture != COGL_INVALID_HANDLE)
    cogl_handle_unref (layer->texture);
  layer->texture = texture;
  layer->texture_overridden = overriden;
  layer->slice_gl_texture = slice_gl_texture;
  layer->slice_gl_target = slice_gl_target;

  /* If we weren't previously the authority on this state then we need
   * to extended our differences mask and so it's possible that some
   * of our ancestry will now become redundant, so we aim to reparent
   * ourselves if that's true... */
  if (layer != authority)
    {
      layer->differences |= change;
      _cogl_material_layer_prune_redundant_ancestry (layer);
    }

changed:

  handle_automatic_blend_enable (material, COGL_MATERIAL_STATE_LAYERS);
}

static void
_cogl_material_set_layer_gl_texture_slice (CoglMaterial *material,
                                           int layer_index,
                                           CoglHandle texture,
                                           GLuint slice_gl_texture,
                                           GLenum slice_gl_target)
{
  g_return_if_fail (cogl_is_material (material));
  /* GL texture overrides can only be set in association with a parent
   * CoglTexture */
  g_return_if_fail (cogl_is_texture (texture));

  _cogl_material_set_layer_texture (material,
                                    layer_index,
                                    texture,
                                    TRUE, /* slice override */
                                    slice_gl_texture,
                                    slice_gl_target);
}

/* XXX: deprecate and replace with cogl_material_set_layer_texture?
 *
 * Originally I was planning on allowing users to set shaders somehow
 * on layers (thus the ambiguous name), but now I wonder if we will do
 * that with a more explicit "snippets" API and materials will have
 * hooks defined to receive these snippets.
 */
void
cogl_material_set_layer (CoglMaterial *material,
			 int layer_index,
			 CoglHandle texture)
{
  g_return_if_fail (cogl_is_material (material));
  g_return_if_fail (texture == COGL_INVALID_HANDLE ||
                    cogl_is_texture (texture));

  _cogl_material_set_layer_texture (material,
                                    layer_index,
                                    texture,
                                    FALSE, /* slice override */
                                    0, /* slice_gl_texture */
                                    0); /* slice_gl_target */
}

typedef struct
{
  int i;
  CoglMaterial *material;
  unsigned long fallback_layers;
} CoglMaterialFallbackState;

static gboolean
fallback_layer_cb (CoglMaterialLayer *layer, void *user_data)
{
  CoglMaterialFallbackState *state = user_data;
  CoglMaterial *material = state->material;
  CoglHandle texture = _cogl_material_layer_get_texture (layer);
  GLenum gl_target;
  COGL_STATIC_COUNTER (layer_fallback_counter,
                       "layer fallback counter",
                       "Increments each time a layer's texture is "
                       "forced to a fallback texture",
                       0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!(state->fallback_layers & 1<<state->i))
    return TRUE;

  COGL_COUNTER_INC (_cogl_uprof_context, layer_fallback_counter);

  if (G_LIKELY (texture != COGL_INVALID_HANDLE))
    cogl_texture_get_gl_texture (texture, NULL, &gl_target);
  else
    gl_target = GL_TEXTURE_2D;

  if (gl_target == GL_TEXTURE_2D)
    texture = ctx->default_gl_texture_2d_tex;
#ifdef HAVE_COGL_GL
  else if (gl_target == GL_TEXTURE_RECTANGLE_ARB)
    texture = ctx->default_gl_texture_rect_tex;
#endif
  else
    {
      g_warning ("We don't have a fallback texture we can use to fill "
                 "in for an invalid material layer, since it was "
                 "using an unsupported texture target ");
      /* might get away with this... */
      texture = ctx->default_gl_texture_2d_tex;
    }

  cogl_material_set_layer (material, layer->index, texture);

  state->i++;

  return TRUE;
}

void
_cogl_material_set_layer_wrap_modes (CoglMaterial        *material,
                                     CoglMaterialLayer   *layer,
                                     CoglMaterialLayer   *authority,
                                     CoglMaterialWrapModeInternal wrap_mode_s,
                                     CoglMaterialWrapModeInternal wrap_mode_t,
                                     CoglMaterialWrapModeInternal wrap_mode_r)
{
  CoglMaterialLayer     *new;
  CoglMaterialLayerState change = COGL_MATERIAL_LAYER_STATE_WRAP_MODES;

  if (authority->wrap_mode_s == wrap_mode_s &&
      authority->wrap_mode_t == wrap_mode_t &&
      authority->wrap_mode_r == wrap_mode_r)
    return;

  new = _cogl_material_layer_pre_change_notify (material, layer, change);
  if (new != layer)
    layer = new;
  else
    {
      /* If the original layer we found is currently the authority on
       * the state we are changing see if we can revert to one of our
       * ancestors being the authority. */
      if (layer == authority &&
          _cogl_material_layer_get_parent (authority) != NULL)
        {
          CoglMaterialLayer *parent =
            _cogl_material_layer_get_parent (authority);
          CoglMaterialLayer *old_authority =
            _cogl_material_layer_get_authority (parent, change);

          if (old_authority->wrap_mode_s == wrap_mode_s &&
              old_authority->wrap_mode_t == wrap_mode_t &&
              old_authority->wrap_mode_r == wrap_mode_r)
            {
              layer->differences &= ~change;

              g_assert (layer->owner == material);
              if (layer->differences == 0)
                _cogl_material_prune_empty_layer_difference (material,
                                                             layer);
              return;
            }
        }
    }

  layer->wrap_mode_s = wrap_mode_s;
  layer->wrap_mode_t = wrap_mode_t;
  layer->wrap_mode_r = wrap_mode_r;

  /* If we weren't previously the authority on this state then we need
   * to extended our differences mask and so it's possible that some
   * of our ancestry will now become redundant, so we aim to reparent
   * ourselves if that's true... */
  if (layer != authority)
    {
      layer->differences |= change;
      _cogl_material_layer_prune_redundant_ancestry (layer);
    }
}

static CoglMaterialWrapModeInternal
public_to_internal_wrap_mode (CoglMaterialWrapMode mode)
{
  return (CoglMaterialWrapModeInternal)mode;
}

static CoglMaterialWrapMode
internal_to_public_wrap_mode (CoglMaterialWrapModeInternal internal_mode)
{
  g_return_val_if_fail (internal_mode !=
                        COGL_MATERIAL_WRAP_MODE_INTERNAL_CLAMP_TO_BORDER,
                        COGL_MATERIAL_WRAP_MODE_AUTOMATIC);
  return (CoglMaterialWrapMode)internal_mode;
}

void
cogl_material_set_layer_wrap_mode_s (CoglMaterial *material,
                                     int layer_index,
                                     CoglMaterialWrapMode mode)
{
  CoglMaterialLayerState       change = COGL_MATERIAL_LAYER_STATE_WRAP_MODES;
  CoglMaterialLayer           *layer;
  CoglMaterialLayer           *authority;
  CoglMaterialWrapModeInternal internal_mode =
    public_to_internal_wrap_mode (mode);

  g_return_if_fail (cogl_is_material (material));

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);

  /* Now find the ancestor of the layer that is the authority for the
   * state we want to change */
  authority = _cogl_material_layer_get_authority (layer, change);

  _cogl_material_set_layer_wrap_modes (material, layer, authority,
                                       internal_mode,
                                       authority->wrap_mode_t,
                                       authority->wrap_mode_r);
}

void
cogl_material_set_layer_wrap_mode_t (CoglMaterial        *material,
                                     int                  layer_index,
                                     CoglMaterialWrapMode mode)
{
  CoglMaterialLayerState       change = COGL_MATERIAL_LAYER_STATE_WRAP_MODES;
  CoglMaterialLayer           *layer;
  CoglMaterialLayer           *authority;
  CoglMaterialWrapModeInternal internal_mode =
    public_to_internal_wrap_mode (mode);

  g_return_if_fail (cogl_is_material (material));

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);

  /* Now find the ancestor of the layer that is the authority for the
   * state we want to change */
  authority = _cogl_material_layer_get_authority (layer, change);

  _cogl_material_set_layer_wrap_modes (material, layer, authority,
                                       authority->wrap_mode_s,
                                       internal_mode,
                                       authority->wrap_mode_r);
}

/* TODO: this should be made public once we add support for 3D
   textures in Cogl */
void
_cogl_material_set_layer_wrap_mode_r (CoglMaterial        *material,
                                      int                  layer_index,
                                      CoglMaterialWrapMode mode)
{
  CoglMaterialLayerState       change = COGL_MATERIAL_LAYER_STATE_WRAP_MODES;
  CoglMaterialLayer           *layer;
  CoglMaterialLayer           *authority;
  CoglMaterialWrapModeInternal internal_mode =
    public_to_internal_wrap_mode (mode);

  g_return_if_fail (cogl_is_material (material));

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);

  /* Now find the ancestor of the layer that is the authority for the
   * state we want to change */
  authority = _cogl_material_layer_get_authority (layer, change);

  _cogl_material_set_layer_wrap_modes (material, layer, authority,
                                       authority->wrap_mode_s,
                                       authority->wrap_mode_t,
                                       internal_mode);
}

void
cogl_material_set_layer_wrap_mode (CoglMaterial        *material,
                                   int                  layer_index,
                                   CoglMaterialWrapMode mode)
{
  CoglMaterialLayerState       change = COGL_MATERIAL_LAYER_STATE_WRAP_MODES;
  CoglMaterialLayer           *layer;
  CoglMaterialLayer           *authority;
  CoglMaterialWrapModeInternal internal_mode =
    public_to_internal_wrap_mode (mode);

  g_return_if_fail (cogl_is_material (material));

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);

  /* Now find the ancestor of the layer that is the authority for the
   * state we want to change */
  authority = _cogl_material_layer_get_authority (layer, change);

  _cogl_material_set_layer_wrap_modes (material, layer, authority,
                                       internal_mode,
                                       internal_mode,
                                       internal_mode);
  /* XXX: I wonder if we should really be duplicating the mode into
   * the 'r' wrap mode too? */
}

/* FIXME: deprecate this API */
CoglMaterialWrapMode
cogl_material_layer_get_wrap_mode_s (CoglMaterialLayer *layer)
{
  CoglMaterialLayerState change = COGL_MATERIAL_LAYER_STATE_WRAP_MODES;
  CoglMaterialLayer     *authority;

  g_return_val_if_fail (_cogl_is_material_layer (layer), FALSE);

  /* Now find the ancestor of the layer that is the authority for the
   * state we want to change */
  authority = _cogl_material_layer_get_authority (layer, change);

  return internal_to_public_wrap_mode (authority->wrap_mode_s);
}

CoglMaterialWrapMode
cogl_material_get_layer_wrap_mode_s (CoglMaterial *material, int layer_index)
{
  CoglMaterialLayer *layer;

  g_return_val_if_fail (cogl_is_material (material), FALSE);

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);
  /* FIXME: we shouldn't ever construct a layer in a getter function */

  return cogl_material_layer_get_wrap_mode_s (layer);
}

/* FIXME: deprecate this API */
CoglMaterialWrapMode
cogl_material_layer_get_wrap_mode_t (CoglMaterialLayer *layer)
{
  CoglMaterialLayerState change = COGL_MATERIAL_LAYER_STATE_WRAP_MODES;
  CoglMaterialLayer     *authority;

  g_return_val_if_fail (_cogl_is_material_layer (layer), FALSE);

  /* Now find the ancestor of the layer that is the authority for the
   * state we want to change */
  authority = _cogl_material_layer_get_authority (layer, change);

  return internal_to_public_wrap_mode (authority->wrap_mode_t);
}

CoglMaterialWrapMode
cogl_material_get_layer_wrap_mode_t (CoglMaterial *material, int layer_index)
{
  CoglMaterialLayer *layer;

  g_return_val_if_fail (cogl_is_material (material), FALSE);

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);
  /* FIXME: we shouldn't ever construct a layer in a getter function */

  return cogl_material_layer_get_wrap_mode_t (layer);
}

CoglMaterialWrapMode
_cogl_material_layer_get_wrap_mode_r (CoglMaterialLayer *layer)
{
  CoglMaterialLayerState change = COGL_MATERIAL_LAYER_STATE_WRAP_MODES;
  CoglMaterialLayer     *authority =
    _cogl_material_layer_get_authority (layer, change);

  return internal_to_public_wrap_mode (authority->wrap_mode_r);
}

/* TODO: make this public when we expose 3D textures. */
CoglMaterialWrapMode
_cogl_material_get_layer_wrap_mode_r (CoglMaterial *material, int layer_index)
{
  CoglMaterialLayer *layer;

  g_return_val_if_fail (cogl_is_material (material), FALSE);

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);

  return _cogl_material_layer_get_wrap_mode_r (layer);
}

static void
_cogl_material_layer_get_wrap_modes (CoglMaterialLayer *layer,
                                     CoglMaterialWrapModeInternal *wrap_mode_s,
                                     CoglMaterialWrapModeInternal *wrap_mode_t,
                                     CoglMaterialWrapModeInternal *wrap_mode_r)
{
  CoglMaterialLayer *authority =
    _cogl_material_layer_get_authority (layer,
                                        COGL_MATERIAL_LAYER_STATE_WRAP_MODES);

  *wrap_mode_s = authority->wrap_mode_s;
  *wrap_mode_t = authority->wrap_mode_t;
  *wrap_mode_r = authority->wrap_mode_r;
}

gboolean
cogl_material_set_layer_point_sprite_coords_enabled (CoglMaterial *material,
                                                     int layer_index,
                                                     gboolean enable,
                                                     GError **error)
{
  CoglMaterialLayerState       change =
    COGL_MATERIAL_LAYER_STATE_POINT_SPRITE_COORDS;
  CoglMaterialLayer           *layer;
  CoglMaterialLayer           *new;
  CoglMaterialLayer           *authority;

  g_return_val_if_fail (cogl_is_material (material), FALSE);

  /* Don't allow point sprite coordinates to be enabled if the driver
     doesn't support it */
  if (enable && !cogl_features_available (COGL_FEATURE_POINT_SPRITE))
    {
      if (error)
        {
          g_set_error (error, COGL_ERROR, COGL_ERROR_MISSING_FEATURE,
                       "Point sprite texture coordinates are enabled "
                       "for a layer but the GL driver does not support it.");
        }
      else
        {
          static gboolean warning_seen = FALSE;
          if (!warning_seen)
            g_warning ("Point sprite texture coordinates are enabled "
                       "for a layer but the GL driver does not support it.");
          warning_seen = TRUE;
        }

      return FALSE;
    }

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);

  /* Now find the ancestor of the layer that is the authority for the
   * state we want to change */
  authority = _cogl_material_layer_get_authority (layer, change);

  if (authority->big_state->point_sprite_coords == enable)
    return TRUE;

  new = _cogl_material_layer_pre_change_notify (material, layer, change);
  if (new != layer)
    layer = new;
  else
    {
      /* If the original layer we found is currently the authority on
       * the state we are changing see if we can revert to one of our
       * ancestors being the authority. */
      if (layer == authority &&
          _cogl_material_layer_get_parent (authority) != NULL)
        {
          CoglMaterialLayer *parent =
            _cogl_material_layer_get_parent (authority);
          CoglMaterialLayer *old_authority =
            _cogl_material_layer_get_authority (parent, change);

          if (old_authority->big_state->point_sprite_coords == enable)
            {
              layer->differences &= ~change;

              g_assert (layer->owner == material);
              if (layer->differences == 0)
                _cogl_material_prune_empty_layer_difference (material,
                                                             layer);
              return TRUE;
            }
        }
    }

  layer->big_state->point_sprite_coords = enable;

  /* If we weren't previously the authority on this state then we need
   * to extended our differences mask and so it's possible that some
   * of our ancestry will now become redundant, so we aim to reparent
   * ourselves if that's true... */
  if (layer != authority)
    {
      layer->differences |= change;
      _cogl_material_layer_prune_redundant_ancestry (layer);
    }

  return TRUE;
}

gboolean
cogl_material_get_layer_point_sprite_coords_enabled (CoglMaterial *material,
                                                     int layer_index)
{
  CoglMaterialLayerState       change =
    COGL_MATERIAL_LAYER_STATE_POINT_SPRITE_COORDS;
  CoglMaterialLayer *layer;
  CoglMaterialLayer *authority;

  g_return_val_if_fail (cogl_is_material (material), FALSE);

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);
  /* FIXME: we shouldn't ever construct a layer in a getter function */

  authority = _cogl_material_layer_get_authority (layer, change);

  return authority->big_state->point_sprite_coords;
}

typedef struct
{
  CoglMaterial *material;
  CoglMaterialWrapModeOverrides *wrap_mode_overrides;
  int i;
} CoglMaterialWrapModeOverridesState;

static gboolean
apply_wrap_mode_overrides_cb (CoglMaterialLayer *layer,
                              void *user_data)
{
  CoglMaterialWrapModeOverridesState *state = user_data;
  CoglMaterialLayer *authority =
    _cogl_material_layer_get_authority (layer,
                                        COGL_MATERIAL_LAYER_STATE_WRAP_MODES);
  CoglMaterialWrapModeInternal wrap_mode_s;
  CoglMaterialWrapModeInternal wrap_mode_t;
  CoglMaterialWrapModeInternal wrap_mode_r;

  g_return_val_if_fail (state->i < 32, FALSE);

  wrap_mode_s = state->wrap_mode_overrides->values[state->i].s;
  if (wrap_mode_s == COGL_MATERIAL_WRAP_MODE_OVERRIDE_NONE)
    wrap_mode_s = (CoglMaterialWrapModeInternal)authority->wrap_mode_s;
  wrap_mode_t = state->wrap_mode_overrides->values[state->i].t;
  if (wrap_mode_t == COGL_MATERIAL_WRAP_MODE_OVERRIDE_NONE)
    wrap_mode_t = (CoglMaterialWrapModeInternal)authority->wrap_mode_t;
  wrap_mode_r = state->wrap_mode_overrides->values[state->i].r;
  if (wrap_mode_r == COGL_MATERIAL_WRAP_MODE_OVERRIDE_NONE)
    wrap_mode_r = (CoglMaterialWrapModeInternal)authority->wrap_mode_r;

  _cogl_material_set_layer_wrap_modes (state->material,
                                       layer,
                                       authority,
                                       wrap_mode_s,
                                       wrap_mode_t,
                                       wrap_mode_r);

  state->i++;

  return TRUE;
}

typedef struct
{
  CoglMaterial *material;
  GLuint gl_texture;
} CoglMaterialOverrideLayerState;

static gboolean
override_layer_texture_cb (CoglMaterialLayer *layer, void *user_data)
{
  CoglMaterialOverrideLayerState *state = user_data;
  CoglHandle texture;
  GLenum gl_target;

  texture = _cogl_material_layer_get_texture (layer);

  if (texture != COGL_INVALID_HANDLE)
    cogl_texture_get_gl_texture (texture, NULL, &gl_target);
  else
    gl_target = GL_TEXTURE_2D;

  _cogl_material_set_layer_gl_texture_slice (state->material,
                                             layer->index,
                                             texture,
                                             state->gl_texture,
                                             gl_target);
  return TRUE;
}

void
_cogl_material_apply_overrides (CoglMaterial *material,
                                CoglMaterialFlushOptions *options)
{
  COGL_STATIC_COUNTER (apply_overrides_counter,
                       "material overrides counter",
                       "Increments each time we have to apply "
                       "override options to a material",
                       0 /* no application private data */);

  COGL_COUNTER_INC (_cogl_uprof_context, apply_overrides_counter);

  if (options->flags & COGL_MATERIAL_FLUSH_DISABLE_MASK)
    {
      int i;

      /* NB: we can assume that once we see one bit to disable
       * a layer, all subsequent layers are also disabled. */
      for (i = 0; i < 32 && options->disable_layers & (1<<i); i++)
        ;

      _cogl_material_prune_to_n_layers (material, i);
    }

  if (options->flags & COGL_MATERIAL_FLUSH_FALLBACK_MASK)
    {
      CoglMaterialFallbackState state;

      state.i = 0;
      state.material = material;
      state.fallback_layers = options->fallback_layers;

      _cogl_material_foreach_layer (material,
                                    fallback_layer_cb,
                                    &state);
    }

  if (options->flags & COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE)
    {
      CoglMaterialOverrideLayerState state;

      _cogl_material_prune_to_n_layers (material, 1);

      /* NB: we are overriding the first layer, but we don't know
       * the user's given layer_index, which is why we use
       * _cogl_material_foreach_layer() here even though we know
       * there's only one layer. */
      state.material = material;
      state.gl_texture = options->layer0_override_texture;
      _cogl_material_foreach_layer (material,
                                    override_layer_texture_cb,
                                    &state);
    }

  if (options->flags & COGL_MATERIAL_FLUSH_WRAP_MODE_OVERRIDES)
    {
      CoglMaterialWrapModeOverridesState state;

      state.material = material;
      state.wrap_mode_overrides = &options->wrap_mode_overrides;
      state.i = 0;
      _cogl_material_foreach_layer (material,
                                    apply_wrap_mode_overrides_cb,
                                    &state);
    }
}

static gboolean
_cogl_material_layer_texture_equal (CoglMaterialLayer *authority0,
                                    CoglMaterialLayer *authority1)
{
  if (authority0->texture != authority1->texture)
    return FALSE;

  if (authority0->texture_overridden != authority1->texture_overridden)
    return FALSE;

  if (authority0->texture_overridden &&
      (authority0->slice_gl_texture != authority1->slice_gl_texture ||
       authority0->slice_gl_target != authority1->slice_gl_target))
    return FALSE;

  return TRUE;
}

/* Determine the mask of differences between two layers.
 *
 * XXX: If layers and materials could both be cast to a common Tree
 * type of some kind then we could have a unified
 * compare_differences() function.
 */
unsigned long
_cogl_material_layer_compare_differences (CoglMaterialLayer *layer0,
                                          CoglMaterialLayer *layer1)
{
  CoglMaterialLayer *node0;
  CoglMaterialLayer *node1;
  int len0;
  int len1;
  int len0_index;
  int len1_index;
  int count;
  int i;
  CoglMaterialLayer *common_ancestor = NULL;
  unsigned long layers_difference = 0;

  _COGL_GET_CONTEXT (ctx, 0);

  /* Algorithm:
   *
   * 1) Walk the ancestors of each layer to the root node, adding a
   *    pointer to each ancester node to two GArrays:
   *    ctx->material0_nodes, and ctx->material1_nodes.
   *
   * 2) Compare the arrays to find the nodes where they stop to
   *    differ.
   *
   * 3) For each array now iterate from index 0 to the first node of
   *    difference ORing that nodes ->difference mask into the final
   *    material_differences mask.
   */

  g_array_set_size (ctx->material0_nodes, 0);
  g_array_set_size (ctx->material1_nodes, 0);
  for (node0 = layer0; node0; node0 = _cogl_material_layer_get_parent (node0))
    g_array_append_vals (ctx->material0_nodes, &node0, 1);
  for (node1 = layer1; node1; node1 = _cogl_material_layer_get_parent (node1))
    g_array_append_vals (ctx->material1_nodes, &node1, 1);

  len0 = ctx->material0_nodes->len;
  len1 = ctx->material1_nodes->len;
  /* There's no point looking at the last entries since we know both
   * layers must have the same default layer as their root node. */
  len0_index = len0 - 2;
  len1_index = len1 - 2;
  count = MIN (len0, len1) - 1;
  for (i = 0; i < count; i++)
    {
      node0 = g_array_index (ctx->material0_nodes,
                             CoglMaterialLayer *, len0_index--);
      node1 = g_array_index (ctx->material1_nodes,
                             CoglMaterialLayer *, len1_index--);
      if (node0 != node1)
        {
          common_ancestor = _cogl_material_layer_get_parent (node0);
          break;
        }
    }

  /* If we didn't already find the first the common_ancestor ancestor
   * that's because one material is a direct descendant of the other
   * and in this case the first common ancestor is the last node we
   * looked at. */
  if (!common_ancestor)
    common_ancestor = node0;

  count = len0 - 1;
  for (i = 0; i < count; i++)
    {
      node0 = g_array_index (ctx->material0_nodes, CoglMaterialLayer *, i);
      if (node0 == common_ancestor)
        break;
      layers_difference |= node0->differences;
    }

  count = len1 - 1;
  for (i = 0; i < count; i++)
    {
      node1 = g_array_index (ctx->material1_nodes, CoglMaterialLayer *, i);
      if (node1 == common_ancestor)
        break;
      layers_difference |= node1->differences;
    }

  return layers_difference;
}

static gboolean
_cogl_material_layer_combine_state_equal (CoglMaterialLayer *authority0,
                                          CoglMaterialLayer *authority1)
{
  CoglMaterialLayerBigState *big_state0 = authority0->big_state;
  CoglMaterialLayerBigState *big_state1 = authority1->big_state;
  int n_args;
  int i;

  if (big_state0->texture_combine_rgb_func !=
      big_state1->texture_combine_rgb_func)
    return FALSE;

  if (big_state0->texture_combine_alpha_func !=
      big_state1->texture_combine_alpha_func)
    return FALSE;

  n_args =
    _cogl_get_n_args_for_combine_func (big_state0->texture_combine_rgb_func);
  for (i = 0; i < n_args; i++)
    {
      if ((big_state0->texture_combine_rgb_src[i] !=
           big_state1->texture_combine_rgb_src[i]) ||
          (big_state0->texture_combine_rgb_op[i] !=
           big_state1->texture_combine_rgb_op[i]))
        return FALSE;
    }

  n_args =
    _cogl_get_n_args_for_combine_func (big_state0->texture_combine_alpha_func);
  for (i = 0; i < n_args; i++)
    {
      if ((big_state0->texture_combine_alpha_src[i] !=
           big_state1->texture_combine_alpha_src[i]) ||
          (big_state0->texture_combine_alpha_op[i] !=
           big_state1->texture_combine_alpha_op[i]))
        return FALSE;
    }

  return TRUE;
}

static gboolean
_cogl_material_layer_combine_constant_equal (CoglMaterialLayer *authority0,
                                             CoglMaterialLayer *authority1)
{
  return memcmp (authority0->big_state->texture_combine_constant,
                 authority1->big_state->texture_combine_constant,
                 sizeof (float) * 4) == 0 ? TRUE : FALSE;
}

static gboolean
_cogl_material_layer_filters_equal (CoglMaterialLayer *authority0,
                                    CoglMaterialLayer *authority1)
{
  if (authority0->mag_filter != authority1->mag_filter)
    return FALSE;
  if (authority0->min_filter != authority1->min_filter)
    return FALSE;

  return TRUE;
}

static gboolean
_cogl_material_layer_wrap_modes_equal (CoglMaterialLayer *authority0,
                                       CoglMaterialLayer *authority1)
{
  if (authority0->wrap_mode_s != authority1->wrap_mode_s ||
      authority0->wrap_mode_t != authority1->wrap_mode_t ||
      authority0->wrap_mode_r != authority1->wrap_mode_r)
    return FALSE;

  return TRUE;
}

static gboolean
_cogl_material_layer_user_matrix_equal (CoglMaterialLayer *authority0,
                                        CoglMaterialLayer *authority1)
{
  CoglMaterialLayerBigState *big_state0 = authority0->big_state;
  CoglMaterialLayerBigState *big_state1 = authority1->big_state;

  if (!cogl_matrix_equal (&big_state0->matrix, &big_state1->matrix))
    return FALSE;

  return TRUE;
}

static gboolean
_cogl_material_layer_point_sprite_coords_equal (CoglMaterialLayer *authority0,
                                                CoglMaterialLayer *authority1)
{
  CoglMaterialLayerBigState *big_state0 = authority0->big_state;
  CoglMaterialLayerBigState *big_state1 = authority1->big_state;

  return big_state0->point_sprite_coords == big_state1->point_sprite_coords;
}

typedef gboolean
(*CoglMaterialLayerStateComparitor) (CoglMaterialLayer *authority0,
                                     CoglMaterialLayer *authority1);

static gboolean
layer_state_equal (CoglMaterialLayerState state,
                   CoglMaterialLayer *layer0,
                   CoglMaterialLayer *layer1,
                   CoglMaterialLayerStateComparitor comparitor)
{
  CoglMaterialLayer *authority0 =
    _cogl_material_layer_get_authority (layer0, state);
  CoglMaterialLayer *authority1 =
    _cogl_material_layer_get_authority (layer1, state);

  return comparitor (authority0, authority1);
}

static gboolean
_cogl_material_layer_equal (CoglMaterialLayer *layer0,
                            CoglMaterialLayer *layer1)
{
  unsigned long layers_difference;

  if (layer0 == layer1)
    return TRUE;

  layers_difference =
    _cogl_material_layer_compare_differences (layer0, layer1);

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_TEXTURE &&
      !layer_state_equal (COGL_MATERIAL_LAYER_STATE_TEXTURE,
                          layer0, layer1,
                          _cogl_material_layer_texture_equal))
    return FALSE;

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_COMBINE &&
      !layer_state_equal (COGL_MATERIAL_LAYER_STATE_COMBINE,
                          layer0, layer1,
                          _cogl_material_layer_combine_state_equal))
    return FALSE;

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_COMBINE_CONSTANT &&
      !layer_state_equal (COGL_MATERIAL_LAYER_STATE_COMBINE_CONSTANT,
                          layer0, layer1,
                          _cogl_material_layer_combine_constant_equal))
    return FALSE;

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_FILTERS &&
      !layer_state_equal (COGL_MATERIAL_LAYER_STATE_FILTERS,
                          layer0, layer1,
                          _cogl_material_layer_filters_equal))
    return FALSE;

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_WRAP_MODES &&
      !layer_state_equal (COGL_MATERIAL_LAYER_STATE_WRAP_MODES,
                          layer0, layer1,
                          _cogl_material_layer_wrap_modes_equal))
    return FALSE;

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_USER_MATRIX &&
      !layer_state_equal (COGL_MATERIAL_LAYER_STATE_USER_MATRIX,
                          layer0, layer1,
                          _cogl_material_layer_user_matrix_equal))
    return FALSE;

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_POINT_SPRITE_COORDS &&
      !layer_state_equal (COGL_MATERIAL_LAYER_STATE_POINT_SPRITE_COORDS,
                          layer0, layer1,
                          _cogl_material_layer_point_sprite_coords_equal))
    return FALSE;

  return TRUE;
}

static gboolean
_cogl_material_color_equal (CoglMaterial *authority0,
                            CoglMaterial *authority1)
{
  return cogl_color_equal (&authority0->color, &authority1->color);
}

static gboolean
_cogl_material_lighting_state_equal (CoglMaterial *authority0,
                                     CoglMaterial *authority1)
{
  CoglMaterialLightingState *state0 = &authority0->big_state->lighting_state;
  CoglMaterialLightingState *state1 = &authority1->big_state->lighting_state;

  if (memcmp (state0->ambient, state1->ambient, sizeof (float) * 4) != 0)
    return FALSE;
  if (memcmp (state0->diffuse, state1->diffuse, sizeof (float) * 4) != 0)
    return FALSE;
  if (memcmp (state0->specular, state1->specular, sizeof (float) * 4) != 0)
    return FALSE;
  if (memcmp (state0->emission, state1->emission, sizeof (float) * 4) != 0)
    return FALSE;
  if (state0->shininess != state1->shininess)
    return FALSE;

  return TRUE;
}

static gboolean
_cogl_material_alpha_state_equal (CoglMaterial *authority0,
                                  CoglMaterial *authority1)
{
  CoglMaterialAlphaFuncState *alpha_state0 =
    &authority0->big_state->alpha_state;
  CoglMaterialAlphaFuncState *alpha_state1 =
    &authority1->big_state->alpha_state;

  if (alpha_state0->alpha_func != alpha_state1->alpha_func ||
      alpha_state0->alpha_func_reference != alpha_state1->alpha_func_reference)
    return FALSE;
  else
    return TRUE;
}

static gboolean
_cogl_material_blend_state_equal (CoglMaterial *authority0,
                                  CoglMaterial *authority1)
{
  CoglMaterialBlendState *blend_state0 = &authority0->big_state->blend_state;
  CoglMaterialBlendState *blend_state1 = &authority1->big_state->blend_state;

#ifndef HAVE_COGL_GLES
  if (blend_state0->blend_equation_rgb != blend_state1->blend_equation_rgb)
    return FALSE;
  if (blend_state0->blend_equation_alpha !=
      blend_state1->blend_equation_alpha)
    return FALSE;
  if (blend_state0->blend_src_factor_alpha !=
      blend_state1->blend_src_factor_alpha)
    return FALSE;
  if (blend_state0->blend_dst_factor_alpha !=
      blend_state1->blend_dst_factor_alpha)
    return FALSE;
#endif
  if (blend_state0->blend_src_factor_rgb !=
      blend_state1->blend_src_factor_rgb)
    return FALSE;
  if (blend_state0->blend_dst_factor_rgb !=
      blend_state1->blend_dst_factor_rgb)
    return FALSE;
#ifndef HAVE_COGL_GLES
  if (!cogl_color_equal (&blend_state0->blend_constant,
                         &blend_state1->blend_constant))
    return FALSE;
#endif

  return TRUE;
}

static gboolean
_cogl_material_depth_state_equal (CoglMaterial *authority0,
                                  CoglMaterial *authority1)
{
  if (authority0->big_state->depth_state.depth_test_enabled == FALSE &&
      authority1->big_state->depth_state.depth_test_enabled == FALSE)
    return TRUE;
  else
    return memcmp (&authority0->big_state->depth_state,
                   &authority1->big_state->depth_state,
                   sizeof (CoglMaterialDepthState)) == 0;
}

static gboolean
_cogl_material_fog_state_equal (CoglMaterial *authority0,
                                CoglMaterial *authority1)
{
  CoglMaterialFogState *fog_state0 = &authority0->big_state->fog_state;
  CoglMaterialFogState *fog_state1 = &authority1->big_state->fog_state;

  if (fog_state0->enabled == fog_state1->enabled &&
      cogl_color_equal (&fog_state0->color, &fog_state1->color) &&
      fog_state0->mode == fog_state1->mode &&
      fog_state0->density == fog_state1->density &&
      fog_state0->z_near == fog_state1->z_near &&
      fog_state0->z_far == fog_state1->z_far)
    return TRUE;
  else
    return FALSE;
}

static gboolean
_cogl_material_point_size_equal (CoglMaterial *authority0,
                                 CoglMaterial *authority1)
{
  return authority0->big_state->point_size == authority1->big_state->point_size;
}

static gboolean
_cogl_material_layers_equal (CoglMaterial *authority0,
                             CoglMaterial *authority1)
{
  int i;

  if (authority0->n_layers != authority1->n_layers)
    return FALSE;

  _cogl_material_update_layers_cache (authority0);
  _cogl_material_update_layers_cache (authority1);

  for (i = 0; i < authority0->n_layers; i++)
    {
      if (!_cogl_material_layer_equal (authority0->layers_cache[i],
                                       authority1->layers_cache[i]))
        return FALSE;
    }
  return TRUE;
}

/* Determine the mask of differences between two materials */
static unsigned long
_cogl_material_compare_differences (CoglMaterial *material0,
                                    CoglMaterial *material1)
{
  CoglMaterial *node0;
  CoglMaterial *node1;
  int len0;
  int len1;
  int len0_index;
  int len1_index;
  int count;
  int i;
  CoglMaterial *common_ancestor = NULL;
  unsigned long materials_difference = 0;

  _COGL_GET_CONTEXT (ctx, 0);

  /* Algorithm:
   *
   * 1) Walk the ancestors of each layer to the root node, adding a
   *    pointer to each ancester node to two GArrays:
   *    ctx->material0_nodes, and ctx->material1_nodes.
   *
   * 2) Compare the arrays to find the nodes where they stop to
   *    differ.
   *
   * 3) For each array now iterate from index 0 to the first node of
   *    difference ORing that nodes ->difference mask into the final
   *    material_differences mask.
   */

  g_array_set_size (ctx->material0_nodes, 0);
  g_array_set_size (ctx->material1_nodes, 0);
  for (node0 = material0; node0; node0 = _cogl_material_get_parent (node0))
    g_array_append_vals (ctx->material0_nodes, &node0, 1);
  for (node1 = material1; node1; node1 = _cogl_material_get_parent (node1))
    g_array_append_vals (ctx->material1_nodes, &node1, 1);

  len0 = ctx->material0_nodes->len;
  len1 = ctx->material1_nodes->len;
  /* There's no point looking at the last entries since we know both
   * layers must have the same default layer as their root node. */
  len0_index = len0 - 2;
  len1_index = len1 - 2;
  count = MIN (len0, len1) - 1;
  for (i = 0; i < count; i++)
    {
      node0 = g_array_index (ctx->material0_nodes,
                             CoglMaterial *, len0_index--);
      node1 = g_array_index (ctx->material1_nodes,
                             CoglMaterial *, len1_index--);
      if (node0 != node1)
        {
          common_ancestor = _cogl_material_get_parent (node0);
          break;
        }
    }

  /* If we didn't already find the first the common_ancestor ancestor
   * that's because one material is a direct descendant of the other
   * and in this case the first common ancestor is the last node we
   * looked at. */
  if (!common_ancestor)
    common_ancestor = node0;

  count = len0 - 1;
  for (i = 0; i < count; i++)
    {
      node0 = g_array_index (ctx->material0_nodes, CoglMaterial *, i);
      if (node0 == common_ancestor)
        break;
      materials_difference |= node0->differences;
    }

  count = len1 - 1;
  for (i = 0; i < count; i++)
    {
      node1 = g_array_index (ctx->material1_nodes, CoglMaterial *, i);
      if (node1 == common_ancestor)
        break;
      materials_difference |= node1->differences;
    }

  return materials_difference;

}

static gboolean
simple_property_equal (CoglMaterial *material0,
                       CoglMaterial *material1,
                       unsigned long materials_difference,
                       CoglMaterialState state,
                       CoglMaterialStateComparitor comparitor)
{
  if (materials_difference & state)
    {
      if (!comparitor (_cogl_material_get_authority (material0, state),
                       _cogl_material_get_authority (material1, state)))
        return FALSE;
    }
  return TRUE;
}

/* Comparison of two arbitrary materials is done by:
 * 1) walking up the parents of each material until a common
 *    ancestor is found, and at each step ORing together the
 *    difference masks.
 *
 * 2) using the final difference mask to determine which state
 *    groups to compare.
 *
 * This is used by the Cogl journal to compare materials so that it
 * can split up geometry that needs different OpenGL state.
 *
 * It is acceptable to have false negatives - although they will result
 * in redundant OpenGL calls that try and update the state.
 *
 * False positives aren't allowed.
 */
gboolean
_cogl_material_equal (CoglMaterial *material0,
                      CoglMaterial *material1,
                      gboolean skip_gl_color)
{
  unsigned long  materials_difference;

  if (material0 == material1)
    return TRUE;

  /* First check non-sparse properties */

  if (material0->real_blend_enable != material1->real_blend_enable)
    return FALSE;

  /* Then check sparse properties */

  materials_difference =
    _cogl_material_compare_differences (material0, material1);

  if (materials_difference & COGL_MATERIAL_STATE_COLOR &&
      !skip_gl_color)
    {
      CoglMaterialState state = COGL_MATERIAL_STATE_COLOR;
      CoglMaterial *authority0 =
        _cogl_material_get_authority (material0, state);
      CoglMaterial *authority1 =
        _cogl_material_get_authority (material1, state);

      if (!cogl_color_equal (&authority0->color, &authority1->color))
        return FALSE;
    }

  if (!simple_property_equal (material0, material1,
                              materials_difference,
                              COGL_MATERIAL_STATE_LIGHTING,
                              _cogl_material_lighting_state_equal))
    return FALSE;

  if (!simple_property_equal (material0, material1,
                              materials_difference,
                              COGL_MATERIAL_STATE_ALPHA_FUNC,
                              _cogl_material_alpha_state_equal))
    return FALSE;

  /* We don't need to compare the detailed blending state if we know
   * blending is disabled for both materials. */
  if (material0->real_blend_enable &&
      materials_difference & COGL_MATERIAL_STATE_BLEND)
    {
      CoglMaterialState state = COGL_MATERIAL_STATE_BLEND;
      CoglMaterial *authority0 =
        _cogl_material_get_authority (material0, state);
      CoglMaterial *authority1 =
        _cogl_material_get_authority (material1, state);

      if (!_cogl_material_blend_state_equal (authority0, authority1))
        return FALSE;
    }

  /* XXX: we don't need to compare the BLEND_ENABLE state because it's
   * already reflected in ->real_blend_enable */
#if 0
  if (!simple_property_equal (material0, material1,
                              materials_difference,
                              COGL_MATERIAL_STATE_BLEND,
                              _cogl_material_blend_enable_equal))
    return FALSE;
#endif

  if (!simple_property_equal (material0, material1,
                              materials_difference,
                              COGL_MATERIAL_STATE_DEPTH,
                              _cogl_material_depth_state_equal))
    return FALSE;

  if (!simple_property_equal (material0, material1,
                              materials_difference,
                              COGL_MATERIAL_STATE_FOG,
                              _cogl_material_fog_state_equal))
    return FALSE;

  if (!simple_property_equal (material0, material1,
                              materials_difference,
                              COGL_MATERIAL_STATE_POINT_SIZE,
                              _cogl_material_point_size_equal))
    return FALSE;

  if (!simple_property_equal (material0, material1,
                              materials_difference,
                              COGL_MATERIAL_STATE_LAYERS,
                              _cogl_material_layers_equal))
    return FALSE;

  return TRUE;
}

void
cogl_material_get_color (CoglMaterial *material,
                         CoglColor    *color)
{
  CoglMaterial *authority;

  g_return_if_fail (cogl_is_material (material));

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_COLOR);

  *color = authority->color;
}

/* This is used heavily by the cogl journal when logging quads */
void
_cogl_material_get_colorubv (CoglMaterial *material,
                             guint8       *color)
{
  CoglMaterial *authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_COLOR);

  _cogl_color_get_rgba_4ubv (&authority->color, color);
}

static void
_cogl_material_prune_redundant_ancestry (CoglMaterial *material)
{
  CoglMaterial *new_parent = _cogl_material_get_parent (material);

  /* walk up past ancestors that are now redundant and potentially
   * reparent the material. */
  while (_cogl_material_get_parent (new_parent) &&
         (new_parent->differences | material->differences) ==
          material->differences)
    new_parent = _cogl_material_get_parent (new_parent);

  _cogl_material_set_parent (material, new_parent);
}

static void
_cogl_material_update_authority (CoglMaterial *material,
                                 CoglMaterial *authority,
                                 CoglMaterialState state,
                                 CoglMaterialStateComparitor comparitor)
{
  /* If we are the current authority see if we can revert to one of
   * our ancestors being the authority */
  if (material == authority &&
      _cogl_material_get_parent (authority) != NULL)
    {
      CoglMaterial *parent = _cogl_material_get_parent (authority);
      CoglMaterial *old_authority =
        _cogl_material_get_authority (parent, state);

      if (comparitor (authority, old_authority))
        material->differences &= ~state;
    }
  else if (material != authority)
    {
      /* If we weren't previously the authority on this state then we
       * need to extended our differences mask and so it's possible
       * that some of our ancestry will now become redundant, so we
       * aim to reparent ourselves if that's true... */
      material->differences |= state;
      _cogl_material_prune_redundant_ancestry (material);
    }
}

void
cogl_material_set_color (CoglMaterial    *material,
			 const CoglColor *color)
{
  CoglMaterialState state = COGL_MATERIAL_STATE_COLOR;
  CoglMaterial *authority;

  g_return_if_fail (cogl_is_material (material));

  authority = _cogl_material_get_authority (material, state);

  if (cogl_color_equal (color, &authority->color))
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, color);

  material->color = *color;

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_color_equal);

  handle_automatic_blend_enable (material, state);
}

void
cogl_material_set_color4ub (CoglMaterial *material,
			    guint8 red,
                            guint8 green,
                            guint8 blue,
                            guint8 alpha)
{
  CoglColor color;
  cogl_color_set_from_4ub (&color, red, green, blue, alpha);
  cogl_material_set_color (material, &color);
}

void
cogl_material_set_color4f (CoglMaterial *material,
			   float red,
                           float green,
                           float blue,
                           float alpha)
{
  CoglColor color;
  cogl_color_set_from_4f (&color, red, green, blue, alpha);
  cogl_material_set_color (material, &color);
}

CoglMaterialBlendEnable
_cogl_material_get_blend_enabled (CoglMaterial *material)
{
  CoglMaterial *authority;

  g_return_val_if_fail (cogl_is_material (material), FALSE);

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_BLEND_ENABLE);
  return authority->blend_enable;
}

static gboolean
_cogl_material_blend_enable_equal (CoglMaterial *authority0,
                                   CoglMaterial *authority1)
{
  return authority0->blend_enable == authority1->blend_enable ? TRUE : FALSE;
}

void
_cogl_material_set_blend_enabled (CoglMaterial *material,
                                  CoglMaterialBlendEnable enable)
{
  CoglMaterialState state = COGL_MATERIAL_STATE_BLEND_ENABLE;
  CoglMaterial *authority;

  g_return_if_fail (cogl_is_material (material));
  g_return_if_fail (enable > 1 &&
                    "don't pass TRUE or FALSE to _set_blend_enabled!");

  authority = _cogl_material_get_authority (material, state);

  if (authority->blend_enable == enable)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  material->blend_enable = enable;

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_blend_enable_equal);

  handle_automatic_blend_enable (material, state);
}

void
cogl_material_get_ambient (CoglMaterial *material,
                           CoglColor    *ambient)
{
  CoglMaterial *authority;

  g_return_if_fail (cogl_is_material (material));

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_LIGHTING);

  cogl_color_init_from_4fv (ambient,
                            authority->big_state->lighting_state.ambient);
}

void
cogl_material_set_ambient (CoglMaterial *material,
			   const CoglColor *ambient)
{
  CoglMaterialState state = COGL_MATERIAL_STATE_LIGHTING;
  CoglMaterial *authority;
  CoglMaterialLightingState *lighting_state;

  g_return_if_fail (cogl_is_material (material));

  authority = _cogl_material_get_authority (material, state);

  lighting_state = &authority->big_state->lighting_state;
  if (cogl_color_equal (ambient, &lighting_state->ambient))
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  lighting_state = &material->big_state->lighting_state;
  lighting_state->ambient[0] = cogl_color_get_red_float (ambient);
  lighting_state->ambient[1] = cogl_color_get_green_float (ambient);
  lighting_state->ambient[2] = cogl_color_get_blue_float (ambient);
  lighting_state->ambient[3] = cogl_color_get_alpha_float (ambient);

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_lighting_state_equal);

  handle_automatic_blend_enable (material, state);
}

void
cogl_material_get_diffuse (CoglMaterial *material,
                           CoglColor    *diffuse)
{
  CoglMaterial *authority;

  g_return_if_fail (cogl_is_material (material));

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_LIGHTING);

  cogl_color_init_from_4fv (diffuse,
                            authority->big_state->lighting_state.diffuse);
}

void
cogl_material_set_diffuse (CoglMaterial *material,
			   const CoglColor *diffuse)
{
  CoglMaterialState state = COGL_MATERIAL_STATE_LIGHTING;
  CoglMaterial *authority;
  CoglMaterialLightingState *lighting_state;

  g_return_if_fail (cogl_is_material (material));

  authority = _cogl_material_get_authority (material, state);

  lighting_state = &authority->big_state->lighting_state;
  if (cogl_color_equal (diffuse, &lighting_state->diffuse))
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  lighting_state = &material->big_state->lighting_state;
  lighting_state->diffuse[0] = cogl_color_get_red_float (diffuse);
  lighting_state->diffuse[1] = cogl_color_get_green_float (diffuse);
  lighting_state->diffuse[2] = cogl_color_get_blue_float (diffuse);
  lighting_state->diffuse[3] = cogl_color_get_alpha_float (diffuse);


  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_lighting_state_equal);

  handle_automatic_blend_enable (material, state);
}

void
cogl_material_set_ambient_and_diffuse (CoglMaterial *material,
				       const CoglColor *color)
{
  cogl_material_set_ambient (material, color);
  cogl_material_set_diffuse (material, color);
}

void
cogl_material_get_specular (CoglMaterial *material,
                            CoglColor    *specular)
{
  CoglMaterial *authority;

  g_return_if_fail (cogl_is_material (material));

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_LIGHTING);

  cogl_color_init_from_4fv (specular,
                            authority->big_state->lighting_state.specular);
}

void
cogl_material_set_specular (CoglMaterial *material, const CoglColor *specular)
{
  CoglMaterial *authority;
  CoglMaterialState state = COGL_MATERIAL_STATE_LIGHTING;
  CoglMaterialLightingState *lighting_state;

  g_return_if_fail (cogl_is_material (material));

  authority = _cogl_material_get_authority (material, state);

  lighting_state = &authority->big_state->lighting_state;
  if (cogl_color_equal (specular, &lighting_state->specular))
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  lighting_state = &material->big_state->lighting_state;
  lighting_state->specular[0] = cogl_color_get_red_float (specular);
  lighting_state->specular[1] = cogl_color_get_green_float (specular);
  lighting_state->specular[2] = cogl_color_get_blue_float (specular);
  lighting_state->specular[3] = cogl_color_get_alpha_float (specular);

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_lighting_state_equal);

  handle_automatic_blend_enable (material, state);
}

float
cogl_material_get_shininess (CoglMaterial *material)
{
  CoglMaterial *authority;

  g_return_val_if_fail (cogl_is_material (material), 0);

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_LIGHTING);

  return authority->big_state->lighting_state.shininess;
}

void
cogl_material_set_shininess (CoglMaterial *material,
			     float shininess)
{
  CoglMaterial *authority;
  CoglMaterialState state = COGL_MATERIAL_STATE_LIGHTING;
  CoglMaterialLightingState *lighting_state;

  g_return_if_fail (cogl_is_material (material));

  if (shininess < 0.0 || shininess > 1.0)
    {
      g_warning ("Out of range shininess %f supplied for material\n",
                 shininess);
      return;
    }

  authority = _cogl_material_get_authority (material, state);

  lighting_state = &authority->big_state->lighting_state;

  if (lighting_state->shininess == shininess)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  lighting_state = &material->big_state->lighting_state;
  lighting_state->shininess = shininess;

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_lighting_state_equal);
}

void
cogl_material_get_emission (CoglMaterial *material,
                            CoglColor    *emission)
{
  CoglMaterial *authority;

  g_return_if_fail (cogl_is_material (material));

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_LIGHTING);

  cogl_color_init_from_4fv (emission,
                            authority->big_state->lighting_state.emission);
}

void
cogl_material_set_emission (CoglMaterial *material, const CoglColor *emission)
{
  CoglMaterial *authority;
  CoglMaterialState state = COGL_MATERIAL_STATE_LIGHTING;
  CoglMaterialLightingState *lighting_state;

  g_return_if_fail (cogl_is_material (material));

  authority = _cogl_material_get_authority (material, state);

  lighting_state = &authority->big_state->lighting_state;
  if (cogl_color_equal (emission, &lighting_state->emission))
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  lighting_state = &material->big_state->lighting_state;
  lighting_state->emission[0] = cogl_color_get_red_float (emission);
  lighting_state->emission[1] = cogl_color_get_green_float (emission);
  lighting_state->emission[2] = cogl_color_get_blue_float (emission);
  lighting_state->emission[3] = cogl_color_get_alpha_float (emission);

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_lighting_state_equal);

  handle_automatic_blend_enable (material, state);
}

void
cogl_material_set_alpha_test_function (CoglMaterial *material,
				       CoglMaterialAlphaFunc alpha_func,
				       float alpha_reference)
{
  CoglMaterialState state = COGL_MATERIAL_STATE_ALPHA_FUNC;
  CoglMaterial *authority;
  CoglMaterialAlphaFuncState *alpha_state;

  g_return_if_fail (cogl_is_material (material));

  authority = _cogl_material_get_authority (material, state);

  alpha_state = &authority->big_state->alpha_state;
  if (alpha_state->alpha_func == alpha_func &&
      alpha_state->alpha_func_reference == alpha_reference)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  alpha_state = &material->big_state->alpha_state;
  alpha_state->alpha_func = alpha_func;
  alpha_state->alpha_func_reference = alpha_reference;

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_alpha_state_equal);
}

GLenum
arg_to_gl_blend_factor (CoglBlendStringArgument *arg)
{
  if (arg->source.is_zero)
    return GL_ZERO;
  if (arg->factor.is_one)
    return GL_ONE;
  else if (arg->factor.is_src_alpha_saturate)
    return GL_SRC_ALPHA_SATURATE;
  else if (arg->factor.source.info->type ==
           COGL_BLEND_STRING_COLOR_SOURCE_SRC_COLOR)
    {
      if (arg->factor.source.mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_SRC_COLOR;
          else
            return GL_SRC_COLOR;
        }
      else
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_SRC_ALPHA;
          else
            return GL_SRC_ALPHA;
        }
    }
  else if (arg->factor.source.info->type ==
           COGL_BLEND_STRING_COLOR_SOURCE_DST_COLOR)
    {
      if (arg->factor.source.mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_DST_COLOR;
          else
            return GL_DST_COLOR;
        }
      else
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_DST_ALPHA;
          else
            return GL_DST_ALPHA;
        }
    }
#ifndef HAVE_COGL_GLES
  else if (arg->factor.source.info->type ==
           COGL_BLEND_STRING_COLOR_SOURCE_CONSTANT)
    {
      if (arg->factor.source.mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_CONSTANT_COLOR;
          else
            return GL_CONSTANT_COLOR;
        }
      else
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_CONSTANT_ALPHA;
          else
            return GL_CONSTANT_ALPHA;
        }
    }
#endif

  g_warning ("Unable to determine valid blend factor from blend string\n");
  return GL_ONE;
}

void
setup_blend_state (CoglBlendStringStatement *statement,
                   GLenum *blend_equation,
                   GLint *blend_src_factor,
                   GLint *blend_dst_factor)
{
#ifndef HAVE_COGL_GLES
  switch (statement->function->type)
    {
    case COGL_BLEND_STRING_FUNCTION_ADD:
      *blend_equation = GL_FUNC_ADD;
      break;
    /* TODO - add more */
    default:
      g_warning ("Unsupported blend function given");
      *blend_equation = GL_FUNC_ADD;
    }
#endif

  *blend_src_factor = arg_to_gl_blend_factor (&statement->args[0]);
  *blend_dst_factor = arg_to_gl_blend_factor (&statement->args[1]);
}

gboolean
cogl_material_set_blend (CoglMaterial *material,
                         const char *blend_description,
                         GError **error)
{
  CoglMaterialState state = COGL_MATERIAL_STATE_BLEND;
  CoglMaterial *authority;
  CoglBlendStringStatement statements[2];
  CoglBlendStringStatement *rgb;
  CoglBlendStringStatement *a;
  GError *internal_error = NULL;
  int count;
  CoglMaterialBlendState *blend_state;

  g_return_val_if_fail (cogl_is_material (material), FALSE);

  count =
    _cogl_blend_string_compile (blend_description,
                                COGL_BLEND_STRING_CONTEXT_BLENDING,
                                statements,
                                &internal_error);
  if (!count)
    {
      if (error)
	g_propagate_error (error, internal_error);
      else
	{
	  g_warning ("Cannot compile blend description: %s\n",
		     internal_error->message);
	  g_error_free (internal_error);
	}
      return FALSE;
    }

  if (count == 1)
    rgb = a = statements;
  else
    {
      rgb = &statements[0];
      a = &statements[1];
    }

  authority =
    _cogl_material_get_authority (material, state);

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  blend_state = &material->big_state->blend_state;
#ifndef HAVE_COGL_GLES
  setup_blend_state (rgb,
                     &blend_state->blend_equation_rgb,
                     &blend_state->blend_src_factor_rgb,
                     &blend_state->blend_dst_factor_rgb);
  setup_blend_state (a,
                     &blend_state->blend_equation_alpha,
                     &blend_state->blend_src_factor_alpha,
                     &blend_state->blend_dst_factor_alpha);
#else
  setup_blend_state (rgb,
                     NULL,
                     &blend_state->blend_src_factor_rgb,
                     &blend_state->blend_dst_factor_rgb);
#endif

  /* If we are the current authority see if we can revert to one of our
   * ancestors being the authority */
  if (material == authority &&
      _cogl_material_get_parent (authority) != NULL)
    {
      CoglMaterial *parent = _cogl_material_get_parent (authority);
      CoglMaterial *old_authority =
        _cogl_material_get_authority (parent, state);

      if (_cogl_material_blend_state_equal (authority, old_authority))
        material->differences &= ~state;
    }

  /* If we weren't previously the authority on this state then we need
   * to extended our differences mask and so it's possible that some
   * of our ancestry will now become redundant, so we aim to reparent
   * ourselves if that's true... */
  if (material != authority)
    {
      material->differences |= state;
      _cogl_material_prune_redundant_ancestry (material);
    }

  handle_automatic_blend_enable (material, state);

  return TRUE;
}

void
cogl_material_set_blend_constant (CoglMaterial *material,
                                  const CoglColor *constant_color)
{
#ifndef HAVE_COGL_GLES
  CoglMaterialState state = COGL_MATERIAL_STATE_BLEND;
  CoglMaterial *authority;
  CoglMaterialBlendState *blend_state;

  g_return_if_fail (cogl_is_material (material));

  authority = _cogl_material_get_authority (material, state);

  blend_state = &authority->big_state->blend_state;
  if (cogl_color_equal (constant_color, &blend_state->blend_constant))
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  blend_state = &material->big_state->blend_state;
  blend_state->blend_constant = *constant_color;

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_blend_state_equal);

  handle_automatic_blend_enable (material, state);
#endif
}

/* XXX: for now we don't mind if the program has vertex shaders
 * attached but if we ever make a similar API public we should only
 * allow attaching of programs containing fragment shaders. Eventually
 * we will have a CoglPipeline abstraction to also cover vertex
 * processing.
 */
void
_cogl_material_set_user_program (CoglMaterial *material,
                                 CoglHandle program)
{
  CoglMaterialState state = COGL_MATERIAL_STATE_USER_SHADER;
  CoglMaterial *authority;

  g_return_if_fail (cogl_is_material (material));

  authority = _cogl_material_get_authority (material, state);

  if (authority->big_state->user_program == program)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  if (program != COGL_INVALID_HANDLE)
    _cogl_material_set_backend (material, COGL_MATERIAL_BACKEND_DEFAULT);

  /* If we are the current authority see if we can revert to one of our
   * ancestors being the authority */
  if (material == authority &&
      _cogl_material_get_parent (authority) != NULL)
    {
      CoglMaterial *parent = _cogl_material_get_parent (authority);
      CoglMaterial *old_authority =
        _cogl_material_get_authority (parent, state);

      if (old_authority->big_state->user_program == program)
        material->differences &= ~state;
    }
  else if (material != authority)
    {
      /* If we weren't previously the authority on this state then we
       * need to extended our differences mask and so it's possible
       * that some of our ancestry will now become redundant, so we
       * aim to reparent ourselves if that's true... */
      material->differences |= state;
      _cogl_material_prune_redundant_ancestry (material);
    }

  if (program != COGL_INVALID_HANDLE)
    cogl_handle_ref (program);
  if (authority == material &&
      material->big_state->user_program != COGL_INVALID_HANDLE)
    cogl_handle_unref (material->big_state->user_program);
  material->big_state->user_program = program;

  handle_automatic_blend_enable (material, state);
}

void
cogl_material_set_depth_test_enabled (CoglMaterial *material,
                                      gboolean enable)
{
  CoglMaterialState state = COGL_MATERIAL_STATE_DEPTH;
  CoglMaterial *authority;
  CoglMaterialDepthState *depth_state;

  g_return_if_fail (cogl_is_material (material));

  authority = _cogl_material_get_authority (material, state);

  depth_state = &authority->big_state->depth_state;
  if (depth_state->depth_test_enabled == enable)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  material->big_state->depth_state.depth_test_enabled = enable;

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_depth_state_equal);
}

gboolean
cogl_material_get_depth_test_enabled (CoglMaterial *material)
{
  CoglMaterial *authority;

  g_return_val_if_fail (cogl_is_material (material), FALSE);

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_DEPTH);

  return authority->big_state->depth_state.depth_test_enabled;
}

void
cogl_material_set_depth_writing_enabled (CoglMaterial *material,
                                         gboolean enable)
{
  CoglMaterialState state = COGL_MATERIAL_STATE_DEPTH;
  CoglMaterial *authority;
  CoglMaterialDepthState *depth_state;

  g_return_if_fail (cogl_is_material (material));

  authority = _cogl_material_get_authority (material, state);

  depth_state = &authority->big_state->depth_state;
  if (depth_state->depth_writing_enabled == enable)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  material->big_state->depth_state.depth_writing_enabled = enable;

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_depth_state_equal);
}

gboolean
cogl_material_get_depth_writing_enabled (CoglMaterial *material)
{
  CoglMaterial *authority;

  g_return_val_if_fail (cogl_is_material (material), TRUE);

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_DEPTH);

  return authority->big_state->depth_state.depth_writing_enabled;
}

void
cogl_material_set_depth_test_function (CoglMaterial *material,
                                       CoglDepthTestFunction function)
{
  CoglMaterialState state = COGL_MATERIAL_STATE_DEPTH;
  CoglMaterial *authority;
  CoglMaterialDepthState *depth_state;

  g_return_if_fail (cogl_is_material (material));

  authority = _cogl_material_get_authority (material, state);

  depth_state = &authority->big_state->depth_state;
  if (depth_state->depth_test_function == function)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  material->big_state->depth_state.depth_test_function = function;

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_depth_state_equal);
}

CoglDepthTestFunction
cogl_material_get_depth_test_function (CoglMaterial *material)
{
  CoglMaterial *authority;

  g_return_val_if_fail (cogl_is_material (material),
                        COGL_DEPTH_TEST_FUNCTION_LESS);

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_DEPTH);

  return authority->big_state->depth_state.depth_test_function;
}


gboolean
cogl_material_set_depth_range (CoglMaterial *material,
                               float near_val,
                               float far_val,
                               GError **error)
{
#ifndef COGL_HAS_GLES
  CoglMaterialState state = COGL_MATERIAL_STATE_DEPTH;
  CoglMaterial *authority;
  CoglMaterialDepthState *depth_state;

  g_return_val_if_fail (cogl_is_material (material), FALSE);

  authority = _cogl_material_get_authority (material, state);

  depth_state = &authority->big_state->depth_state;
  if (depth_state->depth_range_near == near_val &&
      depth_state->depth_range_far == far_val)
    return TRUE;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  material->big_state->depth_state.depth_range_near = near_val;
  material->big_state->depth_state.depth_range_far = far_val;

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_depth_state_equal);
  return TRUE;
#else
  g_set_error (error,
               COGL_ERROR,
               COGL_ERROR_MISSING_FEATURE,
               "glDepthRange not available on GLES 1");
  return FALSE;
#endif
}

void
cogl_material_get_depth_range (CoglMaterial *material,
                               float *near_val,
                               float *far_val)
{
  CoglMaterial *authority;

  g_return_if_fail (cogl_is_material (material));

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_DEPTH);

  *near_val = authority->big_state->depth_state.depth_range_near;
  *far_val = authority->big_state->depth_state.depth_range_far;
}

static void
_cogl_material_set_fog_state (CoglMaterial *material,
                              const CoglMaterialFogState *fog_state)
{
  CoglMaterialState state = COGL_MATERIAL_STATE_FOG;
  CoglMaterial *authority;
  CoglMaterialFogState *current_fog_state;

  g_return_if_fail (cogl_is_material (material));

  authority = _cogl_material_get_authority (material, state);

  current_fog_state = &authority->big_state->fog_state;

  if (current_fog_state->enabled == fog_state->enabled &&
      cogl_color_equal (&current_fog_state->color, &fog_state->color) &&
      current_fog_state->mode == fog_state->mode &&
      current_fog_state->density == fog_state->density &&
      current_fog_state->z_near == fog_state->z_near &&
      current_fog_state->z_far == fog_state->z_far)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  material->big_state->fog_state = *fog_state;

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_fog_state_equal);
}

unsigned long
_cogl_material_get_age (CoglMaterial *material)
{
  g_return_val_if_fail (cogl_is_material (material), 0);

  return material->age;
}

static CoglMaterialLayer *
_cogl_material_layer_copy (CoglMaterialLayer *src)
{
  CoglMaterialLayer *layer = g_slice_new (CoglMaterialLayer);
  int i;

  _cogl_material_node_init (COGL_MATERIAL_NODE (layer));

  layer->owner = NULL;
  layer->index = src->index;
  layer->differences = 0;
  layer->has_big_state = FALSE;

  for (i = 0; i < COGL_MATERIAL_N_BACKENDS; i++)
    layer->backend_priv[i] = NULL;

  _cogl_material_layer_set_parent (layer, src);

  return _cogl_material_layer_object_new (layer);
}

static void
_cogl_material_layer_free (CoglMaterialLayer *layer)
{
  int i;

  _cogl_material_layer_unparent (COGL_MATERIAL_NODE (layer));

  /* NB: layers may be used by multiple materials which may be using
   * different backends, therefore we determine which backends to
   * notify based on the private state pointers for each backend...
   */
  for (i = 0; i < COGL_MATERIAL_N_BACKENDS; i++)
    {
      if (layer->backend_priv[i] && backends[i]->free_layer_priv)
        backends[i]->free_layer_priv (layer);
    }

  if (layer->differences & COGL_MATERIAL_LAYER_STATE_TEXTURE)
    cogl_handle_unref (layer->texture);

  if (layer->differences & COGL_MATERIAL_LAYER_STATE_NEEDS_BIG_STATE)
    g_slice_free (CoglMaterialLayerBigState, layer->big_state);

  g_slice_free (CoglMaterialLayer, layer);
}

  /* If a layer has descendants we can't modify it freely
   *
   * If the layer is owned and the owner has descendants we can't
   * modify it freely.
   *
   * In both cases when we can't freely modify a layer we can either:
   * - create a new layer; splice it in to replace the layer so it can
   *   be directly modified.
   *   XXX: disadvantage is that we have to invalidate the layers_cache
   *   for the owner and its descendants.
   * - create a new derived layer and modify that.
   */

  /* XXX: how is the caller expected to deal with ref-counting?
   *
   * If the layer can't be freely modified and we return a new layer
   * then that will effectively make the caller own a new reference
   * which doesn't happen if we simply modify the given layer.
   *
   * We could make it consistent by taking a reference on the layer if
   * we don't create a new one. At least this way the caller could
   * deal with it consistently, though the semantics are a bit
   * strange.
   *
   * Alternatively we could leave it to the caller to check
   * ...?
   */

void
_cogl_material_init_default_layers (void)
{
  CoglMaterialLayer *layer = g_slice_new0 (CoglMaterialLayer);
  CoglMaterialLayerBigState *big_state =
    g_slice_new0 (CoglMaterialLayerBigState);
  CoglMaterialLayer *new;
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_material_node_init (COGL_MATERIAL_NODE (layer));

  layer->index = 0;

  for (i = 0; i < COGL_MATERIAL_N_BACKENDS; i++)
    layer->backend_priv[i] = NULL;

  layer->differences = COGL_MATERIAL_LAYER_STATE_ALL_SPARSE;

  layer->unit_index = 0;

  layer->texture = COGL_INVALID_HANDLE;
  layer->texture_overridden = FALSE;

  layer->mag_filter = COGL_MATERIAL_FILTER_LINEAR;
  layer->min_filter = COGL_MATERIAL_FILTER_LINEAR;

  layer->wrap_mode_s = COGL_MATERIAL_WRAP_MODE_AUTOMATIC;
  layer->wrap_mode_t = COGL_MATERIAL_WRAP_MODE_AUTOMATIC;
  layer->wrap_mode_r = COGL_MATERIAL_WRAP_MODE_AUTOMATIC;

  layer->big_state = big_state;
  layer->has_big_state = TRUE;

  /* Choose the same default combine mode as OpenGL:
   * RGBA = MODULATE(PREVIOUS[RGBA],TEXTURE[RGBA]) */
  big_state->texture_combine_rgb_func = GL_MODULATE;
  big_state->texture_combine_rgb_src[0] = GL_PREVIOUS;
  big_state->texture_combine_rgb_src[1] = GL_TEXTURE;
  big_state->texture_combine_rgb_op[0] = GL_SRC_COLOR;
  big_state->texture_combine_rgb_op[1] = GL_SRC_COLOR;
  big_state->texture_combine_alpha_func = GL_MODULATE;
  big_state->texture_combine_alpha_src[0] = GL_PREVIOUS;
  big_state->texture_combine_alpha_src[1] = GL_TEXTURE;
  big_state->texture_combine_alpha_op[0] = GL_SRC_ALPHA;
  big_state->texture_combine_alpha_op[1] = GL_SRC_ALPHA;

  big_state->point_sprite_coords = FALSE;

  cogl_matrix_init_identity (&big_state->matrix);

  ctx->default_layer_0 = _cogl_material_layer_object_new (layer);

  /* TODO: we should make default_layer_n comprise of two
   * descendants of default_layer_0:
   * - the first descendant should change the texture combine
   *   to what we expect is most commonly used for multitexturing
   * - the second should revert the above change.
   *
   * why? the documentation for how a new layer is initialized
   * doesn't say that layers > 0 have different defaults so unless
   * we change the documentation we can't use different defaults,
   * but if the user does what we expect and changes the
   * texture combine then we can revert the authority to the
   * first descendant which means we can maximize the number
   * of layers with a common ancestor.
   *
   * The main problem will be that we'll need to disable the
   * optimizations for flattening the ancestry when we make
   * the second descendant which reverts the state.
   */
  ctx->default_layer_n = _cogl_material_layer_copy (layer);
  new = _cogl_material_set_layer_unit (NULL, ctx->default_layer_n, 1);
  g_assert (new == ctx->default_layer_n);
  /* Since we passed a newly allocated layer we don't expect that
   * _set_layer_unit() will have to allocate *another* layer. */

  /* Finally we create a dummy dependant for ->default_layer_n which
   * effectively ensures that ->default_layer_n and ->default_layer_0
   * remain immutable.
   */
  ctx->dummy_layer_dependant =
    _cogl_material_layer_copy (ctx->default_layer_n);
}

static void
setup_texture_combine_state (CoglBlendStringStatement *statement,
                             GLint *texture_combine_func,
                             GLint *texture_combine_src,
                             GLint *texture_combine_op)
{
  int i;

  switch (statement->function->type)
    {
    case COGL_BLEND_STRING_FUNCTION_REPLACE:
      *texture_combine_func = GL_REPLACE;
      break;
    case COGL_BLEND_STRING_FUNCTION_MODULATE:
      *texture_combine_func = GL_MODULATE;
      break;
    case COGL_BLEND_STRING_FUNCTION_ADD:
      *texture_combine_func = GL_ADD;
      break;
    case COGL_BLEND_STRING_FUNCTION_ADD_SIGNED:
      *texture_combine_func = GL_ADD_SIGNED;
      break;
    case COGL_BLEND_STRING_FUNCTION_INTERPOLATE:
      *texture_combine_func = GL_INTERPOLATE;
      break;
    case COGL_BLEND_STRING_FUNCTION_SUBTRACT:
      *texture_combine_func = GL_SUBTRACT;
      break;
    case COGL_BLEND_STRING_FUNCTION_DOT3_RGB:
      *texture_combine_func = GL_DOT3_RGB;
      break;
    case COGL_BLEND_STRING_FUNCTION_DOT3_RGBA:
      *texture_combine_func = GL_DOT3_RGBA;
      break;
    }

  for (i = 0; i < statement->function->argc; i++)
    {
      CoglBlendStringArgument *arg = &statement->args[i];

      switch (arg->source.info->type)
        {
        case COGL_BLEND_STRING_COLOR_SOURCE_CONSTANT:
          texture_combine_src[i] = GL_CONSTANT;
          break;
        case COGL_BLEND_STRING_COLOR_SOURCE_TEXTURE:
          texture_combine_src[i] = GL_TEXTURE;
          break;
        case COGL_BLEND_STRING_COLOR_SOURCE_TEXTURE_N:
          texture_combine_src[i] =
            GL_TEXTURE0 + arg->source.texture;
          break;
        case COGL_BLEND_STRING_COLOR_SOURCE_PRIMARY:
          texture_combine_src[i] = GL_PRIMARY_COLOR;
          break;
        case COGL_BLEND_STRING_COLOR_SOURCE_PREVIOUS:
          texture_combine_src[i] = GL_PREVIOUS;
          break;
        default:
          g_warning ("Unexpected texture combine source");
          texture_combine_src[i] = GL_TEXTURE;
        }

      if (arg->source.mask == COGL_BLEND_STRING_CHANNEL_MASK_RGB)
        {
          if (statement->args[i].source.one_minus)
            texture_combine_op[i] = GL_ONE_MINUS_SRC_COLOR;
          else
            texture_combine_op[i] = GL_SRC_COLOR;
        }
      else
        {
          if (statement->args[i].source.one_minus)
            texture_combine_op[i] = GL_ONE_MINUS_SRC_ALPHA;
          else
            texture_combine_op[i] = GL_SRC_ALPHA;
        }
    }
}

gboolean
cogl_material_set_layer_combine (CoglMaterial *material,
				 int layer_index,
				 const char *combine_description,
                                 GError **error)
{
  CoglMaterialLayerState state = COGL_MATERIAL_LAYER_STATE_COMBINE;
  CoglMaterialLayer *authority;
  CoglMaterialLayer *layer;
  CoglBlendStringStatement statements[2];
  CoglBlendStringStatement split[2];
  CoglBlendStringStatement *rgb;
  CoglBlendStringStatement *a;
  GError *internal_error = NULL;
  int count;

  g_return_val_if_fail (cogl_is_material (material), FALSE);

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);

  /* Now find the ancestor of the layer that is the authority for the
   * state we want to change */
  authority = _cogl_material_layer_get_authority (layer, state);

  count =
    _cogl_blend_string_compile (combine_description,
                                COGL_BLEND_STRING_CONTEXT_TEXTURE_COMBINE,
                                statements,
                                &internal_error);
  if (!count)
    {
      if (error)
	g_propagate_error (error, internal_error);
      else
	{
	  g_warning ("Cannot compile combine description: %s\n",
		     internal_error->message);
	  g_error_free (internal_error);
	}
      return FALSE;
    }

  if (statements[0].mask == COGL_BLEND_STRING_CHANNEL_MASK_RGBA)
    {
      _cogl_blend_string_split_rgba_statement (statements,
                                               &split[0], &split[1]);
      rgb = &split[0];
      a = &split[1];
    }
  else
    {
      rgb = &statements[0];
      a = &statements[1];
    }

  /* FIXME: compare the new state with the current state! */

  /* possibly flush primitives referencing the current state... */
  layer = _cogl_material_layer_pre_change_notify (material, layer, state);

  setup_texture_combine_state (rgb,
                               &layer->big_state->texture_combine_rgb_func,
                               layer->big_state->texture_combine_rgb_src,
                               layer->big_state->texture_combine_rgb_op);

  setup_texture_combine_state (a,
                               &layer->big_state->texture_combine_alpha_func,
                               layer->big_state->texture_combine_alpha_src,
                               layer->big_state->texture_combine_alpha_op);

  /* If the original layer we found is currently the authority on
   * the state we are changing see if we can revert to one of our
   * ancestors being the authority. */
  if (layer == authority &&
      _cogl_material_layer_get_parent (authority) != NULL)
    {
      CoglMaterialLayer *parent = _cogl_material_layer_get_parent (authority);
      CoglMaterialLayer *old_authority =
        _cogl_material_layer_get_authority (parent, state);

      if (_cogl_material_layer_combine_state_equal (authority,
                                                    old_authority))
        {
          layer->differences &= ~state;

          g_assert (layer->owner == material);
          if (layer->differences == 0)
            _cogl_material_prune_empty_layer_difference (material,
                                                         layer);
          goto changed;
        }
    }

  /* If we weren't previously the authority on this state then we need
   * to extended our differences mask and so it's possible that some
   * of our ancestry will now become redundant, so we aim to reparent
   * ourselves if that's true... */
  if (layer != authority)
    {
      layer->differences |= state;
      _cogl_material_layer_prune_redundant_ancestry (layer);
    }

changed:

  handle_automatic_blend_enable (material, COGL_MATERIAL_STATE_LAYERS);
  return TRUE;
}

void
cogl_material_set_layer_combine_constant (CoglMaterial *material,
				          int layer_index,
                                          const CoglColor *constant_color)
{
  CoglMaterialLayerState state = COGL_MATERIAL_LAYER_STATE_COMBINE_CONSTANT;
  CoglMaterialLayer     *layer;
  CoglMaterialLayer     *authority;
  CoglMaterialLayer     *new;

  g_return_if_fail (cogl_is_material (material));

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);

  /* Now find the ancestor of the layer that is the authority for the
   * state we want to change */
  authority = _cogl_material_layer_get_authority (layer, state);

  if (memcmp (authority->big_state->texture_combine_constant,
              constant_color, sizeof (float) * 4) == 0)
    return;

  new = _cogl_material_layer_pre_change_notify (material, layer, state);
  if (new != layer)
    layer = new;
  else
    {
      /* If the original layer we found is currently the authority on
       * the state we are changing see if we can revert to one of our
       * ancestors being the authority. */
      if (layer == authority &&
          _cogl_material_layer_get_parent (authority) != NULL)
        {
          CoglMaterialLayer *parent =
            _cogl_material_layer_get_parent (authority);
          CoglMaterialLayer *old_authority =
            _cogl_material_layer_get_authority (parent, state);
          CoglMaterialLayerBigState *old_big_state = old_authority->big_state;

          if (memcmp (old_big_state->texture_combine_constant,
                      constant_color, sizeof (float) * 4) == 0)
            {
              layer->differences &= ~state;

              g_assert (layer->owner == material);
              if (layer->differences == 0)
                _cogl_material_prune_empty_layer_difference (material,
                                                             layer);
              goto changed;
            }
        }
    }

  layer->big_state->texture_combine_constant[0] =
    cogl_color_get_red_float (constant_color);
  layer->big_state->texture_combine_constant[1] =
    cogl_color_get_green_float (constant_color);
  layer->big_state->texture_combine_constant[2] =
    cogl_color_get_blue_float (constant_color);
  layer->big_state->texture_combine_constant[3] =
    cogl_color_get_alpha_float (constant_color);

  /* If we weren't previously the authority on this state then we need
   * to extended our differences mask and so it's possible that some
   * of our ancestry will now become redundant, so we aim to reparent
   * ourselves if that's true... */
  if (layer != authority)
    {
      layer->differences |= state;
      _cogl_material_layer_prune_redundant_ancestry (layer);
    }

changed:

  handle_automatic_blend_enable (material, COGL_MATERIAL_STATE_LAYERS);
}

void
cogl_material_set_layer_matrix (CoglMaterial *material,
				int layer_index,
                                const CoglMatrix *matrix)
{
  CoglMaterialLayerState state = COGL_MATERIAL_LAYER_STATE_USER_MATRIX;
  CoglMaterialLayer     *layer;
  CoglMaterialLayer     *authority;
  CoglMaterialLayer     *new;

  g_return_if_fail (cogl_is_material (material));

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);

  /* Now find the ancestor of the layer that is the authority for the
   * state we want to change */
  authority = _cogl_material_layer_get_authority (layer, state);

  if (cogl_matrix_equal (matrix, &authority->big_state->matrix))
    return;

  new = _cogl_material_layer_pre_change_notify (material, layer, state);
  if (new != layer)
    layer = new;
  else
    {
      /* If the original layer we found is currently the authority on
       * the state we are changing see if we can revert to one of our
       * ancestors being the authority. */
      if (layer == authority &&
          _cogl_material_layer_get_parent (authority) != NULL)
        {
          CoglMaterialLayer *parent =
            _cogl_material_layer_get_parent (authority);
          CoglMaterialLayer *old_authority =
            _cogl_material_layer_get_authority (parent, state);

          if (cogl_matrix_equal (matrix, &old_authority->big_state->matrix))
            {
              layer->differences &= ~state;

              g_assert (layer->owner == material);
              if (layer->differences == 0)
                _cogl_material_prune_empty_layer_difference (material,
                                                             layer);
              return;
            }
        }
    }

  layer->big_state->matrix = *matrix;

  /* If we weren't previously the authority on this state then we need
   * to extended our differences mask and so it's possible that some
   * of our ancestry will now become redundant, so we aim to reparent
   * ourselves if that's true... */
  if (layer != authority)
    {
      layer->differences |= state;
      _cogl_material_layer_prune_redundant_ancestry (layer);
    }
}

void
cogl_material_remove_layer (CoglMaterial *material, int layer_index)
{
  CoglMaterial         *authority;
  CoglMaterialLayerInfo layer_info;
  int                   i;

  g_return_if_fail (cogl_is_material (material));

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_LAYERS);

  /* The layer index of the layer we want info about */
  layer_info.layer_index = layer_index;

  /* This will be updated with a reference to the layer being removed
   * if it can be found. */
  layer_info.layer = NULL;

  /* This will be filled in with a list of layers that need to be
   * dropped down to a lower texture unit to fill the gap of the
   * removed layer. */
  layer_info.layers_to_shift =
    g_alloca (sizeof (CoglMaterialLayer *) * authority->n_layers);
  layer_info.n_layers_to_shift = 0;

  /* Unlike when we query layer info when adding a layer we must
   * always have a complete layers_to_shift list... */
  layer_info.ignore_shift_layers_if_found = FALSE;

  _cogl_material_get_layer_info (authority, &layer_info);

  if (layer_info.layer == NULL)
    return;

  for (i = 0; i < layer_info.n_layers_to_shift; i++)
    {
      CoglMaterialLayer *shift_layer = layer_info.layers_to_shift[i];
      int unit_index = _cogl_material_layer_get_unit_index (shift_layer);
      _cogl_material_set_layer_unit (material, shift_layer, unit_index - 1);
      /* NB: shift_layer may not be writeable so _set_layer_unit()
       * will allocate a derived layer internally which will become
       * owned by material. Check the return value if we need to do
       * anything else with this layer. */
    }

  _cogl_material_remove_layer_difference (material, layer_info.layer, TRUE);
  _cogl_material_try_reverting_layers_authority (material, NULL);

  handle_automatic_blend_enable (material, COGL_MATERIAL_STATE_LAYERS);
}

static gboolean
prepend_layer_to_list_cb (CoglMaterialLayer *layer,
                          void *user_data)
{
  GList **layers = user_data;

  *layers = g_list_prepend (*layers, layer);
  return TRUE;
}

/* TODO: deprecate this API and replace it with
 * cogl_material_foreach_layer
 * TODO: update the docs to note that if the user modifies any layers
 * then the list may become invalid.
 */
const GList *
cogl_material_get_layers (CoglMaterial *material)
{
  g_return_val_if_fail (cogl_is_material (material), NULL);

  if (!material->deprecated_get_layers_list_dirty)
    g_list_free (material->deprecated_get_layers_list);

  material->deprecated_get_layers_list = NULL;

  _cogl_material_foreach_layer (material,
                                prepend_layer_to_list_cb,
                                &material->deprecated_get_layers_list);
  material->deprecated_get_layers_list =
    g_list_reverse (material->deprecated_get_layers_list);

  material->deprecated_get_layers_list_dirty = 0;

  return material->deprecated_get_layers_list;
}

int
cogl_material_get_n_layers (CoglMaterial *material)
{
  CoglMaterial *authority;

  g_return_val_if_fail (cogl_is_material (material), 0);

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_LAYERS);

  return authority->n_layers;
}

/* FIXME: deprecate and replace with
 * cogl_material_get_layer_type() instead. */
CoglMaterialLayerType
cogl_material_layer_get_type (CoglMaterialLayer *layer)
{
  return COGL_MATERIAL_LAYER_TYPE_TEXTURE;
}

/* FIXME: deprecate and replace with
 * cogl_material_get_layer_texture() instead. */
CoglHandle
cogl_material_layer_get_texture (CoglMaterialLayer *layer)
{
  g_return_val_if_fail (_cogl_is_material_layer (layer),
			COGL_INVALID_HANDLE);

  return _cogl_material_layer_get_texture (layer);
}

gboolean
_cogl_material_layer_has_user_matrix (CoglMaterialLayer *layer)
{
  CoglMaterialLayer *authority;

  g_return_val_if_fail (_cogl_is_material_layer (layer), FALSE);

  authority =
    _cogl_material_layer_get_authority (layer,
                                        COGL_MATERIAL_LAYER_STATE_USER_MATRIX);

  /* If the authority is the default material then no, otherwise yes */
  return _cogl_material_layer_get_parent (authority) ? TRUE : FALSE;
}

static void
_cogl_material_layer_get_filters (CoglMaterialLayer *layer,
                                  CoglMaterialFilter *min_filter,
                                  CoglMaterialFilter *mag_filter)
{
  CoglMaterialLayer *authority =
    _cogl_material_layer_get_authority (layer,
                                        COGL_MATERIAL_LAYER_STATE_FILTERS);

  *min_filter = authority->min_filter;
  *mag_filter = authority->mag_filter;
}

void
_cogl_material_layer_pre_paint (CoglMaterialLayer *layer)
{
  CoglMaterialLayer *texture_authority;

  texture_authority =
    _cogl_material_layer_get_authority (layer,
                                        COGL_MATERIAL_LAYER_STATE_TEXTURE);

  if (texture_authority->texture != COGL_INVALID_HANDLE)
    {
      CoglTexturePrePaintFlags flags = 0;
      CoglMaterialFilter min_filter;
      CoglMaterialFilter mag_filter;

      _cogl_material_layer_get_filters (layer, &min_filter, &mag_filter);

      if (min_filter == COGL_MATERIAL_FILTER_NEAREST_MIPMAP_NEAREST
          || min_filter == COGL_MATERIAL_FILTER_LINEAR_MIPMAP_NEAREST
          || min_filter == COGL_MATERIAL_FILTER_NEAREST_MIPMAP_LINEAR
          || min_filter == COGL_MATERIAL_FILTER_LINEAR_MIPMAP_LINEAR)
        flags |= COGL_TEXTURE_NEEDS_MIPMAP;

      _cogl_texture_pre_paint (layer->texture, flags);
    }
}

CoglMaterialFilter
cogl_material_layer_get_min_filter (CoglMaterialLayer *layer)
{
  CoglMaterialLayer *authority;

  g_return_val_if_fail (_cogl_is_material_layer (layer), 0);

  authority =
    _cogl_material_layer_get_authority (layer,
                                        COGL_MATERIAL_LAYER_STATE_FILTERS);

  return authority->min_filter;
}

CoglMaterialFilter
cogl_material_layer_get_mag_filter (CoglMaterialLayer *layer)
{
  CoglMaterialLayer *authority;

  g_return_val_if_fail (_cogl_is_material_layer (layer), 0);

  authority =
    _cogl_material_layer_get_authority (layer,
                                        COGL_MATERIAL_LAYER_STATE_FILTERS);

  return authority->mag_filter;
}

void
cogl_material_set_layer_filters (CoglMaterial      *material,
                                 int                layer_index,
                                 CoglMaterialFilter min_filter,
                                 CoglMaterialFilter mag_filter)
{
  CoglMaterialLayerState state = COGL_MATERIAL_LAYER_STATE_FILTERS;
  CoglMaterialLayer     *layer;
  CoglMaterialLayer     *authority;
  CoglMaterialLayer     *new;

  g_return_if_fail (cogl_is_material (material));

  /* Note: this will ensure that the layer exists, creating one if it
   * doesn't already.
   *
   * Note: If the layer already existed it's possibly owned by another
   * material. If the layer is created then it will be owned by
   * material. */
  layer = _cogl_material_get_layer (material, layer_index);

  /* Now find the ancestor of the layer that is the authority for the
   * state we want to change */
  authority = _cogl_material_layer_get_authority (layer, state);

  if (authority->min_filter == min_filter &&
      authority->mag_filter == mag_filter)
    return;

  new = _cogl_material_layer_pre_change_notify (material, layer, state);
  if (new != layer)
    layer = new;
  else
    {
      /* If the original layer we found is currently the authority on
       * the state we are changing see if we can revert to one of our
       * ancestors being the authority. */
      if (layer == authority &&
          _cogl_material_layer_get_parent (authority) != NULL)
        {
          CoglMaterialLayer *parent =
            _cogl_material_layer_get_parent (authority);
          CoglMaterialLayer *old_authority =
            _cogl_material_layer_get_authority (parent, state);

          if (old_authority->min_filter == min_filter &&
              old_authority->mag_filter == mag_filter)
            {
              layer->differences &= ~state;

              g_assert (layer->owner == material);
              if (layer->differences == 0)
                _cogl_material_prune_empty_layer_difference (material,
                                                             layer);
              return;
            }
        }
    }

  layer->min_filter = min_filter;
  layer->mag_filter = mag_filter;

  /* If we weren't previously the authority on this state then we need
   * to extended our differences mask and so it's possible that some
   * of our ancestry will now become redundant, so we aim to reparent
   * ourselves if that's true... */
  if (layer != authority)
    {
      layer->differences |= state;
      _cogl_material_layer_prune_redundant_ancestry (layer);
    }
}

float
cogl_material_get_point_size (CoglHandle  handle)
{
  CoglMaterial *material = COGL_MATERIAL (handle);
  CoglMaterial *authority;

  g_return_val_if_fail (cogl_is_material (handle), FALSE);

  authority =
    _cogl_material_get_authority (material, COGL_MATERIAL_STATE_POINT_SIZE);

  return authority->big_state->point_size;
}

void
cogl_material_set_point_size (CoglHandle handle,
                              float      point_size)
{
  CoglMaterial *material = COGL_MATERIAL (handle);
  CoglMaterialState state = COGL_MATERIAL_STATE_POINT_SIZE;
  CoglMaterial *authority;

  g_return_if_fail (cogl_is_material (handle));

  authority = _cogl_material_get_authority (material, state);

  if (authority->big_state->point_size == point_size)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the material has no dependants so it may be modified.
   * - If the material isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_material_pre_change_notify (material, state, NULL);

  material->big_state->point_size = point_size;

  _cogl_material_update_authority (material, authority, state,
                                   _cogl_material_point_size_equal);
}

static void
disable_texture_unit (int unit_index)
{
  CoglTextureUnit *unit;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  unit = &g_array_index (ctx->texture_units, CoglTextureUnit, unit_index);

  if (unit->enabled)
    {
      _cogl_set_active_texture_unit (unit_index);
      GE (glDisable (unit->current_gl_target));
      unit->enabled = FALSE;
    }
}

void
_cogl_gl_use_program_wrapper (GLuint program)
{
#ifdef COGL_MATERIAL_BACKEND_GLSL
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->current_gl_program == program)
    return;

  if (program)
    {
      GLenum gl_error;

      while ((gl_error = glGetError ()) != GL_NO_ERROR)
        ;
      glUseProgram (program);
      if (glGetError () != GL_NO_ERROR)
        {
          GE (glUseProgram (0));
          ctx->current_gl_program = 0;
          return;
        }
    }
  else
    GE (glUseProgram (0));

  ctx->current_gl_program = program;
#endif
}

static void
disable_glsl (void)
{
#ifdef COGL_MATERIAL_BACKEND_GLSL
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->current_use_program_type == COGL_MATERIAL_PROGRAM_TYPE_GLSL)
    _cogl_gl_use_program_wrapper (0);
#endif
}

static void
disable_arbfp (void)
{
#ifdef COGL_MATERIAL_BACKEND_ARBFP
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->current_use_program_type == COGL_MATERIAL_PROGRAM_TYPE_ARBFP)
    GE (glDisable (GL_FRAGMENT_PROGRAM_ARB));
#endif
}

void
_cogl_use_program (CoglHandle program_handle, CoglMaterialProgramType type)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  switch (type)
    {
#ifdef COGL_MATERIAL_BACKEND_GLSL
    case COGL_MATERIAL_PROGRAM_TYPE_GLSL:
      {
        /* The GLES2 backend currently manages its own codegen for
         * fixed function API fallbacks and manages its own shader
         * state.  */
#ifndef HAVE_COGL_GLES2
        CoglProgram *program =
          _cogl_program_pointer_from_handle (program_handle);

        _cogl_gl_use_program_wrapper (program->gl_handle);
        disable_arbfp ();
#endif

        ctx->current_use_program_type = type;
        break;
      }
#else
    case COGL_MATERIAL_PROGRAM_TYPE_GLSL:
      g_warning ("Unexpected use of GLSL backend!");
      break;
#endif
#ifdef COGL_MATERIAL_BACKEND_ARBFP
    case COGL_MATERIAL_PROGRAM_TYPE_ARBFP:

      /* _cogl_gl_use_program_wrapper can be called by cogl-program.c
       * so we can't bailout without making sure we glUseProgram (0)
       * first. */
      disable_glsl ();

      if (ctx->current_use_program_type == COGL_MATERIAL_PROGRAM_TYPE_ARBFP)
        break;

      GE (glEnable (GL_FRAGMENT_PROGRAM_ARB));

      ctx->current_use_program_type = type;
      break;
#else
    case COGL_MATERIAL_PROGRAM_TYPE_ARBFP:
      g_warning ("Unexpected use of GLSL backend!");
      break;
#endif
#ifdef COGL_MATERIAL_BACKEND_FIXED
    case COGL_MATERIAL_PROGRAM_TYPE_FIXED:

      /* _cogl_gl_use_program_wrapper can be called by cogl-program.c
       * so we can't bailout without making sure we glUseProgram (0)
       * first. */
      disable_glsl ();

      if (ctx->current_use_program_type == COGL_MATERIAL_PROGRAM_TYPE_FIXED)
        break;

      disable_arbfp ();

      ctx->current_use_program_type = type;
#endif
    }
}

#if defined (COGL_MATERIAL_BACKEND_GLSL) || \
    defined (COGL_MATERIAL_BACKEND_ARBFP)
int
_cogl_get_max_texture_image_units (void)
{
  _COGL_GET_CONTEXT (ctx, 0);

  /* This function is called quite often so we cache the value to
     avoid too many GL calls */
  if (G_UNLIKELY (ctx->max_texture_image_units == -1))
    {
      ctx->max_texture_image_units = 1;
      GE (glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS,
                         &ctx->max_texture_image_units));
    }

  return ctx->max_texture_image_units;
}
#endif

static void
_cogl_material_layer_get_texture_info (CoglMaterialLayer *layer,
                                       CoglHandle *texture,
                                       GLuint *gl_texture,
                                       GLuint *gl_target)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  *texture = layer->texture;
  if (G_UNLIKELY (*texture == COGL_INVALID_HANDLE))
    *texture = ctx->default_gl_texture_2d_tex;
  if (layer->texture_overridden)
    {
      *gl_texture = layer->slice_gl_texture;
      *gl_target = layer->slice_gl_target;
    }
  else
    cogl_texture_get_gl_texture (*texture, gl_texture, gl_target);
}

#ifndef HAVE_COGL_GLES

static gboolean
blend_factor_uses_constant (GLenum blend_factor)
{
  return (blend_factor == GL_CONSTANT_COLOR ||
          blend_factor == GL_ONE_MINUS_CONSTANT_COLOR ||
          blend_factor == GL_CONSTANT_ALPHA ||
          blend_factor == GL_ONE_MINUS_CONSTANT_ALPHA);
}

#endif

static void
flush_depth_state (CoglMaterialDepthState *depth_state)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->depth_test_function_cache != depth_state->depth_test_function)
    {
      GE (glDepthFunc (depth_state->depth_test_function));
      ctx->depth_test_function_cache = depth_state->depth_test_function;
    }

  if (ctx->depth_writing_enabled_cache != depth_state->depth_writing_enabled)
    {
      GE (glDepthMask (depth_state->depth_writing_enabled ?
                       GL_TRUE : GL_FALSE));
      ctx->depth_writing_enabled_cache = depth_state->depth_writing_enabled;
    }

#ifndef COGL_HAS_GLES
  if (ctx->depth_range_near_cache != depth_state->depth_range_near ||
      ctx->depth_range_far_cache != depth_state->depth_range_far)
    {
#ifdef COGL_HAS_GLES2
      GE (glDepthRangef (depth_state->depth_range_near,
                         depth_state->depth_range_far));
#else
      GE (glDepthRange (depth_state->depth_range_near,
                        depth_state->depth_range_far));
#endif
      ctx->depth_range_near_cache = depth_state->depth_range_near;
      ctx->depth_range_far_cache = depth_state->depth_range_far;
    }
#endif /* COGL_HAS_GLES */
}

static void
_cogl_material_flush_color_blend_alpha_depth_state (
                                            CoglMaterial *material,
                                            unsigned long materials_difference,
                                            gboolean      skip_gl_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!skip_gl_color)
    {
      if ((materials_difference & COGL_MATERIAL_STATE_COLOR) ||
          /* Assume if we were previously told to skip the color, then
           * the current color needs updating... */
          ctx->current_material_skip_gl_color)
        {
          CoglMaterial *authority =
            _cogl_material_get_authority (material, COGL_MATERIAL_STATE_COLOR);
          GE (glColor4ub (cogl_color_get_red_byte (&authority->color),
                          cogl_color_get_green_byte (&authority->color),
                          cogl_color_get_blue_byte (&authority->color),
                          cogl_color_get_alpha_byte (&authority->color)));
        }
    }

  if (materials_difference & COGL_MATERIAL_STATE_LIGHTING)
    {
      CoglMaterial *authority =
        _cogl_material_get_authority (material, COGL_MATERIAL_STATE_LIGHTING);
      CoglMaterialLightingState *lighting_state =
        &authority->big_state->lighting_state;

      /* FIXME - we only need to set these if lighting is enabled... */
      GLfloat shininess = lighting_state->shininess * 128.0f;

      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, lighting_state->ambient));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, lighting_state->diffuse));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, lighting_state->specular));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, lighting_state->emission));
      GE (glMaterialfv (GL_FRONT_AND_BACK, GL_SHININESS, &shininess));
    }

  if (materials_difference & COGL_MATERIAL_STATE_BLEND)
    {
      CoglMaterial *authority =
        _cogl_material_get_authority (material, COGL_MATERIAL_STATE_BLEND);
      CoglMaterialBlendState *blend_state =
        &authority->big_state->blend_state;

#if defined (HAVE_COGL_GLES2)
      gboolean have_blend_equation_seperate = TRUE;
      gboolean have_blend_func_separate = TRUE;
#elif defined (HAVE_COGL_GL)
      gboolean have_blend_equation_seperate = FALSE;
      gboolean have_blend_func_separate = FALSE;
      if (ctx->drv.pf_glBlendEquationSeparate) /* Only GL 2.0 + */
        have_blend_equation_seperate = TRUE;
      if (ctx->drv.pf_glBlendFuncSeparate) /* Only GL 1.4 + */
        have_blend_func_separate = TRUE;
#endif

#ifndef HAVE_COGL_GLES /* GLES 1 only has glBlendFunc */
      if (blend_factor_uses_constant (blend_state->blend_src_factor_rgb) ||
          blend_factor_uses_constant (blend_state->blend_src_factor_alpha) ||
          blend_factor_uses_constant (blend_state->blend_dst_factor_rgb) ||
          blend_factor_uses_constant (blend_state->blend_dst_factor_alpha))
        {
          float red =
            cogl_color_get_red_float (&blend_state->blend_constant);
          float green =
            cogl_color_get_green_float (&blend_state->blend_constant);
          float blue =
            cogl_color_get_blue_float (&blend_state->blend_constant);
          float alpha =
            cogl_color_get_alpha_float (&blend_state->blend_constant);


          GE (glBlendColor (red, green, blue, alpha));
        }

      if (have_blend_equation_seperate &&
          blend_state->blend_equation_rgb != blend_state->blend_equation_alpha)
        GE (glBlendEquationSeparate (blend_state->blend_equation_rgb,
                                     blend_state->blend_equation_alpha));
      else
        GE (glBlendEquation (blend_state->blend_equation_rgb));

      if (have_blend_func_separate &&
          (blend_state->blend_src_factor_rgb != blend_state->blend_src_factor_alpha ||
           (blend_state->blend_src_factor_rgb !=
            blend_state->blend_src_factor_alpha)))
        GE (glBlendFuncSeparate (blend_state->blend_src_factor_rgb,
                                 blend_state->blend_dst_factor_rgb,
                                 blend_state->blend_src_factor_alpha,
                                 blend_state->blend_dst_factor_alpha));
      else
#endif
        GE (glBlendFunc (blend_state->blend_src_factor_rgb,
                         blend_state->blend_dst_factor_rgb));
    }

  if (materials_difference & COGL_MATERIAL_STATE_ALPHA_FUNC)
    {
      CoglMaterial *authority =
        _cogl_material_get_authority (material, COGL_MATERIAL_STATE_ALPHA_FUNC);
      CoglMaterialAlphaFuncState *alpha_state =
        &authority->big_state->alpha_state;

      /* NB: Currently the Cogl defines are compatible with the GL ones: */
      GE (glAlphaFunc (alpha_state->alpha_func,
                       alpha_state->alpha_func_reference));
    }

  if (materials_difference & COGL_MATERIAL_STATE_DEPTH)
    {
      CoglMaterial *authority =
        _cogl_material_get_authority (material, COGL_MATERIAL_STATE_DEPTH);
      CoglMaterialDepthState *depth_state = &authority->big_state->depth_state;

      if (depth_state->depth_test_enabled)
        {
          if (ctx->depth_test_enabled_cache != TRUE)
            {
              GE (glEnable (GL_DEPTH_TEST));
              ctx->depth_test_enabled_cache = depth_state->depth_test_enabled;
            }
          flush_depth_state (depth_state);
        }
      else if (ctx->depth_test_enabled_cache != FALSE)
        {
          GE (glDisable (GL_DEPTH_TEST));
          ctx->depth_test_enabled_cache = depth_state->depth_test_enabled;
        }
    }

  if (materials_difference & COGL_MATERIAL_STATE_POINT_SIZE)
    {
      CoglMaterial *authority =
        _cogl_material_get_authority (material, COGL_MATERIAL_STATE_POINT_SIZE);

      if (ctx->point_size_cache != authority->big_state->point_size)
        {
          GE( glPointSize (authority->big_state->point_size) );
          ctx->point_size_cache = authority->big_state->point_size;
        }
    }

  if (material->real_blend_enable != ctx->gl_blend_enable_cache)
    {
      if (material->real_blend_enable)
        GE (glEnable (GL_BLEND));
      else
        GE (glDisable (GL_BLEND));
      /* XXX: we shouldn't update any other blend state if blending
       * is disabled! */
      ctx->gl_blend_enable_cache = material->real_blend_enable;
    }
}

static int
get_max_activateable_texture_units (void)
{
  _COGL_GET_CONTEXT (ctx, 0);

  if (G_UNLIKELY (ctx->max_activateable_texture_units == -1))
    {
#ifdef HAVE_COGL_GL
      GLint max_tex_coords;
      GLint max_combined_tex_units;
      GE (glGetIntegerv (GL_MAX_TEXTURE_COORDS, &max_tex_coords));
      GE (glGetIntegerv (GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                         &max_combined_tex_units));
      ctx->max_activateable_texture_units =
        MAX (max_tex_coords - 1, max_combined_tex_units);
#else
      GE (glGetIntegerv (GL_MAX_TEXTURE_UNITS,
                         &ctx->max_activateable_texture_units));
#endif
    }

  return ctx->max_activateable_texture_units;
}

typedef struct
{
  int i;
  unsigned long *layer_differences;
} CoglMaterialFlushLayerState;

static gboolean
flush_layers_common_gl_state_cb (CoglMaterialLayer *layer, void *user_data)
{
  CoglMaterialFlushLayerState *flush_state = user_data;
  int                          unit_index = flush_state->i;
  CoglTextureUnit             *unit = _cogl_get_texture_unit (unit_index);
  unsigned long                layers_difference =
    flush_state->layer_differences[unit_index];

  /* There may not be enough texture units so we can bail out if
   * that's the case...
   */
  if (G_UNLIKELY (unit_index >= get_max_activateable_texture_units ()))
    {
      static gboolean shown_warning = FALSE;

      if (!shown_warning)
        {
          g_warning ("Your hardware does not have enough texture units"
                     "to handle this many texture layers");
          shown_warning = TRUE;
        }
      return FALSE;
    }

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_TEXTURE)
    {
      CoglMaterialLayer *authority =
        _cogl_material_layer_get_authority (layer,
                                            COGL_MATERIAL_LAYER_STATE_TEXTURE);
      CoglHandle texture = NULL;
      GLuint     gl_texture;
      GLenum     gl_target;

      _cogl_material_layer_get_texture_info (authority,
                                             &texture,
                                             &gl_texture,
                                             &gl_target);

      _cogl_set_active_texture_unit (unit_index);

      /* NB: There are several Cogl components and some code in
       * Clutter that will temporarily bind arbitrary GL textures to
       * query and modify texture object parameters. If you look at
       * _cogl_bind_gl_texture_transient() you can see we make sure
       * that such code always binds to texture unit 1 which means we
       * can't rely on the unit->gl_texture state if unit->index == 1.
       *
       * Because texture unit 1 is a bit special we actually defer any
       * necessary glBindTexture for it until the end of
       * _cogl_material_flush_gl_state().
       *
       * NB: we get notified whenever glDeleteTextures is used (see
       * _cogl_delete_gl_texture()) where we invalidate
       * unit->gl_texture references to deleted textures so it's safe
       * to compare unit->gl_texture with gl_texture.  (Without the
       * hook it would be possible to delete a GL texture and create a
       * new one with the same name and comparing unit->gl_texture and
       * gl_texture wouldn't detect that.)
       *
       * NB: for foreign textures we don't know how the deletion of
       * the GL texture objects correspond to the deletion of the
       * CoglTextures so if there was previously a foreign texture
       * associated with the texture unit then we can't assume that we
       * aren't seeing a recycled texture name so we have to bind.
       */
      if (unit->gl_texture != gl_texture || unit->is_foreign)
        {
          if (unit_index != 1)
            GE (glBindTexture (gl_target, gl_texture));
          unit->gl_texture = gl_texture;
        }

      unit->is_foreign = _cogl_texture_is_foreign (texture);

      /* Disable the previous target if it was different and it's
       * still enabled */
      if (unit->enabled && unit->current_gl_target != gl_target)
        GE (glDisable (unit->current_gl_target));

      if (!G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DISABLE_TEXTURING) &&
          (!unit->enabled || unit->current_gl_target != gl_target))
        {
          GE (glEnable (gl_target));
          unit->enabled = TRUE;
          unit->current_gl_target = gl_target;
        }

      /* The texture_storage_changed boolean indicates if the
       * CoglTexture's underlying GL texture storage has changed since
       * it was flushed to the texture unit. We've just flushed the
       * latest state so we can reset this. */
      unit->texture_storage_changed = FALSE;
    }
  else
    {
      /* Even though there may be no difference between the last flushed
       * texture state and the current layers texture state it may be that the
       * texture unit has been disabled for some time so we need to assert that
       * it's enabled now.
       */
      if (!G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DISABLE_TEXTURING) &&
          !unit->enabled)
        {
          GE (glEnable (unit->current_gl_target));
          unit->enabled = TRUE;
        }
    }

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_USER_MATRIX)
    {
      CoglMaterialLayerState state = COGL_MATERIAL_LAYER_STATE_USER_MATRIX;
      CoglMaterialLayer *authority =
        _cogl_material_layer_get_authority (layer, state);

      _cogl_matrix_stack_set (unit->matrix_stack,
                              &authority->big_state->matrix);

      _cogl_matrix_stack_flush_to_gl (unit->matrix_stack, COGL_MATRIX_TEXTURE);
    }

  if (layers_difference & COGL_MATERIAL_LAYER_STATE_POINT_SPRITE_COORDS)
    {
      CoglMaterialState change = COGL_MATERIAL_LAYER_STATE_POINT_SPRITE_COORDS;
      CoglMaterialLayer *authority =
        _cogl_material_layer_get_authority (layer, change);
      CoglMaterialLayerBigState *big_state = authority->big_state;

      _cogl_set_active_texture_unit (unit_index);

      GE (glTexEnvi (GL_POINT_SPRITE, GL_COORD_REPLACE,
                     big_state->point_sprite_coords));
    }

  cogl_handle_ref (layer);
  if (unit->layer != COGL_INVALID_HANDLE)
    cogl_handle_unref (unit->layer);

  unit->layer = layer;
  unit->layer_changes_since_flush = 0;

  flush_state->i++;

  return TRUE;
}

static void
_cogl_material_flush_common_gl_state (CoglMaterial  *material,
                                      unsigned long  materials_difference,
                                      unsigned long *layer_differences,
                                      gboolean       skip_gl_color)
{
  CoglMaterialFlushLayerState state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_material_flush_color_blend_alpha_depth_state (material,
                                                      materials_difference,
                                                      skip_gl_color);

  state.i = 0;
  state.layer_differences = layer_differences;
  _cogl_material_foreach_layer (material,
                                flush_layers_common_gl_state_cb,
                                &state);

  /* Disable additional texture units that may have previously been in use.. */
  for (; state.i < ctx->texture_units->len; state.i++)
    disable_texture_unit (state.i);
}

/* Re-assert the layer's wrap modes on the given CoglTexture.
 *
 * Note: we don't simply forward the wrap modes to layer->texture
 * since the actual texture being used may have been overridden.
 */
static void
_cogl_material_layer_forward_wrap_modes (CoglMaterialLayer *layer,
                                         CoglHandle texture)
{
  CoglMaterialWrapModeInternal wrap_mode_s, wrap_mode_t, wrap_mode_r;
  GLenum gl_wrap_mode_s, gl_wrap_mode_t, gl_wrap_mode_r;

  if (texture == COGL_INVALID_HANDLE)
    return;

  _cogl_material_layer_get_wrap_modes (layer,
                                       &wrap_mode_s,
                                       &wrap_mode_t,
                                       &wrap_mode_r);

  /* Update the wrap mode on the texture object. The texture backend
     should cache the value so that it will be a no-op if the object
     already has the same wrap mode set. The backend is best placed to
     do this because it knows how many of the coordinates will
     actually be used (ie, a 1D texture only cares about the 's'
     coordinate but a 3D texture would use all three). GL uses the
     wrap mode as part of the texture object state but we are
     pretending it's part of the per-layer environment state. This
     will break if the application tries to use different modes in
     different layers using the same texture. */

  if (wrap_mode_s == COGL_MATERIAL_WRAP_MODE_INTERNAL_AUTOMATIC)
    gl_wrap_mode_s = GL_CLAMP_TO_EDGE;
  else
    gl_wrap_mode_s = wrap_mode_s;

  if (wrap_mode_t == COGL_MATERIAL_WRAP_MODE_INTERNAL_AUTOMATIC)
    gl_wrap_mode_t = GL_CLAMP_TO_EDGE;
  else
    gl_wrap_mode_t = wrap_mode_t;

  if (wrap_mode_r == COGL_MATERIAL_WRAP_MODE_INTERNAL_AUTOMATIC)
    gl_wrap_mode_r = GL_CLAMP_TO_EDGE;
  else
    gl_wrap_mode_r = wrap_mode_r;

  _cogl_texture_set_wrap_mode_parameters (texture,
                                          gl_wrap_mode_s,
                                          gl_wrap_mode_t,
                                          gl_wrap_mode_r);
}

/* OpenGL associates the min/mag filters and repeat modes with the
 * texture object not the texture unit so we always have to re-assert
 * the filter and repeat modes whenever we use a texture since it may
 * be referenced by multiple materials with different modes.
 *
 * XXX: GL_ARB_sampler_objects fixes this in OpenGL so we should
 * eventually look at using this extension when available.
 */
static void
foreach_texture_unit_update_filter_and_wrap_modes (void)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (i = 0; i < ctx->texture_units->len; i++)
    {
      CoglTextureUnit *unit =
        &g_array_index (ctx->texture_units, CoglTextureUnit, i);

      if (!unit->enabled)
        break;

      if (unit->layer)
        {
          CoglHandle texture = _cogl_material_layer_get_texture (unit->layer);
          CoglMaterialFilter min;
          CoglMaterialFilter mag;

          _cogl_material_layer_get_filters (unit->layer, &min, &mag);
          _cogl_texture_set_filters (texture, min, mag);

          _cogl_material_layer_forward_wrap_modes (unit->layer, texture);
        }
    }
}

typedef struct
{
  int i;
  unsigned long *layer_differences;
} CoglMaterialCompareLayersState;

static gboolean
compare_layer_differences_cb (CoglMaterialLayer *layer, void *user_data)
{
  CoglMaterialCompareLayersState *state = user_data;
  CoglTextureUnit *unit = _cogl_get_texture_unit (state->i);

  if (unit->layer == layer)
    state->layer_differences[state->i] = unit->layer_changes_since_flush;
  else if (unit->layer)
    {
      state->layer_differences[state->i] = unit->layer_changes_since_flush;
      state->layer_differences[state->i] |=
        _cogl_material_layer_compare_differences (layer, unit->layer);
    }
  else
    state->layer_differences[state->i] = COGL_MATERIAL_LAYER_STATE_ALL_SPARSE;

  /* XXX: There is always a possibility that a CoglTexture's
   * underlying GL texture storage has been changed since it was last
   * bound to a texture unit which is why we have a callback into
   * _cogl_material_texture_storage_change_notify whenever a textures
   * underlying GL texture storage changes which will set the
   * unit->texture_intern_changed flag. If we see that's been set here
   * then we force an update of the texture state...
   */
  if (unit->texture_storage_changed)
    state->layer_differences[state->i] |= COGL_MATERIAL_LAYER_STATE_TEXTURE;

  state->i++;

  return TRUE;
}

typedef struct
{
  const CoglMaterialBackend *backend;
  CoglMaterial *material;
  unsigned long *layer_differences;
  gboolean error_adding_layer;
  gboolean added_layer;
} CoglMaterialBackendAddLayerState;


static gboolean
backend_add_layer_cb (CoglMaterialLayer *layer,
                      void *user_data)
{
  CoglMaterialBackendAddLayerState *state = user_data;
  const CoglMaterialBackend *backend = state->backend;
  CoglMaterial *material = state->material;
  int unit_index = _cogl_material_layer_get_unit_index (layer);
  CoglTextureUnit *unit = _cogl_get_texture_unit (unit_index);

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* NB: We don't support the random disabling of texture
   * units, so as soon as we hit a disabled unit we know all
   * subsequent units are also disabled */
  if (!unit->enabled)
    return FALSE;

  if (G_UNLIKELY (unit_index >= backend->get_max_texture_units ()))
    {
      int j;
      for (j = unit_index; j < ctx->texture_units->len; j++)
        disable_texture_unit (j);
      /* TODO: although this isn't considered an error that
       * warrants falling back to a different backend we
       * should print a warning here. */
      return FALSE;
    }

  /* Either generate per layer code snippets or setup the
   * fixed function glTexEnv for each layer... */
  if (G_LIKELY (backend->add_layer (material,
                                    layer,
                                    state->layer_differences[unit_index])))
    state->added_layer = TRUE;
  else
    {
      state->error_adding_layer = TRUE;
      return FALSE;
    }

  return TRUE;
}

/*
 * _cogl_material_flush_gl_state:
 *
 * Details of override options:
 * ->fallback_mask: is a bitmask of the material layers that need to be
 *    replaced with the default, fallback textures. The fallback textures are
 *    fully transparent textures so they hopefully wont contribute to the
 *    texture combining.
 *
 *    The intention of fallbacks is to try and preserve
 *    the number of layers the user is expecting so that texture coordinates
 *    they gave will mostly still correspond to the textures they intended, and
 *    have a fighting chance of looking close to their originally intended
 *    result.
 *
 * ->disable_mask: is a bitmask of the material layers that will simply have
 *    texturing disabled. It's only really intended for disabling all layers
 *    > X; i.e. we'd expect to see a contiguous run of 0 starting from the LSB
 *    and at some point the remaining bits flip to 1. It might work to disable
 *    arbitrary layers; though I'm not sure a.t.m how OpenGL would take to
 *    that.
 *
 *    The intention of the disable_mask is for emitting geometry when the user
 *    hasn't supplied enough texture coordinates for all the layers and it's
 *    not possible to auto generate default texture coordinates for those
 *    layers.
 *
 * ->layer0_override_texture: forcibly tells us to bind this GL texture name for
 *    layer 0 instead of plucking the gl_texture from the CoglTexture of layer
 *    0.
 *
 *    The intention of this is for any primitives that supports sliced textures.
 *    The code will can iterate each of the slices and re-flush the material
 *    forcing the GL texture of each slice in turn.
 *
 * ->wrap_mode_overrides: overrides the wrap modes set on each
 *    layer. This is used to implement the automatic wrap mode.
 *
 * XXX: It might also help if we could specify a texture matrix for code
 *    dealing with slicing that would be multiplied with the users own matrix.
 *
 *    Normaly texture coords in the range [0, 1] refer to the extents of the
 *    texture, but when your GL texture represents a slice of the real texture
 *    (from the users POV) then a texture matrix would be a neat way of
 *    transforming the mapping for each slice.
 *
 *    Currently for textured rectangles we manually calculate the texture
 *    coords for each slice based on the users given coords, but this solution
 *    isn't ideal, and can't be used with CoglVertexBuffers.
 */
void
_cogl_material_flush_gl_state (CoglMaterial *material,
                               gboolean skip_gl_color)
{
  unsigned long    materials_difference;
  int              n_layers;
  unsigned long   *layer_differences = NULL;
  int              i;
  CoglTextureUnit *unit1;

  COGL_STATIC_TIMER (material_flush_timer,
                     "Mainloop", /* parent */
                     "Material Flush",
                     "The time spent flushing material state",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context, material_flush_timer);

  if (ctx->current_material == material)
    materials_difference = ctx->current_material_changes_since_flush;
  else if (ctx->current_material)
    {
      materials_difference = ctx->current_material_changes_since_flush;
      materials_difference |=
        _cogl_material_compare_differences (ctx->current_material,
                                            material);
    }
  else
    materials_difference = COGL_MATERIAL_STATE_ALL_SPARSE;

  /* Get a layer_differences mask for each layer to be flushed */
  n_layers = cogl_material_get_n_layers (material);
  if (n_layers)
    {
      CoglMaterialCompareLayersState state;
      layer_differences = g_alloca (sizeof (unsigned long *) * n_layers);
      memset (layer_differences, 0, sizeof (layer_differences));
      state.i = 0;
      state.layer_differences = layer_differences;
      _cogl_material_foreach_layer (material,
                                    compare_layer_differences_cb,
                                    &state);
    }

  /* First flush everything that's the same regardless of which
   * material backend is being used...
   *
   * 1) top level state:
   *  glColor (or skip if a vertex attribute is being used for color)
   *  blend state
   *  alpha test state (except for GLES 2.0)
   *
   * 2) then foreach layer:
   *  determine gl_target/gl_texture
   *  bind texture
   *  enable/disable target
   *  flush user matrix
   *
   *  Note: After _cogl_material_flush_common_gl_state you can expect
   *  all state of the layers corresponding texture unit to be
   *  updated.
   */
  _cogl_material_flush_common_gl_state (material,
                                        materials_difference,
                                        layer_differences,
                                        skip_gl_color);

  /* Now flush the fragment processing state according to the current
   * fragment processing backend.
   *
   * Note: Some of the backends may not support the current material
   * configuration and in that case it will report an error and we
   * will fallback to a different backend.
   *
   * NB: if material->backend != COGL_MATERIAL_BACKEND_UNDEFINED then
   * we have previously managed to successfully flush this material
   * with the given backend so we will simply use that to avoid
   * fallback code paths.
   */

  if (material->backend == COGL_MATERIAL_BACKEND_UNDEFINED)
    _cogl_material_set_backend (material, COGL_MATERIAL_BACKEND_DEFAULT);

  for (i = material->backend;
       i < G_N_ELEMENTS (backends);
       i++, _cogl_material_set_backend (material, i))
    {
      const CoglMaterialBackend *backend = backends[i];
      CoglMaterialBackendAddLayerState state;

      /* E.g. For backends generating code they can setup their
       * scratch buffers here... */
      if (G_UNLIKELY (!backend->start (material,
                                       n_layers,
                                       materials_difference)))
        continue;

      state.backend = backend;
      state.material = material;
      state.layer_differences = layer_differences;
      state.error_adding_layer = FALSE;
      state.added_layer = FALSE;
      _cogl_material_foreach_layer (material,
                                    backend_add_layer_cb,
                                    &state);

      if (G_UNLIKELY (state.error_adding_layer))
        continue;

      if (!state.added_layer &&
          backend->passthrough &&
          G_UNLIKELY (!backend->passthrough (material)))
        continue;

      /* For backends generating code they may compile and link their
       * programs here, update any uniforms and tell OpenGL to use
       * that program.
       */
      if (G_UNLIKELY (!backend->end (material, materials_difference)))
        continue;

      break;
    }

  /* FIXME: This reference is actually resulting in lots of
   * copy-on-write reparenting because one-shot materials end up
   * living for longer than necessary and so any later modification of
   * the parent will cause a copy-on-write.
   *
   * XXX: The issue should largely go away when we switch to using
   * weak materials for overrides.
   */
  cogl_object_ref (material);
  if (ctx->current_material != NULL)
    cogl_object_unref (ctx->current_material);
  ctx->current_material = material;
  ctx->current_material_changes_since_flush = 0;
  ctx->current_material_skip_gl_color = skip_gl_color;

  /* Handle the fact that OpenGL associates texture filter and wrap
   * modes with the texture objects not the texture units... */
  foreach_texture_unit_update_filter_and_wrap_modes ();

  /* If this material has more than one layer then we always need
   * to make sure we rebind the texture for unit 1.
   *
   * NB: various components of Cogl may temporarily bind arbitrary
   * textures to texture unit 1 so they can query and modify texture
   * object parameters. cogl-material.c (See
   * _cogl_bind_gl_texture_transient)
   */
  unit1 = _cogl_get_texture_unit (1);
  if (unit1->enabled && unit1->dirty_gl_texture)
    {
      _cogl_set_active_texture_unit (1);
      GE (glBindTexture (unit1->current_gl_target, unit1->gl_texture));
      unit1->dirty_gl_texture = FALSE;
    }

  COGL_TIMER_STOP (_cogl_uprof_context, material_flush_timer);
}

/* While a material is referenced by the Cogl journal we can not allow
 * modifications, so this gives us a mechanism to track journal
 * references separately */
CoglMaterial *
_cogl_material_journal_ref (CoglMaterial *material)
{
  material->journal_ref_count++;
  return cogl_object_ref (material);
}

void
_cogl_material_journal_unref (CoglMaterial *material)
{
  material->journal_ref_count--;
  cogl_object_unref (material);
}

void
_cogl_material_apply_legacy_state (CoglMaterial *material)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* It was a mistake that we ever copied the OpenGL style API for
   * associating these things directly with the context when we
   * originally wrote Cogl. Until the corresponding deprecated APIs
   * can be removed though we now shoehorn the state changes through
   * the cogl_material API instead.
   */

  if (ctx->current_program)
    _cogl_material_set_user_program (material, ctx->current_program);

  if (ctx->legacy_depth_test_enabled)
    cogl_material_set_depth_test_enabled (material, TRUE);

  if (ctx->legacy_fog_state.enabled)
    _cogl_material_set_fog_state (material, &ctx->legacy_fog_state);
}

void
_cogl_material_set_static_breadcrumb (CoglMaterial *material,
                                      const char *breadcrumb)
{
  material->has_static_breadcrumb = TRUE;
  material->static_breadcrumb = breadcrumb;
}

typedef struct
{
  int parent_id;
  int *node_id_ptr;
  GString *graph;
  int indent;
} PrintDebugState;

static gboolean
dump_layer_cb (CoglMaterialNode *node, void *user_data)
{
  CoglMaterialLayer *layer = COGL_MATERIAL_LAYER (node);
  PrintDebugState *state = user_data;
  int layer_id = *state->node_id_ptr;
  PrintDebugState state_out;
  GString *changes_label;
  gboolean changes = FALSE;

  if (state->parent_id >= 0)
    g_string_append_printf (state->graph, "%*slayer%d -> layer%d;\n",
                            state->indent, "",
                            state->parent_id,
                            layer_id);

  g_string_append_printf (state->graph,
                          "%*slayer%d [label=\"layer=0x%p\\n"
                          "ref count=%d\" "
                          "color=\"blue\"];\n",
                          state->indent, "",
                          layer_id,
                          layer,
                          COGL_OBJECT (layer)->ref_count);

  changes_label = g_string_new ("");
  g_string_append_printf (changes_label,
                          "%*slayer%d -> layer_state%d [weight=100];\n"
                          "%*slayer_state%d [shape=box label=\"",
                          state->indent, "",
                          layer_id,
                          layer_id,
                          state->indent, "",
                          layer_id);

  if (layer->differences & COGL_MATERIAL_LAYER_STATE_TEXTURE)
    {
      changes = TRUE;
      g_string_append_printf (changes_label,
                              "\\ltexture=%p\\n",
                              layer->texture);
    }

  if (changes)
    {
      g_string_append_printf (changes_label, "\"];\n");
      g_string_append (state->graph, changes_label->str);
      g_string_free (changes_label, TRUE);
    }

  state_out.parent_id = layer_id;

  state_out.node_id_ptr = state->node_id_ptr;
  (*state_out.node_id_ptr)++;

  state_out.graph = state->graph;
  state_out.indent = state->indent + 2;

  _cogl_material_node_foreach_child (COGL_MATERIAL_NODE (layer),
                                     dump_layer_cb,
                                     &state_out);

  return TRUE;
}

static gboolean
dump_layer_ref_cb (CoglMaterialLayer *layer, void *data)
{
  PrintDebugState *state = data;
  int material_id = *state->node_id_ptr;

  g_string_append_printf (state->graph,
                          "%*smaterial_state%d -> layer_ref%d [weight=200];\n",
                          state->indent, "",
                          material_id,
                          material_id);
  g_string_append_printf (state->graph,
                          "%*slayer_ref%d [label=\"addr=0x%p\" "
                          "shape=box color=blue];\n",
                          state->indent, "",
                          material_id,
                          layer);

  return TRUE;
}

static gboolean
dump_material_cb (CoglMaterialNode *node, void *user_data)
{
  CoglMaterial *material = COGL_MATERIAL (node);
  PrintDebugState *state = user_data;
  int material_id = *state->node_id_ptr;
  PrintDebugState state_out;
  GString *changes_label;
  gboolean changes = FALSE;
  gboolean layers = FALSE;

  if (state->parent_id >= 0)
    g_string_append_printf (state->graph, "%*smaterial%d -> material%d;\n",
                            state->indent, "",
                            state->parent_id,
                            material_id);

  g_string_append_printf (state->graph,
                          "%*smaterial%d [label=\"material=0x%p\\n"
                          "ref count=%d\\n"
                          "breadcrumb=\\\"%s\\\"\" color=\"red\"];\n",
                          state->indent, "",
                          material_id,
                          material,
                          COGL_OBJECT (material)->ref_count,
                          material->has_static_breadcrumb ?
                          material->static_breadcrumb : "NULL");

  changes_label = g_string_new ("");
  g_string_append_printf (changes_label,
                          "%*smaterial%d -> material_state%d [weight=100];\n"
                          "%*smaterial_state%d [shape=box label=\"",
                          state->indent, "",
                          material_id,
                          material_id,
                          state->indent, "",
                          material_id);


  if (material->differences & COGL_MATERIAL_STATE_COLOR)
    {
      changes = TRUE;
      g_string_append_printf (changes_label,
                              "\\lcolor=0x%02X%02X%02X%02X\\n",
                              cogl_color_get_red_byte (&material->color),
                              cogl_color_get_green_byte (&material->color),
                              cogl_color_get_blue_byte (&material->color),
                              cogl_color_get_alpha_byte (&material->color));
    }

  if (material->differences & COGL_MATERIAL_STATE_BLEND)
    {
      changes = TRUE;
      const char *blend_enable_name;
      switch (material->blend_enable)
        {
        case COGL_MATERIAL_BLEND_ENABLE_AUTOMATIC:
          blend_enable_name = "AUTO";
          break;
        case COGL_MATERIAL_BLEND_ENABLE_ENABLED:
          blend_enable_name = "ENABLED";
          break;
        case COGL_MATERIAL_BLEND_ENABLE_DISABLED:
          blend_enable_name = "DISABLED";
          break;
        default:
          blend_enable_name = "UNKNOWN";
        }
      g_string_append_printf (changes_label,
                              "\\lblend=%s\\n",
                              blend_enable_name);
    }

  if (material->differences & COGL_MATERIAL_STATE_LAYERS)
    {
      changes = TRUE;
      layers = TRUE;
      g_string_append_printf (changes_label, "\\ln_layers=%d\\n",
                              material->n_layers);
    }

  if (changes)
    {
      g_string_append_printf (changes_label, "\"];\n");
      g_string_append (state->graph, changes_label->str);
      g_string_free (changes_label, TRUE);
    }

  if (layers)
    _cogl_material_foreach_layer (material, dump_layer_ref_cb, state);

  state_out.parent_id = material_id;

  state_out.node_id_ptr = state->node_id_ptr;
  (*state_out.node_id_ptr)++;

  state_out.graph = state->graph;
  state_out.indent = state->indent + 2;

  _cogl_material_node_foreach_child (COGL_MATERIAL_NODE (material),
                                     dump_material_cb,
                                     &state_out);

  return TRUE;
}

void
_cogl_debug_dump_materials_dot_file (const char *filename)
{
  GString *graph;
  PrintDebugState layer_state;
  PrintDebugState material_state;
  int layer_id = 0;
  int material_id = 0;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!ctx->default_material)
    return;

  graph = g_string_new ("");
  g_string_append_printf (graph, "digraph {\n");

  layer_state.graph = graph;
  layer_state.parent_id = -1;
  layer_state.node_id_ptr = &layer_id;
  layer_state.indent = 0;
  dump_layer_cb (ctx->default_layer_0, &layer_state);

  material_state.graph = graph;
  material_state.parent_id = -1;
  material_state.node_id_ptr = &material_id;
  material_state.indent = 0;
  dump_material_cb (ctx->default_material, &material_state);

  g_string_append_printf (graph, "}\n");

  if (filename)
    g_file_set_contents (filename, graph->str, -1, NULL);
  else
    g_print ("%s", graph->str);

  g_string_free (graph, TRUE);
}

