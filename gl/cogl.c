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

#include "config.h"
#include "cogl.h"

#include <GL/gl.h>
#include <string.h>

static gulong __enable_flags = 0;

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

static const char*
error_string(GLenum errorCode)
{
  int i;
  for (i = 0; Errors[i].String; i++) {
    if (Errors[i].Token == errorCode)
      return Errors[i].String;
  }
  return "unknown";
}
#endif

#if COGL_DEBUG
#define GE(x...) G_STMT_START {                                  \
        GLenum err;                                              \
        (x);                                                     \
        while ((err = glGetError()) != GL_NO_ERROR) {            \
                fprintf(stderr, "glError: %s caught at %s:%u\n", \
                                (char *)error_string(err),       \
			         __FILE__, __LINE__);            \
        }                                                        \
} G_STMT_END
#else
#define GE(x) (x);
#endif

CoglFuncPtr
cogl_get_proc_address (const gchar* name)
{
  /* FIXME: This very likely needs to be handled in the backend */
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
cogl_paint_init (const ClutterColor *color)
{
  GE( glClearColor (((float) color->red / 0xff * 1.0),
		    ((float) color->green / 0xff * 1.0),
		    ((float) color->blue / 0xff * 1.0),
		    0.0) );

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glDisable (GL_LIGHTING); 
  glDisable (GL_DEPTH_TEST);

  cogl_enable (CGL_ENABLE_BLEND);

  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
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
cogl_scale (ClutterFixed x, ClutterFixed y)
{
  glScaled (CLUTTER_FIXED_TO_DOUBLE (x),
	    CLUTTER_FIXED_TO_DOUBLE (y),
	    1.0);
}

void
cogl_translatex (ClutterFixed x, ClutterFixed y, ClutterFixed z)
{
  glTranslated (CLUTTER_FIXED_TO_DOUBLE (x),
		CLUTTER_FIXED_TO_DOUBLE (y),
		CLUTTER_FIXED_TO_DOUBLE (z));
}

void
cogl_translate (gint x, gint y, gint z)
{
  glTranslatef ((float)x, (float)y, (float)z);
}

void
cogl_rotatex (ClutterFixed angle, gint x, gint y, gint z)
{
  glRotated (CLUTTER_FIXED_TO_DOUBLE (angle),
	     CLUTTER_FIXED_TO_DOUBLE (x),
	     CLUTTER_FIXED_TO_DOUBLE (y),
	     CLUTTER_FIXED_TO_DOUBLE (z));
}

void
cogl_rotate (gint angle, gint x, gint y, gint z)
{
  glRotatef ((float)angle, (float)x, (float)y, (float)z);
}

void
cogl_enable (gulong flags)
{
  /* This function essentially caches glEnable state() in the
   * hope of lessening number GL traffic.
  */
  if (flags & CGL_ENABLE_BLEND)
    {
      if (!(__enable_flags & CGL_ENABLE_BLEND))
	{
	  glEnable (GL_BLEND);
	  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
      __enable_flags |= CGL_ENABLE_BLEND;
    }
  else if (__enable_flags & CGL_ENABLE_BLEND)
    {
      glDisable (GL_BLEND);
      __enable_flags &= ~CGL_ENABLE_BLEND;
    }

  if (flags & CGL_ENABLE_TEXTURE_2D)
    {
      if (!(__enable_flags & CGL_ENABLE_TEXTURE_2D))
	glEnable (GL_TEXTURE_2D);
      __enable_flags |= CGL_ENABLE_TEXTURE_2D;
    }
  else if (__enable_flags & CGL_ENABLE_TEXTURE_2D)
    {
      glDisable (GL_TEXTURE_2D);
      __enable_flags &= ~CGL_ENABLE_TEXTURE_2D;
    }

  if (flags & CGL_ENABLE_TEXTURE_RECT)
    {
      if (!(__enable_flags & CGL_ENABLE_TEXTURE_RECT))
	  glEnable (GL_TEXTURE_RECTANGLE_ARB);

      __enable_flags |= CGL_ENABLE_TEXTURE_RECT;
    }
  else if (__enable_flags & CGL_ENABLE_TEXTURE_RECT)
    {
      glDisable (GL_TEXTURE_RECTANGLE_ARB);
      __enable_flags &= ~CGL_ENABLE_TEXTURE_RECT;
    }

  if (flags & CGL_ENABLE_ALPHA_TEST)
    {
      if (!(__enable_flags & CGL_ENABLE_ALPHA_TEST))
	glEnable (GL_ALPHA_TEST);

      __enable_flags |= CGL_ENABLE_ALPHA_TEST;
    }
  else if (__enable_flags & CGL_ENABLE_ALPHA_TEST)
    {
      glDisable (GL_ALPHA_TEST);
      __enable_flags &= ~CGL_ENABLE_ALPHA_TEST;
    }
}

void
cogl_color (const ClutterColor *color)
{
  glColor4ub (color->red, color->green, color->blue, color->alpha);  
}

void
cogl_clip_set (const ClutterGeometry *clip)
{
  GE( glEnable (GL_STENCIL_TEST) );

  GE( glClearStencil (0.0f) );
  GE( glClear (GL_STENCIL_BUFFER_BIT) );

  GE( glStencilFunc (GL_NEVER, 0x1, 0x1) );
  GE( glStencilOp (GL_INCR, GL_INCR, GL_INCR) );

  GE( glColor3f (1.0f, 1.0f, 1.0f) );

  GE( glRecti (clip->x, 
	       clip->y,
	       clip->x + clip->width,
	       clip->y + clip->height) );
  
  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
; GE(  glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );
}

void
cogl_clip_unset (void)
{
  GE( glDisable (GL_STENCIL_TEST) );
}

gboolean
cogl_texture_can_size (COGLenum       target,
		       COGLenum pixel_format,
		       COGLenum pixel_type,
		       int    width, 
		       int    height)
{
  if (target == CGL_TEXTURE_RECTANGLE_ARB)
    {
      gint max_size = 0;

      GE( glGetIntegerv(GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB, &max_size) );

      return (max_size && width <= max_size && height <= max_size);
    }
  else /* Assumes CGL_TEXTURE_2D */
    {
      GLint new_width = 0;

      GE( glTexImage2D (GL_PROXY_TEXTURE_2D, 0, GL_RGBA,
			width, height, 0 /* border */,
			pixel_format, pixel_type, NULL) );

      GE( glGetTexLevelParameteriv (GL_PROXY_TEXTURE_2D, 0,
				    GL_TEXTURE_WIDTH, &new_width) );

      return new_width != 0;
    }
}

void
cogl_texture_quad (gint   x1,
		   gint   x2, 
		   gint   y1, 
		   gint   y2,
		   ClutterFixed tx1,
		   ClutterFixed ty1,
		   ClutterFixed tx2,
		   ClutterFixed ty2)
{
  gdouble txf1, tyf1, txf2, tyf2;

  txf1 = CLUTTER_FIXED_TO_DOUBLE (tx1);
  tyf1 = CLUTTER_FIXED_TO_DOUBLE (ty1);
  txf2 = CLUTTER_FIXED_TO_DOUBLE (tx2);
  tyf2 = CLUTTER_FIXED_TO_DOUBLE (ty2);

  glBegin (GL_QUADS);
  glTexCoord2f (txf2, tyf2); glVertex2i   (x2, y2);
  glTexCoord2f (txf1, tyf2); glVertex2i   (x1, y2);
  glTexCoord2f (txf1, tyf1); glVertex2i   (x1, y1);
  glTexCoord2f (txf2, tyf1); glVertex2i   (x2, y1);
  glEnd ();	
}

void
cogl_textures_create (guint num, guint *textures)
{
  GE( glGenTextures (num, textures) );
}

void
cogl_textures_destroy (guint num, const guint *textures)
{
  GE( glDeleteTextures (num, textures) );
}

void
cogl_texture_bind (COGLenum target, guint texture)
{
  GE( glBindTexture (target, texture) );
}

void
cogl_texture_set_alignment (COGLenum target, 
			    guint    alignment,
			    guint    row_length)
{
  GE( glPixelStorei (GL_UNPACK_ROW_LENGTH, row_length) );
  GE( glPixelStorei (GL_UNPACK_ALIGNMENT, alignment) );
}

void
cogl_texture_set_filters (COGLenum target, 
			  COGLenum min_filter,
			  COGLenum max_filter)
{
  GE( glTexParameteri(target, GL_TEXTURE_MAG_FILTER, max_filter) );
  GE( glTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter) );
}

void
cogl_texture_set_wrap (COGLenum target, 
		       COGLenum wrap_s,
		       COGLenum wrap_t)
{
  GE( glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap_s) );
  GE( glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap_s) );
}

