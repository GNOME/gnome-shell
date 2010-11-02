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
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-vertex-buffer-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-profile.h"
#include "cogl-vertex-attribute-private.h"

#include <string.h>
#include <gmodule.h>
#include <math.h>

/* XXX NB:
 * Our journal's vertex data is arranged as follows:
 * 4 vertices per quad:
 *    2 or 3 GLfloats per position (3 when doing software transforms)
 *    4 RGBA GLubytes,
 *    2 GLfloats per tex coord * n_layers
 *
 * Where n_layers corresponds to the number of pipeline layers enabled
 *
 * To avoid frequent changes in the stride of our vertex data we always pad
 * n_layers to be >= 2
 *
 * When we are transforming quads in software we need to also track the z
 * coordinate of transformed vertices.
 *
 * So for a given number of layers this gets the stride in 32bit words:
 */
#define SW_TRANSFORM      (!(cogl_debug_flags & \
                             COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM))
#define POS_STRIDE        (SW_TRANSFORM ? 3 : 2) /* number of 32bit words */
#define N_POS_COMPONENTS  POS_STRIDE
#define COLOR_STRIDE      1 /* number of 32bit words */
#define TEX_STRIDE        2 /* number of 32bit words */
#define MIN_LAYER_PADING  2
#define GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS(N_LAYERS) \
  (POS_STRIDE + COLOR_STRIDE + \
   TEX_STRIDE * (N_LAYERS < MIN_LAYER_PADING ? MIN_LAYER_PADING : N_LAYERS))

typedef struct _CoglJournalFlushState
{
  CoglVertexArray     *vertex_array;
  GArray              *attributes;
  int                  current_attribute;

  gsize                stride;
  size_t               array_offset;
  GLuint               current_vertex;
#ifndef HAVE_COGL_GL
  CoglIndices         *indices;
  gsize                indices_type_size;
#endif
  CoglMatrixStack     *modelview_stack;
  CoglMatrixStack     *projection_stack;

  CoglPipeline        *source;
} CoglJournalFlushState;

typedef void (*CoglJournalBatchCallback) (CoglJournalEntry *start,
                                          int n_entries,
                                          void *data);
typedef gboolean (*CoglJournalBatchTest) (CoglJournalEntry *entry0,
                                          CoglJournalEntry *entry1);

void
_cogl_journal_dump_quad_vertices (guint8 *data, int n_layers)
{
  gsize stride = GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS (n_layers);
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_print ("n_layers = %d; stride = %d; pos stride = %d; color stride = %d; "
           "tex stride = %d; stride in bytes = %d\n",
           n_layers, (int)stride, POS_STRIDE, COLOR_STRIDE,
           TEX_STRIDE, (int)stride * 4);

  for (i = 0; i < 4; i++)
    {
      float *v = (float *)data + (i * stride);
      guint8 *c = data + (POS_STRIDE * 4) + (i * stride * 4);
      int j;

      if (G_UNLIKELY (cogl_debug_flags &
                      COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM))
        g_print ("v%d: x = %f, y = %f, rgba=0x%02X%02X%02X%02X",
                 i, v[0], v[1], c[0], c[1], c[2], c[3]);
      else
        g_print ("v%d: x = %f, y = %f, z = %f, rgba=0x%02X%02X%02X%02X",
                 i, v[0], v[1], v[2], c[0], c[1], c[2], c[3]);
      for (j = 0; j < n_layers; j++)
        {
          float *t = v + POS_STRIDE + COLOR_STRIDE + TEX_STRIDE * j;
          g_print (", tx%d = %f, ty%d = %f", j, t[0], j, t[1]);
        }
      g_print ("\n");
    }
}

void
_cogl_journal_dump_quad_batch (guint8 *data, int n_layers, int n_quads)
{
  gsize byte_stride = GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS (n_layers) * 4;
  int i;

  g_print ("_cogl_journal_dump_quad_batch: n_layers = %d, n_quads = %d\n",
           n_layers, n_quads);
  for (i = 0; i < n_quads; i++)
    _cogl_journal_dump_quad_vertices (data + byte_stride * 4 * i, n_layers);
}

