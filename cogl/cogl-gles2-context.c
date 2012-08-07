/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Intel Corporation.
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
 * Authors:
 *  Tomeu Vizoso <tomeu.vizoso@collabora.com>
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-gles2.h"
#include "cogl-gles2-context-private.h"

#include "cogl-context-private.h"
#include "cogl-display-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-renderer-private.h"
#include "cogl-swap-chain-private.h"
#include "cogl-texture-2d-private.h"

static void _cogl_gles2_context_free (CoglGLES2Context *gles2_context);

COGL_OBJECT_DEFINE (GLES2Context, gles2_context);

static CoglGLES2Context *current_gles2_context;

static CoglUserDataKey offscreen_wrapper_key;

enum {
  RESTORE_FB_NONE,
  RESTORE_FB_FROM_OFFSCREEN,
  RESTORE_FB_FROM_ONSCREEN,
};

GQuark
_cogl_gles2_context_error_quark (void)
{
  return g_quark_from_static_string ("cogl-gles2-context-error-quark");
}

/* We wrap glBindFramebuffer so that when framebuffer 0 is bound
 * we can instead bind the write_framebuffer passed to
 * cogl_push_gles2_context().
 */
static void
gl_bind_framebuffer_wrapper (GLenum target, GLuint framebuffer)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  gles2_ctx->current_fbo_handle = framebuffer;

  if (framebuffer == 0 && cogl_is_offscreen (gles2_ctx->write_buffer))
    {
      CoglGLES2Offscreen *write = gles2_ctx->gles2_write_buffer;
      framebuffer = write->gl_framebuffer.fbo_handle;
    }

  gles2_ctx->context->glBindFramebuffer (target, framebuffer);
}

static int
transient_bind_read_buffer (CoglGLES2Context *gles2_ctx)
{
  if (gles2_ctx->current_fbo_handle == 0)
    {
      if (cogl_is_offscreen (gles2_ctx->read_buffer))
        {
          CoglGLES2Offscreen *read = gles2_ctx->gles2_read_buffer;
          GLuint read_fbo_handle = read->gl_framebuffer.fbo_handle;

          gles2_ctx->context->glBindFramebuffer (GL_FRAMEBUFFER,
                                                 read_fbo_handle);

          return RESTORE_FB_FROM_OFFSCREEN;
        }
      else
        {
          _cogl_gl_framebuffer_bind (gles2_ctx->read_buffer,
                                     0 /* target ignored */);

          return RESTORE_FB_FROM_ONSCREEN;
        }
    }
  else
    return RESTORE_FB_NONE;
}

static void
restore_write_buffer (CoglGLES2Context *gles2_ctx,
                      int restore_mode)
{
  switch (restore_mode)
    {
    case RESTORE_FB_FROM_OFFSCREEN:

      gl_bind_framebuffer_wrapper (GL_FRAMEBUFFER, 0);

      break;
    case RESTORE_FB_FROM_ONSCREEN:

      /* Note: we can't restore the original write buffer using
       * _cogl_gl_framebuffer_bind() if it's an offscreen
       * framebuffer because _cogl_gl_framebuffer_bind() doesn't
       * know about the fbo handle owned by the gles2 context.
       */
      if (cogl_is_offscreen (gles2_ctx->write_buffer))
        gl_bind_framebuffer_wrapper (GL_FRAMEBUFFER, 0);
      else
        _cogl_gl_framebuffer_bind (gles2_ctx->write_buffer, GL_FRAMEBUFFER);

      break;
    case RESTORE_FB_NONE:
      break;
    }
}

/* We wrap glReadPixels so when framebuffer 0 is bound then we can
 * read from the read_framebuffer passed to cogl_push_gles2_context().
 */
static void
gl_read_pixels_wrapper (GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLenum format,
                        GLenum type,
                        GLvoid *pixels)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  int restore_mode = transient_bind_read_buffer (gles2_ctx);

  gles2_ctx->context->glReadPixels (x, y, width, height, format, type, pixels);

  restore_write_buffer (gles2_ctx, restore_mode);
}

static void
gl_copy_tex_image_2d_wrapper (GLenum target,
                              GLint level,
                              GLenum internalformat,
                              GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height,
                              GLint border)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  int restore_mode = transient_bind_read_buffer (gles2_ctx);

  gles2_ctx->context->glCopyTexImage2D (target, level, internalformat,
                                        x, y, width, height, border);

  restore_write_buffer (gles2_ctx, restore_mode);
}

