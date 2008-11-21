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
  /* Sucks to ifdef here but not other option..? would be nice to
   * split the code up for more reuse (once more backends use this
   */
#if defined(HAVE_CLUTTER_GLX)
  static GLXGetProcAddressProc get_proc_func = NULL;
  static void                 *dlhand = NULL;

  if (get_proc_func == NULL && dlhand == NULL)
    {
      dlhand = dlopen (NULL, RTLD_LAZY);

      if (dlhand)
	{
	  dlerror ();

	  get_proc_func =
            (GLXGetProcAddressProc) dlsym (dlhand, "glXGetProcAddress");

	  if (dlerror () != NULL)
            {
              get_proc_func =
                (GLXGetProcAddressProc) dlsym (dlhand, "glXGetProcAddressARB");
            }

	  if (dlerror () != NULL)
	    {
	      get_proc_func = NULL;
	      g_warning ("failed to bind GLXGetProcAddress "
                         "or GLXGetProcAddressARB");
	    }
	}
    }

  if (get_proc_func)
    return get_proc_func ((unsigned char*) name);

#elif defined(HAVE_CLUTTER_WIN32)

  return (CoglFuncPtr) wglGetProcAddress ((LPCSTR) name);

#else /* HAVE_CLUTTER_WIN32 */

  /* this should find the right function if the program is linked against a
   * library providing it */
  static GModule *module = NULL;
  if (module == NULL)
    module = g_module_open (NULL, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

  if (module)
    {
      gpointer symbol;

      if (g_module_symbol (module, name, &symbol))
        return symbol;
    }

#endif /* HAVE_CLUTTER_WIN32 */

  return NULL;
}

gboolean
cogl_check_extension (const gchar *name, const gchar *ext)
{
  gchar *end;
  gint name_len, n;

  if (name == NULL || ext == NULL)
    return FALSE;

  end = (gchar*)(ext + strlen(ext));

  name_len = strlen(name);

  while (ext < end)
    {
      n = strcspn(ext, " ");

      if ((name_len == n) && (!strncmp(name, ext, n)))
	return TRUE;
      ext += (n + 1);
    }

  return FALSE;
}

void
cogl_paint_init (const CoglColor *color)
{
  GE( glClearColor (cogl_color_get_red_float (color),
                    cogl_color_get_green_float (color),
                    cogl_color_get_blue_float (color),
                    0.0) );

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glDisable (GL_LIGHTING);
  glDisable (GL_FOG);

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

/* FIXME: inline most of these  */
void
cogl_push_matrix (void)
{
  glPushMatrix();
}

void
cogl_pop_matrix (void)
{
  glPopMatrix();
}

void
cogl_scale (CoglFixed x, CoglFixed y)
{
  glScaled (COGL_FIXED_TO_DOUBLE (x),
	    COGL_FIXED_TO_DOUBLE (y),
	    1.0);
}

void
cogl_translatex (CoglFixed x, CoglFixed y, CoglFixed z)
{
  glTranslated (COGL_FIXED_TO_DOUBLE (x),
		COGL_FIXED_TO_DOUBLE (y),
		COGL_FIXED_TO_DOUBLE (z));
}

void
cogl_translate (gint x, gint y, gint z)
{
  glTranslatef ((float)x, (float)y, (float)z);
}

void
cogl_rotatex (CoglFixed angle, gint x, gint y, gint z)
{
  glRotated (COGL_FIXED_TO_DOUBLE (angle),
	     COGL_FIXED_TO_DOUBLE (x),
	     COGL_FIXED_TO_DOUBLE (y),
	     COGL_FIXED_TO_DOUBLE (z));
}

void
cogl_rotate (gint angle, gint x, gint y, gint z)
{
  glRotatef ((float)angle, (float)x, (float)y, (float)z);
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
  
  glColor4ub (cogl_color_get_red_byte (color),
	      cogl_color_get_green_byte (color),
	      cogl_color_get_blue_byte (color),
	      cogl_color_get_alpha_byte (color));
  
  /* Store alpha for proper blending enables */
  ctx->color_alpha = cogl_color_get_alpha_byte (color);
}

static void
apply_matrix (const GLfloat *matrix, GLfloat *vertex)
{
  int x, y;
  GLfloat vertex_out[4] = { 0 };

  for (y = 0; y < 4; y++)
    for (x = 0; x < 4; x++)
      vertex_out[y] += vertex[x] * matrix[y + x * 4];

  memcpy (vertex, vertex_out, sizeof (vertex_out));
}

static void
project_vertex (GLfloat *modelview, GLfloat *project, GLfloat *vertex)
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
		const GLfloat *vertex_a,
		const GLfloat *vertex_b)
{
  GLdouble plane[4];
  GLfloat angle;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Calculate the angle between the axes and the line crossing the
     two points */
  angle = atan2f ((vertex_b[1] - vertex_a[1]),
		  (vertex_b[0] - vertex_a[0])) * 180.0f / G_PI;

  GE( glPushMatrix () );
  /* Load the identity matrix and multiply by the reverse of the
     projection matrix so we can specify the plane in screen
     coordinates */
  GE( glLoadIdentity () );
  GE( glMultMatrixf (ctx->inverse_projection) );
  /* Rotate about point a */
  GE( glTranslatef (vertex_a[0], vertex_a[1], vertex_a[2]) );
  /* Rotate the plane by the calculated angle so that it will connect
     the two points */
  GE( glRotatef (angle, 0.0f, 0.0f, 1.0f) );
  GE( glTranslatef (-vertex_a[0], -vertex_a[1], -vertex_a[2]) );

  plane[0] = 0.0f;
  plane[1] = -1.0f;
  plane[2] = 0.0f;
  plane[3] = vertex_a[1];
  GE( glClipPlane (plane_num, plane) );

  GE( glPopMatrix () );

  GE( glEnable (plane_num) );
}

