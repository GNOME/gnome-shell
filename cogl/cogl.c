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

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <gmodule.h>

#include "cogl-debug.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context.h"
#include "cogl-material-private.h"
#include "cogl-winsys.h"
#include "cogl-draw-buffer-private.h"
#include "cogl-matrix-private.h"

#if defined (HAVE_COGL_GLES2) || defined (HAVE_COGL_GLES)
#include "cogl-gles2-wrapper.h"
#endif

#ifdef HAVE_COGL_GL
#define glClientActiveTexture ctx->drv.pf_glClientActiveTexture
#endif

#ifdef COGL_GL_DEBUG
/* GL error to string conversion */
static const struct {
  GLuint error_code;
  const gchar *error_string;
} gl_errors[] = {
  { GL_NO_ERROR,          "No error" },
  { GL_INVALID_ENUM,      "Invalid enumeration value" },
  { GL_INVALID_VALUE,     "Invalid value" },
  { GL_INVALID_OPERATION, "Invalid operation" },
  { GL_STACK_OVERFLOW,    "Stack overflow" },
  { GL_STACK_UNDERFLOW,   "Stack underflow" },
  { GL_OUT_OF_MEMORY,     "Out of memory" },

#ifdef GL_INVALID_FRAMEBUFFER_OPERATION_EXT
  { GL_INVALID_FRAMEBUFFER_OPERATION_EXT, "Invalid framebuffer operation" }
#endif
};

static const guint n_gl_errors = G_N_ELEMENTS (gl_errors);

const gchar *
cogl_gl_error_to_string (GLenum error_code)
{
  gint i;

  for (i = 0; i < n_gl_errors; i++)
    {
      if (gl_errors[i].error_code == error_code)
        return gl_errors[i].error_string;
    }

  return "Unknown GL error";
}
#endif /* COGL_GL_DEBUG */

CoglFuncPtr
cogl_get_proc_address (const gchar* name)
{
  void *address;
  static GModule *module = NULL;

  address = _cogl_winsys_get_proc_address (name);
  if (address)
    return address;

  /* this should find the right function if the program is linked against a
   * library providing it */
  if (G_UNLIKELY (module == NULL))
    module = g_module_open (NULL, 0);

  if (module)
    {
      gpointer symbol;

      if (g_module_symbol (module, name, &symbol))
        return symbol;
    }

  return NULL;
}

void
cogl_clear (const CoglColor *color, gulong buffers)
{
  GLbitfield gl_buffers = 0;

  COGL_NOTE (DRAW, "Clear begin");

  _cogl_journal_flush ();

  /* NB: _cogl_draw_buffer_flush_state may disrupt various state (such
   * as the material state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_draw_buffer_flush_state (_cogl_get_draw_buffer (), 0);

  if (buffers & COGL_BUFFER_BIT_COLOR)
    {
      GE( glClearColor (cogl_color_get_red_float (color),
			cogl_color_get_green_float (color),
			cogl_color_get_blue_float (color),
			cogl_color_get_alpha_float (color)) );
      gl_buffers |= GL_COLOR_BUFFER_BIT;
    }

  if (buffers & COGL_BUFFER_BIT_DEPTH)
    gl_buffers |= GL_DEPTH_BUFFER_BIT;

  if (buffers & COGL_BUFFER_BIT_STENCIL)
    gl_buffers |= GL_STENCIL_BUFFER_BIT;

  if (!gl_buffers)
    {
      static gboolean shown = FALSE;

      if (!shown)
        {
	  g_warning ("You should specify at least one auxiliary buffer "
                     "when calling cogl_clear");
        }

      return;
    }

  glClear (gl_buffers);

  COGL_NOTE (DRAW, "Clear end");
}

static inline gboolean
cogl_toggle_flag (CoglContext *ctx,
		  gulong new_flags,
		  gulong flag,
		  GLenum gl_flag)
{
  /* Toggles and caches a single enable flag on or off
   * by comparing to current state
   */
  if (new_flags & flag)
    {
      if (!(ctx->enable_flags & flag))
	{
	  GE( glEnable (gl_flag) );
	  ctx->enable_flags |= flag;
	  return TRUE;
	}
    }
  else if (ctx->enable_flags & flag)
    {
      GE( glDisable (gl_flag) );
      ctx->enable_flags &= ~flag;
    }

  return FALSE;
}

