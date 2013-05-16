/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#include <config.h>

#include <string.h>
#include <math.h>
#include <stdlib.h>

#define COGL_VERSION_MIN_REQUIRED COGL_VERSION_1_4

#include "cogl-i18n-private.h"
#include "cogl-debug.h"
#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-winsys-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-matrix-private.h"
#include "cogl-journal-private.h"
#include "cogl-bitmap-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-driver.h"
#include "cogl-attribute-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-renderer-private.h"
#include "cogl-config-private.h"
#include "cogl-private.h"
#include "cogl1-context.h"
#include "cogl-offscreen.h"
#include "cogl-attribute-gl-private.h"
#include "cogl-clutter.h"

CoglFuncPtr
cogl_get_proc_address (const char* name)
{
  _COGL_GET_CONTEXT (ctx, NULL);

  return _cogl_renderer_get_proc_address (ctx->display->renderer, name, FALSE);
}

CoglBool
_cogl_check_extension (const char *name, char * const *ext)
{
  while (*ext)
    if (!strcmp (name, *ext))
      return TRUE;
    else
      ext++;

  return FALSE;
}

/* XXX: This has been deprecated as public API */
CoglBool
cogl_check_extension (const char *name, const char *ext)
{
  return cogl_clutter_check_extension (name, ext);
}

/* XXX: it's expected that we'll deprecated this with
 * cogl_framebuffer_clear at some point. */
void
cogl_clear (const CoglColor *color, unsigned long buffers)
{
  cogl_framebuffer_clear (cogl_get_draw_framebuffer (), buffers, color);
}

/* XXX: This API has been deprecated */
void
cogl_set_depth_test_enabled (CoglBool setting)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->legacy_depth_test_enabled == setting)
    return;

  ctx->legacy_depth_test_enabled = setting;
  if (ctx->legacy_depth_test_enabled)
    ctx->legacy_state_set++;
  else
    ctx->legacy_state_set--;
}

/* XXX: This API has been deprecated */
CoglBool
cogl_get_depth_test_enabled (void)
{
  _COGL_GET_CONTEXT (ctx, FALSE);
  return ctx->legacy_depth_test_enabled;
}

void
cogl_set_backface_culling_enabled (CoglBool setting)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->legacy_backface_culling_enabled == setting)
    return;

  ctx->legacy_backface_culling_enabled = setting;

  if (ctx->legacy_backface_culling_enabled)
    ctx->legacy_state_set++;
  else
    ctx->legacy_state_set--;
}

CoglBool
cogl_get_backface_culling_enabled (void)
{
  _COGL_GET_CONTEXT (ctx, FALSE);

  return ctx->legacy_backface_culling_enabled;
}

void
cogl_set_source_color (const CoglColor *color)
{
  CoglPipeline *pipeline;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (cogl_color_get_alpha_byte (color) == 0xff)
    {
      cogl_pipeline_set_color (ctx->opaque_color_pipeline, color);
      pipeline = ctx->opaque_color_pipeline;
    }
  else
    {
      CoglColor premultiplied = *color;
      cogl_color_premultiply (&premultiplied);
      cogl_pipeline_set_color (ctx->blended_color_pipeline, &premultiplied);
      pipeline = ctx->blended_color_pipeline;
    }

  cogl_set_source (pipeline);
}

void
cogl_set_viewport (int x,
                   int y,
                   int width,
                   int height)
{
  CoglFramebuffer *framebuffer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = cogl_get_draw_framebuffer ();

  cogl_framebuffer_set_viewport (framebuffer,
                                 x,
                                 y,
                                 width,
                                 height);
}

/* XXX: This should be deprecated, and we should expose a way to also
 * specify an x and y viewport offset */
void
cogl_viewport (unsigned int width,
	       unsigned int height)
{
  cogl_set_viewport (0, 0, width, height);
}

CoglFeatureFlags
cogl_get_features (void)
{
  _COGL_GET_CONTEXT (ctx, 0);

  return ctx->feature_flags;
}

CoglBool
cogl_features_available (CoglFeatureFlags features)
{
  _COGL_GET_CONTEXT (ctx, 0);

  return (ctx->feature_flags & features) == features;
}

