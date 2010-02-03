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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context.h"
#include "cogl-journal-private.h"
#include "cogl-texture-private.h"
#include "cogl-material-private.h"
#include "cogl-framebuffer-private.h"

#include <string.h>

extern void
_cogl_create_context_driver (CoglContext *context);

static CoglContext *_context = NULL;
static gboolean gl_is_indirect = FALSE;

static gboolean
cogl_create_context (void)
{
  GLubyte default_texture_data[] = { 0xff, 0xff, 0xff, 0x0 };
  gulong  enable_flags = 0;
  CoglHandle window_buffer;

  if (_context != NULL)
    return FALSE;

  /* Allocate context memory */
  _context = (CoglContext*) g_malloc (sizeof (CoglContext));

  /* Init default values */
  _context->feature_flags = 0;
  _context->features_cached = FALSE;

  /* Initialise the driver specific state */
  /* TODO: combine these two into one function */
  _cogl_create_context_driver (_context);
  _cogl_features_init ();

  _cogl_material_init_default_material ();

  _context->enable_flags = 0;
  _context->color_alpha = 0;

  _context->enable_backface_culling = FALSE;
  _context->flushed_front_winding = COGL_FRONT_WINDING_COUNTER_CLOCKWISE;

  _context->indirect = gl_is_indirect;

  cogl_matrix_init_identity (&_context->identity_matrix);
  cogl_matrix_init_identity (&_context->y_flip_matrix);
  cogl_matrix_scale (&_context->y_flip_matrix, 1, -1, 1);

  _context->flushed_matrix_mode = COGL_MATRIX_MODELVIEW;
  _context->texture_units = NULL;

  _context->simple_material = cogl_material_new ();
  _context->source_material = NULL;

  _context->default_gl_texture_2d_tex = COGL_INVALID_HANDLE;
  _context->default_gl_texture_rect_tex = COGL_INVALID_HANDLE;

  _context->journal = g_array_new (FALSE, FALSE, sizeof (CoglJournalEntry));
  _context->logged_vertices = g_array_new (FALSE, FALSE, sizeof (GLfloat));

  _context->current_material = NULL;
  _context->current_material_flags = 0;
  memset (&_context->current_material_flush_options,
          0, sizeof (CoglMaterialFlushOptions));
  _context->current_layers = g_array_new (FALSE, FALSE,
                                          sizeof (CoglLayerInfo));
  _context->n_texcoord_arrays_enabled = 0;

  _context->framebuffer_stack = _cogl_create_framebuffer_stack ();
  window_buffer = _cogl_onscreen_new ();
  cogl_set_framebuffer (window_buffer);
  /* XXX: the deprecated _cogl_set_draw_buffer API expects to
   * find the window buffer here... */
  _context->window_buffer = window_buffer;

  _context->dirty_bound_framebuffer = TRUE;
  _context->dirty_gl_viewport = TRUE;

  _context->path_nodes = g_array_new (FALSE, FALSE, sizeof (CoglPathNode));
  _context->last_path = 0;
  _context->stencil_material = cogl_material_new ();

  _context->in_begin_gl_block = FALSE;

  _context->quad_indices_byte = COGL_INVALID_HANDLE;
  _context->quad_indices_short = COGL_INVALID_HANDLE;
  _context->quad_indices_short_len = 0;

  _context->texture_download_material = COGL_INVALID_HANDLE;

  /* Create default textures used for fall backs */
  _context->default_gl_texture_2d_tex =
    cogl_texture_new_from_data (1, /* width */
                                1, /* height */
                                COGL_TEXTURE_NO_SLICING,
                                COGL_PIXEL_FORMAT_RGBA_8888_PRE, /* data format */
                                /* internal format */
                                COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                0, /* auto calc row stride */
                                default_texture_data);
  _context->default_gl_texture_rect_tex =
    cogl_texture_new_from_data (1, /* width */
                                1, /* height */
                                COGL_TEXTURE_NO_SLICING,
                                COGL_PIXEL_FORMAT_RGBA_8888_PRE, /* data format */
                                /* internal format */
                                COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                0, /* auto calc row stride */
                                default_texture_data);

  cogl_set_source (_context->simple_material);
  _cogl_material_flush_gl_state (_context->source_material, NULL);
  enable_flags =
    _cogl_material_get_cogl_enable_flags (_context->source_material);
  cogl_enable (enable_flags);
  _cogl_flush_face_winding ();

  _context->atlas = NULL;
  _context->atlas_texture = COGL_INVALID_HANDLE;

  return TRUE;
}

void
_cogl_destroy_context ()
{

  if (_context == NULL)
    return;

  _cogl_destroy_texture_units ();

  _cogl_free_framebuffer_stack (_context->framebuffer_stack);

  if (_context->path_nodes)
    g_array_free (_context->path_nodes, TRUE);

  if (_context->default_gl_texture_2d_tex)
    cogl_handle_unref (_context->default_gl_texture_2d_tex);
  if (_context->default_gl_texture_rect_tex)
    cogl_handle_unref (_context->default_gl_texture_rect_tex);

  if (_context->simple_material)
    cogl_handle_unref (_context->simple_material);

  if (_context->journal)
    g_array_free (_context->journal, TRUE);
  if (_context->logged_vertices)
    g_array_free (_context->logged_vertices, TRUE);

  if (_context->current_layers)
    g_array_free (_context->current_layers, TRUE);

  if (_context->quad_indices_byte)
    cogl_handle_unref (_context->quad_indices_byte);
  if (_context->quad_indices_short)
    cogl_handle_unref (_context->quad_indices_short);

  if (_context->default_material)
    cogl_handle_unref (_context->default_material);

  if (_context->atlas)
    _cogl_atlas_free (_context->atlas);
  if (_context->atlas_texture)
    cogl_handle_unref (_context->atlas_texture);

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

/**
 * _cogl_set_indirect_context:
 * @indirect: TRUE if GL context is indirect
 *
 * Advises COGL that the GL context is indirect (commands are sent
 * over a socket). COGL uses this information to try to avoid
 * round-trips in its use of GL, for example.
 *
 * This function cannot be called "on the fly," only before COGL
 * initializes.
 */
void
_cogl_set_indirect_context (gboolean indirect)
{
  /* we get called multiple times if someone creates
   * more than the default stage
   */
  if (_context != NULL)
    {
      if (indirect != _context->indirect)
        g_warning ("Right now all stages will be treated as "
                   "either direct or indirect, ignoring attempt "
                   "to change to indirect=%d", indirect);
      return;
    }

  gl_is_indirect = indirect;
}
