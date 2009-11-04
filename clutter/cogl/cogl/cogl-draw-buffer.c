/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-draw-buffer-private.h"
#include "cogl-clip-stack.h"

#ifdef HAVE_COGL_GLES2

#include "../gles/cogl-gles2-wrapper.h"

#else

#define glGenRenderbuffers                ctx->drv.pf_glGenRenderbuffers
#define glDeleteRenderbuffers             ctx->drv.pf_glDeleteRenderbuffers
#define glBindRenderbuffer                ctx->drv.pf_glBindRenderbuffer
#define glRenderbufferStorage             ctx->drv.pf_glRenderbufferStorage
#define glGenFramebuffers                 ctx->drv.pf_glGenFramebuffers
#define glBindFramebuffer                 ctx->drv.pf_glBindFramebuffer
#define glFramebufferTexture2D            ctx->drv.pf_glFramebufferTexture2D
#define glFramebufferRenderbuffer         ctx->drv.pf_glFramebufferRenderbuffer
#define glCheckFramebufferStatus          ctx->drv.pf_glCheckFramebufferStatus
#define glDeleteFramebuffers              ctx->drv.pf_glDeleteFramebuffers

#endif

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER		0x8D40
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER		0x8D41
#endif
#ifndef GL_STENCIL_ATTACHMENT
#define GL_STENCIL_ATTACHMENT	0x8D00
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0	0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_STENCIL_INDEX8
#define GL_STENCIL_INDEX8       0x8D48
#endif

static void _cogl_draw_buffer_free (CoglDrawBuffer *draw_buffer);
static void _cogl_onscreen_free (CoglOnscreen *onscreen);
static void _cogl_offscreen_free (CoglOffscreen *offscreen);

COGL_HANDLE_DEFINE (Onscreen, onscreen);
COGL_HANDLE_DEFINE (Offscreen, offscreen);

/* XXX:
 * The CoglHandle macros don't support any form of inheritance, so for
 * now we implement the CoglHandle support for the CoglDrawBuffer
 * abstract class manually.
 */

gboolean
cogl_is_draw_buffer (CoglHandle handle)
{
  CoglHandleObject *obj = (CoglHandleObject *)handle;

  if (handle == COGL_INVALID_HANDLE)
    return FALSE;

  return obj->klass->type == _cogl_handle_onscreen_get_type ()
    || obj->klass->type == _cogl_handle_offscreen_get_type ();
}

static void
_cogl_draw_buffer_init (CoglDrawBuffer *draw_buffer,
                        CoglDrawBufferType type,
                        int width,
                        int height)
{
  draw_buffer->type             = type;
  draw_buffer->width            = width;
  draw_buffer->height           = height;
  draw_buffer->viewport_x       = 0;
  draw_buffer->viewport_y       = 0;
  draw_buffer->viewport_width   = width;
  draw_buffer->viewport_height  = height;

  draw_buffer->modelview_stack  = _cogl_matrix_stack_new ();
  draw_buffer->projection_stack = _cogl_matrix_stack_new ();

  /* Initialise the clip stack */
  _cogl_clip_stack_state_init (&draw_buffer->clip_state);
}

void
_cogl_draw_buffer_free (CoglDrawBuffer *draw_buffer)
{
  _cogl_clip_stack_state_destroy (&draw_buffer->clip_state);

  _cogl_matrix_stack_destroy (draw_buffer->modelview_stack);
  draw_buffer->modelview_stack = NULL;

  _cogl_matrix_stack_destroy (draw_buffer->projection_stack);
  draw_buffer->projection_stack = NULL;
}

int
_cogl_draw_buffer_get_width (CoglHandle handle)
{
  CoglDrawBuffer *draw_buffer = COGL_DRAW_BUFFER (handle);
  return draw_buffer->width;
}

int
_cogl_draw_buffer_get_height (CoglHandle handle)
{
  CoglDrawBuffer *draw_buffer = COGL_DRAW_BUFFER (handle);
  return draw_buffer->height;
}