static void
batch_and_call (CoglJournalEntry *entries,
                int n_entries,
                CoglJournalBatchTest can_batch_callback,
                CoglJournalBatchCallback batch_callback,
                void *data)
{
  int i;
  int batch_len = 1;
  CoglJournalEntry *batch_start = entries;

  if (n_entries < 1)
    return;

  for (i = 1; i < n_entries; i++)
    {
      CoglJournalEntry *entry0 = &entries[i - 1];
      CoglJournalEntry *entry1 = entry0 + 1;

      if (can_batch_callback (entry0, entry1))
        {
          batch_len++;
          continue;
        }

      batch_callback (batch_start, batch_len, data);

      batch_start = entry1;
      batch_len = 1;
    }

  /* The last batch... */
  batch_callback (batch_start, batch_len, data);
}

static void
_cogl_journal_flush_modelview_and_entries (CoglJournalEntry *batch_start,
                                           int               batch_len,
                                           void             *data)
{
  CoglJournalFlushState *state = data;
  CoglVertexAttribute **attributes;
  COGL_STATIC_TIMER (time_flush_modelview_and_entries,
                     "flush: pipeline+entries", /* parent */
                     "flush: modelview+entries",
                     "The time spent flushing modelview + entries",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context, time_flush_modelview_and_entries);

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_BATCHING))
    g_print ("BATCHING:     modelview batch len = %d\n", batch_len);

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM))
    {
      _cogl_matrix_stack_set (state->modelview_stack,
                              &batch_start->model_view);
      _cogl_matrix_stack_flush_to_gl (state->modelview_stack,
                                      COGL_MATRIX_MODELVIEW);
    }

  attributes = (CoglVertexAttribute **)state->attributes->data;
  cogl_push_source (state->source);

#ifdef HAVE_COGL_GL

  /* XXX: it's rather evil that we sneak in the GL_QUADS enum here... */
  _cogl_draw_vertex_attributes_array (GL_QUADS,
                                      state->current_vertex, batch_len * 4,
                                      attributes);

#else /* HAVE_COGL_GL */
  if (batch_len > 1)
    {
      _cogl_draw_indexed_vertex_attributes_array (COGL_VERTICES_MODE_TRIANGLES,
                                                  state->current_vertex * 6 / 4,
                                                  batch_len * 6,
                                                  state->indices,
                                                  attributes);
    }
  else
    {
      _cogl_draw_vertex_attributes_array (COGL_VERTICES_MODE_TRIANGLE_FAN,
                                          state->current_vertex, 4,
                                          attributes);
    }
#endif

  /* DEBUGGING CODE XXX: This path will cause all rectangles to be
   * drawn with a coloured outline. Each batch will be rendered with
   * the same color. This may e.g. help with debugging texture slicing
   * issues, visually seeing what is batched and debugging blending
   * issues, plus it looks quite cool.
   */
  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_RECTANGLES))
    {
      static CoglPipeline *outline = NULL;
      guint8 color_intensity;
      int i;
      CoglVertexAttribute *loop_attributes[2];

      _COGL_GET_CONTEXT (ctxt, NO_RETVAL);

      if (outline == NULL)
        outline = cogl_pipeline_new ();

      /* The least significant three bits represent the three
         components so that the order of colours goes red, green,
         yellow, blue, magenta, cyan. Black and white are skipped. The
         next two bits give four scales of intensity for those colours
         in the order 0xff, 0xcc, 0x99, and 0x66. This gives a total
         of 24 colours. If there are more than 24 batches on the stage
         then it will wrap around */
      color_intensity = 0xff - 0x33 * (ctxt->journal_rectangles_color >> 3);
      cogl_pipeline_set_color4ub (outline,
                                  (ctxt->journal_rectangles_color & 1) ?
                                  color_intensity : 0,
                                  (ctxt->journal_rectangles_color & 2) ?
                                  color_intensity : 0,
                                  (ctxt->journal_rectangles_color & 4) ?
                                  color_intensity : 0,
                                  0xff);
      cogl_set_source (outline);

      loop_attributes[0] = attributes[0]; /* we just want the position */
      loop_attributes[1] = NULL;
      for (i = 0; i < batch_len; i++)
        _cogl_draw_vertex_attributes_array (COGL_VERTICES_MODE_LINE_LOOP,
                                            4 * i + state->current_vertex, 4,
                                            loop_attributes);

      /* Go to the next color */
      do
        ctxt->journal_rectangles_color = ((ctxt->journal_rectangles_color + 1) &
                                          ((1 << 5) - 1));
      /* We don't want to use black or white */
      while ((ctxt->journal_rectangles_color & 0x07) == 0
             || (ctxt->journal_rectangles_color & 0x07) == 0x07);
    }

  state->current_vertex += (4 * batch_len);

  cogl_pop_source ();

  COGL_TIMER_STOP (_cogl_uprof_context, time_flush_modelview_and_entries);
}

