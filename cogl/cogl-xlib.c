/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010,2011 Intel Corporation.
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
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cogl-xlib.h>

#include <cogl-object-private.h>
#include <cogl-context-private.h>
#include <cogl-framebuffer-private.h>
#include <cogl-display-private.h>
#include <cogl-renderer-private.h>
#include <cogl-xlib-renderer-private.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>

#include "cogl-xlib.h"

/* FIXME: when we remove the last X11 based Clutter backend then we
 * will get rid of these functions and instead rely on the equivalent
 * _cogl_xlib_renderer API
 */

/* This can't be in the Cogl context because it can be set before
   context is created */
static Display *_cogl_xlib_display = NULL;

Display *
cogl_xlib_get_display (void)
{
  _COGL_GET_CONTEXT (ctx, NULL);

  return cogl_xlib_renderer_get_display (ctx->display->renderer);
}

void
cogl_xlib_set_display (Display *display)
{
  /* This can only be called once before the Cogl context is created */
  g_assert (_cogl_xlib_display == NULL);

  _cogl_xlib_display = display;
}

/* These three functions are wrappers around the equivalent renderer
   functions. They can be removed once all xlib-based backends in
   Clutter know about the renderer */
CoglFilterReturn
cogl_xlib_handle_event (XEvent *xevent)
{
  _COGL_GET_CONTEXT (ctx, COGL_FILTER_CONTINUE);

  /* Pass the event on to the renderer */
  return cogl_xlib_renderer_handle_event (ctx->display->renderer, xevent);
}

void
_cogl_xlib_query_damage_extension (void)
{
  int damage_error;
  Display *display;

  _COGL_GET_CONTEXT (ctxt, NO_RETVAL);

  /* Check whether damage events are supported on this display */
  display = cogl_xlib_renderer_get_display (ctxt->display->renderer);
  if (!XDamageQueryExtension (display, &ctxt->damage_base, &damage_error))
    ctxt->damage_base = -1;
}

int
_cogl_xlib_get_damage_base (void)
{
  CoglX11Renderer *x11_renderer;
  _COGL_GET_CONTEXT (ctxt, -1);

  x11_renderer =
    (CoglX11Renderer *) _cogl_xlib_renderer_get_data (ctxt->display->renderer);
  return x11_renderer->damage_base;
}