CoglClipStackState *
_cogl_draw_buffer_get_clip_state (CoglHandle handle)
{
  CoglDrawBuffer *draw_buffer = COGL_DRAW_BUFFER (handle);

  return &draw_buffer->clip_state;
}

void
_cogl_draw_buffer_set_viewport (CoglHandle handle,
                                int x,
                                int y,
                                int width,
                                int height)
{
  CoglDrawBuffer *draw_buffer = COGL_DRAW_BUFFER (handle);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (draw_buffer->viewport_x == x &&
      draw_buffer->viewport_y == y &&
      draw_buffer->viewport_width == width &&
      draw_buffer->viewport_height == height)
    return;

  _cogl_journal_flush ();

  draw_buffer->viewport_x = x;
  draw_buffer->viewport_y = y;
  draw_buffer->viewport_width = width;
  draw_buffer->viewport_height = height;

  if (_cogl_get_draw_buffer () == draw_buffer)
    ctx->dirty_gl_viewport = TRUE;
}

int
_cogl_draw_buffer_get_viewport_x (CoglHandle handle)
{
  CoglDrawBuffer *draw_buffer = COGL_DRAW_BUFFER (handle);
  return draw_buffer->viewport_x;
}

int
_cogl_draw_buffer_get_viewport_y (CoglHandle handle)
{
  CoglDrawBuffer *draw_buffer = COGL_DRAW_BUFFER (handle);
  return draw_buffer->viewport_y;
}

int
_cogl_draw_buffer_get_viewport_width (CoglHandle handle)
{
  CoglDrawBuffer *draw_buffer = COGL_DRAW_BUFFER (handle);
  return draw_buffer->viewport_width;
}

int
_cogl_draw_buffer_get_viewport_height (CoglHandle handle)
{
  CoglDrawBuffer *draw_buffer = COGL_DRAW_BUFFER (handle);
  return draw_buffer->viewport_height;
}

void
_cogl_draw_buffer_get_viewport4fv (CoglHandle handle, int *viewport)
{
  CoglDrawBuffer *draw_buffer = COGL_DRAW_BUFFER (handle);
  viewport[0] = draw_buffer->viewport_x;
  viewport[1] = draw_buffer->viewport_y;
  viewport[2] = draw_buffer->viewport_width;
  viewport[3] = draw_buffer->viewport_height;
}

CoglMatrixStack *
_cogl_draw_buffer_get_modelview_stack (CoglHandle handle)
{
  CoglDrawBuffer *draw_buffer = COGL_DRAW_BUFFER (handle);
  return draw_buffer->modelview_stack;
}

CoglMatrixStack *
_cogl_draw_buffer_get_projection_stack (CoglHandle handle)
{
  CoglDrawBuffer *draw_buffer = COGL_DRAW_BUFFER (handle);
  return draw_buffer->projection_stack;
}