static gboolean
compare_entry_modelviews (CoglJournalEntry *entry0,
                          CoglJournalEntry *entry1)
{
  /* Batch together quads with the same model view matrix */

  /* FIXME: this is nasty, there are much nicer ways to track this
   * (at the add_quad_vertices level) without resorting to a memcmp!
   *
   * E.g. If the cogl-current-matrix code maintained an "age" for
   * the modelview matrix we could simply check in add_quad_vertices
   * if the age has increased, and if so record the change as a
   * boolean in the journal.
   */

  if (memcmp (&entry0->model_view, &entry1->model_view,
              sizeof (GLfloat) * 16) == 0)
    return TRUE;
  else
    return FALSE;
}

/* At this point we have a run of quads that we know have compatible
 * pipelines, but they may not all have the same modelview matrix */
static void
_cogl_journal_flush_pipeline_and_entries (CoglJournalEntry *batch_start,
                                          int               batch_len,
                                          void             *data)
{
  CoglJournalFlushState *state = data;
  COGL_STATIC_TIMER (time_flush_pipeline_entries,
                     "flush: texcoords+pipeline+entries", /* parent */
                     "flush: pipeline+entries",
                     "The time spent flushing pipeline + entries",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context, time_flush_pipeline_entries);

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_BATCHING))
    g_print ("BATCHING:    pipeline batch len = %d\n", batch_len);

  state->source = batch_start->pipeline;

  /* If we haven't transformed the quads in software then we need to also break
   * up batches according to changes in the modelview matrix... */
  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM))
    {
      batch_and_call (batch_start,
                      batch_len,
                      compare_entry_modelviews,
                      _cogl_journal_flush_modelview_and_entries,
                      data);
    }
  else
    _cogl_journal_flush_modelview_and_entries (batch_start, batch_len, data);

  COGL_TIMER_STOP (_cogl_uprof_context, time_flush_pipeline_entries);
}

static gboolean
compare_entry_pipelines (CoglJournalEntry *entry0, CoglJournalEntry *entry1)
{
  /* batch rectangles using compatible pipelines */

  /* XXX: _cogl_pipeline_equal may give false negatives since it avoids
   * deep comparisons as an optimization. It aims to compare enough so
   * that we that we are able to batch the 90% common cases, but may not
   * look at less common differences. */
  if (_cogl_pipeline_equal (entry0->pipeline,
                            entry1->pipeline,
                            TRUE))
    return TRUE;
  else
    return FALSE;
}

/* Since the stride may not reflect the number of texture layers in use
 * (due to padding) we deal with texture coordinate offsets separately
 * from vertex and color offsets... */
