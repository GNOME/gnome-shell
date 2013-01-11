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

#include "cogl-object.h"
#include "cogl-private.h"
#include "cogl-winsys-private.h"
#include "winsys/cogl-winsys-stub-private.h"
#include "cogl-profile.h"
#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-display-private.h"
#include "cogl-renderer-private.h"
#include "cogl-journal-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-3d-private.h"
#include "cogl-texture-rectangle-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl2-path.h"
#include "cogl-attribute-private.h"
#include "cogl1-context.h"
#include "cogl-gpu-info-private.h"
#include "cogl-config-private.h"
#include "cogl-error-private.h"

#include <string.h>
#include <stdlib.h>

#ifdef HAVE_COGL_GL
#include "cogl-pipeline-fragend-arbfp-private.h"
#endif

/* These aren't defined in the GLES headers */
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif

#ifndef GL_NUM_EXTENSIONS
#define GL_NUM_EXTENSIONS 0x821D
#endif

static void _cogl_context_free (CoglContext *context);

COGL_OBJECT_DEFINE (Context, context);

extern void
_cogl_create_context_driver (CoglContext *context);

static CoglContext *_cogl_context = NULL;

static void
_cogl_init_feature_overrides (CoglContext *ctx)
{
  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_VBOS)))
    ctx->private_feature_flags &= ~COGL_PRIVATE_FEATURE_VBOS;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_PBOS)))
    ctx->private_feature_flags &= ~COGL_PRIVATE_FEATURE_PBOS;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_ARBFP)))
    {
      ctx->feature_flags &= ~COGL_FEATURE_SHADERS_ARBFP;
      COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_ARBFP, FALSE);
    }

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_GLSL)))
    {
      ctx->feature_flags &= ~COGL_FEATURE_SHADERS_GLSL;
      COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_GLSL, FALSE);
    }

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_NPOT_TEXTURES)))
    {
      ctx->feature_flags &= ~(COGL_FEATURE_TEXTURE_NPOT |
                              COGL_FEATURE_TEXTURE_NPOT_BASIC |
                              COGL_FEATURE_TEXTURE_NPOT_MIPMAP |
                              COGL_FEATURE_TEXTURE_NPOT_REPEAT);
      COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_TEXTURE_NPOT, FALSE);
      COGL_FLAGS_SET (ctx->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_BASIC, FALSE);
      COGL_FLAGS_SET (ctx->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_MIPMAP, FALSE);
      COGL_FLAGS_SET (ctx->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_REPEAT, FALSE);
    }
}

const CoglWinsysVtable *
_cogl_context_get_winsys (CoglContext *context)
{
  return context->display->renderer->winsys_vtable;
}

/* For reference: There was some deliberation over whether to have a
 * constructor that could throw an exception but looking at standard
 * practices with several high level OO languages including python, C++,
 * C# Java and Ruby they all support exceptions in constructors and the
 * general consensus appears to be that throwing an exception is neater
 * than successfully constructing with an internal error status that
 * would then have to be explicitly checked via some form of ::is_ok()
 * method.
 */
