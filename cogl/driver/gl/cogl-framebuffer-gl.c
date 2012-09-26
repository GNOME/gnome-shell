/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2012 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-framebuffer-gl-private.h"
#include "cogl-buffer-gl-private.h"
#include "cogl-error-private.h"

#include <glib.h>

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
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8     0x88F0
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT     0x8D00
#endif
#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16    0x81A5
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE      0x8212
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE    0x8213
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE     0x8214
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE    0x8215
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE    0x8216
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE  0x8217
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER               0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER               0x8CA9
#endif
#ifndef GL_TEXTURE_SAMPLES_IMG
#define GL_TEXTURE_SAMPLES_IMG            0x9136
#endif
#ifndef GL_PACK_INVERT_MESA
#define GL_PACK_INVERT_MESA 0x8758
#endif

static void
_cogl_framebuffer_gl_flush_viewport_state (CoglFramebuffer *framebuffer)
{
  float gl_viewport_y;

  g_assert (framebuffer->viewport_width >=0 &&
            framebuffer->viewport_height >=0);

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

  COGL_NOTE (OPENGL, "Calling glViewport(%f, %f, %f, %f)",
             framebuffer->viewport_x,
             gl_viewport_y,
             framebuffer->viewport_width,
             framebuffer->viewport_height);

  GE (framebuffer->context,
      glViewport (framebuffer->viewport_x,
                  gl_viewport_y,
                  framebuffer->viewport_width,
                  framebuffer->viewport_height));
}

static void
_cogl_framebuffer_gl_flush_clip_state (CoglFramebuffer *framebuffer)
{
  CoglClipStack *stack = _cogl_clip_state_get_stack (&framebuffer->clip_state);
  _cogl_clip_stack_flush (stack, framebuffer);
}

static void
_cogl_framebuffer_gl_flush_dither_state (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;

  if (ctx->current_gl_dither_enabled != framebuffer->dither_enabled)
    {
      if (framebuffer->dither_enabled)
        GE (ctx, glEnable (GL_DITHER));
      else
        GE (ctx, glDisable (GL_DITHER));
      ctx->current_gl_dither_enabled = framebuffer->dither_enabled;
    }
}

static void
_cogl_framebuffer_gl_flush_modelview_state (CoglFramebuffer *framebuffer)
{
  CoglMatrixEntry *modelview_entry =
    _cogl_framebuffer_get_modelview_entry (framebuffer);
  _cogl_context_set_current_modelview_entry (framebuffer->context,
                                             modelview_entry);
}

static void
_cogl_framebuffer_gl_flush_projection_state (CoglFramebuffer *framebuffer)
{
  CoglMatrixEntry *projection_entry =
    _cogl_framebuffer_get_projection_entry (framebuffer);
  _cogl_context_set_current_projection_entry (framebuffer->context,
                                             projection_entry);
}

static void
_cogl_framebuffer_gl_flush_color_mask_state (CoglFramebuffer *framebuffer)
{
  CoglContext *context = framebuffer->context;

  /* The color mask state is really owned by a CoglPipeline so to
   * ensure the color mask is updated the next time we draw something
   * we need to make sure the logic ops for the pipeline are
   * re-flushed... */
  context->current_pipeline_changes_since_flush |=
    COGL_PIPELINE_STATE_LOGIC_OPS;
  context->current_pipeline_age--;
}

static void
_cogl_framebuffer_gl_flush_front_face_winding_state (CoglFramebuffer *framebuffer)
{
  CoglContext *context = framebuffer->context;
  CoglPipelineCullFaceMode mode;

  /* NB: The face winding state is actually owned by the current
   * CoglPipeline.
   *
   * If we don't have a current pipeline then we can just assume that
   * when we later do flush a pipeline we will check the current
   * framebuffer to know how to setup the winding */
  if (!context->current_pipeline)
    return;

  mode = cogl_pipeline_get_cull_face_mode (context->current_pipeline);

  /* If the current CoglPipeline has a culling mode that doesn't care
   * about the winding we can avoid forcing an update of the state and
   * bail out. */
  if (mode == COGL_PIPELINE_CULL_FACE_MODE_NONE ||
      mode == COGL_PIPELINE_CULL_FACE_MODE_BOTH)
    return;

  /* Since the winding state is really owned by the current pipeline
   * the way we "flush" an updated winding is to dirty the pipeline
   * state... */
  context->current_pipeline_changes_since_flush |=
    COGL_PIPELINE_STATE_CULL_FACE;
  context->current_pipeline_age--;
}