static inline gboolean
cogl_toggle_client_flag (CoglContext *ctx,
			 gulong new_flags,
			 gulong flag,
			 GLenum gl_flag)
{
  /* Toggles and caches a single client-side enable flag
   * on or off by comparing to current state
   */
  if (new_flags & flag)
    {
      if (!(ctx->enable_flags & flag))
	{
	  GE( glEnableClientState (gl_flag) );
	  ctx->enable_flags |= flag;
	  return TRUE;
	}
    }
  else if (ctx->enable_flags & flag)
    {
      GE( glDisableClientState (gl_flag) );
      ctx->enable_flags &= ~flag;
    }

  return FALSE;
}

void
cogl_enable (gulong flags)
{
  /* This function essentially caches glEnable state() in the
   * hope of lessening number GL traffic.
  */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_toggle_flag (ctx, flags,
                    COGL_ENABLE_BLEND,
                    GL_BLEND);

  cogl_toggle_flag (ctx, flags,
                    COGL_ENABLE_BACKFACE_CULLING,
                    GL_CULL_FACE);

  cogl_toggle_client_flag (ctx, flags,
			   COGL_ENABLE_VERTEX_ARRAY,
			   GL_VERTEX_ARRAY);

  cogl_toggle_client_flag (ctx, flags,
			   COGL_ENABLE_COLOR_ARRAY,
			   GL_COLOR_ARRAY);
}

gulong
cogl_get_enable ()
{
  _COGL_GET_CONTEXT (ctx, 0);

  return ctx->enable_flags;
}

void
cogl_set_depth_test_enabled (gboolean setting)
{
  /* Currently the journal can't track changes to depth state... */
  _cogl_journal_flush ();

  if (setting)
    {
      glEnable (GL_DEPTH_TEST);
      glDepthFunc (GL_LEQUAL);
    }
  else
    glDisable (GL_DEPTH_TEST);
}

gboolean
cogl_get_depth_test_enabled (void)
{
  return glIsEnabled (GL_DEPTH_TEST) == GL_TRUE ? TRUE : FALSE;
}

void
cogl_set_backface_culling_enabled (gboolean setting)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->enable_backface_culling == setting)
    return;

  /* Currently the journal can't track changes to backface culling state... */
  _cogl_journal_flush ();

  ctx->enable_backface_culling = setting;
}

gboolean
cogl_get_backface_culling_enabled (void)
{
  _COGL_GET_CONTEXT (ctx, FALSE);

  return ctx->enable_backface_culling;
}

void
_cogl_flush_face_winding (void)
{
  CoglFrontWinding winding;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* The front face winding doesn't matter if we aren't performing any
   * backface culling... */
  if (!ctx->enable_backface_culling)
    return;

  /* NB: We use a clockwise face winding order when drawing offscreen because
   * all offscreen rendering is done upside down resulting in reversed winding
   * for all triangles.
   */
  if (cogl_is_offscreen (_cogl_get_draw_buffer ()))
    winding = COGL_FRONT_WINDING_CLOCKWISE;
  else
    winding = COGL_FRONT_WINDING_COUNTER_CLOCKWISE;

  if (winding != ctx->flushed_front_winding)
    {

      if (winding == COGL_FRONT_WINDING_CLOCKWISE)
        GE (glFrontFace (GL_CW));
      else
        GE (glFrontFace (GL_CCW));
      ctx->flushed_front_winding = winding;
    }
}

void
cogl_set_source_color (const CoglColor *color)
{
  CoglColor premultiplied;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* In case cogl_set_source_texture was previously used... */
  cogl_material_remove_layer (ctx->default_material, 0);

  premultiplied = *color;
  cogl_color_premultiply (&premultiplied);
  cogl_material_set_color (ctx->default_material, &premultiplied);

  cogl_set_source (ctx->default_material);
}

void
cogl_set_viewport (int x,
                   int y,
                   int width,
                   int height)
{
  CoglHandle draw_buffer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  draw_buffer = _cogl_get_draw_buffer ();

  _cogl_draw_buffer_set_viewport (draw_buffer,
                                  x,
                                  y,
                                  width,
                                  height);
}

