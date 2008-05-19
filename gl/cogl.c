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
cogl_paint_init (const ClutterColor *color)
{
  GE( glClearColor (((float) color->red / 0xff * 1.0),
		    ((float) color->green / 0xff * 1.0),
		    ((float) color->blue / 0xff * 1.0),
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
  
  cogl_toggle_client_flag (ctx, flags,
			   COGL_ENABLE_VERTEX_ARRAY,
			   GL_VERTEX_ARRAY);
  
  cogl_toggle_client_flag (ctx, flags,
			   COGL_ENABLE_TEXCOORD_ARRAY,
			   GL_TEXTURE_COORD_ARRAY);
  
#ifdef GL_TEXTURE_RECTANGLE_ARB
  cogl_toggle_flag (ctx, flags,
		    COGL_ENABLE_TEXTURE_RECT,
		    GL_TEXTURE_RECTANGLE_ARB);
#endif
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
cogl_color (const ClutterColor *color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  glColor4ub (color->red,
	      color->green,
	      color->blue,
	      color->alpha);
  
  /* Store alpha for proper blending enables */
  ctx->color_alpha = color->alpha;
}

void
cogl_clip_set (ClutterFixed x_offset,
               ClutterFixed y_offset,
               ClutterFixed width,
               ClutterFixed height)
{
  if (cogl_features_available (COGL_FEATURE_FOUR_CLIP_PLANES))
    {
      GLdouble eqn_left[4] = { 1.0, 0, 0,
			       -CLUTTER_FIXED_TO_FLOAT (x_offset) };
      GLdouble eqn_right[4] = { -1.0, 0, 0, 
				CLUTTER_FIXED_TO_FLOAT (x_offset + width) };
      GLdouble eqn_top[4] = { 0, 1.0, 0, -CLUTTER_FIXED_TO_FLOAT (y_offset) };
      GLdouble eqn_bottom[4] = { 0, -1.0, 0, CLUTTER_FIXED_TO_FLOAT
				 (y_offset + height) };

      GE( glClipPlane (GL_CLIP_PLANE0, eqn_left) );
      GE( glClipPlane (GL_CLIP_PLANE1, eqn_right) );
      GE( glClipPlane (GL_CLIP_PLANE2, eqn_top) );
      GE( glClipPlane (GL_CLIP_PLANE3, eqn_bottom) );
      GE( glEnable (GL_CLIP_PLANE0) );
      GE( glEnable (GL_CLIP_PLANE1) );
      GE( glEnable (GL_CLIP_PLANE2) );
      GE( glEnable (GL_CLIP_PLANE3) );     
    }
  else if (cogl_features_available (COGL_FEATURE_STENCIL_BUFFER))
    {
      GE( glEnable (GL_STENCIL_TEST) );

      GE( glClearStencil (0.0f) );
      GE( glClear (GL_STENCIL_BUFFER_BIT) );

      GE( glStencilFunc (GL_NEVER, 0x1, 0x1) );
      GE( glStencilOp (GL_INCR, GL_INCR, GL_INCR) );

      GE( glColor3f (1.0f, 1.0f, 1.0f) );

      GE( glRectf (CLUTTER_FIXED_TO_FLOAT (x_offset),
		   CLUTTER_FIXED_TO_FLOAT (y_offset),
		   CLUTTER_FIXED_TO_FLOAT (x_offset + width),
		   CLUTTER_FIXED_TO_FLOAT (y_offset + height)) );

      GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
      GE( glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );
    }
}

void
cogl_clip_unset (void)
{
  if (cogl_features_available (COGL_FEATURE_FOUR_CLIP_PLANES))
    {
      GE( glDisable (GL_CLIP_PLANE3) );
      GE( glDisable (GL_CLIP_PLANE2) );
      GE( glDisable (GL_CLIP_PLANE1) );
      GE( glDisable (GL_CLIP_PLANE0) );
    }
  else if (cogl_features_available (COGL_FEATURE_STENCIL_BUFFER))
    {
      GE( glDisable (GL_STENCIL_TEST) );
    }
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
  ClutterFixed fovy_rad_half = CLUTTER_FIXED_MUL (fovy, CFX_PI) / 360;

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
  ymax = clutter_qmulx (zNear, CLUTTER_FIXED_DIV (clutter_sinx (fovy_rad_half),
						  clutter_cosx (fovy_rad_half)));
  xmax = clutter_qmulx (ymax, aspect);

  x = CLUTTER_FIXED_DIV (zNear, xmax);
  y = CLUTTER_FIXED_DIV (zNear, ymax);
  c = CLUTTER_FIXED_DIV (-(zFar + zNear), ( zFar - zNear));
  d = CLUTTER_FIXED_DIV (-(clutter_qmulx (2*zFar, zNear)), (zFar - zNear));

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

  GE( glViewport (0, 0, width, height) );

  GE( glMatrixMode (GL_PROJECTION) );
  GE( glLoadIdentity () );

  cogl_perspective (fovy, aspect, z_near, z_far);

  GE( glMatrixMode (GL_MODELVIEW) );
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


  if (fovy != CFX_60)
  {
    ClutterFixed fovy_rad = CFX_MUL (fovy, CFX_PI) / 180;

    z_camera =
      CLUTTER_FIXED_TO_FLOAT (CFX_DIV (clutter_sinx (fovy_rad),
				       clutter_cosx (fovy_rad)) >> 1);
  }

  GE( glTranslatef (-0.5f, -0.5f, -z_camera) );
  GE( glScalef ( 1.0f / width,
 	    -1.0f / height,
		 1.0f / width) );
  GE( glTranslatef (0.0f, -1.0 * height, 0.0f) );
}

static void
_cogl_features_init ()
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  ClutterFeatureFlags flags = 0;
  const gchar        *gl_extensions;
  int                 max_clip_planes = 0;
  int                 stencil_bits = 0;

  flags = COGL_FEATURE_TEXTURE_READ_PIXELS;

  gl_extensions = (const gchar*) glGetString (GL_EXTENSIONS);

#if defined(GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB) && defined(GL_TEXTURE_RECTANGLE_ARB)
  if (cogl_check_extension ("GL_ARB_texture_rectangle", gl_extensions) ||
      cogl_check_extension ("GL_EXT_texture_rectangle", gl_extensions))
    {
      flags |= COGL_FEATURE_TEXTURE_RECTANGLE;
    }
#endif
  
  if (cogl_check_extension ("GL_ARB_texture_non_power_of_two", gl_extensions))
    {
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
	  ctx->pf_glUniform1fARB)
	flags |= COGL_FEATURE_SHADERS_GLSL;
    }
  

  if (cogl_check_extension ("GL_EXT_framebuffer_object", gl_extensions) ||
      cogl_check_extension ("GL_ARB_framebuffer_object", gl_extensions))
    { 
      ctx->pf_glGenRenderbuffersEXT =
	(COGL_PFNGLGENRENDERBUFFERSEXTPROC)
	cogl_get_proc_address ("glGenRenderbuffersEXT");
      
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

  GE( glGetIntegerv (GL_STENCIL_BITS, &stencil_bits) );
  if (stencil_bits > 0)
    flags |= COGL_FEATURE_STENCIL_BUFFER;

  GE( glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
  if (max_clip_planes >= 4)
    flags |= COGL_FEATURE_FOUR_CLIP_PLANES;
  
  /* Cache features */
  ctx->feature_flags = flags;
  ctx->features_cached = TRUE;
}

ClutterFeatureFlags
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
cogl_fog_set (const ClutterColor *fog_color,
              ClutterFixed        density,
              ClutterFixed        start,
              ClutterFixed        stop)
{
  GLfloat fogColor[4];

  fogColor[0] = ((float) fog_color->red   / 0xff * 1.0);
  fogColor[1] = ((float) fog_color->green / 0xff * 1.0);
  fogColor[2] = ((float) fog_color->blue  / 0xff * 1.0);
  fogColor[3] = ((float) fog_color->alpha / 0xff * 1.0);

  glEnable (GL_FOG);

  glFogfv (GL_FOG_COLOR, fogColor);

  glFogi (GL_FOG_MODE, GL_LINEAR);
  glHint (GL_FOG_HINT, GL_NICEST);

  glFogf (GL_FOG_DENSITY, CLUTTER_FIXED_TO_FLOAT (density));
  glFogf (GL_FOG_START, CLUTTER_FIXED_TO_FLOAT (start));
  glFogf (GL_FOG_END, CLUTTER_FIXED_TO_FLOAT (stop));
}