static void
gl_copy_tex_sub_image_2d_wrapper (GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  int restore_mode = transient_bind_read_buffer (gles2_ctx);

  gles2_ctx->context->glCopyTexSubImage2D (target, level,
                                           xoffset, yoffset,
                                           x, y, width, height);

  restore_write_buffer (gles2_ctx, restore_mode);
}

static void
_cogl_gles2_offscreen_free (CoglGLES2Offscreen *gles2_offscreen)
{
  COGL_LIST_REMOVE (gles2_offscreen, list_node);
  g_slice_free (CoglGLES2Offscreen, gles2_offscreen);
}

static void
_cogl_gles2_context_free (CoglGLES2Context *gles2_context)
{
  CoglContext *ctx = gles2_context->context;
  const CoglWinsysVtable *winsys;

  winsys = ctx->display->renderer->winsys_vtable;
  winsys->destroy_gles2_context (gles2_context);

  while (gles2_context->foreign_offscreens.lh_first)
    {
      CoglGLES2Offscreen *gles2_offscreen =
        gles2_context->foreign_offscreens.lh_first;

      /* Note: this will also indirectly free the gles2_offscreen by
       * calling the destroy notify for the _user_data */
      cogl_object_set_user_data (COGL_OBJECT (gles2_offscreen->original_offscreen),
                                 &offscreen_wrapper_key,
                                 NULL,
                                 NULL);
    }

  cogl_object_unref (gles2_context->context);

  g_free (gles2_context->vtable);

  g_free (gles2_context);
}

CoglGLES2Context *
cogl_gles2_context_new (CoglContext *ctx, GError **error)
{
  CoglGLES2Context *gles2_ctx;
  const CoglWinsysVtable *winsys;

  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_GLES2_CONTEXT))
    {
      g_set_error (error, COGL_GLES2_CONTEXT_ERROR,
                   COGL_GLES2_CONTEXT_ERROR_UNSUPPORTED,
                   "Backend doesn't support creating GLES2 contexts");

      return NULL;
    }

  gles2_ctx = g_malloc0 (sizeof (CoglGLES2Context));

  cogl_object_ref (ctx);
  gles2_ctx->context = ctx;

  COGL_LIST_INIT (&gles2_ctx->foreign_offscreens);

  winsys = ctx->display->renderer->winsys_vtable;
  gles2_ctx->winsys = winsys->context_create_gles2_context (ctx, error);
  if (gles2_ctx->winsys == NULL)
    {
      cogl_object_unref (gles2_ctx->context);
      g_free (gles2_ctx);
      return NULL;
    }

  gles2_ctx->vtable = g_malloc0 (sizeof (CoglGLES2Vtable));
#define COGL_EXT_BEGIN(name, \
                       min_gl_major, min_gl_minor, \
                       gles_availability, \
                       extension_suffixes, extension_names)

#define COGL_EXT_FUNCTION(ret, name, args) \
  gles2_ctx->vtable->name = ctx->name;

#define COGL_EXT_END()

#include "gl-prototypes/cogl-gles2-functions.h"

#undef COGL_EXT_BEGIN
#undef COGL_EXT_FUNCTION
#undef COGL_EXT_END

  gles2_ctx->vtable->glBindFramebuffer = gl_bind_framebuffer_wrapper;
  gles2_ctx->vtable->glReadPixels = gl_read_pixels_wrapper;
  gles2_ctx->vtable->glCopyTexImage2D = gl_copy_tex_image_2d_wrapper;
  gles2_ctx->vtable->glCopyTexSubImage2D = gl_copy_tex_sub_image_2d_wrapper;

  return _cogl_gles2_context_object_new (gles2_ctx);
}

const CoglGLES2Vtable *
cogl_gles2_context_get_vtable (CoglGLES2Context *gles2_ctx)
{
  return gles2_ctx->vtable;
}

/* When drawing to a CoglFramebuffer from a separate context we have
 * to be able to allocate ancillary buffers for that context...
 */
