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
#include <stdlib.h>

#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context.h"

#include "cogl-gles2-wrapper.h"

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
cogl_paint_init (const CoglColor *color)
{
#if COGL_DEBUG
  fprintf(stderr, "\n ============== Paint Start ================ \n");
#endif

  cogl_wrap_glClearColorx (cogl_color_get_red (color),
			   cogl_color_get_green (color),
			   cogl_color_get_blue (color),
			   0);

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  cogl_wrap_glDisable (GL_LIGHTING);
  cogl_wrap_glDisable (GL_FOG);
}

/* FIXME: inline most of these  */
void
cogl_push_matrix (void)
{
  GE( cogl_wrap_glPushMatrix() );
}

void
cogl_pop_matrix (void)
{
  GE( cogl_wrap_glPopMatrix() );
}

void
cogl_scale (CoglFixed x, CoglFixed y)
{
  GE( cogl_wrap_glScalex (x, y, COGL_FIXED_1) );
}

void
cogl_translatex (CoglFixed x, CoglFixed y, CoglFixed z)
{
  GE( cogl_wrap_glTranslatex (x, y, z) );
}

void
cogl_translate (gint x, gint y, gint z)
{
  GE( cogl_wrap_glTranslatex (COGL_FIXED_FROM_INT(x), 
			      COGL_FIXED_FROM_INT(y), 
			      COGL_FIXED_FROM_INT(z)) );
}

void
cogl_rotatex (CoglFixed angle, 
	      CoglFixed x, 
	      CoglFixed y, 
	      CoglFixed z)
{
  GE( cogl_wrap_glRotatex (angle,x,y,z) );
}

void
cogl_rotate (gint angle, gint x, gint y, gint z)
{
  GE( cogl_wrap_glRotatex (COGL_FIXED_FROM_INT(angle),
		 COGL_FIXED_FROM_INT(x), 
		 COGL_FIXED_FROM_INT(y), 
		 COGL_FIXED_FROM_INT(z)) );
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
	  GE( cogl_wrap_glEnable (gl_flag) );
	  ctx->enable_flags |= flag;
	  return TRUE;
	}
    }
  else if (ctx->enable_flags & flag)
    {
      GE( cogl_wrap_glDisable (gl_flag) );
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
	  GE( cogl_wrap_glEnableClientState (gl_flag) );
	  ctx->enable_flags |= flag;
	  return TRUE;
	}
    }
  else if (ctx->enable_flags & flag)
    {
      GE( cogl_wrap_glDisableClientState (gl_flag) );
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
		    COGL_ENABLE_TEXTURE_2D,
		    GL_TEXTURE_2D);

  cogl_toggle_flag (ctx, flags,
                    COGL_ENABLE_BACKFACE_CULLING,
                    GL_CULL_FACE);

  cogl_toggle_client_flag (ctx, flags,
			   COGL_ENABLE_VERTEX_ARRAY,
			   GL_VERTEX_ARRAY);
  
  cogl_toggle_client_flag (ctx, flags,
			   COGL_ENABLE_TEXCOORD_ARRAY,
			   GL_TEXTURE_COORD_ARRAY);

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
cogl_blend_func (COGLenum src_factor, COGLenum dst_factor)
{
  /* This function caches the blending setup in the
   * hope of lessening GL traffic.
   */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (ctx->blend_src_factor != src_factor ||
      ctx->blend_dst_factor != dst_factor)
    {
      glBlendFunc (src_factor, dst_factor);
      ctx->blend_src_factor = src_factor;
      ctx->blend_dst_factor = dst_factor;
    }
}