CoglBool
cogl_has_feature (CoglContext *ctx, CoglFeatureID feature)
{
  return COGL_FLAGS_GET (ctx->features, feature);
}

CoglBool
cogl_has_features (CoglContext *ctx, ...)
{
  va_list args;
  CoglFeatureID feature;

  va_start (args, ctx);
  while ((feature = va_arg (args, CoglFeatureID)))
    if (!cogl_has_feature (ctx, feature))
      return FALSE;
  va_end (args);

  return TRUE;
}

void
cogl_foreach_feature (CoglContext *ctx,
                      CoglFeatureCallback callback,
                      void *user_data)
{
  int i;
  for (i = 0; i < _COGL_N_FEATURE_IDS; i++)
    if (COGL_FLAGS_GET (ctx->features, i))
      callback (i, user_data);
}

/* XXX: This function should either be replaced with one returning
 * integers, or removed/deprecated and make the
 * _cogl_framebuffer_get_viewport* functions public.
 */
void
cogl_get_viewport (float viewport[4])
{
  CoglFramebuffer *framebuffer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = cogl_get_draw_framebuffer ();
  cogl_framebuffer_get_viewport4fv (framebuffer, viewport);
}

void
cogl_get_bitmasks (int *red,
                   int *green,
                   int *blue,
                   int *alpha)
{
  CoglFramebuffer *framebuffer;

  framebuffer = cogl_get_draw_framebuffer ();

  if (red)
    *red = cogl_framebuffer_get_red_bits (framebuffer);

  if (green)
    *green = cogl_framebuffer_get_green_bits (framebuffer);

  if (blue)
    *blue = cogl_framebuffer_get_blue_bits (framebuffer);

  if (alpha)
    *alpha = cogl_framebuffer_get_alpha_bits (framebuffer);
}

void
cogl_set_fog (const CoglColor *fog_color,
              CoglFogMode      mode,
              float            density,
              float            z_near,
              float            z_far)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->legacy_fog_state.enabled == FALSE)
    ctx->legacy_state_set++;

  ctx->legacy_fog_state.enabled = TRUE;
  ctx->legacy_fog_state.color = *fog_color;
  ctx->legacy_fog_state.mode = mode;
  ctx->legacy_fog_state.density = density;
  ctx->legacy_fog_state.z_near = z_near;
  ctx->legacy_fog_state.z_far = z_far;
}

void
cogl_disable_fog (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->legacy_fog_state.enabled == TRUE)
    ctx->legacy_state_set--;

  ctx->legacy_fog_state.enabled = FALSE;
}

void
cogl_flush (void)
{
  GList *l;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (l = ctx->framebuffers; l; l = l->next)
    _cogl_framebuffer_flush_journal (l->data);
}

void
cogl_read_pixels (int x,
                  int y,
                  int width,
                  int height,
                  CoglReadPixelsFlags source,
                  CoglPixelFormat format,
                  uint8_t *pixels)
{
  int bpp = _cogl_pixel_format_get_bytes_per_pixel (format);
  CoglBitmap *bitmap;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  bitmap = cogl_bitmap_new_for_data (ctx,
                                     width, height,
                                     format,
                                     bpp * width, /* rowstride */
                                     pixels);
  cogl_framebuffer_read_pixels_into_bitmap (_cogl_get_read_framebuffer (),
                                            x, y,
                                            source,
                                            bitmap);
  cogl_object_unref (bitmap);
}

