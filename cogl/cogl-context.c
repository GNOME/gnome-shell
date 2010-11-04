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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-profile.h"
#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-journal-private.h"
#include "cogl-texture-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl2-path.h"

#include <string.h>

#ifdef HAVE_COGL_GL
#include "cogl-pipeline-fragend-arbfp-private.h"
#define glActiveTexture _context->drv.pf_glActiveTexture
#endif

/* This isn't defined in the GLES headers */
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif

extern void
_cogl_create_context_driver (CoglContext *context);
extern void
_cogl_create_context_winsys (CoglContext *context);
extern void
_cogl_destroy_context_winsys (CoglContext *context);

static CoglContext *_context = NULL;
static gboolean gl_is_indirect = FALSE;

static void
_cogl_init_feature_overrides (CoglContext *ctx)
{
  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_VBOS)))
    ctx->feature_flags &= ~COGL_FEATURE_VBOS;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_PBOS)))
    ctx->feature_flags &= ~COGL_FEATURE_PBOS;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_ARBFP)))
    ctx->feature_flags &= ~COGL_FEATURE_SHADERS_ARBFP;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_GLSL)))
    ctx->feature_flags &= ~COGL_FEATURE_SHADERS_GLSL;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_NPOT_TEXTURES)))
    ctx->feature_flags &= ~(COGL_FEATURE_TEXTURE_NPOT |
                            COGL_FEATURE_TEXTURE_NPOT_BASIC |
                            COGL_FEATURE_TEXTURE_NPOT_MIPMAP |
                            COGL_FEATURE_TEXTURE_NPOT_REPEAT);
}