void
_cogl_set_clip_planes (CoglFixed x_offset,
		       CoglFixed y_offset,
		       CoglFixed width,
		       CoglFixed height)
{
  GLfloat modelview[16], projection[16];

  GLfloat vertex_tl[4] = { COGL_FIXED_TO_FLOAT (x_offset),
			   COGL_FIXED_TO_FLOAT (y_offset),
			   0.0f, 1.0f };
  GLfloat vertex_tr[4] = { COGL_FIXED_TO_FLOAT (x_offset + width),
			   COGL_FIXED_TO_FLOAT (y_offset),
			   0.0f, 1.0f };
  GLfloat vertex_bl[4] = { COGL_FIXED_TO_FLOAT (x_offset),
			   COGL_FIXED_TO_FLOAT (y_offset + height),
			   0.0f, 1.0f };
  GLfloat vertex_br[4] = { COGL_FIXED_TO_FLOAT (x_offset + width),
			   COGL_FIXED_TO_FLOAT (y_offset + height),
			   0.0f, 1.0f };

  GE( glGetFloatv (GL_MODELVIEW_MATRIX, modelview) );
  GE( glGetFloatv (GL_PROJECTION_MATRIX, projection) );

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
      GLfloat temp[4];
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

static int
compare_y_coordinate (const void *a, const void *b)
{
  GLfloat ay = ((const GLfloat *) a)[1];
  GLfloat by = ((const GLfloat *) b)[1];
  
  return ay < by ? -1 : ay > by ? 1 : 0;
}

void
_cogl_add_stencil_clip (CoglFixed x_offset,
			CoglFixed y_offset,
			CoglFixed width,
			CoglFixed height,
			gboolean first)
{
  gboolean has_clip_planes
    = cogl_features_available (COGL_FEATURE_FOUR_CLIP_PLANES);
  
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (has_clip_planes)
    {
      GE( glDisable (GL_CLIP_PLANE3) );
      GE( glDisable (GL_CLIP_PLANE2) );
      GE( glDisable (GL_CLIP_PLANE1) );
      GE( glDisable (GL_CLIP_PLANE0) );
    }

  if (first)
    {
      GE( glEnable (GL_STENCIL_TEST) );
      
      /* Initially disallow everything */
      GE( glClearStencil (0) );
      GE( glClear (GL_STENCIL_BUFFER_BIT) );

      /* Punch out a hole to allow the rectangle */
      GE( glStencilFunc (GL_NEVER, 0x1, 0x1) );
      GE( glStencilOp (GL_REPLACE, GL_REPLACE, GL_REPLACE) );
      GE( glRectf (COGL_FIXED_TO_FLOAT (x_offset),
		   COGL_FIXED_TO_FLOAT (y_offset),
		   COGL_FIXED_TO_FLOAT (x_offset + width),
		   COGL_FIXED_TO_FLOAT (y_offset + height)) );
    }
  else if (ctx->num_stencil_bits > 1)
    {
      /* Add one to every pixel of the stencil buffer in the
	 rectangle */
      GE( glStencilFunc (GL_NEVER, 0x1, 0x3) );
      GE( glStencilOp (GL_INCR, GL_INCR, GL_INCR) );
      GE( glRectf (COGL_FIXED_TO_FLOAT (x_offset),
		   COGL_FIXED_TO_FLOAT (y_offset),
		   COGL_FIXED_TO_FLOAT (x_offset + width),
		   COGL_FIXED_TO_FLOAT (y_offset + height)) );

      /* Subtract one from all pixels in the stencil buffer so that
	 only pixels where both the original stencil buffer and the
	 rectangle are set will be valid */
      GE( glStencilOp (GL_DECR, GL_DECR, GL_DECR) );
      GE( glPushMatrix () );
      GE( glLoadIdentity () );
      GE( glMatrixMode (GL_PROJECTION) );
      GE( glPushMatrix () );
      GE( glLoadIdentity () );
      GE( glRecti (-1, 1, 1, -1) );
      GE( glPopMatrix () );
      GE( glMatrixMode (GL_MODELVIEW) );
      GE( glPopMatrix () );
    }
  else
    {
      /* Slower fallback if there is exactly one stencil bit. This
	 tries to draw enough triangles to tessalate around the
	 rectangle so that it can subtract from the stencil buffer for
	 every pixel in the screen except those in the rectangle */
      GLfloat modelview[16], projection[16];
      GLfloat temp_point[4];
      GLfloat left_edge, right_edge, bottom_edge, top_edge;
      int i;
      GLfloat points[16] =
	{
	  COGL_FIXED_TO_FLOAT (x_offset),
	  COGL_FIXED_TO_FLOAT (y_offset),
	  0, 1,
	  COGL_FIXED_TO_FLOAT (x_offset + width),
	  COGL_FIXED_TO_FLOAT (y_offset),
	  0, 1,
	  COGL_FIXED_TO_FLOAT (x_offset),
	  COGL_FIXED_TO_FLOAT (y_offset + height),
	  0, 1,
	  COGL_FIXED_TO_FLOAT (x_offset + width),
	  COGL_FIXED_TO_FLOAT (y_offset + height),
	  0, 1
	};

      GE( glGetFloatv (GL_MODELVIEW_MATRIX, modelview) );
      GE( glGetFloatv (GL_PROJECTION_MATRIX, projection) );

      /* Project all of the vertices into screen coordinates */
      for (i = 0; i < 4; i++)
	project_vertex (modelview, projection, points + i * 4);

      /* Sort the points by y coordinate */
      qsort (points, 4, sizeof (GLfloat) * 4, compare_y_coordinate);

      /* Put the bottom two pairs and the top two pairs in
	 left-right order */
      if (points[0] > points[4])
	{
	  memcpy (temp_point, points, sizeof (GLfloat) * 4);
	  memcpy (points, points + 4, sizeof (GLfloat) * 4);
	  memcpy (points + 4, temp_point, sizeof (GLfloat) * 4);
	}
      if (points[8] > points[12])
	{
	  memcpy (temp_point, points + 8, sizeof (GLfloat) * 4);
	  memcpy (points + 8, points + 12, sizeof (GLfloat) * 4);
	  memcpy (points + 12, temp_point, sizeof (GLfloat) * 4);
	}

      /* If the clip rect goes outside of the screen then use the
	 extents of the rect instead */
      left_edge   = MIN (-1.0f, MIN (points[0], points[8]));
      right_edge  = MAX ( 1.0f, MAX (points[4], points[12]));
      bottom_edge = MIN (-1.0f, MIN (points[1], points[5]));
      top_edge    = MAX ( 1.0f, MAX (points[9], points[13]));

      /* Using the identity matrix for the projection and
	 modelview matrix, draw the triangles around the inner
	 rectangle */
      GE( glStencilFunc (GL_NEVER, 0x1, 0x1) );
      GE( glStencilOp (GL_ZERO, GL_ZERO, GL_ZERO) );
      GE( glPushMatrix () );
      GE( glLoadIdentity () );
      GE( glMatrixMode (GL_PROJECTION) );
      GE( glPushMatrix () );
      GE( glLoadIdentity () );

      /* Clear the left side */
      glBegin (GL_TRIANGLE_STRIP);
      glVertex2f (left_edge, bottom_edge);
      glVertex2fv (points);
      glVertex2f (left_edge, points[1]);
      glVertex2fv (points + 8);
      glVertex2f (left_edge, points[9]);
      glVertex2f (left_edge, top_edge);
      glEnd ();

      /* Clear the right side */
      glBegin (GL_TRIANGLE_STRIP);
      glVertex2f (right_edge, top_edge);
      glVertex2fv (points + 12);
      glVertex2f (right_edge, points[13]);
      glVertex2fv (points + 4);
      glVertex2f (right_edge, points[5]);
      glVertex2f (right_edge, bottom_edge);
      glEnd ();

      /* Clear the top side */
      glBegin (GL_TRIANGLE_STRIP);
      glVertex2f (left_edge, top_edge);
      glVertex2fv (points + 8);
      glVertex2f (points[8], top_edge);
      glVertex2fv (points + 12);
      glVertex2f (points[12], top_edge);
      glVertex2f (right_edge, top_edge);
      glEnd ();

      /* Clear the bottom side */
      glBegin (GL_TRIANGLE_STRIP);
      glVertex2f (left_edge, bottom_edge);
      glVertex2fv (points);
      glVertex2f (points[0], bottom_edge);
      glVertex2fv (points + 4);
      glVertex2f (points[4], bottom_edge);
      glVertex2f (right_edge, bottom_edge);
      glEnd ();

      GE( glPopMatrix () );
      GE( glMatrixMode (GL_MODELVIEW) );
      GE( glPopMatrix () );
    }

  if (has_clip_planes)
    {
      GE( glEnable (GL_CLIP_PLANE0) );
      GE( glEnable (GL_CLIP_PLANE1) );
      GE( glEnable (GL_CLIP_PLANE2) );
      GE( glEnable (GL_CLIP_PLANE3) );
    }

  /* Restore the stencil mode */
  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );
}

