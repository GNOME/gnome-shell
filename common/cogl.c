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

#ifdef HAVE_CLUTTER_GLX
#include <dlfcn.h>
#include <GL/glx.h>

typedef CoglFuncPtr (*GLXGetProcAddressProc) (const guint8 *procName);
#endif

#include "cogl-debug.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context.h"
#include "cogl-material-private.h"

#if defined (HAVE_COGL_GLES2) || defined (HAVE_COGL_GLES)
#include "cogl-gles2-wrapper.h"
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

void
cogl_clear (const CoglColor *color, gulong buffers)
{
  GLbitfield gl_buffers = 0;

  COGL_NOTE (DRAW, "Clear begin");

  cogl_clip_ensure ();

  if (buffers & COGL_BUFFER_BIT_COLOR)
    {
      GE( glClearColor (cogl_color_get_red_float (color),
			cogl_color_get_green_float (color),
			cogl_color_get_blue_float (color),
			0.0) );
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
cogl_enable_depth_test (gboolean setting)
{
  if (setting)
    {
      glEnable (GL_DEPTH_TEST);
      glEnable (GL_ALPHA_TEST);
      glDepthFunc (GL_LEQUAL);
      glAlphaFunc (GL_GREATER, 0.1);
    }
  else
    {
      glDisable (GL_DEPTH_TEST);
      glDisable (GL_ALPHA_TEST);
    }
}

void
cogl_enable_backface_culling (gboolean setting)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  ctx->enable_backface_culling = setting;
}

void
cogl_set_source_color (const CoglColor *color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* In case cogl_set_source_texture was previously used... */
  cogl_material_remove_layer (ctx->default_material, 0);

  cogl_material_set_color (ctx->default_material, color);
  cogl_set_source (ctx->default_material);
}

static void
project_vertex (const CoglMatrix *modelview_matrix,
		const CoglMatrix *projection_matrix,
		float *vertex)
{
  int i;

  /* Apply the modelview matrix */
  cogl_matrix_transform_point (modelview_matrix,
                               &vertex[0], &vertex[1],
                               &vertex[2], &vertex[3]);
  /* Apply the projection matrix */
  cogl_matrix_transform_point (projection_matrix,
                               &vertex[0], &vertex[1],
                               &vertex[2], &vertex[3]);
  /* Convert from homogenized coordinates */
  for (i = 0; i < 4; i++)
    vertex[i] /= vertex[3];
}

static void
set_clip_plane (GLint plane_num,
		const float *vertex_a,
		const float *vertex_b)
{
#if defined (HAVE_COGL_GLES2) || defined (HAVE_COGL_GLES)
  GLfloat plane[4];
#else
  GLdouble plane[4];
#endif
  GLfloat angle;
  CoglMatrix inverse_projection;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Calculate the angle between the axes and the line crossing the
     two points */
  angle = atan2f (vertex_b[1] - vertex_a[1],
                  vertex_b[0] - vertex_a[0]) * (180.0/G_PI);

  _cogl_current_matrix_push ();
  /* Load the identity matrix and multiply by the reverse of the
     projection matrix so we can specify the plane in screen
     coordinates */
  _cogl_current_matrix_identity ();
  cogl_matrix_init_from_array (&inverse_projection,
                               ctx->inverse_projection);
  _cogl_current_matrix_multiply (&inverse_projection);
  /* Rotate about point a */
  _cogl_current_matrix_translate (vertex_a[0], vertex_a[1], vertex_a[2]);
  /* Rotate the plane by the calculated angle so that it will connect
     the two points */
  _cogl_current_matrix_rotate (angle, 0.0f, 0.0f, 1.0f);
  _cogl_current_matrix_translate (-vertex_a[0], -vertex_a[1], -vertex_a[2]);

  _cogl_current_matrix_state_flush ();

  plane[0] = 0;
  plane[1] = -1.0;
  plane[2] = 0;
  plane[3] = vertex_a[1];
#if defined (HAVE_COGL_GLES2) || defined (HAVE_COGL_GLES)
  GE( glClipPlanef (plane_num, plane) );
#else
  GE( glClipPlane (plane_num, plane) );
#endif

  _cogl_current_matrix_pop ();
}

void
_cogl_set_clip_planes (float x_offset,
		       float y_offset,
		       float width,
		       float height)
{
  CoglMatrix modelview_matrix;
  CoglMatrix projection_matrix;

  float vertex_tl[4] = { x_offset, y_offset, 0, 1.0 };
  float vertex_tr[4] = { x_offset + width, y_offset, 0, 1.0 };
  float vertex_bl[4] = { x_offset, y_offset + height, 0, 1.0 };
  float vertex_br[4] = { x_offset + width, y_offset + height,
                        0, 1.0 };

  _cogl_get_matrix (COGL_MATRIX_PROJECTION,
                    &projection_matrix);
  _cogl_get_matrix (COGL_MATRIX_MODELVIEW,
                    &modelview_matrix);

  project_vertex (&modelview_matrix, &projection_matrix, vertex_tl);
  project_vertex (&modelview_matrix, &projection_matrix, vertex_tr);
  project_vertex (&modelview_matrix, &projection_matrix, vertex_bl);
  project_vertex (&modelview_matrix, &projection_matrix, vertex_br);

  /* If the order of the top and bottom lines is different from the
     order of the left and right lines then the clip rect must have
     been transformed so that the back is visible. We therefore need
     to swap one pair of vertices otherwise all of the planes will be
     the wrong way around */
  if ((vertex_tl[0] < vertex_tr[0] ? 1 : 0)
      != (vertex_bl[1] < vertex_tl[1] ? 1 : 0))
    {
      float temp[4];
      memcpy (temp, vertex_tl, sizeof (temp));
      memcpy (vertex_tl, vertex_tr, sizeof (temp));
      memcpy (vertex_tr, temp, sizeof (temp));
      memcpy (temp, vertex_bl, sizeof (temp));
      memcpy (vertex_bl, vertex_br, sizeof (temp));
      memcpy (vertex_br, temp, sizeof (temp));
    }

  set_clip_plane (GL_CLIP_PLANE0, vertex_tl, vertex_tr);
  set_clip_plane (GL_CLIP_PLANE1, vertex_tr, vertex_br);
  set_clip_plane (GL_CLIP_PLANE2, vertex_br, vertex_bl);
  set_clip_plane (GL_CLIP_PLANE3, vertex_bl, vertex_tl);
}

void
_cogl_add_stencil_clip (float x_offset,
			float y_offset,
			float width,
			float height,
			gboolean first)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_material_flush_gl_state (ctx->stencil_material, NULL);

  if (first)
    {
      GE( glEnable (GL_STENCIL_TEST) );

      /* Initially disallow everything */
      GE( glClearStencil (0) );
      GE( glClear (GL_STENCIL_BUFFER_BIT) );

      /* Punch out a hole to allow the rectangle */
      GE( glStencilFunc (GL_NEVER, 0x1, 0x1) );
      GE( glStencilOp (GL_REPLACE, GL_REPLACE, GL_REPLACE) );

      cogl_rectangle (x_offset, y_offset,
                      x_offset + width, y_offset + height);
    }
  else
    {
      /* Add one to every pixel of the stencil buffer in the
	 rectangle */
      GE( glStencilFunc (GL_NEVER, 0x1, 0x3) );
      GE( glStencilOp (GL_INCR, GL_INCR, GL_INCR) );
      cogl_rectangle (x_offset, y_offset,
                      x_offset + width, y_offset + height);

      /* Subtract one from all pixels in the stencil buffer so that
	 only pixels where both the original stencil buffer and the
	 rectangle are set will be valid */
      GE( glStencilOp (GL_DECR, GL_DECR, GL_DECR) );

      _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
      _cogl_current_matrix_push ();
      _cogl_current_matrix_identity ();

      /* Cogl generally assumes the modelview matrix is current, so since
       * cogl_rectangle will be flushing GL state and emitting geometry
       * to OpenGL it will be confused if we leave the projection matrix
       * active... */
      _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
      _cogl_current_matrix_push ();
      _cogl_current_matrix_identity ();

      cogl_rectangle (-1.0, -1.0, 1.0, 1.0);

      _cogl_current_matrix_pop ();

      _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
      _cogl_current_matrix_pop ();

      _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
    }

  /* Restore the stencil mode */
  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );
}