void
_cogl_framebuffer_gl_bind (CoglFramebuffer *framebuffer, GLenum target)
{
  CoglContext *ctx = framebuffer->context;

  if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN)
    {
      CoglOffscreen *offscreen = COGL_OFFSCREEN (framebuffer);
      GE (ctx, glBindFramebuffer (target,
                                  offscreen->gl_framebuffer.fbo_handle));
    }
  else
    {
      const CoglWinsysVtable *winsys =
        _cogl_framebuffer_get_winsys (framebuffer);
      winsys->onscreen_bind (COGL_ONSCREEN (framebuffer));
      /* glBindFramebuffer is an an extension with OpenGL ES 1.1 */
      if (cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
        GE (ctx, glBindFramebuffer (target, 0));
    }
}

void
_cogl_framebuffer_gl_flush_state (CoglFramebuffer *draw_buffer,
                                  CoglFramebuffer *read_buffer,
                                  CoglFramebufferState state)
{
  CoglContext *ctx = draw_buffer->context;
  unsigned long differences;
  int bit;

  /* We can assume that any state that has changed for the current
   * framebuffer is different to the currently flushed value. */
  differences = ctx->current_draw_buffer_changes;

  /* Any state of the current framebuffer that hasn't already been
   * flushed is assumed to be unknown so we will always flush that
   * state if asked. */
  differences |= ~ctx->current_draw_buffer_state_flushed;

  /* We only need to consider the state we've been asked to flush */
  differences &= state;

  if (ctx->current_draw_buffer != draw_buffer)
    {
      /* If the previous draw buffer is NULL then we'll assume
         everything has changed. This can happen if a framebuffer is
         destroyed while it is the last flushed draw buffer. In that
         case the framebuffer destructor will set
         ctx->current_draw_buffer to NULL */
      if (ctx->current_draw_buffer == NULL)
        differences |= state;
      else
        /* NB: we only need to compare the state we're being asked to flush
         * and we don't need to compare the state we've already decided
         * we will definitely flush... */
        differences |= _cogl_framebuffer_compare (ctx->current_draw_buffer,
                                                  draw_buffer,
                                                  state & ~differences);

      /* NB: we don't take a reference here, to avoid a circular
       * reference. */
      ctx->current_draw_buffer = draw_buffer;
      ctx->current_draw_buffer_state_flushed = 0;
    }

  if (ctx->current_read_buffer != read_buffer &&
      state & COGL_FRAMEBUFFER_STATE_BIND)
    {
      differences |= COGL_FRAMEBUFFER_STATE_BIND;
      /* NB: we don't take a reference here, to avoid a circular
       * reference. */
      ctx->current_read_buffer = read_buffer;
    }

  if (!differences)
    return;

  /* Lazily ensure the framebuffers have been allocated */
  if (G_UNLIKELY (!draw_buffer->allocated))
    cogl_framebuffer_allocate (draw_buffer, NULL);
  if (G_UNLIKELY (!read_buffer->allocated))
    cogl_framebuffer_allocate (read_buffer, NULL);

  /* We handle buffer binding separately since the method depends on whether
   * we are binding the same buffer for read and write or not unlike all
   * other state that only relates to the draw_buffer. */
  if (differences & COGL_FRAMEBUFFER_STATE_BIND)
    {
      if (draw_buffer == read_buffer)
        _cogl_framebuffer_gl_bind (draw_buffer, GL_FRAMEBUFFER);
      else
        {
          /* NB: Currently we only take advantage of binding separate
           * read/write buffers for offscreen framebuffer blit
           * purposes.  */
          _COGL_RETURN_IF_FAIL (ctx->private_feature_flags &
                                COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT);
          _COGL_RETURN_IF_FAIL (draw_buffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN);
          _COGL_RETURN_IF_FAIL (read_buffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN);

          _cogl_framebuffer_gl_bind (draw_buffer, GL_DRAW_FRAMEBUFFER);
          _cogl_framebuffer_gl_bind (read_buffer, GL_READ_FRAMEBUFFER);
        }

      differences &= ~COGL_FRAMEBUFFER_STATE_BIND;
    }

  COGL_FLAGS_FOREACH_START (&differences, 1, bit)
    {
      /* XXX: We considered having an array of callbacks for each state index
       * that we'd call here but decided that this way the compiler is more
       * likely going to be able to in-line the flush functions and use the
       * index to jump straight to the required code. */
      switch (bit)
        {
        case COGL_FRAMEBUFFER_STATE_INDEX_VIEWPORT:
          _cogl_framebuffer_gl_flush_viewport_state (draw_buffer);
          break;
        case COGL_FRAMEBUFFER_STATE_INDEX_CLIP:
          _cogl_framebuffer_gl_flush_clip_state (draw_buffer);
          break;
        case COGL_FRAMEBUFFER_STATE_INDEX_DITHER:
          _cogl_framebuffer_gl_flush_dither_state (draw_buffer);
          break;
        case COGL_FRAMEBUFFER_STATE_INDEX_MODELVIEW:
          _cogl_framebuffer_gl_flush_modelview_state (draw_buffer);
          break;
        case COGL_FRAMEBUFFER_STATE_INDEX_PROJECTION:
          _cogl_framebuffer_gl_flush_projection_state (draw_buffer);
          break;
        case COGL_FRAMEBUFFER_STATE_INDEX_COLOR_MASK:
          _cogl_framebuffer_gl_flush_color_mask_state (draw_buffer);
          break;
        case COGL_FRAMEBUFFER_STATE_INDEX_FRONT_FACE_WINDING:
          _cogl_framebuffer_gl_flush_front_face_winding_state (draw_buffer);
          break;
        default:
          g_warn_if_reached ();
        }
    }
  COGL_FLAGS_FOREACH_END;

  ctx->current_draw_buffer_state_flushed |= state;
  ctx->current_draw_buffer_changes &= ~state;
}