CoglHandle
cogl_offscreen_new_to_texture (CoglHandle texhandle)
{
  CoglOffscreen    *offscreen;
  int               width;
  int               height;
  GLuint            tex_gl_handle;
  GLenum            tex_gl_target;
  GLuint            fbo_gl_handle;
  GLuint            gl_stencil_handle;
  GLenum            status;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN))
    return COGL_INVALID_HANDLE;

  /* Make texhandle is a valid texture object */
  if (!cogl_is_texture (texhandle))
    return COGL_INVALID_HANDLE;

  /* The texture must not be sliced */
  if (cogl_texture_is_sliced (texhandle))
    return COGL_INVALID_HANDLE;

  /* Pick the single texture slice width, height and GL id */

  width = cogl_texture_get_width (texhandle);
  height = cogl_texture_get_height (texhandle);

  if (!cogl_texture_get_gl_texture (texhandle, &tex_gl_handle, &tex_gl_target))
    return COGL_INVALID_HANDLE;

  if (tex_gl_target != GL_TEXTURE_2D)
    return COGL_INVALID_HANDLE;

  /* Create a renderbuffer for stenciling */
  GE (glGenRenderbuffers (1, &gl_stencil_handle));
  GE (glBindRenderbuffer (GL_RENDERBUFFER, gl_stencil_handle));
  GE (glRenderbufferStorage (GL_RENDERBUFFER, GL_STENCIL_INDEX8,
			     cogl_texture_get_width (texhandle),
			     cogl_texture_get_height (texhandle)));
  GE (glBindRenderbuffer (GL_RENDERBUFFER, 0));

  /* We are about to generate and bind a new fbo, so when next flushing the
   * journal, we will need to rebind the current draw buffer... */
  ctx->dirty_bound_framebuffer = 1;

  /* Generate framebuffer */
  glGenFramebuffers (1, &fbo_gl_handle);
  GE (glBindFramebuffer (GL_FRAMEBUFFER, fbo_gl_handle));
  GE (glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			      tex_gl_target, tex_gl_handle, 0));
  GE (glFramebufferRenderbuffer (GL_FRAMEBUFFER,
				 GL_STENCIL_ATTACHMENT,
				 GL_RENDERBUFFER, gl_stencil_handle));

  /* XXX: The framebuffer_object spec isn't clear in defining whether attaching
   * a texture as a renderbuffer with mipmap filtering enabled while the
   * mipmaps have not been uploaded should result in an incomplete framebuffer
   * object. (different drivers make different decisions)
   *
   * To avoid an error with drivers that do consider this a problem we
   * explicitly set non mipmapped filters here. These will later be reset when
   * the texture is actually used for rendering according to the filters set on
   * the corresponding CoglMaterial.
   */
  _cogl_texture_set_filters (texhandle, GL_NEAREST, GL_NEAREST);

  /* Make sure it's complete */
  status = glCheckFramebufferStatus (GL_FRAMEBUFFER);

  if (status != GL_FRAMEBUFFER_COMPLETE)
    {
      /* Stencil renderbuffers aren't always supported. Try again
	 without the stencil buffer */
      GE (glFramebufferRenderbuffer (GL_FRAMEBUFFER,
				     GL_STENCIL_ATTACHMENT,
				     GL_RENDERBUFFER,
				     0));
      GE (glDeleteRenderbuffers (1, &gl_stencil_handle));
      gl_stencil_handle = 0;

      status = glCheckFramebufferStatus (GL_FRAMEBUFFER);

      if (status != GL_FRAMEBUFFER_COMPLETE)
	{
	  /* Still failing, so give up */
	  GE (glDeleteFramebuffers (1, &fbo_gl_handle));
	  GE (glBindFramebuffer (GL_FRAMEBUFFER, 0));
	  return COGL_INVALID_HANDLE;
	}
    }

  offscreen                     = g_new0 (CoglOffscreen, 1);

  _cogl_draw_buffer_init (COGL_DRAW_BUFFER (offscreen),
                          COGL_DRAW_BUFFER_TYPE_OFFSCREEN,
                          width,
                          height);

  offscreen->fbo_handle         = fbo_gl_handle;
  offscreen->gl_stencil_handle  = gl_stencil_handle;

  /* XXX: Can we get a away with removing this? It wasn't documented, and most
   * users of the API are hopefully setting up the modelview from scratch
   * anyway */
#if 0
  cogl_matrix_translate (&draw_buffer->modelview, -1.0f, -1.0f, 0.0f);
  cogl_matrix_scale (&draw_buffer->modelview,
                     2.0f / draw_buffer->width, 2.0f / draw_buffer->height, 1.0f);
#endif

  return _cogl_offscreen_handle_new (offscreen);
}

static void
_cogl_offscreen_free (CoglOffscreen *offscreen)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Chain up to parent */
  _cogl_draw_buffer_free (COGL_DRAW_BUFFER (offscreen));

  if (offscreen->gl_stencil_handle)
    GE (glDeleteRenderbuffers (1, &offscreen->gl_stencil_handle));
  GE (glDeleteFramebuffers (1, &offscreen->fbo_handle));
  g_free (offscreen);
}

