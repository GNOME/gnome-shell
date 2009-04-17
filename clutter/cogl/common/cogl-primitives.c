/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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
#include "cogl-texture-private.h"
#include "cogl-material.h"

#include <string.h>
#include <gmodule.h>
#include <math.h>

#define _COGL_MAX_BEZ_RECURSE_DEPTH 16

#ifdef HAVE_COGL_GL

#define glDrawRangeElements ctx->pf_glDrawRangeElements
#define glClientActiveTexture ctx->pf_glClientActiveTexture

#else

/* GLES doesn't have glDrawRangeElements, so we simply pretend it does
 * but that it makes no use of the start, end constraints: */
#define glDrawRangeElements(mode, start, end, count, type, indices) \
  glDrawElements (mode, count, type, indices)

#endif

/* these are defined in the particular backend */
void _cogl_path_add_node    (gboolean new_sub_path,
			     float x,
                             float y);
void _cogl_path_fill_nodes    ();
void _cogl_path_stroke_nodes  ();

static void
_cogl_journal_flush_quad_batch (CoglJournalEntry *batch_start,
                                gint              batch_len,
                                GLfloat          *vertex_pointer)
{
  int     needed_indices;
  gsize   stride;
  int     i;
  gulong  enable_flags = 0;
  guint32 disable_mask;
  int     prev_n_texcoord_arrays_enabled;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* The indices are always the same sequence regardless of the vertices so we
   * only need to change it if there are more vertices than ever before. */
  needed_indices = batch_len * 6;
  if (needed_indices > ctx->static_indices->len)
    {
      int old_len = ctx->static_indices->len;
      int vert_num = old_len / 6 * 4;
      GLushort *q;

      /* Add two triangles for each quad to the list of
         indices. That makes six new indices but two of the
         vertices in the triangles are shared. */
      g_array_set_size (ctx->static_indices, needed_indices);
      q = &g_array_index (ctx->static_indices, GLushort, old_len);

      for (i = old_len;
           i < ctx->static_indices->len;
           i += 6, vert_num += 4)
        {
          *(q++) = vert_num + 0;
          *(q++) = vert_num + 1;
          *(q++) = vert_num + 3;

          *(q++) = vert_num + 1;
          *(q++) = vert_num + 2;
          *(q++) = vert_num + 3;
        }
    }

  /* XXX NB:
   * Our vertex data is arranged as follows:
   * 4 vertices per quad: 2 GLfloats per position,
   *                      2 GLfloats per tex coord * n_layers
   */
  stride = 2 + 2 * batch_start->n_layers;
  stride *= sizeof (GLfloat);

  disable_mask = (1 << batch_start->n_layers) - 1;
  disable_mask = ~disable_mask;

  cogl_material_flush_gl_state (ctx->source_material,
                                COGL_MATERIAL_FLUSH_FALLBACK_MASK,
                                batch_start->fallback_mask,
                                COGL_MATERIAL_FLUSH_DISABLE_MASK,
                                disable_mask,
                                /* Redundant when dealing with unsliced
                                 * textures but does no harm... */
                                COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE,
                                batch_start->layer0_override_texture,
                                NULL);

  for (i = 0; i < batch_start->n_layers; i++)
    {
      GE (glClientActiveTexture (GL_TEXTURE0 + i));
      GE (glEnableClientState (GL_TEXTURE_COORD_ARRAY));
      GE (glTexCoordPointer (2, GL_FLOAT, stride, vertex_pointer + 2 + 2 * i));
    }
  prev_n_texcoord_arrays_enabled =
    ctx->n_texcoord_arrays_enabled;
  ctx->n_texcoord_arrays_enabled = batch_start->n_layers;
  for (; i < prev_n_texcoord_arrays_enabled; i++)
    {
      GE (glClientActiveTexture (GL_TEXTURE0 + i));
      GE (glDisableClientState (GL_TEXTURE_COORD_ARRAY));
    }

  /* FIXME: This api is a bit yukky, ideally it will be removed if we
   * re-work the cogl_enable mechanism */
  enable_flags |= cogl_material_get_cogl_enable_flags (ctx->source_material);

  if (ctx->enable_backface_culling)
    enable_flags |= COGL_ENABLE_BACKFACE_CULLING;

  enable_flags |= COGL_ENABLE_VERTEX_ARRAY;
  cogl_enable (enable_flags);

  GE (glVertexPointer (2, GL_FLOAT, stride, vertex_pointer));
  _cogl_current_matrix_state_flush ();
  GE (glDrawRangeElements (GL_TRIANGLES,
                           0, ctx->static_indices->len - 1,
                           6 * batch_len,
                           GL_UNSIGNED_SHORT,
                           ctx->static_indices->data));


  /* DEBUGGING CODE XXX:
   * This path will cause all rectangles to be drawn with a red, green
   * or blue outline with no blending. This may e.g. help with debugging
   * texture slicing issues or blending issues, plus it looks quite cool.
   */
  if (cogl_debug_flags & COGL_DEBUG_RECTANGLES)
    {
      static CoglHandle outline = COGL_INVALID_HANDLE;
      static int color = 0;
      if (outline == COGL_INVALID_HANDLE)
        outline = cogl_material_new ();

      cogl_enable (COGL_ENABLE_VERTEX_ARRAY);
      for (i = 0; i < batch_len; i++, color = (color + 1) % 3)
        {
          cogl_material_set_color4ub (outline,
                                      color == 0 ? 0xff : 0x00,
                                      color == 1 ? 0xff : 0x00,
                                      color == 2 ? 0xff : 0x00,
                                      0xff);
          cogl_material_flush_gl_state (outline, NULL);
          _cogl_current_matrix_state_flush ();
          GE( glDrawArrays (GL_LINE_LOOP, 4 * i, 4) );
        }
    }
}