void
cogl_begin_gl (void)
{
  CoglPipeline *pipeline;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->in_begin_gl_block)
    {
      static CoglBool shown = FALSE;
      if (!shown)
        g_warning ("You should not nest cogl_begin_gl/cogl_end_gl blocks");
      shown = TRUE;
      return;
    }
  ctx->in_begin_gl_block = TRUE;

  /* Flush all batched primitives */
  cogl_flush ();

  /* Flush framebuffer state, including clip state, modelview and
   * projection matrix state
   *
   * NB: _cogl_framebuffer_flush_state may disrupt various state (such
   * as the pipeline state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_framebuffer_flush_state (cogl_get_draw_framebuffer (),
                                 _cogl_get_read_framebuffer (),
                                 COGL_FRAMEBUFFER_STATE_ALL);

  /* Setup the state for the current pipeline */

  /* We considered flushing a specific, minimal pipeline here to try and
   * simplify the GL state, but decided to avoid special cases and second
   * guessing what would be actually helpful.
   *
   * A user should instead call cogl_set_source_color4ub() before
   * cogl_begin_gl() to simplify the state flushed.
   *
   * XXX: note defining n_tex_coord_attribs using
   * cogl_pipeline_get_n_layers is a hack, but the problem is that
   * n_tex_coord_attribs is usually defined when drawing a primitive
   * which isn't happening here.
   *
   * Maybe it would be more useful if this code did flush the
   * opaque_color_pipeline and then call into cogl-pipeline-opengl.c to then
   * restore all state for the material's backend back to default OpenGL
   * values.
   */
  pipeline = cogl_get_source ();
  _cogl_pipeline_flush_gl_state (ctx,
                                 pipeline,
                                 cogl_get_draw_framebuffer (),
                                 FALSE,
                                 FALSE);

  /* Disable any cached vertex arrays */
  _cogl_gl_disable_all_attributes (ctx);
}

void
cogl_end_gl (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!ctx->in_begin_gl_block)
    {
      static CoglBool shown = FALSE;
      if (!shown)
        g_warning ("cogl_end_gl is being called before cogl_begin_gl");
      shown = TRUE;
      return;
    }
  ctx->in_begin_gl_block = FALSE;
}

void
cogl_push_matrix (void)
{
  cogl_framebuffer_push_matrix (cogl_get_draw_framebuffer ());
}

void
cogl_pop_matrix (void)
{
  cogl_framebuffer_pop_matrix (cogl_get_draw_framebuffer ());
}

void
cogl_scale (float x, float y, float z)
{
  cogl_framebuffer_scale (cogl_get_draw_framebuffer (), x, y, z);
}

void
cogl_translate (float x, float y, float z)
{
  cogl_framebuffer_translate (cogl_get_draw_framebuffer (), x, y, z);
}

void
cogl_rotate (float angle, float x, float y, float z)
{
  cogl_framebuffer_rotate (cogl_get_draw_framebuffer (), angle, x, y, z);
}

void
cogl_transform (const CoglMatrix *matrix)
{
  cogl_framebuffer_transform (cogl_get_draw_framebuffer (), matrix);
}

void
cogl_perspective (float fov_y,
		  float aspect,
		  float z_near,
		  float z_far)
{
  cogl_framebuffer_perspective (cogl_get_draw_framebuffer (),
                                fov_y, aspect, z_near, z_far);
}

void
cogl_frustum (float        left,
	      float        right,
	      float        bottom,
	      float        top,
	      float        z_near,
	      float        z_far)
{
  cogl_framebuffer_frustum (cogl_get_draw_framebuffer (),
                            left, right, bottom, top, z_near, z_far);
}

void
cogl_ortho (float left,
	    float right,
	    float bottom,
	    float top,
	    float near,
	    float far)
{
  cogl_framebuffer_orthographic (cogl_get_draw_framebuffer (),
                                 left, top, right, bottom, near, far);
}

void
cogl_get_modelview_matrix (CoglMatrix *matrix)
{
  cogl_framebuffer_get_modelview_matrix (cogl_get_draw_framebuffer (), matrix);
}

void
cogl_set_modelview_matrix (CoglMatrix *matrix)
{
  cogl_framebuffer_set_modelview_matrix (cogl_get_draw_framebuffer (), matrix);
}

void
cogl_get_projection_matrix (CoglMatrix *matrix)
{
  cogl_framebuffer_get_projection_matrix (cogl_get_draw_framebuffer (), matrix);
}

void
cogl_set_projection_matrix (CoglMatrix *matrix)
{
  cogl_framebuffer_set_projection_matrix (cogl_get_draw_framebuffer (), matrix);
}

uint32_t
_cogl_driver_error_quark (void)
{
  return g_quark_from_static_string ("cogl-driver-error-quark");
}

typedef struct _CoglSourceState
{
  CoglPipeline *pipeline;
  int push_count;
  /* If this is TRUE then the pipeline will be copied and the legacy
     state will be applied whenever the pipeline is used. This is
     necessary because some internal Cogl code expects to be able to
     push a temporary pipeline to put GL into a known state. For that
     to work it also needs to prevent applying the legacy state */
  CoglBool enable_legacy;
} CoglSourceState;

