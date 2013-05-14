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

#include "cogl-renderer-private.h"
#include "cogl-win32-renderer.h"

CoglFilterReturn
cogl_win32_renderer_handle_event (CoglRenderer *renderer,
                                  MSG *event)
{
  return _cogl_renderer_handle_native_event (renderer, event);
}

void
cogl_win32_renderer_add_filter (CoglRenderer *renderer,
                                CoglWin32FilterFunc func,
                                void *data)
{
  _cogl_renderer_add_native_filter (renderer,
                                    (CoglNativeFilterFunc)func, data);
}

void
cogl_win32_renderer_remove_filter (CoglRenderer *renderer,
                                   CoglWin32FilterFunc func,
                                   void *data)
{
  _cogl_renderer_remove_native_filter (renderer,
                                       (CoglNativeFilterFunc)func, data);
}

void
cogl_win32_renderer_set_event_retrieval_enabled (CoglRenderer *renderer,
                                                 CoglBool enable)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));
  /* NB: Renderers are considered immutable once connected */
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->win32_enable_event_retrieval = enable;
}
