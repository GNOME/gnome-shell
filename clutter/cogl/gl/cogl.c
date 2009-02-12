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
cogl_scale (float x, float y, float z)
{
  GE( glScalef (x, y, z) );
}

void
cogl_translate (float x, float y, float z)
{
  GE( glTranslatef (x, y, z) );
}

void
cogl_rotate (float angle, float x, float y, float z)
{
  GE( glRotatef (angle, x, y, z) );
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
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Calculate the angle between the axes and the line crossing the
     two points */
  angle = atan2f (vertex_b[1] - vertex_a[1],
                  vertex_b[0] - vertex_a[0]) * (180.0/G_PI);

  GE( glPushMatrix () );
  /* Load the identity matrix and multiply by the reverse of the
     projection matrix so we can specify the plane in screen
     coordinates */
  GE( glLoadIdentity () );
  GE( glMultMatrixf ((GLfloat *) ctx->inverse_projection) );
  /* Rotate about point a */
  GE( glTranslatef (vertex_a[0], vertex_a[1], vertex_a[2]) );
  /* Rotate the plane by the calculated angle so that it will connect
     the two points */
  GE( glRotatef (angle, 0.0f, 0.0f, 1.0f) );
  GE( glTranslatef (-vertex_a[0], -vertex_a[1], -vertex_a[2]) );

  plane[0] = 0;
  plane[1] = -1.0;
  plane[2] = 0;
  plane[3] = vertex_a[1];
#if defined (HAVE_COGL_GLES2) || defined (HAVE_COGL_GLES)
  GE( glClipPlanef (plane_num, plane) );
#else
  GE( glClipPlane (plane_num, plane) );
#endif

  GE( glPopMatrix () );
}

void
_cogl_set_clip_planes (float x_offset,
		       float y_offset,
		       float width,
		       float height)
{
  GLfloat modelview[16], projection[16];

  float vertex_tl[4] = { x_offset, y_offset, 0, 1.0 };
  float vertex_tr[4] = { x_offset + width, y_offset, 0, 1.0 };
  float vertex_bl[4] = { x_offset, y_offset + height, 0, 1.0 };
  float vertex_br[4] = { x_offset + width, y_offset + height,
                        0, 1.0 };

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
      GE( glPushMatrix () );
      GE( glLoadIdentity () );
      GE( glMatrixMode (GL_PROJECTION) );
      GE( glPushMatrix () );
      GE( glLoadIdentity () );
      cogl_rectangle (-1.0, -1.0, 1.0, 1.0);
      GE( glPopMatrix () );
      GE( glMatrixMode (GL_MODELVIEW) );
      GE( glPopMatrix () );
    }

  /* Restore the stencil mode */
  GE( glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );
}

