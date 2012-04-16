/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010,2011 Intel Corporation.
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
 *
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cogl-xlib.h>

#include <cogl-internal.h>
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

  _COGL_GET_CONTEXT (ctxt, NO_RETVAL);

  /* Check whether damage events are supported on this display */
  if (!XDamageQueryExtension (cogl_xlib_get_display (),
                              &ctxt->damage_base,
                              &damage_error))
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
