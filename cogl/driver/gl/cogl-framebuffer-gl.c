/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2012 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-framebuffer-gl-private.h"
#include "cogl-buffer-gl-private.h"
#include "cogl-error-private.h"
#include "cogl-texture-gl-private.h"
#include "cogl-texture-private.h"

#include <glib.h>
#include <string.h>

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
#ifndef GL_DEPTH_STENCIL_ATTACHMENT
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
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

#ifndef GL_COLOR
#define GL_COLOR 0x1800
#endif
#ifndef GL_DEPTH
#define GL_DEPTH 0x1801
#endif
#ifndef GL_STENCIL
#define GL_STENCIL 0x1802
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
  _cogl_clip_stack_flush (framebuffer->clip_stack, framebuffer);
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

      /* Initialise the glDrawBuffer state the first time the context
       * is bound to the default framebuffer. If the winsys is using a
       * surfaceless context for the initial make current then the
       * default draw buffer will be GL_NONE so we need to correct
       * that. We can't do it any earlier because binding GL_BACK when
       * there is no default framebuffer won't work */
      if (!ctx->was_bound_to_onscreen)
        {
          if (ctx->glDrawBuffer)
            {
              GE (ctx, glDrawBuffer (GL_BACK));
            }
          else if (ctx->glDrawBuffers)
            {
              /* glDrawBuffer isn't available on GLES 3.0 so we need
               * to be able to use glDrawBuffers as well. On GLES 2
               * neither is available but the state should always be
               * GL_BACK anyway so we don't need to set anything. On
               * desktop GL this must be GL_BACK_LEFT instead of
               * GL_BACK but as this code path will only be hit for
               * GLES we can just use GL_BACK. */
              static const GLenum buffers[] = { GL_BACK };

              GE (ctx, glDrawBuffers (G_N_ELEMENTS (buffers), buffers));
            }

          ctx->was_bound_to_onscreen = TRUE;
        }
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
          _COGL_RETURN_IF_FAIL (_cogl_has_private_feature
                                (ctx, COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT));
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
        case COGL_FRAMEBUFFER_STATE_INDEX_DEPTH_WRITE:
          /* Nothing to do for depth write state change; the state will always
           * be taken into account when flushing the pipeline's depth state. */
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
  CoglTexture2D *depth_texture =
    cogl_texture_2d_new_with_size (ctx, width, height);

  cogl_texture_set_components (COGL_TEXTURE (depth_texture),
                               COGL_TEXTURE_COMPONENTS_DEPTH);

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
      g_assert (_cogl_texture_get_format (depth_texture) ==
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
      g_assert (_cogl_texture_get_format (depth_texture) ==
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

      /* WebGL adds a GL_DEPTH_STENCIL_ATTACHMENT and requires that we
       * use the GL_DEPTH_STENCIL format. */
#ifdef HAVE_COGL_WEBGL
      format = GL_DEPTH_STENCIL;
#else
      /* Although GL_OES_packed_depth_stencil is mostly equivalent to
       * GL_EXT_packed_depth_stencil, one notable difference is that
       * GL_OES_packed_depth_stencil doesn't allow GL_DEPTH_STENCIL to
       * be passed as an internal format to glRenderbufferStorage.
       */
      if (_cogl_has_private_feature
          (ctx, COGL_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL))
        format = GL_DEPTH_STENCIL;
      else
        {
          _COGL_RETURN_VAL_IF_FAIL (
            _cogl_has_private_feature (ctx,
              COGL_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL),
            NULL);
          format = GL_DEPTH24_STENCIL8;
        }
#endif

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


#ifdef HAVE_COGL_WEBGL
      GE (ctx, glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                          GL_DEPTH_STENCIL_ATTACHMENT,
                                          GL_RENDERBUFFER,
                                          gl_depth_stencil_handle));
#else
      GE (ctx, glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                          GL_STENCIL_ATTACHMENT,
                                          GL_RENDERBUFFER,
                                          gl_depth_stencil_handle));
      GE (ctx, glFramebufferRenderbuffer (GL_FRAMEBUFFER,
                                          GL_DEPTH_ATTACHMENT,
                                          GL_RENDERBUFFER,
                                          gl_depth_stencil_handle));
