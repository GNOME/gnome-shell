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

#include <GLES/gl.h>
#include <string.h>

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define PIXEL_TYPE GL_UNSIGNED_BYTE
#else
#define PIXEL_TYPE GL_UNSIGNED_INT_8_8_8_8_REV
#endif

static gulong __enable_flags = 0;

#define COGL_DEBUG 0

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
#define GE(x...) {                                               \
        GLenum err;                                              \
        (x);                                                     \
        fprintf(stderr, "%s\n", #x);                             \
        while ((err = glGetError()) != GL_NO_ERROR) {            \
                fprintf(stderr, "glError: %s caught at %s:%u\n", \
                                (char *)error_string(err),       \
			         __FILE__, __LINE__);            \
        }                                                        \
}
#else
#define GE(x) (x);
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
cogl_paint_init (const ClutterColor *color)
{
#if COGL_DEBUG
  fprintf(stderr, "\n ============== Paint Start ================ \n");
#endif

  glClearColorx ((color->red << 16) / 0xff, 
		 (color->green << 16) / 0xff,
		 (color->blue << 16) / 0xff, 
		 0xff);

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glDisable (GL_LIGHTING); 
  glDisable (GL_DEPTH_TEST);

  cogl_enable (CGL_ENABLE_BLEND);

  glTexEnvx (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/* FIXME: inline most of these  */
void
cogl_push_matrix (void)
{
  GE( glPushMatrix() );
}

void
cogl_pop_matrix (void)
{
  GE( glPopMatrix() );
}

void
cogl_scale (ClutterFixed x, ClutterFixed y)
{
  GE( glScalex (x, y, CFX_ONE) );
}

void
cogl_translatex (ClutterFixed x, ClutterFixed y, ClutterFixed z)
{
  GE( glTranslatex (x, y, z) );
}

void
cogl_translate (gint x, gint y, gint z)
{
  GE( glTranslatex (CLUTTER_INT_TO_FIXED(x), 
		    CLUTTER_INT_TO_FIXED(y), 
		    CLUTTER_INT_TO_FIXED(z)) );
}

void
cogl_rotatex (ClutterFixed angle, 
	      ClutterFixed x, 
	      ClutterFixed y, 
	      ClutterFixed z)
{
  GE( glRotatex (angle,x,y,z) );
}

void
cogl_rotate (gint angle, gint x, gint y, gint z)
{
  GE( glRotatex (CLUTTER_INT_TO_FIXED(angle),
		 CLUTTER_INT_TO_FIXED(x), 
		 CLUTTER_INT_TO_FIXED(y), 
		 CLUTTER_INT_TO_FIXED(z)) );
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
	  GE( glEnable (GL_BLEND) );
	  GE( glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) );
	}
      __enable_flags |= CGL_ENABLE_BLEND;
    }
  else if (__enable_flags & CGL_ENABLE_BLEND)
    {
      GE( glDisable (GL_BLEND) );
      __enable_flags &= ~CGL_ENABLE_BLEND;
    }

  if (flags & CGL_ENABLE_TEXTURE_2D)
    {
      if (!(__enable_flags & CGL_ENABLE_TEXTURE_2D))
	GE( glEnable (GL_TEXTURE_2D) );
      __enable_flags |= CGL_ENABLE_TEXTURE_2D;
    }
  else if (__enable_flags & CGL_ENABLE_TEXTURE_2D)
    {
      GE( glDisable (GL_TEXTURE_2D) );
      __enable_flags &= ~CGL_ENABLE_TEXTURE_2D;
    }

#if 0
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
#endif

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
  GE( glColor4x ((color->red << 16) / 0xff, 
		 (color->green << 16) / 0xff,
		 (color->blue << 16) / 0xff, 
		 (color->alpha << 16) / 0xff) );  
}

void
cogl_clip_set (const ClutterGeometry *clip)
{
  GE( glEnable (GL_STENCIL_TEST) );

  GE( glClearStencil (0) );
  GE( glClear (GL_STENCIL_BUFFER_BIT) );

  GE( glStencilFunc (GL_NEVER, 0x1, 0x1) );
  GE( glStencilOp (GL_INCR, GL_INCR, GL_INCR) );

  GE( glColor4x (CFX_ONE, CFX_ONE, CFX_ONE, CFX_ONE ) );

  cogl_rectangle (clip->x, clip->y, clip->width, clip->height);
  
  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );
}

void
cogl_clip_unset (void)
{
  GE( glDisable (GL_STENCIL_TEST) );
}