void
_cogl_set_matrix (const float *matrix)
{
  GE( glLoadIdentity () );
  GE( glMultMatrixf (matrix) );
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
cogl_perspective (float fovy,
		  float aspect,
		  float zNear,
		  float zFar)
{
  float xmax, ymax;
  float x, y, c, d;
  float fovy_rad_half = (fovy * G_PI) / 360;

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
  ymax = (zNear * (sinf (fovy_rad_half) / cosf (fovy_rad_half)));
  xmax = (ymax * aspect);

  x = (zNear / xmax);
  y = (zNear / ymax);
  c = (-(zFar + zNear) / ( zFar - zNear));
  d = (-(2 * zFar) * zNear) / (zFar - zNear);

#define M(row,col)  m[col*4+row]
  M(0,0) = x;
  M(1,1) = y;
  M(2,2) = c;
  M(2,3) = d;
  M(3,2) = -1.0;

  GE( glMultMatrixf (m) );

  GE( glMatrixMode (GL_MODELVIEW) );

  /* Calculate and store the inverse of the matrix */
  memset (ctx->inverse_projection, 0, sizeof (float) * 16);

#define m ctx->inverse_projection
  M(0, 0) = (1.0 / x);
  M(1, 1) = (1.0 / y);
  M(2, 3) = -1.0;
  M(3, 2) = (1.0 / d);
  M(3, 3) = (c / d);
#undef m

#undef M
}

void
cogl_frustum (float        left,
	      float        right,
	      float        bottom,
	      float        top,
	      float        z_near,
	      float        z_far)
{
  float c, d;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( glMatrixMode (GL_PROJECTION) );
  GE( glLoadIdentity () );

  GE( glFrustum ((GLdouble)(left),
		 (GLdouble)(right),
		 (GLdouble)(bottom),
		 (GLdouble)(top),
		 (GLdouble)(z_near),
		 (GLdouble)(z_far)) );

  GE( glMatrixMode (GL_MODELVIEW) );

  /* Calculate and store the inverse of the matrix */
  memset (ctx->inverse_projection, 0, sizeof (float) * 16);

  c = - (z_far + z_near) /  (z_far - z_near);
  d = - (2 * (z_far * z_near)) /  (z_far - z_near);

#define M(row,col)  ctx->inverse_projection[col*4+row]
  M(0,0) =  (right - left) /  (2 * z_near);
  M(0,3) =  (right + left) /  (2 * z_near);
  M(1,1) =  (top - bottom) /  (2 * z_near);
  M(1,3) =  (top + bottom) /  (2 * z_near);
  M(2,3) = -1.0;
  M(3,2) = 1.0 / d;
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
cogl_setup_viewport (guint width,
		     guint height,
		     float fovy,
		     float aspect,
		     float z_near,
		     float z_far)
{
  float z_camera;
  float projection_matrix[16];

  GE( glViewport (0, 0, width, height) );

  /* For Ortho projection.
   * glOrthof (0, width << 16, 0,  height << 16,  -1 << 16, 1 << 16);
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

  cogl_get_projection_matrix (projection_matrix);
  z_camera = 0.5 * projection_matrix[0];

  GE( glLoadIdentity () );

  GE( glTranslatef (-0.5f, -0.5f, -z_camera) );
  GE( glScalef (1.0f / width, -1.0f / height, 1.0f / width) );
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
  GLint             num_stencil_bits = 0;

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

  GE( glGetIntegerv (GL_STENCIL_BITS, &num_stencil_bits) );
  /* We need at least three stencil bits to combine clips */
  if (num_stencil_bits > 2)
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
      ctx->pf_glBufferSubDataARB =
	    (COGL_PFNGLBUFFERSUBDATAARBPROC)
	    cogl_get_proc_address ("glBufferSubDataARB");
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
	  && ctx->pf_glBufferSubDataARB
	  && ctx->pf_glDeleteBuffersARB
	  && ctx->pf_glMapBufferARB
	  && ctx->pf_glUnmapBufferARB)
      flags |= COGL_FEATURE_VBOS;
    }

  /* These should always be available because they are defined in GL
     1.2, but we can't call it directly because under Windows
     functions > 1.1 aren't exported */
  ctx->pf_glDrawRangeElements =
        (COGL_PFNGLDRAWRANGEELEMENTSPROC)
        cogl_get_proc_address ("glDrawRangeElements");
  ctx->pf_glActiveTexture =
        (COGL_PFNGLACTIVETEXTUREPROC)
        cogl_get_proc_address ("glActiveTexture");
  ctx->pf_glClientActiveTexture =
        (COGL_PFNGLCLIENTACTIVETEXTUREPROC)
        cogl_get_proc_address ("glClientActiveTexture");

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
cogl_get_modelview_matrix (float m[16])
{
  glGetFloatv (GL_MODELVIEW_MATRIX, m);
}

void
cogl_get_projection_matrix (float m[16])
{
  glGetFloatv (GL_PROJECTION_MATRIX, m);
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
  GLenum gl_mode;

  fogColor[0] = cogl_color_get_red_float (fog_color);
  fogColor[1] = cogl_color_get_green_float (fog_color);
  fogColor[2] = cogl_color_get_blue_float (fog_color);
  fogColor[3] = cogl_color_get_alpha_float (fog_color);

  glEnable (GL_FOG);

  glFogfv (GL_FOG_COLOR, fogColor);

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