void
_cogl_journal_flush (void)
{
  GLfloat          *current_vertex_pointer;
  GLfloat          *batch_vertex_pointer;
  CoglJournalEntry *batch_start;
  guint             batch_len;
  int               i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->journal->len == 0)
    return;

  /* Current non-variables / constraints:
   *
   * - We don't have to worry about much GL state changing between journal
   *   entries since currently the journal never out lasts a single call to
   *   _cogl_multitexture_multiple_rectangles. So the user doesn't get the
   *   chance to fiddle with anything. (XXX: later this will be extended at
   *   which point we can start logging certain state changes)
   *
   * - Implied from above: all entries will refer to the same material.
   *
   * - Although _cogl_multitexture_multiple_rectangles can cause the wrap mode
   *   of textures to be modified, the journal is flushed if a wrap mode is
   *   changed so we don't currently have to log wrap mode changes.
   *
   * - XXX - others?
   */

  /* TODO: "compile" the journal to find ways of batching draw calls and vertex
   * data.
   *
   * Simple E.g. given current constraints...
   * pass 0 - load all data into a single CoglVertexBuffer
   * pass 1 - batch gl draw calls according to entries that use the same
   *          textures.
   *
   * We will be able to do cooler stuff here when we extend the life of
   * journals beyond _cogl_multitexture_multiple_rectangles.
   */

  batch_vertex_pointer = (GLfloat *)ctx->logged_vertices->data;
  batch_start = (CoglJournalEntry *)ctx->journal->data;
  batch_len = 1;

  current_vertex_pointer = batch_vertex_pointer;

  for (i = 1; i < ctx->journal->len; i++)
    {
      CoglJournalEntry *prev_entry =
        &g_array_index (ctx->journal, CoglJournalEntry, i - 1);
      CoglJournalEntry *current_entry = prev_entry + 1;
      gsize             stride;

      /* Progress the vertex pointer to the next quad */
      stride = 2 + current_entry->n_layers * 2;
      current_vertex_pointer += stride * 4;

      /* batch rectangles using the same textures */
      if (current_entry->material == prev_entry->material &&
          current_entry->n_layers == prev_entry->n_layers &&
          current_entry->fallback_mask == prev_entry->fallback_mask &&
          current_entry->layer0_override_texture
          == prev_entry->layer0_override_texture)
        {
          batch_len++;
          continue;
        }

      _cogl_journal_flush_quad_batch (batch_start,
                                      batch_len,
                                      batch_vertex_pointer);

      batch_start = current_entry;
      batch_len = 1;
      batch_vertex_pointer = current_vertex_pointer;
    }

  /* The last batch... */
  _cogl_journal_flush_quad_batch (batch_start,
                                  batch_len,
                                  batch_vertex_pointer);


  g_array_set_size (ctx->journal, 0);
  g_array_set_size (ctx->logged_vertices, 0);
}