void
cogl_texture_image_2d (COGLenum      target,
		       COGLint       internal_format,
		       gint          width, 
		       gint          height, 
		       COGLenum      format,
		       COGLenum      type,
		       const guchar* pixels)
{
  GE( glTexImage2D (target,
		    0, 		/* No mipmap support as yet */
		    internal_format,
		    width,
		    height,
		    0, 		/* 0 pixel border */
		    format,
		    type,
		    pixels) );
}

void
cogl_texture_sub_image_2d (COGLenum      target,
			   gint          xoff,
			   gint          yoff,
			   gint          width, 
			   gint          height,
			   COGLenum      format, 
			   COGLenum      type,
			   const guchar* pixels)
{
  GE( glTexSubImage2D (target,
		       0,
		       xoff,
		       yoff,
		       width,
		       height,
		       format,
		       type,
		       pixels));
}

void
cogl_rectangle (gint x, gint y, guint width, guint height)
{
  GE( glRecti (x, y, x + width, y + height) );
}

/* FIXME: Should use ClutterReal or Fixed */
void
cogl_trapezoid (gint y1,
		gint x11,
		gint x21,
		gint y2,
		gint x12,
		gint x22)
{
  GE( glBegin (GL_QUADS) );
  GE( glVertex2i (x11, y1) );
  GE( glVertex2i (x21, y1) );
  GE( glVertex2i (x22, y2) );
  GE( glVertex2i (x12, y2) );
  GE( glEnd () );
}