CoglHandle
_cogl_onscreen_new (void)
{
  CoglOnscreen *onscreen;

  /* XXX: Until we have full winsys support in Cogl then we can't fully
   * implement CoglOnscreen draw buffers, since we can't, e.g. keep track of
   * the window size. */

  onscreen = g_new0 (CoglOnscreen, 1);
  _cogl_draw_buffer_init (COGL_DRAW_BUFFER (onscreen),
                          COGL_DRAW_BUFFER_TYPE_ONSCREEN,
                          0xdeadbeef, /* width */
                          0xdeadbeef); /* height */

  return _cogl_onscreen_handle_new (onscreen);
}

static void
_cogl_onscreen_free (CoglOnscreen *onscreen)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Chain up to parent */
  _cogl_draw_buffer_free (COGL_DRAW_BUFFER (onscreen));

  g_free (onscreen);
}

void
_cogl_onscreen_clutter_backend_set_size (int width, int height)
{
  CoglDrawBuffer *draw_buffer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  draw_buffer = COGL_DRAW_BUFFER (ctx->window_buffer);

  if (draw_buffer->width == width && draw_buffer->height == height)
    return;

  draw_buffer->width = width;
  draw_buffer->height = height;

  /* We'll need to recalculate the GL viewport state derived
   * from the Cogl viewport */
  ctx->dirty_gl_viewport = 1;
}

GSList *
_cogl_create_draw_buffer_stack (void)
{
  GSList *stack = NULL;
  CoglDrawBufferStackEntry *entry;

  entry = g_slice_new0 (CoglDrawBufferStackEntry);
  entry->target = COGL_WINDOW_BUFFER;
  entry->draw_buffer = COGL_INVALID_HANDLE;

  return g_slist_prepend (stack, entry);
}

void
_cogl_free_draw_buffer_stack (GSList *stack)
{
  GSList *l;

  for (l = stack; l != NULL; l = l->next)
    {
      CoglDrawBufferStackEntry *entry = l->data;
      CoglDrawBuffer *draw_buffer = COGL_DRAW_BUFFER (entry->draw_buffer);
      if (draw_buffer->type == COGL_DRAW_BUFFER_TYPE_OFFSCREEN)
        _cogl_offscreen_free (COGL_OFFSCREEN (draw_buffer));
      else
        _cogl_onscreen_free (COGL_ONSCREEN (draw_buffer));
    }
  g_slist_free (stack);
}

/* XXX: The target argument is redundant; when we break API, we should
 * remove it! */
void
cogl_set_draw_buffer (CoglBufferTarget target, CoglHandle handle)
{
  CoglDrawBuffer *draw_buffer = NULL;
  CoglDrawBufferStackEntry *entry;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_journal_flush ();

  g_assert (ctx->draw_buffer_stack != NULL);
  entry = ctx->draw_buffer_stack->data;

  if (target == COGL_WINDOW_BUFFER)
    handle = ctx->window_buffer;
  else if (!cogl_is_draw_buffer (handle))
    return;

  draw_buffer = COGL_DRAW_BUFFER (handle);

  if (entry->draw_buffer != draw_buffer)
    {
      entry->target = target;

      ctx->dirty_bound_framebuffer = 1;
      ctx->dirty_gl_viewport = 1;

      if (draw_buffer != COGL_INVALID_HANDLE)
        cogl_handle_ref (draw_buffer);
      if (entry->draw_buffer != COGL_INVALID_HANDLE)
        cogl_handle_unref (entry->draw_buffer);
      entry->draw_buffer = draw_buffer;

      /* We've effectively just switched the current modelview and
       * projection matrix stacks and clip state so we need to dirty
       * them to ensure they get flushed for the next batch of geometry
       * we flush */
      _cogl_matrix_stack_dirty (draw_buffer->modelview_stack);
      _cogl_matrix_stack_dirty (draw_buffer->projection_stack);
      _cogl_clip_stack_state_dirty (&draw_buffer->clip_state);
    }
}