static void
_cogl_journal_log_quad (float       x_1,
                        float       y_1,
                        float       x_2,
                        float       y_2,
                        CoglHandle  material,
                        gint        n_layers,
                        guint32     fallback_mask,
                        GLuint      layer0_override_texture,
                        float      *tex_coords,
                        guint       tex_coords_len)
{
  int               stride;
  int               next_vert;
  GLfloat          *v;
  int               i;
  int               next_entry;
  CoglJournalEntry *entry;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* The vertex data is logged into a seperate array in a layout that can be
   * directly passed to OpenGL
   */

  /* We pack the vertex data as 2 (x,y) GLfloats folowed by 2 (tx,ty) GLfloats
   * for each texture being used, E.g.:
   *  [X, Y, TX0, TY0, TX1, TY1, X, Y, TX0, TY0, X, Y, ...]
   */
  stride = 2 + n_layers * 2;

  next_vert = ctx->logged_vertices->len;
  g_array_set_size (ctx->logged_vertices, next_vert + 4 * stride);
  v = &g_array_index (ctx->logged_vertices, GLfloat, next_vert);

  /* XXX: All the jumping around to fill in this strided buffer doesn't
   * seem ideal. */

  /* XXX: we could defer expanding the vertex data for GL until we come
   * to flushing the journal. */

  v[0] = x_1; v[1] = y_1;
  v += stride;
  v[0] = x_1; v[1] = y_2;
  v += stride;
  v[0] = x_2; v[1] = y_2;
  v += stride;
  v[0] = x_2; v[1] = y_1;

  for (i = 0; i < n_layers; i++)
    {
      GLfloat *t =
        &g_array_index (ctx->logged_vertices, GLfloat, next_vert + 2 + 2 * i);

      t[0] = tex_coords[0]; t[1] = tex_coords[1];
      t += stride;
      t[0] = tex_coords[0]; t[1] = tex_coords[3];
      t += stride;
      t[0] = tex_coords[2]; t[1] = tex_coords[3];
      t += stride;
      t[0] = tex_coords[2]; t[1] = tex_coords[1];
    }

  next_entry = ctx->journal->len;
  g_array_set_size (ctx->journal, next_entry + 1);
  entry = &g_array_index (ctx->journal, CoglJournalEntry, next_entry);

  entry->material = material;
  entry->n_layers = n_layers;
  entry->fallback_mask = fallback_mask;
  entry->layer0_override_texture = layer0_override_texture;
}