static CoglTexture *
create_depth_texture (CoglContext *ctx,
                      int width,
                      int height)
{
  CoglPixelFormat format;
  CoglTexture2D *depth_texture;

  if (ctx->private_feature_flags &
      (COGL_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL |
       COGL_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL))
    {
      format = COGL_PIXEL_FORMAT_DEPTH_24_STENCIL_8;
    }
  else
    format = COGL_PIXEL_FORMAT_DEPTH_16;

  depth_texture =  cogl_texture_2d_new_with_size (ctx,
                                                  width, height,
                                                  format,
                                                  NULL);

  return COGL_TEXTURE (depth_texture);
}

static CoglTexture *
attach_depth_texture (CoglContext *ctx,
                      CoglTexture *depth_texture,
                      CoglOffscreenAllocateFlags flags)
{
  GLuint tex_gl_handle;
  GLenum tex_gl_target;

  if (flags & COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL)
    {
      /* attach a GL_DEPTH_STENCIL texture to the GL_DEPTH_ATTACHMENT and
       * GL_STENCIL_ATTACHMENT attachement points */
      g_assert (cogl_texture_get_format (depth_texture) ==
                COGL_PIXEL_FORMAT_DEPTH_24_STENCIL_8);

      cogl_texture_get_gl_texture (depth_texture,
                                   &tex_gl_handle, &tex_gl_target);

      GE (ctx, glFramebufferTexture2D (GL_FRAMEBUFFER,
                                       GL_DEPTH_ATTACHMENT,
                                       tex_gl_target, tex_gl_handle,
                                       0));
      GE (ctx, glFramebufferTexture2D (GL_FRAMEBUFFER,
                                       GL_STENCIL_ATTACHMENT,
                                       tex_gl_target, tex_gl_handle,
                                       0));
    }
  else if (flags & COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH)
    {
      /* attach a newly created GL_DEPTH_COMPONENT16 texture to the
       * GL_DEPTH_ATTACHMENT attachement point */
      g_assert (cogl_texture_get_format (depth_texture) ==
                COGL_PIXEL_FORMAT_DEPTH_16);

      cogl_texture_get_gl_texture (COGL_TEXTURE (depth_texture),
                                   &tex_gl_handle, &tex_gl_target);

      GE (ctx, glFramebufferTexture2D (GL_FRAMEBUFFER,
                                       GL_DEPTH_ATTACHMENT,
                                       tex_gl_target, tex_gl_handle,
                                       0));
    }

  return COGL_TEXTURE (depth_texture);
}

