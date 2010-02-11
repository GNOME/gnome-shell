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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-journal-private.h"
#include "cogl-texture-private.h"
#include "cogl-material-private.h"
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
                                        const float *position,
                                        float        tx_1,
                                        float        ty_1,
                                        float        tx_2,
                                        float        ty_2)
{
  TextureSlicedQuadState state;
  gboolean tex_virtual_flipped_x;
  gboolean tex_virtual_flipped_y;
  gboolean quad_flipped_x;
  gboolean quad_flipped_y;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_NOTE (DRAW, "Drawing Tex Quad (Multi-Prim Mode)");

  /* We can't use hardware repeat so we need to set clamp to edge
     otherwise it might pull in edge pixels from the other side */
  /* FIXME: wrap modes should be part of the material! */
  _cogl_texture_set_wrap_mode_parameter (tex_handle, GL_CLAMP_TO_EDGE);

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

  _COGL_GET_CONTEXT (ctx, FALSE);

  /*
   * Validate the texture coordinates for this rectangle.
   */
  layers = cogl_material_get_layers (material);
  for (tmp = (GList *)layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle         layer = (CoglHandle)tmp->data;
      CoglHandle         tex_handle;
      const float       *in_tex_coords;
      float             *out_tex_coords;
      float              default_tex_coords[4] = {0.0, 0.0, 1.0, 1.0};
      gboolean           need_repeat = FALSE;
      int               coord_num;
      GLenum             wrap_mode;

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

      /* Convert the texture coordinates to GL. We also work out
         whether any of the texture coordinates are outside the range
         [0.0,1.0]. We need to do this after calling
         transform_coords_to_gl in case the texture backend is munging
         the coordinates (such as in the sub texture backend). This
         should be safe to call because we know that the texture only
         has one slice. */
      if (!_cogl_texture_transform_quad_coords_to_gl (tex_handle,
                                                      out_tex_coords))
        /* If the backend can't support these coordinates then bail out */
        return FALSE;
      for (coord_num = 0; coord_num < 4; coord_num++)
        if (out_tex_coords[coord_num] < 0.0f ||
            out_tex_coords[coord_num] > 1.0f)
          need_repeat = TRUE;

      /* If the texture has waste or we are using GL_TEXTURE_RECT we
       * can't handle texture repeating so we can't use the layer if
       * repeating is required.
       *
       * NB: We already know that no texture matrix is being used if the
       * texture doesn't support hardware repeat.
       */
      if (!_cogl_texture_can_hardware_repeat (tex_handle) && need_repeat)
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

      /* If we're not repeating then we want to clamp the coords
         to the edge otherwise it can pull in edge pixels from the
         wrong side when scaled */
      if (need_repeat)
        wrap_mode = GL_REPEAT;
      else
        wrap_mode = GL_CLAMP_TO_EDGE;

      _cogl_texture_set_wrap_mode_parameter (tex_handle, wrap_mode);
    }

  _cogl_journal_log_quad (position,
                          material,
                          n_layers,
                          fallback_layers,
                          0, /* don't replace the layer0 texture */
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
      unsigned long  flags;

      if (cogl_material_layer_get_type (layer)
	  != COGL_MATERIAL_LAYER_TYPE_TEXTURE)
	continue;

      /* We need to ensure the mipmaps are ready before deciding
         anything else about the texture because it could become
         something completely different if it needs to be migrated out
         of the atlas */
      _cogl_material_layer_ensure_mipmaps (layer);

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
      flags = _cogl_material_layer_get_flags (layer);
      if (flags & COGL_MATERIAL_LAYER_FLAG_HAS_USER_MATRIX
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
      first_layer = layers->data;
      tex_handle = cogl_material_layer_get_texture (first_layer);

      if (rects[i].tex_coords)
        tex_coords = rects[i].tex_coords;
      else
        tex_coords = default_tex_coords;

      _cogl_texture_quad_multiple_primitives (tex_handle,
                                              material,
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

  options.flags =
    COGL_MATERIAL_FLUSH_DISABLE_MASK |
    COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE;
  /* disable all except the first layer */
  options.disable_layers = (guint32)~1;
  options.layer0_override_texture = gl_handle;

  _cogl_material_flush_gl_state (ctx->source_material, &options);

  GE (glDrawArrays (GL_TRIANGLE_FAN, 0, state->n_vertices));
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
                                             guint32 fallback_layers)
{
  CoglHandle           material;
  const GList         *layers;
  int                  i;
  GList               *tmp;
  GLfloat             *v;
  CoglMaterialFlushOptions options;

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

  options.flags = COGL_MATERIAL_FLUSH_FALLBACK_MASK;
  if (use_color)
    options.flags |= COGL_MATERIAL_FLUSH_SKIP_GL_COLOR;
  options.fallback_layers = fallback_layers;
  _cogl_material_flush_gl_state (ctx->source_material, &options);

  GE (glDrawArrays (GL_TRIANGLE_FAN, 0, n_vertices));
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
  int                  prev_n_texcoord_arrays_enabled;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_journal_flush ();

  /* NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the material state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_framebuffer_flush_state (_cogl_get_framebuffer (), 0);

  material = ctx->source_material;
  layers = cogl_material_get_layers (ctx->source_material);
  n_layers = g_list_length ((GList *)layers);

  for (tmp = layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle layer = tmp->data;
      CoglHandle tex_handle = cogl_material_layer_get_texture (layer);

      /* COGL_INVALID_HANDLE textures will be handled in
       * _cogl_material_flush_layers_gl_state */
      if (tex_handle == COGL_INVALID_HANDLE)
        continue;

      /* Give the texture a chance to know that we're rendering
         non-quad shaped primitives. If the texture is in an atlas it
         will be migrated */
      _cogl_texture_ensure_non_quad_rendering (tex_handle);

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

#ifdef HAVE_COGL_GL
          {
            /* Temporarily change the wrapping mode on all of the slices to use
             * a transparent border
             * XXX: it's doesn't look like we save/restore this, like
             * the comment implies? */
            _cogl_texture_set_wrap_mode_parameter (tex_handle,
                                                   GL_CLAMP_TO_BORDER);
          }
#endif
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
  enable_flags |= _cogl_material_get_cogl_enable_flags (ctx->source_material);

  if (ctx->enable_backface_culling)
    enable_flags |= COGL_ENABLE_BACKFACE_CULLING;

  if (use_color)
    {
      enable_flags |= COGL_ENABLE_COLOR_ARRAY | COGL_ENABLE_BLEND;
      GE( glColorPointer (4, GL_UNSIGNED_BYTE,
                          stride_bytes,
                          /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
                          v + 3 + 2 * n_layers) );
    }

  cogl_enable (enable_flags);
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

  prev_n_texcoord_arrays_enabled = ctx->n_texcoord_arrays_enabled;
  ctx->n_texcoord_arrays_enabled = n_layers;

  for (; i < prev_n_texcoord_arrays_enabled; i++)
    {
      GE (glClientActiveTexture (GL_TEXTURE0 + i));
      GE (glDisableClientState (GL_TEXTURE_COORD_ARRAY));
    }

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
                                                 fallback_layers);

  /* Reset the size of the logged vertex array because rendering
     rectangles expects it to start at 0 */
  g_array_set_size (ctx->logged_vertices, 0);
}