gboolean
cogl_texture_can_size (COGLenum pixel_format,
		       COGLenum pixel_type,
		       int    width, 
		       int    height)
{
  /* FIXME */
  return TRUE;

#if 0
  GLint new_width = 0;

  GE( glTexImage2D (GL_PROXY_TEXTURE_2D, 0, GL_RGBA,
		    width, height, 0 /* border */,
		    pixel_format, pixel_type, NULL) );

  GE( glGetTexLevelParameteriv (GL_PROXY_TEXTURE_2D, 0,
				GL_TEXTURE_WIDTH, &new_width) );

  return new_width != 0;
#endif
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
#define FIX CLUTTER_INT_TO_FIXED

  GLfixed quadVerts[] = {
    FIX(x1), FIX(y1), 0,
    FIX(x2), FIX(y1), 0,
    FIX(x2), FIX(y2), 0,
    FIX(x2), FIX(y2), 0,
    FIX(x1), FIX(y2), 0,
    FIX(x1), FIX(y1), 0
  };

  GLfixed quadTex[] = {
    tx1, ty1,
    tx2, ty1,
    tx2, ty2,
    tx2, ty2,
    tx1, ty2,
    tx1, ty1
  };

#undef FIX

  GE( glEnableClientState(GL_VERTEX_ARRAY) );
  GE( glEnableClientState(GL_TEXTURE_COORD_ARRAY) );
  GE( glVertexPointer(3, GL_FIXED, 0, quadVerts) );
  GE( glTexCoordPointer(2, GL_FIXED, 0, quadTex) );
  GE( glDrawArrays(GL_TRIANGLES, 0, 6) );
  GE( glDisableClientState(GL_TEXTURE_COORD_ARRAY) );
  GE( glDisableClientState(GL_VERTEX_ARRAY) );
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
  /* GE( glPixelStorei (GL_UNPACK_ROW_LENGTH, row_length) ); */
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
#define FIX CLUTTER_INT_TO_FIXED

  GLfixed rect_verts[] = {
    FIX(x), FIX(y), 
    FIX((x + width)), FIX(y), 
    FIX(x), FIX((y + height)), 
    FIX((x + width)), FIX((y + height)), 
  };

#undef FIX

  GE( glEnableClientState(GL_VERTEX_ARRAY) );
  GE( glVertexPointer(2, GL_FIXED, 0, rect_verts) );
  GE( glDrawArrays(GL_TRIANGLE_STRIP, 0, 4) );
  GE( glDisableClientState(GL_VERTEX_ARRAY) );
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
  /* FIXME */
}


void
cogl_alpha_func (COGLenum     func, 
		 ClutterFixed ref)
{
  GE( glAlphaFunc (func, CLUTTER_FIXED_TO_FLOAT(ref)) );
}

/*
 * Fixed point implementation of the perspective function
 */
void
cogl_perspective (ClutterAngle fovy,
		  ClutterFixed aspect,
		  ClutterFixed zNear,
		  ClutterFixed zFar)
{
  ClutterFixed xmax, ymax;
  ClutterFixed x, y, c, d;

  GLfixed m[16];
  
  memset (&m[0], 0, sizeof (m));

  /*
   * Based on the original algorithm in perspective():
   * 
   * 1) xmin = -xmax => xmax + xmin == 0 && xmax - xmin == 2 * xmax
   * same true for y, hence: a == 0 && b == 0;
   *
   * 2) When working with small numbers, we can are loosing significant
   * precision, hence we use clutter_qmulx() here, not the fast macro.
   */
  ymax = clutter_qmulx (zNear, clutter_tani (fovy >> 1));
  xmax = clutter_qmulx (ymax, aspect);

  x = CFX_DIV (zNear, xmax);
  y = CFX_DIV (zNear, ymax);
  c = CFX_DIV (-(zFar + zNear), ( zFar - zNear));
  d = CFX_DIV (-(clutter_qmulx (2*zFar, zNear)), (zFar - zNear));

#define M(row,col)  m[col*4+row]
  M(0,0) = x;
  M(1,1) = y;
  M(2,2) = c;
  M(2,3) = d;
  M(3,2) = 1 + ~CFX_ONE;

  GE( glMultMatrixx (m) );
#undef M
}

void
cogl_setup_viewport (guint         w,
		     guint         h,
		     ClutterAngle fovy,
		     ClutterFixed aspect,
		     ClutterFixed z_near,
		     ClutterFixed z_far)
{
  gint width = (gint) w;
  gint height = (gint) h;
  ClutterFixed z_camera;
  
  GE( glViewport (0, 0, width, height) );
  GE( glMatrixMode (GL_PROJECTION) );
  GE( glLoadIdentity () );

  /* For Ortho projection.
   * glOrthox (0, width << 16, 0,  height << 16,  -1 << 16, 1 << 16);
  */

  cogl_perspective (fovy, aspect, z_near, z_far);
  
  GE( glMatrixMode (GL_MODELVIEW) );
  GE( glLoadIdentity () );

  /* camera distance from screen, 0.5 * tan (FOV) */
#define DEFAULT_Z_CAMERA 0.866025404f
  z_camera = clutter_tani (fovy) << 1;

  /*  
  printf("%i vs %i\n", 
	 CLUTTER_FLOAT_TO_FIXED(DEFAULT_Z_CAMERA),
	 clutter_tani (fovy) << 1);
  */

  GE( glTranslatex (-1 << 15, -1 << 15, /*-z_camera*/
		    -CLUTTER_FLOAT_TO_FIXED(DEFAULT_Z_CAMERA)));

  GE( glScalex ( CFX_ONE / width, 
		 -CFX_ONE / height,
		 CFX_ONE / width));

  GE( glTranslatex (0, -CFX_ONE * height, 0) );
}

ClutterFeatureFlags
cogl_get_features ()
{
  /* Suck */
  return 0;
}