static GList *
try_creating_renderbuffers (CoglContext *ctx,
                            int width,
                            int height,
                            CoglOffscreenAllocateFlags flags,
                            int n_samples)
{
  GList *renderbuffers = NULL;
  GLuint gl_depth_stencil_handle;

  if (flags & COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL)
    {
      GLenum format;

      /* Although GL_OES_packed_depth_stencil is mostly equivalent to
       * GL_EXT_packed_depth_stencil, one notable difference is that
       * GL_OES_packed_depth_stencil doesn't allow GL_DEPTH_STENCIL to
       * be passed as an internal format to glRenderbufferStorage.
       */
      if (ctx->private_feature_flags &
          COGL_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL)
        format = GL_DEPTH_STENCIL;
      else
        {
          _COGL_RETURN_VAL_IF_FAIL (
                                  ctx->private_feature_flags &
                                  COGL_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL,
                                  NULL);
          format = GL_DEPTH24_STENCIL8;
        }

      /* Create a renderbuffer for depth and stenciling */
      GE (ctx, glGenRenderbuffers (1, &gl_depth_stencil_handle));
      GE (ctx, glBindRenderbuffer (GL_RENDERBUFFER, gl_depth_stencil_handle));
      if (n_samples)
        GE (ctx, glRenderbufferStorageMultisampleIMG (GL_RENDERBUFFER,
                                                      n_samples,
                                                      format,
                                                      width, height));
      else
        GE (ctx, glRenderbufferStorage (GL_RENDERBUFFER, format,
                                        width, height));
      GE (ctx, glBindRenderbuffer (GL_RENDERBUFFER, 0));
      GE (ctx, glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                          GL_STENCIL_ATTACHMENT,
                                          GL_RENDERBUFFER,
                                          gl_depth_stencil_handle));
      GE (ctx, glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                          GL_DEPTH_ATTACHMENT,
                                          GL_RENDERBUFFER,
                                          gl_depth_stencil_handle));
      renderbuffers =
        g_list_prepend (renderbuffers,
                        GUINT_TO_POINTER (gl_depth_stencil_handle));
    }

  if (flags & COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH)
    {
      GLuint gl_depth_handle;

      GE (ctx, glGenRenderbuffers (1, &gl_depth_handle));
      GE (ctx, glBindRenderbuffer (GL_RENDERBUFFER, gl_depth_handle));
      /* For now we just ask for GL_DEPTH_COMPONENT16 since this is all that's
       * available under GLES */
      if (n_samples)
        GE (ctx, glRenderbufferStorageMultisampleIMG (GL_RENDERBUFFER,
                                                      n_samples,
                                                      GL_DEPTH_COMPONENT16,
                                                      width, height));
      else
        GE (ctx, glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                                        width, height));
      GE (ctx, glBindRenderbuffer (GL_RENDERBUFFER, 0));
      GE (ctx, glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                          GL_DEPTH_ATTACHMENT,
                                          GL_RENDERBUFFER, gl_depth_handle));
      renderbuffers =
        g_list_prepend (renderbuffers, GUINT_TO_POINTER (gl_depth_handle));
    }

  if (flags & COGL_OFFSCREEN_ALLOCATE_FLAG_STENCIL)
    {
      GLuint gl_stencil_handle;

      GE (ctx, glGenRenderbuffers (1, &gl_stencil_handle));
      GE (ctx, glBindRenderbuffer (GL_RENDERBUFFER, gl_stencil_handle));
      if (n_samples)
        GE (ctx, glRenderbufferStorageMultisampleIMG (GL_RENDERBUFFER,
                                                      n_samples,
                                                      GL_STENCIL_INDEX8,
                                                      width, height));
      else
        GE (ctx, glRenderbufferStorage (GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                                        width, height));
      GE (ctx, glBindRenderbuffer (GL_RENDERBUFFER, 0));
      GE (ctx, glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                          GL_STENCIL_ATTACHMENT,
                                          GL_RENDERBUFFER, gl_stencil_handle));
      renderbuffers =
        g_list_prepend (renderbuffers, GUINT_TO_POINTER (gl_stencil_handle));
    }

  return renderbuffers;
}

static void
delete_renderbuffers (CoglContext *ctx, GList *renderbuffers)
{
  GList *l;

  for (l = renderbuffers; l; l = l->next)
    {
      GLuint renderbuffer = GPOINTER_TO_UINT (l->data);
      GE (ctx, glDeleteRenderbuffers (1, &renderbuffer));
    }

  g_list_free (renderbuffers);
}

/*
 * NB: This function may be called with a standalone GLES2 context
 * bound so we can create a shadow framebuffer that wraps the same
 * CoglTexture as the given CoglOffscreen. This function shouldn't
 * modify anything in
 */
