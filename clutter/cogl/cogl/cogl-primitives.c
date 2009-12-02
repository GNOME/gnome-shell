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
  CoglTextureVertex *vertices;
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
  _cogl_journal_log_quad (quad_coords[0],
                          quad_coords[1],
                          quad_coords[2],
                          quad_coords[3],
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
                                        float        x_1,
                                        float        y_1,
                                        float        x_2,
                                        float        y_2,
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

  tex_virtual_flipped_x = (tx_1 > tx_2) ? TRUE : FALSE;
  tex_virtual_flipped_y = (ty_1 > ty_2) ? TRUE : FALSE;
  state.tex_virtual_origin_x = tex_virtual_flipped_x ? tx_2 : tx_1;
  state.tex_virtual_origin_y = tex_virtual_flipped_y ? ty_2 : ty_1;

  quad_flipped_x = (x_1 > x_2) ? TRUE : FALSE;
  quad_flipped_y = (y_1 > y_2) ? TRUE : FALSE;
  state.quad_origin_x = quad_flipped_x ? x_2 : x_1;
  state.quad_origin_y = quad_flipped_y ? y_2 : y_1;

  /* flatten the two forms of coordinate inversion into one... */
  state.flipped_x = tex_virtual_flipped_x ^ quad_flipped_x;
  state.flipped_y = tex_virtual_flipped_y ^ quad_flipped_y;

  /* We use the _len_AXIS naming here instead of _width and _height because
   * log_quad_slice_cb uses a macro with symbol concatenation to handle both
   * axis, so this is more convenient... */
  state.quad_len_x = fabs (x_2 - x_1);
  state.quad_len_y = fabs (y_2 - y_1);

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
_cogl_multitexture_quad_single_primitive (float        x_1,
                                          float        y_1,
                                          float        x_2,
                                          float        y_2,
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
      gint               coord_num;
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
      for (coord_num = 0; coord_num < 2; coord_num++)
        {
          float *s = out_tex_coords + coord_num * 2;
          float *t = s + 1;
          _cogl_texture_transform_coords_to_gl (tex_handle, s, t);
          if (*s < 0.0f || *s > 1.0f || *t < 0.0f || *t > 1.0f)
            need_repeat = TRUE;
        }

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

  _cogl_journal_log_quad (x_1,
                          y_1,
                          x_2,
                          y_2,
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
  float        x_1;
  float        y_1;
  float        x_2;
  float        y_2;
  const float *tex_coords;
  gint             tex_coords_len;
};

static void
_cogl_rectangles_with_multitexture_coords (
                                        struct _CoglMutiTexturedRect *rects,
                                        gint                          n_rects)
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
      gulong         flags;

      if (cogl_material_layer_get_type (layer)
	  != COGL_MATERIAL_LAYER_TYPE_TEXTURE)
	continue;

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
            _cogl_multitexture_quad_single_primitive (rects[i].x_1,
                                                      rects[i].y_1,
                                                      rects[i].x_2,
                                                      rects[i].y_2,
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
                                              rects[i].x_1, rects[i].y_1,
                                              rects[i].x_2, rects[i].y_2,
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
                 guint        n_rects)
{
  struct _CoglMutiTexturedRect *rects;
  int i;

  rects = g_alloca (n_rects * sizeof (struct _CoglMutiTexturedRect));

  for (i = 0; i < n_rects; i++)
    {
      rects[i].x_1 = verts[i * 4];
      rects[i].y_1 = verts[i * 4 + 1];
      rects[i].x_2 = verts[i * 4 + 2];
      rects[i].y_2 = verts[i * 4 + 3];
      rects[i].tex_coords = NULL;
      rects[i].tex_coords_len = 0;
    }

  _cogl_rectangles_with_multitexture_coords (rects, n_rects);
}

void
cogl_rectangles_with_texture_coords (const float *verts,
                                     guint        n_rects)
{
  struct _CoglMutiTexturedRect *rects;
  int i;

  rects = g_alloca (n_rects * sizeof (struct _CoglMutiTexturedRect));

  for (i = 0; i < n_rects; i++)
    {
      rects[i].x_1 = verts[i * 8];
      rects[i].y_1 = verts[i * 8 + 1];
      rects[i].x_2 = verts[i * 8 + 2];
      rects[i].y_2 = verts[i * 8 + 3];
      /* FIXME: rect should be defined to have a const float *geom;
       * instead, to avoid this copy
       * rect[i].geom = &verts[n_rects * 8]; */
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
  float verts[8];

  verts[0] = x_1;
  verts[1] = y_1;
  verts[2] = x_2;
  verts[3] = y_2;
  verts[4] = tx_1;
  verts[5] = ty_1;
  verts[6] = tx_2;
  verts[7] = ty_2;

  cogl_rectangles_with_texture_coords (verts, 1);
}

void
cogl_rectangle_with_multitexture_coords (float        x_1,
			                 float        y_1,
			                 float        x_2,
			                 float        y_2,
			                 const float *user_tex_coords,
                                         gint         user_tex_coords_len)
{
  struct _CoglMutiTexturedRect rect;

  rect.x_1 = x_1;
  rect.y_1 = y_1;
  rect.x_2 = x_2;
  rect.y_2 = y_2;
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
  cogl_rectangle_with_multitexture_coords (x_1, y_1,
                                           x_2, y_2,
                                           NULL, 0);
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
_cogl_texture_polygon_multiple_primitives (CoglTextureVertex *vertices,
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
_cogl_multitexture_polygon_single_primitive (CoglTextureVertex *vertices,
                                             guint n_vertices,
                                             guint n_layers,
                                             guint stride,
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
cogl_polygon (CoglTextureVertex *vertices,
              guint              n_vertices,
	      gboolean           use_color)
{
  CoglHandle           material;
  const GList         *layers;
  int                  n_layers;
  GList               *tmp;
  gboolean	       use_sliced_polygon_fallback = FALSE;
  guint32              fallback_layers = 0;
  int                  i;
  gulong               enable_flags;
  guint                stride;
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

  for (tmp = (GList *)layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle   layer = (CoglHandle)tmp->data;
      CoglHandle   tex_handle = cogl_material_layer_get_texture (layer);

      /* COGL_INVALID_HANDLE textures will be handled in
       * _cogl_material_flush_layers_gl_state */
      if (tex_handle == COGL_INVALID_HANDLE)
        continue;

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
                             "CGL_NEAREST");
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
  prev_n_texcoord_arrays_enabled =
    ctx->n_texcoord_arrays_enabled;
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

static void
_cogl_path_add_node (gboolean new_sub_path,
		     float x,
		     float y)
{
  CoglPathNode new_node;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  new_node.x = x;
  new_node.y = y;
  new_node.path_size = 0;

  if (new_sub_path || ctx->path_nodes->len == 0)
    ctx->last_path = ctx->path_nodes->len;

  g_array_append_val (ctx->path_nodes, new_node);

  g_array_index (ctx->path_nodes, CoglPathNode, ctx->last_path).path_size++;

  if (ctx->path_nodes->len == 1)
    {
      ctx->path_nodes_min.x = ctx->path_nodes_max.x = x;
      ctx->path_nodes_min.y = ctx->path_nodes_max.y = y;
    }
  else
    {
      if (x < ctx->path_nodes_min.x) ctx->path_nodes_min.x = x;
      if (x > ctx->path_nodes_max.x) ctx->path_nodes_max.x = x;
      if (y < ctx->path_nodes_min.y) ctx->path_nodes_min.y = y;
      if (y > ctx->path_nodes_max.y) ctx->path_nodes_max.y = y;
    }
}

static void
_cogl_path_stroke_nodes (void)
{
  unsigned int   path_start = 0;
  unsigned long  enable_flags = COGL_ENABLE_VERTEX_ARRAY;
  CoglMaterialFlushOptions options;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_journal_flush ();

  /* NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the material state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_framebuffer_flush_state (_cogl_get_framebuffer (), 0);

  enable_flags |= _cogl_material_get_cogl_enable_flags (ctx->source_material);
  cogl_enable (enable_flags);

  options.flags = COGL_MATERIAL_FLUSH_DISABLE_MASK;
  /* disable all texture layers */
  options.disable_layers = (guint32)~0;

  _cogl_material_flush_gl_state (ctx->source_material, &options);

  while (path_start < ctx->path_nodes->len)
    {
      CoglPathNode *path = &g_array_index (ctx->path_nodes, CoglPathNode,
                                           path_start);

      GE( glVertexPointer (2, GL_FLOAT, sizeof (CoglPathNode),
                           (guchar *) path
                           + G_STRUCT_OFFSET (CoglPathNode, x)) );
      GE( glDrawArrays (GL_LINE_STRIP, 0, path->path_size) );

      path_start += path->path_size;
    }
}

static void
_cogl_path_get_bounds (floatVec2 nodes_min,
                       floatVec2 nodes_max,
                       float *bounds_x,
                       float *bounds_y,
                       float *bounds_w,
                       float *bounds_h)
{
  *bounds_x = nodes_min.x;
  *bounds_y = nodes_min.y;
  *bounds_w = nodes_max.x - *bounds_x;
  *bounds_h = nodes_max.y - *bounds_y;
}

void
_cogl_add_path_to_stencil_buffer (floatVec2 nodes_min,
                                  floatVec2 nodes_max,
                                  unsigned int  path_size,
                                  CoglPathNode *path,
                                  gboolean      merge,
                                  gboolean      need_clear)
{
  unsigned int     path_start = 0;
  unsigned int     sub_path_num = 0;
  float            bounds_x;
  float            bounds_y;
  float            bounds_w;
  float            bounds_h;
  unsigned long    enable_flags = COGL_ENABLE_VERTEX_ARRAY;
  CoglHandle       prev_source;
  int              i;
  CoglHandle       framebuffer = _cogl_get_framebuffer ();
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);


  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We don't track changes to the stencil buffer in the journal
   * so we need to flush any batched geometry first */
  _cogl_journal_flush ();

  /* NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the material state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_framebuffer_flush_state (framebuffer, 0);

  /* Just setup a simple material that doesn't use texturing... */
  prev_source = cogl_handle_ref (ctx->source_material);
  cogl_set_source (ctx->stencil_material);

  _cogl_material_flush_gl_state (ctx->source_material, NULL);

  enable_flags |=
    _cogl_material_get_cogl_enable_flags (ctx->source_material);
  cogl_enable (enable_flags);

  _cogl_path_get_bounds (nodes_min, nodes_max,
                         &bounds_x, &bounds_y, &bounds_w, &bounds_h);

  GE( glEnable (GL_STENCIL_TEST) );

  GE( glColorMask (FALSE, FALSE, FALSE, FALSE) );
  GE( glDepthMask (FALSE) );

  if (merge)
    {
      GE (glStencilMask (2));
      GE (glStencilFunc (GL_LEQUAL, 0x2, 0x6));
    }
  else
    {
      /* If we're not using the stencil buffer for clipping then we
         don't need to clear the whole stencil buffer, just the area
         that will be drawn */
      if (need_clear)
        cogl_clear (NULL, COGL_BUFFER_BIT_STENCIL);
      else
        {
          /* Just clear the bounding box */
          GE( glStencilMask (~(GLuint) 0) );
          GE( glStencilOp (GL_ZERO, GL_ZERO, GL_ZERO) );
          cogl_rectangle (bounds_x, bounds_y,
                          bounds_x + bounds_w, bounds_y + bounds_h);
          /* Make sure the rectangle hits the stencil buffer before
           * directly changing other GL state. */
          _cogl_journal_flush ();
          /* NB: The journal flushing may trash the modelview state and
           * enable flags */
          _cogl_matrix_stack_flush_to_gl (modelview_stack,
                                          COGL_MATRIX_MODELVIEW);
          cogl_enable (enable_flags);
        }
      GE (glStencilMask (1));
      GE (glStencilFunc (GL_LEQUAL, 0x1, 0x3));
    }

  GE (glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT));

  for (i = 0; i < ctx->n_texcoord_arrays_enabled; i++)
    {
      GE (glClientActiveTexture (GL_TEXTURE0 + i));
      GE (glDisableClientState (GL_TEXTURE_COORD_ARRAY));
    }
  ctx->n_texcoord_arrays_enabled = 0;

  while (path_start < path_size)
    {
      GE (glVertexPointer (2, GL_FLOAT, sizeof (CoglPathNode),
                           (guchar *) path
                           + G_STRUCT_OFFSET (CoglPathNode, x)));
      GE (glDrawArrays (GL_TRIANGLE_FAN, 0, path->path_size));

      if (sub_path_num > 0)
        {
          /* Union the two stencil buffers bits into the least
             significant bit */
          GE (glStencilMask (merge ? 6 : 3));
          GE (glStencilOp (GL_ZERO, GL_REPLACE, GL_REPLACE));
          cogl_rectangle (bounds_x, bounds_y,
                          bounds_x + bounds_w, bounds_y + bounds_h);
          /* Make sure the rectangle hits the stencil buffer before
           * directly changing other GL state. */
          _cogl_journal_flush ();
          /* NB: The journal flushing may trash the modelview state and
           * enable flags */
          _cogl_matrix_stack_flush_to_gl (modelview_stack,
                                          COGL_MATRIX_MODELVIEW);
          cogl_enable (enable_flags);

          GE (glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT));
        }

      GE (glStencilMask (merge ? 4 : 2));

      path_start += path->path_size;
      path += path->path_size;
      sub_path_num++;
    }

  if (merge)
    {
      /* Now we have the new stencil buffer in bit 1 and the old
         stencil buffer in bit 0 so we need to intersect them */
      GE (glStencilMask (3));
      GE (glStencilFunc (GL_NEVER, 0x2, 0x3));
      GE (glStencilOp (GL_DECR, GL_DECR, GL_DECR));
      /* Decrement all of the bits twice so that only pixels where the
         value is 3 will remain */

      _cogl_matrix_stack_push (projection_stack);
      _cogl_matrix_stack_load_identity (projection_stack);
      _cogl_matrix_stack_flush_to_gl (projection_stack,
                                      COGL_MATRIX_PROJECTION);

      _cogl_matrix_stack_push (modelview_stack);
      _cogl_matrix_stack_load_identity (modelview_stack);
      _cogl_matrix_stack_flush_to_gl (modelview_stack,
                                      COGL_MATRIX_MODELVIEW);

      cogl_rectangle (-1.0, -1.0, 1.0, 1.0);
      cogl_rectangle (-1.0, -1.0, 1.0, 1.0);
      /* Make sure these rectangles hit the stencil buffer before we
       * restore the stencil op/func. */
      _cogl_journal_flush ();

      _cogl_matrix_stack_pop (modelview_stack);
      _cogl_matrix_stack_pop (projection_stack);
    }

  GE (glStencilMask (~(GLuint) 0));
  GE (glDepthMask (TRUE));
  GE (glColorMask (TRUE, TRUE, TRUE, TRUE));

  GE (glStencilFunc (GL_EQUAL, 0x1, 0x1));
  GE (glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP));

  /* restore the original material */
  cogl_set_source (prev_source);
  cogl_handle_unref (prev_source);
}

static gint
compare_ints (gconstpointer a,
              gconstpointer b)
{
  return GPOINTER_TO_INT(a)-GPOINTER_TO_INT(b);
}

static void
_cogl_path_fill_nodes_scanlines (CoglPathNode *path,
                                 unsigned int  path_size,
                                 int           bounds_x,
                                 int           bounds_y,
                                 unsigned int  bounds_w,
                                 unsigned int  bounds_h)
{
  /* This is our edge list it stores intersections between our
   * curve and scanlines, it should probably be implemented with a
   * data structure that has smaller overhead for inserting the
   * curve/scanline intersections.
   */
  GSList **scanlines = g_alloca (bounds_h * sizeof (GSList *));

  int i;
  int prev_x;
  int prev_y;
  int first_x;
  int first_y;
  int lastdir = -2; /* last direction we vere moving */
  int lastline = -1; /* the previous scanline we added to */

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We are going to use GL to draw directly so make sure any
   * previously batched geometry gets to GL before we start...
   */
  _cogl_journal_flush ();

  /* NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the material state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_framebuffer_flush_state (_cogl_get_framebuffer (), 0);

  _cogl_material_flush_gl_state (ctx->source_material, NULL);

  cogl_enable (COGL_ENABLE_VERTEX_ARRAY
               | (ctx->color_alpha < 255 ? COGL_ENABLE_BLEND : 0));

  /* clear scanline intersection lists */
  for (i = 0; i < bounds_h; i++)
    scanlines[i]=NULL;

  first_x = prev_x = path->x;
  first_y = prev_y = path->y;

  /* create scanline intersection list */
  for (i=1; i < path_size; i++)
    {
      int dest_x = path[i].x;
      int dest_y = path[i].y;
      int ydir;
      int dx;
      int dy;
      int y;

    fill_close:
      dx = dest_x - prev_x;
      dy = dest_y - prev_y;

      if (dy < 0)
        ydir = -1;
      else if (dy > 0)
        ydir = 1;
      else
        ydir = 0;

      /* do linear interpolation between vertices */
      for (y = prev_y; y != dest_y; y += ydir)
        {

          /* only add a point if the scanline has changed and we're
           * within bounds.
           */
          if (y - bounds_y >= 0 &&
              y - bounds_y < bounds_h &&
              lastline != y)
            {
              gint x = prev_x + (dx * (y-prev_y)) / dy;

              scanlines[ y - bounds_y ]=
                g_slist_insert_sorted (scanlines[ y - bounds_y],
                                       GINT_TO_POINTER(x),
                                       compare_ints);

              if (ydir != lastdir &&  /* add a double entry when changing */
                  lastdir != -2)        /* vertical direction */
                scanlines[ y - bounds_y ]=
                  g_slist_insert_sorted (scanlines[ y - bounds_y],
                                         GINT_TO_POINTER(x),
                                         compare_ints);
              lastdir = ydir;
              lastline = y;
            }
        }

      prev_x = dest_x;
      prev_y = dest_y;

      /* if we're on the last knot, fake the first vertex being a
         next one */
      if (path_size == i+1)
        {
          dest_x = first_x;
          dest_y = first_y;
          i++; /* to make the loop finally end */
          goto fill_close;
        }
    }

  {
    int spans = 0;
    int span_no;
    GLfloat *coords;

    /* count number of spans */
    for (i = 0; i < bounds_h; i++)
      {
        GSList *iter = scanlines[i];
        while (iter)
          {
            GSList *next = iter->next;
            if (!next)
              {
                break;
              }
            /* draw the segments that should be visible */
            spans ++;
            iter = next->next;
          }
      }
    coords = g_malloc0 (spans * sizeof (GLfloat) * 3 * 2 * 2);

    span_no = 0;
    /* build list of triangles */
    for (i = 0; i < bounds_h; i++)
      {
        GSList *iter = scanlines[i];
        while (iter)
          {
            GSList *next = iter->next;
            GLfloat x_0, x_1;
            GLfloat y_0, y_1;
            if (!next)
              break;

            x_0 = GPOINTER_TO_INT (iter->data);
            x_1 = GPOINTER_TO_INT (next->data);
            y_0 = bounds_y + i;
            y_1 = bounds_y + i + 1.0625f;
            /* render scanlines 1.0625 high to avoid gaps when
               transformed */

            coords[span_no * 12 + 0] = x_0;
            coords[span_no * 12 + 1] = y_0;
            coords[span_no * 12 + 2] = x_1;
            coords[span_no * 12 + 3] = y_0;
            coords[span_no * 12 + 4] = x_1;
            coords[span_no * 12 + 5] = y_1;
            coords[span_no * 12 + 6] = x_0;
            coords[span_no * 12 + 7] = y_0;
            coords[span_no * 12 + 8] = x_0;
            coords[span_no * 12 + 9] = y_1;
            coords[span_no * 12 + 10] = x_1;
            coords[span_no * 12 + 11] = y_1;
            span_no ++;
            iter = next->next;
          }
      }
    for (i = 0; i < bounds_h; i++)
      g_slist_free (scanlines[i]);

    /* render triangles */
    GE (glVertexPointer (2, GL_FLOAT, 0, coords ));
    GE (glDrawArrays (GL_TRIANGLES, 0, spans * 2 * 3));
    g_free (coords);
  }
}

static void
_cogl_path_fill_nodes (void)
{
  float bounds_x;
  float bounds_y;
  float bounds_w;
  float bounds_h;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_path_get_bounds (ctx->path_nodes_min, ctx->path_nodes_max,
                         &bounds_x, &bounds_y, &bounds_w, &bounds_h);

  if (G_LIKELY (!(cogl_debug_flags & COGL_DEBUG_FORCE_SCANLINE_PATHS)) &&
      cogl_features_available (COGL_FEATURE_STENCIL_BUFFER))
    {
      CoglHandle framebuffer;
      CoglClipStackState *clip_state;

      _cogl_journal_flush ();

      framebuffer = _cogl_get_framebuffer ();
      clip_state = _cogl_framebuffer_get_clip_state (framebuffer);

      _cogl_add_path_to_stencil_buffer (ctx->path_nodes_min,
                                        ctx->path_nodes_max,
                                        ctx->path_nodes->len,
                                        &g_array_index (ctx->path_nodes,
                                                        CoglPathNode, 0),
                                        clip_state->stencil_used,
                                        FALSE);

      cogl_rectangle (bounds_x, bounds_y,
                      bounds_x + bounds_w, bounds_y + bounds_h);

      /* The stencil buffer now contains garbage so the clip area needs to
         be rebuilt */
      _cogl_clip_stack_state_dirty (clip_state);
    }
  else
    {
      unsigned int path_start = 0;

      while (path_start < ctx->path_nodes->len)
        {
          CoglPathNode *path = &g_array_index (ctx->path_nodes, CoglPathNode,
                                               path_start);

          _cogl_path_fill_nodes_scanlines (path,
                                           path->path_size,
                                           bounds_x, bounds_y,
                                           bounds_w, bounds_h);

          path_start += path->path_size;
        }
    }
}

void
cogl_path_fill (void)
{
  cogl_path_fill_preserve ();

  cogl_path_new ();
}

void
cogl_path_fill_preserve (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->path_nodes->len == 0)
    return;

  _cogl_path_fill_nodes ();
}

void
cogl_path_stroke (void)
{
  cogl_path_stroke_preserve ();

  cogl_path_new ();
}

void
cogl_path_stroke_preserve (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->path_nodes->len == 0)
    return;

  _cogl_path_stroke_nodes ();
}

void
cogl_path_move_to (float x,
                   float y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* FIXME: handle multiple contours maybe? */

  _cogl_path_add_node (TRUE, x, y);

  ctx->path_start.x = x;
  ctx->path_start.y = y;

  ctx->path_pen = ctx->path_start;
}

void
cogl_path_rel_move_to (float x,
                       float y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_path_move_to (ctx->path_pen.x + x,
                     ctx->path_pen.y + y);
}

void
cogl_path_line_to (float x,
                   float y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_path_add_node (FALSE, x, y);

  ctx->path_pen.x = x;
  ctx->path_pen.y = y;
}

void
cogl_path_rel_line_to (float x,
                       float y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_path_line_to (ctx->path_pen.x + x,
                     ctx->path_pen.y + y);
}

void
cogl_path_close (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_path_add_node (FALSE, ctx->path_start.x, ctx->path_start.y);
  ctx->path_pen = ctx->path_start;
}

void
cogl_path_new (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_array_set_size (ctx->path_nodes, 0);
}

void
cogl_path_line (float x_1,
	        float y_1,
	        float x_2,
	        float y_2)
{
  cogl_path_move_to (x_1, y_1);
  cogl_path_line_to (x_2, y_2);
}

void
cogl_path_polyline (float *coords,
	            gint num_points)
{
  gint c = 0;

  cogl_path_move_to (coords[0], coords[1]);

  for (c = 1; c < num_points; ++c)
    cogl_path_line_to (coords[2*c], coords[2*c+1]);
}

void
cogl_path_polygon (float *coords,
	           gint          num_points)
{
  cogl_path_polyline (coords, num_points);
  cogl_path_close ();
}

void
cogl_path_rectangle (float x_1,
                     float y_1,
                     float x_2,
                     float y_2)
{
  cogl_path_move_to (x_1, y_1);
  cogl_path_line_to (x_2, y_1);
  cogl_path_line_to (x_2, y_2);
  cogl_path_line_to (x_1, y_2);
  cogl_path_close   ();
}

static void
_cogl_path_arc (float center_x,
	        float center_y,
                float radius_x,
                float radius_y,
                float angle_1,
                float angle_2,
                float angle_step,
                guint        move_first)
{
  float a     = 0x0;
  float cosa  = 0x0;
  float sina  = 0x0;
  float px    = 0x0;
  float py    = 0x0;

  /* Fix invalid angles */

  if (angle_1 == angle_2 || angle_step == 0x0)
    return;

  if (angle_step < 0x0)
    angle_step = -angle_step;

  /* Walk the arc by given step */

  a = angle_1;
  while (a != angle_2)
    {
      cosa = cosf (a * (G_PI/180.0));
      sina = sinf (a * (G_PI/180.0));

      px = center_x + (cosa * radius_x);
      py = center_y + (sina * radius_y);

      if (a == angle_1 && move_first)
	cogl_path_move_to (px, py);
      else
	cogl_path_line_to (px, py);

      if (G_LIKELY (angle_2 > angle_1))
        {
          a += angle_step;
          if (a > angle_2)
            a = angle_2;
        }
      else
        {
          a -= angle_step;
          if (a < angle_2)
            a = angle_2;
        }
    }

  /* Make sure the final point is drawn */

  cosa = cosf (angle_2 * (G_PI/180.0));
  sina = sinf (angle_2 * (G_PI/180.0));

  px = center_x + (cosa * radius_x);
  py = center_y + (sina * radius_y);

  cogl_path_line_to (px, py);
}

void
cogl_path_arc (float center_x,
               float center_y,
               float radius_x,
               float radius_y,
               float angle_1,
               float angle_2)
{
  float angle_step = 10;
  /* it is documented that a move to is needed to create a freestanding
   * arc
   */
  _cogl_path_arc (center_x,   center_y,
	          radius_x,   radius_y,
	          angle_1,    angle_2,
	          angle_step, 0 /* no move */);
}


void
cogl_path_arc_rel (float center_x,
		   float center_y,
		   float radius_x,
		   float radius_y,
		   float angle_1,
		   float angle_2,
		   float angle_step)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_path_arc (ctx->path_pen.x + center_x,
	          ctx->path_pen.y + center_y,
	          radius_x,   radius_y,
	          angle_1,    angle_2,
	          angle_step, 0 /* no move */);
}

