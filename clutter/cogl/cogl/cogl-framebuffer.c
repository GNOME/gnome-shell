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
#include "cogl-framebuffer-private.h"
#include "cogl-clip-stack.h"
#include "cogl-journal-private.h"

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
#ifndef GL_DEPTH_STENCIL
#define GL_DEPTH_STENCIL        0x84F9
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT     0x8D00
#endif
#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16    0x81A5
#endif

typedef enum {
  _TRY_DEPTH_STENCIL = 1L<<0,
  _TRY_DEPTH         = 1L<<1,
  _TRY_STENCIL       = 1L<<2
} TryFBOFlags;

static void _cogl_framebuffer_free (CoglFramebuffer *framebuffer);
static void _cogl_onscreen_free (CoglOnscreen *onscreen);
static void _cogl_offscreen_free (CoglOffscreen *offscreen);

COGL_HANDLE_DEFINE (Onscreen, onscreen);
COGL_HANDLE_DEFINE (Offscreen, offscreen);

/* XXX:
 * The CoglHandle macros don't support any form of inheritance, so for
 * now we implement the CoglHandle support for the CoglFramebuffer
 * abstract class manually.
 */

gboolean
cogl_is_framebuffer (CoglHandle handle)
{
  CoglHandleObject *obj = (CoglHandleObject *)handle;

  if (handle == COGL_INVALID_HANDLE)
    return FALSE;

  return obj->klass->type == _cogl_handle_onscreen_get_type ()
    || obj->klass->type == _cogl_handle_offscreen_get_type ();
}

static void
_cogl_framebuffer_init (CoglFramebuffer *framebuffer,
                        CoglFramebufferType type,
                        int width,
                        int height)
{
  framebuffer->type             = type;
  framebuffer->width            = width;
  framebuffer->height           = height;
  framebuffer->viewport_x       = 0;
  framebuffer->viewport_y       = 0;
  framebuffer->viewport_width   = width;
  framebuffer->viewport_height  = height;

  framebuffer->modelview_stack  = _cogl_matrix_stack_new ();
  framebuffer->projection_stack = _cogl_matrix_stack_new ();

  /* Initialise the clip stack */
  _cogl_clip_stack_state_init (&framebuffer->clip_state);
}

void
_cogl_framebuffer_free (CoglFramebuffer *framebuffer)
{
  _cogl_clip_stack_state_destroy (&framebuffer->clip_state);

  _cogl_matrix_stack_destroy (framebuffer->modelview_stack);
  framebuffer->modelview_stack = NULL;

  _cogl_matrix_stack_destroy (framebuffer->projection_stack);
  framebuffer->projection_stack = NULL;
}

int
_cogl_framebuffer_get_width (CoglHandle handle)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (handle);
  return framebuffer->width;
}

int
_cogl_framebuffer_get_height (CoglHandle handle)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (handle);
  return framebuffer->height;
}

CoglClipStackState *
_cogl_framebuffer_get_clip_state (CoglHandle handle)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (handle);

  return &framebuffer->clip_state;
}

void
_cogl_framebuffer_set_viewport (CoglHandle handle,
                                int x,
                                int y,
                                int width,
                                int height)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (handle);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (framebuffer->viewport_x == x &&
      framebuffer->viewport_y == y &&
      framebuffer->viewport_width == width &&
      framebuffer->viewport_height == height)
    return;

  _cogl_journal_flush ();

  framebuffer->viewport_x = x;
  framebuffer->viewport_y = y;
  framebuffer->viewport_width = width;
  framebuffer->viewport_height = height;

  if (_cogl_get_framebuffer () == framebuffer)
    ctx->dirty_gl_viewport = TRUE;
}

int
_cogl_framebuffer_get_viewport_x (CoglHandle handle)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (handle);
  return framebuffer->viewport_x;
}

int
_cogl_framebuffer_get_viewport_y (CoglHandle handle)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (handle);
  return framebuffer->viewport_y;
}

int
_cogl_framebuffer_get_viewport_width (CoglHandle handle)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (handle);
  return framebuffer->viewport_width;
}

int
_cogl_framebuffer_get_viewport_height (CoglHandle handle)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (handle);
  return framebuffer->viewport_height;
}

void
_cogl_framebuffer_get_viewport4fv (CoglHandle handle, int *viewport)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (handle);
  viewport[0] = framebuffer->viewport_x;
  viewport[1] = framebuffer->viewport_y;
  viewport[2] = framebuffer->viewport_width;
  viewport[3] = framebuffer->viewport_height;
}

CoglMatrixStack *
_cogl_framebuffer_get_modelview_stack (CoglHandle handle)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (handle);
  return framebuffer->modelview_stack;
}