static CoglBool
try_creating_fbo (CoglContext *ctx,
                  CoglTexture *texture,
                  int texture_level,
                  int texture_level_width,
                  int texture_level_height,
                  CoglTexture *depth_texture,
                  CoglFramebufferConfig *config,
                  CoglOffscreenAllocateFlags flags,
                  CoglGLFramebuffer *gl_framebuffer)
{
  GLuint tex_gl_handle;
  GLenum tex_gl_target;
  GLenum status;
  int n_samples;

  if (!cogl_texture_get_gl_texture (texture, &tex_gl_handle, &tex_gl_target))
    return FALSE;

  if (tex_gl_target != GL_TEXTURE_2D
#ifdef HAVE_COGL_GL
      && tex_gl_target != GL_TEXTURE_RECTANGLE_ARB
#endif
      )
    return FALSE;

  if (config->samples_per_pixel)
    {
      if (!ctx->glFramebufferTexture2DMultisampleIMG)
        return FALSE;
      n_samples = config->samples_per_pixel;
    }
  else
    n_samples = 0;

  /* We are about to generate and bind a new fbo, so we pretend to
   * change framebuffer state so that the old framebuffer will be
   * rebound again before drawing. */
  ctx->current_draw_buffer_changes |= COGL_FRAMEBUFFER_STATE_BIND;

  /* Generate framebuffer */
  ctx->glGenFramebuffers (1, &gl_framebuffer->fbo_handle);
  GE (ctx, glBindFramebuffer (GL_FRAMEBUFFER, gl_framebuffer->fbo_handle));

  if (n_samples)
    {
      GE (ctx, glFramebufferTexture2DMultisampleIMG (GL_FRAMEBUFFER,
                                                     GL_COLOR_ATTACHMENT0,
                                                     tex_gl_target, tex_gl_handle,
                                                     n_samples,
                                                     texture_level));
    }
  else
    GE (ctx, glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     tex_gl_target, tex_gl_handle,
                                     texture_level));

  /* attach either a depth/stencil texture, a depth texture or render buffers
   * depending on what we've been asked to provide */

  if (depth_texture &&
      flags & (COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL |
               COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH))
    {
      attach_depth_texture (ctx, depth_texture, flags);

      /* Let's clear the flags that are now fulfilled as we might need to
       * create renderbuffers (for the ALLOCATE_FLAG_DEPTH |
       * ALLOCATE_FLAG_STENCIL case) */
      flags &= ~(COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL |
                 COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH);
    }

  if (flags)
    {
      gl_framebuffer->renderbuffers =
        try_creating_renderbuffers (ctx,
                                    texture_level_width,
                                    texture_level_height,
                                    flags,
                                    n_samples);
    }

  /* Make sure it's complete */
  status = ctx->glCheckFramebufferStatus (GL_FRAMEBUFFER);

  if (status != GL_FRAMEBUFFER_COMPLETE)
    {
      GE (ctx, glDeleteFramebuffers (1, &gl_framebuffer->fbo_handle));

      delete_renderbuffers (ctx, gl_framebuffer->renderbuffers);
      gl_framebuffer->renderbuffers = NULL;

      return FALSE;
    }

  /* Update the real number of samples_per_pixel now that we have a
   * complete framebuffer */
  if (n_samples)
    {
      GLenum attachment = GL_COLOR_ATTACHMENT0;
      GLenum pname = GL_TEXTURE_SAMPLES_IMG;
      int texture_samples;

      GE( ctx, glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER,
                                                      attachment,
                                                      pname,
                                                      &texture_samples) );
      gl_framebuffer->samples_per_pixel = texture_samples;
    }

  return TRUE;
}

CoglBool
_cogl_framebuffer_try_creating_gl_fbo (CoglContext *ctx,
                                       CoglTexture *texture,
                                       int texture_level,
                                       int texture_level_width,
                                       int texture_level_height,
                                       CoglTexture *depth_texture,
                                       CoglFramebufferConfig *config,
                                       CoglOffscreenAllocateFlags flags,
                                       CoglGLFramebuffer *gl_framebuffer)
{
  return try_creating_fbo (ctx,
                           texture,
                           texture_level,
                           texture_level_width,
                           texture_level_height,
                           depth_texture,
                           config,
                           flags,
                           gl_framebuffer);
}