static void
_cogl_texture_sliced_quad (CoglTexture *tex,
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
  CoglSpanIter  iter_x    ,  iter_y;
  float         tw        ,  th;
  float         tqx       ,  tqy;
  float         first_tx  ,  first_ty;
  float         first_qx  ,  first_qy;
  float         slice_tx1 ,  slice_ty1;
  float         slice_tx2 ,  slice_ty2;
  float         slice_qx1 ,  slice_qy1;
  float         slice_qx2 ,  slice_qy2;
  GLuint        gl_handle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

#if COGL_DEBUG
  printf("=== Drawing Tex Quad (Sliced Mode) ===\n");
#endif

  /* We can't use hardware repeat so we need to set clamp to edge
     otherwise it might pull in edge pixels from the other side */
  _cogl_texture_set_wrap_mode_parameter (tex, GL_CLAMP_TO_EDGE);

  /* If the texture coordinates are backwards then swap both the
     geometry and texture coordinates so that the texture will be
     flipped but we can still use the same algorithm to iterate the
     slices */
  if (tx_2 < tx_1)
    {
      float temp = x_1;
      x_1 = x_2;
      x_2 = temp;
      temp = tx_1;
      tx_1 = tx_2;
      tx_2 = temp;
    }
  if (ty_2 < ty_1)
    {
      float temp = y_1;
      y_1 = y_2;
      y_2 = temp;
      temp = ty_1;
      ty_1 = ty_2;
      ty_2 = temp;
    }

  /* Scale ratio from texture to quad widths */
  tw = (float)(tex->bitmap.width);
  th = (float)(tex->bitmap.height);

  tqx = (x_2 - x_1) / (tw * (tx_2 - tx_1));
  tqy = (y_2 - y_1) / (th * (ty_2 - ty_1));

  /* Integral texture coordinate for first tile */
  first_tx = (float)(floorf (tx_1));
  first_ty = (float)(floorf (ty_1));

  /* Denormalize texture coordinates */
  first_tx = (first_tx * tw);
  first_ty = (first_ty * th);
  tx_1 = (tx_1 * tw);
  ty_1 = (ty_1 * th);
  tx_2 = (tx_2 * tw);
  ty_2 = (ty_2 * th);

  /* Quad coordinate of the first tile */
  first_qx = x_1 - (tx_1 - first_tx) * tqx;
  first_qy = y_1 - (ty_1 - first_ty) * tqy;


  /* Iterate until whole quad height covered */
  for (_cogl_span_iter_begin (&iter_y, tex->slice_y_spans,
			      first_ty, ty_1, ty_2) ;
       !_cogl_span_iter_end  (&iter_y) ;
       _cogl_span_iter_next  (&iter_y) )
    {
      float tex_coords[4];

      /* Discard slices out of quad early */
      if (!iter_y.intersects) continue;

      /* Span-quad intersection in quad coordinates */
      slice_qy1 = first_qy + (iter_y.intersect_start - first_ty) * tqy;

      slice_qy2 = first_qy + (iter_y.intersect_end - first_ty) * tqy;

      /* Localize slice texture coordinates */
      slice_ty1 = iter_y.intersect_start - iter_y.pos;
      slice_ty2 = iter_y.intersect_end - iter_y.pos;

      /* Normalize texture coordinates to current slice
         (rectangle texture targets take denormalized) */
#if HAVE_COGL_GL
      if (tex->gl_target != CGL_TEXTURE_RECTANGLE_ARB)
#endif
        {
          slice_ty1 /= iter_y.span->size;
          slice_ty2 /= iter_y.span->size;
        }

      /* Iterate until whole quad width covered */
      for (_cogl_span_iter_begin (&iter_x, tex->slice_x_spans,
				  first_tx, tx_1, tx_2) ;
	   !_cogl_span_iter_end  (&iter_x) ;
	   _cogl_span_iter_next  (&iter_x) )
        {
	  /* Discard slices out of quad early */
	  if (!iter_x.intersects) continue;

	  /* Span-quad intersection in quad coordinates */
	  slice_qx1 = first_qx + (iter_x.intersect_start - first_tx) * tqx;

	  slice_qx2 = first_qx + (iter_x.intersect_end - first_tx) * tqx;

	  /* Localize slice texture coordinates */
	  slice_tx1 = iter_x.intersect_start - iter_x.pos;
	  slice_tx2 = iter_x.intersect_end - iter_x.pos;

	  /* Normalize texture coordinates to current slice
             (rectangle texture targets take denormalized) */
#if HAVE_COGL_GL
          if (tex->gl_target != CGL_TEXTURE_RECTANGLE_ARB)
#endif
            {
              slice_tx1 /= iter_x.span->size;
              slice_tx2 /= iter_x.span->size;
            }

#if COGL_DEBUG
	  printf("~~~~~ slice (%d,%d)\n", iter_x.index, iter_y.index);
	  printf("qx1: %f\n",  (slice_qx1));
	  printf("qy1: %f\n",  (slice_qy1));
	  printf("qx2: %f\n",  (slice_qx2));
	  printf("qy2: %f\n",  (slice_qy2));
	  printf("tx1: %f\n",  (slice_tx1));
	  printf("ty1: %f\n",  (slice_ty1));
	  printf("tx2: %f\n",  (slice_tx2));
	  printf("ty2: %f\n",  (slice_ty2));
#endif

	  /* Pick and bind opengl texture object */
	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint,
				     iter_y.index * iter_x.array->len +
				     iter_x.index);

          tex_coords[0] = slice_tx1;
          tex_coords[1] = slice_ty1;
          tex_coords[2] = slice_tx2;
          tex_coords[3] = slice_ty2;
          _cogl_journal_log_quad (slice_qx1,
                                  slice_qy1,
                                  slice_qx2,
                                  slice_qy2,
                                  material,
                                  1, /* one layer */
                                  0, /* don't need to use fallbacks */
                                  gl_handle, /* replace the layer0 texture */
                                  tex_coords,
                                  4);
	}
    }
}

