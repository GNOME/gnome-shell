/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-debug.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-journal-private.h"
#include "cogl-texture-private.h"
#include "cogl-material-private.h"
#include "cogl-material-opengl-private.h"
#include "cogl-vertex-buffer-private.h"
#include "cogl-framebuffer-private.h"

#include <string.h>
#include <math.h>

#define _COGL_MAX_BEZ_RECURSE_DEPTH 16

#ifdef HAVE_COGL_GL
#define glClientActiveTexture ctx->drv.pf_glClientActiveTexture
#endif


typedef struct _TextureSlicedQuadState
{
  CoglHandle material;
  float tex_virtual_origin_x;
  float tex_virtual_origin_y;
  float quad_origin_x;
  float quad_origin_y;
  float v_to_q_scale_x;
  float v_to_q_scale_y;
  float quad_len_x;
  float quad_len_y;
  gboolean flipped_x;
  gboolean flipped_y;
  CoglMaterialWrapModeOverrides *wrap_mode_overrides;
} TextureSlicedQuadState;

typedef struct _TextureSlicedPolygonState
{
  const CoglTextureVertex *vertices;
  int n_vertices;
  int stride;
} TextureSlicedPolygonState;

static void
log_quad_sub_textures_cb (CoglHandle texture_handle,
                          GLuint gl_handle,
                          GLenum gl_target,
                          const float *subtexture_coords,
                          const float *virtual_coords,
                          void *user_data)
{
  TextureSlicedQuadState *state = user_data;
  float quad_coords[4];

#define TEX_VIRTUAL_TO_QUAD(V, Q, AXIS) \
    do { \
	Q = V - state->tex_virtual_origin_##AXIS; \
	Q *= state->v_to_q_scale_##AXIS; \
	if (state->flipped_##AXIS) \
	    Q = state->quad_len_##AXIS - Q; \
	Q += state->quad_origin_##AXIS; \
    } while (0);

  TEX_VIRTUAL_TO_QUAD (virtual_coords[0], quad_coords[0], x);
  TEX_VIRTUAL_TO_QUAD (virtual_coords[1], quad_coords[1], y);

  TEX_VIRTUAL_TO_QUAD (virtual_coords[2], quad_coords[2], x);
  TEX_VIRTUAL_TO_QUAD (virtual_coords[3], quad_coords[3], y);

#undef TEX_VIRTUAL_TO_QUAD

  COGL_NOTE (DRAW,
             "~~~~~ slice\n"
             "qx1: %f\t"
             "qy1: %f\n"
             "qx2: %f\t"
             "qy2: %f\n"
             "tx1: %f\t"
             "ty1: %f\n"
             "tx2: %f\t"
             "ty2: %f\n",
             quad_coords[0], quad_coords[1],
             quad_coords[2], quad_coords[3],
             subtexture_coords[0], subtexture_coords[1],
             subtexture_coords[2], subtexture_coords[3]);

  /* FIXME: when the wrap mode becomes part of the material we need to
   * be able to override the wrap mode when logging a quad. */
  _cogl_journal_log_quad (quad_coords,
                          state->material,
                          1, /* one layer */
                          0, /* don't need to use fallbacks */
                          gl_handle, /* replace the layer0 texture */
                          state->wrap_mode_overrides, /* use GL_CLAMP_TO_EDGE */
                          subtexture_coords,
                          4);
}

/* This path doesn't currently support multitexturing but is used for
 * CoglTextures that don't support repeating using the GPU so we need to
 * manually emit extra geometry to fake the repeating. This includes:
 *
 * - CoglTexture2DSliced: when made of > 1 slice or if the users given
 *   texture coordinates require repeating,
 * - CoglTexture2DAtlas: if the users given texture coordinates require
 *   repeating,
 * - CoglTextureRectangle: if the users given texture coordinates require
 *   repeating,
 * - CoglTexturePixmap: if the users given texture coordinates require
 *   repeating
 */
/* TODO: support multitexturing */
static void
_cogl_texture_quad_multiple_primitives (CoglHandle   tex_handle,
                                        CoglHandle   material,
                                        gboolean     clamp_s,
                                        gboolean     clamp_t,
                                        const float *position,
                                        float        tx_1,
                                        float        ty_1,
                                        float        tx_2,
                                        float        ty_2)
{
  TextureSlicedQuadState state;
  CoglMaterialWrapModeOverrides wrap_mode_overrides;
  gboolean tex_virtual_flipped_x;
  gboolean tex_virtual_flipped_y;
  gboolean quad_flipped_x;
  gboolean quad_flipped_y;
  CoglHandle first_layer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* If the wrap mode is clamp to edge then we'll recursively draw the
     stretched part and replace the coordinates */
  if (clamp_s && tx_1 != tx_2)
    {
      float *replacement_position = g_newa (float, 4);
      float old_tx_1 = tx_1, old_tx_2 = tx_2;

      memcpy (replacement_position, position, sizeof (float) * 4);

      tx_1 = CLAMP (tx_1, 0.0f, 1.0f);
      tx_2 = CLAMP (tx_2, 0.0f, 1.0f);

      if (old_tx_1 != tx_1)
        {
          /* Draw the left part of the quad as a stretched copy of tx_1 */
          float tmp_position[] =
            { position[0], position[1],
              (position[0] +
               (position[2] - position[0]) *
               (tx_1 - old_tx_1) / (old_tx_2 - old_tx_1)),
              position[3] };
          _cogl_texture_quad_multiple_primitives (tex_handle, material,
                                                  FALSE, clamp_t,
                                                  tmp_position,
                                                  tx_1, ty_1, tx_1, ty_2);
          replacement_position[0] = tmp_position[2];
        }

      if (old_tx_2 != tx_2)
        {
          /* Draw the right part of the quad as a stretched copy of tx_2 */
          float tmp_position[] =
            { (position[0] +
               (position[2] - position[0]) *
               (tx_2 - old_tx_1) / (old_tx_2 - old_tx_1)),
              position[1], position[2], position[3] };
          _cogl_texture_quad_multiple_primitives (tex_handle, material,
                                                  FALSE, clamp_t,
                                                  tmp_position,
                                                  tx_2, ty_1, tx_2, ty_2);
          replacement_position[2] = tmp_position[0];
        }

      /* If there's no main part left then we don't need to continue */
      if (tx_1 == tx_2)
        return;

      position = replacement_position;
    }

  if (clamp_t && ty_1 != ty_2)
    {
      float *replacement_position = g_newa (float, 4);
      float old_ty_1 = ty_1, old_ty_2 = ty_2;

      memcpy (replacement_position, position, sizeof (float) * 4);

      ty_1 = CLAMP (ty_1, 0.0f, 1.0f);
      ty_2 = CLAMP (ty_2, 0.0f, 1.0f);

      if (old_ty_1 != ty_1)
        {
          /* Draw the top part of the quad as a stretched copy of ty_1 */
          float tmp_position[] =
            { position[0], position[1], position[2],
              (position[1] +
               (position[3] - position[1]) *
               (ty_1 - old_ty_1) / (old_ty_2 - old_ty_1)) };
          _cogl_texture_quad_multiple_primitives (tex_handle, material,
                                                  clamp_s, FALSE,
                                                  tmp_position,
                                                  tx_1, ty_1, tx_2, ty_1);
          replacement_position[1] = tmp_position[3];
        }

      if (old_ty_2 != ty_2)
        {
          /* Draw the bottom part of the quad as a stretched copy of ty_2 */
          float tmp_position[] =
            { position[0],
              (position[1] +
               (position[3] - position[1]) *
               (ty_2 - old_ty_1) / (old_ty_2 - old_ty_1)),
              position[2], position[3] };
          _cogl_texture_quad_multiple_primitives (tex_handle, material,
                                                  clamp_s, FALSE,
                                                  tmp_position,
                                                  tx_1, ty_2, tx_2, ty_2);
          replacement_position[3] = tmp_position[1];
        }

      /* If there's no main part left then we don't need to continue */
      if (ty_1 == ty_2)
        return;

      position = replacement_position;
    }

  state.wrap_mode_overrides = NULL;
  memset (&wrap_mode_overrides, 0, sizeof (wrap_mode_overrides));

  /* We can't use hardware repeat so we need to set clamp to edge
     otherwise it might pull in edge pixels from the other side. By
     default WRAP_MODE_AUTOMATIC becomes CLAMP_TO_EDGE so we only need
     to override if the wrap mode is repeat */
  first_layer = cogl_material_get_layers (material)->data;
  if (cogl_material_layer_get_wrap_mode_s (first_layer) ==
      COGL_MATERIAL_WRAP_MODE_REPEAT)
    {
      state.wrap_mode_overrides = &wrap_mode_overrides;
      wrap_mode_overrides.values[0].s =
        COGL_MATERIAL_WRAP_MODE_OVERRIDE_CLAMP_TO_EDGE;
    }
  if (cogl_material_layer_get_wrap_mode_t (first_layer) ==
      COGL_MATERIAL_WRAP_MODE_REPEAT)
    {
      state.wrap_mode_overrides = &wrap_mode_overrides;
      wrap_mode_overrides.values[0].t =
        COGL_MATERIAL_WRAP_MODE_OVERRIDE_CLAMP_TO_EDGE;
    }

  state.material = material;

  /* Get together the data we need to transform the virtual texture
   * coordinates of each slice into quad coordinates...
   *
   * NB: We need to consider that the quad coordinates and the texture
   * coordinates may be inverted along the x or y axis, and must preserve the
   * inversions when we emit the final geometry.
   */

#define X0 0
#define Y0 1
#define X1 2
#define Y1 3

  tex_virtual_flipped_x = (tx_1 > tx_2) ? TRUE : FALSE;
  tex_virtual_flipped_y = (ty_1 > ty_2) ? TRUE : FALSE;
  state.tex_virtual_origin_x = tex_virtual_flipped_x ? tx_2 : tx_1;
  state.tex_virtual_origin_y = tex_virtual_flipped_y ? ty_2 : ty_1;

  quad_flipped_x = (position[X0] > position[X1]) ? TRUE : FALSE;
  quad_flipped_y = (position[Y0] > position[Y1]) ? TRUE : FALSE;
  state.quad_origin_x = quad_flipped_x ? position[X1] : position[X0];
  state.quad_origin_y = quad_flipped_y ? position[Y1] : position[Y0];

  /* flatten the two forms of coordinate inversion into one... */
  state.flipped_x = tex_virtual_flipped_x ^ quad_flipped_x;
  state.flipped_y = tex_virtual_flipped_y ^ quad_flipped_y;

  /* We use the _len_AXIS naming here instead of _width and _height because
   * log_quad_slice_cb uses a macro with symbol concatenation to handle both
   * axis, so this is more convenient... */
  state.quad_len_x = fabs (position[X1] - position[X0]);
  state.quad_len_y = fabs (position[Y1] - position[Y0]);

#undef X0
#undef Y0
#undef X1
#undef Y1

  state.v_to_q_scale_x = fabs (state.quad_len_x / (tx_2 - tx_1));
  state.v_to_q_scale_y = fabs (state.quad_len_y / (ty_2 - ty_1));

  _cogl_texture_foreach_sub_texture_in_region (tex_handle,
                                               tx_1, ty_1, tx_2, ty_2,
                                               log_quad_sub_textures_cb,
                                               &state);
}

/* This path supports multitexturing but only when each of the layers is
 * handled with a single GL texture. Also if repeating is necessary then
 * _cogl_texture_can_hardware_repeat() must return TRUE.
 * This includes layers made from:
 *
 * - CoglTexture2DSliced: if only comprised of a single slice with optional
 *   waste, assuming the users given texture coordinates don't require
 *   repeating.
 * - CoglTexture{1D,2D,3D}: always.
 * - CoglTexture2DAtlas: assuming the users given texture coordinates don't
 *   require repeating.
 * - CoglTextureRectangle: assuming the users given texture coordinates don't
 *   require repeating.
 * - CoglTexturePixmap: assuming the users given texture coordinates don't
 *   require repeating.
 */
static gboolean
_cogl_multitexture_quad_single_primitive (const float *position,
                                          CoglHandle   material,
                                          guint32      fallback_layers,
                                          const float *user_tex_coords,
                                          int          user_tex_coords_len)
{
  int          n_layers = cogl_material_get_n_layers (material);
  float       *final_tex_coords = alloca (sizeof (float) * 4 * n_layers);
  const GList *layers;
  GList       *tmp;
  int          i;
  CoglMaterialWrapModeOverrides wrap_mode_overrides;
  /* This will be set to point to wrap_mode_overrides when an override
     is needed */
  CoglMaterialWrapModeOverrides *wrap_mode_overrides_p = NULL;

  _COGL_GET_CONTEXT (ctx, FALSE);

  memset (&wrap_mode_overrides, 0, sizeof (wrap_mode_overrides));

  /*
   * Validate the texture coordinates for this rectangle.
   */
  layers = cogl_material_get_layers (material);
  for (tmp = (GList *)layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle          layer = (CoglHandle)tmp->data;
      CoglHandle          tex_handle;
      const float        *in_tex_coords;
      float              *out_tex_coords;
      float               default_tex_coords[4] = {0.0, 0.0, 1.0, 1.0};
      CoglTransformResult transform_result;

      tex_handle = cogl_material_layer_get_texture (layer);

      /* COGL_INVALID_HANDLE textures are handled by
       * _cogl_material_flush_gl_state */
      if (tex_handle == COGL_INVALID_HANDLE)
        continue;

      /* If the user didn't supply texture coordinates for this layer
         then use the default coords */
      if (i >= user_tex_coords_len / 4)
        in_tex_coords = default_tex_coords;
      else
        in_tex_coords = &user_tex_coords[i * 4];

      out_tex_coords = &final_tex_coords[i * 4];

      memcpy (out_tex_coords, in_tex_coords, sizeof (GLfloat) * 4);

      /* Convert the texture coordinates to GL.
       */
      transform_result =
        _cogl_texture_transform_quad_coords_to_gl (tex_handle,
                                                   out_tex_coords);
      /* If the texture has waste or we are using GL_TEXTURE_RECT we
       * can't handle texture repeating so we can't use the layer if
       * repeating is required.
       *
       * NB: We already know that no texture matrix is being used if the
       * texture doesn't support hardware repeat.
       */
      if (transform_result == COGL_TRANSFORM_SOFTWARE_REPEAT)
        {
          if (i == 0)
            {
              if (n_layers > 1)
                {
                  static gboolean warning_seen = FALSE;
                  if (!warning_seen)
                    g_warning ("Skipping layers 1..n of your material since "
                               "the first layer doesn't support hardware "
                               "repeat (e.g. because of waste or use of "
                               "GL_TEXTURE_RECTANGLE_ARB) and you supplied "
                               "texture coordinates outside the range [0,1]."
                               "Falling back to software repeat assuming "
                               "layer 0 is the most important one keep");
                  warning_seen = TRUE;
                }
              return FALSE;
            }
          else
            {
              static gboolean warning_seen = FALSE;
              if (!warning_seen)
                g_warning ("Skipping layer %d of your material "
                           "since you have supplied texture coords "
                           "outside the range [0,1] but the texture "
                           "doesn't support hardware repeat (e.g. "
                           "because of waste or use of "
                           "GL_TEXTURE_RECTANGLE_ARB). This isn't "
                           "supported with multi-texturing.", i);
              warning_seen = TRUE;

              /* NB: marking for fallback will replace the layer with
               * a default transparent texture */
              fallback_layers |= (1 << i);
            }
        }

      /* By default WRAP_MODE_AUTOMATIC becomes to CLAMP_TO_EDGE. If
         the texture coordinates need repeating then we'll override
         this to GL_REPEAT. Otherwise we'll leave it at CLAMP_TO_EDGE
         so that it won't blend in pixels from the opposite side when
         the full texture is drawn with GL_LINEAR filter mode */
      if (transform_result == COGL_TRANSFORM_HARDWARE_REPEAT)
        {
          if (cogl_material_layer_get_wrap_mode_s (layer) ==
              COGL_MATERIAL_WRAP_MODE_AUTOMATIC)
            {
              wrap_mode_overrides.values[i].s
                = COGL_MATERIAL_WRAP_MODE_OVERRIDE_REPEAT;
              wrap_mode_overrides_p = &wrap_mode_overrides;
            }
          if (cogl_material_layer_get_wrap_mode_t (layer) ==
              COGL_MATERIAL_WRAP_MODE_AUTOMATIC)
            {
              wrap_mode_overrides.values[i].t
                = COGL_MATERIAL_WRAP_MODE_OVERRIDE_REPEAT;
              wrap_mode_overrides_p = &wrap_mode_overrides;
            }
        }
    }

  _cogl_journal_log_quad (position,
                          material,
                          n_layers,
                          fallback_layers,
                          0, /* don't replace the layer0 texture */
                          wrap_mode_overrides_p,
                          final_tex_coords,
                          n_layers * 4);

  return TRUE;
}

struct _CoglMutiTexturedRect
{
  const float *position; /* x0,y0,x1,y1 */
  const float *tex_coords; /* (tx0,ty0,tx1,ty1)(tx0,ty0,tx1,ty1)(... */
  int          tex_coords_len; /* number of floats in tex_coords? */
};

static void
_cogl_rectangles_with_multitexture_coords (
                                        struct _CoglMutiTexturedRect *rects,
                                        int                           n_rects)
{
  CoglHandle	 material;
  const GList	*layers;
  int		 n_layers;
  const GList	*tmp;
  guint32        fallback_layers = 0;
  gboolean	 all_use_sliced_quad_fallback = FALSE;
  int		 i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  material = ctx->source_material;

  layers = cogl_material_get_layers (material);
  n_layers = cogl_material_get_n_layers (material);

  /*
   * Validate all the layers of the current source material...
   */

  for (tmp = layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle     layer = tmp->data;
      CoglHandle     tex_handle;

      if (cogl_material_layer_get_type (layer)
	  != COGL_MATERIAL_LAYER_TYPE_TEXTURE)
	continue;

      /* We need to ensure the mipmaps are ready before deciding
       * anything else about the texture because the texture storage
       * could completely change if it needs to be migrated out of the
       * atlas and will affect how we validate the layer.
       *
       * FIXME: this needs to be generalized. There could be any
       * number of things that might require a shuffling of the
       * underlying texture storage. We could add two mechanisms to
       * generalize this a bit...
       *
       * 1) add a _cogl_material_layer_update_storage() function that
       * would for instance consider if mipmapping is necessary and
       * potentially migrate the texture from an atlas.
       *
       * 2) allow setting of transient primitive-flags on a material
       * that may affect the outcome of _update_storage(). One flag
       * could indicate that we expect to sample beyond the bounds of
       * the texture border.
       *
       *   flags = COGL_MATERIAL_PRIMITIVE_FLAG_VALID_BORDERS;
       *   _cogl_material_layer_assert_primitive_flags (layer, flags)
       *   _cogl_material_layer_update_storage (layer)
       *   enqueue primitive in journal
       *
       *   when the primitive is dequeued and drawn we should:
       *   _cogl_material_flush_gl_state (material)
       *   draw primitive
       *   _cogl_material_unassert_primitive_flags (layer, flags);
       *
       * _cogl_material_layer_update_storage should take into
       * consideration all the asserted primitive requirements.  (E.g.
       * there could be multiple primitives in the journal - or in a
       * renderlist in the future - that need mipmaps or that need
       * valid contents beyond their borders (for cogl_polygon)
       * meaning they can't work with textures in an atas, so
       * _cogl_material_layer_update_storage would pass on these
       * requirements to the texture atlas backend which would make
       * sure the referenced texture is migrated out of the atlas and
       * mipmaps are generated.)
       */
      _cogl_material_layer_pre_paint (layer);

      tex_handle = cogl_material_layer_get_texture (layer);

      /* COGL_INVALID_HANDLE textures are handled by
       * _cogl_material_flush_gl_state */
      if (tex_handle == COGL_INVALID_HANDLE)
        continue;

      /* XXX:
       * For now, if the first layer is sliced then all other layers are
       * ignored since we currently don't support multi-texturing with
       * sliced textures. If the first layer is not sliced then any other
       * layers found to be sliced will be skipped. (with a warning)
       *
       * TODO: Add support for multi-texturing rectangles with sliced
       * textures if no texture matrices are in use.
       */
      if (cogl_texture_is_sliced (tex_handle))
	{
	  if (i == 0)
	    {
              fallback_layers = ~1; /* fallback all except the first layer */
	      all_use_sliced_quad_fallback = TRUE;
              if (tmp->next)
                {
                  static gboolean warning_seen = FALSE;
                  if (!warning_seen)
                    g_warning ("Skipping layers 1..n of your material since "
                               "the first layer is sliced. We don't currently "
                               "support any multi-texturing with sliced "
                               "textures but assume layer 0 is the most "
                               "important to keep");
                  warning_seen = TRUE;
                }
	      break;
	    }
          else
            {
              static gboolean warning_seen = FALSE;
              if (!warning_seen)
                g_warning ("Skipping layer %d of your material consisting of "
                           "a sliced texture (unsuported for multi texturing)",
                           i);
              warning_seen = TRUE;

              /* NB: marking for fallback will replace the layer with
               * a default transparent texture */
              fallback_layers |= (1 << i);
	      continue;
            }
	}

      /* If the texture can't be repeated with the GPU (e.g. because it has
       * waste or if using GL_TEXTURE_RECTANGLE_ARB) then we don't support
       * multi texturing since we don't know if the result will end up trying
       * to texture from the waste area. */
      if (_cogl_material_layer_has_user_matrix (layer)
          && !_cogl_texture_can_hardware_repeat (tex_handle))
        {
          static gboolean warning_seen = FALSE;
          if (!warning_seen)
            g_warning ("Skipping layer %d of your material since a custom "
                       "texture matrix was given for a texture that can't be "
                       "repeated using the GPU and the result may try to "
                       "sample beyond the bounds of the texture ",
                       i);
          warning_seen = TRUE;

          /* NB: marking for fallback will replace the layer with
           * a default transparent texture */
          fallback_layers |= (1 << i);
          continue;
        }
    }

  /*
   * Emit geometry for each of the rectangles...
   */

  for (i = 0; i < n_rects; i++)
    {
      CoglHandle   first_layer, tex_handle;
      const float  default_tex_coords[4] = {0.0, 0.0, 1.0, 1.0};
      const float *tex_coords;
      gboolean     clamp_s, clamp_t;

      if (!all_use_sliced_quad_fallback)
        {
          gboolean success =
            _cogl_multitexture_quad_single_primitive (rects[i].position,
                                                      material,
                                                      fallback_layers,
                                                      rects[i].tex_coords,
                                                      rects[i].tex_coords_len);

          /* NB: If _cogl_multitexture_quad_single_primitive fails then it
           * means the user tried to use texture repeat with a texture that
           * can't be repeated by the GPU (e.g. due to waste or use of
           * GL_TEXTURE_RECTANGLE_ARB) */
          if (success)
            continue;
        }

      /* If multitexturing failed or we are drawing with a sliced texture
       * then we only support a single layer so we pluck out the texture
       * from the first material layer... */
      layers = cogl_material_get_layers (material);
      first_layer = layers->data;
      tex_handle = cogl_material_layer_get_texture (first_layer);

      if (rects[i].tex_coords)
        tex_coords = rects[i].tex_coords;
      else
        tex_coords = default_tex_coords;

      clamp_s = (cogl_material_layer_get_wrap_mode_s (first_layer) ==
                 COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE);
      clamp_t = (cogl_material_layer_get_wrap_mode_t (first_layer) ==
                 COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE);

      COGL_NOTE (DRAW, "Drawing Tex Quad (Multi-Prim Mode)");

      _cogl_texture_quad_multiple_primitives (tex_handle,
                                              material,
                                              clamp_s, clamp_t,
                                              rects[i].position,
                                              tex_coords[0],
                                              tex_coords[1],
                                              tex_coords[2],
                                              tex_coords[3]);
    }

#if 0
  /* XXX: The current journal doesn't handle changes to the model view matrix
   * so for now we force a flush at the end of every primitive. */
  _cogl_journal_flush ();
#endif
}

void
cogl_rectangles (const float *verts,
                 unsigned int n_rects)
{
  struct _CoglMutiTexturedRect *rects;
  int i;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * _CoglMutiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rects = g_alloca (n_rects * sizeof (struct _CoglMutiTexturedRect));

  for (i = 0; i < n_rects; i++)
    {
      rects[i].position = &verts[i * 4];
      rects[i].tex_coords = NULL;
      rects[i].tex_coords_len = 0;
    }

  _cogl_rectangles_with_multitexture_coords (rects, n_rects);
}

void
cogl_rectangles_with_texture_coords (const float *verts,
                                     unsigned int n_rects)
{
  struct _CoglMutiTexturedRect *rects;
  int i;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * _CoglMutiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rects = g_alloca (n_rects * sizeof (struct _CoglMutiTexturedRect));

  for (i = 0; i < n_rects; i++)
    {
      rects[i].position = &verts[i * 8];
      rects[i].tex_coords = &verts[i * 8 + 4];
      rects[i].tex_coords_len = 4;
    }

  _cogl_rectangles_with_multitexture_coords (rects, n_rects);
}

void
cogl_rectangle_with_texture_coords (float x_1,
			            float y_1,
			            float x_2,
			            float y_2,
			            float tx_1,
			            float ty_1,
			            float tx_2,
			            float ty_2)
{
  const float position[4] = {x_1, y_1, x_2, y_2};
  const float tex_coords[4] = {tx_1, ty_1, tx_2, ty_2};
  struct _CoglMutiTexturedRect rect;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * _CoglMutiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rect.position = position;
  rect.tex_coords = tex_coords;
  rect.tex_coords_len = 4;

  _cogl_rectangles_with_multitexture_coords (&rect, 1);
}

void
cogl_rectangle_with_multitexture_coords (float        x_1,
			                 float        y_1,
			                 float        x_2,
			                 float        y_2,
			                 const float *user_tex_coords,
                                         int          user_tex_coords_len)
{
  const float position[4] = {x_1, y_1, x_2, y_2};
  struct _CoglMutiTexturedRect rect;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * _CoglMutiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rect.position = position;
  rect.tex_coords = user_tex_coords;
  rect.tex_coords_len = user_tex_coords_len;

  _cogl_rectangles_with_multitexture_coords (&rect, 1);
}

void
cogl_rectangle (float x_1,
                float y_1,
                float x_2,
                float y_2)
{
  const float position[4] = {x_1, y_1, x_2, y_2};
  struct _CoglMutiTexturedRect rect;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * _CoglMutiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rect.position = position;
  rect.tex_coords = NULL;
  rect.tex_coords_len = 0;

  _cogl_rectangles_with_multitexture_coords (&rect, 1);
}

void
draw_polygon_sub_texture_cb (CoglHandle tex_handle,
                             GLuint gl_handle,
                             GLenum gl_target,
                             const float *subtexture_coords,
                             const float *virtual_coords,
                             void *user_data)
{
  TextureSlicedPolygonState *state = user_data;
  GLfloat *v;
  int i;
  CoglMaterialFlushOptions options;
  float slice_origin_x;
  float slice_origin_y;
  float virtual_origin_x;
  float virtual_origin_y;
  float v_to_s_scale_x;
  float v_to_s_scale_y;
  CoglHandle source;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  slice_origin_x = subtexture_coords[0];
  slice_origin_y = subtexture_coords[1];
  virtual_origin_x = virtual_coords[0];
  virtual_origin_y = virtual_coords[1];
  v_to_s_scale_x = ((virtual_coords[2] - virtual_coords[0]) /
                    (subtexture_coords[2] - subtexture_coords[0]));
  v_to_s_scale_y = ((virtual_coords[3] - virtual_coords[1]) /
                    (subtexture_coords[3] - subtexture_coords[1]));

  /* Convert the vertices into an array of GLfloats ready to pass to
   * OpenGL */
  v = (GLfloat *)ctx->logged_vertices->data;
  for (i = 0; i < state->n_vertices; i++)
    {
      /* NB: layout = [X,Y,Z,TX,TY,R,G,B,A,...] */
      GLfloat *t = v + 3;

      t[0] = ((state->vertices[i].tx - virtual_origin_x) * v_to_s_scale_x
              + slice_origin_x);
      t[1] = ((state->vertices[i].ty - virtual_origin_y) * v_to_s_scale_y
              + slice_origin_y);

      v += state->stride;
    }

  if (G_UNLIKELY (ctx->legacy_state_set))
    {
      source = cogl_material_copy (ctx->source_material);
      _cogl_material_apply_legacy_state (source);
    }
  else
    source = ctx->source_material;

  options.flags =
    COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE |
    COGL_MATERIAL_FLUSH_WRAP_MODE_OVERRIDES;

  options.layer0_override_texture = gl_handle;

  /* Override the wrapping mode on all of the slices to use a
     transparent border so that we can draw the full polygon for
     each slice. Coordinates outside the texture will be transparent
     so only the part of the polygon that intersects the slice will
     be visible. This is a fairly hacky fallback and it relies on
     the blending function working correctly */

  memset (&options.wrap_mode_overrides, 0,
          sizeof (options.wrap_mode_overrides));
  options.wrap_mode_overrides.values[0].s =
    COGL_MATERIAL_WRAP_MODE_OVERRIDE_CLAMP_TO_BORDER;
  options.wrap_mode_overrides.values[0].t =
    COGL_MATERIAL_WRAP_MODE_OVERRIDE_CLAMP_TO_BORDER;

  if (cogl_material_get_n_layers (source) != 1)
    {
      /* disable all except the first layer */
      options.disable_layers = (guint32)~1;
      options.flags |= COGL_MATERIAL_FLUSH_DISABLE_MASK;
    }

  /* If we haven't already created a derived material... */
  if (source == ctx->source_material)
    source = cogl_material_copy (ctx->source_material);
  _cogl_material_apply_overrides (source, &options);

  _cogl_material_flush_gl_state (source, FALSE);

  GE (glDrawArrays (GL_TRIANGLE_FAN, 0, state->n_vertices));

  if (G_UNLIKELY (source != ctx->source_material))
    cogl_handle_unref (source);
}

/* handles 2d-sliced textures with > 1 slice */
static void
_cogl_texture_polygon_multiple_primitives (const CoglTextureVertex *vertices,
                                           unsigned int n_vertices,
                                           unsigned int stride,
                                           gboolean use_color)
{
  const GList               *layers;
  CoglHandle                 layer0;
  CoglHandle                 tex_handle;
  GLfloat                   *v;
  int                        i;
  TextureSlicedPolygonState  state;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We can assume in this case that we have at least one layer in the
   * material that corresponds to a sliced cogl texture */
  layers = cogl_material_get_layers (ctx->source_material);
  layer0 = (CoglHandle)layers->data;
  tex_handle = cogl_material_layer_get_texture (layer0);

  v = (GLfloat *)ctx->logged_vertices->data;
  for (i = 0; i < n_vertices; i++)
    {
      guint8 *c;

      v[0] = vertices[i].x;
      v[1] = vertices[i].y;
      v[2] = vertices[i].z;

      if (use_color)
        {
          /* NB: [X,Y,Z,TX,TY,R,G,B,A,...] */
          c = (guint8 *) (v + 5);
          c[0] = cogl_color_get_red_byte (&vertices[i].color);
          c[1] = cogl_color_get_green_byte (&vertices[i].color);
          c[2] = cogl_color_get_blue_byte (&vertices[i].color);
          c[3] = cogl_color_get_alpha_byte (&vertices[i].color);
        }

      v += stride;
    }

  state.stride = stride;
  state.vertices = vertices;
  state.n_vertices = n_vertices;

  _cogl_texture_foreach_sub_texture_in_region (tex_handle,
                                               0, 0, 1, 1,
                                               draw_polygon_sub_texture_cb,
                                               &state);
}

static void
_cogl_multitexture_polygon_single_primitive (const CoglTextureVertex *vertices,
                                             unsigned int n_vertices,
                                             unsigned int n_layers,
                                             unsigned int stride,
                                             gboolean use_color,
                                             guint32 fallback_layers,
                                             CoglMaterialWrapModeOverrides *
                                               wrap_mode_overrides)
{
  CoglHandle           material;
  const GList         *layers;
  int                  i;
  GList               *tmp;
  GLfloat             *v;
  CoglMaterialFlushOptions options;
  CoglHandle           source;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  material = ctx->source_material;
  layers = cogl_material_get_layers (material);

  /* Convert the vertices into an array of GLfloats ready to pass to
     OpenGL */
  for (v = (GLfloat *)ctx->logged_vertices->data, i = 0;
       i < n_vertices;
       v += stride, i++)
    {
      guint8 *c;
      int     j;

      /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
      v[0] = vertices[i].x;
      v[1] = vertices[i].y;
      v[2] = vertices[i].z;

      for (tmp = (GList *)layers, j = 0; tmp != NULL; tmp = tmp->next, j++)
        {
          CoglHandle   layer = (CoglHandle)tmp->data;
          CoglHandle   tex_handle;
          GLfloat     *t;
          float        tx, ty;

          tex_handle = cogl_material_layer_get_texture (layer);

          /* COGL_INVALID_HANDLE textures will be handled in
           * _cogl_material_flush_layers_gl_state but there is no need to worry
           * about scaling texture coordinates in this case */
          if (tex_handle == COGL_INVALID_HANDLE)
            continue;

          tx = vertices[i].tx;
          ty = vertices[i].ty;
          _cogl_texture_transform_coords_to_gl (tex_handle, &tx, &ty);

          /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
          t = v + 3 + 2 * j;
          t[0] = tx;
          t[1] = ty;
        }

      if (use_color)
        {
          /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
          c = (guint8 *) (v + 3 + 2 * n_layers);
          c[0] = cogl_color_get_red_byte (&vertices[i].color);
          c[1] = cogl_color_get_green_byte (&vertices[i].color);
          c[2] = cogl_color_get_blue_byte (&vertices[i].color);
          c[3] = cogl_color_get_alpha_byte (&vertices[i].color);
        }
    }

  if (G_UNLIKELY (ctx->legacy_state_set))
    {
      source = cogl_material_copy (ctx->source_material);
      _cogl_material_apply_legacy_state (source);
    }
  else
    source = ctx->source_material;

  options.flags = 0;

  if (G_UNLIKELY (fallback_layers))
    {
      options.flags |= COGL_MATERIAL_FLUSH_FALLBACK_MASK;
      options.fallback_layers = fallback_layers;
    }
  if (wrap_mode_overrides)
    {
      options.flags |= COGL_MATERIAL_FLUSH_WRAP_MODE_OVERRIDES;
      options.wrap_mode_overrides = *wrap_mode_overrides;
    }
  if (options.flags)
    {
      /* If we haven't already created a derived material... */
      if (source == ctx->source_material)
        source = cogl_material_copy (ctx->source_material);
      _cogl_material_apply_overrides (source, &options);
    }

  _cogl_material_flush_gl_state (source, use_color);

  GE (glDrawArrays (GL_TRIANGLE_FAN, 0, n_vertices));

  if (G_UNLIKELY (source != ctx->source_material))
    cogl_handle_unref (source);
}

void
cogl_polygon (const CoglTextureVertex *vertices,
              unsigned int             n_vertices,
	      gboolean                 use_color)
{
  CoglHandle           material;
  const GList         *layers, *tmp;
  int                  n_layers;
  gboolean	       use_sliced_polygon_fallback = FALSE;
  guint32              fallback_layers = 0;
  int                  i;
  unsigned long        enable_flags;
  unsigned int         stride;
  gsize                stride_bytes;
  GLfloat             *v;
  CoglMaterialWrapModeOverrides wrap_mode_overrides;
  CoglMaterialWrapModeOverrides *wrap_mode_overrides_p = NULL;
  CoglHandle           original_source_material = NULL;
  gboolean             overrode_material = FALSE;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_journal_flush ();

  /* NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the material state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_framebuffer_flush_state (_cogl_get_framebuffer (), 0);

  material = ctx->source_material;
  layers = cogl_material_get_layers (ctx->source_material);
  n_layers = g_list_length ((GList *)layers);

  memset (&wrap_mode_overrides, 0, sizeof (wrap_mode_overrides));

  for (tmp = layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle layer = tmp->data;
      CoglHandle tex_handle = cogl_material_layer_get_texture (layer);

      /* COGL_INVALID_HANDLE textures will be handled in
       * _cogl_material_flush_layers_gl_state */
      if (tex_handle == COGL_INVALID_HANDLE)
        continue;

      /* Give the texture a chance to know that we're rendering
       * non-quad shaped primitives. If the texture is in an atlas it
       * will be migrated
       *
       * FIXME: this needs to be generalized. There could be any
       * number of things that might require a shuffling of the
       * underlying texture storage.
       */
      _cogl_texture_ensure_non_quad_rendering (tex_handle);

      /* We need to ensure the mipmaps are ready before deciding
       * anything else about the texture because the texture storate
       * could completely change if it needs to be migrated out of the
       * atlas and will affect how we validate the layer.
       */
      _cogl_material_layer_pre_paint (layer);

      if (i == 0 && cogl_texture_is_sliced (tex_handle))
        {
#if defined (HAVE_COGL_GLES) || defined (HAVE_COGL_GLES2)
          {
            static gboolean warning_seen = FALSE;
            if (!warning_seen)
              g_warning ("cogl_polygon does not work for sliced textures "
                         "on GL ES");
            warning_seen = TRUE;
            return;
          }
#endif
          if (n_layers > 1)
            {
              static gboolean warning_seen = FALSE;
              if (!warning_seen)
                {
                  g_warning ("Disabling layers 1..n since multi-texturing with "
                             "cogl_polygon isn't supported when using sliced "
                             "textures\n");
                  warning_seen = TRUE;
                }
            }

          use_sliced_polygon_fallback = TRUE;
          n_layers = 1;

          if (cogl_material_layer_get_min_filter (layer) != GL_NEAREST
              || cogl_material_layer_get_mag_filter (layer) != GL_NEAREST)
            {
              static gboolean warning_seen = FALSE;
              if (!warning_seen)
                {
                  g_warning ("cogl_texture_polygon does not work for sliced textures "
                             "when the minification and magnification filters are not "
                             "COGL_MATERIAL_FILTER_NEAREST");
                  warning_seen = TRUE;
                }
              return;
            }

          break;
        }

      if (cogl_texture_is_sliced (tex_handle))
        {
          static gboolean warning_seen = FALSE;
          if (!warning_seen)
            g_warning ("Disabling layer %d of the current source material, "
                       "because texturing with the vertex buffer API is not "
                       "currently supported using sliced textures, or "
                       "textures with waste\n", i);
          warning_seen = TRUE;

          fallback_layers |= (1 << i);
          continue;
        }

      /* By default COGL_MATERIAL_WRAP_MODE_AUTOMATIC becomes
         GL_CLAMP_TO_EDGE but we want the polygon API to use GL_REPEAT
         to maintain compatibility with previous releases */
      if (cogl_material_layer_get_wrap_mode_s (layer) ==
          COGL_MATERIAL_WRAP_MODE_AUTOMATIC)
        {
          wrap_mode_overrides.values[i].s =
            COGL_MATERIAL_WRAP_MODE_OVERRIDE_REPEAT;
          wrap_mode_overrides_p = &wrap_mode_overrides;
        }
      if (cogl_material_layer_get_wrap_mode_t (layer) ==
          COGL_MATERIAL_WRAP_MODE_AUTOMATIC)
        {
          wrap_mode_overrides.values[i].t =
            COGL_MATERIAL_WRAP_MODE_OVERRIDE_REPEAT;
          wrap_mode_overrides_p = &wrap_mode_overrides;
        }
    }

  /* Our data is arranged like:
   * [X, Y, Z, TX0, TY0, TX1, TY1..., R, G, B, A,...] */
  stride = 3 + (2 * n_layers) + (use_color ? 1 : 0);
  stride_bytes = stride * sizeof (GLfloat);

  /* Make sure there is enough space in the global vertex
     array. This is used so we can render the polygon with a single
     call to OpenGL but still support any number of vertices */
  g_array_set_size (ctx->logged_vertices, n_vertices * stride);
  v = (GLfloat *)ctx->logged_vertices->data;

  /* Prepare GL state */
  enable_flags = COGL_ENABLE_VERTEX_ARRAY;

  if (ctx->enable_backface_culling)
    enable_flags |= COGL_ENABLE_BACKFACE_CULLING;

  if (use_color)
    {
      CoglHandle override;
      enable_flags |= COGL_ENABLE_COLOR_ARRAY;
      GE( glColorPointer (4, GL_UNSIGNED_BYTE,
                          stride_bytes,
                          /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
                          v + 3 + 2 * n_layers) );

      if (!_cogl_material_get_real_blend_enabled (ctx->source_material))
        {
          CoglMaterialBlendEnable blend_enabled =
            COGL_MATERIAL_BLEND_ENABLE_ENABLED;
          original_source_material = ctx->source_material;
          override = cogl_material_copy (original_source_material);
          _cogl_material_set_blend_enabled (override, blend_enabled);

          /* XXX: cogl_push_source () */
          overrode_material = TRUE;
          ctx->source_material = override;
        }
    }

  _cogl_enable (enable_flags);
  _cogl_flush_face_winding ();

  GE (glVertexPointer (3, GL_FLOAT, stride_bytes, v));

  for (i = 0; i < n_layers; i++)
    {
      GE (glClientActiveTexture (GL_TEXTURE0 + i));
      GE (glEnableClientState (GL_TEXTURE_COORD_ARRAY));
      GE (glTexCoordPointer (2, GL_FLOAT,
                             stride_bytes,
                             /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
                             v + 3 + 2 * i));
    }

  _cogl_bitmask_clear_all (&ctx->temp_bitmask);
  _cogl_bitmask_set_range (&ctx->temp_bitmask, n_layers, TRUE);
  _cogl_disable_other_texcoord_arrays (&ctx->temp_bitmask);

  if (use_sliced_polygon_fallback)
    _cogl_texture_polygon_multiple_primitives (vertices,
                                               n_vertices,
                                               stride,
                                               use_color);
  else
    _cogl_multitexture_polygon_single_primitive (vertices,
                                                 n_vertices,
                                                 n_layers,
                                                 stride,
                                                 use_color,
                                                 fallback_layers,
                                                 wrap_mode_overrides_p);

  /* XXX: cogl_pop_source () */
  if (overrode_material)
    {
      cogl_handle_unref (ctx->source_material);
      ctx->source_material = original_source_material;
      /* XXX: when we have weak materials then any override material
       * should get associated with the original material so we don't
       * create lots of one-shot materials! */
    }

  /* Reset the size of the logged vertex array because rendering
     rectangles expects it to start at 0 */
  g_array_set_size (ctx->logged_vertices, 0);
}

