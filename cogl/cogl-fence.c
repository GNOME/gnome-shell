/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Collabora Ltd.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-fence.h"
#include "cogl-fence-private.h"
#include "cogl-context-private.h"
#include "cogl-winsys-private.h"

#define FENCE_CHECK_TIMEOUT 5000 /* microseconds */

void *
cogl_fence_closure_get_user_data (CoglFenceClosure *closure)
{
  return closure->user_data;
}

static void
_cogl_fence_check (CoglFenceClosure *fence)
{
  CoglContext *context = fence->framebuffer->context;

  if (fence->type == FENCE_TYPE_WINSYS)
    {
      const CoglWinsysVtable *winsys = _cogl_context_get_winsys (context);
      CoglBool ret;

      ret = winsys->fence_is_complete (context, fence->fence_obj);
      if (!ret)
        return;
    }
#ifdef GL_ARB_sync
  else if (fence->type == FENCE_TYPE_GL_ARB)
    {
      GLenum arb;

      arb = context->glClientWaitSync (fence->fence_obj,
                                       GL_SYNC_FLUSH_COMMANDS_BIT,
                                       0);
      if (arb != GL_ALREADY_SIGNALED && arb != GL_CONDITION_SATISFIED)
        return;
    }
#endif

  fence->callback (NULL, /* dummy CoglFence object */
                   fence->user_data);
  cogl_framebuffer_cancel_fence_callback (fence->framebuffer, fence);
}

static void
_cogl_fence_poll_dispatch (void *source, int revents)
{
  CoglContext *context = source;
  CoglFenceClosure *fence, *next;

  COGL_TAILQ_FOREACH_SAFE (fence, &context->fences, list, next)
    _cogl_fence_check (fence);
}

static int64_t
_cogl_fence_poll_prepare (void *source)
{
  CoglContext *context = source;
  GList *l;

  /* If there are any pending fences in any of the journals then we
   * need to flush the journal otherwise the fence will never be
   * hit and the main loop might block forever */
  for (l = context->framebuffers; l; l = l->next)
    {
      CoglFramebuffer *fb = l->data;

      if (!COGL_TAILQ_EMPTY (&fb->journal->pending_fences))
        _cogl_framebuffer_flush_journal (fb);
    }

  if (!COGL_TAILQ_EMPTY (&context->fences))
    return FENCE_CHECK_TIMEOUT;
  else
    return -1;
}

void
_cogl_fence_submit (CoglFenceClosure *fence)
{
  CoglContext *context = fence->framebuffer->context;
  const CoglWinsysVtable *winsys = _cogl_context_get_winsys (context);

  fence->type = FENCE_TYPE_ERROR;

  if (winsys->fence_add)
    {
      fence->fence_obj = winsys->fence_add (context);
      if (fence->fence_obj)
        {
          fence->type = FENCE_TYPE_WINSYS;
          goto done;
        }
    }

#ifdef GL_ARB_sync
  if (context->glFenceSync)
    {
      fence->fence_obj = context->glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE,
                                               0);
      if (fence->fence_obj)
        {
          fence->type = FENCE_TYPE_GL_ARB;
          goto done;
        }
    }
#endif

 done:
  COGL_TAILQ_INSERT_TAIL (&context->fences, fence, list);

  if (!context->fences_poll_source)
    {
      context->fences_poll_source =
        _cogl_poll_renderer_add_source (context->display->renderer,
                                        _cogl_fence_poll_prepare,
                                        _cogl_fence_poll_dispatch,
                                        context);
    }
}

CoglFenceClosure *
cogl_framebuffer_add_fence_callback (CoglFramebuffer *framebuffer,
                                     CoglFenceCallback callback,
                                     void *user_data)
{
  CoglContext *context = framebuffer->context;
  CoglJournal *journal = framebuffer->journal;
  CoglFenceClosure *fence;

  if (!COGL_FLAGS_GET (context->features, COGL_FEATURE_ID_FENCE))
    return NULL;

  fence = g_slice_new (CoglFenceClosure);
  fence->framebuffer = framebuffer;
  fence->callback = callback;
  fence->user_data = user_data;
  fence->fence_obj = NULL;

  if (journal->entries->len)
    {
      COGL_TAILQ_INSERT_TAIL (&journal->pending_fences, fence, list);
      fence->type = FENCE_TYPE_PENDING;
    }
  else
    _cogl_fence_submit (fence);

  return fence;
}

void
cogl_framebuffer_cancel_fence_callback (CoglFramebuffer *framebuffer,
                                        CoglFenceClosure *fence)
{
  CoglJournal *journal = framebuffer->journal;
  CoglContext *context = framebuffer->context;

  if (fence->type == FENCE_TYPE_PENDING)
    {
      COGL_TAILQ_REMOVE (&journal->pending_fences, fence, list);
    }
  else
    {
      COGL_TAILQ_REMOVE (&context->fences, fence, list);

      if (fence->type == FENCE_TYPE_WINSYS)
        {
          const CoglWinsysVtable *winsys = _cogl_context_get_winsys (context);

          winsys->fence_destroy (context, fence->fence_obj);
        }
#ifdef GL_ARB_sync
      else if (fence->type == FENCE_TYPE_GL_ARB)
        {
          context->glDeleteSync (fence->fence_obj);
        }
#endif
    }

  g_slice_free (CoglFenceClosure, fence);
}

void
_cogl_fence_cancel_fences_for_framebuffer (CoglFramebuffer *framebuffer)
{
  CoglJournal *journal = framebuffer->journal;
  CoglContext *context = framebuffer->context;
  CoglFenceClosure *fence, *next;

  while (!COGL_TAILQ_EMPTY (&journal->pending_fences))
    {
      fence = COGL_TAILQ_FIRST (&journal->pending_fences);
      cogl_framebuffer_cancel_fence_callback (framebuffer, fence);
    }

  COGL_TAILQ_FOREACH_SAFE (fence, &context->fences, list, next)
    {
      if (fence->framebuffer == framebuffer)
        cogl_framebuffer_cancel_fence_callback (framebuffer, fence);
    }
}
