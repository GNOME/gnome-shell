/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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
 *
 * Authors:
 *   Havoc Pennington <hp@pobox.com> for litl
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-context.h"
#include "cogl-internal.h"
#include "cogl-current-matrix.h"
#include "cogl-matrix-stack.h"

#ifdef HAVE_COGL_GLES2
#include "cogl-gles2-wrapper.h"

#define glFrustum(L,R,B,T,N,F) \
            cogl_wrap_glFrustumf((GLfloat)L, (GLfloat)R, (GLfloat)B, \
                                 (GLfloat)T, (GLfloat)N, (GLfloat)F)
#elif defined (HAVE_COGL_GLES)

#define glFrustum(L,R,B,T,N,F) \
            glFrustumf((GLfloat)L, (GLfloat)R, (GLfloat)B, \
                       (GLfloat)T, (GLfloat)N, (GLfloat)F)

#define glOrtho glOrthof

#endif

#include <string.h>
#include <math.h>

void
_cogl_set_current_matrix (CoglMatrixMode mode)
{
  GLenum gl_mode;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (mode == ctx->matrix_mode)
    return;
  ctx->matrix_mode = mode;

  gl_mode = 0; /* silence compiler warning */
  switch (mode)
    {
    case COGL_MATRIX_MODELVIEW:
      gl_mode = GL_MODELVIEW;
      break;
    case COGL_MATRIX_PROJECTION:
      gl_mode = GL_PROJECTION;
      break;
    case COGL_MATRIX_TEXTURE:
      gl_mode = GL_TEXTURE;
      break;
    }

  GE (glMatrixMode (gl_mode));
}

static void
_cogl_get_client_stack (CoglContext      *ctx,
                        CoglMatrixStack **current_stack_p)
{
  if (ctx->modelview_stack &&
      ctx->matrix_mode == COGL_MATRIX_MODELVIEW)
    *current_stack_p  = ctx->modelview_stack;
  else
    *current_stack_p = NULL;
}

#define _COGL_GET_CONTEXT_AND_STACK(contextvar, stackvar, rval) \
  CoglMatrixStack *stackvar;                                    \
  _COGL_GET_CONTEXT (contextvar, rval);                         \
  _cogl_get_client_stack (contextvar, &stackvar)

void
_cogl_current_matrix_push (void)
{
  _COGL_GET_CONTEXT_AND_STACK (ctx, current_stack, NO_RETVAL);

  if (current_stack != NULL)
    _cogl_matrix_stack_push (current_stack);
  else
    GE (glPushMatrix ());
}

void
_cogl_current_matrix_pop (void)
{
  _COGL_GET_CONTEXT_AND_STACK (ctx, current_stack, NO_RETVAL);

  if (current_stack != NULL)
    _cogl_matrix_stack_pop (current_stack);
  else
    GE (glPopMatrix ());
}

void
_cogl_current_matrix_identity (void)
{
  _COGL_GET_CONTEXT_AND_STACK (ctx, current_stack, NO_RETVAL);

  if (current_stack != NULL)
    _cogl_matrix_stack_load_identity (current_stack);
  else
    GE (glLoadIdentity ());
}

void
_cogl_current_matrix_load (const CoglMatrix *matrix)
{
  _COGL_GET_CONTEXT_AND_STACK (ctx, current_stack, NO_RETVAL);

  if (current_stack != NULL)
    _cogl_matrix_stack_set (current_stack, matrix);
  else
    GE (glLoadMatrixf (cogl_matrix_get_array (matrix)));
}

void
_cogl_current_matrix_multiply (const CoglMatrix *matrix)
{
  _COGL_GET_CONTEXT_AND_STACK (ctx, current_stack, NO_RETVAL);

  if (current_stack != NULL)
    _cogl_matrix_stack_multiply (current_stack, matrix);
  else
    GE (glMultMatrixf (cogl_matrix_get_array (matrix)));
}

