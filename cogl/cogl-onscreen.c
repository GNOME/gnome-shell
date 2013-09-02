/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011, 2013 Intel Corporation.
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

#include "cogl-util.h"
#include "cogl-onscreen-private.h"
#include "cogl-frame-info-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl1-context.h"
#include "cogl-closure-list-private.h"
#include "cogl-poll-private.h"
#include "cogl-gtype-private.h"

static void _cogl_onscreen_free (CoglOnscreen *onscreen);

COGL_OBJECT_DEFINE_WITH_CODE (Onscreen, onscreen,
                              _cogl_onscreen_class.virt_unref =
                              _cogl_framebuffer_unref);
COGL_GTYPE_DEFINE_CLASS (Onscreen, onscreen,
                         COGL_GTYPE_IMPLEMENT_INTERFACE (framebuffer));

static void
_cogl_onscreen_init_from_template (CoglOnscreen *onscreen,
                                   CoglOnscreenTemplate *onscreen_template)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);

  _cogl_list_init (&onscreen->frame_closures);
  _cogl_list_init (&onscreen->resize_closures);
  _cogl_list_init (&onscreen->dirty_closures);

  framebuffer->config = onscreen_template->config;
  cogl_object_ref (framebuffer->config.swap_chain);
}

/* XXX: While we still have backend in Clutter we need a dummy object
 * to represent the CoglOnscreen framebuffer that the backend
 * creates... */
CoglOnscreen *
_cogl_onscreen_new (void)
{
  CoglOnscreen *onscreen = g_new0 (CoglOnscreen, 1);

  _COGL_GET_CONTEXT (ctx, NULL);

  _cogl_framebuffer_init (COGL_FRAMEBUFFER (onscreen),
                          ctx,
                          COGL_FRAMEBUFFER_TYPE_ONSCREEN,
                          0x1eadbeef, /* width */
                          0x1eadbeef); /* height */
  /* NB: make sure to pass positive width/height numbers here
   * because otherwise we'll hit input validation assertions!*/

  _cogl_onscreen_init_from_template (onscreen, ctx->display->onscreen_template);

  COGL_FRAMEBUFFER (onscreen)->allocated = TRUE;

  /* XXX: Note we don't initialize onscreen->winsys in this case. */

  return _cogl_onscreen_object_new (onscreen);
}

CoglOnscreen *
cogl_onscreen_new (CoglContext *ctx, int width, int height)
{
  CoglOnscreen *onscreen;

  /* FIXME: We are assuming onscreen buffers will always be
     premultiplied so we'll set the premult flag on the bitmap
     format. This will usually be correct because the result of the
     default blending operations for Cogl ends up with premultiplied
     data in the framebuffer. However it is possible for the
     framebuffer to be in whatever format depending on what
     CoglPipeline is used to render to it. Eventually we may want to
     add a way for an application to inform Cogl that the framebuffer
     is not premultiplied in case it is being used for some special
     purpose. */

  onscreen = g_new0 (CoglOnscreen, 1);
  _cogl_framebuffer_init (COGL_FRAMEBUFFER (onscreen),
                          ctx,
                          COGL_FRAMEBUFFER_TYPE_ONSCREEN,
                          width, /* width */
                          height); /* height */

  _cogl_onscreen_init_from_template (onscreen, ctx->display->onscreen_template);

  return _cogl_onscreen_object_new (onscreen);
}

static void
_cogl_onscreen_free (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  const CoglWinsysVtable *winsys = _cogl_framebuffer_get_winsys (framebuffer);
  CoglFrameInfo *frame_info;

  _cogl_closure_list_disconnect_all (&onscreen->resize_closures);
  _cogl_closure_list_disconnect_all (&onscreen->frame_closures);
  _cogl_closure_list_disconnect_all (&onscreen->dirty_closures);

  while ((frame_info = g_queue_pop_tail (&onscreen->pending_frame_infos)))
    cogl_object_unref (frame_info);
  g_queue_clear (&onscreen->pending_frame_infos);

  if (framebuffer->context->window_buffer == COGL_FRAMEBUFFER (onscreen))
    framebuffer->context->window_buffer = NULL;

  winsys->onscreen_deinit (onscreen);
  _COGL_RETURN_IF_FAIL (onscreen->winsys == NULL);

  /* Chain up to parent */
  _cogl_framebuffer_free (framebuffer);

  g_free (onscreen);
}