#endif
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
  int level_width;
  int level_height;

  _COGL_RETURN_VAL_IF_FAIL (offscreen->texture_level <
                            _cogl_texture_get_n_levels (offscreen->texture),
                            NULL);

  _cogl_texture_get_level_size (offscreen->texture,
                                offscreen->texture_level,
                                &level_width,
                                &level_height,
                                NULL);

  if (fb->config.depth_texture_enabled &&
      offscreen->depth_texture == NULL)
    {
      offscreen->depth_texture =
        create_depth_texture (ctx,
                              level_width,
                              level_height);

      if (!cogl_texture_allocate (offscreen->depth_texture, error))
        {
          cogl_object_unref (offscreen->depth_texture);
          offscreen->depth_texture = NULL;
          return FALSE;
        }

      _cogl_texture_associate_framebuffer (offscreen->depth_texture, fb);
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
                         level_width,
                         level_height,
                         offscreen->depth_texture,
                         &fb->config,
                         flags = 0,
                         gl_framebuffer)) ||

      (ctx->have_last_offscreen_allocate_flags &&
       try_creating_fbo (ctx,
                         offscreen->texture,
                         offscreen->texture_level,
                         level_width,
                         level_height,
                         offscreen->depth_texture,
                         &fb->config,
                         flags = ctx->last_offscreen_allocate_flags,
                         gl_framebuffer)) ||

      (
       /* NB: WebGL introduces a DEPTH_STENCIL_ATTACHMENT and doesn't
        * need an extension to handle _FLAG_DEPTH_STENCIL */
#ifndef HAVE_COGL_WEBGL
       (_cogl_has_private_feature
        (ctx, COGL_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL) ||
        _cogl_has_private_feature
        (ctx, COGL_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL)) &&
#endif
       try_creating_fbo (ctx,
                         offscreen->texture,
                         offscreen->texture_level,
                         level_width,
                         level_height,
                         offscreen->depth_texture,
                         &fb->config,
                         flags = COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL,
                         gl_framebuffer)) ||

      try_creating_fbo (ctx,
                        offscreen->texture,
                        offscreen->texture_level,
                        level_width,
                        level_height,
                        offscreen->depth_texture,
                        &fb->config,
                        flags = COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH |
                        COGL_OFFSCREEN_ALLOCATE_FLAG_STENCIL,
                        gl_framebuffer) ||

      try_creating_fbo (ctx,
                        offscreen->texture,
                        offscreen->texture_level,
                        level_width,
                        level_height,
                        offscreen->depth_texture,
                        &fb->config,
                        flags = COGL_OFFSCREEN_ALLOCATE_FLAG_STENCIL,
                        gl_framebuffer) ||

      try_creating_fbo (ctx,
                        offscreen->texture,
                        offscreen->texture_level,
                        level_width,
                        level_height,
                        offscreen->depth_texture,
                        &fb->config,
                        flags = COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH,
                        gl_framebuffer) ||

      try_creating_fbo (ctx,
                        offscreen->texture,
                        offscreen->texture_level,
                        level_width,
                        level_height,
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
    {
      gl_buffers |= GL_DEPTH_BUFFER_BIT;

      if (ctx->depth_writing_enabled_cache != framebuffer->depth_writing_enabled)
        {
          GE( ctx, glDepthMask (framebuffer->depth_writing_enabled));

          ctx->depth_writing_enabled_cache = framebuffer->depth_writing_enabled;

          /* Make sure the DepthMask is updated when the next primitive is drawn */
          ctx->current_pipeline_changes_since_flush |=
            COGL_PIPELINE_STATE_DEPTH;
          ctx->current_pipeline_age--;
        }
    }

  if (buffers & COGL_BUFFER_BIT_STENCIL)
    gl_buffers |= GL_STENCIL_BUFFER_BIT;


  GE (ctx, glClear (gl_buffers));
}