CoglHandle
_cogl_get_draw_buffer (void)
{
  CoglDrawBufferStackEntry *entry;

  _COGL_GET_CONTEXT (ctx, NULL);

  g_assert (ctx->draw_buffer_stack);
  entry = ctx->draw_buffer_stack->data;

  return entry->draw_buffer;
}

void
cogl_push_draw_buffer (void)
{
  CoglDrawBufferStackEntry *old;
  CoglDrawBufferStackEntry *entry;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  entry = g_slice_new0 (CoglDrawBufferStackEntry);

  g_assert (ctx->draw_buffer_stack);

  old = ctx->draw_buffer_stack->data;
  *entry = *old;

  cogl_handle_ref (entry->draw_buffer);

  ctx->draw_buffer_stack =
    g_slist_prepend (ctx->draw_buffer_stack, entry);
}

void
cogl_pop_draw_buffer (void)
{
  CoglDrawBufferStackEntry *to_pop;
  CoglDrawBufferStackEntry *to_restore;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_assert (ctx->draw_buffer_stack != NULL);

  to_pop = ctx->draw_buffer_stack->data;
  to_restore = ctx->draw_buffer_stack->next->data;

  cogl_set_draw_buffer (to_restore->target, to_restore->draw_buffer);

  cogl_handle_unref (to_pop->draw_buffer);
  ctx->draw_buffer_stack =
    g_slist_remove_link (ctx->draw_buffer_stack,
                         ctx->draw_buffer_stack);
  g_slice_free (CoglDrawBufferStackEntry, to_pop);
}

void
_cogl_draw_buffer_flush_state (CoglHandle handle,
                               CoglDrawBufferFlushFlags flags)
{
  CoglDrawBuffer *draw_buffer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  draw_buffer = COGL_DRAW_BUFFER (handle);

  if (cogl_features_available (COGL_FEATURE_OFFSCREEN) &&
      ctx->dirty_bound_framebuffer)
    {
      if (draw_buffer->type == COGL_DRAW_BUFFER_TYPE_OFFSCREEN)
        {
          GE (glBindFramebuffer (GL_FRAMEBUFFER,
                                 COGL_OFFSCREEN (draw_buffer)->fbo_handle));
        }
      else
        GE (glBindFramebuffer (GL_FRAMEBUFFER, 0));
      ctx->dirty_bound_framebuffer = FALSE;
    }

  if (ctx->dirty_gl_viewport)
    {
      int gl_viewport_y;

      /* Convert the Cogl viewport y offset to an OpenGL viewport y offset
       * NB: OpenGL defines its window and viewport origins to be bottom
       * left, while Cogl defines them to be top left.
       * NB: We render upside down to offscreen draw buffers so we don't
       * need to convert the y offset in this case. */
      if (cogl_is_offscreen (draw_buffer))
        gl_viewport_y = draw_buffer->viewport_y;
      else
        gl_viewport_y = draw_buffer->height -
          (draw_buffer->viewport_y + draw_buffer->viewport_height);

      GE (glViewport (draw_buffer->viewport_x,
                      gl_viewport_y,
                      draw_buffer->viewport_width,
                      draw_buffer->viewport_height));
      ctx->dirty_gl_viewport = FALSE;
    }

  /* XXX: Flushing clip state may trash the modelview and projection
   * matrices so we must do it before flushing the matrices...
   */
  _cogl_flush_clip_state (&draw_buffer->clip_state);

  if (!(flags & COGL_DRAW_BUFFER_FLUSH_SKIP_MODELVIEW))
    _cogl_matrix_stack_flush_to_gl (draw_buffer->modelview_stack,
                                    COGL_MATRIX_MODELVIEW);

  _cogl_matrix_stack_flush_to_gl (draw_buffer->projection_stack,
                                  COGL_MATRIX_PROJECTION);
}