static void
notify_event (CoglOnscreen *onscreen,
              CoglFrameEvent event,
              CoglFrameInfo *info)
{
  _cogl_closure_list_invoke (&onscreen->frame_closures,
                             CoglFrameCallback,
                             onscreen, event, info);
}

static void
_cogl_dispatch_onscreen_cb (CoglContext *context)
{
  CoglOnscreenEvent *event, *tmp;
  CoglList queue;

  /* Dispatching the event callback may cause another frame to be
   * drawn which in may cause another event to be queued immediately.
   * To make sure this loop will only dispatch one set of events we'll
   * steal the queue and iterate that separately */
  _cogl_list_init (&queue);
  _cogl_list_insert_list (&queue, &context->onscreen_events_queue);
  _cogl_list_init (&context->onscreen_events_queue);

  _cogl_closure_disconnect (context->onscreen_dispatch_idle);
  context->onscreen_dispatch_idle = NULL;

  _cogl_list_for_each_safe (event, tmp, &queue, link)
    {
      CoglOnscreen *onscreen = event->onscreen;
      CoglFrameInfo *info = event->info;

      notify_event (onscreen, event->type, info);

      cogl_object_unref (onscreen);
      cogl_object_unref (info);

      g_slice_free (CoglOnscreenEvent, event);
    }

  while (!_cogl_list_empty (&context->onscreen_dirty_queue))
    {
      CoglOnscreenQueuedDirty *qe =
        _cogl_container_of (context->onscreen_dirty_queue.next,
                            CoglOnscreenQueuedDirty,
                            link);

      _cogl_list_remove (&qe->link);

      _cogl_closure_list_invoke (&qe->onscreen->dirty_closures,
                                 CoglOnscreenDirtyCallback,
                                 qe->onscreen,
                                 &qe->info);

      cogl_object_unref (qe->onscreen);

      g_slice_free (CoglOnscreenQueuedDirty, qe);
    }
}

static void
_cogl_onscreen_queue_dispatch_idle (CoglOnscreen *onscreen)
{
  CoglContext *ctx = COGL_FRAMEBUFFER (onscreen)->context;

  if (!ctx->onscreen_dispatch_idle)
    {
      ctx->onscreen_dispatch_idle =
        _cogl_poll_renderer_add_idle (ctx->display->renderer,
                                      (CoglIdleCallback)
                                      _cogl_dispatch_onscreen_cb,
                                      ctx,
                                      NULL);
    }
}

void
_cogl_onscreen_queue_dirty (CoglOnscreen *onscreen,
                            const CoglOnscreenDirtyInfo *info)
{
  CoglContext *ctx = COGL_FRAMEBUFFER (onscreen)->context;
  CoglOnscreenQueuedDirty *qe = g_slice_new (CoglOnscreenQueuedDirty);

  qe->onscreen = cogl_object_ref (onscreen);
  qe->info = *info;
  _cogl_list_insert (ctx->onscreen_dirty_queue.prev, &qe->link);

  _cogl_onscreen_queue_dispatch_idle (onscreen);
}

void
_cogl_onscreen_queue_full_dirty (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglOnscreenDirtyInfo info;

  info.x = 0;
  info.y = 0;
  info.width = framebuffer->width;
  info.height = framebuffer->height;

  _cogl_onscreen_queue_dirty (onscreen, &info);
}

void
_cogl_onscreen_queue_event (CoglOnscreen *onscreen,
                            CoglFrameEvent type,
                            CoglFrameInfo *info)
{
  CoglContext *ctx = COGL_FRAMEBUFFER (onscreen)->context;

  CoglOnscreenEvent *event = g_slice_new (CoglOnscreenEvent);

  event->onscreen = cogl_object_ref (onscreen);
  event->info = cogl_object_ref (info);
  event->type = type;

  _cogl_list_insert (ctx->onscreen_events_queue.prev, &event->link);

  _cogl_onscreen_queue_dispatch_idle (onscreen);
}