static gboolean
cogl_create_context (void)
{
  GLubyte default_texture_data[] = { 0xff, 0xff, 0xff, 0x0 };
  unsigned long  enable_flags = 0;
  CoglHandle window_buffer;
  int i;

  if (_context != NULL)
    return FALSE;

#ifdef CLUTTER_ENABLE_PROFILE
  /* We need to be absolutely sure that uprof has been initialized
   * before calling _cogl_uprof_init. uprof_init (NULL, NULL)
   * will be a NOP if it has been initialized but it will also
   * mean subsequent parsing of the UProf GOptionGroup will have no
   * affect.
   *
   * Sadly GOptionGroup based library initialization is extremely
   * fragile by design because GOptionGroups have no notion of
   * dependencies and so the order things are initialized isn't
   * currently under tight control.
   */
  uprof_init (NULL, NULL);
  _cogl_uprof_init ();
#endif

  /* Allocate context memory */
  _context = (CoglContext*) g_malloc (sizeof (CoglContext));

  /* Init default values */
  _context->feature_flags = 0;
  _context->feature_flags_private = 0;

  _context->texture_types = NULL;
  _context->buffer_types = NULL;

  /* Initialise the driver specific state */
  /* TODO: combine these two into one function */
  _cogl_create_context_driver (_context);
  _cogl_features_init ();
  _cogl_init_feature_overrides (_context);

  _cogl_create_context_winsys (_context);

  _cogl_pipeline_init_default_pipeline ();
  _cogl_pipeline_init_default_layers ();
  _cogl_pipeline_init_state_hash_functions ();
  _cogl_pipeline_init_layer_state_hash_functions ();

  _context->enable_flags = 0;

  _context->enable_backface_culling = FALSE;
  _context->flushed_front_winding = COGL_FRONT_WINDING_COUNTER_CLOCKWISE;

  _context->indirect = gl_is_indirect;

  cogl_matrix_init_identity (&_context->identity_matrix);
  cogl_matrix_init_identity (&_context->y_flip_matrix);
  cogl_matrix_scale (&_context->y_flip_matrix, 1, -1, 1);

  _context->flushed_matrix_mode = COGL_MATRIX_MODELVIEW;

  _context->texture_units =
    g_array_new (FALSE, FALSE, sizeof (CoglTextureUnit));

  /* See cogl-pipeline.c for more details about why we leave texture unit 1
   * active by default... */
  _context->active_texture_unit = 1;
  GE (glActiveTexture (GL_TEXTURE1));

  _context->legacy_fog_state.enabled = FALSE;

  _context->opaque_color_pipeline = cogl_pipeline_new ();
  _context->blended_color_pipeline = cogl_pipeline_new ();
  _context->texture_pipeline = cogl_pipeline_new ();
  _context->codegen_header_buffer = g_string_new ("");
  _context->codegen_source_buffer = g_string_new ("");
  _context->source_stack = NULL;

  _context->legacy_state_set = 0;

  _context->default_gl_texture_2d_tex = COGL_INVALID_HANDLE;
  _context->default_gl_texture_rect_tex = COGL_INVALID_HANDLE;

  _context->framebuffers = NULL;

  _context->journal_flush_attributes_array =
    g_array_new (TRUE, FALSE, sizeof (CoglAttribute *));
  _context->journal_clip_bounds = NULL;

  _context->polygon_vertices = g_array_new (FALSE, FALSE, sizeof (float));

  _context->current_pipeline = NULL;
  _context->current_pipeline_changes_since_flush = 0;
  _context->current_pipeline_skip_gl_color = FALSE;

  _context->pipeline0_nodes =
    g_array_sized_new (FALSE, FALSE, sizeof (CoglHandle), 20);
  _context->pipeline1_nodes =
    g_array_sized_new (FALSE, FALSE, sizeof (CoglHandle), 20);

  _cogl_bitmask_init (&_context->arrays_enabled);
  _cogl_bitmask_init (&_context->temp_bitmask);
  _cogl_bitmask_init (&_context->arrays_to_change);

  _context->max_texture_units = -1;
  _context->max_texture_image_units = -1;
  _context->max_activateable_texture_units = -1;

  _context->current_program = COGL_INVALID_HANDLE;

  _context->current_fragment_program_type = COGL_PIPELINE_PROGRAM_TYPE_FIXED;
  _context->current_vertex_program_type = COGL_PIPELINE_PROGRAM_TYPE_FIXED;
  _context->current_gl_program = 0;

  _context->gl_blend_enable_cache = FALSE;

  _context->depth_test_enabled_cache = FALSE;
  _context->depth_test_function_cache = COGL_DEPTH_TEST_FUNCTION_LESS;
  _context->depth_writing_enabled_cache = TRUE;
  _context->depth_range_near_cache = 0;
  _context->depth_range_far_cache = 1;

  _context->point_size_cache = 1.0f;

  _context->legacy_depth_test_enabled = FALSE;

#ifdef HAVE_COGL_GL
  _context->arbfp_cache = g_hash_table_new (_cogl_pipeline_fragend_arbfp_hash,
                                            _cogl_pipeline_fragend_arbfp_equal);
#endif

  for (i = 0; i < COGL_BUFFER_BIND_TARGET_COUNT; i++)
    _context->current_buffer[i] = NULL;

  _context->framebuffer_stack = _cogl_create_framebuffer_stack ();

  _context->current_clip_stack_valid = FALSE;

  window_buffer = _cogl_onscreen_new ();
  cogl_set_framebuffer (window_buffer);
  /* XXX: the deprecated _cogl_set_draw_buffer API expects to
   * find the window buffer here... */
  _context->window_buffer = window_buffer;

  _context->dirty_bound_framebuffer = TRUE;
  _context->dirty_gl_viewport = TRUE;

  _context->current_path = cogl2_path_new ();
  _context->stencil_pipeline = cogl_pipeline_new ();

  _context->in_begin_gl_block = FALSE;

  _context->quad_buffer_indices_byte = COGL_INVALID_HANDLE;
  _context->quad_buffer_indices = COGL_INVALID_HANDLE;
  _context->quad_buffer_indices_len = 0;

  _context->rectangle_byte_indices = NULL;
  _context->rectangle_short_indices = NULL;
  _context->rectangle_short_indices_len = 0;

  _context->texture_download_pipeline = COGL_INVALID_HANDLE;
  _context->blit_texture_pipeline = COGL_INVALID_HANDLE;

#ifndef HAVE_COGL_GLES2
  /* The default for GL_ALPHA_TEST is to always pass which is equivalent to
   * the test being disabled therefore we assume that for all drivers there
   * will be no performance impact if we always leave the test enabled which
   * makes things a bit simpler for us. Under GLES2 the alpha test is
   * implemented in the fragment shader so there is no enable for it
   */
  GE (glEnable (GL_ALPHA_TEST));
#endif

#ifdef HAVE_COGL_GLES2
  _context->flushed_modelview_stack = NULL;
  _context->flushed_projection_stack = NULL;
#endif

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

  cogl_push_source (_context->opaque_color_pipeline);
  _cogl_pipeline_flush_gl_state (_context->opaque_color_pipeline, FALSE, 0);
  _cogl_enable (enable_flags);
  _cogl_flush_face_winding ();

  _context->atlases = NULL;

  _context->buffer_map_fallback_array = g_byte_array_new ();
  _context->buffer_map_fallback_in_use = FALSE;

  /* As far as I can tell, GL_POINT_SPRITE doesn't have any effect
     unless GL_COORD_REPLACE is enabled for an individual
     layer. Therefore it seems like it should be ok to just leave it
     enabled all the time instead of having to have a set property on
     each pipeline to track whether any layers have point sprite
     coords enabled. We don't need to do this for GLES2 because point
     sprites are handled using a builtin varying in the shader. */
#ifndef HAVE_COGL_GLES2
  if (cogl_features_available (COGL_FEATURE_POINT_SPRITE))
    GE (glEnable (GL_POINT_SPRITE));
#endif

  return TRUE;
}