static void
_push_source_real (CoglPipeline *pipeline, CoglBool enable_legacy)
{
  CoglSourceState *top = g_slice_new (CoglSourceState);
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  top->pipeline = cogl_object_ref (pipeline);
  top->enable_legacy = enable_legacy;
  top->push_count = 1;

  ctx->source_stack = g_list_prepend (ctx->source_stack, top);
}

/* FIXME: This should take a context pointer for Cogl 2.0 Technically
 * we could make it so we can retrieve a context reference from the
 * pipeline, but this would not by symmetric with cogl_pop_source. */
void
cogl_push_source (void *material_or_pipeline)
{
  CoglPipeline *pipeline = COGL_PIPELINE (material_or_pipeline);

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  _cogl_push_source (pipeline, TRUE);
}

/* This internal version of cogl_push_source is the same except it
   never applies the legacy state. Some parts of Cogl use this
   internally to set a temporary pipeline with a known state */
void
_cogl_push_source (CoglPipeline *pipeline, CoglBool enable_legacy)
{
  CoglSourceState *top;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  if (ctx->source_stack)
    {
      top = ctx->source_stack->data;
      if (top->pipeline == pipeline && top->enable_legacy == enable_legacy)
        {
          top->push_count++;
          return;
        }
      else
        _push_source_real (pipeline, enable_legacy);
    }
  else
    _push_source_real (pipeline, enable_legacy);
}

/* FIXME: This needs to take a context pointer for Cogl 2.0 */
void
cogl_pop_source (void)
{
  CoglSourceState *top;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _COGL_RETURN_IF_FAIL (ctx->source_stack);

  top = ctx->source_stack->data;
  top->push_count--;
  if (top->push_count == 0)
    {
      cogl_object_unref (top->pipeline);
      g_slice_free (CoglSourceState, top);
      ctx->source_stack = g_list_delete_link (ctx->source_stack,
                                              ctx->source_stack);
    }
}

/* FIXME: This needs to take a context pointer for Cogl 2.0 */
void *
cogl_get_source (void)
{
  CoglSourceState *top;

  _COGL_GET_CONTEXT (ctx, NULL);

  _COGL_RETURN_VAL_IF_FAIL (ctx->source_stack, NULL);

  top = ctx->source_stack->data;
  return top->pipeline;
}

CoglBool
_cogl_get_enable_legacy_state (void)
{
  CoglSourceState *top;

  _COGL_GET_CONTEXT (ctx, FALSE);

  _COGL_RETURN_VAL_IF_FAIL (ctx->source_stack, FALSE);

  top = ctx->source_stack->data;
  return top->enable_legacy;
}

void
cogl_set_source (void *material_or_pipeline)
{
  CoglSourceState *top;
  CoglPipeline *pipeline = COGL_PIPELINE (material_or_pipeline);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));
  _COGL_RETURN_IF_FAIL (ctx->source_stack);

  top = ctx->source_stack->data;
  if (top->pipeline == pipeline && top->enable_legacy)
    return;

  if (top->push_count == 1)
    {
      /* NB: top->pipeline may be only thing keeping pipeline
       * alive currently so ref pipeline first... */
      cogl_object_ref (pipeline);
      cogl_object_unref (top->pipeline);
      top->pipeline = pipeline;
      top->enable_legacy = TRUE;
    }
  else
    {
      top->push_count--;
      cogl_push_source (pipeline);
    }
}

void
cogl_set_source_texture (CoglTexture *texture)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _COGL_RETURN_IF_FAIL (texture != NULL);

  cogl_pipeline_set_layer_texture (ctx->texture_pipeline, 0, texture);
  cogl_set_source (ctx->texture_pipeline);
}

void
cogl_set_source_color4ub (uint8_t red,
                          uint8_t green,
                          uint8_t blue,
                          uint8_t alpha)
{
  CoglColor c = { 0, };

  cogl_color_init_from_4ub (&c, red, green, blue, alpha);
  cogl_set_source_color (&c);
}

void
cogl_set_source_color4f (float red,
                         float green,
                         float blue,
                         float alpha)
{
  CoglColor c = { 0, };

  cogl_color_init_from_4f (&c, red, green, blue, alpha);
  cogl_set_source_color (&c);
}

