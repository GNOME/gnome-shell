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

  xlib_renderer->trap_state = NULL;

  register_xlib_renderer (renderer);

  return TRUE;
}

void
_cogl_renderer_xlib_disconnect (CoglRenderer *renderer)
{
  CoglRendererXlib *xlib_renderer = renderer->winsys;

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