void
cogl_enable_depth_test (gboolean setting)
{
  if (setting)
    {
      cogl_wrap_glEnable (GL_DEPTH_TEST);
      cogl_wrap_glEnable (GL_ALPHA_TEST);
      glDepthFunc (GL_LEQUAL);
      cogl_wrap_glAlphaFunc (GL_GREATER, 0.1);
    }
  else
    {
      cogl_wrap_glDisable (GL_DEPTH_TEST);
      cogl_wrap_glDisable (GL_ALPHA_TEST);
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
  
#if 0 /*HAVE_GLES_COLOR4UB*/

  /* NOTE: seems SDK_OGLES-1.1_LINUX_PCEMULATION_2.02.22.0756 has this call 
   * but is broken - see #857. Therefor disabling. 
  */

  /* 
   * GLES 1.1 does actually have this function, it's in the header file but
   * missing in the reference manual (and SDK):
   *
   * http://www.khronos.org/egl/headers/1_1/gl.h
   */
  GE( glColor4ub (color->red,
		  color->green,
		  color->blue,
		  color->alpha) );


#else
  /* conversion can cause issues with picking on some gles implementations */
  GE( cogl_wrap_glColor4x (cogl_color_get_red (color),
                           cogl_color_get_green (color),
                           cogl_color_get_blue (color),
                           cogl_color_get_alpha (color)) );
#endif
  
  /* Store alpha for proper blending enables */
  ctx->color_alpha = cogl_color_get_alpha_byte (color);
}

static void
apply_matrix (const CoglFixed *matrix, CoglFixed *vertex)
{
  int x, y;
  CoglFixed vertex_out[4] = { 0 };

  for (y = 0; y < 4; y++)
    for (x = 0; x < 4; x++)
      vertex_out[y] += cogl_fixed_mul (vertex[x], matrix[y + x * 4]);

  memcpy (vertex, vertex_out, sizeof (vertex_out));
}

static void
project_vertex (CoglFixed *modelview,
		CoglFixed *project,
		CoglFixed *vertex)
{
  int i;

  /* Apply the modelview matrix */
  apply_matrix (modelview, vertex);
  /* Apply the projection matrix */
  apply_matrix (project, vertex);
  /* Convert from homogenized coordinates */
  for (i = 0; i < 4; i++)
    vertex[i] = cogl_fixed_div (vertex[i], vertex[3]);
}

static void
set_clip_plane (GLint plane_num,
		const CoglFixed *vertex_a,
		const CoglFixed *vertex_b)
{
  GLfixed plane[4];
  GLfixed angle;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Calculate the angle between the axes and the line crossing the
     two points */
  angle = cogl_fixed_mul (cogl_fixed_atan2 (vertex_b[1] - vertex_a[1],
                                            vertex_b[0] - vertex_a[0]),
		          COGL_RADIANS_TO_DEGREES);

  GE( cogl_wrap_glPushMatrix () );
  /* Load the identity matrix and multiply by the reverse of the
     projection matrix so we can specify the plane in screen
     coordinates */
  GE( cogl_wrap_glLoadIdentity () );
  GE( cogl_wrap_glMultMatrixx ((GLfixed *) ctx->inverse_projection) );
  /* Rotate about point a */
  GE( cogl_wrap_glTranslatex (vertex_a[0], vertex_a[1], vertex_a[2]) );
  /* Rotate the plane by the calculated angle so that it will connect
     the two points */
  GE( cogl_wrap_glRotatex (angle, 0.0f, 0.0f, 1.0f) );
  GE( cogl_wrap_glTranslatex (-vertex_a[0], -vertex_a[1], -vertex_a[2]) );

  plane[0] = 0;
  plane[1] = -COGL_FIXED_1;
  plane[2] = 0;
  plane[3] = vertex_a[1];
  GE( cogl_wrap_glClipPlanex (plane_num, plane) );

  GE( cogl_wrap_glPopMatrix () );
}

void
_cogl_set_clip_planes (CoglFixed x_offset,
		       CoglFixed y_offset,
		       CoglFixed width,
		       CoglFixed height)
{
  GLfixed modelview[16], projection[16];

  CoglFixed vertex_tl[4] = { x_offset, y_offset, 0, COGL_FIXED_1 };
  CoglFixed vertex_tr[4] = { x_offset + width, y_offset, 0, COGL_FIXED_1 };
  CoglFixed vertex_bl[4] = { x_offset, y_offset + height, 0, COGL_FIXED_1 };
  CoglFixed vertex_br[4] = { x_offset + width, y_offset + height,
				0, COGL_FIXED_1 };

  GE( cogl_wrap_glGetFixedv (GL_MODELVIEW_MATRIX, modelview) );
  GE( cogl_wrap_glGetFixedv (GL_PROJECTION_MATRIX, projection) );

  project_vertex (modelview, projection, vertex_tl);
  project_vertex (modelview, projection, vertex_tr);
  project_vertex (modelview, projection, vertex_bl);
  project_vertex (modelview, projection, vertex_br);

  /* If the order of the top and bottom lines is different from
     the order of the left and right lines then the clip rect must
     have been transformed so that the back is visible. We
     therefore need to swap one pair of vertices otherwise all of
     the planes will be the wrong way around */
  if ((vertex_tl[0] < vertex_tr[0] ? 1 : 0)
      != (vertex_bl[1] < vertex_tl[1] ? 1 : 0))
    {
      CoglFixed temp[4];
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
_cogl_add_stencil_clip (CoglFixed x_offset,
			CoglFixed y_offset,
			CoglFixed width,
			CoglFixed height,
			gboolean first)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (first)
    {
      GE( cogl_wrap_glEnable (GL_STENCIL_TEST) );
      
      /* Initially disallow everything */
      GE( glClearStencil (0) );
      GE( glClear (GL_STENCIL_BUFFER_BIT) );

      /* Punch out a hole to allow the rectangle */
      GE( glStencilFunc (GL_NEVER, 0x1, 0x1) );
      GE( glStencilOp (GL_REPLACE, GL_REPLACE, GL_REPLACE) );

      cogl_rectanglex (x_offset, y_offset, width, height);
    }
  else
    {
      /* Add one to every pixel of the stencil buffer in the
	 rectangle */
      GE( glStencilFunc (GL_NEVER, 0x1, 0x3) );
      GE( glStencilOp (GL_INCR, GL_INCR, GL_INCR) );
      cogl_rectanglex (x_offset, y_offset, width, height);

      /* Subtract one from all pixels in the stencil buffer so that
	 only pixels where both the original stencil buffer and the
	 rectangle are set will be valid */
      GE( glStencilOp (GL_DECR, GL_DECR, GL_DECR) );
      GE( cogl_wrap_glPushMatrix () );
      GE( cogl_wrap_glLoadIdentity () );
      GE( cogl_wrap_glMatrixMode (GL_PROJECTION) );
      GE( cogl_wrap_glPushMatrix () );
      GE( cogl_wrap_glLoadIdentity () );
      cogl_rectanglex (-COGL_FIXED_1, -COGL_FIXED_1,
		       COGL_FIXED_FROM_INT (2),
		       COGL_FIXED_FROM_INT (2));
      GE( cogl_wrap_glPopMatrix () );
      GE( cogl_wrap_glMatrixMode (GL_MODELVIEW) );
      GE( cogl_wrap_glPopMatrix () );
    }

  /* Restore the stencil mode */
  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );
}

void
_cogl_set_matrix (const CoglFixed *matrix)
{
  GE( cogl_wrap_glLoadIdentity () );
  GE( cogl_wrap_glMultMatrixx (matrix) );
}

void
_cogl_disable_stencil_buffer (void)
{
  GE( cogl_wrap_glDisable (GL_STENCIL_TEST) );
}

void
_cogl_enable_clip_planes (void)
{
  GE( cogl_wrap_glEnable (GL_CLIP_PLANE0) );
  GE( cogl_wrap_glEnable (GL_CLIP_PLANE1) );
  GE( cogl_wrap_glEnable (GL_CLIP_PLANE2) );
  GE( cogl_wrap_glEnable (GL_CLIP_PLANE3) );
}

void
_cogl_disable_clip_planes (void)
{
  GE( cogl_wrap_glDisable (GL_CLIP_PLANE3) );
  GE( cogl_wrap_glDisable (GL_CLIP_PLANE2) );
  GE( cogl_wrap_glDisable (GL_CLIP_PLANE1) );
  GE( cogl_wrap_glDisable (GL_CLIP_PLANE0) );
}

void
cogl_alpha_func (COGLenum     func, 
		 CoglFixed ref)
{
  GE( cogl_wrap_glAlphaFunc (func, COGL_FIXED_TO_FLOAT(ref)) );
}

/*
 * Fixed point implementation of the perspective function
 */
void
cogl_perspective (CoglFixed fovy,
		  CoglFixed aspect,
		  CoglFixed zNear,
		  CoglFixed zFar)
{
  CoglFixed xmax, ymax;
  CoglFixed x, y, c, d;
  CoglFixed fovy_rad_half = cogl_fixed_mul (fovy, COGL_FIXED_PI) / 360;

  GLfixed m[16];
  
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  memset (&m[0], 0, sizeof (m));

  GE( cogl_wrap_glMatrixMode (GL_PROJECTION) );
  GE( cogl_wrap_glLoadIdentity () );

 /*
   * Based on the original algorithm in perspective():
   * 
   * 1) xmin = -xmax => xmax + xmin == 0 && xmax - xmin == 2 * xmax
   * same true for y, hence: a == 0 && b == 0;
   *
   * 2) When working with small numbers, we can are loosing significant
   * precision
   */
  ymax = cogl_fixed_mul (zNear,
                         cogl_fixed_div (cogl_fixed_sin (fovy_rad_half),
                                         cogl_fixed_cos (fovy_rad_half)));
  xmax = cogl_fixed_mul (ymax, aspect);

  x = cogl_fixed_div (zNear, xmax);
  y = cogl_fixed_div (zNear, ymax);
  c = cogl_fixed_div (-(zFar + zNear), ( zFar - zNear));
  d = cogl_fixed_div (-(cogl_fixed_mul (2 * zFar, zNear)), (zFar - zNear));

#define M(row,col)  m[col*4+row]
  M(0,0) = x;
  M(1,1) = y;
  M(2,2) = c;
  M(2,3) = d;
  M(3,2) = -COGL_FIXED_1;

  GE( cogl_wrap_glMultMatrixx (m) );

  GE( cogl_wrap_glMatrixMode (GL_MODELVIEW) );

  /* Calculate and store the inverse of the matrix */
  memset (ctx->inverse_projection, 0, sizeof (CoglFixed) * 16);

#define m ctx->inverse_projection
  M(0, 0) = cogl_fixed_div (COGL_FIXED_1, x);
  M(1, 1) = cogl_fixed_div (COGL_FIXED_1, y);
  M(2, 3) = -COGL_FIXED_1;
  M(3, 2) = cogl_fixed_div (COGL_FIXED_1, d);
  M(3, 3) = cogl_fixed_div (c, d);
#undef m

#undef M
}

void
cogl_frustum (CoglFixed        left,
	      CoglFixed        right,
	      CoglFixed        bottom,
	      CoglFixed        top,
	      CoglFixed        z_near,
	      CoglFixed        z_far)
{
  CoglFixed c, d;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( cogl_wrap_glMatrixMode (GL_PROJECTION) );
  GE( cogl_wrap_glLoadIdentity () );

  GE( cogl_wrap_glFrustumx (left, right,
			    bottom, top,
			    z_near, z_far) );

  GE( cogl_wrap_glMatrixMode (GL_MODELVIEW) );

  /* Calculate and store the inverse of the matrix */
  memset (ctx->inverse_projection, 0, sizeof (CoglFixed) * 16);

  c = -cogl_fixed_div (z_far + z_near, z_far - z_near);
  d = -cogl_fixed_div (2 * cogl_fixed_mul (z_far, z_near), z_far - z_near);

#define M(row,col)  ctx->inverse_projection[col*4+row]
  M(0,0) = cogl_fixed_div (right - left, 2 * z_near);
  M(0,3) = cogl_fixed_div (right + left, 2 * z_near);
  M(1,1) = cogl_fixed_div (top - bottom, 2 * z_near);
  M(1,3) = cogl_fixed_div (top + bottom, 2 * z_near);
  M(2,3) = -COGL_FIXED_1;
  M(3,2) = cogl_fixed_div (COGL_FIXED_1, d);
  M(3,3) = cogl_fixed_div (c, d);
#undef M  
}

void
cogl_viewport (guint width,
	       guint height)
{
  GE( glViewport (0, 0, width, height) );
}

void
cogl_setup_viewport (guint        w,
		     guint        h,
		     CoglFixed fovy,
		     CoglFixed aspect,
		     CoglFixed z_near,
		     CoglFixed z_far)
{
  gint width = (gint) w;
  gint height = (gint) h;
  CoglFixed z_camera;
  
  GE( glViewport (0, 0, width, height) );

  /* For Ortho projection.
   * cogl_wrap_glOrthox (0, width << 16, 0,  height << 16,  -1 << 16, 1 << 16);
  */

  cogl_perspective (fovy, aspect, z_near, z_far);
  
  GE( cogl_wrap_glLoadIdentity () );

  /*
   * camera distance from screen, 0.5 * tan (FOV)
   *
   * See comments in ../gl/cogl.c
   */
#define DEFAULT_Z_CAMERA 0.869f
  z_camera = COGL_FIXED_FROM_FLOAT (DEFAULT_Z_CAMERA);

  if (fovy != COGL_FIXED_60)
    {
      CoglFixed fovy_rad = cogl_fixed_mul (fovy, COGL_FIXED_PI) / 180;
  
      z_camera = cogl_fixed_div (cogl_fixed_sin (fovy_rad),
                                 cogl_fixed_cos (fovy_rad)) >> 1;
    }
  

  GE( cogl_wrap_glTranslatex (-1 << 15, -1 << 15, -z_camera) );

  GE( cogl_wrap_glScalex ( COGL_FIXED_1 / width,
                          -COGL_FIXED_1 / height,
                           COGL_FIXED_1 / width) );

  GE( cogl_wrap_glTranslatex (0, -COGL_FIXED_1 * height, 0) );
}

static void
_cogl_features_init ()
{
  CoglFeatureFlags flags = 0;
  int              max_clip_planes = 0;
  GLint            num_stencil_bits = 0;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( cogl_wrap_glGetIntegerv (GL_STENCIL_BITS, &num_stencil_bits) );
  /* We need at least three stencil bits to combine clips */
  if (num_stencil_bits > 2)
    flags |= COGL_FEATURE_STENCIL_BUFFER;

  GE( cogl_wrap_glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
  if (max_clip_planes >= 4)
    flags |= COGL_FEATURE_FOUR_CLIP_PLANES;

#ifdef HAVE_COGL_GLES2
  flags |= COGL_FEATURE_SHADERS_GLSL | COGL_FEATURE_OFFSCREEN;
#endif

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
cogl_get_modelview_matrix (CoglFixed m[16])
{
  cogl_wrap_glGetFixedv(GL_MODELVIEW_MATRIX, &m[0]);
}

void
cogl_get_projection_matrix (CoglFixed m[16])
{
  cogl_wrap_glGetFixedv(GL_PROJECTION_MATRIX, &m[0]);
}

void
cogl_get_viewport (CoglFixed v[4])
{
  GLint viewport[4];
  int i;

  cogl_wrap_glGetIntegerv (GL_VIEWPORT, viewport);

  for (i = 0; i < 4; i++)
    v[i] = COGL_FIXED_FROM_INT (viewport[i]);
}

void
cogl_get_bitmasks (gint *red, gint *green, gint *blue, gint *alpha)
{
  if (red)
    GE( cogl_wrap_glGetIntegerv(GL_RED_BITS, red) );
  if (green)
    GE( cogl_wrap_glGetIntegerv(GL_GREEN_BITS, green) );
  if (blue)
    GE( cogl_wrap_glGetIntegerv(GL_BLUE_BITS, blue) );
  if (alpha)
    GE( cogl_wrap_glGetIntegerv(GL_ALPHA_BITS, alpha ) );
}

void
cogl_fog_set (const CoglColor *fog_color,
              CoglFixed        density,
              CoglFixed        z_near,
              CoglFixed        z_far)
{
  GLfixed fogColor[4];

  fogColor[0] = cogl_color_get_red (fog_color);
  fogColor[1] = cogl_color_get_green (fog_color);
  fogColor[2] = cogl_color_get_blue (fog_color);
  fogColor[3] = cogl_color_get_alpha (fog_color);

  cogl_wrap_glEnable (GL_FOG);

  cogl_wrap_glFogxv (GL_FOG_COLOR, fogColor);

  cogl_wrap_glFogx (GL_FOG_MODE, GL_LINEAR);
  glHint (GL_FOG_HINT, GL_NICEST);

  cogl_wrap_glFogx (GL_FOG_DENSITY, (GLfixed) density);
  cogl_wrap_glFogx (GL_FOG_START, (GLfixed) z_near);
  cogl_wrap_glFogx (GL_FOG_END, (GLfixed) z_far);
}