void
cogl_alpha_func (COGLenum     func, 
		 ClutterFixed ref)
{
  GE( glAlphaFunc (func, CLUTTER_FIXED_TO_FLOAT(ref)) );
}

void
cogl_perspective (ClutterFixed fovy,
		  ClutterFixed aspect,
		  ClutterFixed zNear,
		  ClutterFixed zFar)
{
  ClutterFixed xmax, ymax;
  ClutterFixed x, y, c, d;
  ClutterFixed fovy_rad_half = CFX_MUL (fovy, CFX_PI) / 360;

  GLfloat m[16];
  
  memset (&m[0], 0, sizeof (m));

  /*
   * Based on the original algorithm in perspective():
   * 
   * 1) xmin = -xmax => xmax + xmin == 0 && xmax - xmin == 2 * xmax
   * same true for y, hence: a == 0 && b == 0;
   *
   * 2) When working with small numbers, we are loosing significant
   * precision, hence we use clutter_qmulx() here, not the fast macro.
   */
  ymax = clutter_qmulx (zNear, CFX_DIV (clutter_sinx (fovy_rad_half),
					clutter_cosx (fovy_rad_half)));
  xmax = clutter_qmulx (ymax, aspect);

  x = CFX_DIV (zNear, xmax);
  y = CFX_DIV (zNear, ymax);
  c = CFX_DIV (-(zFar + zNear), ( zFar - zNear));
  d = CFX_DIV (-(clutter_qmulx (2*zFar, zNear)), (zFar - zNear));

#define M(row,col)  m[col*4+row]
  M(0,0) = CLUTTER_FIXED_TO_FLOAT (x);
  M(1,1) = CLUTTER_FIXED_TO_FLOAT (y);
  M(2,2) = CLUTTER_FIXED_TO_FLOAT (c);
  M(2,3) = CLUTTER_FIXED_TO_FLOAT (d);
  M(3,2) = -1.0F;

  GE( glMultMatrixf (m) );
#undef M
}

void
cogl_setup_viewport (guint        width,
		     guint        height,
		     ClutterFixed fovy,
		     ClutterFixed aspect,
		     ClutterFixed z_near,
		     ClutterFixed z_far)
{
  GLfloat z_camera;
  ClutterFixed fovy_rad = CFX_MUL (fovy, CFX_PI) / 180;
  
  GE( glViewport (0, 0, width, height) );
  
  GE( glMatrixMode (GL_PROJECTION) );
  GE( glLoadIdentity () );

  cogl_perspective (fovy, aspect, z_near, z_far);
  
  GE( glMatrixMode (GL_MODELVIEW) );
  GE( glLoadIdentity () );

  /* camera distance from screen, 0.5 * tan (FOV) */
#define DEFAULT_Z_CAMERA 0.866025404f
  z_camera = CLUTTER_FIXED_TO_FLOAT (CFX_DIV (clutter_sinx (fovy_rad),
					      clutter_cosx (fovy_rad)) >> 1);
  GE( glTranslatef (-0.5f, -0.5f, -z_camera) );
  GE( glScalef ( 1.0f / width, 
 	    -1.0f / height, 
		 1.0f / width) );
  GE( glTranslatef (0.0f, -1.0 * height, 0.0f) );
}

ClutterFeatureFlags
cogl_get_features ()
{
  ClutterFeatureFlags flags = 0;
  const gchar        *gl_extensions;
          
  flags = CLUTTER_FEATURE_TEXTURE_READ_PIXELS;

  gl_extensions = (const gchar*) glGetString (GL_EXTENSIONS);

  if (cogl_check_extension ("GL_ARB_texture_rectangle", gl_extensions) ||
      cogl_check_extension ("GL_EXT_texture_rectangle", gl_extensions))
    {
      flags |= CLUTTER_FEATURE_TEXTURE_RECTANGLE;
    }

#ifdef GL_YCBCR_MESA
  if (cogl_check_extension ("GL_MESA_ycbcr_texture", gl_extensions))
    {
      flags |= CLUTTER_FEATURE_TEXTURE_YUV;
    }
#endif
  
  return flags;
}

