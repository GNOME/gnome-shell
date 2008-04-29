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

typedef struct
{
  /* Features cache */
  CoglFeatureFlags  feature_flags;
  gboolean          features_cached;
  
  /* Enable cache */
  gulong            enable_flags;
  guint8            color_alpha;
  
  /* Primitives */
  CoglFixedVec2     path_start;
  CoglFixedVec2     path_pen;
  CoglFloatVec2    *path_nodes;
  guint             path_nodes_cap;
  guint             path_nodes_size;
  CoglFixedVec2     path_nodes_min;
  CoglFixedVec2     path_nodes_max;
  
  /* Textures */
  GArray           *texture_handles;
  
  /* Framebuffer objects */
  GArray           *fbo_handles;
  CoglBufferTarget  draw_buffer;

  /* Shaders */
  GArray           *shader_handles;

  /* Programs */
  GArray           *program_handles;
  
  /* Relying on glext.h to define these */
  PFNGLGENRENDERBUFFERSEXTPROC                pf_glGenRenderbuffersEXT;
  PFNGLBINDRENDERBUFFEREXTPROC                pf_glBindRenderbufferEXT;
  PFNGLRENDERBUFFERSTORAGEEXTPROC             pf_glRenderbufferStorageEXT;
  PFNGLGENFRAMEBUFFERSEXTPROC                 pf_glGenFramebuffersEXT;
  PFNGLBINDFRAMEBUFFEREXTPROC                 pf_glBindFramebufferEXT;
  PFNGLFRAMEBUFFERTEXTURE2DEXTPROC            pf_glFramebufferTexture2DEXT;
  PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC         pf_glFramebufferRenderbufferEXT;
  PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC          pf_glCheckFramebufferStatusEXT;
  PFNGLDELETEFRAMEBUFFERSEXTPROC              pf_glDeleteFramebuffersEXT;
  PFNGLBLITFRAMEBUFFEREXTPROC                 pf_glBlitFramebufferEXT;
  PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC  pf_glRenderbufferStorageMultisampleEXT;
  
  PFNGLCREATEPROGRAMOBJECTARBPROC             pf_glCreateProgramObjectARB;
  PFNGLCREATESHADEROBJECTARBPROC              pf_glCreateShaderObjectARB;
  PFNGLSHADERSOURCEARBPROC                    pf_glShaderSourceARB;
  PFNGLCOMPILESHADERARBPROC                   pf_glCompileShaderARB;
  PFNGLATTACHOBJECTARBPROC                    pf_glAttachObjectARB;
  PFNGLLINKPROGRAMARBPROC                     pf_glLinkProgramARB;
  PFNGLUSEPROGRAMOBJECTARBPROC                pf_glUseProgramObjectARB;
  PFNGLGETUNIFORMLOCATIONARBPROC              pf_glGetUniformLocationARB;
  PFNGLDELETEOBJECTARBPROC                    pf_glDeleteObjectARB;
  PFNGLGETINFOLOGARBPROC                      pf_glGetInfoLogARB;
  PFNGLGETOBJECTPARAMETERIVARBPROC            pf_glGetObjectParameterivARB;
  PFNGLUNIFORM1FARBPROC                       pf_glUniform1fARB;
  
} CoglContext;

CoglContext *
_cogl_context_get_default ();

/* Obtains the context and returns retval if NULL */
#define _COGL_GET_CONTEXT(ctxvar, retval) \
CoglContext *ctxvar = _cogl_context_get_default (); \
if (ctxvar == NULL) return retval;

#define NO_RETVAL 

#endif /* __COGL_CONTEXT_H */