static gboolean
_cogl_multitexture_unsliced_quad (float        x_1,
                                  float        y_1,
                                  float        x_2,
                                  float        y_2,
                                  CoglHandle   material,
                                  gint         n_layers,
                                  guint32      fallback_mask,
                                  const float *user_tex_coords,
                                  gint         user_tex_coords_len)
{
  float   *final_tex_coords = alloca (sizeof (float) * 4 * n_layers);
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
      /* CoglLayerInfo   *layer_info; */
      CoglHandle         tex_handle;
      CoglTexture       *tex;
      const float       *in_tex_coords;
      float             *out_tex_coords;
      CoglTexSliceSpan  *x_span;
      CoglTexSliceSpan  *y_span;

      /* layer_info = &layers[i]; */

      /* FIXME - we shouldn't be checking this stuff if layer_info->gl_texture
       * already == 0 */

      tex_handle = cogl_material_layer_get_texture (layer);
      tex = _cogl_texture_pointer_from_handle (tex_handle);

      in_tex_coords = &user_tex_coords[i * 4];
      out_tex_coords = &final_tex_coords[i * 4];


      /* If the texture has waste or we are using GL_TEXTURE_RECT we
       * can't handle texture repeating so we check that the texture
       * coords lie in the range [0,1].
       *
       * NB: We already know that no texture matrix is being used
       * if the texture has waste since we validated that early on.
       * TODO: check for a texture matrix in the GL_TEXTURE_RECT
       * case.
       */
      if ((
#if HAVE_COGL_GL
           tex->gl_target == GL_TEXTURE_RECTANGLE_ARB ||
#endif
           _cogl_texture_span_has_waste (tex, 0, 0))
          && i < user_tex_coords_len / 4
          && (in_tex_coords[0] < 0 || in_tex_coords[0] > 1.0
              || in_tex_coords[1] < 0 || in_tex_coords[1] > 1.0
              || in_tex_coords[2] < 0 || in_tex_coords[2] > 1.0
              || in_tex_coords[3] < 0 || in_tex_coords[3] > 1.0))
        {
          if (i == 0)
            {
              if (n_layers > 1)
                {
                  static gboolean warning_seen = FALSE;
                  if (!warning_seen)
                    g_warning ("Skipping layers 1..n of your material since "
                               "the first layer has waste and you supplied "
                               "texture coordinates outside the range [0,1]. "
                               "We don't currently support any "
                               "multi-texturing using textures with waste "
                               "when repeating is necissary so we are "
                               "falling back to sliced textures assuming "
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
                           "consisting of a texture with waste since "
                           "you have supplied texture coords outside "
                           "the range [0,1] (unsupported when "
                           "multi-texturing)", i);
              warning_seen = TRUE;

              /* NB: marking for fallback will replace the layer with
               * a default transparent texture */
              fallback_mask |= (1 << i);
            }
        }


      /*
       * Setup the texture unit...
       */

      /* NB: The user might not have supplied texture coordinates for all
       * layers... */
      if (i < (user_tex_coords_len / 4))
        {
          GLenum wrap_mode;

          /* If the texture coords are all in the range [0,1] then we want to
             clamp the coords to the edge otherwise it can pull in edge pixels
             from the wrong side when scaled */
          if (in_tex_coords[0] >= 0 && in_tex_coords[0] <= 1.0
              && in_tex_coords[1] >= 0 && in_tex_coords[1] <= 1.0
              && in_tex_coords[2] >= 0 && in_tex_coords[2] <= 1.0
              && in_tex_coords[3] >= 0 && in_tex_coords[3] <= 1.0)
            wrap_mode = GL_CLAMP_TO_EDGE;
          else
            wrap_mode = GL_REPEAT;

          memcpy (out_tex_coords, in_tex_coords, sizeof (GLfloat) * 4);

          _cogl_texture_set_wrap_mode_parameter (tex, wrap_mode);
        }
      else
        {
          out_tex_coords[0] = 0; /* tx_1 */
          out_tex_coords[1] = 0; /* ty_1 */
          out_tex_coords[2] = 1.0; /* tx_2 */
          out_tex_coords[3] = 1.0; /* ty_2 */

          _cogl_texture_set_wrap_mode_parameter (tex, GL_CLAMP_TO_EDGE);
        }

      /* Don't include the waste in the texture coordinates */
      x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, 0);
      y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, 0);

      out_tex_coords[0] =
        out_tex_coords[0] * (x_span->size - x_span->waste) / x_span->size;
      out_tex_coords[1] =
        out_tex_coords[1] * (y_span->size - y_span->waste) / y_span->size;
      out_tex_coords[2] =
        out_tex_coords[2] * (x_span->size - x_span->waste) / x_span->size;
      out_tex_coords[3] =
        out_tex_coords[3] * (y_span->size - y_span->waste) / y_span->size;