void
_cogl_destroy_context (void)
{

  if (_context == NULL)
    return;

  _cogl_destroy_context_winsys (_context);

  _cogl_destroy_texture_units ();

  _cogl_free_framebuffer_stack (_context->framebuffer_stack);

  if (_context->current_path)
    cogl_handle_unref (_context->current_path);

  if (_context->default_gl_texture_2d_tex)
    cogl_handle_unref (_context->default_gl_texture_2d_tex);
  if (_context->default_gl_texture_rect_tex)
    cogl_handle_unref (_context->default_gl_texture_rect_tex);

  if (_context->opaque_color_pipeline)
    cogl_handle_unref (_context->opaque_color_pipeline);
  if (_context->blended_color_pipeline)
    cogl_handle_unref (_context->blended_color_pipeline);
  if (_context->texture_pipeline)
    cogl_handle_unref (_context->texture_pipeline);

  if (_context->blit_texture_pipeline)
    cogl_handle_unref (_context->blit_texture_pipeline);

  if (_context->journal_flush_attributes_array)
    g_array_free (_context->journal_flush_attributes_array, TRUE);
  if (_context->journal_clip_bounds)
    g_array_free (_context->journal_clip_bounds, TRUE);

  if (_context->polygon_vertices)
    g_array_free (_context->polygon_vertices, TRUE);

  if (_context->quad_buffer_indices_byte)
    cogl_handle_unref (_context->quad_buffer_indices_byte);
  if (_context->quad_buffer_indices)
    cogl_handle_unref (_context->quad_buffer_indices);

  if (_context->rectangle_byte_indices)
    cogl_object_unref (_context->rectangle_byte_indices);
  if (_context->rectangle_short_indices)
    cogl_object_unref (_context->rectangle_short_indices);

  if (_context->default_pipeline)
    cogl_handle_unref (_context->default_pipeline);

  if (_context->dummy_layer_dependant)
    cogl_handle_unref (_context->dummy_layer_dependant);
  if (_context->default_layer_n)
    cogl_handle_unref (_context->default_layer_n);
  if (_context->default_layer_0)
    cogl_handle_unref (_context->default_layer_0);

  if (_context->current_clip_stack_valid)
    _cogl_clip_stack_unref (_context->current_clip_stack);

  g_slist_free (_context->atlases);

  _cogl_bitmask_destroy (&_context->arrays_enabled);
  _cogl_bitmask_destroy (&_context->temp_bitmask);
  _cogl_bitmask_destroy (&_context->arrays_to_change);

  g_slist_free (_context->texture_types);
  g_slist_free (_context->buffer_types);

#ifdef HAVE_COGL_GLES2
  if (_context->flushed_modelview_stack)
    cogl_object_unref (_context->flushed_modelview_stack);
  if (_context->flushed_projection_stack)
    cogl_object_unref (_context->flushed_projection_stack);
#endif

#ifdef HAVE_COGL_GL
  g_hash_table_unref (_context->arbfp_cache);
#endif

  g_byte_array_free (_context->buffer_map_fallback_array, TRUE);

  g_free (_context);
}

CoglContext *
_cogl_context_get_default (void)
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