CoglBool
_cogl_offscreen_gl_allocate (CoglOffscreen *offscreen,
                             CoglError **error)
{
  CoglFramebuffer *fb = COGL_FRAMEBUFFER (offscreen);
  CoglContext *ctx = fb->context;
  CoglOffscreenAllocateFlags flags;
  CoglGLFramebuffer *gl_framebuffer = &offscreen->gl_framebuffer;

  if (fb->config.depth_texture_enabled &&
      offscreen->depth_texture == NULL)
    {
      offscreen->depth_texture =
        create_depth_texture (ctx,
                              offscreen->texture_level_width,
                              offscreen->texture_level_height);

      if (offscreen->depth_texture)
        _cogl_texture_associate_framebuffer (offscreen->depth_texture, fb);
      else
        {
          _cogl_set_error (error, COGL_FRAMEBUFFER_ERROR,
                           COGL_FRAMEBUFFER_ERROR_ALLOCATE,
                           "Failed to allocate depth texture for framebuffer");
        }
    }

  /* XXX: The framebuffer_object spec isn't clear in defining whether attaching
   * a texture as a renderbuffer with mipmap filtering enabled while the
   * mipmaps have not been uploaded should result in an incomplete framebuffer
   * object. (different drivers make different decisions)
   *
   * To avoid an error with drivers that do consider this a problem we
   * explicitly set non mipmapped filters here. These will later be reset when
   * the texture is actually used for rendering according to the filters set on
   * the corresponding CoglPipeline.
   */
  _cogl_texture_gl_flush_legacy_texobj_filters (offscreen->texture,
                                                GL_NEAREST, GL_NEAREST);

  if (((offscreen->create_flags & COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL) &&
       try_creating_fbo (ctx,
                         offscreen->texture,
                         offscreen->texture_level,
                         offscreen->texture_level_width,
                         offscreen->texture_level_height,
                         offscreen->depth_texture,
                         &fb->config,
                         flags = 0,
                         gl_framebuffer)) ||

      (ctx->have_last_offscreen_allocate_flags &&
       try_creating_fbo (ctx,
                         offscreen->texture,
                         offscreen->texture_level,
                         offscreen->texture_level_width,
                         offscreen->texture_level_height,
                         offscreen->depth_texture,
                         &fb->config,
                         flags = ctx->last_offscreen_allocate_flags,
                         gl_framebuffer)) ||

      ((ctx->private_feature_flags &
        (COGL_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL |
         COGL_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL)) &&
       try_creating_fbo (ctx,
                         offscreen->texture,
                         offscreen->texture_level,
                         offscreen->texture_level_width,
                         offscreen->texture_level_height,
                         offscreen->depth_texture,
                         &fb->config,
                         flags = COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL,
                         gl_framebuffer)) ||

      try_creating_fbo (ctx,
                        offscreen->texture,
                        offscreen->texture_level,
                        offscreen->texture_level_width,
                        offscreen->texture_level_height,
                        offscreen->depth_texture,
                        &fb->config,
                        flags = COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH |
                        COGL_OFFSCREEN_ALLOCATE_FLAG_STENCIL,
                        gl_framebuffer) ||

      try_creating_fbo (ctx,
                        offscreen->texture,
                        offscreen->texture_level,
                        offscreen->texture_level_width,
                        offscreen->texture_level_height,
                        offscreen->depth_texture,
                        &fb->config,
                        flags = COGL_OFFSCREEN_ALLOCATE_FLAG_STENCIL,
                        gl_framebuffer) ||

      try_creating_fbo (ctx,
                        offscreen->texture,
                        offscreen->texture_level,
                        offscreen->texture_level_width,
                        offscreen->texture_level_height,
                        offscreen->depth_texture,
                        &fb->config,
                        flags = COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH,
                        gl_framebuffer) ||

      try_creating_fbo (ctx,
                        offscreen->texture,
                        offscreen->texture_level,
                        offscreen->texture_level_width,
                        offscreen->texture_level_height,
                        offscreen->depth_texture,
                        &fb->config,
                        flags = 0,
                        gl_framebuffer))
    {
      fb->samples_per_pixel = gl_framebuffer->samples_per_pixel;

      if (!offscreen->create_flags & COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL)
        {
          /* Record that the last set of flags succeeded so that we can
             try that set first next time */
          ctx->last_offscreen_allocate_flags = flags;
          ctx->have_last_offscreen_allocate_flags = TRUE;
        }

      /* Save the flags we managed to successfully allocate the
       * renderbuffers with in case we need to make renderbuffers for a
       * GLES2 context later */
      offscreen->allocation_flags = flags;

      return TRUE;
    }
  else
    {
      _cogl_set_error (error, COGL_FRAMEBUFFER_ERROR,
                       COGL_FRAMEBUFFER_ERROR_ALLOCATE,
                       "Failed to create an OpenGL framebuffer object");
      return FALSE;
    }
}