static void
_cogl_journal_flush_texcoord_vbo_offsets_and_entries (
                                          CoglJournalEntry *batch_start,
                                          int               batch_len,
                                          void             *data)
{
  CoglJournalFlushState *state = data;
  int                    i;
  COGL_STATIC_TIMER (time_flush_texcoord_pipeline_entries,
                     "flush: vbo+texcoords+pipeline+entries", /* parent */
                     "flush: texcoords+pipeline+entries",
                     "The time spent flushing texcoord offsets + pipeline "
                     "+ entries",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context, time_flush_texcoord_pipeline_entries);

  /* NB: attributes 0 and 1 are position and color */

  for (i = 2; i < state->attributes->len; i++)
    cogl_object_unref (g_array_index (state->attributes,
                                      CoglVertexAttribute *, i));

  g_array_set_size (state->attributes, batch_start->n_layers + 2);

  for (i = 0; i < batch_start->n_layers; i++)
    {
      CoglVertexAttribute **attribute_entry =
        &g_array_index (state->attributes, CoglVertexAttribute *, i + 2);
      const char *names[] = {
          "cogl_tex_coord0_in",
          "cogl_tex_coord1_in",
          "cogl_tex_coord2_in",
          "cogl_tex_coord3_in",
          "cogl_tex_coord4_in",
          "cogl_tex_coord5_in",
          "cogl_tex_coord6_in",
          "cogl_tex_coord7_in"
      };
      char *name;

      /* XXX NB:
       * Our journal's vertex data is arranged as follows:
       * 4 vertices per quad:
       *    2 or 3 floats per position (3 when doing software transforms)
       *    4 RGBA bytes,
       *    2 floats per tex coord * n_layers
       * (though n_layers may be padded; see definition of
       *  GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS for details)
       */
      name = i < 8 ? (char *)names[i] :
        g_strdup_printf ("cogl_tex_coord%d_in", i);

      /* XXX: it may be worth having some form of static initializer for
       * attributes... */
      *attribute_entry =
        cogl_vertex_attribute_new (state->vertex_array,
                                   name,
                                   state->stride,
                                   state->array_offset +
                                      (POS_STRIDE + COLOR_STRIDE) * 4 +
                                      TEX_STRIDE * 4 * i,
                                   2,
                                   COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);

      if (i >= 8)
        g_free (name);
    }

  batch_and_call (batch_start,
                  batch_len,
                  compare_entry_pipelines,
                  _cogl_journal_flush_pipeline_and_entries,
                  data);
  COGL_TIMER_STOP (_cogl_uprof_context, time_flush_texcoord_pipeline_entries);
}

static gboolean
compare_entry_n_layers (CoglJournalEntry *entry0, CoglJournalEntry *entry1)
{
  if (entry0->n_layers == entry1->n_layers)
    return TRUE;
  else
    return FALSE;
}

/* At this point we know the stride has changed from the previous batch
 * of journal entries */
