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
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-fbo.h"
#include "cogl-context.h"
#include "cogl-handle.h"

/* Expecting EXT functions not to be defined - redirect to pointers in context  */
#define glGenRenderbuffersEXT                ctx->pf_glGenRenderbuffersEXT
#define glDeleteRenderbuffersEXT             ctx->pf_glDeleteRenderbuffersEXT
#define glBindRenderbufferEXT                ctx->pf_glBindRenderbufferEXT
#define glRenderbufferStorageEXT             ctx->pf_glRenderbufferStorageEXT
#define glGenFramebuffersEXT                 ctx->pf_glGenFramebuffersEXT
#define glBindFramebufferEXT                 ctx->pf_glBindFramebufferEXT
#define glFramebufferTexture2DEXT            ctx->pf_glFramebufferTexture2DEXT
#define glFramebufferRenderbufferEXT         ctx->pf_glFramebufferRenderbufferEXT
#define glCheckFramebufferStatusEXT          ctx->pf_glCheckFramebufferStatusEXT
#define glDeleteFramebuffersEXT              ctx->pf_glDeleteFramebuffersEXT
#define glBlitFramebufferEXT                 ctx->pf_glBlitFramebufferEXT
#define glRenderbufferStorageMultisampleEXT  ctx->pf_glRenderbufferStorageMultisampleEXT

#ifndef GL_READ_FRAMEBUFFER_EXT
#define GL_READ_FRAMEBUFFER_EXT 0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER_EXT
#define GL_DRAW_FRAMEBUFFER_EXT 0x8CA9
#endif

static void _cogl_offscreen_free (CoglFbo *fbo);

COGL_HANDLE_DEFINE (Fbo, offscreen);

CoglHandle
cogl_offscreen_new_to_texture (CoglHandle texhandle)
{
  CoglTexture      *tex;
  CoglFbo          *fbo;
  CoglTexSliceSpan *x_span;
  CoglTexSliceSpan *y_span;
  GLuint            tex_gl_handle;
  GLuint            fbo_gl_handle;
  GLuint            gl_stencil_handle;
  GLenum            status;
  
  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);
  
  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN))
    return COGL_INVALID_HANDLE;
  
  /* Make texhandle is a valid texture object */
  if (!cogl_is_texture (texhandle))
    return COGL_INVALID_HANDLE;
  
  tex = _cogl_texture_pointer_from_handle (texhandle);
  
  /* The texture must not be sliced */
  if (tex->slice_gl_handles == NULL)
    return COGL_INVALID_HANDLE;
  
  if (tex->slice_gl_handles->len != 1)
    return COGL_INVALID_HANDLE;
  
  /* Pick the single texture slice width, height and GL id */
  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, 0);
  y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, 0);
  tex_gl_handle = g_array_index (tex->slice_gl_handles, GLuint, 0);

  /* Create a renderbuffer for stenciling */
  GE( glGenRenderbuffersEXT (1, &gl_stencil_handle) );
  GE( glBindRenderbufferEXT (GL_RENDERBUFFER_EXT, gl_stencil_handle) );
  GE( glRenderbufferStorageEXT (GL_RENDERBUFFER_EXT, GL_STENCIL_INDEX8_EXT,
				cogl_texture_get_width (texhandle),
				cogl_texture_get_height (texhandle)) );
  GE( glBindRenderbufferEXT (GL_RENDERBUFFER_EXT, 0) );

  /* Generate framebuffer */
  glGenFramebuffersEXT (1, &fbo_gl_handle);
  GE( glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, fbo_gl_handle) );
  GE( glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
				 tex->gl_target, tex_gl_handle, 0) );
  GE( glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT,
				    GL_STENCIL_ATTACHMENT_EXT,
				    GL_RENDERBUFFER_EXT, gl_stencil_handle) );
  
  /* Make sure it's complete */
  status = glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT);
  
  if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
    {
      /* Stencil renderbuffers aren't always supported. Try again
	 without the stencil buffer */
      GE( glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT,
					GL_STENCIL_ATTACHMENT_EXT,
					GL_RENDERBUFFER_EXT,
					0) );
      GE( glDeleteRenderbuffersEXT (1, &gl_stencil_handle) );
      gl_stencil_handle = 0;
      
      status = glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT);
  
      if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
	  /* Still failing, so give up */
	  GE( glDeleteFramebuffersEXT (1, &fbo_gl_handle) );
	  GE( glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0) );
	  return COGL_INVALID_HANDLE;
	}
    }
  
  GE( glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0) );
  
  /* Allocate and init a CoglFbo object (store non-wasted size
     for subsequent blits and viewport setup) */
  fbo = (CoglFbo*) g_malloc (sizeof (CoglFbo));
  fbo->width             = x_span->size - x_span->waste;
  fbo->height            = y_span->size - y_span->waste;
  fbo->gl_handle         = fbo_gl_handle;
  fbo->gl_stencil_handle = gl_stencil_handle;

  return _cogl_offscreen_handle_new (fbo);
}

CoglHandle
cogl_offscreen_new_multisample ()
{
  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN_MULTISAMPLE))
    return COGL_INVALID_HANDLE;
  
  return COGL_INVALID_HANDLE;
}

static void
_cogl_offscreen_free (CoglFbo *fbo)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Frees FBO resources but its handle is not
     released! Do that separately before this! */
  if (fbo->gl_stencil_handle)
    GE( glDeleteRenderbuffersEXT (1, &fbo->gl_stencil_handle) );
  GE( glDeleteFramebuffersEXT (1, &fbo->gl_handle) );
  g_free (fbo);
}