/* XXX: This should be deprecated, and we should expose a way to also
 * specify an x and y viewport offset */
void
cogl_viewport (guint width,
	       guint height)
{
  cogl_set_viewport (0, 0, width, height);
}

void
_cogl_setup_viewport (guint width,
                      guint height,
                      float fovy,
                      float aspect,
                      float z_near,
                      float z_far)
{
  float z_camera;
  CoglMatrix projection_matrix;
  CoglMatrixStack *modelview_stack;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_set_viewport (0, 0, width, height);

  /* For Ortho projection.
   * _cogl_matrix_stack_ortho (projection_stack, 0, width, 0,  height, -1, 1);
   */

  cogl_perspective (fovy, aspect, z_near, z_far);

  /*
   * In theory, we can compute the camera distance from screen as:
   *
   *   0.5 * tan (FOV)
   *
   * However, it's better to compute the z_camera from our projection
   * matrix so that we get a 1:1 mapping at the screen distance. Consider
   * the upper-left corner of the screen. It has object coordinates
   * (0,0,0), so by the transform below, ends up with eye coordinate
   *
   *   x_eye = x_object / width - 0.5 = - 0.5
   *   y_eye = (height - y_object) / width - 0.5 = 0.5
   *   z_eye = z_object / width - z_camera = - z_camera
   *
   * From cogl_perspective(), we know that the projection matrix has
   * the form:
   *
   *  (x, 0,  0, 0)
   *  (0, y,  0, 0)
   *  (0, 0,  c, d)
   *  (0, 0, -1, 0)
   *
   * Applied to the above, we get clip coordinates of
   *
   *  x_clip = x * (- 0.5)
   *  y_clip = y * 0.5
   *  w_clip = - 1 * (- z_camera) = z_camera
   *
   * Dividing through by w to get normalized device coordinates, we
   * have, x_nd = x * 0.5 / z_camera, y_nd = - y * 0.5 / z_camera.
   * The upper left corner of the screen has normalized device coordinates,
   * (-1, 1), so to have the correct 1:1 mapping, we have to have:
   *
   *   z_camera = 0.5 * x = 0.5 * y
   *
   * If x != y, then we have a non-uniform aspect ration, and a 1:1 mapping
   * doesn't make sense.
   */

  cogl_get_projection_matrix (&projection_matrix);
  z_camera = 0.5 * projection_matrix.xx;

  modelview_stack =
    _cogl_draw_buffer_get_modelview_stack (_cogl_get_draw_buffer ());
  _cogl_matrix_stack_load_identity (modelview_stack);
  _cogl_matrix_stack_translate (modelview_stack, -0.5f, -0.5f, -z_camera);
  _cogl_matrix_stack_scale (modelview_stack,
                            1.0f / width, -1.0f / height, 1.0f / width);
  _cogl_matrix_stack_translate (modelview_stack,
                                0.0f, -1.0 * height, 0.0f);
}

CoglFeatureFlags
cogl_get_features (void)
{
  _COGL_GET_CONTEXT (ctx, 0);

  if (cogl_debug_flags & COGL_DEBUG_DISABLE_VBOS)
    ctx->feature_flags &= ~COGL_FEATURE_VBOS;

  return ctx->feature_flags;
}

gboolean
cogl_features_available (CoglFeatureFlags features)
{
  _COGL_GET_CONTEXT (ctx, 0);

  if (!ctx->features_cached)
    _cogl_features_init ();

  return (ctx->feature_flags & features) == features;
}

/* XXX: This function should either be replaced with one returning
 * integers, or removed/deprecated and make the
 * _cogl_draw_buffer_get_viewport* functions public.
 */
void
cogl_get_viewport (float v[4])
{
  CoglHandle draw_buffer;
  int viewport[4];
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  draw_buffer = _cogl_get_draw_buffer ();
  _cogl_draw_buffer_get_viewport4fv (draw_buffer, viewport);

  for (i = 0; i < 4; i++)
    v[i] = viewport[i];
}