/* Scale from OpenGL normalized device coordinates (ranging from -1 to 1)
 * to Cogl window/framebuffer coordinates (ranging from 0 to buffer-size) with
 * (0,0) being top left. */
#define VIEWPORT_TRANSFORM_X(x, vp_origin_x, vp_width) \
    (  ( ((x) + 1.0) * ((vp_width) / 2.0) ) + (vp_origin_x)  )
/* Note: for Y we first flip all coordinates around the X axis while in
 * normalized device coodinates */
#define VIEWPORT_TRANSFORM_Y(y, vp_origin_y, vp_height) \
    (  ( ((-(y)) + 1.0) * ((vp_height) / 2.0) ) + (vp_origin_y)  )

/* Transform a homogeneous vertex position from model space to Cogl
 * window coordinates (with 0,0 being top left) */
void
_cogl_transform_point (const CoglMatrix *matrix_mv,
                       const CoglMatrix *matrix_p,
                       const float *viewport,
                       float *x,
                       float *y)
{
  float z = 0;
  float w = 1;

  /* Apply the modelview matrix transform */
  cogl_matrix_transform_point (matrix_mv, x, y, &z, &w);

  /* Apply the projection matrix transform */
  cogl_matrix_transform_point (matrix_p, x, y, &z, &w);

  /* Perform perspective division */
  *x /= w;
  *y /= w;

  /* Apply viewport transform */
  *x = VIEWPORT_TRANSFORM_X (*x, viewport[0], viewport[2]);
  *y = VIEWPORT_TRANSFORM_Y (*y, viewport[1], viewport[3]);
}

#undef VIEWPORT_TRANSFORM_X
#undef VIEWPORT_TRANSFORM_Y

uint32_t
_cogl_system_error_quark (void)
{
  return g_quark_from_static_string ("cogl-system-error-quark");
}

void
_cogl_init (void)
{
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
#ifdef ENABLE_NLS
      bindtextdomain (GETTEXT_PACKAGE, COGL_LOCALEDIR);
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

#ifdef COGL_HAS_GTYPE_SUPPORT
      g_type_init ();
#endif

      _cogl_config_read ();
      _cogl_debug_check_environment ();
      initialized = TRUE;
    }
}

/*
 * Returns the number of bytes-per-pixel of a given format. The bpp
 * can be extracted from the least significant nibble of the pixel
 * format (see CoglPixelFormat).
 *
 * The mapping is the following (see discussion on bug #660188):
 *
 * 0     = undefined
 * 1, 8  = 1 bpp (e.g. A_8, G_8)
 * 2     = 3 bpp, aligned (e.g. 888)
 * 3     = 4 bpp, aligned (e.g. 8888)
 * 4-6   = 2 bpp, not aligned (e.g. 565, 4444, 5551)
 * 7     = undefined yuv
 * 9     = 2 bpp, aligned
 * 10     = undefined
 * 11     = undefined
 * 12    = 3 bpp, not aligned
 * 13    = 4 bpp, not aligned (e.g. 2101010)
 * 14-15 = undefined
 */
int
_cogl_pixel_format_get_bytes_per_pixel (CoglPixelFormat format)
{
  int bpp_lut[] = { 0, 1, 3, 4,
                    2, 2, 2, 0,
                    1, 2, 0, 0,
                    3, 4, 0, 0 };

  return bpp_lut [format & 0xf];
}

/* Note: this also refers to the mapping defined above for
 * _cogl_pixel_format_get_bytes_per_pixel() */
CoglBool
_cogl_pixel_format_is_endian_dependant (CoglPixelFormat format)
{
  int aligned_lut[] = { -1, 1,  1,  1,
                         0, 0,  0, -1,
                         1, 1, -1, -1,
                         0, 0, -1, -1};
  int aligned = aligned_lut[format & 0xf];

  _COGL_RETURN_VAL_IF_FAIL (aligned != -1, FALSE);

  /* NB: currently checking whether the format components are aligned
   * or not determines whether the format is endian dependent or not.
   * In the future though we might consider adding formats with
   * aligned components that are also endian independant. */

  return aligned;
}
