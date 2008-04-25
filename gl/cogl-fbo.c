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
#include "cogl-texture.h"
#include "cogl-fbo.h"
#include "cogl-context.h"

/* Expecting EXT functions not to be defined - redirect to pointers in context  */
#define glGenRenderbuffersEXT                ctx->pf_glGenRenderbuffersEXT
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

static gint
_cogl_fbo_handle_find (CoglHandle handle)
{
  _COGL_GET_CONTEXT (ctx, -1);
  
  gint i;
  
  if (ctx->fbo_handles == NULL)
    return -1;
  
  for (i=0; i < ctx->fbo_handles->len; ++i)
    if (g_array_index (ctx->fbo_handles, CoglHandle, i) == handle)
      return i;
  
  return -1;
}

static CoglHandle
_cogl_fbo_handle_new (CoglFbo *fbo)
{
  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);
  
  CoglHandle handle = (CoglHandle)fbo;
  
  if (ctx->fbo_handles == NULL)
    ctx->fbo_handles = g_array_new (FALSE, FALSE, sizeof (CoglHandle));
  
  g_array_append_val (ctx->fbo_handles, handle);
  
  return handle;
}

static void
_cogl_fbo_handle_release (CoglHandle handle)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  gint i;
  
  if ( (i = _cogl_fbo_handle_find (handle)) == -1)
    return;
  
  g_array_remove_index_fast (ctx->fbo_handles, i);
}

static CoglFbo*
_cogl_fbo_pointer_from_handle (CoglHandle handle)
{
  return (CoglFbo*) handle;
}

gboolean
cogl_is_offscreen_buffer (CoglHandle handle)
{ 
  if (handle == COGL_INVALID_HANDLE)
    return FALSE;
  
  return _cogl_fbo_handle_find (handle) >= 0;
}

CoglHandle
cogl_offscreen_new_to_texture (CoglHandle texhandle)
{
  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);
  
  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN))
    return COGL_INVALID_HANDLE;
  
  CoglTexture      *tex;
  CoglFbo          *fbo;
  CoglTexSliceSpan *x_span;
  CoglTexSliceSpan *y_span;
  GLuint            tex_gl_handle;
  GLuint            fbo_gl_handle;
  GLenum            status;
  
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
  
  /* Generate framebuffer */
  glGenFramebuffersEXT (1, &fbo_gl_handle);
  GE( glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, fbo_gl_handle) );
  GE( glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
				 tex->gl_target, tex_gl_handle, 0) );
  
  /* Make sure it's complete */
  status = glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT);
  
  if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
    {
      GE( glDeleteFramebuffersEXT (1, &fbo_gl_handle) );
      GE( glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0) );
      return COGL_INVALID_HANDLE;
    }
  
  GE( glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0) );
  
  /* Allocate and init a CoglFbo object (store non-wasted size
     for subsequent blits and viewport setup) */
  fbo = (CoglFbo*) g_malloc (sizeof (CoglFbo));
  fbo->ref_count = 1;
  fbo->width     = x_span->size - x_span->waste;
  fbo->height    = y_span->size - y_span->waste;
  fbo->gl_handle = fbo_gl_handle;
  
  return _cogl_fbo_handle_new (fbo);
}

CoglHandle
cogl_offscreen_new_multisample ()
{
  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN_MULTISAMPLE))
    return COGL_INVALID_HANDLE;
  
  return COGL_INVALID_HANDLE;
}

CoglHandle
cogl_offscreen_ref (CoglHandle handle)
{
  CoglFbo *fbo;

  if (!cogl_is_offscreen_buffer (handle))
    return COGL_INVALID_HANDLE;

  fbo = _cogl_fbo_pointer_from_handle (handle);

  fbo->ref_count++;

  return handle;
}

void
cogl_offscreen_unref (CoglHandle handle)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  CoglFbo *fbo;
  
  /* Make sure this is a valid fbo handle */
  if (!cogl_is_offscreen_buffer (handle))
    return;
  
  fbo = _cogl_fbo_pointer_from_handle (handle);

  if (--fbo->ref_count < 1)
    {  
      /* Release handle and destroy object */
      GE( glDeleteFramebuffersEXT (1, &fbo->gl_handle) );

      _cogl_fbo_handle_release (handle);
      
      g_free (fbo);
    }
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
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN_BLIT))
    return;
  
  CoglFbo *src_fbo;
  CoglFbo *dst_fbo;
  
  /* Make sure these are valid fbo handles */
  if (!cogl_is_offscreen_buffer (src_buffer))
    return;
  
  if (!cogl_is_offscreen_buffer (dst_buffer))
    return;
  
  src_fbo = _cogl_fbo_pointer_from_handle (src_buffer);
  dst_fbo = _cogl_fbo_pointer_from_handle (dst_buffer);
  
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
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN_BLIT))
    return;
  
  CoglFbo *src_fbo;
  CoglFbo *dst_fbo;
  
  /* Make sure these are valid fbo handles */
  if (!cogl_is_offscreen_buffer (src_buffer))
    return;
  
  if (!cogl_is_offscreen_buffer (dst_buffer))
    return;
  
  src_fbo = _cogl_fbo_pointer_from_handle (src_buffer);
  dst_fbo = _cogl_fbo_pointer_from_handle (dst_buffer);
  
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
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  CoglFbo *fbo = NULL;
  
  if (target == COGL_OFFSCREEN_BUFFER)
    {
      /* Make sure it is a valid fbo handle */
      if (!cogl_is_offscreen_buffer (offscreen))
	return;
      
      fbo = _cogl_fbo_pointer_from_handle (offscreen);
      
      /* Check current draw buffer target */
      if (ctx->draw_buffer != COGL_OFFSCREEN_BUFFER)
	{
	  /* Push the viewport and matrix setup if redirecting
             from a non-screen buffer */
	  GE( glPushAttrib (GL_VIEWPORT_BIT) );
	  
	  GE( glMatrixMode (GL_PROJECTION) );
	  GE( glPushMatrix () );
	  GE( glLoadIdentity () );
	  
	  GE( glMatrixMode (GL_MODELVIEW) );
	  GE( glPushMatrix () );
	  GE( glLoadIdentity () );
	}
      else
	{
	  /* Override viewport and matrix setup if redirecting
             from another offscreen buffer */
	  GE( glMatrixMode (GL_PROJECTION) );
	  GE( glLoadIdentity () );
	  
	  GE( glMatrixMode (GL_MODELVIEW) );
	  GE( glLoadIdentity () );
	}
      
      /* Setup new viewport and matrices */
      GE( glViewport (0, 0, fbo->width, fbo->height) );
      GE( glTranslatef (-1.0f, -1.0f, 0.0f) );
      GE( glScalef (2.0f / fbo->width, 2.0f / fbo->height, 1.0f) );
      
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
	  
	  GE( glMatrixMode (GL_PROJECTION) );
	  GE( glPopMatrix () );
	  
	  GE( glMatrixMode (GL_MODELVIEW) );
	  GE( glPopMatrix () );
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