CoglContext *
cogl_context_new (CoglDisplay *display,
                  CoglError **error)
{
  CoglContext *context;
  GLubyte default_texture_data[] = { 0xff, 0xff, 0xff, 0x0 };
  CoglBitmap *default_texture_bitmap;
  const CoglWinsysVtable *winsys;
  int i;
  CoglError *internal_error = NULL;

  _cogl_init ();

#ifdef COGL_ENABLE_PROFILE
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
  context = g_malloc0 (sizeof (CoglContext));

  /* Convert the context into an object immediately in case any of the
     code below wants to verify that the context pointer is a valid
     object */
  _cogl_context_object_new (context);

  /* XXX: Gross hack!
   * Currently everything in Cogl just assumes there is a default
   * context which it can access via _COGL_GET_CONTEXT() including
   * code used to construct a CoglContext. Until all of that code
   * has been updated to take an explicit context argument we have
   * to immediately make our pointer the default context.
   */
  _cogl_context = context;

  /* Init default values */
  memset (context->features, 0, sizeof (context->features));
  context->feature_flags = 0;
  context->private_feature_flags = 0;

  context->rectangle_state = COGL_WINSYS_RECTANGLE_STATE_UNKNOWN;

  memset (context->winsys_features, 0, sizeof (context->winsys_features));

  if (!display)
    {
      CoglRenderer *renderer = cogl_renderer_new ();
      if (!cogl_renderer_connect (renderer, error))
        {
          g_free (context);
          return NULL;
        }

      display = cogl_display_new (renderer, NULL);
    }
  else
    cogl_object_ref (display);

  if (!cogl_display_setup (display, error))
    {
      cogl_object_unref (display);
      g_free (context);
      return NULL;
    }

  context->display = display;

  /* This is duplicated data, but it's much more convenient to have
     the driver attached to the context and the value is accessed a
     lot throughout Cogl */
  context->driver = display->renderer->driver;

  /* Again this is duplicated data, but it convenient to be able
   * access these from the context. */
  context->driver_vtable = display->renderer->driver_vtable;
  context->texture_driver = display->renderer->texture_driver;

  winsys = _cogl_context_get_winsys (context);
  if (!winsys->context_init (context, error))
    {
      cogl_object_unref (display);
      g_free (context);
      return NULL;
    }

  context->attribute_name_states_hash =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  context->attribute_name_index_map = NULL;
  context->n_attribute_names = 0;

  /* The "cogl_color_in" attribute needs a deterministic name_index
   * so we make sure it's the first attribute name we register */
  _cogl_attribute_register_attribute_name (context, "cogl_color_in");


  context->uniform_names =
    g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);
  context->uniform_name_hash = g_hash_table_new (g_str_hash, g_str_equal);
  context->n_uniform_names = 0;

  /* Initialise the driver specific state */
  _cogl_init_feature_overrides (context);

  /* XXX: ONGOING BUG: Intel viewport scissor
   *
   * Intel gen6 drivers don't currently correctly handle offset
   * viewports, since primitives aren't clipped within the bounds of
   * the viewport.  To workaround this we push our own clip for the
   * viewport that will use scissoring to ensure we clip as expected.
   *
   * TODO: file a bug upstream!
   */
  if (context->gpu.driver_package == COGL_GPU_INFO_DRIVER_PACKAGE_MESA &&
      context->gpu.architecture == COGL_GPU_INFO_ARCHITECTURE_SANDYBRIDGE &&
      !getenv ("COGL_DISABLE_INTEL_VIEWPORT_SCISSORT_WORKAROUND"))
    context->needs_viewport_scissor_workaround = TRUE;
  else
    context->needs_viewport_scissor_workaround = FALSE;

  context->sampler_cache = _cogl_sampler_cache_new (context);

  _cogl_pipeline_init_default_pipeline ();
  _cogl_pipeline_init_default_layers ();
  _cogl_pipeline_init_state_hash_functions ();
  _cogl_pipeline_init_layer_state_hash_functions ();

  context->current_clip_stack_valid = FALSE;
  context->current_clip_stack = NULL;

  context->legacy_backface_culling_enabled = FALSE;

  cogl_matrix_init_identity (&context->identity_matrix);
  cogl_matrix_init_identity (&context->y_flip_matrix);
  cogl_matrix_scale (&context->y_flip_matrix, 1, -1, 1);

  context->flushed_matrix_mode = COGL_MATRIX_MODELVIEW;

  context->texture_units =
    g_array_new (FALSE, FALSE, sizeof (CoglTextureUnit));

  if ((context->private_feature_flags & COGL_PRIVATE_FEATURE_ANY_GL))
    {
      /* See cogl-pipeline.c for more details about why we leave texture unit 1
       * active by default... */
      context->active_texture_unit = 1;
      GE (context, glActiveTexture (GL_TEXTURE1));
    }

  context->legacy_fog_state.enabled = FALSE;

  context->opaque_color_pipeline = cogl_pipeline_new (context);
  context->blended_color_pipeline = cogl_pipeline_new (context);
  context->texture_pipeline = cogl_pipeline_new (context);
  context->codegen_header_buffer = g_string_new ("");
  context->codegen_source_buffer = g_string_new ("");
  context->codegen_boilerplate_buffer = g_string_new ("");
  context->source_stack = NULL;

  context->legacy_state_set = 0;

  context->default_gl_texture_2d_tex = NULL;
  context->default_gl_texture_3d_tex = NULL;
  context->default_gl_texture_rect_tex = NULL;

  context->framebuffers = NULL;
  context->current_draw_buffer = NULL;
  context->current_read_buffer = NULL;
  context->current_draw_buffer_state_flushed = 0;
  context->current_draw_buffer_changes = COGL_FRAMEBUFFER_STATE_ALL;

  context->swap_callback_closures =
    g_hash_table_new (g_direct_hash, g_direct_equal);

  COGL_TAILQ_INIT (&context->onscreen_events_queue);

  g_queue_init (&context->gles2_context_stack);

  context->journal_flush_attributes_array =
    g_array_new (TRUE, FALSE, sizeof (CoglAttribute *));
  context->journal_clip_bounds = NULL;

  context->polygon_vertices = g_array_new (FALSE, FALSE, sizeof (float));

  context->current_pipeline = NULL;
  context->current_pipeline_changes_since_flush = 0;
  context->current_pipeline_skip_gl_color = FALSE;

  _cogl_bitmask_init (&context->enabled_builtin_attributes);
  _cogl_bitmask_init (&context->enable_builtin_attributes_tmp);
  _cogl_bitmask_init (&context->enabled_texcoord_attributes);
  _cogl_bitmask_init (&context->enable_texcoord_attributes_tmp);
  _cogl_bitmask_init (&context->enabled_custom_attributes);
  _cogl_bitmask_init (&context->enable_custom_attributes_tmp);
  _cogl_bitmask_init (&context->changed_bits_tmp);

  context->max_texture_units = -1;
  context->max_activateable_texture_units = -1;

  context->current_fragment_program_type = COGL_PIPELINE_PROGRAM_TYPE_FIXED;
  context->current_vertex_program_type = COGL_PIPELINE_PROGRAM_TYPE_FIXED;
  context->current_gl_program = 0;

  context->current_gl_dither_enabled = TRUE;
  context->current_gl_color_mask = COGL_COLOR_MASK_ALL;

  context->gl_blend_enable_cache = FALSE;

  context->depth_test_enabled_cache = FALSE;
  context->depth_test_function_cache = COGL_DEPTH_TEST_FUNCTION_LESS;
  context->depth_writing_enabled_cache = TRUE;
  context->depth_range_near_cache = 0;
  context->depth_range_far_cache = 1;

  context->legacy_depth_test_enabled = FALSE;

  context->pipeline_cache = _cogl_pipeline_cache_new ();

  for (i = 0; i < COGL_BUFFER_BIND_TARGET_COUNT; i++)
    context->current_buffer[i] = NULL;

  context->window_buffer = NULL;
  context->framebuffer_stack = _cogl_create_framebuffer_stack ();

  /* XXX: In this case the Clutter backend is still responsible for
   * the OpenGL binding API and for creating onscreen framebuffers and
   * so we have to add a dummy framebuffer to represent the backend
   * owned window... */
  if (_cogl_context_get_winsys (context) == _cogl_winsys_stub_get_vtable ())
    {
      CoglOnscreen *window = _cogl_onscreen_new ();
      cogl_set_framebuffer (COGL_FRAMEBUFFER (window));
      cogl_object_unref (COGL_FRAMEBUFFER (window));
    }

  context->current_path = cogl2_path_new ();
  context->stencil_pipeline = cogl_pipeline_new (context);

  context->in_begin_gl_block = FALSE;

  context->quad_buffer_indices_byte = NULL;
  context->quad_buffer_indices = NULL;
  context->quad_buffer_indices_len = 0;

  context->rectangle_byte_indices = NULL;
  context->rectangle_short_indices = NULL;
  context->rectangle_short_indices_len = 0;

  context->texture_download_pipeline = NULL;
  context->blit_texture_pipeline = NULL;

