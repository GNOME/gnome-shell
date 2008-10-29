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

#ifndef __COGL_CONTEXT_H
#define __COGL_CONTEXT_H

#include "cogl-primitives.h"

#include "cogl-gles2-wrapper.h"

typedef struct
{
  GLfixed v[3];
  GLfixed t[2];
  GLfixed c[4];
} CoglTextureGLVertex;

typedef struct
{
  /* Features cache */
  CoglFeatureFlags     feature_flags;
  gboolean             features_cached;
  GLint                num_stencil_bits;
  
  /* Enable cache */
  gulong               enable_flags;
  guint8               color_alpha;
  COGLenum             blend_src_factor;
  COGLenum             blend_dst_factor;

  gboolean             enable_backface_culling;
  
  /* Primitives */
  CoglFixedVec2        path_start;
  CoglFixedVec2        path_pen;
  CoglFixedVec2       *path_nodes;
  guint                path_nodes_cap;
  guint                path_nodes_size;
  CoglFixedVec2        path_nodes_min;
  CoglFixedVec2        path_nodes_max;
  
  /* Cache of inverse projection matrix */
  ClutterFixed         inverse_projection[16];

  /* Textures */
  GArray              *texture_handles;
  CoglTextureGLVertex *texture_vertices;
  gulong               texture_vertices_size;
  
  /* Framebuffer objects */
  GArray              *fbo_handles;
  CoglBufferTarget     draw_buffer;

  /* Shaders */
  GArray              *program_handles;
  GArray              *shader_handles;

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
