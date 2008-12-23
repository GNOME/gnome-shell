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
#include "cogl-util.h"
#include "cogl-context.h"

#include <string.h>

static CoglContext *_context = NULL;

gboolean
cogl_create_context ()
{
  if (_context != NULL)
    return FALSE;

  /* Allocate context memory */
  _context = (CoglContext*) g_malloc (sizeof (CoglContext));

  /* Init default values */
  _context->feature_flags = 0;
  _context->features_cached = FALSE;

  _context->enable_flags = 0;
  _context->color_alpha = 255;

  _context->path_nodes = g_array_new (FALSE, FALSE, sizeof (CoglPathNode));
  _context->last_path = 0;

  _context->texture_handles = NULL;
  _context->texture_vertices_size = 0;
  _context->texture_vertices = NULL;

  _context->fbo_handles = NULL;
  _context->draw_buffer = COGL_WINDOW_BUFFER;

  _context->blend_src_factor = CGL_SRC_ALPHA;
  _context->blend_dst_factor = CGL_ONE_MINUS_SRC_ALPHA;

  _context->shader_handles = NULL;

  _context->program_handles = NULL;

  _context->mesh_handles = NULL;

  _context->pf_glGenRenderbuffersEXT = NULL;
  _context->pf_glBindRenderbufferEXT = NULL;
  _context->pf_glRenderbufferStorageEXT = NULL;
  _context->pf_glGenFramebuffersEXT = NULL;
  _context->pf_glBindFramebufferEXT = NULL;
  _context->pf_glFramebufferTexture2DEXT = NULL;
  _context->pf_glFramebufferRenderbufferEXT = NULL;
  _context->pf_glCheckFramebufferStatusEXT = NULL;
  _context->pf_glDeleteFramebuffersEXT = NULL;
  _context->pf_glBlitFramebufferEXT = NULL;
  _context->pf_glRenderbufferStorageMultisampleEXT = NULL;

  _context->pf_glCreateProgramObjectARB = NULL;
  _context->pf_glCreateShaderObjectARB = NULL;
  _context->pf_glShaderSourceARB = NULL;
  _context->pf_glCompileShaderARB = NULL;
  _context->pf_glAttachObjectARB = NULL;
  _context->pf_glLinkProgramARB = NULL;
  _context->pf_glUseProgramObjectARB = NULL;
  _context->pf_glGetUniformLocationARB = NULL;
  _context->pf_glDeleteObjectARB = NULL;
  _context->pf_glGetInfoLogARB = NULL;
  _context->pf_glGetObjectParameterivARB = NULL;
  _context->pf_glUniform1fARB = NULL;
  _context->pf_glUniform2fARB = NULL;
  _context->pf_glUniform3fARB = NULL;
  _context->pf_glUniform4fARB = NULL;
  _context->pf_glUniform1fvARB = NULL;
  _context->pf_glUniform2fvARB = NULL;
  _context->pf_glUniform3fvARB = NULL;
  _context->pf_glUniform4fvARB = NULL;
  _context->pf_glUniform1iARB = NULL;
  _context->pf_glUniform2iARB = NULL;
  _context->pf_glUniform3iARB = NULL;
  _context->pf_glUniform4iARB = NULL;
  _context->pf_glUniform1ivARB = NULL;
  _context->pf_glUniform2ivARB = NULL;
  _context->pf_glUniform3ivARB = NULL;
  _context->pf_glUniform4ivARB = NULL;
  _context->pf_glUniformMatrix2fvARB = NULL;
  _context->pf_glUniformMatrix3fvARB = NULL;
  _context->pf_glUniformMatrix4fvARB = NULL;

  _context->pf_glDrawRangeElements = NULL;

  /* Init OpenGL state */
  GE( glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE) );
  GE( glColorMask (TRUE, TRUE, TRUE, FALSE) );
  GE( glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) );
  cogl_enable (0);

  /* Initialise the clip stack */
  _cogl_clip_stack_state_init ();

  return TRUE;
}

void
cogl_destroy_context ()
{
  if (_context == NULL)
    return;

  _cogl_clip_stack_state_destroy ();

  if (_context->path_nodes)
    g_array_free (_context->path_nodes, TRUE);

  if (_context->texture_handles)
    g_array_free (_context->texture_handles, TRUE);
  if (_context->fbo_handles)
    g_array_free (_context->fbo_handles, TRUE);
  if (_context->shader_handles)
    g_array_free (_context->shader_handles, TRUE);
  if (_context->program_handles)
    g_array_free (_context->program_handles, TRUE);

  g_free (_context);
}

CoglContext *
_cogl_context_get_default ()
{
  /* Create if doesn't exist yet */
  if (_context == NULL)
    cogl_create_context ();

  return _context;
}
