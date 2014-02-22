/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-private.h"
#include "cogl-object.h"

#include "cogl-display-private.h"
#include "cogl-renderer-private.h"
#include "cogl-winsys-private.h"
#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
#include "cogl-wayland-server.h"
#endif

static void _cogl_display_free (CoglDisplay *display);

COGL_OBJECT_DEFINE (Display, display);

static const CoglWinsysVtable *
_cogl_display_get_winsys (CoglDisplay *display)
{
  return display->renderer->winsys_vtable;
}

static void
_cogl_display_free (CoglDisplay *display)
{
  const CoglWinsysVtable *winsys;

  if (display->setup)
    {
      winsys = _cogl_display_get_winsys (display);
      winsys->display_destroy (display);
      display->setup = FALSE;
    }

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
  CoglError *error = NULL;

  _cogl_init ();

  display->renderer = renderer;
  if (renderer)
    cogl_object_ref (renderer);
  else
    display->renderer = cogl_renderer_new ();

  if (!cogl_renderer_connect (display->renderer, &error))
    g_error ("Failed to connect to renderer: %s\n", error->message);

  display->setup = FALSE;

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  display->gdl_plane = GDL_PLANE_ID_UPP_C;
#endif

  display = _cogl_display_object_new (display);

  cogl_display_set_onscreen_template (display, onscreen_template);

  return display;
}

CoglRenderer *
cogl_display_get_renderer (CoglDisplay *display)
{
  return display->renderer;
}

void
cogl_display_set_onscreen_template (CoglDisplay *display,
                                    CoglOnscreenTemplate *onscreen_template)
{
  _COGL_RETURN_IF_FAIL (display->setup == FALSE);

  if (onscreen_template)
    cogl_object_ref (onscreen_template);

  if (display->onscreen_template)
    cogl_object_unref (display->onscreen_template);

  display->onscreen_template = onscreen_template;

  /* NB: we want to maintain the invariable that there is always an
   * onscreen template associated with a CoglDisplay... */
  if (!onscreen_template)
    display->onscreen_template = cogl_onscreen_template_new (NULL);
}

CoglBool
cogl_display_setup (CoglDisplay *display,
                    CoglError **error)
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
                            gdl_plane_id_t plane)
{
  _COGL_RETURN_IF_FAIL (display->setup == FALSE);

  display->gdl_plane = plane;
}
#endif

#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
void
cogl_wayland_display_set_compositor_display (CoglDisplay *display,
                                             struct wl_display *wayland_display)
{
  _COGL_RETURN_IF_FAIL (display->setup == FALSE);

  display->wayland_compositor_display = wayland_display;
}
#endif
