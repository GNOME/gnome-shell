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

#include <string.h>

#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context.h"

#include "cogl-gles2-wrapper.h"

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
  _context->program_handles = NULL;
  _context->shader_handles = NULL;
  _context->draw_buffer = COGL_WINDOW_BUFFER;

  _context->mesh_handles = NULL;
  
  _context->blend_src_factor = CGL_SRC_ALPHA;
  _context->blend_dst_factor = CGL_ONE_MINUS_SRC_ALPHA;

  /* Init the GLES2 wrapper */
#ifdef HAVE_COGL_GLES2
  cogl_gles2_wrapper_init (&_context->gles2);
#endif
  
  /* Init OpenGL state */
  GE( cogl_wrap_glTexEnvx (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE) );
  GE( glColorMask (TRUE, TRUE, TRUE, FALSE) );
  GE( glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) );
  cogl_enable (0);

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

#ifdef HAVE_COGL_GLES2
  cogl_gles2_wrapper_deinit (&_context->gles2);
#endif

  if (_context->texture_vertices)
    g_free (_context->texture_vertices);
  
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
  /* Create if doesn't exists yet */
  if (_context == NULL)
    cogl_create_context ();
  
  return _context;
}