void
_cogl_disable_stencil_buffer (void)
{
  GE( glDisable (GL_STENCIL_TEST) );
}

void
_cogl_enable_clip_planes (void)
{
  GE( glEnable (GL_CLIP_PLANE0) );
  GE( glEnable (GL_CLIP_PLANE1) );
  GE( glEnable (GL_CLIP_PLANE2) );
  GE( glEnable (GL_CLIP_PLANE3) );
}

void
_cogl_disable_clip_planes (void)
{
  GE( glDisable (GL_CLIP_PLANE3) );
  GE( glDisable (GL_CLIP_PLANE2) );
  GE( glDisable (GL_CLIP_PLANE1) );
  GE( glDisable (GL_CLIP_PLANE0) );
}

void
cogl_viewport (guint width,
	       guint height)
{
  GE( glViewport (0, 0, width, height) );
}

void
cogl_setup_viewport (guint width,
		     guint height,
		     float fovy,
		     float aspect,
		     float z_near,
		     float z_far)
{
  float z_camera;
  CoglMatrix projection_matrix;

  GE( glViewport (0, 0, width, height) );

  /* For Ortho projection.
   * _cogl_current_matrix_ortho (0, width << 16, 0,  height << 16,  -1 << 16, 1 << 16);
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

  _cogl_current_matrix_identity ();
  _cogl_current_matrix_translate (-0.5f, -0.5f, -z_camera);
  _cogl_current_matrix_scale (1.0f / width, -1.0f / height, 1.0f / width);
  _cogl_current_matrix_translate (0.0f, -1.0 * height, 0.0f);
}

CoglFeatureFlags
cogl_get_features ()
{
  _COGL_GET_CONTEXT (ctx, 0);

  if (!ctx->features_cached)
    _cogl_features_init ();

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

void
cogl_get_viewport (float v[4])
{
  /* FIXME: cogl_get_viewport should return a gint vec */
#if defined (HAVE_COGL_GLES2) || defined (HAVE_COGL_GLES)
  GLint viewport[4];
  int i;

  glGetIntegerv (GL_VIEWPORT, viewport);

  for (i = 0; i < 4; i++)
    v[i] = (float)(viewport[i]);
#else
  glGetFloatv (GL_VIEWPORT, v);
#endif
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
  glDisable (GL_FOG);
}

void
cogl_flush_gl_state (int flags)
{
  _cogl_current_matrix_state_flush ();
}

