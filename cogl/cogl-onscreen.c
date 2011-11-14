/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-util.h"
#include "cogl-onscreen-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"

static void _cogl_onscreen_free (CoglOnscreen *onscreen);

COGL_OBJECT_INTERNAL_DEFINE (Onscreen, onscreen);

static void
_cogl_onscreen_init_from_template (CoglOnscreen *onscreen,
                                   CoglOnscreenTemplate *onscreen_template)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);

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
                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
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
                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                          width, /* width */
                          height); /* height */

  _cogl_onscreen_init_from_template (onscreen, ctx->display->onscreen_template);

  /* FIXME: This should be configurable via the template too */
  onscreen->swap_throttled = TRUE;

  return _cogl_onscreen_object_new (onscreen);
}

static void
_cogl_onscreen_free (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  const CoglWinsysVtable *winsys = _cogl_framebuffer_get_winsys (framebuffer);

  if (framebuffer->context->window_buffer == onscreen)
    framebuffer->context->window_buffer = NULL;

  winsys->onscreen_deinit (onscreen);
  _COGL_RETURN_IF_FAIL (onscreen->winsys == NULL);

  /* Chain up to parent */
  _cogl_framebuffer_free (framebuffer);

  g_free (onscreen);
}

void
cogl_framebuffer_swap_buffers (CoglFramebuffer *framebuffer)
{
  const CoglWinsysVtable *winsys;

  _COGL_RETURN_IF_FAIL  (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN);

  /* FIXME: we shouldn't need to flush *all* journals here! */
  cogl_flush ();
  winsys = _cogl_framebuffer_get_winsys (framebuffer);
  winsys->onscreen_swap_buffers (COGL_ONSCREEN (framebuffer));
  cogl_framebuffer_discard_buffers (framebuffer,
                                    COGL_BUFFER_BIT_COLOR |
                                    COGL_BUFFER_BIT_DEPTH |
                                    COGL_BUFFER_BIT_STENCIL);
}

void
cogl_framebuffer_swap_region (CoglFramebuffer *framebuffer,
                              const int *rectangles,
                              int n_rectangles)
{
  const CoglWinsysVtable *winsys;

  _COGL_RETURN_IF_FAIL  (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN);

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
}

#ifdef COGL_HAS_X11_SUPPORT
void
cogl_x11_onscreen_set_foreign_window_xid (CoglOnscreen *onscreen,
                                          guint32 xid,
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

guint32
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

guint32
cogl_x11_onscreen_get_visual_xid (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  const CoglWinsysVtable *winsys = _cogl_framebuffer_get_winsys (framebuffer);
  XVisualInfo *visinfo;
  guint32 id;

  /* This should only be called for xlib based onscreens */
  _COGL_RETURN_VAL_IF_FAIL (winsys->xlib_get_visual_info != NULL, 0);

  visinfo = winsys->xlib_get_visual_info ();
  id = (guint32)visinfo->visualid;

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

unsigned int
cogl_framebuffer_add_swap_buffers_callback (CoglFramebuffer *framebuffer,
                                            CoglSwapBuffersNotify callback,
                                            void *user_data)
{
  CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
  const CoglWinsysVtable *winsys = _cogl_framebuffer_get_winsys (framebuffer);

  /* Should this just be cogl_onscreen API instead? */
  _COGL_RETURN_VAL_IF_FAIL (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN, 0);

  /* This should only be called when
     COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT is advertised */
  _COGL_RETURN_VAL_IF_FAIL (winsys->onscreen_add_swap_buffers_callback != NULL, 0);

  return winsys->onscreen_add_swap_buffers_callback (onscreen,
                                                     callback,
                                                     user_data);
}

void
cogl_framebuffer_remove_swap_buffers_callback (CoglFramebuffer *framebuffer,
                                               unsigned int id)
{
  CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
  const CoglWinsysVtable *winsys = _cogl_framebuffer_get_winsys (framebuffer);

  /* This should only be called when
     COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT is advertised */
  _COGL_RETURN_IF_FAIL (winsys->onscreen_remove_swap_buffers_callback != NULL);

  winsys->onscreen_remove_swap_buffers_callback (onscreen, id);
}

void
cogl_onscreen_set_swap_throttled (CoglOnscreen *onscreen,
                                  gboolean throttled)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  onscreen->swap_throttled = throttled;
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
_cogl_framebuffer_winsys_update_size (CoglFramebuffer *framebuffer,
                                      int width, int height)
{
  CoglContext *ctx = framebuffer->context;

  if (framebuffer->width == width && framebuffer->height == height)
    return;

  framebuffer->width = width;
  framebuffer->height = height;

  /* We'll need to recalculate the GL viewport state derived
   * from the Cogl viewport */
  ctx->dirty_gl_viewport = 1;
}
