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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-renderer-private.h"
#include "cogl-display-private.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-private.h"

#include <string.h>

static int _cogl_winsys_stub_dummy_ptr;

/* This provides a NOP winsys. This can be useful for debugging or for
 * integrating with toolkits that already have window system
 * integration code.
 */

static CoglFuncPtr
_cogl_winsys_get_proc_address (const char *name)
{
  return NULL;
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  renderer->winsys = NULL;
}

static gboolean
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  renderer->winsys = &_cogl_winsys_stub_dummy_ptr;
  return TRUE;
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  display->winsys = NULL;
}

static gboolean
_cogl_winsys_display_setup (CoglDisplay *display,
                            GError **error)
{
  display->winsys = &_cogl_winsys_stub_dummy_ptr;
  return TRUE;
}

static gboolean
_cogl_winsys_context_init (CoglContext *context, GError **error)
{
  context->winsys = &_cogl_winsys_stub_dummy_ptr;

  if (!_cogl_context_update_features (context, error))
    return FALSE;

  memset (context->winsys_features, 0, sizeof (context->winsys_features));

  return TRUE;
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
  context->winsys = NULL;
}

static gboolean
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            GError **error)
{
  return TRUE;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      gboolean visibility)
{
}

static CoglWinsysVtable _cogl_winsys_vtable =
  {
    .id = COGL_WINSYS_ID_STUB,
    .name = "STUB",
    .get_proc_address = _cogl_winsys_get_proc_address,
    .renderer_connect = _cogl_winsys_renderer_connect,
    .renderer_disconnect = _cogl_winsys_renderer_disconnect,
    .display_setup = _cogl_winsys_display_setup,
    .display_destroy = _cogl_winsys_display_destroy,
    .context_init = _cogl_winsys_context_init,
    .context_deinit = _cogl_winsys_context_deinit,

    .onscreen_init = _cogl_winsys_onscreen_init,
    .onscreen_deinit = _cogl_winsys_onscreen_deinit,
    .onscreen_bind = _cogl_winsys_onscreen_bind,
    .onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers,
    .onscreen_update_swap_throttled =
      _cogl_winsys_onscreen_update_swap_throttled,
    .onscreen_set_visibility = _cogl_winsys_onscreen_set_visibility,
  };

const CoglWinsysVtable *
_cogl_winsys_stub_get_vtable (void)
{
  return &_cogl_winsys_vtable;
}
