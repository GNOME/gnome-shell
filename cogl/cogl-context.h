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

#ifndef __COGL_CONTEXT_H
#define __COGL_CONTEXT_H

#include "cogl-internal.h"
#include "cogl-context-driver.h"
#include "cogl-primitives.h"
#include "cogl-clip-stack.h"
#include "cogl-matrix-stack.h"
#include "cogl-material-private.h"

typedef struct
{
  GLfloat v[3];
  GLfloat t[2];
  GLubyte c[4];
} CoglTextureGLVertex;

typedef struct
{
  /* Features cache */
  CoglFeatureFlags  feature_flags;
  gboolean          features_cached;

  /* Enable cache */
  gulong            enable_flags;
  guint8            color_alpha;

  gboolean          enable_backface_culling;

  gboolean          indirect;

  /* A few handy matrix constants */
  CoglMatrix        identity_matrix;
  CoglMatrix        y_flip_matrix;

  /* Client-side matrix stack or NULL if none */
  CoglMatrixMode    flushed_matrix_mode;
  GList            *texture_units;

  /* Cache of inverse projection matrix */
  float            inverse_projection[16];

  /* Materials */
  CoglHandle        default_material;
  CoglHandle	    source_material;

  /* Textures */
  CoglHandle        default_gl_texture_2d_tex;
  CoglHandle        default_gl_texture_rect_tex;

  /* Batching geometry... */
  /* We journal the texture rectangles we want to submit to OpenGL so
   * we have an oppertunity to optimise the final order so that we
   * can batch things together. */
  GArray           *journal;
  GArray           *logged_vertices;
  GArray           *polygon_vertices;

  /* Some simple caching, to minimize state changes... */
  CoglHandle	    current_material;
  gulong            current_material_flags;
  CoglMaterialFlushOptions current_material_flush_options;
  GArray           *current_layers;
  guint             n_texcoord_arrays_enabled;

  /* Draw Buffers */
  GSList           *draw_buffer_stack;
  CoglHandle        window_buffer;
  gboolean          dirty_bound_framebuffer;
  gboolean          dirty_gl_viewport;

  /* Primitives */
  floatVec2         path_start;
  floatVec2         path_pen;
  GArray           *path_nodes;
  guint             last_path;
  floatVec2         path_nodes_min;
  floatVec2         path_nodes_max;
  CoglHandle        stencil_material;

  /* Pre-generated VBOs containing indices to generate GL_TRIANGLES
     out of a vertex array of quads */
  CoglHandle        quad_indices_byte;
  guint             quad_indices_short_len;
  CoglHandle        quad_indices_short;

  gboolean          in_begin_gl_block;

  CoglHandle        texture_download_material;

  CoglContextDriver drv;
} CoglContext;

CoglContext *
_cogl_context_get_default ();

/* Obtains the context and returns retval if NULL */
#define _COGL_GET_CONTEXT(ctxvar, retval) \
CoglContext *ctxvar = _cogl_context_get_default (); \
if (ctxvar == NULL) return retval;

#define NO_RETVAL

#endif /* __COGL_CONTEXT_H */
