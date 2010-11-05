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

#ifndef __COGL_JOURNAL_PRIVATE_H
#define __COGL_JOURNAL_PRIVATE_H

#include "cogl.h"
#include "cogl-handle.h"
#include "cogl-clip-stack.h"

typedef struct _CoglJournal
{
  CoglObject _parent;

  GArray *entries;
  GArray *vertices;
  size_t needed_vbo_len;

  int fast_read_pixel_count;

} CoglJournal;

/* To improve batching of geometry when submitting vertices to OpenGL we
 * log the texture rectangles we want to draw to a journal, so when we
 * later flush the journal we aim to batch data, and gl draw calls. */
typedef struct _CoglJournalEntry
{
  CoglPipeline            *pipeline;
  int                      n_layers;
  CoglMatrix               model_view;
  CoglClipStack           *clip_stack;
  /* Offset into ctx->logged_vertices */
  size_t                   array_offset;
  /* XXX: These entries are pretty big now considering the padding in
   * CoglPipelineFlushOptions and CoglMatrix, so we might need to optimize this
   * later. */
} CoglJournalEntry;

CoglJournal *
_cogl_journal_new (void);

void
_cogl_journal_log_quad (CoglJournal  *journal,
                        const float  *position,
                        CoglPipeline *pipeline,
                        int           n_layers,
                        CoglHandle    layer0_override_texture,
                        const float  *tex_coords,
                        unsigned int  tex_coords_len);

void
_cogl_journal_flush (CoglJournal *journal,
                     CoglFramebuffer *framebuffer);

void
_cogl_journal_discard (CoglJournal *journal);

gboolean
_cogl_journal_all_entries_within_bounds (CoglJournal *journal,
                                         float clip_x0,
                                         float clip_y0,
                                         float clip_x1,
                                         float clip_y1);

gboolean
_cogl_journal_try_read_pixel (CoglJournal *journal,
                              int x,
                              int y,
                              CoglPixelFormat format,
                              guint8 *pixel,
                              gboolean *found_intersection);

#endif /* __COGL_JOURNAL_PRIVATE_H */