void
cogl_path_ellipse (float center_x,
                   float center_y,
                   float radius_x,
                   float radius_y)
{
  float angle_step = 10;

  /* FIXME: if shows to be slow might be optimized
   * by mirroring just a quarter of it */

  _cogl_path_arc (center_x, center_y,
	          radius_x, radius_y,
	          0, 360,
	          angle_step, 1 /* move first */);

  cogl_path_close();
}

void
cogl_path_round_rectangle (float x_1,
                           float y_1,
                           float x_2,
                           float y_2,
                           float radius,
                           float arc_step)
{
  float inner_width = x_2 - x_1 - radius * 2;
  float inner_height = y_2 - y_1 - radius * 2;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_path_move_to (x_1, y_1 + radius);
  cogl_path_arc_rel (radius, 0,
		     radius, radius,
		     180,
		     270,
		     arc_step);

  cogl_path_line_to       (ctx->path_pen.x + inner_width,
                           ctx->path_pen.y);
  cogl_path_arc_rel       (0, radius,
			   radius, radius,
			   -90,
			   0,
			   arc_step);

  cogl_path_line_to       (ctx->path_pen.x,
                           ctx->path_pen.y + inner_height);

  cogl_path_arc_rel       (-radius, 0,
			   radius, radius,
			   0,
			   90,
			   arc_step);

  cogl_path_line_to       (ctx->path_pen.x - inner_width,
                           ctx->path_pen.y);
  cogl_path_arc_rel       (0, -radius,
			   radius, radius,
			   90,
			   180,
			   arc_step);

  cogl_path_close ();
}