void
cogl_get_bitmasks (gint *red,
                   gint *green,
                   gint *blue,
                   gint *alpha)
{
  GLint value;

  if (red)
    {
      GE( glGetIntegerv(GL_RED_BITS, &value) );
      *red = value;
    }

  if (green)
    {
      GE( glGetIntegerv(GL_GREEN_BITS, &value) );
      *green = value;
    }

  if (blue)
    {
      GE( glGetIntegerv(GL_BLUE_BITS, &value) );
      *blue = value;
    }

  if (alpha)
    {
      GE( glGetIntegerv(GL_ALPHA_BITS, &value ) );
      *alpha = value;
    }
}

void
cogl_set_fog (const CoglColor *fog_color,
              CoglFogMode      mode,
              float            density,
              float            z_near,
              float            z_far)
{
  GLfloat fogColor[4];
  GLenum gl_mode = GL_LINEAR;

  /* The cogl journal doesn't currently track fog state changes */
  _cogl_journal_flush ();

  fogColor[0] = cogl_color_get_red_float (fog_color);
  fogColor[1] = cogl_color_get_green_float (fog_color);
  fogColor[2] = cogl_color_get_blue_float (fog_color);
  fogColor[3] = cogl_color_get_alpha_float (fog_color);

  glEnable (GL_FOG);

  glFogfv (GL_FOG_COLOR, fogColor);

#if HAVE_COGL_GLES
  switch (mode)
    {
    case COGL_FOG_MODE_LINEAR:
      gl_mode = GL_LINEAR;
      break;
    case COGL_FOG_MODE_EXPONENTIAL:
      gl_mode = GL_EXP;
      break;
    case COGL_FOG_MODE_EXPONENTIAL_SQUARED:
      gl_mode = GL_EXP2;
      break;
    }
#endif
  /* TODO: support other modes for GLES2 */

  /* NB: GLES doesn't have glFogi */
  glFogf (GL_FOG_MODE, gl_mode);
  glHint (GL_FOG_HINT, GL_NICEST);

  glFogf (GL_FOG_DENSITY, (GLfloat) density);
  glFogf (GL_FOG_START, (GLfloat) z_near);
  glFogf (GL_FOG_END, (GLfloat) z_far);
}

void
cogl_disable_fog (void)
{
  /* Currently the journal can't track changes to fog state... */
  _cogl_journal_flush ();

  glDisable (GL_FOG);
}

void
cogl_flush (void)
{
  _cogl_journal_flush ();
}

void
cogl_read_pixels (int x,
                  int y,
                  int width,
                  int height,
                  CoglReadPixelsFlags source,
                  CoglPixelFormat format,
                  guint8 *pixels)
{
  CoglHandle draw_buffer;
  int        draw_buffer_height;
  int        rowstride = width * 4;
  guint8    *temprow;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (format == COGL_PIXEL_FORMAT_RGBA_8888);
  g_return_if_fail (source == COGL_READ_PIXELS_COLOR_BUFFER);

  temprow = g_alloca (rowstride * sizeof (guint8));

  draw_buffer = _cogl_get_draw_buffer ();

  _cogl_draw_buffer_flush_state (draw_buffer, 0);

  draw_buffer_height = _cogl_draw_buffer_get_height (draw_buffer);

  /* The y co-ordinate should be given in OpenGL's coordinate system
   * so 0 is the bottom row
   *
   * NB: all offscreen rendering is done upside down so no conversion
   * is necissary in this case.
   */
  if (!cogl_is_offscreen (draw_buffer))
    y = draw_buffer_height - y - height;

  /* make sure any batched primitives get emitted to the GL driver before
   * issuing our read pixels... */
  cogl_flush ();

  /* Setup the pixel store parameters that may have been changed by
     Cogl */
  GE (glPixelStorei (GL_PACK_ALIGNMENT, 1));
#ifdef HAVE_COGL_GL
  GE (glPixelStorei (GL_PACK_ROW_LENGTH, 0));
  GE (glPixelStorei (GL_PACK_SKIP_PIXELS, 0));
  GE (glPixelStorei (GL_PACK_SKIP_ROWS, 0));
#endif /* HAVE_COGL_GL */

  GE (glReadPixels (x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels));

  /* NB: All offscreen rendering is done upside down so there is no need
   * to flip in this case... */
  if (cogl_is_offscreen (draw_buffer))
    return;

  /* TODO: consider using the GL_MESA_pack_invert extension in the future
   * to avoid this flip... */

  /* vertically flip the buffer in-place */
  for (y = 0; y < height / 2; y++)
    {
      if (y != height - y - 1) /* skip center row */
        {
          memcpy (temprow,
                  pixels + y * rowstride, rowstride);
          memcpy (pixels + y * rowstride,
                  pixels + (height - y - 1) * rowstride, rowstride);
          memcpy (pixels + (height - y - 1) * rowstride,
                  temprow,
                  rowstride);
        }
    }
}