static void
_cogl_journal_flush_vbo_offsets_and_entries (CoglJournalEntry *batch_start,
                                             int               batch_len,
                                             void             *data)
{
  CoglJournalFlushState   *state = data;
  gsize                    stride;
  int                      i;
  CoglVertexAttribute    **attribute_entry;
  COGL_STATIC_TIMER (time_flush_vbo_texcoord_pipeline_entries,
                     "flush: clip+vbo+texcoords+pipeline+entries", /* parent */
                     "flush: vbo+texcoords+pipeline+entries",
                     "The time spent flushing vbo + texcoord offsets + "
                     "pipeline + entries",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context,
                    time_flush_vbo_texcoord_pipeline_entries);

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_BATCHING))
    g_print ("BATCHING:   vbo offset batch len = %d\n", batch_len);

  /* XXX NB:
   * Our journal's vertex data is arranged as follows:
   * 4 vertices per quad:
   *    2 or 3 GLfloats per position (3 when doing software transforms)
   *    4 RGBA GLubytes,
   *    2 GLfloats per tex coord * n_layers
   * (though n_layers may be padded; see definition of
   *  GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS for details)
   */
  stride = GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS (batch_start->n_layers);
  stride *= sizeof (float);
  state->stride = stride;

  for (i = 0; i < state->attributes->len; i++)
    cogl_object_unref (g_array_index (state->attributes,
                                      CoglVertexAttribute *, i));

  g_array_set_size (state->attributes, 2);

  attribute_entry =
    &g_array_index (state->attributes, CoglVertexAttribute *, 0);
  *attribute_entry =
    cogl_vertex_attribute_new (state->vertex_array,
                               "cogl_position_in",
                               stride,
                               state->array_offset,
                               N_POS_COMPONENTS,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);

  attribute_entry =
    &g_array_index (state->attributes, CoglVertexAttribute *, 1);
  *attribute_entry =
    cogl_vertex_attribute_new (state->vertex_array,
                               "cogl_color_in",
                               stride,
                               state->array_offset + (POS_STRIDE * 4),
                               4,
                               COGL_VERTEX_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

#ifndef HAVE_COGL_GL
  state->indices = cogl_get_rectangle_indices (batch_len);
#endif

  /* We only create new VertexAttributes when the stride within the
   * VertexArray changes. (due to a change in the number of pipeline layers)
   * While the stride remains constant we walk forward through the above
   * VertexArray using a vertex offset passed to cogl_draw_vertex_attributes
   */
  state->current_vertex = 0;

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_JOURNAL))
    {
      guint8 *verts;

      if (cogl_get_features () & COGL_FEATURE_VBOS)
        verts = ((guint8 *)ctx->logged_vertices->data) +
          (size_t)state->array_offset;
      else
        verts = (guint8 *)state->array_offset;
      _cogl_journal_dump_quad_batch (verts,
                                     batch_start->n_layers,
                                     batch_len);
    }

  batch_and_call (batch_start,
                  batch_len,
                  compare_entry_n_layers,
                  _cogl_journal_flush_texcoord_vbo_offsets_and_entries,
                  data);

  /* progress forward through the VBO containing all our vertices */
  state->array_offset += (stride * 4 * batch_len);
  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_JOURNAL))
    g_print ("new vbo offset = %lu\n", (unsigned long)state->array_offset);

  COGL_TIMER_STOP (_cogl_uprof_context,
                   time_flush_vbo_texcoord_pipeline_entries);
}

static gboolean
compare_entry_strides (CoglJournalEntry *entry0, CoglJournalEntry *entry1)
{
  /* Currently the only thing that affects the stride for our vertex arrays
   * is the number of pipeline layers. We need to update our VBO offsets
   * whenever the stride changes. */
  /* TODO: We should be padding the n_layers == 1 case as if it were
   * n_layers == 2 so we can reduce the need to split batches. */
  if (entry0->n_layers == entry1->n_layers ||
      (entry0->n_layers <= MIN_LAYER_PADING &&
       entry1->n_layers <= MIN_LAYER_PADING))
    return TRUE;
  else
    return FALSE;
}

/* At this point we know the batch has a unique clip stack */
static void
_cogl_journal_flush_clip_stacks_and_entries (CoglJournalEntry *batch_start,
                                             int               batch_len,
                                             void             *data)
{
  CoglJournalFlushState *state = data;

  COGL_STATIC_TIMER (time_flush_clip_stack_pipeline_entries,
                     "Journal Flush", /* parent */
                     "flush: clip+vbo+texcoords+pipeline+entries",
                     "The time spent flushing clip + vbo + texcoord offsets + "
                     "pipeline + entries",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context,
                    time_flush_clip_stack_pipeline_entries);

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_BATCHING))
    g_print ("BATCHING:  clip stack batch len = %d\n", batch_len);

  _cogl_clip_stack_flush (batch_start->clip_stack);

  _cogl_matrix_stack_push (state->modelview_stack);

  /* If we have transformed all our quads at log time then we ensure
   * no further model transform is applied by loading the identity
   * matrix here. We need to do this after flushing the clip stack
   * because the clip stack flushing code can modify the matrix */
  if (G_LIKELY (!(cogl_debug_flags & COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM)))
    {
      _cogl_matrix_stack_load_identity (state->modelview_stack);
      _cogl_matrix_stack_flush_to_gl (state->modelview_stack,
                                      COGL_MATRIX_MODELVIEW);
    }

  /* Setting up the clip state can sometimes also flush the projection
     matrix so we should flush it again. This will be a no-op if the
     clip code didn't modify the projection */
  _cogl_matrix_stack_flush_to_gl (state->projection_stack,
                                  COGL_MATRIX_PROJECTION);

  batch_and_call (batch_start,
                  batch_len,
                  compare_entry_strides,
                  _cogl_journal_flush_vbo_offsets_and_entries, /* callback */
                  data);

  _cogl_matrix_stack_pop (state->modelview_stack);

  COGL_TIMER_STOP (_cogl_uprof_context,
                   time_flush_clip_stack_pipeline_entries);
}