#if HAVE_COGL_GL
      /* Denormalize texture coordinates for rectangle textures */
      if (tex->gl_target == GL_TEXTURE_RECTANGLE_ARB)
        {
          out_tex_coords[0] *= x_span->size;
          out_tex_coords[1] *= y_span->size;
          out_tex_coords[2] *= x_span->size;
          out_tex_coords[3] *= y_span->size;
        }
#endif
    }

  _cogl_journal_log_quad (x_1,
                          y_1,
                          x_2,
                          y_2,
                          material,
                          n_layers,
                          fallback_mask,
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
  guint32        fallback_mask = 0;
  gboolean	 all_use_sliced_quad_fallback = FALSE;
  int		 i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_clip_ensure ();

  material = ctx->source_material;

  layers = cogl_material_get_layers (material);
  n_layers = g_list_length ((GList *)layers);

  /*
   * Validate all the layers of the current source material...
   */

  for (tmp = layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle     layer = tmp->data;
      CoglHandle     tex_handle = cogl_material_layer_get_texture (layer);
      CoglTexture   *texture = _cogl_texture_pointer_from_handle (tex_handle);
      gulong         flags;

      if (cogl_material_layer_get_type (layer)
	  != COGL_MATERIAL_LAYER_TYPE_TEXTURE)
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
              fallback_mask = ~1; /* fallback all except the first layer */
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
              fallback_mask |= (1 << i);
	      continue;
            }
	}

      /* We don't support multi texturing using textures with any waste if the
       * user has supplied a custom texture matrix, since we don't know if
       * the result will end up trying to texture from the waste area. */
      flags = cogl_material_layer_get_flags (layer);
      if (flags & COGL_MATERIAL_LAYER_FLAG_HAS_USER_MATRIX
          && _cogl_texture_span_has_waste (texture, 0, 0))
        {
          static gboolean warning_seen = FALSE;
          if (!warning_seen)
            g_warning ("Skipping layer %d of your material consisting of a "
                       "texture with waste since you have supplied a custom "
                       "texture matrix and the result may try to sample from "
                       "the waste area of your texture.", i);
          warning_seen = TRUE;

          /* NB: marking for fallback will replace the layer with
           * a default transparent texture */
          fallback_mask |= (1 << i);
          continue;
        }
    }

  /*
   * Emit geometry for each of the rectangles...
   */

  for (i = 0; i < n_rects; i++)
    {
      if (all_use_sliced_quad_fallback
          || !_cogl_multitexture_unsliced_quad (rects[i].x_1, rects[i].y_1,
                                                rects[i].x_2, rects[i].y_2,
                                                material,
                                                n_layers,
                                                fallback_mask,
                                                rects[i].tex_coords,
                                                rects[i].tex_coords_len))
        {
          CoglHandle   first_layer, tex_handle;
          CoglTexture *texture;

          first_layer = layers->data;
          tex_handle = cogl_material_layer_get_texture (first_layer);
          texture = _cogl_texture_pointer_from_handle (tex_handle);
          if (rects[i].tex_coords)
            _cogl_texture_sliced_quad (texture,
                                       material,
                                       rects[i].x_1, rects[i].y_1,
                                       rects[i].x_2, rects[i].y_2,
                                       rects[i].tex_coords[0],
                                       rects[i].tex_coords[1],
                                       rects[i].tex_coords[2],
                                       rects[i].tex_coords[3]);
          else
            _cogl_texture_sliced_quad (texture,
                                       material,
                                       rects[i].x_1, rects[i].y_1,
                                       rects[i].x_2, rects[i].y_2,
                                       0.0f, 0.0f, 1.0f, 1.0f);
        }
    }

  _cogl_journal_flush ();
}