#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES)
  if ((context->private_feature_flags & COGL_PRIVATE_FEATURE_ALPHA_TEST))
    /* The default for GL_ALPHA_TEST is to always pass which is equivalent to
     * the test being disabled therefore we assume that for all drivers there
     * will be no performance impact if we always leave the test enabled which
     * makes things a bit simpler for us. Under GLES2 the alpha test is
     * implemented in the fragment shader so there is no enable for it
     */
    GE (context, glEnable (GL_ALPHA_TEST));
#endif

#if defined (HAVE_COGL_GL)
  if ((context->driver == COGL_DRIVER_GL3))
    {
      GLuint vertex_array;

      /* In a forward compatible context, GL 3 doesn't support rendering
       * using the default vertex array object. Cogl doesn't use vertex
       * array objects yet so for now we just create a dummy array
       * object that we will use as our own default object. Eventually
       * it could be good to attach the vertex array objects to
       * CoglPrimitives */
      context->glGenVertexArrays (1, &vertex_array);
      context->glBindVertexArray (vertex_array);
    }
#endif

  context->current_modelview_entry = NULL;
  context->current_projection_entry = NULL;
  _cogl_matrix_entry_identity_init (&context->identity_entry);
  _cogl_matrix_entry_cache_init (&context->builtin_flushed_projection);
  _cogl_matrix_entry_cache_init (&context->builtin_flushed_modelview);

  default_texture_bitmap =
    cogl_bitmap_new_for_data (context,
                              1, 1, /* width/height */
                              COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                              4, /* rowstride */
                              default_texture_data);

  /* Create default textures used for fall backs */
  context->default_gl_texture_2d_tex =
    cogl_texture_2d_new_from_bitmap (default_texture_bitmap,
                                     /* internal format */
                                     COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                     NULL);

  /* If 3D or rectangle textures aren't supported then these will
   * return errors that we can simply ignore. */
  internal_error = NULL;
  context->default_gl_texture_3d_tex =
    cogl_texture_3d_new_from_bitmap (default_texture_bitmap,
                                     1, /* height */
                                     1, /* depth */
                                     COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                     &internal_error);
  if (internal_error)
    cogl_error_free (internal_error);

  internal_error = NULL;
  context->default_gl_texture_rect_tex =
    cogl_texture_rectangle_new_from_bitmap (default_texture_bitmap,
                                            COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                            &internal_error);
  if (internal_error)
    cogl_error_free (internal_error);

  cogl_object_unref (default_texture_bitmap);

  cogl_push_source (context->opaque_color_pipeline);

  context->atlases = NULL;
  g_hook_list_init (&context->atlas_reorganize_callbacks, sizeof (GHook));

  context->buffer_map_fallback_array = g_byte_array_new ();
  context->buffer_map_fallback_in_use = FALSE;

  /* As far as I can tell, GL_POINT_SPRITE doesn't have any effect
     unless GL_COORD_REPLACE is enabled for an individual layer.
     Therefore it seems like it should be ok to just leave it enabled
     all the time instead of having to have a set property on each
     pipeline to track whether any layers have point sprite coords
     enabled. We don't need to do this for GL3 or GLES2 because point
     sprites are handled using a builtin varying in the shader. */
  if ((context->private_feature_flags & COGL_PRIVATE_FEATURE_FIXED_FUNCTION) &&
      cogl_has_feature (context, COGL_FEATURE_ID_POINT_SPRITE))
    GE (context, glEnable (GL_POINT_SPRITE));

  COGL_TAILQ_INIT (&context->fences);

  return context;
}