void
cogl_get_modelview_matrix (ClutterFixed m[16])
{
  GLdouble md[16];

  glGetDoublev(GL_MODELVIEW_MATRIX, &md[0]);

#define M(m,row,col)  m[col*4+row]
  M(m,0,0) = CLUTTER_FLOAT_TO_FIXED (M(md,0,0));
  M(m,0,1) = CLUTTER_FLOAT_TO_FIXED (M(md,0,1));
  M(m,0,2) = CLUTTER_FLOAT_TO_FIXED (M(md,0,2));
  M(m,0,3) = CLUTTER_FLOAT_TO_FIXED (M(md,0,3));

  M(m,1,0) = CLUTTER_FLOAT_TO_FIXED (M(md,1,0));
  M(m,1,1) = CLUTTER_FLOAT_TO_FIXED (M(md,1,1));
  M(m,1,2) = CLUTTER_FLOAT_TO_FIXED (M(md,1,2));
  M(m,1,3) = CLUTTER_FLOAT_TO_FIXED (M(md,1,3));
  
  M(m,2,0) = CLUTTER_FLOAT_TO_FIXED (M(md,2,0));
  M(m,2,1) = CLUTTER_FLOAT_TO_FIXED (M(md,2,1));
  M(m,2,2) = CLUTTER_FLOAT_TO_FIXED (M(md,2,2));
  M(m,2,3) = CLUTTER_FLOAT_TO_FIXED (M(md,2,3));

  M(m,3,0) = CLUTTER_FLOAT_TO_FIXED (M(md,3,0));
  M(m,3,1) = CLUTTER_FLOAT_TO_FIXED (M(md,3,1));
  M(m,3,2) = CLUTTER_FLOAT_TO_FIXED (M(md,3,2));
  M(m,3,3) = CLUTTER_FLOAT_TO_FIXED (M(md,3,3));
#undef M
}

void
cogl_get_projection_matrix (ClutterFixed m[16])
{
  GLdouble md[16];
  
  glGetDoublev(GL_PROJECTION_MATRIX, &md[0]);
  
#define M(m,row,col)  m[col*4+row]
  M(m,0,0) = CLUTTER_FLOAT_TO_FIXED (M(md,0,0));
  M(m,0,1) = CLUTTER_FLOAT_TO_FIXED (M(md,0,1));
  M(m,0,2) = CLUTTER_FLOAT_TO_FIXED (M(md,0,2));
  M(m,0,3) = CLUTTER_FLOAT_TO_FIXED (M(md,0,3));

  M(m,1,0) = CLUTTER_FLOAT_TO_FIXED (M(md,1,0));
  M(m,1,1) = CLUTTER_FLOAT_TO_FIXED (M(md,1,1));
  M(m,1,2) = CLUTTER_FLOAT_TO_FIXED (M(md,1,2));
  M(m,1,3) = CLUTTER_FLOAT_TO_FIXED (M(md,1,3));
  
  M(m,2,0) = CLUTTER_FLOAT_TO_FIXED (M(md,2,0));
  M(m,2,1) = CLUTTER_FLOAT_TO_FIXED (M(md,2,1));
  M(m,2,2) = CLUTTER_FLOAT_TO_FIXED (M(md,2,2));
  M(m,2,3) = CLUTTER_FLOAT_TO_FIXED (M(md,2,3));

  M(m,3,0) = CLUTTER_FLOAT_TO_FIXED (M(md,3,0));
  M(m,3,1) = CLUTTER_FLOAT_TO_FIXED (M(md,3,1));
  M(m,3,2) = CLUTTER_FLOAT_TO_FIXED (M(md,3,2));
  M(m,3,3) = CLUTTER_FLOAT_TO_FIXED (M(md,3,3));
#undef M
}

void
cogl_get_viewport (ClutterFixed v[4])
{
  GLdouble vd[4];
  glGetDoublev(GL_VIEWPORT, &vd[0]);

  v[0] = CLUTTER_FLOAT_TO_FIXED (vd[0]);
  v[1] = CLUTTER_FLOAT_TO_FIXED (vd[1]);
  v[2] = CLUTTER_FLOAT_TO_FIXED (vd[2]);
  v[3] = CLUTTER_FLOAT_TO_FIXED (vd[3]);
}