CoglMatrixStack *
_cogl_framebuffer_get_projection_stack (CoglHandle handle)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (handle);
  return framebuffer->projection_stack;
}

static gboolean
try_creating_fbo (CoglOffscreen *offscreen,
                  TryFBOFlags flags,
                  CoglHandle texture)
{
  GLuint gl_depth_stencil_handle;
  GLuint gl_depth_handle;
  GLuint gl_stencil_handle;
  GLuint tex_gl_handle;
  GLenum tex_gl_target;
  GLuint fbo_gl_handle;
  GLenum status;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_texture_get_gl_texture (texture, &tex_gl_handle, &tex_gl_target))
    return FALSE;

  if (tex_gl_target != GL_TEXTURE_2D
#ifdef HAVE_COGL_GL
      && tex_gl_target != GL_TEXTURE_RECTANGLE_ARB
#endif
      )
    return FALSE;

  /* We are about to generate and bind a new fbo, so when next flushing the
   * journal, we will need to rebind the current framebuffer... */
  ctx->dirty_bound_framebuffer = 1;

  /* Generate framebuffer */
  glGenFramebuffers (1, &fbo_gl_handle);
  GE (glBindFramebuffer (GL_FRAMEBUFFER, fbo_gl_handle));
  offscreen->fbo_handle = fbo_gl_handle;

  GE (glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			      tex_gl_target, tex_gl_handle, 0));

  if (flags & _TRY_DEPTH_STENCIL)
    {
      /* Create a renderbuffer for depth and stenciling */
      GE (glGenRenderbuffers (1, &gl_depth_stencil_handle));
      GE (glBindRenderbuffer (GL_RENDERBUFFER, gl_depth_stencil_handle));
      GE (glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_STENCIL,
                                 cogl_texture_get_width (texture),
                                 cogl_texture_get_height (texture)));
      GE (glBindRenderbuffer (GL_RENDERBUFFER, 0));
      GE (glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                     GL_STENCIL_ATTACHMENT,
                                     GL_RENDERBUFFER, gl_depth_stencil_handle));
      GE (glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                     GL_DEPTH_ATTACHMENT,
                                     GL_RENDERBUFFER, gl_depth_stencil_handle));
      offscreen->renderbuffers =
        g_slist_prepend (offscreen->renderbuffers,
                         GUINT_TO_POINTER (gl_depth_stencil_handle));
    }

  if (flags & _TRY_DEPTH)
    {
      GE (glGenRenderbuffers (1, &gl_depth_handle));
      GE (glBindRenderbuffer (GL_RENDERBUFFER, gl_depth_handle));
      /* For now we just ask for GL_DEPTH_COMPONENT16 since this is all that's
       * available under GLES */
      GE (glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                                 cogl_texture_get_width (texture),
                                 cogl_texture_get_height (texture)));
      GE (glBindRenderbuffer (GL_RENDERBUFFER, 0));
      GE (glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                     GL_DEPTH_ATTACHMENT,
                                     GL_RENDERBUFFER, gl_depth_handle));
      offscreen->renderbuffers =
        g_slist_prepend (offscreen->renderbuffers,
                         GUINT_TO_POINTER (gl_depth_handle));
    }

  if (flags & _TRY_STENCIL)
    {
      GE (glGenRenderbuffers (1, &gl_stencil_handle));
      GE (glBindRenderbuffer (GL_RENDERBUFFER, gl_stencil_handle));
      GE (glRenderbufferStorage (GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                                 cogl_texture_get_width (texture),
                                 cogl_texture_get_height (texture)));
      GE (glBindRenderbuffer (GL_RENDERBUFFER, 0));
      GE (glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                     GL_STENCIL_ATTACHMENT,
                                     GL_RENDERBUFFER, gl_stencil_handle));
      offscreen->renderbuffers =
        g_slist_prepend (offscreen->renderbuffers,
                         GUINT_TO_POINTER (gl_stencil_handle));
    }

  /* Make sure it's complete */
  status = glCheckFramebufferStatus (GL_FRAMEBUFFER);

  if (status != GL_FRAMEBUFFER_COMPLETE)
    {
      GSList *l;

      GE (glDeleteFramebuffers (1, &fbo_gl_handle));

      for (l = offscreen->renderbuffers; l; l = l->next)
        {
          GLuint renderbuffer = GPOINTER_TO_UINT (l->data);
          GE (glDeleteRenderbuffers (1, &renderbuffer));
        }
      return FALSE;
    }

  return TRUE;
}

