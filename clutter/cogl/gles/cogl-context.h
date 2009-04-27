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

#include "cogl-primitives.h"
#include "cogl-clip-stack.h"
#include "cogl-matrix-stack.h"
#include "cogl-current-matrix.h"

#include "cogl-gles2-wrapper.h"

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

  /* Client-side matrix stack or NULL if none */
  CoglMatrixMode    matrix_mode;
  CoglMatrixStack  *modelview_stack;

  /* Cache of inverse projection matrix */
  float            inverse_projection[16];

  /* Materials */
  CoglHandle        default_material;
  CoglHandle	    source_material;

  /* Textures */
  CoglHandle        default_gl_texture_2d_tex;
  CoglHandle        default_gl_texture_rect_tex;
  CoglHandle        texture_download_material;

  /* Batching geometry... */
  /* We journal the texture rectangles we want to submit to OpenGL so
   * we have an oppertunity to optimise the final order so that we
   * can batch things together. */
  GArray           *journal;
  GArray           *logged_vertices;
  GArray           *static_indices;
  GArray           *polygon_vertices;

  /* Some simple caching, to minimize state changes... */
  CoglHandle	    current_material;
  gulong            current_material_flags;
  GArray           *current_layers;
  guint             n_texcoord_arrays_enabled;

  /* Framebuffer objects */
  CoglBufferTarget  draw_buffer;

  /* Clip stack */
  CoglClipStackState clip;

  /* Primitives */
  floatVec2         path_start;
  floatVec2         path_pen;
  GArray           *path_nodes;
  guint             last_path;
  floatVec2         path_nodes_min;
  floatVec2         path_nodes_max;
  CoglHandle        stencil_material;

#ifdef HAVE_COGL_GLES2
  CoglGles2Wrapper     gles2;

  /* Viewport store for FBOs. Needed because glPushAttrib() isn't
     supported */
  GLint                viewport_store[4];
#endif

} CoglContext;

CoglContext *
_cogl_context_get_default ();

/* Obtains the context and returns retval if NULL */
#define _COGL_GET_CONTEXT(ctxvar, retval) \
CoglContext *ctxvar = _cogl_context_get_default (); \
if (ctxvar == NULL) return retval;

#define NO_RETVAL

#endif /* __COGL_CONTEXT_H */