void
_cogl_current_matrix_rotate (float angle,
                             float x,
                             float y,
                             float z)
{
  _COGL_GET_CONTEXT_AND_STACK (ctx, current_stack, NO_RETVAL);

  if (current_stack != NULL)
    _cogl_matrix_stack_rotate (current_stack, angle, x, y, z);
  else
    GE (glRotatef (angle, x, y, z));
}

void
_cogl_current_matrix_scale (float x,
                            float y,
                            float z)
{
  _COGL_GET_CONTEXT_AND_STACK (ctx, current_stack, NO_RETVAL);

  if (current_stack != NULL)
    _cogl_matrix_stack_scale (current_stack, x, y, z);
  else
    GE (glScalef (x, y, z));
}

void
_cogl_current_matrix_translate (float x,
                                float y,
                                float z)
{
  _COGL_GET_CONTEXT_AND_STACK (ctx, current_stack, NO_RETVAL);

  if (current_stack != NULL)
    _cogl_matrix_stack_translate (current_stack, x, y, z);
  else
    GE (glTranslatef (x, y, z));
}

void
_cogl_current_matrix_frustum (float left,
                              float right,
                              float bottom,
                              float top,
                              float near_val,
                              float far_val)
{
  _COGL_GET_CONTEXT_AND_STACK (ctx, current_stack, NO_RETVAL);

  if (current_stack != NULL)
    _cogl_matrix_stack_frustum (current_stack,
                                left, right,
                                top, bottom,
                                near_val,
                                far_val);
  else
    GE (glFrustum (left, right, bottom, top, near_val, far_val));
}

void
_cogl_current_matrix_perspective (float fov_y,
                                  float aspect,
                                  float z_near,
                                  float z_far)
{
  _COGL_GET_CONTEXT_AND_STACK (ctx, current_stack, NO_RETVAL);

  if (current_stack != NULL)
    _cogl_matrix_stack_perspective (current_stack,
                                    fov_y, aspect, z_near, z_far);
  else
    {
      /* NB: There is no glPerspective() (only gluPerspective()) so we use
       * cogl_matrix_perspective: */
      CoglMatrix matrix;
      _cogl_get_matrix (ctx->matrix_mode, &matrix);
      cogl_matrix_perspective (&matrix,
                               fov_y, aspect, z_near, z_far);
      _cogl_current_matrix_load (&matrix);
    }
}

void
_cogl_current_matrix_ortho (float left,
                            float right,
                            float bottom,
                            float top,
                            float near_val,
                            float far_val)
{
  _COGL_GET_CONTEXT_AND_STACK (ctx, current_stack, NO_RETVAL);

  if (current_stack != NULL)
    _cogl_matrix_stack_ortho (current_stack,
                              left, right,
                              bottom, top,
                              near_val,
                              far_val);
  else
    {
#ifdef HAVE_COGL_GLES2
      /* NB: GLES 2 has no glOrtho(): */
      CoglMatrix matrix;
      _cogl_get_matrix (ctx->matrix_mode, &matrix);
      cogl_matrix_ortho (&matrix,
                         left, right, bottom, top, near_val, far_val);
      _cogl_current_matrix_load (&matrix);
#else
      GE (glOrtho (left, right, bottom, top, near_val, far_val));
#endif
    }
}

void
_cogl_get_matrix (CoglMatrixMode mode,
                  CoglMatrix    *matrix)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->modelview_stack != NULL &&
      mode == COGL_MATRIX_MODELVIEW)
    {
      _cogl_matrix_stack_get (ctx->modelview_stack, matrix);
    }
  else
    {
      GLenum gl_mode;
      GLfloat gl_matrix[16];

      gl_mode = 0; /* silence compiler warning */
      switch (mode)
        {
        case COGL_MATRIX_MODELVIEW:
          gl_mode = GL_MODELVIEW_MATRIX;
          break;
        case COGL_MATRIX_PROJECTION:
          gl_mode = GL_PROJECTION_MATRIX;
          break;
        case COGL_MATRIX_TEXTURE:
          gl_mode = GL_TEXTURE_MATRIX;
          break;
        }

      /* Note: we have a redundant copy happening here. If that turns out to be
       * a problem then, since this is internal to Cogl, we could pass the
       * CoglMatrix pointer directly to glGetFloatv; the only problem with that
       * is that if we later add internal flags to CoglMatrix they will need to
       * be initialized seperatly.
       */
      GE (glGetFloatv (gl_mode, gl_matrix));
      cogl_matrix_init_from_array (matrix, gl_matrix);
    }
}

