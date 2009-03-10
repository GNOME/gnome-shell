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
#include "cogl-texture-private.h"
#include "cogl-material-private.h"

#include "cogl-gles2-wrapper.h"

#include <string.h>

static CoglContext *_context = NULL;

gboolean
cogl_create_context ()
{
  GLubyte default_texture_data[] = { 0xff, 0xff, 0xff, 0x0 };
  gulong  enable_flags = 0;

  if (_context != NULL)
    return FALSE;

  /* Allocate context memory */
  _context = (CoglContext*) g_malloc (sizeof (CoglContext));

  /* Init default values */
  _context->feature_flags = 0;
  _context->features_cached = FALSE;

  _context->enable_flags = 0;

  _context->enable_backface_culling = FALSE;

  _context->material_handles = NULL;
  _context->material_layer_handles = NULL;
  _context->default_material = cogl_material_new ();
  _context->source_material = NULL;

  _context->texture_handles = NULL;
  _context->default_gl_texture_2d_tex = COGL_INVALID_HANDLE;
  _context->default_gl_texture_rect_tex = COGL_INVALID_HANDLE;
  _context->texture_download_material = COGL_INVALID_HANDLE;

  _context->journal = g_array_new (FALSE, FALSE, sizeof (CoglJournalEntry));
  _context->logged_vertices = g_array_new (FALSE, FALSE, sizeof (GLfloat));
  _context->static_indices = g_array_new (FALSE, FALSE, sizeof (GLushort));
  _context->polygon_vertices = g_array_new (FALSE, FALSE,
                                            sizeof (CoglTextureGLVertex));

  _context->current_material = NULL;
  _context->current_material_flags = 0;
  _context->current_layers = g_array_new (FALSE, FALSE,
                                          sizeof (CoglLayerInfo));
  _context->n_texcoord_arrays_enabled = 0;

  _context->fbo_handles = NULL;
  _context->draw_buffer = COGL_WINDOW_BUFFER;

  _context->shader_handles = NULL;

  _context->program_handles = NULL;

  _context->vertex_buffer_handles = NULL;

  _context->path_nodes = g_array_new (FALSE, FALSE, sizeof (CoglPathNode));
  _context->last_path = 0;
  _context->stencil_material = cogl_material_new ();

  /* Init the GLES2 wrapper */
#ifdef HAVE_COGL_GLES2
  cogl_gles2_wrapper_init (&_context->gles2);
#endif

  /* Initialise the clip stack */
  _cogl_clip_stack_state_init ();

  /* Create default textures used for fall backs */
  _context->default_gl_texture_2d_tex =
    cogl_texture_new_from_data (1, /* width */
                                1, /* height */
                                -1, /* max waste */
                                COGL_TEXTURE_NONE, /* flags */
                                COGL_PIXEL_FORMAT_RGBA_8888, /* data format */
                                /* internal format */
                                COGL_PIXEL_FORMAT_RGBA_8888,
                                0, /* auto calc row stride */
                                default_texture_data);
  _context->default_gl_texture_rect_tex =
    cogl_texture_new_from_data (1, /* width */
                                1, /* height */
                                -1, /* max waste */
                                COGL_TEXTURE_NONE, /* flags */
                                COGL_PIXEL_FORMAT_RGBA_8888, /* data format */
                                /* internal format */
                                COGL_PIXEL_FORMAT_RGBA_8888,
                                0, /* auto calc row stride */
                                default_texture_data);

  cogl_set_source (_context->default_material);
  cogl_material_flush_gl_state (_context->source_material, NULL);
  enable_flags =
    cogl_material_get_cogl_enable_flags (_context->source_material);
  cogl_enable (enable_flags);

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

  if (_context->default_gl_texture_2d_tex)
    cogl_texture_unref (_context->default_gl_texture_2d_tex);
  if (_context->default_gl_texture_rect_tex)
    cogl_texture_unref (_context->default_gl_texture_rect_tex);

  if (_context->default_material)
    cogl_material_unref (_context->default_material);

  if (_context->journal)
    g_array_free (_context->journal, TRUE);
  if (_context->logged_vertices)
    g_array_free (_context->logged_vertices, TRUE);

  if (_context->static_indices)
    g_array_free (_context->static_indices, TRUE);
  if (_context->polygon_vertices)
    g_array_free (_context->polygon_vertices, TRUE);
  if (_context->current_layers)
    g_array_free (_context->current_layers, TRUE);

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