CoglHandle
cogl_offscreen_new_to_texture (CoglHandle texhandle)
{
  CoglOffscreen      *offscreen;
  static TryFBOFlags  flags;
  static gboolean     have_working_flags = FALSE;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN))
    return COGL_INVALID_HANDLE;

  /* Make texhandle is a valid texture object */
  if (!cogl_is_texture (texhandle))
    return COGL_INVALID_HANDLE;

  /* The texture must not be sliced */
  if (cogl_texture_is_sliced (texhandle))
    return COGL_INVALID_HANDLE;

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

  offscreen = g_new0 (CoglOffscreen, 1);
  offscreen->texture = cogl_handle_ref (texhandle);

  if ((have_working_flags &&
       try_creating_fbo (offscreen, flags, texhandle)) ||
      try_creating_fbo (offscreen, flags = _TRY_DEPTH_STENCIL, texhandle) ||
      try_creating_fbo (offscreen, flags = _TRY_DEPTH | _TRY_STENCIL,
                        texhandle) ||
      try_creating_fbo (offscreen, flags = _TRY_STENCIL, texhandle) ||
      try_creating_fbo (offscreen, flags = _TRY_DEPTH, texhandle) ||
      try_creating_fbo (offscreen, flags = 0, texhandle))
    {
      /* Record that the last set of flags succeeded so that we can
         try that set first next time */
      have_working_flags = TRUE;

      _cogl_framebuffer_init (COGL_FRAMEBUFFER (offscreen),
                              COGL_FRAMEBUFFER_TYPE_OFFSCREEN,
                              cogl_texture_get_width (texhandle),
                              cogl_texture_get_height (texhandle));

      return _cogl_offscreen_handle_new (offscreen);
    }
  else
    {
      g_free (offscreen);
      /* XXX: This API should probably have been defined to take a GError */
      g_warning ("%s: Failed to create an OpenGL framebuffer", G_STRLOC);
      return COGL_INVALID_HANDLE;
    }
}

static void
_cogl_offscreen_free (CoglOffscreen *offscreen)
{
  GSList *l;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Chain up to parent */
  _cogl_framebuffer_free (COGL_FRAMEBUFFER (offscreen));

  for (l = offscreen->renderbuffers; l; l = l->next)
    {
      GLuint renderbuffer = GPOINTER_TO_UINT (l->data);
      GE (glDeleteRenderbuffers (1, &renderbuffer));
    }
  g_slist_free (offscreen->renderbuffers);

  GE (glDeleteFramebuffers (1, &offscreen->fbo_handle));

  if (offscreen->texture != COGL_INVALID_HANDLE)
    cogl_handle_unref (offscreen->texture);

  g_free (offscreen);
}

CoglHandle
_cogl_onscreen_new (void)
{
  CoglOnscreen *onscreen;

  /* XXX: Until we have full winsys support in Cogl then we can't fully
   * implement CoglOnscreen framebuffers, since we can't, e.g. keep track of
   * the window size. */

  onscreen = g_new0 (CoglOnscreen, 1);
  _cogl_framebuffer_init (COGL_FRAMEBUFFER (onscreen),
                          COGL_FRAMEBUFFER_TYPE_ONSCREEN,
                          0xdeadbeef, /* width */
                          0xdeadbeef); /* height */

  return _cogl_onscreen_handle_new (onscreen);
}

static void
_cogl_onscreen_free (CoglOnscreen *onscreen)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Chain up to parent */
  _cogl_framebuffer_free (COGL_FRAMEBUFFER (onscreen));

  g_free (onscreen);
}

void
_cogl_onscreen_clutter_backend_set_size (int width, int height)
{
  CoglFramebuffer *framebuffer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = COGL_FRAMEBUFFER (ctx->window_buffer);

  if (framebuffer->width == width && framebuffer->height == height)
    return;

  framebuffer->width = width;
  framebuffer->height = height;

  /* We'll need to recalculate the GL viewport state derived
   * from the Cogl viewport */
  ctx->dirty_gl_viewport = 1;
}

GSList *
_cogl_create_framebuffer_stack (void)
{
  GSList *stack = NULL;

  return g_slist_prepend (stack, COGL_INVALID_HANDLE);
}

void
_cogl_free_framebuffer_stack (GSList *stack)
{
  GSList *l;

  for (l = stack; l != NULL; l = l->next)
    {
      CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (l->data);
      if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN)
        _cogl_offscreen_free (COGL_OFFSCREEN (framebuffer));
      else
        _cogl_onscreen_free (COGL_ONSCREEN (framebuffer));
    }
  g_slist_free (stack);
}

/* Set the current framebuffer without checking if it's already the
 * current framebuffer. This is used by cogl_pop_framebuffer while
 * the top of the stack is currently not up to date. */