static void
_cogl_context_free (CoglContext *context)
{
  const CoglWinsysVtable *winsys = _cogl_context_get_winsys (context);

  winsys->context_deinit (context);

  _cogl_free_framebuffer_stack (context->framebuffer_stack);

  if (context->current_path)
    cogl_handle_unref (context->current_path);

  if (context->default_gl_texture_2d_tex)
    cogl_object_unref (context->default_gl_texture_2d_tex);
  if (context->default_gl_texture_3d_tex)
    cogl_object_unref (context->default_gl_texture_3d_tex);
  if (context->default_gl_texture_rect_tex)
    cogl_object_unref (context->default_gl_texture_rect_tex);

  if (context->opaque_color_pipeline)
    cogl_object_unref (context->opaque_color_pipeline);
  if (context->blended_color_pipeline)
    cogl_object_unref (context->blended_color_pipeline);
  if (context->texture_pipeline)
    cogl_object_unref (context->texture_pipeline);

  if (context->blit_texture_pipeline)
    cogl_object_unref (context->blit_texture_pipeline);

  if (context->swap_callback_closures)
    g_hash_table_destroy (context->swap_callback_closures);

  g_warn_if_fail (context->gles2_context_stack.length == 0);

  if (context->journal_flush_attributes_array)
    g_array_free (context->journal_flush_attributes_array, TRUE);
  if (context->journal_clip_bounds)
    g_array_free (context->journal_clip_bounds, TRUE);

  if (context->polygon_vertices)
    g_array_free (context->polygon_vertices, TRUE);

  if (context->quad_buffer_indices_byte)
    cogl_object_unref (context->quad_buffer_indices_byte);
  if (context->quad_buffer_indices)
    cogl_object_unref (context->quad_buffer_indices);

  if (context->rectangle_byte_indices)
    cogl_object_unref (context->rectangle_byte_indices);
  if (context->rectangle_short_indices)
    cogl_object_unref (context->rectangle_short_indices);

  if (context->default_pipeline)
    cogl_object_unref (context->default_pipeline);

  if (context->dummy_layer_dependant)
    cogl_object_unref (context->dummy_layer_dependant);
  if (context->default_layer_n)
    cogl_object_unref (context->default_layer_n);
  if (context->default_layer_0)
    cogl_object_unref (context->default_layer_0);

  if (context->current_clip_stack_valid)
    _cogl_clip_stack_unref (context->current_clip_stack);

  g_slist_free (context->atlases);
  g_hook_list_clear (&context->atlas_reorganize_callbacks);

  _cogl_bitmask_destroy (&context->enabled_builtin_attributes);
  _cogl_bitmask_destroy (&context->enable_builtin_attributes_tmp);
  _cogl_bitmask_destroy (&context->enabled_texcoord_attributes);
  _cogl_bitmask_destroy (&context->enable_texcoord_attributes_tmp);
  _cogl_bitmask_destroy (&context->enabled_custom_attributes);
  _cogl_bitmask_destroy (&context->enable_custom_attributes_tmp);
  _cogl_bitmask_destroy (&context->changed_bits_tmp);

  if (context->current_modelview_entry)
    cogl_matrix_entry_unref (context->current_modelview_entry);
  if (context->current_projection_entry)
    cogl_matrix_entry_unref (context->current_projection_entry);
  _cogl_matrix_entry_cache_destroy (&context->builtin_flushed_projection);
  _cogl_matrix_entry_cache_destroy (&context->builtin_flushed_modelview);

  _cogl_pipeline_cache_free (context->pipeline_cache);

  _cogl_sampler_cache_free (context->sampler_cache);

  _cogl_destroy_texture_units ();

  g_ptr_array_free (context->uniform_names, TRUE);
  g_hash_table_destroy (context->uniform_name_hash);

  g_hash_table_destroy (context->attribute_name_states_hash);
  g_array_free (context->attribute_name_index_map, TRUE);

  g_byte_array_free (context->buffer_map_fallback_array, TRUE);

  cogl_object_unref (context->display);

  g_free (context);
}