static void
_cogl_path_bezier3_sub (CoglBezCubic *cubic)
{
  CoglBezCubic   cubics[_COGL_MAX_BEZ_RECURSE_DEPTH];
  CoglBezCubic  *cleft;
  CoglBezCubic  *cright;
  CoglBezCubic  *c;
  floatVec2  dif1;
  floatVec2  dif2;
  floatVec2  mm;
  floatVec2  c1;
  floatVec2  c2;
  floatVec2  c3;
  floatVec2  c4;
  floatVec2  c5;
  gint           cindex;

  /* Put first curve on stack */
  cubics[0] = *cubic;
  cindex    =  0;

  while (cindex >= 0)
    {
      c = &cubics[cindex];


      /* Calculate distance of control points from their
       * counterparts on the line between end points */
      dif1.x = (c->p2.x * 3) - (c->p1.x * 2) - c->p4.x;
      dif1.y = (c->p2.y * 3) - (c->p1.y * 2) - c->p4.y;
      dif2.x = (c->p3.x * 3) - (c->p4.x * 2) - c->p1.x;
      dif2.y = (c->p3.y * 3) - (c->p4.y * 2) - c->p1.y;

      if (dif1.x < 0)
        dif1.x = -dif1.x;
      if (dif1.y < 0)
        dif1.y = -dif1.y;
      if (dif2.x < 0)
        dif2.x = -dif2.x;
      if (dif2.y < 0)
        dif2.y = -dif2.y;


      /* Pick the greatest of two distances */
      if (dif1.x < dif2.x) dif1.x = dif2.x;
      if (dif1.y < dif2.y) dif1.y = dif2.y;

      /* Cancel if the curve is flat enough */
      if (dif1.x + dif1.y <= 1.0 ||
	  cindex == _COGL_MAX_BEZ_RECURSE_DEPTH-1)
	{
	  /* Add subdivision point (skip last) */
	  if (cindex == 0)
            return;

	  _cogl_path_add_node (FALSE, c->p4.x, c->p4.y);

	  --cindex;

          continue;
	}

      /* Left recursion goes on top of stack! */
      cright = c; cleft = &cubics[++cindex];

      /* Subdivide into 2 sub-curves */
      c1.x = ((c->p1.x + c->p2.x) / 2);
      c1.y = ((c->p1.y + c->p2.y) / 2);
      mm.x = ((c->p2.x + c->p3.x) / 2);
      mm.y = ((c->p2.y + c->p3.y) / 2);
      c5.x = ((c->p3.x + c->p4.x) / 2);
      c5.y = ((c->p3.y + c->p4.y) / 2);

      c2.x = ((c1.x + mm.x) / 2);
      c2.y = ((c1.y + mm.y) / 2);
      c4.x = ((mm.x + c5.x) / 2);
      c4.y = ((mm.y + c5.y) / 2);

      c3.x = ((c2.x + c4.x) / 2);
      c3.y = ((c2.y + c4.y) / 2);

      /* Add left recursion to stack */
      cleft->p1 = c->p1;
      cleft->p2 = c1;
      cleft->p3 = c2;
      cleft->p4 = c3;

      /* Add right recursion to stack */
      cright->p1 = c3;
      cright->p2 = c4;
      cright->p3 = c5;
      cright->p4 = c->p4;
    }
}

