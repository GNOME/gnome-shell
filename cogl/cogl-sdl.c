/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-sdl.h"
#include "cogl-context-private.h"
#include "cogl-renderer-private.h"

void
cogl_sdl_renderer_set_event_type (CoglRenderer *renderer, uint8_t type)
{
  renderer->sdl_event_type_set = TRUE;
  renderer->sdl_event_type = type;
}

uint8_t
cogl_sdl_renderer_get_event_type (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (renderer->sdl_event_type_set, SDL_USEREVENT);

  return renderer->sdl_event_type;
}

CoglContext *
cogl_sdl_context_new (uint8_t type, GError **error)
{
  CoglRenderer *renderer = cogl_renderer_new ();
  CoglDisplay *display;

  cogl_renderer_set_winsys_id (renderer, COGL_WINSYS_ID_SDL);

  cogl_sdl_renderer_set_event_type (renderer, type);

  if (!cogl_renderer_connect (renderer, error))
    return NULL;

  display = cogl_display_new (renderer, NULL);
  if (!cogl_display_setup (display, error))
    return NULL;

  return cogl_context_new (display, error);
}

void
cogl_sdl_handle_event (CoglContext *context, SDL_Event *event)
{
  const CoglWinsysVtable *winsys;

  _COGL_RETURN_IF_FAIL (cogl_is_context (context));

  winsys = _cogl_context_get_winsys (context);

  if (winsys->poll_dispatch)
    winsys->poll_dispatch (context, NULL, 0);
}

void
cogl_sdl_idle (CoglContext *context)
{
  /* NOP since Cogl doesn't currently need to do anything when idle */
}