void
cogl_begin_gl (void)
{
  CoglMaterialFlushOptions options;
  gulong enable_flags = 0;
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->in_begin_gl_block)
    {
      static gboolean shown = FALSE;
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
   * NB: _cogl_draw_buffer_flush_state may disrupt various state (such
   * as the material state) when flushing the clip stack, so should
   * always be done first when preparing to draw. */
  _cogl_draw_buffer_flush_state (_cogl_get_draw_buffer (), 0);

  /* Setup the state for the current material */

  /* We considered flushing a specific, minimal material here to try and
   * simplify the GL state, but decided to avoid special cases and second
   * guessing what would be actually helpful.
   *
   * A user should instead call cogl_set_source_color4ub() before
   * cogl_begin_gl() to simplify the state flushed.
   */
  options.flags = 0;
  _cogl_material_flush_gl_state (ctx->source_material, &options);

  /* FIXME: This api is a bit yukky, ideally it will be removed if we
   * re-work the cogl_enable mechanism */
  enable_flags |= _cogl_material_get_cogl_enable_flags (ctx->source_material);

  if (ctx->enable_backface_culling)
    enable_flags |= COGL_ENABLE_BACKFACE_CULLING;

  cogl_enable (enable_flags);
  _cogl_flush_face_winding ();

  /* Disable all client texture coordinate arrays */
  for (i = 0; i < ctx->n_texcoord_arrays_enabled; i++)
    {
      GE (glClientActiveTexture (GL_TEXTURE0 + i));
      GE (glDisableClientState (GL_TEXTURE_COORD_ARRAY));
    }
  ctx->n_texcoord_arrays_enabled = 0;
}

void
cogl_end_gl (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!ctx->in_begin_gl_block)
    {
      static gboolean shown = FALSE;
      if (!shown)
        g_warning ("cogl_end_gl is being called before cogl_begin_gl");
      shown = TRUE;
      return;
    }
  ctx->in_begin_gl_block = FALSE;
}

static CoglTextureUnit *
_cogl_texture_unit_new (void)
{
  CoglTextureUnit *unit = g_new0 (CoglTextureUnit, 1);
  unit->matrix_stack = _cogl_matrix_stack_new ();
  return unit;
}

static void
_cogl_texture_unit_free (CoglTextureUnit *unit)
{
  _cogl_matrix_stack_destroy (unit->matrix_stack);
  g_free (unit);
}

CoglTextureUnit *
_cogl_get_texture_unit (int index_)
{
  GList *l;
  CoglTextureUnit *unit;

  _COGL_GET_CONTEXT (ctx, NULL);

  for (l = ctx->texture_units; l; l = l->next)
    {
      unit = l->data;

      if (unit->index == index_)
        return unit;

      /* The units are always sorted, so at this point we know this unit
       * doesn't exist */
      if (unit->index > index_)
        break;
    }
  /* NB: if we now insert a new layer before l, that will maintain order.
   */

  unit = _cogl_texture_unit_new ();

  /* Note: see comment after for() loop above */
  ctx->texture_units =
    g_list_insert_before (ctx->texture_units, l, unit);

  return unit;
}

void
_cogl_destroy_texture_units (void)
{
  GList *l;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (l = ctx->texture_units; l; l = l->next)
    _cogl_texture_unit_free (l->data);
  g_list_free (ctx->texture_units);
}

void
cogl_push_matrix (void)
{
  CoglMatrixStack *modelview_stack =
    _cogl_draw_buffer_get_modelview_stack (_cogl_get_draw_buffer ());
  _cogl_matrix_stack_push (modelview_stack);
}

