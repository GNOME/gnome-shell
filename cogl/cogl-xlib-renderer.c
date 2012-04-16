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

#include "cogl-xlib-renderer.h"
#include "cogl-util.h"
#include "cogl-internal.h"
#include "cogl-object.h"

#include "cogl-renderer-private.h"
#include "cogl-xlib-renderer-private.h"
#include "cogl-x11-renderer-private.h"
#include "cogl-winsys-private.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>

#include <stdlib.h>

static char *_cogl_x11_display_name = NULL;
static GList *_cogl_xlib_renderers = NULL;

static void
destroy_xlib_renderer_data (void *user_data)
{
  g_slice_free (CoglXlibRenderer, user_data);
}

CoglXlibRenderer *
_cogl_xlib_renderer_get_data (CoglRenderer *renderer)
{
  static CoglUserDataKey key;
  CoglXlibRenderer *data;

  /* Constructs a CoglXlibRenderer struct on demand and attaches it to
     the object using user data. It's done this way instead of using a
     subclassing hierarchy in the winsys data because all EGL winsys's
     need the EGL winsys data but only one of them wants the Xlib
     data. */

  data = cogl_object_get_user_data (COGL_OBJECT (renderer), &key);

  if (data == NULL)
    {
      data = g_slice_new0 (CoglXlibRenderer);

      cogl_object_set_user_data (COGL_OBJECT (renderer),
                                 &key,
                                 data,
                                 destroy_xlib_renderer_data);
    }

  return data;
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
      CoglXlibRenderer *xlib_renderer =
        _cogl_xlib_renderer_get_data (renderer);

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
  CoglXlibRenderer *xlib_renderer;

  renderer = get_renderer_for_xdisplay (xdpy);

  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);
  g_assert (xlib_renderer->trap_state);

  xlib_renderer->trap_state->trapped_error_code = error->error_code;

  return 0;
}

void
_cogl_xlib_renderer_trap_errors (CoglRenderer *renderer,
                                 CoglXlibTrapState *state)
{
  CoglXlibRenderer *xlib_renderer;

  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  state->trapped_error_code = 0;
  state->old_error_handler = XSetErrorHandler (error_handler);

  state->old_state = xlib_renderer->trap_state;
  xlib_renderer->trap_state = state;
}

int
_cogl_xlib_renderer_untrap_errors (CoglRenderer *renderer,
                                   CoglXlibTrapState *state)
{
  CoglXlibRenderer *xlib_renderer;

  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);
  g_assert (state == xlib_renderer->trap_state);

  XSetErrorHandler (state->old_error_handler);

  xlib_renderer->trap_state = state->old_state;

  return state->trapped_error_code;
}

static Display *
assert_xlib_display (CoglRenderer *renderer, GError **error)
{
  Display *xdpy = cogl_xlib_renderer_get_foreign_display (renderer);
  CoglXlibRenderer *xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

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

CoglBool
_cogl_xlib_renderer_connect (CoglRenderer *renderer, GError **error)
{
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (renderer);
  CoglX11Renderer *x11_renderer =
    (CoglX11Renderer *) xlib_renderer;
  int damage_error;

  if (!assert_xlib_display (renderer, error))
    return FALSE;

  if (getenv ("COGL_X11_SYNC"))
    XSynchronize (xlib_renderer->xdpy, TRUE);

  /* Check whether damage events are supported on this display */
  if (!XDamageQueryExtension (xlib_renderer->xdpy,
                              &x11_renderer->damage_base,
                              &damage_error))
    x11_renderer->damage_base = -1;

  xlib_renderer->trap_state = NULL;

  xlib_renderer->poll_fd.fd = ConnectionNumber (xlib_renderer->xdpy);
  xlib_renderer->poll_fd.events = COGL_POLL_FD_EVENT_IN;

  register_xlib_renderer (renderer);

  return TRUE;
}

void
_cogl_xlib_renderer_disconnect (CoglRenderer *renderer)
{
  CoglXlibRenderer *xlib_renderer =
    _cogl_xlib_renderer_get_data (renderer);

  if (!renderer->foreign_xdpy && xlib_renderer->xdpy)
    XCloseDisplay (xlib_renderer->xdpy);

  unregister_xlib_renderer (renderer);
}

Display *
cogl_xlib_renderer_get_display (CoglRenderer *renderer)
{
  CoglXlibRenderer *xlib_renderer;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), NULL);

  xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  return xlib_renderer->xdpy;
}

CoglFilterReturn
cogl_xlib_renderer_handle_event (CoglRenderer *renderer,
                                 XEvent *event)
{
  return _cogl_renderer_handle_native_event (renderer, event);
}

void
cogl_xlib_renderer_add_filter (CoglRenderer *renderer,
                               CoglXlibFilterFunc func,
                               void *data)
{
  _cogl_renderer_add_native_filter (renderer,
                                    (CoglNativeFilterFunc)func, data);
}

void
cogl_xlib_renderer_remove_filter (CoglRenderer *renderer,
                                  CoglXlibFilterFunc func,
                                  void *data)
{
  _cogl_renderer_remove_native_filter (renderer,
                                       (CoglNativeFilterFunc)func, data);
}

void
_cogl_xlib_renderer_poll_get_info (CoglRenderer *renderer,
                                   CoglPollFD **poll_fds,
                                   int *n_poll_fds,
                                   int64_t *timeout)
{
  CoglXlibRenderer *xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  if (renderer->xlib_enable_event_retrieval)
    {
      *n_poll_fds = 1;
      *poll_fds = &xlib_renderer->poll_fd;
      if (XPending (xlib_renderer->xdpy))
        *timeout = 0;
      else
        *timeout = -1;
    }
  else
    {
      *n_poll_fds = 0;
      *poll_fds = NULL;
      *timeout = -1;
    }
}

void
_cogl_xlib_renderer_poll_dispatch (CoglRenderer *renderer,
                                   const CoglPollFD *poll_fds,
                                   int n_poll_fds)
{
  CoglXlibRenderer *xlib_renderer = _cogl_xlib_renderer_get_data (renderer);

  if (renderer->xlib_enable_event_retrieval)
    while (XPending (xlib_renderer->xdpy))
      {
        XEvent xevent;

        XNextEvent (xlib_renderer->xdpy, &xevent);

        cogl_xlib_renderer_handle_event (renderer, &xevent);
      }
}