void
cogl_onscreen_swap_buffers_with_damage (CoglOnscreen *onscreen,
                                        const int *rectangles,
                                        int n_rectangles)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  const CoglWinsysVtable *winsys;
  CoglFrameInfo *info;

  _COGL_RETURN_IF_FAIL  (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN);

  info = _cogl_frame_info_new ();
  info->frame_counter = onscreen->frame_counter;
  g_queue_push_tail (&onscreen->pending_frame_infos, info);

  /* FIXME: we shouldn't need to flush *all* journals here! */
  cogl_flush ();

  winsys = _cogl_framebuffer_get_winsys (framebuffer);
  winsys->onscreen_swap_buffers_with_damage (onscreen,
                                             rectangles, n_rectangles);
  cogl_framebuffer_discard_buffers (framebuffer,
                                    COGL_BUFFER_BIT_COLOR |
                                    COGL_BUFFER_BIT_DEPTH |
                                    COGL_BUFFER_BIT_STENCIL);

  if (!_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT))
    {
      CoglFrameInfo *info;

      g_warn_if_fail (onscreen->pending_frame_infos.length == 1);

      info = g_queue_pop_tail (&onscreen->pending_frame_infos);

      _cogl_onscreen_queue_event (onscreen, COGL_FRAME_EVENT_SYNC, info);
      _cogl_onscreen_queue_event (onscreen, COGL_FRAME_EVENT_COMPLETE, info);

      cogl_object_unref (info);
    }

  onscreen->frame_counter++;
  framebuffer->mid_scene = FALSE;
}

void
cogl_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  cogl_onscreen_swap_buffers_with_damage (onscreen, NULL, 0);
}

void
cogl_onscreen_swap_region (CoglOnscreen *onscreen,
                           const int *rectangles,
                           int n_rectangles)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  const CoglWinsysVtable *winsys;
  CoglFrameInfo *info;

  _COGL_RETURN_IF_FAIL  (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN);

  info = _cogl_frame_info_new ();
  info->frame_counter = onscreen->frame_counter;
  g_queue_push_tail (&onscreen->pending_frame_infos, info);

  /* FIXME: we shouldn't need to flush *all* journals here! */
  cogl_flush ();

  winsys = _cogl_framebuffer_get_winsys (framebuffer);

  /* This should only be called if the winsys advertises
     COGL_WINSYS_FEATURE_SWAP_REGION */
  _COGL_RETURN_IF_FAIL (winsys->onscreen_swap_region != NULL);

  winsys->onscreen_swap_region (COGL_ONSCREEN (framebuffer),
                                rectangles,
                                n_rectangles);

  cogl_framebuffer_discard_buffers (framebuffer,
                                    COGL_BUFFER_BIT_COLOR |
                                    COGL_BUFFER_BIT_DEPTH |
                                    COGL_BUFFER_BIT_STENCIL);

  if (!_cogl_winsys_has_feature (COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT))
    {
      CoglFrameInfo *info;

      g_warn_if_fail (onscreen->pending_frame_infos.length == 1);

      info = g_queue_pop_tail (&onscreen->pending_frame_infos);

      _cogl_onscreen_queue_event (onscreen, COGL_FRAME_EVENT_SYNC, info);
      _cogl_onscreen_queue_event (onscreen, COGL_FRAME_EVENT_COMPLETE, info);

      cogl_object_unref (info);
    }

  onscreen->frame_counter++;
  framebuffer->mid_scene = FALSE;
}

int
cogl_onscreen_get_buffer_age (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  const CoglWinsysVtable *winsys;

  _COGL_RETURN_VAL_IF_FAIL  (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN, 0);

  winsys = _cogl_framebuffer_get_winsys (framebuffer);

  if (!winsys->onscreen_get_buffer_age)
    return 0;

  return winsys->onscreen_get_buffer_age (onscreen);
}

#ifdef COGL_HAS_X11_SUPPORT
void
cogl_x11_onscreen_set_foreign_window_xid (CoglOnscreen *onscreen,
                                          uint32_t xid,
                                          CoglOnscreenX11MaskCallback update,
                                          void *user_data)
{
  /* We don't wan't applications to get away with being lazy here and not
   * passing an update callback... */
  _COGL_RETURN_IF_FAIL (update);

  onscreen->foreign_xid = xid;
  onscreen->foreign_update_mask_callback = update;
  onscreen->foreign_update_mask_data = user_data;
}

uint32_t
cogl_x11_onscreen_get_window_xid (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);

  if (onscreen->foreign_xid)
    return onscreen->foreign_xid;
  else
    {
      const CoglWinsysVtable *winsys = _cogl_framebuffer_get_winsys (framebuffer);

      /* This should only be called for x11 onscreens */
      _COGL_RETURN_VAL_IF_FAIL (winsys->onscreen_x11_get_window_xid != NULL, 0);

      return winsys->onscreen_x11_get_window_xid (onscreen);
    }
}

