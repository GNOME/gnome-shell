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
/*
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
}*/

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
  return COGL_INVALID_HANDLE;
}

CoglHandle
cogl_offscreen_new_multisample ()
{
  return COGL_INVALID_HANDLE;
}

CoglHandle
cogl_offscreen_ref (CoglHandle handle)
{
  return COGL_INVALID_HANDLE;
}

void
cogl_offscreen_unref (CoglHandle handle)
{
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
}

void
cogl_offscreen_blit (CoglHandle src_buffer,
		     CoglHandle dst_buffer)
{
}

void
cogl_draw_buffer (CoglBufferTarget target, CoglHandle offscreen)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  switch (target)
    {
    case COGL_OFFSCREEN_BUFFER:
      
      /* Not supported */
      return;
      
    case COGL_WINDOW_BUFFER:
      
      /* Draw to RGB channels */
      GE( glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE) );
      
      break;
      
    case COGL_MASK_BUFFER:
      
      /* Draw only to ALPHA channel */
      GE( glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE) );
      
      break;
    case COGL_WINDOW_BUFFER & COGL_MASK_BUFFER:
      
      /* Draw to all channels */
      GE( glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE) );
      
      break;
    }
  
  /* Store new target */
  ctx->draw_buffer = target;
}
