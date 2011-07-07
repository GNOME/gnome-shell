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

#include <string.h>

#include "cogl.h"
#include "cogl-private.h"
#include "cogl-object.h"
#include "cogl-internal.h"

#include "cogl-display-private.h"
#include "cogl-renderer-private.h"
#include "cogl-winsys-private.h"

static void _cogl_display_free (CoglDisplay *display);

COGL_OBJECT_DEFINE (Display, display);

GQuark
cogl_display_error_quark (void)
{
  return g_quark_from_static_string ("cogl-display-error-quark");
}

static void
_cogl_display_free (CoglDisplay *display)
{
  if (display->renderer)
    {
      cogl_object_unref (display->renderer);
      display->renderer = NULL;
    }

  if (display->onscreen_template)
    {
      cogl_object_unref (display->onscreen_template);
      display->onscreen_template = NULL;
    }

  g_slice_free (CoglDisplay, display);
}

CoglDisplay *
cogl_display_new (CoglRenderer *renderer,
                  CoglOnscreenTemplate *onscreen_template)
{
  CoglDisplay *display = g_slice_new0 (CoglDisplay);
  GError *error = NULL;

  _cogl_init ();

  display->renderer = renderer;
  if (renderer)
    cogl_object_ref (renderer);
  else
    display->renderer = cogl_renderer_new ();

  if (!cogl_renderer_connect (display->renderer, &error))
    {
      g_warning ("Failed to connect renderer: %s\n", error->message);
      g_error_free (error);
      g_object_unref (display->renderer);
      g_slice_free (CoglDisplay, display);
      return NULL;
    }

  display->onscreen_template = onscreen_template;
  if (onscreen_template)
    cogl_object_ref (onscreen_template);

  display->setup = FALSE;

  return _cogl_display_object_new (display);
}

static const CoglWinsysVtable *
_cogl_display_get_winsys (CoglDisplay *display)
{
  return display->renderer->winsys_vtable;
}

gboolean
cogl_display_setup (CoglDisplay *display,
                    GError **error)
{
  const CoglWinsysVtable *winsys;

  if (display->setup)
    return TRUE;

  winsys = _cogl_display_get_winsys (display);
  if (!winsys->display_setup (display, error))
    return FALSE;

  display->setup = TRUE;

  return TRUE;
}

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
void
cogl_gdl_display_set_plane (CoglDisplay *display,
                            struct gdl_plane *plane)
{
  g_return_if_fail (display->setup == FALSE);

  display->gdl_plane = plane;
}
#endif

#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
void
cogl_wayland_display_set_compositor_display (CoglDisplay *display,
                                             struct wl_display *wayland_display)
{
  g_return_if_fail (display->setup == FALSE);

  display->wayland_compositor_display = wayland_display;
}
#endif