uint32_t
cogl_x11_onscreen_get_visual_xid (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  const CoglWinsysVtable *winsys = _cogl_framebuffer_get_winsys (framebuffer);
  XVisualInfo *visinfo;
  uint32_t id;

  /* This should only be called for xlib based onscreens */
  _COGL_RETURN_VAL_IF_FAIL (winsys->xlib_get_visual_info != NULL, 0);

  visinfo = winsys->xlib_get_visual_info ();
  id = (uint32_t)visinfo->visualid;

  XFree (visinfo);
  return id;
}
#endif /* COGL_HAS_X11_SUPPORT */

#ifdef COGL_HAS_WIN32_SUPPORT

void
cogl_win32_onscreen_set_foreign_window (CoglOnscreen *onscreen,
                                        HWND hwnd)
{
  onscreen->foreign_hwnd = hwnd;
}

HWND
cogl_win32_onscreen_get_window (CoglOnscreen *onscreen)
{
  if (onscreen->foreign_hwnd)
    return onscreen->foreign_hwnd;
  else
    {
      CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
      const CoglWinsysVtable *winsys =
        _cogl_framebuffer_get_winsys (framebuffer);

      /* This should only be called for win32 onscreens */
      _COGL_RETURN_VAL_IF_FAIL (winsys->onscreen_win32_get_window != NULL, 0);

      return winsys->onscreen_win32_get_window (onscreen);
    }
}

#endif /* COGL_HAS_WIN32_SUPPORT */

CoglFrameClosure *
cogl_onscreen_add_frame_callback (CoglOnscreen *onscreen,
                                  CoglFrameCallback callback,
                                  void *user_data,
                                  CoglUserDataDestroyCallback destroy)
{
  return _cogl_closure_list_add (&onscreen->frame_closures,
                                 callback,
                                 user_data,
                                 destroy);
}

void
cogl_onscreen_remove_frame_callback (CoglOnscreen *onscreen,
                                     CoglFrameClosure *closure)
{
  _COGL_RETURN_IF_FAIL (closure);

  _cogl_closure_disconnect (closure);
}

typedef struct _SwapBufferCallbackState
{
  CoglSwapBuffersNotify callback;
  void *user_data;
} SwapBufferCallbackState;

static void
destroy_swap_buffers_callback_state (void *user_data)
{
  g_slice_free (SwapBufferCallbackState, user_data);
}

static void
shim_swap_buffers_callback (CoglOnscreen *onscreen,
                            CoglFrameEvent event,
                            CoglFrameInfo *info,
                            void *user_data)
{
  SwapBufferCallbackState *state = user_data;

  /* XXX: Note that technically it is a change in semantics for this
   * interface to forward _SYNC events here and also makes the api
   * name somewhat missleading.
   *
   * In practice though this interface is currently used by
   * applications for throttling, not because they are strictly
   * interested in knowing when a frame has been presented and so
   * forwarding _SYNC events should serve them better.
   */
  if (event == COGL_FRAME_EVENT_SYNC)
    state->callback (COGL_FRAMEBUFFER (onscreen), state->user_data);
}

unsigned int
cogl_onscreen_add_swap_buffers_callback (CoglOnscreen *onscreen,
                                         CoglSwapBuffersNotify callback,
                                         void *user_data)
{
  CoglContext *ctx = COGL_FRAMEBUFFER (onscreen)->context;
  SwapBufferCallbackState *state = g_slice_new (SwapBufferCallbackState);
  CoglFrameClosure *closure;
  unsigned int id = ctx->next_swap_callback_id++;

  state->callback = callback;
  state->user_data = user_data;

  closure =
    cogl_onscreen_add_frame_callback (onscreen,
                                      shim_swap_buffers_callback,
                                      state,
                                      destroy_swap_buffers_callback_state);

  g_hash_table_insert (ctx->swap_callback_closures,
                       GINT_TO_POINTER (id),
                       closure);

  return id;
}

void
cogl_onscreen_remove_swap_buffers_callback (CoglOnscreen *onscreen,
                                            unsigned int id)
{
  CoglContext *ctx = COGL_FRAMEBUFFER (onscreen)->context;
  CoglFrameClosure *closure = g_hash_table_lookup (ctx->swap_callback_closures,
                                                   GINT_TO_POINTER (id));

  _COGL_RETURN_IF_FAIL (closure);

  cogl_onscreen_remove_frame_callback (onscreen, closure);
}