void
cogl_path_curve_to (float x_1,
                    float y_1,
                    float x_2,
                    float y_2,
                    float x_3,
                    float y_3)
{
  CoglBezCubic cubic;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Prepare cubic curve */
  cubic.p1 = ctx->path_pen;
  cubic.p2.x = x_1;
  cubic.p2.y = y_1;
  cubic.p3.x = x_2;
  cubic.p3.y = y_2;
  cubic.p4.x = x_3;
  cubic.p4.y = y_3;

  /* Run subdivision */
  _cogl_path_bezier3_sub (&cubic);

  /* Add last point */
  _cogl_path_add_node (FALSE, cubic.p4.x, cubic.p4.y);
  ctx->path_pen = cubic.p4;
}

void
cogl_path_rel_curve_to (float x_1,
                        float y_1,
                        float x_2,
                        float y_2,
                        float x_3,
                        float y_3)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_path_curve_to (ctx->path_pen.x + x_1,
                      ctx->path_pen.y + y_1,
                      ctx->path_pen.x + x_2,
                      ctx->path_pen.y + y_2,
                      ctx->path_pen.x + x_3,
                      ctx->path_pen.y + y_3);
}


/* If second order beziers were needed the following code could
 * be re-enabled:
 */
#if 0

static void
_cogl_path_bezier2_sub (CoglBezQuad *quad)
{
  CoglBezQuad     quads[_COGL_MAX_BEZ_RECURSE_DEPTH];
  CoglBezQuad    *qleft;
  CoglBezQuad    *qright;
  CoglBezQuad    *q;
  floatVec2   mid;
  floatVec2   dif;
  floatVec2   c1;
  floatVec2   c2;
  floatVec2   c3;
  gint            qindex;

  /* Put first curve on stack */
  quads[0] = *quad;
  qindex   =  0;

  /* While stack is not empty */
  while (qindex >= 0)
    {

      q = &quads[qindex];

      /* Calculate distance of control point from its
       * counterpart on the line between end points */
      mid.x = ((q->p1.x + q->p3.x) / 2);
      mid.y = ((q->p1.y + q->p3.y) / 2);
      dif.x = (q->p2.x - mid.x);
      dif.y = (q->p2.y - mid.y);
      if (dif.x < 0) dif.x = -dif.x;
      if (dif.y < 0) dif.y = -dif.y;

      /* Cancel if the curve is flat enough */
      if (dif.x + dif.y <= 1.0 ||
          qindex == _COGL_MAX_BEZ_RECURSE_DEPTH - 1)
	{
	  /* Add subdivision point (skip last) */
	  if (qindex == 0) return;
	  _cogl_path_add_node (FALSE, q->p3.x, q->p3.y);
	  --qindex; continue;
	}

      /* Left recursion goes on top of stack! */
      qright = q; qleft = &quads[++qindex];

      /* Subdivide into 2 sub-curves */
      c1.x = ((q->p1.x + q->p2.x) / 2);
      c1.y = ((q->p1.y + q->p2.y) / 2);
      c3.x = ((q->p2.x + q->p3.x) / 2);
      c3.y = ((q->p2.y + q->p3.y) / 2);
      c2.x = ((c1.x + c3.x) / 2);
      c2.y = ((c1.y + c3.y) / 2);

      /* Add left recursion onto stack */
      qleft->p1 = q->p1;
      qleft->p2 = c1;
      qleft->p3 = c2;

      /* Add right recursion onto stack */
      qright->p1 = c2;
      qright->p2 = c3;
      qright->p3 = q->p3;
    }
}

void
cogl_path_curve2_to (float x_1,
                     float y_1,
                     float x_2,
                     float y_2)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  CoglBezQuad quad;

  /* Prepare quadratic curve */
  quad.p1 = ctx->path_pen;
  quad.p2.x = x_1;
  quad.p2.y = y_1;
  quad.p3.x = x_2;
  quad.p3.y = y_2;

  /* Run subdivision */
  _cogl_path_bezier2_sub (&quad);

  /* Add last point */
  _cogl_path_add_node (FALSE, quad.p3.x, quad.p3.y);
  ctx->path_pen = quad.p3;
}

void
cogl_rel_curve2_to (float x_1,
                    float y_1,
                    float x_2,
                    float y_2)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_path_curve2_to (ctx->path_pen.x + x_1,
                       ctx->path_pen.y + y_1,
                       ctx->path_pen.x + x_2,
                       ctx->path_pen.y + y_2);
}
#endif
