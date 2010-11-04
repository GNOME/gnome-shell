/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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

#include <cogl.h>
#include <cogl-internal.h>
#include <cogl-handle.h>
#include <cogl-context-private.h>
#include <cogl-framebuffer-private.h>

#include <X11/Xlib.h>

#include "cogl-xlib.h"

/* This can't be in the Cogl context because it can be set before
   context is created */
static Display *_cogl_xlib_display = NULL;

CoglXlibFilterReturn
_cogl_xlib_handle_event (XEvent *xevent)
{
  GSList *l;

  _COGL_GET_CONTEXT (ctx, COGL_XLIB_FILTER_CONTINUE);

  /* Pass the event on to all of the registered filters in turn */
  for (l = ctx->winsys.event_filters; l; l = l->next)
    {
      CoglXlibFilterClosure *closure = l->data;

      if (closure->func (xevent, closure->data) == COGL_XLIB_FILTER_REMOVE)
        return COGL_XLIB_FILTER_REMOVE;
    }

  switch (xevent->type)
    {
      /* TODO... */
    default:
      break;
    }

  return COGL_XLIB_FILTER_CONTINUE;
}

Display *
_cogl_xlib_get_display (void)
{
  _COGL_GET_CONTEXT (ctx, NULL);

  /* _cogl_xlib_set_display should be called before this function */
  g_assert (_cogl_xlib_display != NULL);

  return _cogl_xlib_display;
}

void
_cogl_xlib_set_display (Display *display)
{
  /* This can only be called once before the Cogl context is created */
  g_assert (_cogl_xlib_display == NULL);

  _cogl_xlib_display = display;
}

void
_cogl_xlib_add_filter (CoglXlibFilterFunc func,
                       gpointer data)
{
  CoglXlibFilterClosure *closure;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  closure = g_slice_new (CoglXlibFilterClosure);
  closure->func = func;
  closure->data = data;

  ctx->winsys.event_filters =
    g_slist_prepend (ctx->winsys.event_filters, closure);
}

void
_cogl_xlib_remove_filter (CoglXlibFilterFunc func,
                          gpointer data)
{
  GSList *l, *prev = NULL;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  for (l = ctx->winsys.event_filters; l; prev = l, l = l->next)
    {
      CoglXlibFilterClosure *closure = l->data;

      if (closure->func == func && closure->data == data)
        {
          g_slice_free (CoglXlibFilterClosure, closure);
          if (prev)
            prev->next = g_slist_delete_link (prev->next, l);
          else
            ctx->winsys.event_filters =
              g_slist_delete_link (ctx->winsys.event_filters, l);
          break;
        }
    }
}

static int
error_handler (Display     *xdpy,
               XErrorEvent *error)
{
  _COGL_GET_CONTEXT (ctxt, 0);

  g_assert (ctxt->winsys.trap_state);

  ctxt->winsys.trap_state->trapped_error_code = error->error_code;

  return 0;
}

void
_cogl_xlib_trap_errors (CoglXlibTrapState *state)
{
  _COGL_GET_CONTEXT (ctxt, NO_RETVAL);

  state->trapped_error_code = 0;
  state->old_error_handler = XSetErrorHandler (error_handler);

  state->old_state = ctxt->winsys.trap_state;
  ctxt->winsys.trap_state = state;
}

int
_cogl_xlib_untrap_errors (CoglXlibTrapState *state)
{
  _COGL_GET_CONTEXT (ctxt, 0);

  g_assert (state == ctxt->winsys.trap_state);

  XSetErrorHandler (state->old_error_handler);

  ctxt->winsys.trap_state = state->old_state;

  return state->trapped_error_code;
}