static void
_cogl_set_framebuffer_real (CoglFramebuffer *framebuffer)
{
  CoglHandle *entry;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_flush ();

  entry = &ctx->framebuffer_stack->data;

  ctx->dirty_bound_framebuffer = 1;
  ctx->dirty_gl_viewport = 1;

  if (framebuffer != COGL_INVALID_HANDLE)
    cogl_handle_ref (framebuffer);
  if (*entry != COGL_INVALID_HANDLE)
    cogl_handle_unref (*entry);

  *entry = framebuffer;

  /* We've effectively just switched the current modelview and
   * projection matrix stacks and clip state so we need to dirty
   * them to ensure they get flushed for the next batch of geometry
   * we flush */
  _cogl_matrix_stack_dirty (framebuffer->modelview_stack);
  _cogl_matrix_stack_dirty (framebuffer->projection_stack);
  _cogl_clip_stack_state_dirty (&framebuffer->clip_state);
}

void
cogl_set_framebuffer (CoglHandle handle)
{
  g_return_if_fail (cogl_is_framebuffer (handle));

  if (_cogl_get_framebuffer () != handle)
    _cogl_set_framebuffer_real (COGL_FRAMEBUFFER (handle));
}

/* XXX: deprecated API */
void
cogl_set_draw_buffer (CoglBufferTarget target, CoglHandle handle)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (target == COGL_WINDOW_BUFFER)
    handle = ctx->window_buffer;

  cogl_set_framebuffer (handle);
}

CoglHandle
_cogl_get_framebuffer (void)
{
  _COGL_GET_CONTEXT (ctx, NULL);

  g_assert (ctx->framebuffer_stack);

  return (CoglHandle)ctx->framebuffer_stack->data;
}

void
cogl_push_framebuffer (CoglHandle buffer)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_is_framebuffer (buffer));
  g_assert (ctx->framebuffer_stack);

  cogl_flush ();

  ctx->framebuffer_stack =
    g_slist_prepend (ctx->framebuffer_stack, COGL_INVALID_HANDLE);

  cogl_set_framebuffer (buffer);
}

/* XXX: deprecated API */
void
cogl_push_draw_buffer (void)
{
  cogl_push_framebuffer (_cogl_get_framebuffer ());
}

void
cogl_pop_framebuffer (void)
{
  CoglHandle to_pop;
  CoglHandle to_restore;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_assert (ctx->framebuffer_stack != NULL);
  g_assert (ctx->framebuffer_stack->next != NULL);

  to_pop = ctx->framebuffer_stack->data;
  to_restore = ctx->framebuffer_stack->next->data;

  cogl_flush ();

  cogl_handle_unref (to_pop);
  ctx->framebuffer_stack =
    g_slist_remove_link (ctx->framebuffer_stack,
                         ctx->framebuffer_stack);

  /* If the framebuffer has changed as a result of popping the top
   * then re-assert the current buffer so as to dirty state as
   * necessary. */
  if (to_pop != to_restore)
    _cogl_set_framebuffer_real (to_restore);
}

/* XXX: deprecated API */
void
cogl_pop_draw_buffer (void)
{
  cogl_pop_framebuffer ();
}

void
_cogl_framebuffer_flush_state (CoglHandle handle,
                               CoglFramebufferFlushFlags flags)
{
  CoglFramebuffer *framebuffer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  framebuffer = COGL_FRAMEBUFFER (handle);

  if (cogl_features_available (COGL_FEATURE_OFFSCREEN) &&
      ctx->dirty_bound_framebuffer)
    {
      if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN)
        {
          GE (glBindFramebuffer (GL_FRAMEBUFFER,
                                 COGL_OFFSCREEN (framebuffer)->fbo_handle));
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
       * NB: We render upside down to offscreen framebuffers so we don't
       * need to convert the y offset in this case. */
      if (cogl_is_offscreen (framebuffer))
        gl_viewport_y = framebuffer->viewport_y;
      else
        gl_viewport_y = framebuffer->height -
          (framebuffer->viewport_y + framebuffer->viewport_height);

      GE (glViewport (framebuffer->viewport_x,
                      gl_viewport_y,
                      framebuffer->viewport_width,
                      framebuffer->viewport_height));
      ctx->dirty_gl_viewport = FALSE;
    }

  /* XXX: Flushing clip state may trash the modelview and projection
   * matrices so we must do it before flushing the matrices...
   */
  _cogl_flush_clip_state (&framebuffer->clip_state);

  if (!(flags & COGL_FRAMEBUFFER_FLUSH_SKIP_MODELVIEW))
    _cogl_matrix_stack_flush_to_gl (framebuffer->modelview_stack,
                                    COGL_MATRIX_MODELVIEW);

  _cogl_matrix_stack_flush_to_gl (framebuffer->projection_stack,
                                  COGL_MATRIX_PROJECTION);
}