static CoglGLES2Offscreen *
_cogl_gles2_offscreen_allocate (CoglOffscreen *offscreen,
                                CoglGLES2Context *gles2_context,
                                GError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (offscreen);
  const CoglWinsysVtable *winsys;
  GError *internal_error = NULL;
  CoglGLES2Offscreen *gles2_offscreen;

  if (!framebuffer->allocated &&
      !cogl_framebuffer_allocate (framebuffer, error))
    {
      return NULL;
    }

  for (gles2_offscreen = gles2_context->foreign_offscreens.lh_first;
       gles2_offscreen;
       gles2_offscreen = gles2_offscreen->list_node.le_next)
    {
      if (gles2_offscreen->original_offscreen == offscreen)
        return gles2_offscreen;
    }

  winsys = _cogl_framebuffer_get_winsys (framebuffer);
  winsys->save_context (framebuffer->context);
  if (!winsys->set_gles2_context (gles2_context, &internal_error))
    {
      winsys->restore_context (framebuffer->context);

      g_error_free (internal_error);
      g_set_error (error, COGL_FRAMEBUFFER_ERROR,
                   COGL_FRAMEBUFFER_ERROR_ALLOCATE,
                   "Failed to bind gles2 context to create framebuffer");
      return NULL;
    }

  gles2_offscreen = g_slice_new0 (CoglGLES2Offscreen);
  if (!_cogl_framebuffer_try_creating_gl_fbo (gles2_context->context,
                                              offscreen->texture,
                                              offscreen->texture_level,
                                              offscreen->texture_level_width,
                                              offscreen->texture_level_height,
                                              &COGL_FRAMEBUFFER (offscreen)->config,
                                              offscreen->allocation_flags,
                                              &gles2_offscreen->gl_framebuffer))
    {
      winsys->restore_context (framebuffer->context);

      g_slice_free (CoglGLES2Offscreen, gles2_offscreen);

      g_set_error (error, COGL_FRAMEBUFFER_ERROR,
                   COGL_FRAMEBUFFER_ERROR_ALLOCATE,
                   "Failed to create an OpenGL framebuffer object");
      return NULL;
    }

  winsys->restore_context (framebuffer->context);

  gles2_offscreen->original_offscreen = offscreen;

  COGL_LIST_INSERT_HEAD (&gles2_context->foreign_offscreens,
                         gles2_offscreen,
                         list_node);

  /* So we avoid building up an ever growing collection of ancillary
   * buffers for wrapped framebuffers, we make sure that the wrappers
   * get freed when the original offscreen framebuffer is freed. */
  cogl_object_set_user_data (COGL_OBJECT (framebuffer),
                             &offscreen_wrapper_key,
                             gles2_offscreen,
                             (CoglUserDataDestroyCallback)
                                _cogl_gles2_offscreen_free);

  return gles2_offscreen;
}