void
_cogl_set_matrix (const CoglFixed *matrix)
{
  float float_matrix[16];
  int i;

  for (i = 0; i < 16; i++)
    float_matrix[i] = COGL_FIXED_TO_FLOAT (matrix[i]);

  GE( glLoadIdentity () );
  GE( glMultMatrixf (float_matrix) );
}

void
_cogl_disable_stencil_buffer (void)
{
  GE( glDisable (GL_STENCIL_TEST) );
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
cogl_alpha_func (COGLenum     func,
		 CoglFixed ref)
{
  GE( glAlphaFunc (func, COGL_FIXED_TO_FLOAT(ref)) );
}

void
cogl_perspective (CoglFixed fovy,
		  CoglFixed aspect,
		  CoglFixed zNear,
		  CoglFixed zFar)
{
  CoglFixed xmax, ymax;
  CoglFixed x, y, c, d;
  CoglFixed fovy_rad_half = COGL_FIXED_MUL (fovy, COGL_FIXED_PI) / 360;

  GLfloat m[16];

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  memset (&m[0], 0, sizeof (m));

  GE( glMatrixMode (GL_PROJECTION) );
  GE( glLoadIdentity () );

  /*
   * Based on the original algorithm in perspective():
   *
   * 1) xmin = -xmax => xmax + xmin == 0 && xmax - xmin == 2 * xmax
   * same true for y, hence: a == 0 && b == 0;
   *
   * 2) When working with small numbers, we are loosing significant
   * precision
   */
  ymax =
    COGL_FIXED_MUL (zNear,
                    COGL_FIXED_FAST_DIV (cogl_fixed_sin (fovy_rad_half),
                                         cogl_fixed_cos (fovy_rad_half)));

  xmax = COGL_FIXED_MUL (ymax, aspect);

  x = COGL_FIXED_FAST_DIV (zNear, xmax);
  y = COGL_FIXED_FAST_DIV (zNear, ymax);
  c = COGL_FIXED_FAST_DIV (-(zFar + zNear), ( zFar - zNear));
  d = cogl_fixed_mul_div (-(2 * zFar), zNear, (zFar - zNear));

#define M(row,col)  m[col*4+row]
  M(0,0) = COGL_FIXED_TO_FLOAT (x);
  M(1,1) = COGL_FIXED_TO_FLOAT (y);
  M(2,2) = COGL_FIXED_TO_FLOAT (c);
  M(2,3) = COGL_FIXED_TO_FLOAT (d);
  M(3,2) = -1.0F;

  GE( glMultMatrixf (m) );

  GE( glMatrixMode (GL_MODELVIEW) );

  /* Calculate and store the inverse of the matrix */
  memset (ctx->inverse_projection, 0, sizeof (GLfloat) * 16);

#define m ctx->inverse_projection
  M(0, 0) = 1.0f / COGL_FIXED_TO_FLOAT (x);
  M(1, 1) = 1.0f / COGL_FIXED_TO_FLOAT (y);
  M(2, 3) = -1.0f;
  M(3, 2) = 1.0f / COGL_FIXED_TO_FLOAT (d);
  M(3, 3) = COGL_FIXED_TO_FLOAT (c) / COGL_FIXED_TO_FLOAT (d);
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
  GLfloat c, d;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( glMatrixMode (GL_PROJECTION) );
  GE( glLoadIdentity () );

  GE( glFrustum (COGL_FIXED_TO_DOUBLE (left),
		 COGL_FIXED_TO_DOUBLE (right),
		 COGL_FIXED_TO_DOUBLE (bottom),
		 COGL_FIXED_TO_DOUBLE (top),
		 COGL_FIXED_TO_DOUBLE (z_near),
		 COGL_FIXED_TO_DOUBLE (z_far)) );

  GE( glMatrixMode (GL_MODELVIEW) );

  /* Calculate and store the inverse of the matrix */
  memset (ctx->inverse_projection, 0, sizeof (GLfloat) * 16);

  c = -COGL_FIXED_TO_FLOAT (z_far + z_near)
    / COGL_FIXED_TO_FLOAT (z_far - z_near);
  d = -COGL_FIXED_TO_FLOAT (2 * COGL_FIXED_MUL (z_far, z_near))
    / COGL_FIXED_TO_FLOAT (z_far - z_near);

#define M(row,col)  ctx->inverse_projection[col*4+row]
  M(0,0) = COGL_FIXED_TO_FLOAT (right - left)
    / COGL_FIXED_TO_FLOAT (2 * z_near);
  M(0,3) = COGL_FIXED_TO_FLOAT (right + left)
    / COGL_FIXED_TO_FLOAT (2 * z_near);
  M(1,1) = COGL_FIXED_TO_FLOAT (top - bottom)
    / COGL_FIXED_TO_FLOAT (2 * z_near);
  M(1,3) = COGL_FIXED_TO_FLOAT (top + bottom)
    / COGL_FIXED_TO_FLOAT (2 * z_near);
  M(2,3) = -1.0f;
  M(3,2) = 1.0f / d;
  M(3,3) = c / d;
#undef M
}

void
cogl_viewport (guint width,
	       guint height)
{
  GE( glViewport (0, 0, width, height) );
}

void
cogl_setup_viewport (guint        width,
		     guint        height,
		     CoglFixed fovy,
		     CoglFixed aspect,
		     CoglFixed z_near,
		     CoglFixed z_far)
{
  GLfloat z_camera;

  GE( glViewport (0, 0, width, height) );

  cogl_perspective (fovy, aspect, z_near, z_far);

  GE( glLoadIdentity () );

  /*
   * camera distance from screen, 0.5 * tan (FOV)
   *
   * We have been having some problems with this; the theoretically correct
   * value of 0.866025404f for the default 60 deg fovy angle happens to be
   * touch to small in reality, which on full-screen stage with an actor of
   * the same size results in about 1px on the left and top edges of the
   * actor being offscreen. Perhaps more significantly, it also causes
   * hinting artifacts when rendering text.
   *
   * So for the default 60 deg angle we worked out that the value of 0.869
   * is giving correct stretch and no noticeable artifacts on text. Seems
   * good on all drivers too.
   */
#define DEFAULT_Z_CAMERA 0.869f
  z_camera = DEFAULT_Z_CAMERA;


  if (fovy != COGL_FIXED_60)
  {
    CoglFixed fovy_rad = COGL_FIXED_MUL (fovy, COGL_FIXED_PI) / 180;

    z_camera =
      COGL_FIXED_TO_FLOAT (COGL_FIXED_DIV (cogl_fixed_sin (fovy_rad),
				           cogl_fixed_cos (fovy_rad)) >> 1);
  }

  GE( glTranslatef (-0.5f, -0.5f, -z_camera) );
  GE( glScalef ( 1.0f / width,
 	    -1.0f / height,
		 1.0f / width) );
  GE( glTranslatef (0.0f, -1.0 * height, 0.0f) );
}

#ifdef HAVE_CLUTTER_OSX
static gboolean
really_enable_npot (void)
{
  /* OSX backend + ATI Radeon X1600 + NPOT texture + GL_REPEAT seems to crash
   * http://bugzilla.openedhand.com/show_bug.cgi?id=929
   *
   * Temporary workaround until post 0.8 we rejig the features set up a
   * little to allow the backend to overide.
   */
  const char *gl_renderer;
  const char *env_string;

  /* Regardless of hardware, allow user to decide. */
  env_string = g_getenv ("COGL_ENABLE_NPOT");
  if (env_string != NULL)
    return env_string[0] == '1';

  gl_renderer = (char*)glGetString (GL_RENDERER);
  if (strstr (gl_renderer, "ATI Radeon X1600") != NULL)
    return FALSE;

  return TRUE;
}
#endif

static void
_cogl_features_init ()
{
  CoglFeatureFlags  flags = 0;
  const gchar      *gl_extensions;
  GLint             max_clip_planes = 0;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  flags = COGL_FEATURE_TEXTURE_READ_PIXELS;

  gl_extensions = (const gchar*) glGetString (GL_EXTENSIONS);

  if (cogl_check_extension ("GL_ARB_texture_non_power_of_two", gl_extensions))
    {
#ifdef HAVE_CLUTTER_OSX
      if (really_enable_npot ())
#endif
        flags |= COGL_FEATURE_TEXTURE_NPOT;
    }
  
#ifdef GL_YCBCR_MESA
  if (cogl_check_extension ("GL_MESA_ycbcr_texture", gl_extensions))
    {
      flags |= COGL_FEATURE_TEXTURE_YUV;
    }
#endif

  if (cogl_check_extension ("GL_ARB_shader_objects", gl_extensions) &&
      cogl_check_extension ("GL_ARB_vertex_shader", gl_extensions) &&
      cogl_check_extension ("GL_ARB_fragment_shader", gl_extensions))
    {
      ctx->pf_glCreateProgramObjectARB =
	(COGL_PFNGLCREATEPROGRAMOBJECTARBPROC)
	cogl_get_proc_address ("glCreateProgramObjectARB");
      
      ctx->pf_glCreateShaderObjectARB =
	(COGL_PFNGLCREATESHADEROBJECTARBPROC)
	cogl_get_proc_address ("glCreateShaderObjectARB");
      
      ctx->pf_glShaderSourceARB =
	(COGL_PFNGLSHADERSOURCEARBPROC)
	cogl_get_proc_address ("glShaderSourceARB");
      
      ctx->pf_glCompileShaderARB =
	(COGL_PFNGLCOMPILESHADERARBPROC)
	cogl_get_proc_address ("glCompileShaderARB");
      
      ctx->pf_glAttachObjectARB =
	(COGL_PFNGLATTACHOBJECTARBPROC)
	cogl_get_proc_address ("glAttachObjectARB");
      
      ctx->pf_glLinkProgramARB =
	(COGL_PFNGLLINKPROGRAMARBPROC)
	cogl_get_proc_address ("glLinkProgramARB");
      
      ctx->pf_glUseProgramObjectARB =
	(COGL_PFNGLUSEPROGRAMOBJECTARBPROC)
	cogl_get_proc_address ("glUseProgramObjectARB");
      
      ctx->pf_glGetUniformLocationARB =
	(COGL_PFNGLGETUNIFORMLOCATIONARBPROC)
	cogl_get_proc_address ("glGetUniformLocationARB");
      
      ctx->pf_glDeleteObjectARB =
	(COGL_PFNGLDELETEOBJECTARBPROC)
	cogl_get_proc_address ("glDeleteObjectARB");
      
      ctx->pf_glGetInfoLogARB =
	(COGL_PFNGLGETINFOLOGARBPROC)
	cogl_get_proc_address ("glGetInfoLogARB");
      
      ctx->pf_glGetObjectParameterivARB =
	(COGL_PFNGLGETOBJECTPARAMETERIVARBPROC)
	cogl_get_proc_address ("glGetObjectParameterivARB");
      
      ctx->pf_glUniform1fARB =
	(COGL_PFNGLUNIFORM1FARBPROC)
	cogl_get_proc_address ("glUniform1fARB");

      ctx->pf_glVertexAttribPointerARB =
	(COGL_PFNGLVERTEXATTRIBPOINTERARBPROC)
	cogl_get_proc_address ("glVertexAttribPointerARB");

      ctx->pf_glEnableVertexAttribArrayARB =
	(COGL_PFNGLENABLEVERTEXATTRIBARRAYARBPROC)
	cogl_get_proc_address ("glEnableVertexAttribArrayARB");

      ctx->pf_glDisableVertexAttribArrayARB =
	(COGL_PFNGLDISABLEVERTEXATTRIBARRAYARBPROC)
	cogl_get_proc_address ("glDisableVertexAttribArrayARB");
      
      ctx->pf_glUniform2fARB =
	(COGL_PFNGLUNIFORM2FARBPROC)
	cogl_get_proc_address ("glUniform2fARB");

      ctx->pf_glUniform3fARB =
	(COGL_PFNGLUNIFORM3FARBPROC)
	cogl_get_proc_address ("glUniform3fARB");

      ctx->pf_glUniform4fARB =
	(COGL_PFNGLUNIFORM4FARBPROC)
	cogl_get_proc_address ("glUniform4fARB");

      ctx->pf_glUniform1fvARB =
	(COGL_PFNGLUNIFORM1FVARBPROC)
	cogl_get_proc_address ("glUniform1fvARB");

      ctx->pf_glUniform2fvARB =
	(COGL_PFNGLUNIFORM2FVARBPROC)
	cogl_get_proc_address ("glUniform2fvARB");

      ctx->pf_glUniform3fvARB =
	(COGL_PFNGLUNIFORM3FVARBPROC)
	cogl_get_proc_address ("glUniform3fvARB");

      ctx->pf_glUniform4fvARB =
	(COGL_PFNGLUNIFORM4FVARBPROC)
	cogl_get_proc_address ("glUniform4fvARB");

      ctx->pf_glUniform1iARB =
	(COGL_PFNGLUNIFORM1IARBPROC)
	cogl_get_proc_address ("glUniform1iARB");

      ctx->pf_glUniform2iARB =
	(COGL_PFNGLUNIFORM2IARBPROC)
	cogl_get_proc_address ("glUniform2iARB");

      ctx->pf_glUniform3iARB =
	(COGL_PFNGLUNIFORM3IARBPROC)
	cogl_get_proc_address ("glUniform3iARB");

      ctx->pf_glUniform4iARB =
	(COGL_PFNGLUNIFORM4IARBPROC)
	cogl_get_proc_address ("glUniform4iARB");

      ctx->pf_glUniform1ivARB =
	(COGL_PFNGLUNIFORM1IVARBPROC)
	cogl_get_proc_address ("glUniform1ivARB");

      ctx->pf_glUniform2ivARB =
	(COGL_PFNGLUNIFORM2IVARBPROC)
	cogl_get_proc_address ("glUniform2ivARB");

      ctx->pf_glUniform3ivARB =
	(COGL_PFNGLUNIFORM3IVARBPROC)
	cogl_get_proc_address ("glUniform3ivARB");

      ctx->pf_glUniform4ivARB =
	(COGL_PFNGLUNIFORM4IVARBPROC)
	cogl_get_proc_address ("glUniform4ivARB");

      ctx->pf_glUniformMatrix2fvARB =
	(COGL_PFNGLUNIFORMMATRIX2FVARBPROC)
	cogl_get_proc_address ("glUniformMatrix2fvARB");
 
      ctx->pf_glUniformMatrix3fvARB =
	(COGL_PFNGLUNIFORMMATRIX3FVARBPROC)
	cogl_get_proc_address ("glUniformMatrix3fvARB");

      ctx->pf_glUniformMatrix4fvARB =
	(COGL_PFNGLUNIFORMMATRIX4FVARBPROC)
	cogl_get_proc_address ("glUniformMatrix4fvARB");

      if (ctx->pf_glCreateProgramObjectARB    &&
	  ctx->pf_glCreateShaderObjectARB     &&
	  ctx->pf_glShaderSourceARB           &&
	  ctx->pf_glCompileShaderARB          &&
	  ctx->pf_glAttachObjectARB           &&
	  ctx->pf_glLinkProgramARB            &&
	  ctx->pf_glUseProgramObjectARB       &&
	  ctx->pf_glGetUniformLocationARB     &&
	  ctx->pf_glDeleteObjectARB           &&
	  ctx->pf_glGetInfoLogARB             &&
	  ctx->pf_glGetObjectParameterivARB   &&
	  ctx->pf_glUniform1fARB              &&
	  ctx->pf_glUniform2fARB              &&
	  ctx->pf_glUniform3fARB              &&
	  ctx->pf_glUniform4fARB              &&
	  ctx->pf_glUniform1fvARB             &&
	  ctx->pf_glUniform2fvARB             &&
	  ctx->pf_glUniform3fvARB             &&
	  ctx->pf_glUniform4fvARB             &&
	  ctx->pf_glUniform1iARB              &&
	  ctx->pf_glUniform2iARB              &&
	  ctx->pf_glUniform3iARB              &&
	  ctx->pf_glUniform4iARB              &&
	  ctx->pf_glUniform1ivARB             &&
	  ctx->pf_glUniform2ivARB             &&
	  ctx->pf_glUniform3ivARB             &&
	  ctx->pf_glUniform4ivARB             &&
	  ctx->pf_glUniformMatrix2fvARB       &&
	  ctx->pf_glUniformMatrix3fvARB       &&
	  ctx->pf_glUniformMatrix4fvARB       &&
	  ctx->pf_glVertexAttribPointerARB    &&
	  ctx->pf_glEnableVertexAttribArrayARB &&
	  ctx->pf_glDisableVertexAttribArrayARB)
	flags |= COGL_FEATURE_SHADERS_GLSL;
    }
  
  if (cogl_check_extension ("GL_EXT_framebuffer_object", gl_extensions) ||
      cogl_check_extension ("GL_ARB_framebuffer_object", gl_extensions))
    { 
      ctx->pf_glGenRenderbuffersEXT =
	(COGL_PFNGLGENRENDERBUFFERSEXTPROC)
	cogl_get_proc_address ("glGenRenderbuffersEXT");
      
      ctx->pf_glDeleteRenderbuffersEXT =
	(COGL_PFNGLDELETERENDERBUFFERSEXTPROC)
	cogl_get_proc_address ("glDeleteRenderbuffersEXT");
      
      ctx->pf_glBindRenderbufferEXT =
	(COGL_PFNGLBINDRENDERBUFFEREXTPROC)
	cogl_get_proc_address ("glBindRenderbufferEXT");
      
      ctx->pf_glRenderbufferStorageEXT =
	(COGL_PFNGLRENDERBUFFERSTORAGEEXTPROC)
	cogl_get_proc_address ("glRenderbufferStorageEXT");
      
      ctx->pf_glGenFramebuffersEXT =
	(COGL_PFNGLGENFRAMEBUFFERSEXTPROC)
	cogl_get_proc_address ("glGenFramebuffersEXT");
      
      ctx->pf_glBindFramebufferEXT =
	(COGL_PFNGLBINDFRAMEBUFFEREXTPROC)
	cogl_get_proc_address ("glBindFramebufferEXT");
      
      ctx->pf_glFramebufferTexture2DEXT =
	(COGL_PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)
	cogl_get_proc_address ("glFramebufferTexture2DEXT");
      
      ctx->pf_glFramebufferRenderbufferEXT =
	(COGL_PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)
	cogl_get_proc_address ("glFramebufferRenderbufferEXT");
      
      ctx->pf_glCheckFramebufferStatusEXT =
	(COGL_PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)
	cogl_get_proc_address ("glCheckFramebufferStatusEXT");
      
      ctx->pf_glDeleteFramebuffersEXT =
	(COGL_PFNGLDELETEFRAMEBUFFERSEXTPROC)
	cogl_get_proc_address ("glDeleteFramebuffersEXT");
      
      if (ctx->pf_glGenRenderbuffersEXT         &&
	  ctx->pf_glBindRenderbufferEXT         &&
	  ctx->pf_glRenderbufferStorageEXT      &&
	  ctx->pf_glGenFramebuffersEXT          &&
	  ctx->pf_glBindFramebufferEXT          &&
	  ctx->pf_glFramebufferTexture2DEXT     &&
	  ctx->pf_glFramebufferRenderbufferEXT  &&
	  ctx->pf_glCheckFramebufferStatusEXT   &&
	  ctx->pf_glDeleteFramebuffersEXT)
	flags |= COGL_FEATURE_OFFSCREEN;
    }
  
  if (cogl_check_extension ("GL_EXT_framebuffer_blit", gl_extensions))
    {
      ctx->pf_glBlitFramebufferEXT =
	(COGL_PFNGLBLITFRAMEBUFFEREXTPROC)
	cogl_get_proc_address ("glBlitFramebufferEXT");
      
      if (ctx->pf_glBlitFramebufferEXT)
	flags |= COGL_FEATURE_OFFSCREEN_BLIT;
    }
  
  if (cogl_check_extension ("GL_EXT_framebuffer_multisample", gl_extensions))
    {
      ctx->pf_glRenderbufferStorageMultisampleEXT =
	(COGL_PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)
	cogl_get_proc_address ("glRenderbufferStorageMultisampleEXT");
      
      if (ctx->pf_glRenderbufferStorageMultisampleEXT)
	flags |= COGL_FEATURE_OFFSCREEN_MULTISAMPLE;
    }

  ctx->num_stencil_bits = 0;
  GE( glGetIntegerv (GL_STENCIL_BITS, &ctx->num_stencil_bits) );
  if (ctx->num_stencil_bits > 0)
    flags |= COGL_FEATURE_STENCIL_BUFFER;

  GE( glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
  if (max_clip_planes >= 4)
    flags |= COGL_FEATURE_FOUR_CLIP_PLANES;

  if (cogl_check_extension ("GL_ARB_vertex_buffer_object", gl_extensions))
    {
      ctx->pf_glGenBuffersARB =
	    (COGL_PFNGLGENBUFFERSARBPROC)
	    cogl_get_proc_address ("glGenBuffersARB");
      ctx->pf_glBindBufferARB =
	    (COGL_PFNGLBINDBUFFERARBPROC)
	    cogl_get_proc_address ("glBindBufferARB");
      ctx->pf_glBufferDataARB =
	    (COGL_PFNGLBUFFERDATAARBPROC)
	    cogl_get_proc_address ("glBufferDataARB");
      ctx->pf_glBufferDataSubARB =
	    (COGL_PFNGLBUFFERDATASUBARBPROC)
	    cogl_get_proc_address ("glBufferDataSubARB");
      ctx->pf_glDeleteBuffersARB =
	    (COGL_PFNGLDELETEBUFFERSARBPROC)
	    cogl_get_proc_address ("glDeleteBuffersARB");
      ctx->pf_glMapBufferARB =
	    (COGL_PFNGLMAPBUFFERARBPROC)
	    cogl_get_proc_address ("glMapBufferARB");
      ctx->pf_glUnmapBufferARB =
	    (COGL_PFNGLUNMAPBUFFERARBPROC)
	    cogl_get_proc_address ("glUnmapBufferARB");
      if (ctx->pf_glGenBuffersARB
	  && ctx->pf_glBindBufferARB
	  && ctx->pf_glBufferDataARB
	  && ctx->pf_glBufferDataSubARB
	  && ctx->pf_glDeleteBuffersARB
	  && ctx->pf_glMapBufferARB
	  && ctx->pf_glUnmapBufferARB)
      flags |= COGL_FEATURE_VBOS;
    }

  /* This should always be available because it is defined in GL 1.2,
     but we can't call it directly because under Windows functions >
     1.1 aren't exported */
  ctx->pf_glDrawRangeElements =
        (COGL_PFNGLDRAWRANGEELEMENTSPROC)
        cogl_get_proc_address ("glDrawRangeElements");

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
cogl_get_modelview_matrix (CoglFixed m[16])
{
  GLdouble md[16];

  glGetDoublev(GL_MODELVIEW_MATRIX, &md[0]);

#define M(m,row,col)  m[col*4+row]
  M(m,0,0) = COGL_FIXED_FROM_FLOAT (M(md,0,0));
  M(m,0,1) = COGL_FIXED_FROM_FLOAT (M(md,0,1));
  M(m,0,2) = COGL_FIXED_FROM_FLOAT (M(md,0,2));
  M(m,0,3) = COGL_FIXED_FROM_FLOAT (M(md,0,3));

  M(m,1,0) = COGL_FIXED_FROM_FLOAT (M(md,1,0));
  M(m,1,1) = COGL_FIXED_FROM_FLOAT (M(md,1,1));
  M(m,1,2) = COGL_FIXED_FROM_FLOAT (M(md,1,2));
  M(m,1,3) = COGL_FIXED_FROM_FLOAT (M(md,1,3));

  M(m,2,0) = COGL_FIXED_FROM_FLOAT (M(md,2,0));
  M(m,2,1) = COGL_FIXED_FROM_FLOAT (M(md,2,1));
  M(m,2,2) = COGL_FIXED_FROM_FLOAT (M(md,2,2));
  M(m,2,3) = COGL_FIXED_FROM_FLOAT (M(md,2,3));

  M(m,3,0) = COGL_FIXED_FROM_FLOAT (M(md,3,0));
  M(m,3,1) = COGL_FIXED_FROM_FLOAT (M(md,3,1));
  M(m,3,2) = COGL_FIXED_FROM_FLOAT (M(md,3,2));
  M(m,3,3) = COGL_FIXED_FROM_FLOAT (M(md,3,3));
#undef M
}

void
cogl_get_projection_matrix (CoglFixed m[16])
{
  GLdouble md[16];

  glGetDoublev(GL_PROJECTION_MATRIX, &md[0]);

#define M(m,row,col)  m[col*4+row]
  M(m,0,0) = COGL_FIXED_FROM_FLOAT (M(md,0,0));
  M(m,0,1) = COGL_FIXED_FROM_FLOAT (M(md,0,1));
  M(m,0,2) = COGL_FIXED_FROM_FLOAT (M(md,0,2));
  M(m,0,3) = COGL_FIXED_FROM_FLOAT (M(md,0,3));

  M(m,1,0) = COGL_FIXED_FROM_FLOAT (M(md,1,0));
  M(m,1,1) = COGL_FIXED_FROM_FLOAT (M(md,1,1));
  M(m,1,2) = COGL_FIXED_FROM_FLOAT (M(md,1,2));
  M(m,1,3) = COGL_FIXED_FROM_FLOAT (M(md,1,3));

  M(m,2,0) = COGL_FIXED_FROM_FLOAT (M(md,2,0));
  M(m,2,1) = COGL_FIXED_FROM_FLOAT (M(md,2,1));
  M(m,2,2) = COGL_FIXED_FROM_FLOAT (M(md,2,2));
  M(m,2,3) = COGL_FIXED_FROM_FLOAT (M(md,2,3));

  M(m,3,0) = COGL_FIXED_FROM_FLOAT (M(md,3,0));
  M(m,3,1) = COGL_FIXED_FROM_FLOAT (M(md,3,1));
  M(m,3,2) = COGL_FIXED_FROM_FLOAT (M(md,3,2));
  M(m,3,3) = COGL_FIXED_FROM_FLOAT (M(md,3,3));
#undef M
}

void
cogl_get_viewport (CoglFixed v[4])
{
  GLdouble vd[4];
  glGetDoublev(GL_VIEWPORT, &vd[0]);

  v[0] = COGL_FIXED_FROM_FLOAT (vd[0]);
  v[1] = COGL_FIXED_FROM_FLOAT (vd[1]);
  v[2] = COGL_FIXED_FROM_FLOAT (vd[2]);
  v[3] = COGL_FIXED_FROM_FLOAT (vd[3]);
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
cogl_fog_set (const CoglColor *fog_color,
              CoglFixed        density,
              CoglFixed        start,
              CoglFixed        stop)
{
  GLfloat fogColor[4];

  fogColor[0] = cogl_color_get_red_float (fog_color);
  fogColor[1] = cogl_color_get_green_float (fog_color);
  fogColor[2] = cogl_color_get_blue_float (fog_color);
  fogColor[3] = cogl_color_get_alpha_float (fog_color);

  glEnable (GL_FOG);

  glFogfv (GL_FOG_COLOR, fogColor);

  glFogi (GL_FOG_MODE, GL_LINEAR);
  glHint (GL_FOG_HINT, GL_NICEST);

  glFogf (GL_FOG_DENSITY, COGL_FIXED_TO_FLOAT (density));
  glFogf (GL_FOG_START, COGL_FIXED_TO_FLOAT (start));
  glFogf (GL_FOG_END, COGL_FIXED_TO_FLOAT (stop));
}