void
_cogl_offscreen_gl_free (CoglOffscreen *offscreen)
{
  CoglContext *ctx = COGL_FRAMEBUFFER (offscreen)->context;

  delete_renderbuffers (ctx, offscreen->gl_framebuffer.renderbuffers);

  GE (ctx, glDeleteFramebuffers (1, &offscreen->gl_framebuffer.fbo_handle));
}

void
_cogl_framebuffer_gl_clear (CoglFramebuffer *framebuffer,
                            unsigned long buffers,
                            float red,
                            float green,
                            float blue,
                            float alpha)
{
  CoglContext *ctx = framebuffer->context;
  GLbitfield gl_buffers = 0;

  if (buffers & COGL_BUFFER_BIT_COLOR)
    {
      GE( ctx, glClearColor (red, green, blue, alpha) );
      gl_buffers |= GL_COLOR_BUFFER_BIT;

      if (ctx->current_gl_color_mask != framebuffer->color_mask)
        {
          CoglColorMask color_mask = framebuffer->color_mask;
          GE( ctx, glColorMask (!!(color_mask & COGL_COLOR_MASK_RED),
                                !!(color_mask & COGL_COLOR_MASK_GREEN),
                                !!(color_mask & COGL_COLOR_MASK_BLUE),
                                !!(color_mask & COGL_COLOR_MASK_ALPHA)));
          ctx->current_gl_color_mask = color_mask;
          /* Make sure the ColorMask is updated when the next primitive is drawn */
          ctx->current_pipeline_changes_since_flush |=
            COGL_PIPELINE_STATE_LOGIC_OPS;
          ctx->current_pipeline_age--;
        }
    }

  if (buffers & COGL_BUFFER_BIT_DEPTH)
    gl_buffers |= GL_DEPTH_BUFFER_BIT;

  if (buffers & COGL_BUFFER_BIT_STENCIL)
    gl_buffers |= GL_STENCIL_BUFFER_BIT;


  GE (ctx, glClear (gl_buffers));
}

static inline void
_cogl_framebuffer_init_bits (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;

  cogl_framebuffer_allocate (framebuffer, NULL);

  if (G_LIKELY (!framebuffer->dirty_bitmasks))
    return;

#ifdef HAVE_COGL_GL
  if ((ctx->private_feature_flags &
       COGL_PRIVATE_FEATURE_QUERY_FRAMEBUFFER_BITS) &&
      framebuffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN)
    {
      GLenum attachment, pname;

      attachment = GL_COLOR_ATTACHMENT0;

      pname = GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE;
      GE( ctx, glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER,
                                                      attachment,
                                                      pname,
                                                      &framebuffer->red_bits) );

      pname = GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE;
      GE( ctx, glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER,
                                                      attachment,
                                                      pname,
                                                      &framebuffer->green_bits)
          );

      pname = GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE;
      GE( ctx, glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER,
                                                      attachment,
                                                      pname,
                                                      &framebuffer->blue_bits)
          );

      pname = GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE;
      GE( ctx, glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER,
                                                      attachment,
                                                      pname,
                                                      &framebuffer->alpha_bits)
          );
    }
  else
#endif /* HAVE_COGL_GL */
    {
      GE( ctx, glGetIntegerv (GL_RED_BITS,   &framebuffer->red_bits)   );
      GE( ctx, glGetIntegerv (GL_GREEN_BITS, &framebuffer->green_bits) );
      GE( ctx, glGetIntegerv (GL_BLUE_BITS,  &framebuffer->blue_bits)  );
      GE( ctx, glGetIntegerv (GL_ALPHA_BITS, &framebuffer->alpha_bits) );
    }


  COGL_NOTE (OFFSCREEN,
             "RGBA Bits for framebuffer[%p, %s]: %d, %d, %d, %d",
             framebuffer,
             framebuffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN
               ? "offscreen"
               : "onscreen",
             framebuffer->red_bits,
             framebuffer->blue_bits,
             framebuffer->green_bits,
             framebuffer->alpha_bits);

  framebuffer->dirty_bitmasks = FALSE;
}

void
_cogl_framebuffer_gl_query_bits (CoglFramebuffer *framebuffer,
                                 int *red,
                                 int *green,
                                 int *blue,
                                 int *alpha)
{
  _cogl_framebuffer_init_bits (framebuffer);

  /* TODO: cache these in some driver specific location not
   * directly as part of CoglFramebuffer. */
  *red = framebuffer->red_bits;
  *green = framebuffer->green_bits;
  *blue = framebuffer->blue_bits;
  *alpha = framebuffer->alpha_bits;
}

