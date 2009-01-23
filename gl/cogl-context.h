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
#include "cogl-clip-stack.h"

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
  COGLenum          blend_src_factor;
  COGLenum          blend_dst_factor;

  gboolean          enable_backface_culling;

  /* Primitives */
  floatVec2     path_start;
  floatVec2     path_pen;
  GArray           *path_nodes;
  guint             last_path;
  floatVec2     path_nodes_min;
  floatVec2     path_nodes_max;

  /* Cache of inverse projection matrix */
  GLfloat           inverse_projection[16];

  /* Textures */
  GArray	      *texture_handles;
  GArray              *texture_vertices;
  GArray              *texture_indices;
  /* The gl texture number that the above vertices apply to. This to
     detect when a different slice is encountered so that the vertices
     can be flushed */
  GLuint               texture_current;
  GLenum               texture_target;
  GLenum               texture_wrap_mode;

  /* Materials */
  GArray           *material_handles;
  GArray           *material_layer_handles;
  CoglHandle	    source_material;
  CoglHandle	    current_material;

  /* Framebuffer objects */
  GArray           *fbo_handles;
  CoglBufferTarget  draw_buffer;

  /* Shaders */
  GArray           *shader_handles;

  /* Programs */
  GArray           *program_handles;

  /* Clip stack */
  CoglClipStackState clip;

  /* Vertex buffers */
  GArray           *vertex_buffer_handles;

  /* Relying on glext.h to define these */
  COGL_PFNGLGENRENDERBUFFERSEXTPROC                pf_glGenRenderbuffersEXT;
  COGL_PFNGLDELETERENDERBUFFERSEXTPROC             pf_glDeleteRenderbuffersEXT;
  COGL_PFNGLBINDRENDERBUFFEREXTPROC                pf_glBindRenderbufferEXT;
  COGL_PFNGLRENDERBUFFERSTORAGEEXTPROC             pf_glRenderbufferStorageEXT;
  COGL_PFNGLGENFRAMEBUFFERSEXTPROC                 pf_glGenFramebuffersEXT;
  COGL_PFNGLBINDFRAMEBUFFEREXTPROC                 pf_glBindFramebufferEXT;
  COGL_PFNGLFRAMEBUFFERTEXTURE2DEXTPROC            pf_glFramebufferTexture2DEXT;
  COGL_PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC         pf_glFramebufferRenderbufferEXT;
  COGL_PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC          pf_glCheckFramebufferStatusEXT;
  COGL_PFNGLDELETEFRAMEBUFFERSEXTPROC              pf_glDeleteFramebuffersEXT;
  COGL_PFNGLBLITFRAMEBUFFEREXTPROC                 pf_glBlitFramebufferEXT;
  COGL_PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC  pf_glRenderbufferStorageMultisampleEXT;

  COGL_PFNGLCREATEPROGRAMOBJECTARBPROC             pf_glCreateProgramObjectARB;
  COGL_PFNGLCREATESHADEROBJECTARBPROC              pf_glCreateShaderObjectARB;
  COGL_PFNGLSHADERSOURCEARBPROC                    pf_glShaderSourceARB;
  COGL_PFNGLCOMPILESHADERARBPROC                   pf_glCompileShaderARB;
  COGL_PFNGLATTACHOBJECTARBPROC                    pf_glAttachObjectARB;
  COGL_PFNGLLINKPROGRAMARBPROC                     pf_glLinkProgramARB;
  COGL_PFNGLUSEPROGRAMOBJECTARBPROC                pf_glUseProgramObjectARB;
  COGL_PFNGLGETUNIFORMLOCATIONARBPROC              pf_glGetUniformLocationARB;
  COGL_PFNGLDELETEOBJECTARBPROC                    pf_glDeleteObjectARB;
  COGL_PFNGLGETINFOLOGARBPROC                      pf_glGetInfoLogARB;
  COGL_PFNGLGETOBJECTPARAMETERIVARBPROC            pf_glGetObjectParameterivARB;

  COGL_PFNGLVERTEXATTRIBPOINTERARBPROC		   pf_glVertexAttribPointerARB;
  COGL_PFNGLENABLEVERTEXATTRIBARRAYARBPROC	   pf_glEnableVertexAttribArrayARB;
  COGL_PFNGLDISABLEVERTEXATTRIBARRAYARBPROC	   pf_glDisableVertexAttribArrayARB;

  COGL_PFNGLGENBUFFERSARBPROC			   pf_glGenBuffersARB;
  COGL_PFNGLBINDBUFFERARBPROC			   pf_glBindBufferARB;
  COGL_PFNGLBUFFERDATAARBPROC			   pf_glBufferDataARB;
  COGL_PFNGLBUFFERSUBDATAARBPROC		   pf_glBufferSubDataARB;
  COGL_PFNGLMAPBUFFERARBPROC			   pf_glMapBufferARB;
  COGL_PFNGLUNMAPBUFFERARBPROC			   pf_glUnmapBufferARB;
  COGL_PFNGLDELETEBUFFERSARBPROC		   pf_glDeleteBuffersARB;

  COGL_PFNGLUNIFORM1FARBPROC                       pf_glUniform1fARB;
  COGL_PFNGLUNIFORM2FARBPROC                       pf_glUniform2fARB;
  COGL_PFNGLUNIFORM3FARBPROC                       pf_glUniform3fARB;
  COGL_PFNGLUNIFORM4FARBPROC                       pf_glUniform4fARB;
  COGL_PFNGLUNIFORM1FVARBPROC                      pf_glUniform1fvARB;
  COGL_PFNGLUNIFORM2FVARBPROC                      pf_glUniform2fvARB;
  COGL_PFNGLUNIFORM3FVARBPROC                      pf_glUniform3fvARB;
  COGL_PFNGLUNIFORM4FVARBPROC                      pf_glUniform4fvARB;
  COGL_PFNGLUNIFORM1IARBPROC                       pf_glUniform1iARB;
  COGL_PFNGLUNIFORM2IARBPROC                       pf_glUniform2iARB;
  COGL_PFNGLUNIFORM3IARBPROC                       pf_glUniform3iARB;
  COGL_PFNGLUNIFORM4IARBPROC                       pf_glUniform4iARB;
  COGL_PFNGLUNIFORM1IVARBPROC                      pf_glUniform1ivARB;
  COGL_PFNGLUNIFORM2IVARBPROC                      pf_glUniform2ivARB;
  COGL_PFNGLUNIFORM3IVARBPROC                      pf_glUniform3ivARB;
  COGL_PFNGLUNIFORM4IVARBPROC                      pf_glUniform4ivARB;
  COGL_PFNGLUNIFORMMATRIX2FVARBPROC                pf_glUniformMatrix2fvARB;
  COGL_PFNGLUNIFORMMATRIX3FVARBPROC                pf_glUniformMatrix3fvARB;
  COGL_PFNGLUNIFORMMATRIX4FVARBPROC                pf_glUniformMatrix4fvARB;

  COGL_PFNGLDRAWRANGEELEMENTSPROC                  pf_glDrawRangeElements;
} CoglContext;

CoglContext *
_cogl_context_get_default ();

/* Obtains the context and returns retval if NULL */
#define _COGL_GET_CONTEXT(ctxvar, retval) \
CoglContext *ctxvar = _cogl_context_get_default (); \
if (ctxvar == NULL) return retval;

#define NO_RETVAL

#endif /* __COGL_CONTEXT_H */