static gboolean
compare_entry_clip_stacks (CoglJournalEntry *entry0, CoglJournalEntry *entry1)
{
  return entry0->clip_stack == entry1->clip_stack;
}

static CoglVertexArray *
upload_vertices (GArray *vertices, CoglJournalFlushState *state)
{
  gsize needed_vbo_len;
  CoglVertexArray *array;
  CoglBuffer *buffer;

  _COGL_GET_CONTEXT (ctx, 0);

  needed_vbo_len = vertices->len * sizeof (float);
  g_assert (needed_vbo_len);

  array = cogl_vertex_array_new (needed_vbo_len);
  buffer = COGL_BUFFER (array);
  cogl_buffer_set_update_hint (buffer, COGL_BUFFER_UPDATE_HINT_STATIC);
  cogl_buffer_set_data (buffer, 0, vertices->data, needed_vbo_len);

  /* As we flush the journal entries in batches we walk forward through the
   * above VBO starting at offset 0... */
  state->array_offset = 0;

  return array;
}

/* XXX NB: When _cogl_journal_flush() returns all state relating
 * to pipelines, all glEnable flags and current matrix state
 * is undefined.
 */
void
_cogl_journal_flush (void)
{
  CoglJournalFlushState state;
  int                   i;
  CoglFramebuffer      *framebuffer;
  CoglMatrixStack      *modelview_stack;
  COGL_STATIC_TIMER (flush_timer,
                     "Mainloop", /* parent */
                     "Journal Flush",
                     "The time spent flushing the Cogl journal",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->journal->len == 0)
    return;

  COGL_TIMER_START (_cogl_uprof_context, flush_timer);

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_BATCHING))
    g_print ("BATCHING: journal len = %d\n", ctx->journal->len);

  state.vertex_array = upload_vertices (ctx->logged_vertices, &state);
  state.attributes = ctx->journal_flush_attributes_array;
  g_array_set_size (ctx->journal_flush_attributes_array, 0);

  framebuffer = _cogl_get_framebuffer ();
  modelview_stack = _cogl_framebuffer_get_modelview_stack (framebuffer);
  state.modelview_stack = modelview_stack;
  state.projection_stack = _cogl_framebuffer_get_projection_stack (framebuffer);

  /* batch_and_call() batches a list of journal entries according to some
   * given criteria and calls a callback once for each determined batch.
   *
   * The process of flushing the journal is staggered to reduce the amount
   * of driver/GPU state changes necessary:
   * 1) We split the entries according to the clip state.
   * 2) We split the entries according to the stride of the vertices:
   *      Each time the stride of our vertex data changes we need to call
   *      gl{Vertex,Color}Pointer to inform GL of new VBO offsets.
   *      Currently the only thing that affects the stride of our vertex data
   *      is the number of pipeline layers.
   * 3) We split the entries explicitly by the number of pipeline layers:
   *      We pad our vertex data when the number of layers is < 2 so that we
   *      can minimize changes in stride. Each time the number of layers
   *      changes we need to call glTexCoordPointer to inform GL of new VBO
   *      offsets.
   * 4) We then split according to compatible Cogl pipelines:
   *      This is where we flush pipeline state
   * 5) Finally we split according to modelview matrix changes:
   *      This is when we finally tell GL to draw something.
   *      Note: Splitting by modelview changes is skipped when are doing the
   *      vertex transformation in software at log time.
   */
  batch_and_call ((CoglJournalEntry *)ctx->journal->data, /* first entry */
                  ctx->journal->len, /* max number of entries to consider */
                  compare_entry_clip_stacks,
                  _cogl_journal_flush_clip_stacks_and_entries, /* callback */
                  &state); /* data */

  for (i = 0; i < ctx->journal->len; i++)
    {
      CoglJournalEntry *entry =
        &g_array_index (ctx->journal, CoglJournalEntry, i);
      _cogl_pipeline_journal_unref (entry->pipeline);
      _cogl_clip_stack_unref (entry->clip_stack);
    }

  g_array_set_size (ctx->journal, 0);
  g_array_set_size (ctx->logged_vertices, 0);

  COGL_TIMER_STOP (_cogl_uprof_context, flush_timer);
}

