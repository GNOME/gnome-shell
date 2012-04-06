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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <SDL.h>

#include "cogl-renderer-private.h"
#include "cogl-display-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-swap-chain-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-context-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-winsys-sdl-private.h"

typedef struct _CoglRendererSdl
{
  int stub;
} CoglRendererSdl;

typedef struct _CoglDisplaySdl
{
  SDL_Surface *surface;
  gboolean has_onscreen;
} CoglDisplaySdl;

static CoglFuncPtr
_cogl_winsys_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name)
{
  return SDL_GL_GetProcAddress (name);
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  SDL_Quit ();

  g_slice_free (CoglRendererSdl, renderer->winsys);
}

static gboolean
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  if (renderer->driver != COGL_DRIVER_GL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "The SDL winsys only supports the GL driver");
      return FALSE;
    }

  if (SDL_Init (SDL_INIT_VIDEO) == -1)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "SDL_Init failed: %s",
                   SDL_GetError ());
      return FALSE;
    }

  renderer->winsys = g_slice_new0 (CoglRendererSdl);

  return TRUE;
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  CoglDisplaySdl *sdl_display = display->winsys;

  _COGL_RETURN_IF_FAIL (sdl_display != NULL);

  /* No need to destroy the surface - it is freed by SDL_Quit */

  g_slice_free (CoglDisplaySdl, display->winsys);
  display->winsys = NULL;
}

static void
set_gl_attribs_from_framebuffer_config (CoglFramebufferConfig *config)
{
  SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 1);
  SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 1);
  SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 1);
  SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 1);

  SDL_GL_SetAttribute (SDL_GL_STENCIL_SIZE,
                       config->need_stencil ? 1 : 0);

  SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER,
                       config->swap_chain->length > 1 ? 1 : 0);

  SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE,
                       config->swap_chain->has_alpha ? 1 : 0);
}

static gboolean
_cogl_winsys_display_setup (CoglDisplay *display,
                            GError **error)
{
  CoglDisplaySdl *sdl_display;

  _COGL_RETURN_VAL_IF_FAIL (display->winsys == NULL, FALSE);

  sdl_display = g_slice_new0 (CoglDisplaySdl);
  display->winsys = sdl_display;

  set_gl_attribs_from_framebuffer_config (&display->onscreen_template->config);

  /* There's no way to know what size the application will need until
     it creates the first onscreen but we need to set the video mode
     now so that we can get a GL context. We'll have to just guess at
     a size an resize it later */
  sdl_display->surface = SDL_SetVideoMode (640, 480, 0, SDL_OPENGL);

  if (sdl_display->surface == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "SDL_SetVideoMode failed: %s",
                   SDL_GetError ());
      goto error;
    }

  return TRUE;

error:
  _cogl_winsys_display_destroy (display);
  return FALSE;
}

static gboolean
_cogl_winsys_context_init (CoglContext *context, GError **error)
{
  CoglRenderer *renderer = context->display->renderer;

  if (G_UNLIKELY (renderer->sdl_event_type_set == FALSE))
    g_error ("cogl_sdl_renderer_set_event_type() or cogl_sdl_context_new() "
             "must be called during initialization");

  return _cogl_context_update_features (context, error);
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglDisplay *display = context->display;
  CoglDisplaySdl *sdl_display = display->winsys;

  sdl_display->has_onscreen = FALSE;
}

static gboolean
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            GError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplaySdl *sdl_display = display->winsys;
  int width, height;

  if (sdl_display->has_onscreen)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "SDL winsys only supports a single onscreen window");
      return FALSE;
    }

  width = cogl_framebuffer_get_width (framebuffer);
  height = cogl_framebuffer_get_height (framebuffer);

  /* Try to update the video size using the onscreen size */
  if (width != sdl_display->surface->w ||
      height != sdl_display->surface->h)
    {
      sdl_display->surface = SDL_SetVideoMode (width, height, 0, SDL_OPENGL);

      if (sdl_display->surface == NULL)
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "SDL_SetVideoMode failed: %s",
                       SDL_GetError ());
          return FALSE;
        }
    }

  _cogl_framebuffer_winsys_update_size (framebuffer,
                                        sdl_display->surface->w,
                                        sdl_display->surface->h);

  sdl_display->has_onscreen = TRUE;

  return TRUE;
}

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  SDL_GL_SwapBuffers ();
}

static void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
  /* SDL doesn't appear to provide a way to set this */
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      gboolean visibility)
{
  /* SDL doesn't appear to provide a way to set this */
}

const CoglWinsysVtable *
_cogl_winsys_sdl_get_vtable (void)
{
  static gboolean vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  /* It would be nice if we could use C99 struct initializers here
     like the GLX backend does. However this code is more likely to be
     compiled using Visual Studio which (still!) doesn't support them
     so we initialize it in code instead */

  if (!vtable_inited)
    {
      memset (&vtable, 0, sizeof (vtable));

      vtable.id = COGL_WINSYS_ID_SDL;
      vtable.name = "SDL";
      vtable.renderer_get_proc_address = _cogl_winsys_renderer_get_proc_address;
      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;
      vtable.display_setup = _cogl_winsys_display_setup;
      vtable.display_destroy = _cogl_winsys_display_destroy;
      vtable.context_init = _cogl_winsys_context_init;
      vtable.context_deinit = _cogl_winsys_context_deinit;
      vtable.onscreen_init = _cogl_winsys_onscreen_init;
      vtable.onscreen_deinit = _cogl_winsys_onscreen_deinit;
      vtable.onscreen_bind = _cogl_winsys_onscreen_bind;
      vtable.onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers;
      vtable.onscreen_update_swap_throttled =
        _cogl_winsys_onscreen_update_swap_throttled;
      vtable.onscreen_set_visibility = _cogl_winsys_onscreen_set_visibility;

      vtable_inited = TRUE;
    }

  return &vtable;
}