static inline void
_cogl_framebuffer_init_bits (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;

  if (G_LIKELY (!framebuffer->dirty_bitmasks))
    return;

  cogl_framebuffer_allocate (framebuffer, NULL);

  _cogl_framebuffer_flush_state (framebuffer,
                                 framebuffer,
                                 COGL_FRAMEBUFFER_STATE_BIND);

#ifdef HAVE_COGL_GL
  if (_cogl_has_private_feature
      (ctx, COGL_PRIVATE_FEATURE_QUERY_FRAMEBUFFER_BITS) &&
      framebuffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN)
    {
      static const struct
      {
        GLenum attachment, pname;
        size_t offset;
      } params[] =
          {
            { GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,
              offsetof (CoglFramebufferBits, red) },
            { GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
              offsetof (CoglFramebufferBits, green) },
            { GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,
              offsetof (CoglFramebufferBits, blue) },
            { GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
              offsetof (CoglFramebufferBits, alpha) },
            { GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,
              offsetof (CoglFramebufferBits, depth) },
            { GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,
              offsetof (CoglFramebufferBits, stencil) },
          };
      int i;

      for (i = 0; i < G_N_ELEMENTS (params); i++)
        {
          int *value =
            (int *) ((uint8_t *) &framebuffer->bits + params[i].offset);
          GE( ctx, glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER,
                                                          params[i].attachment,
                                                          params[i].pname,
                                                          value) );
        }
    }
  else
#endif /* HAVE_COGL_GL */
    {
      GE( ctx, glGetIntegerv (GL_RED_BITS,   &framebuffer->bits.red)   );
      GE( ctx, glGetIntegerv (GL_GREEN_BITS, &framebuffer->bits.green) );
      GE( ctx, glGetIntegerv (GL_BLUE_BITS,  &framebuffer->bits.blue)  );
      GE( ctx, glGetIntegerv (GL_ALPHA_BITS, &framebuffer->bits.alpha) );
      GE( ctx, glGetIntegerv (GL_DEPTH_BITS, &framebuffer->bits.depth) );
      GE( ctx, glGetIntegerv (GL_STENCIL_BITS, &framebuffer->bits.stencil) );
    }

  /* If we don't have alpha textures then the alpha bits are actually
   * stored in the red component */
  if (!_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_ALPHA_TEXTURES) &&
      framebuffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN &&
      framebuffer->internal_format == COGL_PIXEL_FORMAT_A_8)
    {
      framebuffer->bits.alpha = framebuffer->bits.red;
      framebuffer->bits.red = 0;
    }

  COGL_NOTE (OFFSCREEN,
             "RGBA/D/S Bits for framebuffer[%p, %s]: %d, %d, %d, %d, %d, %d",
             framebuffer,
             framebuffer->type == COGL_FRAMEBUFFER_TYPE_OFFSCREEN
               ? "offscreen"
               : "onscreen",
             framebuffer->bits.red,
             framebuffer->bits.blue,
             framebuffer->bits.green,
             framebuffer->bits.alpha,
             framebuffer->bits.depth,
             framebuffer->bits.stencil);

  framebuffer->dirty_bitmasks = FALSE;
}