void
cogl_offscreen_blit_region (CoglHandle src_buffer,
			    CoglHandle dst_buffer,
			    int src_x,
			    int src_y,
			    int src_w,
			    int src_h,
			    int dst_x,
			    int dst_y,
			    int dst_w,
			    int dst_h)
{
  CoglFbo *src_fbo;
  CoglFbo *dst_fbo;
  
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN_BLIT))
    return;
  
  /* Make sure these are valid fbo handles */
  if (!cogl_is_offscreen (src_buffer))
    return;
  
  if (!cogl_is_offscreen (dst_buffer))
    return;
  
  src_fbo = _cogl_offscreen_pointer_from_handle (src_buffer);
  dst_fbo = _cogl_offscreen_pointer_from_handle (dst_buffer);
  
  /* Copy (and scale) a region from one to another framebuffer */
  GE( glBindFramebufferEXT (GL_READ_FRAMEBUFFER_EXT, src_fbo->gl_handle) );
  GE( glBindFramebufferEXT (GL_DRAW_FRAMEBUFFER_EXT, dst_fbo->gl_handle) );
  GE( glBlitFramebufferEXT (src_x, src_y, src_x + src_w, src_y + src_h,
			    dst_x, dst_y, dst_x + dst_w, dst_y + dst_h,
			    GL_COLOR_BUFFER_BIT, GL_LINEAR) );
}

void
cogl_offscreen_blit (CoglHandle src_buffer,
		     CoglHandle dst_buffer)
{
  CoglFbo *src_fbo;
  CoglFbo *dst_fbo;
  
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN_BLIT))
    return;
  
  /* Make sure these are valid fbo handles */
  if (!cogl_is_offscreen (src_buffer))
    return;
  
  if (!cogl_is_offscreen (dst_buffer))
    return;
  
  src_fbo = _cogl_offscreen_pointer_from_handle (src_buffer);
  dst_fbo = _cogl_offscreen_pointer_from_handle (dst_buffer);
  
  /* Copy (and scale) whole image from one to another framebuffer */
  GE( glBindFramebufferEXT (GL_READ_FRAMEBUFFER_EXT, src_fbo->gl_handle) );
  GE( glBindFramebufferEXT (GL_DRAW_FRAMEBUFFER_EXT, dst_fbo->gl_handle) );
  GE( glBlitFramebufferEXT (0, 0, src_fbo->width, src_fbo->height,
			    0, 0, dst_fbo->width, dst_fbo->height,
			    GL_COLOR_BUFFER_BIT, GL_LINEAR) );
}

void
cogl_draw_buffer (CoglBufferTarget target, CoglHandle offscreen)
{
  CoglFbo *fbo = NULL;
  
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (target == COGL_OFFSCREEN_BUFFER)
    {
      /* Make sure it is a valid fbo handle */
      if (!cogl_is_offscreen (offscreen))
	return;
      
      fbo = _cogl_offscreen_pointer_from_handle (offscreen);
      
      /* Check current draw buffer target */
      if (ctx->draw_buffer != COGL_OFFSCREEN_BUFFER)
	{
	  /* Push the viewport and matrix setup if redirecting
             from a non-screen buffer */
	  GE( glPushAttrib (GL_VIEWPORT_BIT) );

          _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
          _cogl_current_matrix_push ();
          _cogl_current_matrix_identity ();

          _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
          _cogl_current_matrix_push ();
          _cogl_current_matrix_identity ();
	}
      else
	{
	  /* Override viewport and matrix setup if redirecting
             from another offscreen buffer */
          _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
          _cogl_current_matrix_identity ();

          _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
          _cogl_current_matrix_identity ();
	}
      
      /* Setup new viewport and matrices */
      GE( glViewport (0, 0, fbo->width, fbo->height) );
      _cogl_current_matrix_translate (-1.0f, -1.0f, 0.0f);
      _cogl_current_matrix_scale (2.0f / fbo->width, 2.0f / fbo->height, 1.0f);
      
      /* Bind offscreen framebuffer object */
      GE( glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, fbo->gl_handle) );
      GE( glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE) );
      
      /* Some implementation require a clear before drawing
         to an fbo. Luckily it is affected by scissor test. */
      /* FIXME: test where exactly this is needed end whether
         a glClear with 0 argument is enough */
      GE( glPushAttrib (GL_SCISSOR_BIT) );
      GE( glScissor (0,0,0,0) );
      GE( glEnable (GL_SCISSOR_TEST) );
      GE( glClear (GL_COLOR_BUFFER_BIT) );
      GE( glPopAttrib () );
      
    }
  else if ((target & COGL_WINDOW_BUFFER) ||
	   (target & COGL_MASK_BUFFER))
    {
      /* Check current draw buffer target */
      if (ctx->draw_buffer == COGL_OFFSCREEN_BUFFER)
	{
	  /* Pop viewport and matrices if redirecting back
             from an offscreen buffer */
	  GE( glPopAttrib () );

          _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
          _cogl_current_matrix_pop ();

          _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
          _cogl_current_matrix_pop ();
	}
      
      /* Bind window framebuffer object */
      GE( glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0) );
      
      
      if (target == COGL_WINDOW_BUFFER)
	{
	  /* Draw to RGB channels */
	  GE( glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE) );
	}
      else if (target == COGL_MASK_BUFFER)
	{
	  /* Draw only to ALPHA channel */
	  GE( glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE) );
	}
      else
	{
	  /* Draw to all channels */
	  GE( glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE) );
	}
    }
  
  /* Store new target */
  ctx->draw_buffer = target;
}