void
cogl_onscreen_set_swap_throttled (CoglOnscreen *onscreen,
                                  CoglBool throttled)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  framebuffer->config.swap_throttled = throttled;
  if (framebuffer->allocated)
    {
      const CoglWinsysVtable *winsys =
        _cogl_framebuffer_get_winsys (framebuffer);
      winsys->onscreen_update_swap_throttled (onscreen);
    }
}

void
cogl_onscreen_show (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  const CoglWinsysVtable *winsys;

  if (!framebuffer->allocated)
    {
      if (!cogl_framebuffer_allocate (framebuffer, NULL))
        return;
    }

  winsys = _cogl_framebuffer_get_winsys (framebuffer);
  if (winsys->onscreen_set_visibility)
    winsys->onscreen_set_visibility (onscreen, TRUE);
}

void
cogl_onscreen_hide (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);

  if (framebuffer->allocated)
    {
      const CoglWinsysVtable *winsys =
        _cogl_framebuffer_get_winsys (framebuffer);
      if (winsys->onscreen_set_visibility)
        winsys->onscreen_set_visibility (onscreen, FALSE);
    }
}

void
_cogl_onscreen_notify_frame_sync (CoglOnscreen *onscreen, CoglFrameInfo *info)
{
  notify_event (onscreen, COGL_FRAME_EVENT_SYNC, info);
}

void
_cogl_onscreen_notify_complete (CoglOnscreen *onscreen, CoglFrameInfo *info)
{
  notify_event (onscreen, COGL_FRAME_EVENT_COMPLETE, info);
}

void
_cogl_onscreen_notify_resize (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);

  _cogl_closure_list_invoke (&onscreen->resize_closures,
                             CoglOnscreenResizeCallback,
                             onscreen,
                             framebuffer->width,
                             framebuffer->height);
}

void
_cogl_framebuffer_winsys_update_size (CoglFramebuffer *framebuffer,
                                      int width, int height)
{
  if (framebuffer->width == width && framebuffer->height == height)
    return;

  framebuffer->width = width;
  framebuffer->height = height;

  cogl_framebuffer_set_viewport (framebuffer, 0, 0, width, height);

  if (!_cogl_has_private_feature (framebuffer->context,
                                  COGL_PRIVATE_FEATURE_DIRTY_EVENTS))
    _cogl_onscreen_queue_full_dirty (COGL_ONSCREEN (framebuffer));
}

void
cogl_onscreen_set_resizable (CoglOnscreen *onscreen,
                             CoglBool resizable)
{
  CoglFramebuffer *framebuffer;
  const CoglWinsysVtable *winsys;

  if (onscreen->resizable == resizable)
    return;

  onscreen->resizable = resizable;

  framebuffer = COGL_FRAMEBUFFER (onscreen);
  if (framebuffer->allocated)
    {
      winsys = _cogl_framebuffer_get_winsys (COGL_FRAMEBUFFER (onscreen));

      if (winsys->onscreen_set_resizable)
        winsys->onscreen_set_resizable (onscreen, resizable);
    }
}

CoglBool
cogl_onscreen_get_resizable (CoglOnscreen *onscreen)
{
  return onscreen->resizable;
}

CoglOnscreenResizeClosure *
cogl_onscreen_add_resize_callback (CoglOnscreen *onscreen,
                                   CoglOnscreenResizeCallback callback,
                                   void *user_data,
                                   CoglUserDataDestroyCallback destroy)
{
  return _cogl_closure_list_add (&onscreen->resize_closures,
                                 callback,
                                 user_data,
                                 destroy);
}

void
cogl_onscreen_remove_resize_callback (CoglOnscreen *onscreen,
                                      CoglOnscreenResizeClosure *closure)
{
  _cogl_closure_disconnect (closure);
}

CoglOnscreenDirtyClosure *
cogl_onscreen_add_dirty_callback (CoglOnscreen *onscreen,
                                  CoglOnscreenDirtyCallback callback,
                                  void *user_data,
                                  CoglUserDataDestroyCallback destroy)
{
  return _cogl_closure_list_add (&onscreen->dirty_closures,
                                 callback,
                                 user_data,
                                 destroy);
}

void
cogl_onscreen_remove_dirty_callback (CoglOnscreen *onscreen,
                                     CoglOnscreenDirtyClosure *closure)
{
  _COGL_RETURN_IF_FAIL (closure);

  _cogl_closure_disconnect (closure);
}

int64_t
cogl_onscreen_get_frame_counter (CoglOnscreen *onscreen)
{
  return onscreen->frame_counter;
}