void
_cogl_framebuffer_gl_finish (CoglFramebuffer *framebuffer)
{
  GE (framebuffer->context, glFinish ());
}

void
_cogl_framebuffer_gl_discard_buffers (CoglFramebuffer *framebuffer,
                                      unsigned long buffers)
{
#ifdef GL_EXT_discard_framebuffer
  CoglContext *ctx = framebuffer->context;

  if (ctx->glDiscardFramebuffer)
    {
      GLenum attachments[3];
      int i = 0;

      if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
        {
          if (buffers & COGL_BUFFER_BIT_COLOR)
            attachments[i++] = GL_COLOR_EXT;
          if (buffers & COGL_BUFFER_BIT_DEPTH)
            attachments[i++] = GL_DEPTH_EXT;
          if (buffers & COGL_BUFFER_BIT_STENCIL)
            attachments[i++] = GL_STENCIL_EXT;
        }
      else
        {
          if (buffers & COGL_BUFFER_BIT_COLOR)
            attachments[i++] = GL_COLOR_ATTACHMENT0;
          if (buffers & COGL_BUFFER_BIT_DEPTH)
            attachments[i++] = GL_DEPTH_ATTACHMENT;
          if (buffers & COGL_BUFFER_BIT_STENCIL)
            attachments[i++] = GL_STENCIL_ATTACHMENT;
        }

      GE (ctx, glDiscardFramebuffer (GL_FRAMEBUFFER, i, attachments));
    }
#endif /* GL_EXT_discard_framebuffer */
}

void
_cogl_framebuffer_gl_draw_attributes (CoglFramebuffer *framebuffer,
                                      CoglPipeline *pipeline,
                                      CoglVerticesMode mode,
                                      int first_vertex,
                                      int n_vertices,
                                      CoglAttribute **attributes,
                                      int n_attributes,
                                      CoglDrawFlags flags)
{
  _cogl_flush_attributes_state (framebuffer, pipeline, flags,
                                attributes, n_attributes);

  GE (framebuffer->context,
      glDrawArrays ((GLenum)mode, first_vertex, n_vertices));
}

static size_t
sizeof_index_type (CoglIndicesType type)
{
  switch (type)
    {
    case COGL_INDICES_TYPE_UNSIGNED_BYTE:
      return 1;
    case COGL_INDICES_TYPE_UNSIGNED_SHORT:
      return 2;
    case COGL_INDICES_TYPE_UNSIGNED_INT:
      return 4;
    }
  g_return_val_if_reached (0);
}

void
_cogl_framebuffer_gl_draw_indexed_attributes (CoglFramebuffer *framebuffer,
                                              CoglPipeline *pipeline,
                                              CoglVerticesMode mode,
                                              int first_vertex,
                                              int n_vertices,
                                              CoglIndices *indices,
                                              CoglAttribute **attributes,
                                              int n_attributes,
                                              CoglDrawFlags flags)
{
  CoglBuffer *buffer;
  uint8_t *base;
  size_t buffer_offset;
  size_t index_size;
  GLenum indices_gl_type = 0;

  _cogl_flush_attributes_state (framebuffer, pipeline, flags,
                                attributes, n_attributes);

  buffer = COGL_BUFFER (cogl_indices_get_buffer (indices));
  base = _cogl_buffer_gl_bind (buffer, COGL_BUFFER_BIND_TARGET_INDEX_BUFFER);
  buffer_offset = cogl_indices_get_offset (indices);
  index_size = sizeof_index_type (cogl_indices_get_type (indices));

  switch (cogl_indices_get_type (indices))
    {
    case COGL_INDICES_TYPE_UNSIGNED_BYTE:
      indices_gl_type = GL_UNSIGNED_BYTE;
      break;
    case COGL_INDICES_TYPE_UNSIGNED_SHORT:
      indices_gl_type = GL_UNSIGNED_SHORT;
      break;
    case COGL_INDICES_TYPE_UNSIGNED_INT:
      indices_gl_type = GL_UNSIGNED_INT;
      break;
    }

  GE (framebuffer->context,
      glDrawElements ((GLenum)mode,
                      n_vertices,
                      indices_gl_type,
                      base + buffer_offset + index_size * first_vertex));

  _cogl_buffer_gl_unbind (buffer);
}