void
cogl_pop_matrix (void)
{
  CoglMatrixStack *modelview_stack =
    _cogl_draw_buffer_get_modelview_stack (_cogl_get_draw_buffer ());
  _cogl_matrix_stack_pop (modelview_stack);
}

void
cogl_scale (float x, float y, float z)
{
  CoglMatrixStack *modelview_stack =
    _cogl_draw_buffer_get_modelview_stack (_cogl_get_draw_buffer ());
  _cogl_matrix_stack_scale (modelview_stack, x, y, z);
}

void
cogl_translate (float x, float y, float z)
{
  CoglMatrixStack *modelview_stack =
    _cogl_draw_buffer_get_modelview_stack (_cogl_get_draw_buffer ());
  _cogl_matrix_stack_translate (modelview_stack, x, y, z);
}

void
cogl_rotate (float angle, float x, float y, float z)
{
  CoglMatrixStack *modelview_stack =
    _cogl_draw_buffer_get_modelview_stack (_cogl_get_draw_buffer ());
  _cogl_matrix_stack_rotate (modelview_stack, angle, x, y, z);
}

void
cogl_perspective (float fov_y,
		  float aspect,
		  float z_near,
		  float z_far)
{
  float ymax = z_near * tanf (fov_y * G_PI / 360.0);

  cogl_frustum (-ymax * aspect,  /* left */
                ymax * aspect,   /* right */
                -ymax,           /* bottom */
                ymax,            /* top */
                z_near,
                z_far);
}

void
cogl_frustum (float        left,
	      float        right,
	      float        bottom,
	      float        top,
	      float        z_near,
	      float        z_far)
{
  CoglMatrixStack *projection_stack =
    _cogl_draw_buffer_get_projection_stack (_cogl_get_draw_buffer ());

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_matrix_stack_load_identity (projection_stack);

  _cogl_matrix_stack_frustum (projection_stack,
                              left,
                              right,
                              bottom,
                              top,
                              z_near,
                              z_far);
}

void
cogl_ortho (float left,
	    float right,
	    float bottom,
	    float top,
	    float z_near,
	    float z_far)
{
  CoglMatrix ortho;
  CoglMatrixStack *projection_stack =
    _cogl_draw_buffer_get_projection_stack (_cogl_get_draw_buffer ());

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_matrix_init_identity (&ortho);
  cogl_matrix_ortho (&ortho, left, right, bottom, top, z_near, z_far);
  _cogl_matrix_stack_set (projection_stack, &ortho);
}

void
cogl_get_modelview_matrix (CoglMatrix *matrix)
{
  CoglMatrixStack *modelview_stack =
    _cogl_draw_buffer_get_modelview_stack (_cogl_get_draw_buffer ());
  _cogl_matrix_stack_get (modelview_stack, matrix);
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

void
cogl_set_modelview_matrix (CoglMatrix *matrix)
{
  CoglMatrixStack *modelview_stack =
    _cogl_draw_buffer_get_modelview_stack (_cogl_get_draw_buffer ());
  _cogl_matrix_stack_set (modelview_stack, matrix);
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

void
cogl_get_projection_matrix (CoglMatrix *matrix)
{
  CoglMatrixStack *projection_stack =
    _cogl_draw_buffer_get_projection_stack (_cogl_get_draw_buffer ());
  _cogl_matrix_stack_get (projection_stack, matrix);
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

void
cogl_set_projection_matrix (CoglMatrix *matrix)
{
  CoglMatrixStack *projection_stack =
    _cogl_draw_buffer_get_projection_stack (_cogl_get_draw_buffer ());
  _cogl_matrix_stack_set (projection_stack, matrix);

  /* FIXME: Update the inverse projection matrix!! Presumably use
   * of clip planes must currently be broken if this API is used. */
  _COGL_MATRIX_DEBUG_PRINT (matrix);
}

CoglClipStackState *
_cogl_get_clip_state (void)
{
  CoglHandle draw_buffer;

  draw_buffer = _cogl_get_draw_buffer ();
  return _cogl_draw_buffer_get_clip_state (draw_buffer);
}

GQuark
_cogl_driver_error_quark (void)
{
  return g_quark_from_static_string ("cogl-driver-error-quark");
}