static void
_cogl_journal_init (void)
{
  /* Here we flush anything that we know must remain constant until the
   * next the the journal is flushed. Note: This lets up flush things
   * that themselves depend on the journal, such as clip state. */

  /* NB: the journal deals with flushing the modelview stack and clip
     state manually */
  _cogl_framebuffer_flush_state (_cogl_get_framebuffer (),
                                 COGL_FRAMEBUFFER_FLUSH_SKIP_MODELVIEW |
                                 COGL_FRAMEBUFFER_FLUSH_SKIP_CLIP_STATE);
}

void
_cogl_journal_log_quad (const float  *position,
                        CoglPipeline *pipeline,
                        int           n_layers,
                        guint32       fallback_layers,
                        GLuint        layer0_override_texture,
                        const CoglPipelineWrapModeOverrides *
                                      wrap_mode_overrides,
                        const float  *tex_coords,
                        unsigned int  tex_coords_len)
{
  gsize            stride;
  gsize            byte_stride;
  int               next_vert;
  GLfloat          *v;
  GLubyte          *c;
  GLubyte          *src_c;
  int               i;
  int               next_entry;
  guint32           disable_layers;
  CoglJournalEntry *entry;
  CoglPipeline     *source;
  CoglPipelineFlushOptions flush_options;
  COGL_STATIC_TIMER (log_timer,
                     "Mainloop", /* parent */
                     "Journal Log",
                     "The time spent logging in the Cogl journal",
                     0 /* no application private data */);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  COGL_TIMER_START (_cogl_uprof_context, log_timer);

  if (ctx->logged_vertices->len == 0)
    _cogl_journal_init ();

  /* The vertex data is logged into a separate array in a layout that can be
   * directly passed to OpenGL
   */

  /* XXX: See definition of GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS for details
   * about how we pack our vertex data */
  stride = GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS (n_layers);
  /* NB: stride is in 32bit words */
  byte_stride = stride * 4;

  next_vert = ctx->logged_vertices->len;
  g_array_set_size (ctx->logged_vertices, next_vert + 4 * stride);
  v = &g_array_index (ctx->logged_vertices, GLfloat, next_vert);
  c = (GLubyte *)(v + POS_STRIDE);

  /* XXX: All the jumping around to fill in this strided buffer doesn't
   * seem ideal. */

  /* XXX: we could defer expanding the vertex data for GL until we come
   * to flushing the journal. */

  /* FIXME: This is a hacky optimization, since it will break if we
   * change the definition of CoglColor: */
  _cogl_pipeline_get_colorubv (pipeline, c);
  src_c = c;
  for (i = 0; i < 3; i++)
    {
      c += byte_stride;
      memcpy (c, src_c, 4);
    }

#define X0 0
#define Y0 1
#define X1 2
#define Y1 3

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM))
    {
      v[0] = position[X0]; v[1] = position[Y0];
      v += stride;
      v[0] = position[X0]; v[1] = position[Y1];
      v += stride;
      v[0] = position[X1]; v[1] = position[Y1];
      v += stride;
      v[0] = position[X1]; v[1] = position[Y0];
    }
  else
    {
      CoglMatrix  mv;
      float       x, y, z, w;

      cogl_get_modelview_matrix (&mv);

      x = position[X0], y = position[Y0], z = 0; w = 1;
      cogl_matrix_transform_point (&mv, &x, &y, &z, &w);
      v[0] = x; v[1] = y; v[2] = z;
      v += stride;
      x = position[X0], y = position[Y1], z = 0; w = 1;
      cogl_matrix_transform_point (&mv, &x, &y, &z, &w);
      v[0] = x; v[1] = y; v[2] = z;
      v += stride;
      x = position[X1], y = position[Y1], z = 0; w = 1;
      cogl_matrix_transform_point (&mv, &x, &y, &z, &w);
      v[0] = x; v[1] = y; v[2] = z;
      v += stride;
      x = position[X1], y = position[Y0], z = 0; w = 1;
      cogl_matrix_transform_point (&mv, &x, &y, &z, &w);
      v[0] = x; v[1] = y; v[2] = z;
    }