void
cogl_rectangles (const float *verts,
                 guint        n_rects)
{
  struct _CoglMutiTexturedRect rects[n_rects];
  int i;

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
  struct _CoglMutiTexturedRect rects[n_rects];
  int i;

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

static void
_cogl_texture_sliced_polygon (CoglTextureVertex *vertices,
                              guint              n_vertices,
                              guint              stride,
                              gboolean           use_color)
{
  const GList         *layers;
  CoglHandle           layer0;
  CoglHandle           tex_handle;
  CoglTexture         *tex;
  CoglTexSliceSpan    *y_span, *x_span;
  int                  x, y, tex_num, i;
  GLuint               gl_handle;
  GLfloat             *v;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We can assume in this case that we have at least one layer in the
   * material that corresponds to a sliced cogl texture */
  layers = cogl_material_get_layers (ctx->source_material);
  layer0 = (CoglHandle)layers->data;
  tex_handle = cogl_material_layer_get_texture (layer0);
  tex = _cogl_texture_pointer_from_handle (tex_handle);

  v = (GLfloat *)ctx->logged_vertices->data;
  for (i = 0; i < n_vertices; i++)
    {
      GLfloat *c;

      v[0] = vertices[i].x;
      v[1] = vertices[i].y;
      v[2] = vertices[i].z;

      /* NB: [X,Y,Z,TX,TY,R,G,B,A,...] */
      c = v + 5;
      c[0] = cogl_color_get_red_byte (&vertices[i].color);
      c[1] = cogl_color_get_green_byte (&vertices[i].color);
      c[2] = cogl_color_get_blue_byte (&vertices[i].color);
      c[3] = cogl_color_get_alpha_byte (&vertices[i].color);

      v += stride;
    }

  /* Render all of the slices with the full geometry but use a
     transparent border color so that any part of the texture not
     covered by the slice will be ignored */
  tex_num = 0;
  for (y = 0; y < tex->slice_y_spans->len; y++)
    {
      y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, y);

      for (x = 0; x < tex->slice_x_spans->len; x++)
	{
	  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, x);

	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint, tex_num++);

	  /* Convert the vertices into an array of GLfloats ready to pass to
	     OpenGL */
          v = (GLfloat *)ctx->logged_vertices->data;
	  for (i = 0; i < n_vertices; i++)
	    {
              GLfloat *t;
              float    tx, ty;

              tx = ((vertices[i].tx
                     - ((float)(x_span->start)
                        / tex->bitmap.width))
                    * tex->bitmap.width / x_span->size);
              ty = ((vertices[i].ty
                     - ((float)(y_span->start)
                        / tex->bitmap.height))
                    * tex->bitmap.height / y_span->size);

#if HAVE_COGL_GL
              /* Scale the coordinates up for rectangle textures */
              if (tex->gl_target == CGL_TEXTURE_RECTANGLE_ARB)
                {
                  tx *= x_span->size;
                  ty *= y_span->size;
                }
#endif

              /* NB: [X,Y,Z,TX,TY,R,G,B,A,...] */
              t = v + 3;
	      t[0] = tx;
	      t[1] = ty;

              v += stride;
	    }

          cogl_material_flush_gl_state (ctx->source_material,
                                        COGL_MATERIAL_FLUSH_DISABLE_MASK,
                                        (guint32)~1, /* disable all except the
                                                        first layer */
                                        COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE,
                                        gl_handle,
                                        NULL);
          _cogl_current_matrix_state_flush ();

	  GE( glDrawArrays (GL_TRIANGLE_FAN, 0, n_vertices) );
	}
    }
}

static void
_cogl_multitexture_unsliced_polygon (CoglTextureVertex *vertices,
                                     guint              n_vertices,
                                     guint              n_layers,
                                     guint              stride,
                                     gboolean           use_color,
                                     guint32            fallback_mask)
{
  CoglHandle           material;
  const GList         *layers;
  int                  i;
  GList               *tmp;
  CoglTexSliceSpan    *y_span, *x_span;
  GLfloat             *v;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);


  material = ctx->source_material;
  layers = cogl_material_get_layers (material);

  /* Convert the vertices into an array of GLfloats ready to pass to
     OpenGL */
  for (v = (GLfloat *)ctx->logged_vertices->data, i = 0;
       i < n_vertices;
       v += stride, i++)
    {
      GLfloat *c;
      int      j;

      /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
      v[0] = vertices[i].x;
      v[1] = vertices[i].y;
      v[2] = vertices[i].z;

      for (tmp = (GList *)layers, j = 0; tmp != NULL; tmp = tmp->next, j++)
        {
          CoglHandle   layer = (CoglHandle)tmp->data;
          CoglHandle   tex_handle;
          CoglTexture *tex;
          GLfloat     *t;
          float        tx, ty;

          tex_handle = cogl_material_layer_get_texture (layer);
          tex = _cogl_texture_pointer_from_handle (tex_handle);

          y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, 0);
          x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, 0);

          tx = ((vertices[i].tx
                 - ((float)(x_span->start)
                    / tex->bitmap.width))
                * tex->bitmap.width / x_span->size);
          ty = ((vertices[i].ty
                 - ((float)(y_span->start)
                    / tex->bitmap.height))
                * tex->bitmap.height / y_span->size);

