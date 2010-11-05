/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-object.h"

#include "cogl-renderer-private.h"
#include "cogl-renderer-xlib-private.h"
#include "cogl-renderer-x11-private.h"
#include "cogl-winsys-private.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>

static char *_cogl_x11_display_name = NULL;
static GList *_cogl_xlib_renderers = NULL;

CoglXlibFilterReturn
cogl_renderer_xlib_handle_event (CoglRenderer *renderer,
                                 XEvent *xevent)
{
  CoglRendererXlib *xlib_renderer = renderer->winsys;
  GSList *l;

  g_return_val_if_fail (xlib_renderer->xdpy != NULL, COGL_XLIB_FILTER_CONTINUE);

  /* XXX: should this be a more graceful check? */
  g_return_val_if_fail (xlib_renderer != NULL, COGL_XLIB_FILTER_CONTINUE);

  /* Pass the event on to all of the registered filters in turn */
  for (l = xlib_renderer->event_filters; l; l = l->next)
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

void
cogl_renderer_xlib_add_filter (CoglRenderer *renderer,
                               CoglXlibFilterFunc func,
                               void *data)
{
  CoglRendererXlib *xlib_renderer;
  CoglXlibFilterClosure *closure;

  xlib_renderer = renderer->winsys;

  closure = g_slice_new (CoglXlibFilterClosure);
  closure->func = func;
  closure->data = data;

  xlib_renderer->event_filters =
    g_slist_prepend (xlib_renderer->event_filters, closure);
}

void
cogl_renderer_xlib_remove_filter (CoglRenderer *renderer,
                                  CoglXlibFilterFunc func,
                                  void *data)
{
  CoglRendererXlib *xlib_renderer;
  GSList *l, *prev = NULL;

  xlib_renderer = renderer->winsys;

  for (l = xlib_renderer->event_filters; l; prev = l, l = l->next)
    {
      CoglXlibFilterClosure *closure = l->data;

      if (closure->func == func && closure->data == data)
        {
          g_slice_free (CoglXlibFilterClosure, closure);
          if (prev)
            prev->next = g_slist_delete_link (prev->next, l);
          else
            xlib_renderer->event_filters =
              g_slist_delete_link (xlib_renderer->event_filters, l);
          break;
        }
    }
}

static void
register_xlib_renderer (CoglRenderer *renderer)
{
  GList *l;

  for (l = _cogl_xlib_renderers; l; l = l->next)
    if (l->data == renderer)
      return;

  _cogl_xlib_renderers = g_list_prepend (_cogl_xlib_renderers, renderer);
}

static void
unregister_xlib_renderer (CoglRenderer *renderer)
{
  _cogl_xlib_renderers = g_list_remove (_cogl_xlib_renderers, renderer);
}

static CoglRenderer *
get_renderer_for_xdisplay (Display *xdpy)
{
  GList *l;

  for (l = _cogl_xlib_renderers; l; l = l->next)
    {
      CoglRenderer *renderer = l->data;
      CoglRendererXlib *xlib_renderer = renderer->winsys;

      if (xlib_renderer->xdpy == xdpy)
        return renderer;
    }

  return NULL;
}

static int
error_handler (Display *xdpy,
               XErrorEvent *error)
{
  CoglRenderer *renderer;
  CoglRendererXlib *xlib_renderer;

  renderer = get_renderer_for_xdisplay (xdpy);

  xlib_renderer = renderer->winsys;
  g_assert (xlib_renderer->trap_state);

  xlib_renderer->trap_state->trapped_error_code = error->error_code;

  return 0;
}

void
_cogl_renderer_xlib_trap_errors (CoglRenderer *renderer,
                                 CoglXlibTrapState *state)
{
  CoglRendererXlib *xlib_renderer;

  xlib_renderer = renderer->winsys;

  state->trapped_error_code = 0;
  state->old_error_handler = XSetErrorHandler (error_handler);

  state->old_state = xlib_renderer->trap_state;
  xlib_renderer->trap_state = state;
}

int
_cogl_renderer_xlib_untrap_errors (CoglRenderer *renderer,
                                   CoglXlibTrapState *state)
{
  CoglRendererXlib *xlib_renderer;

  xlib_renderer = renderer->winsys;
  g_assert (state == xlib_renderer->trap_state);

  XSetErrorHandler (state->old_error_handler);

  xlib_renderer->trap_state = state->old_state;

  return state->trapped_error_code;
}

static Display *
assert_xlib_display (CoglRenderer *renderer, GError **error)
{
  Display *xdpy = cogl_renderer_xlib_get_foreign_display (renderer);
  CoglRendererXlib *xlib_renderer = renderer->winsys;

  /* A foreign display may have already been set... */
  if (xdpy)
    {
      xlib_renderer->xdpy = xdpy;
      return xdpy;
    }

  xdpy = XOpenDisplay (_cogl_x11_display_name);
  if (xdpy == NULL)
    {
      g_set_error (error,
                   COGL_RENDERER_ERROR,
                   COGL_RENDERER_ERROR_XLIB_DISPLAY_OPEN,
                   "Failed to open X Display %s", _cogl_x11_display_name);
      return NULL;
    }

  xlib_renderer->xdpy = xdpy;
  return xdpy;
}

gboolean
_cogl_renderer_xlib_connect (CoglRenderer *renderer, GError **error)
{
  CoglRendererXlib *xlib_renderer = renderer->winsys;
  CoglRendererX11 *x11_renderer = renderer->winsys;
  int damage_error;

  if (!assert_xlib_display (renderer, error))
    return FALSE;

  /* Check whether damage events are supported on this display */
  if (!XDamageQueryExtension (xlib_renderer->xdpy,
                              &x11_renderer->damage_base,
                              &damage_error))
    x11_renderer->damage_base = -1;

  xlib_renderer->event_filters = NULL;

  xlib_renderer->trap_state = NULL;

  register_xlib_renderer (renderer);

  return TRUE;
}

static void
free_xlib_filter_closure (void *data, void *user_data)
{
  g_slice_free (CoglXlibFilterClosure, data);
}

void
_cogl_renderer_xlib_disconnect (CoglRenderer *renderer)
{
  CoglRendererXlib *xlib_renderer = renderer->winsys;

  g_slist_foreach (xlib_renderer->event_filters,
                   free_xlib_filter_closure, NULL);
  g_slist_free (xlib_renderer->event_filters);

  if (!renderer->foreign_xdpy)
    XCloseDisplay (xlib_renderer->xdpy);

  unregister_xlib_renderer (renderer);
}

Display *
cogl_renderer_xlib_get_display (CoglRenderer *renderer)
{
  CoglRendererXlib *xlib_renderer;

  g_return_val_if_fail (cogl_is_renderer (renderer), NULL);

  xlib_renderer = renderer->winsys;

  return xlib_renderer->xdpy;
}
