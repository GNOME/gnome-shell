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
#include <gmodule.h>
#include <math.h>
#include <stdlib.h>

#ifdef HAVE_CLUTTER_GLX
#include <dlfcn.h>
#include <GL/glx.h>

typedef CoglFuncPtr (*GLXGetProcAddressProc) (const guint8 *procName);
#endif

#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context.h"

#if defined (HAVE_COGL_GLES2) || defined (HAVE_COGL_GLES)
#include "cogl-gles2-wrapper.h"
#endif

/* GL error to string conversion */
#if COGL_DEBUG
struct token_string
{
  GLuint Token;
  const char *String;
};

static const struct token_string Errors[] = {
  { GL_NO_ERROR, "no error" },
  { GL_INVALID_ENUM, "invalid enumerant" },
  { GL_INVALID_VALUE, "invalid value" },
  { GL_INVALID_OPERATION, "invalid operation" },
  { GL_STACK_OVERFLOW, "stack overflow" },
  { GL_STACK_UNDERFLOW, "stack underflow" },
  { GL_OUT_OF_MEMORY, "out of memory" },
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION_EXT
  { GL_INVALID_FRAMEBUFFER_OPERATION_EXT, "invalid framebuffer operation" },
#endif
  { ~0, NULL }
};

const char*
_cogl_error_string(GLenum errorCode)
{
  int i;
  for (i = 0; Errors[i].String; i++) {
    if (Errors[i].Token == errorCode)
      return Errors[i].String;
  }
  return "unknown";
}
#endif

CoglFuncPtr
cogl_get_proc_address (const gchar* name)
{
  return NULL;
}

gboolean
cogl_check_extension (const gchar *name, const gchar *ext)
{
  return FALSE;
}

void
cogl_clear (const CoglColor *color)
{
#if COGL_DEBUG
  fprintf(stderr, "\n ============== Paint Start ================ \n");
#endif

  GE( glClearColor (cogl_color_get_red_float (color),
                    cogl_color_get_green_float (color),
                    cogl_color_get_blue_float (color),
                    0.0) );

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  /*
   *  Disable the depth test for now as has some strange side effects,
   *  mainly on x/y axis rotation with multiple layers at same depth
   *  (eg rotating text on a bg has very strange effect). Seems no clean
   *  100% effective way to fix without other odd issues.. So for now
   *  move to application to handle and add cogl_enable_depth_test()
   *  as for custom actors (i.e groups) to enable if need be.
   *
   * glEnable (GL_DEPTH_TEST);
   * glEnable (GL_ALPHA_TEST)
   * glDepthFunc (GL_LEQUAL);
   * glAlphaFunc (GL_GREATER, 0.1);
   */
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
apply_matrix (const float *matrix, float *vertex)
{
  int x, y;
  float vertex_out[4] = { 0 };

  for (y = 0; y < 4; y++)
    for (x = 0; x < 4; x++)
      vertex_out[y] += vertex[x] * matrix[y + x * 4];

  memcpy (vertex, vertex_out, sizeof (vertex_out));
}

static void
project_vertex (float *modelview,
		float *project,
		float *vertex)
{
  int i;

  /* Apply the modelview matrix */
  apply_matrix (modelview, vertex);
  /* Apply the projection matrix */
  apply_matrix (project, vertex);
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
  GLfloat *modelview;
  GLfloat *projection;

  float vertex_tl[4] = { x_offset, y_offset, 0, 1.0 };
  float vertex_tr[4] = { x_offset + width, y_offset, 0, 1.0 };
  float vertex_bl[4] = { x_offset, y_offset + height, 0, 1.0 };
  float vertex_br[4] = { x_offset + width, y_offset + height,
                        0, 1.0 };

  /* hack alert: there's no way to get *and modify*
   * CoglMatrix as a float array. So we just
   * use a cast instead of cogl_matrix_get_array(),
   * and know that we will not call any more CoglMatrix
   * methods after we write to it directly.
   */
  _cogl_get_matrix (COGL_MATRIX_PROJECTION,
                    &projection_matrix);
  projection = (GLfloat*) &projection_matrix;
  _cogl_get_matrix (COGL_MATRIX_MODELVIEW,
                    &modelview_matrix);
  modelview = (GLfloat*) &modelview_matrix;

  project_vertex (modelview, projection, vertex_tl);
  project_vertex (modelview, projection, vertex_tr);
  project_vertex (modelview, projection, vertex_bl);
  project_vertex (modelview, projection, vertex_br);

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

  cogl_material_flush_gl_state (ctx->stencil_material, NULL);

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
      _cogl_current_matrix_push ();
      _cogl_current_matrix_identity ();
      _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
      _cogl_current_matrix_push ();
      _cogl_current_matrix_identity ();
      cogl_rectangle (-1.0, -1.0, 1.0, 1.0);
      _cogl_current_matrix_pop ();
      _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
      _cogl_current_matrix_pop ();
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
   * camera distance from screen
   *
   * See comments in ../gl/cogl.c
   */

  cogl_get_projection_matrix (&projection_matrix);
  z_camera = 0.5 * projection_matrix.xx;

  _cogl_current_matrix_identity ();
  _cogl_current_matrix_translate (-0.5f, -0.5f, -z_camera);
  _cogl_current_matrix_scale (1.0f / width, -1.0f / height, 1.0f / width);
  _cogl_current_matrix_translate (0.0f, -1.0 * height, 0.0f);
}

static void
_cogl_features_init ()
{
  CoglFeatureFlags flags = 0;
  int              max_clip_planes = 0;
  GLint            num_stencil_bits = 0;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( glGetIntegerv (GL_STENCIL_BITS, &num_stencil_bits) );
  /* We need at least three stencil bits to combine clips */
  if (num_stencil_bits > 2)
    flags |= COGL_FEATURE_STENCIL_BUFFER;

  GE( glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
  if (max_clip_planes >= 4)
    flags |= COGL_FEATURE_FOUR_CLIP_PLANES;

#ifdef HAVE_COGL_GLES2
  flags |= COGL_FEATURE_SHADERS_GLSL | COGL_FEATURE_OFFSCREEN;
#endif

  /* Cache features */
  ctx->feature_flags = flags;
  ctx->features_cached = TRUE;
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
cogl_get_bitmasks (gint *red, gint *green, gint *blue, gint *alpha)
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
  GLenum  gl_mode;

  fogColor[0] = cogl_color_get_red_float (fog_color);
  fogColor[1] = cogl_color_get_green_float (fog_color);
  fogColor[2] = cogl_color_get_blue_float (fog_color);
  fogColor[3] = cogl_color_get_alpha_float (fog_color);

  glEnable (GL_FOG);

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
#else
  /* TODO: support other modes for GLES2 */
  gl_mode = GL_LINEAR;
#endif

  glFogfv (GL_FOG_COLOR, fogColor);

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