#if HAVE_COGL_GL
          /* Scale the coordinates up for rectangle textures */
          if (tex->gl_target == CGL_TEXTURE_RECTANGLE_ARB)
            {
              tx *= x_span->size;
              ty *= y_span->size;
            }
#endif

          /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
          t = v + 3 + 2 * j;
          t[0] = tx;
          t[1] = ty;
        }

      /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
      c = v + 3 + 2 * n_layers;
      c[0] = cogl_color_get_red_float (&vertices[i].color);
      c[1] = cogl_color_get_green_float (&vertices[i].color);
      c[2] = cogl_color_get_blue_float (&vertices[i].color);
      c[3] = cogl_color_get_alpha_float (&vertices[i].color);
    }

  cogl_material_flush_gl_state (ctx->source_material,
                                COGL_MATERIAL_FLUSH_FALLBACK_MASK,
                                fallback_mask,
                                NULL);
  _cogl_current_matrix_state_flush ();

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
  guint32              fallback_mask = 0;
  int                  i;
  gulong               enable_flags;
  guint                stride;
  gsize                stride_bytes;
  GLfloat             *v;
  int                  prev_n_texcoord_arrays_enabled;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_clip_ensure ();

  material = ctx->source_material;
  layers = cogl_material_get_layers (ctx->source_material);
  n_layers = g_list_length ((GList *)layers);

  for (tmp = (GList *)layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle   layer = (CoglHandle)tmp->data;
      CoglHandle   tex_handle = cogl_material_layer_get_texture (layer);
      CoglTexture *tex = _cogl_texture_pointer_from_handle (tex_handle);

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

          if (tex->min_filter != GL_NEAREST || tex->mag_filter != GL_NEAREST)
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
          /* Temporarily change the wrapping mode on all of the slices to use
           * a transparent border
           * XXX: it's doesn't look like we save/restore this, like the comment
           * implies? */
          _cogl_texture_set_wrap_mode_parameter (tex, GL_CLAMP_TO_BORDER);
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

          fallback_mask |= (1 << i);
          continue;
        }
    }

  /* Our data is arranged like:
   * [X, Y, Z, TX0, TY0, TX1, TY1..., R, G, B, A,...] */
  stride = 3 + (2 * n_layers) + (use_color ? 4 : 0);
  stride_bytes = stride * sizeof (GLfloat);

  /* Make sure there is enough space in the global vertex
     array. This is used so we can render the polygon with a single
     call to OpenGL but still support any number of vertices */
  g_array_set_size (ctx->logged_vertices, n_vertices * stride);
  v = (GLfloat *)ctx->logged_vertices->data;

  /* Prepare GL state */
  enable_flags = COGL_ENABLE_VERTEX_ARRAY;
  enable_flags |= cogl_material_get_cogl_enable_flags (ctx->source_material);

  if (ctx->enable_backface_culling)
    enable_flags |= COGL_ENABLE_BACKFACE_CULLING;

  if (use_color)
    {
      enable_flags |= COGL_ENABLE_COLOR_ARRAY;
      GE( glColorPointer (4, GL_FLOAT,
                          stride_bytes,
                          /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
                          v + 3 + 2 * n_layers) );
    }

  cogl_enable (enable_flags);

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
    _cogl_texture_sliced_polygon (vertices,
                                  n_vertices,
                                  stride,
                                  use_color);
  else
    _cogl_multitexture_unsliced_polygon (vertices,
                                         n_vertices,
                                         n_layers,
                                         stride,
                                         use_color,
                                         fallback_mask);

  /* Reset the size of the logged vertex array because rendering
     rectangles expects it to start at 0 */
  g_array_set_size (ctx->logged_vertices, 0);
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

  cogl_clip_ensure ();
  
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
  
  cogl_clip_ensure ();

  if (ctx->path_nodes->len == 0)
    return;
  
  _cogl_path_stroke_nodes();
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