CoglContext *
_cogl_context_get_default (void)
{
  CoglError *error = NULL;
  /* Create if doesn't exist yet */
  if (_cogl_context == NULL)
    {
      _cogl_context = cogl_context_new (NULL, &error);
      if (!_cogl_context)
        {
          g_warning ("Failed to create default context: %s",
                     error->message);
          cogl_error_free (error);
        }
    }

  return _cogl_context;
}

CoglDisplay *
cogl_context_get_display (CoglContext *context)
{
  return context->display;
}

CoglRenderer *
cogl_context_get_renderer (CoglContext *context)
{
  return context->display->renderer;
}

#ifdef COGL_HAS_EGL_SUPPORT
EGLDisplay
cogl_egl_context_get_egl_display (CoglContext *context)
{
  const CoglWinsysVtable *winsys = _cogl_context_get_winsys (context);

  /* This should only be called for EGL contexts */
  _COGL_RETURN_VAL_IF_FAIL (winsys->context_egl_get_egl_display != NULL, NULL);

  return winsys->context_egl_get_egl_display (context);
}
#endif

CoglBool
_cogl_context_update_features (CoglContext *context,
                               CoglError **error)
{
  return context->driver_vtable->update_features (context, error);
}

void
_cogl_context_set_current_projection_entry (CoglContext *context,
                                            CoglMatrixEntry *entry)
{
  cogl_matrix_entry_ref (entry);
  if (context->current_projection_entry)
    cogl_matrix_entry_unref (context->current_projection_entry);
  context->current_projection_entry = entry;
}