#undef X0
#undef Y0
#undef X1
#undef Y1

  for (i = 0; i < n_layers; i++)
    {
      /* XXX: See definition of GET_JOURNAL_VB_STRIDE_FOR_N_LAYERS for details
       * about how we pack our vertex data */
      GLfloat *t = &g_array_index (ctx->logged_vertices, GLfloat,
                                   next_vert +  POS_STRIDE +
                                   COLOR_STRIDE + TEX_STRIDE * i);

      t[0] = tex_coords[i * 4 + 0]; t[1] = tex_coords[i * 4 + 1];
      t += stride;
      t[0] = tex_coords[i * 4 + 0]; t[1] = tex_coords[i * 4 + 3];
      t += stride;
      t[0] = tex_coords[i * 4 + 2]; t[1] = tex_coords[i * 4 + 3];
      t += stride;
      t[0] = tex_coords[i * 4 + 2]; t[1] = tex_coords[i * 4 + 1];
    }

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_JOURNAL))
    {
      g_print ("Logged new quad:\n");
      v = &g_array_index (ctx->logged_vertices, GLfloat, next_vert);
      _cogl_journal_dump_quad_vertices ((guint8 *)v, n_layers);
    }

  next_entry = ctx->journal->len;
  g_array_set_size (ctx->journal, next_entry + 1);
  entry = &g_array_index (ctx->journal, CoglJournalEntry, next_entry);

  entry->n_layers = n_layers;

  source = pipeline;

  if (G_UNLIKELY (ctx->legacy_state_set))
    {
      source = cogl_pipeline_copy (pipeline);
      _cogl_pipeline_apply_legacy_state (source);
    }

  flush_options.flags = 0;
  if (G_UNLIKELY (cogl_pipeline_get_n_layers (pipeline) != n_layers))
    {
      disable_layers = (1 << n_layers) - 1;
      disable_layers = ~disable_layers;
      flush_options.disable_layers = disable_layers;
      flush_options.flags |= COGL_PIPELINE_FLUSH_DISABLE_MASK;
    }
  if (G_UNLIKELY (fallback_layers))
    {
      flush_options.fallback_layers = fallback_layers;
      flush_options.flags |= COGL_PIPELINE_FLUSH_FALLBACK_MASK;
    }
  if (G_UNLIKELY (layer0_override_texture))
    {
      flush_options.flags |= COGL_PIPELINE_FLUSH_LAYER0_OVERRIDE;
      flush_options.layer0_override_texture = layer0_override_texture;
    }
  if (wrap_mode_overrides)
    {
      flush_options.flags |= COGL_PIPELINE_FLUSH_WRAP_MODE_OVERRIDES;
      flush_options.wrap_mode_overrides = *wrap_mode_overrides;
    }

  if (G_UNLIKELY (flush_options.flags))
    {
      /* If we haven't already created a derived pipeline... */
      if (source == pipeline)
        source = cogl_pipeline_copy (pipeline);
      _cogl_pipeline_apply_overrides (source, &flush_options);
    }

  entry->pipeline = _cogl_pipeline_journal_ref (source);
  entry->clip_stack = _cogl_clip_stack_ref (_cogl_get_clip_stack ());

  if (G_UNLIKELY (source != pipeline))
    cogl_handle_unref (source);

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DISABLE_SOFTWARE_TRANSFORM))
    cogl_get_modelview_matrix (&entry->model_view);

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DISABLE_BATCHING))
    _cogl_journal_flush ();

  COGL_TIMER_STOP (_cogl_uprof_context, log_timer);
}