void
_cogl_framebuffer_gl_query_bits (CoglFramebuffer *framebuffer,
                                 CoglFramebufferBits *bits)
{
  _cogl_framebuffer_init_bits (framebuffer);

  /* TODO: cache these in some driver specific location not
   * directly as part of CoglFramebuffer. */
  *bits = framebuffer->bits;
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
  CoglContext *ctx = framebuffer->context;

  if (ctx->glDiscardFramebuffer)
    {
      GLenum attachments[3];
      int i = 0;

      if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
        {
          if (buffers & COGL_BUFFER_BIT_COLOR)
            attachments[i++] = GL_COLOR;
          if (buffers & COGL_BUFFER_BIT_DEPTH)
            attachments[i++] = GL_DEPTH;
          if (buffers & COGL_BUFFER_BIT_STENCIL)
            attachments[i++] = GL_STENCIL;
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

      _cogl_framebuffer_flush_state (framebuffer,
                                     framebuffer,
                                     COGL_FRAMEBUFFER_STATE_BIND);
      GE (ctx, glDiscardFramebuffer (GL_FRAMEBUFFER, i, attachments));
    }
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

  /* Note: we don't try and catch errors with binding the index buffer
   * here since OOM errors at this point indicate that nothing has yet
   * been uploaded to the indices buffer which we consider to be a
   * programmer error.
   */
  base = _cogl_buffer_gl_bind (buffer,
                               COGL_BUFFER_BIND_TARGET_INDEX_BUFFER, NULL);
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

static CoglBool
mesa_46631_slow_read_pixels_workaround (CoglFramebuffer *framebuffer,
                                        int x,
                                        int y,
                                        CoglReadPixelsFlags source,
                                        CoglBitmap *bitmap,
                                        CoglError **error)
{
  CoglContext *ctx;
  CoglPixelFormat format;
  CoglBitmap *pbo;
  int width;
  int height;
  CoglBool res;
  uint8_t *dst;
  const uint8_t *src;

  ctx = cogl_framebuffer_get_context (framebuffer);

  width = cogl_bitmap_get_width (bitmap);
  height = cogl_bitmap_get_height (bitmap);
  format = cogl_bitmap_get_format (bitmap);

  pbo = cogl_bitmap_new_with_size (ctx, width, height, format);

  /* Read into the pbo. We need to disable the flipping because the
     blit fast path in the driver does not work with
     GL_PACK_INVERT_MESA is set */
  res = _cogl_framebuffer_read_pixels_into_bitmap (framebuffer,
                                                   x, y,
                                                   source |
                                                   COGL_READ_PIXELS_NO_FLIP,
                                                   pbo,
                                                   error);
  if (!res)
    {
      cogl_object_unref (pbo);
      return FALSE;
    }

  /* Copy the pixels back into application's buffer */
  dst = _cogl_bitmap_map (bitmap,
                          COGL_BUFFER_ACCESS_WRITE,
                          COGL_BUFFER_MAP_HINT_DISCARD,
                          error);
  if (!dst)
    {
      cogl_object_unref (pbo);
      return FALSE;
    }

  src = _cogl_bitmap_map (pbo,
                          COGL_BUFFER_ACCESS_READ,
                          0, /* hints */
                          error);
  if (src)
    {
      int src_rowstride = cogl_bitmap_get_rowstride (pbo);
      int dst_rowstride = cogl_bitmap_get_rowstride (bitmap);
      int to_copy =
        _cogl_pixel_format_get_bytes_per_pixel (format) * width;
      int y;

      /* If the framebuffer is onscreen we need to flip the
         data while copying */
      if (!cogl_is_offscreen (framebuffer))
        {
          src += src_rowstride * (height - 1);
          src_rowstride = -src_rowstride;
        }

      for (y = 0; y < height; y++)
        {
          memcpy (dst, src, to_copy);
          dst += dst_rowstride;
          src += src_rowstride;
        }

      _cogl_bitmap_unmap (pbo);
    }
  else
    res = FALSE;

  _cogl_bitmap_unmap (bitmap);

  cogl_object_unref (pbo);

  return res;
}

CoglBool
_cogl_framebuffer_gl_read_pixels_into_bitmap (CoglFramebuffer *framebuffer,
                                              int x,
                                              int y,
                                              CoglReadPixelsFlags source,
                                              CoglBitmap *bitmap,
                                              CoglError **error)
{
  CoglContext *ctx = framebuffer->context;
  int framebuffer_height = cogl_framebuffer_get_height (framebuffer);
  int width = cogl_bitmap_get_width (bitmap);
  int height = cogl_bitmap_get_height (bitmap);
  CoglPixelFormat format = cogl_bitmap_get_format (bitmap);
  CoglPixelFormat required_format;
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;
  CoglBool pack_invert_set;
  int status = FALSE;

  /* Workaround for cases where its faster to read into a temporary
   * PBO. This is only worth doing if:
   *
   * • The GPU is an Intel GPU. In that case there is a known
   *   fast-path when reading into a PBO that will use the blitter
   *   instead of the Mesa fallback code. The driver bug will only be
   *   set if this is the case.
   * • We're not already reading into a PBO.
   * • The target format is BGRA. The fast-path blit does not get hit
   *   otherwise.
   * • The size of the data is not trivially small. This isn't a
   *   requirement to hit the fast-path blit but intuitively it feels
   *   like if the amount of data is too small then the cost of
   *   allocating a PBO will outweigh the cost of temporarily
   *   converting the data to floats.
   */
  if ((ctx->gpu.driver_bugs &
       COGL_GPU_INFO_DRIVER_BUG_MESA_46631_SLOW_READ_PIXELS) &&
      (width > 8 || height > 8) &&
      (format & ~COGL_PREMULT_BIT) == COGL_PIXEL_FORMAT_BGRA_8888 &&
      cogl_bitmap_get_buffer (bitmap) == NULL)
    {
      CoglError *ignore_error = NULL;

      if (mesa_46631_slow_read_pixels_workaround (framebuffer,
                                                  x, y,
                                                  source,
                                                  bitmap,
                                                  &ignore_error))
        return TRUE;
      else
        cogl_error_free (ignore_error);
    }

  _cogl_framebuffer_flush_state (framebuffer,
                                 framebuffer,
                                 COGL_FRAMEBUFFER_STATE_BIND);

  /* The y co-ordinate should be given in OpenGL's coordinate system
   * so 0 is the bottom row
   *
   * NB: all offscreen rendering is done upside down so no conversion
   * is necissary in this case.
   */
  if (!cogl_is_offscreen (framebuffer))
    y = framebuffer_height - y - height;

  required_format = ctx->driver_vtable->pixel_format_to_gl (ctx,
                                                            format,
                                                            &gl_intformat,
                                                            &gl_format,
                                                            &gl_type);

  /* NB: All offscreen rendering is done upside down so there is no need
   * to flip in this case... */
  if (_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_MESA_PACK_INVERT) &&
      (source & COGL_READ_PIXELS_NO_FLIP) == 0 &&
      !cogl_is_offscreen (framebuffer))
    {
      GE (ctx, glPixelStorei (GL_PACK_INVERT_MESA, TRUE));
      pack_invert_set = TRUE;
    }
  else
    pack_invert_set = FALSE;

  /* Under GLES only GL_RGBA with GL_UNSIGNED_BYTE as well as an
     implementation specific format under
     GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES and
     GL_IMPLEMENTATION_COLOR_READ_TYPE_OES is supported. We could try
     to be more clever and check if the requested type matches that
     but we would need some reliable functions to convert from GL
     types to Cogl types. For now, lets just always read in
     GL_RGBA/GL_UNSIGNED_BYTE and convert if necessary. We also need
     to use this intermediate buffer if the rowstride has padding
     because GLES does not support setting GL_ROW_LENGTH */
  if ((!_cogl_has_private_feature
       (ctx, COGL_PRIVATE_FEATURE_READ_PIXELS_ANY_FORMAT) &&
       (gl_format != GL_RGBA || gl_type != GL_UNSIGNED_BYTE ||
        cogl_bitmap_get_rowstride (bitmap) != 4 * width)) ||
      (required_format & ~COGL_PREMULT_BIT) != (format & ~COGL_PREMULT_BIT))
    {
      CoglBitmap *tmp_bmp;
      CoglPixelFormat read_format;
      int bpp, rowstride;
      uint8_t *tmp_data;
      CoglBool succeeded;

      if (_cogl_has_private_feature
          (ctx, COGL_PRIVATE_FEATURE_READ_PIXELS_ANY_FORMAT))
        read_format = required_format;
      else
        {
          read_format = COGL_PIXEL_FORMAT_RGBA_8888;
          gl_format = GL_RGBA;
          gl_type = GL_UNSIGNED_BYTE;
        }

      if (COGL_PIXEL_FORMAT_CAN_HAVE_PREMULT (read_format))
        read_format = ((read_format & ~COGL_PREMULT_BIT) |
                       (framebuffer->internal_format & COGL_PREMULT_BIT));

      tmp_bmp = _cogl_bitmap_new_with_malloc_buffer (ctx,
                                                     width, height,
                                                     read_format,
                                                     error);
      if (!tmp_bmp)
        goto EXIT;

      bpp = _cogl_pixel_format_get_bytes_per_pixel (read_format);
      rowstride = cogl_bitmap_get_rowstride (tmp_bmp);

      ctx->texture_driver->prep_gl_for_pixels_download (ctx,
                                                        rowstride,
                                                        width,
                                                        bpp);

      /* Note: we don't worry about catching errors here since we know
       * we won't be lazily allocating storage for this buffer so it
       * won't fail due to lack of memory. */
      tmp_data = _cogl_bitmap_gl_bind (tmp_bmp,
                                       COGL_BUFFER_ACCESS_WRITE,
                                       COGL_BUFFER_MAP_HINT_DISCARD,
                                       NULL);

      GE( ctx, glReadPixels (x, y, width, height,
                             gl_format, gl_type,
                             tmp_data) );

      _cogl_bitmap_gl_unbind (tmp_bmp);

      succeeded = _cogl_bitmap_convert_into_bitmap (tmp_bmp, bitmap, error);

      cogl_object_unref (tmp_bmp);

      if (!succeeded)
        goto EXIT;
    }
  else
    {
      CoglBitmap *shared_bmp;
      CoglPixelFormat bmp_format;
      int bpp, rowstride;
      CoglBool succeeded = FALSE;
      uint8_t *pixels;
      CoglError *internal_error = NULL;

      rowstride = cogl_bitmap_get_rowstride (bitmap);

      /* We match the premultiplied state of the target buffer to the
       * premultiplied state of the framebuffer so that it will get
       * converted to the right format below */
      if (COGL_PIXEL_FORMAT_CAN_HAVE_PREMULT (format))
        bmp_format = ((format & ~COGL_PREMULT_BIT) |
                      (framebuffer->internal_format & COGL_PREMULT_BIT));
      else
        bmp_format = format;

      if (bmp_format != format)
        shared_bmp = _cogl_bitmap_new_shared (bitmap,
                                              bmp_format,
                                              width, height,
                                              rowstride);
      else
        shared_bmp = cogl_object_ref (bitmap);

      bpp = _cogl_pixel_format_get_bytes_per_pixel (bmp_format);

      ctx->texture_driver->prep_gl_for_pixels_download (ctx,
                                                        rowstride,
                                                        width,
                                                        bpp);

      pixels = _cogl_bitmap_gl_bind (shared_bmp,
                                     COGL_BUFFER_ACCESS_WRITE,
                                     0, /* hints */
                                     &internal_error);
      /* NB: _cogl_bitmap_gl_bind() can return NULL in sucessfull
       * cases so we have to explicitly check the cogl error pointer
       * to know if there was a problem */
      if (internal_error)
        {
          cogl_object_unref (shared_bmp);
          _cogl_propagate_error (error, internal_error);
          goto EXIT;
        }

      GE( ctx, glReadPixels (x, y,
                             width, height,
                             gl_format, gl_type,
                             pixels) );

      _cogl_bitmap_gl_unbind (shared_bmp);

      /* Convert to the premult format specified by the caller
         in-place. This will do nothing if the premult status is already
         correct. */
      if (_cogl_bitmap_convert_premult_status (shared_bmp, format, error))
        succeeded = TRUE;

      cogl_object_unref (shared_bmp);

      if (!succeeded)
        goto EXIT;
    }

  /* NB: All offscreen rendering is done upside down so there is no need
   * to flip in this case... */
  if (!cogl_is_offscreen (framebuffer) &&
      (source & COGL_READ_PIXELS_NO_FLIP) == 0 &&
      !pack_invert_set)
    {
      uint8_t *temprow;
      int rowstride;
      uint8_t *pixels;

      rowstride = cogl_bitmap_get_rowstride (bitmap);
      pixels = _cogl_bitmap_map (bitmap,
                                 COGL_BUFFER_ACCESS_READ |
                                 COGL_BUFFER_ACCESS_WRITE,
                                 0, /* hints */
                                 error);

      if (pixels == NULL)
        goto EXIT;

      temprow = g_alloca (rowstride * sizeof (uint8_t));

      /* vertically flip the buffer in-place */
      for (y = 0; y < height / 2; y++)
        {
          if (y != height - y - 1) /* skip center row */
            {
              memcpy (temprow,
                      pixels + y * rowstride, rowstride);
              memcpy (pixels + y * rowstride,
                      pixels + (height - y - 1) * rowstride, rowstride);
              memcpy (pixels + (height - y - 1) * rowstride,
                      temprow,
                      rowstride);
            }
        }

      _cogl_bitmap_unmap (bitmap);
    }

  status = TRUE;

EXIT:

  /* Currently this function owns the pack_invert state and we don't want this
   * to interfere with other Cogl components so all other code can assume that
   * we leave the pack_invert state off. */
  if (pack_invert_set)
    GE (ctx, glPixelStorei (GL_PACK_INVERT_MESA, FALSE));

  return status;
}