void
_cogl_context_set_current_modelview_entry (CoglContext *context,
                                           CoglMatrixEntry *entry)
{
  cogl_matrix_entry_ref (entry);
  if (context->current_modelview_entry)
    cogl_matrix_entry_unref (context->current_modelview_entry);
  context->current_modelview_entry = entry;
}

char **
_cogl_context_get_gl_extensions (CoglContext *context)
{
  const char *env_disabled_extensions;
  char **ret;

  /* In GL 3, querying GL_EXTENSIONS is deprecated so we have to build
   * the array using glGetStringi instead */
#ifdef HAVE_COGL_GL
  if (context->driver == COGL_DRIVER_GL3)
    {
      int num_extensions, i;

      context->glGetIntegerv (GL_NUM_EXTENSIONS, &num_extensions);

      ret = g_malloc (sizeof (char *) * (num_extensions + 1));

      for (i = 0; i < num_extensions; i++)
        {
          const char *ext =
            (const char *) context->glGetStringi (GL_EXTENSIONS, i);
          ret[i] = g_strdup (ext);
        }

      ret[num_extensions] = NULL;
    }
  else
#endif
    {
      const char *all_extensions =
        (const char *) context->glGetString (GL_EXTENSIONS);

      ret = g_strsplit (all_extensions, " ", 0 /* max tokens */);
    }

  if ((env_disabled_extensions = g_getenv ("COGL_DISABLE_GL_EXTENSIONS"))
      || _cogl_config_disable_gl_extensions)
    {
      char **split_env_disabled_extensions;
      char **split_conf_disabled_extensions;
      char **src, **dst;

      if (env_disabled_extensions)
        split_env_disabled_extensions =
          g_strsplit (env_disabled_extensions,
                      ",",
                      0 /* no max tokens */);
      else
        split_env_disabled_extensions = NULL;

      if (_cogl_config_disable_gl_extensions)
        split_conf_disabled_extensions =
          g_strsplit (_cogl_config_disable_gl_extensions,
                      ",",
                      0 /* no max tokens */);
      else
        split_conf_disabled_extensions = NULL;

      for (dst = ret, src = ret;
           *src;
           src++)
        {
          char **d;

          if (split_env_disabled_extensions)
            for (d = split_env_disabled_extensions; *d; d++)
              if (!strcmp (*src, *d))
                goto disabled;
          if (split_conf_disabled_extensions)
            for (d = split_conf_disabled_extensions; *d; d++)
              if (!strcmp (*src, *d))
                goto disabled;

          *(dst++) = *src;
          continue;

        disabled:
          g_free (*src);
          continue;
        }

      *dst = NULL;

      if (split_env_disabled_extensions)
        g_strfreev (split_env_disabled_extensions);
      if (split_conf_disabled_extensions)
        g_strfreev (split_conf_disabled_extensions);
    }

  return ret;
}

const char *
_cogl_context_get_gl_version (CoglContext *context)
{
  const char *version_override;

  if ((version_override = g_getenv ("COGL_OVERRIDE_GL_VERSION")))
    return version_override;
  else if (_cogl_config_override_gl_version)
    return _cogl_config_override_gl_version;
  else
    return (const char *) context->glGetString (GL_VERSION);

}

int64_t
cogl_get_clock_time (CoglContext *context)
{
  const CoglWinsysVtable *winsys = _cogl_context_get_winsys (context);

  if (winsys->context_get_clock_time)
    return winsys->context_get_clock_time (context);
  else
    return 0;
}