void
_cogl_set_matrix (const CoglMatrix *matrix)
{
  _cogl_current_matrix_load (matrix);
}

void
_cogl_current_matrix_state_init (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  ctx->matrix_mode = COGL_MATRIX_MODELVIEW;
  ctx->modelview_stack = NULL;

  if (ctx->indirect ||
      cogl_debug_flags & COGL_DEBUG_FORCE_CLIENT_SIDE_MATRICES)
    {
      ctx->modelview_stack =
        _cogl_matrix_stack_new ();
    }
}

void
_cogl_current_matrix_state_destroy (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->modelview_stack)
    _cogl_matrix_stack_destroy (ctx->modelview_stack);
}

void
_cogl_current_matrix_state_flush (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->matrix_mode != COGL_MATRIX_MODELVIEW)
    {
      g_warning ("matrix state must be flushed in MODELVIEW mode");
      return;
    }

  if (ctx->modelview_stack)
    {
      _cogl_matrix_stack_flush_to_gl (ctx->modelview_stack,
                                      GL_MODELVIEW);
    }
}

void
_cogl_current_matrix_state_dirty (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->matrix_mode != COGL_MATRIX_MODELVIEW)
    {
      g_warning ("matrix state must be dirtied in MODELVIEW mode");
      return;
    }

  if (ctx->modelview_stack)
    _cogl_matrix_stack_dirty (ctx->modelview_stack);
}

void
cogl_push_matrix (void)
{
  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
  _cogl_current_matrix_push ();
}

void
cogl_pop_matrix (void)
{
  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
  _cogl_current_matrix_pop ();
}

void
cogl_scale (float x, float y, float z)
{
  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
  _cogl_current_matrix_scale (x, y, z);
}

void
cogl_translate (float x, float y, float z)
{
  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
  _cogl_current_matrix_translate (x, y, z);
}

void
cogl_rotate (float angle, float x, float y, float z)
{
  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
  _cogl_current_matrix_rotate (angle, x, y, z);
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
  float c, d;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
  _cogl_current_matrix_identity ();

  _cogl_current_matrix_frustum (left,
                                right,
                                bottom,
                                top,
                                z_near,
                                z_far);

  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);

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

  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
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
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_matrix_init_identity (&ortho);
  cogl_matrix_ortho (&ortho, left, right, bottom, top, z_near, z_far);
  _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
  _cogl_current_matrix_load (&ortho);

  /* Calculate and store the inverse of the matrix */
  memset (ctx->inverse_projection, 0, sizeof (float) * 16);

#define M(row,col)  ctx->inverse_projection[col*4+row]
  M(0,0) =  1.0 / ortho.xx;
  M(0,3) =  -ortho.xw;
  M(1,1) =  1.0 / ortho.yy;
  M(1,3) =  -ortho.yw;
  M(2,2) =  1.0 / ortho.zz;
  M(2,3) =  -ortho.zw;
  M(3,3) =  1.0;
#undef M
}

void
cogl_get_modelview_matrix (CoglMatrix *matrix)
{
  _cogl_get_matrix (COGL_MATRIX_MODELVIEW,
                    matrix);
}

void
cogl_set_modelview_matrix (CoglMatrix *matrix)
{
  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
  _cogl_current_matrix_load (matrix);
}

void
cogl_get_projection_matrix (CoglMatrix *matrix)
{
  _cogl_get_matrix (COGL_MATRIX_PROJECTION,
                    matrix);
}

void
cogl_set_projection_matrix (CoglMatrix *matrix)
{
  _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
  _cogl_current_matrix_load (matrix);
}