CoglBool
cogl_push_gles2_context (CoglContext *ctx,
                         CoglGLES2Context *gles2_ctx,
                         CoglFramebuffer *read_buffer,
                         CoglFramebuffer *write_buffer,
                         GError **error)
{
  const CoglWinsysVtable *winsys = ctx->display->renderer->winsys_vtable;
  GError *internal_error = NULL;

  _COGL_RETURN_VAL_IF_FAIL (gles2_ctx != NULL, FALSE);

  /* The read/write buffers are properties of the gles2 context and we
   * don't currently track the read/write buffers as part of the stack
   * entries so we explicitly don't allow the same context to be
   * pushed multiple times. */
  if (g_queue_find (&ctx->gles2_context_stack, gles2_ctx))
    {
      g_critical ("Pushing the same GLES2 context multiple times isn't "
                  "supported");
      return FALSE;
    }

  if (ctx->gles2_context_stack.length == 0)
    {
      _cogl_journal_flush (read_buffer->journal);
      if (write_buffer != read_buffer)
        _cogl_journal_flush (write_buffer->journal);
      winsys->save_context (ctx);
    }
  else
    gles2_ctx->vtable->glFlush ();

  if (gles2_ctx->read_buffer != read_buffer)
    {
      if (cogl_is_offscreen (read_buffer))
        {
          gles2_ctx->gles2_read_buffer =
            _cogl_gles2_offscreen_allocate (COGL_OFFSCREEN (read_buffer),
                                            gles2_ctx,
                                            error);
          /* XXX: what consistency guarantees should this api have?
           *
           * It should be safe to return at this point but we provide
           * no guarantee to the caller whether their given buffers
           * may be referenced and old buffers unreferenced even
           * if the _push fails. */
          if (!gles2_ctx->gles2_read_buffer)
            return FALSE;
        }
      else
        gles2_ctx->gles2_read_buffer = NULL;
      if (gles2_ctx->read_buffer)
        cogl_object_unref (gles2_ctx->read_buffer);
      gles2_ctx->read_buffer = cogl_object_ref (read_buffer);
    }

  if (gles2_ctx->write_buffer != write_buffer)
    {
      if (cogl_is_offscreen (write_buffer))
        {
          gles2_ctx->gles2_write_buffer =
            _cogl_gles2_offscreen_allocate (COGL_OFFSCREEN (write_buffer),
                                            gles2_ctx,
                                            error);
          /* XXX: what consistency guarantees should this api have?
           *
           * It should be safe to return at this point but we provide
           * no guarantee to the caller whether their given buffers
           * may be referenced and old buffers unreferenced even
           * if the _push fails. */
          if (!gles2_ctx->gles2_write_buffer)
            return FALSE;
        }
      else
        gles2_ctx->gles2_write_buffer = NULL;
      if (gles2_ctx->write_buffer)
        cogl_object_unref (gles2_ctx->write_buffer);
      gles2_ctx->write_buffer = cogl_object_ref (write_buffer);
    }

  if (!winsys->set_gles2_context (gles2_ctx, &internal_error))
    {
      winsys->restore_context (ctx);

      g_error_free (internal_error);
      g_set_error (error, COGL_GLES2_CONTEXT_ERROR,
                   COGL_GLES2_CONTEXT_ERROR_DRIVER,
                   "Driver failed to make GLES2 context current");
      return FALSE;
    }

  g_queue_push_tail (&ctx->gles2_context_stack, gles2_ctx);

  /* The last time this context was pushed may have been with a
   * different offscreen draw framebuffer and so if GL framebuffer 0
   * is bound for this GLES2 context we may need to bind a new,
   * corresponding, window system framebuffer... */
  if (gles2_ctx->current_fbo_handle == 0)
    {
      if (cogl_is_offscreen (gles2_ctx->write_buffer))
        {
          CoglGLES2Offscreen *write = gles2_ctx->gles2_write_buffer;
          GLuint handle = write->gl_framebuffer.fbo_handle;
          gles2_ctx->context->glBindFramebuffer (GL_FRAMEBUFFER, handle);
        }
    }

  current_gles2_context = gles2_ctx;

  /* If this is the first time this gles2 context has been used then
   * we'll force the viewport and scissor to the right size. GL has
   * the semantics that the viewport and scissor default to the size
   * of the first surface the context is used with. If the first
   * CoglFramebuffer that this context is used with is an offscreen,
   * then the surface from GL's point of view will be the 1x1 dummy
   * surface so the viewport will be wrong. Therefore we just override
   * the default viewport and scissor here */
  if (!gles2_ctx->has_been_bound)
    {
      int fb_width = cogl_framebuffer_get_width (write_buffer);
      int fb_height = cogl_framebuffer_get_height (write_buffer);

      gles2_ctx->vtable->glViewport (0, 0, /* x/y */
                                     fb_width, fb_height);
      gles2_ctx->vtable->glScissor (0, 0, /* x/y */
                                    fb_width, fb_height);
      gles2_ctx->has_been_bound = TRUE;
    }

  return TRUE;
}

CoglGLES2Vtable *
cogl_gles2_get_current_vtable (void)
{
  return current_gles2_context ? current_gles2_context->vtable : NULL;
}

void
cogl_pop_gles2_context (CoglContext *ctx)
{
  CoglGLES2Context *gles2_ctx;
  const CoglWinsysVtable *winsys = ctx->display->renderer->winsys_vtable;

  _COGL_RETURN_IF_FAIL (ctx->gles2_context_stack.length > 0);

  g_queue_pop_tail (&ctx->gles2_context_stack);

  gles2_ctx = g_queue_peek_tail (&ctx->gles2_context_stack);

  if (gles2_ctx)
    {
      winsys->set_gles2_context (gles2_ctx, NULL);
      current_gles2_context = gles2_ctx;
    }
  else
    {
      winsys->restore_context (ctx);
      current_gles2_context = NULL;
    }
}

CoglTexture2D *
cogl_gles2_texture_2d_new_from_handle (CoglContext *ctx,
                                       CoglGLES2Context *gles2_ctx,
                                       unsigned int handle,
                                       int width,
                                       int height,
                                       CoglPixelFormat internal_format,
                                       GError **error)
{
  return cogl_texture_2d_new_from_foreign (ctx,
                                           handle,
                                           width,
                                           height,
                                           internal_format,
                                           error);
}

CoglBool
cogl_gles2_texture_get_handle (CoglTexture *texture,
                               unsigned int *handle,
                               unsigned int *target)
{
  return cogl_texture_get_gl_texture (texture, handle, target);
}
