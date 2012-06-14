/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011, 2012 Intel Corporation.
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

typedef struct _CoglContextSdl2
{
  SDL_Window *current_window;
} CoglContextSdl2;

typedef struct _CoglRendererSdl2
{
  int stub;
} CoglRendererSdl2;

typedef struct _CoglDisplaySdl2
{
  SDL_Window *dummy_window;
  SDL_GLContext *context;
} CoglDisplaySdl2;

typedef struct _CoglOnscreenSdl2
{
  SDL_Window *window;
} CoglOnscreenSdl2;

static CoglFuncPtr
_cogl_winsys_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name)
{
  return SDL_GL_GetProcAddress (name);
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  SDL_VideoQuit ();

  g_slice_free (CoglRendererSdl2, renderer->winsys);
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  if (SDL_VideoInit (NULL) < 0)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "SDL_Init failed: %s",
                   SDL_GetError ());
      return FALSE;
    }

  renderer->winsys = g_slice_new0 (CoglRendererSdl2);

  return TRUE;
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  CoglDisplaySdl2 *sdl_display = display->winsys;

  _COGL_RETURN_IF_FAIL (sdl_display != NULL);

  if (sdl_display->context)
    SDL_GL_DeleteContext (sdl_display->context);

  if (sdl_display->dummy_window)
    SDL_DestroyWindow (sdl_display->dummy_window);

  g_slice_free (CoglDisplaySdl2, display->winsys);
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

static CoglBool
_cogl_winsys_display_setup (CoglDisplay *display,
                            GError **error)
{
  CoglDisplaySdl2 *sdl_display;
  const char * (* get_string_func) (GLenum name);
  const char *gl_version;

  _COGL_RETURN_VAL_IF_FAIL (display->winsys == NULL, FALSE);

  sdl_display = g_slice_new0 (CoglDisplaySdl2);
  display->winsys = sdl_display;

  set_gl_attribs_from_framebuffer_config (&display->onscreen_template->config);

  if (display->renderer->driver == COGL_DRIVER_GLES1)
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, 1);
  else if (display->renderer->driver == COGL_DRIVER_GLES2)
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, 2);

  /* Create a dummy 1x1 window that never gets display so that we can
   * create a GL context */
  sdl_display->dummy_window = SDL_CreateWindow ("",
                                                0, 0, /* x/y */
                                                1, 1, /* w/h */
                                                SDL_WINDOW_OPENGL |
                                                SDL_WINDOW_HIDDEN);
  if (sdl_display->dummy_window == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "SDL_CreateWindow failed: %s",
                   SDL_GetError ());
      goto error;
    }

  sdl_display->context = SDL_GL_CreateContext (sdl_display->dummy_window);

  if (sdl_display->context == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "SDL_GL_CreateContext failed: %s",
                   SDL_GetError ());
      goto error;
    }

  /* SDL doesn't seem to provide a way to select between GL and GLES
   * and instead it will just pick one itself. We can at least try to
   * verify that it picked the one we were expecting by looking at the
   * GL version string */
  get_string_func = SDL_GL_GetProcAddress ("glGetString");
  gl_version = get_string_func (GL_VERSION);

  switch (display->renderer->driver)
    {
    case COGL_DRIVER_GL:
      /* The first character of the version string will be a digit if
       * it's normal GL */
      if (!g_ascii_isdigit (gl_version[0]))
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "The GL driver was requested but SDL is using GLES");
          goto error;
        }
      break;

    case COGL_DRIVER_GLES2:
      if (!g_str_has_prefix (gl_version, "OpenGL ES 2"))
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "The GLES2 driver was requested but SDL is "
                       "not using GLES2");
          goto error;
        }
      break;

    case COGL_DRIVER_GLES1:
      if (!g_str_has_prefix (gl_version, "OpenGL ES 1"))
        {
          g_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "The GLES1 driver was requested but SDL is "
                       "not using GLES1");
          goto error;
        }
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;

error:
  _cogl_winsys_display_destroy (display);
  return FALSE;
}

static CoglBool
_cogl_winsys_context_init (CoglContext *context, GError **error)
{
  CoglRenderer *renderer = context->display->renderer;

  context->winsys = g_new0 (CoglContextSdl2, 1);

  if (G_UNLIKELY (renderer->sdl_event_type_set == FALSE))
    g_error ("cogl_sdl_renderer_set_event_type() or cogl_sdl_context_new() "
             "must be called during initialization");

  if (!_cogl_context_update_features (context, error))
    return FALSE;

  if (SDL_GL_GetSwapInterval () != -1)
    COGL_FLAGS_SET (context->winsys_features,
                    COGL_WINSYS_FEATURE_SWAP_REGION_THROTTLE,
                    TRUE);

  return TRUE;
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
  g_free (context->winsys);
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
  CoglFramebuffer *fb = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = fb->context;
  CoglContextSdl2 *sdl_context = context->winsys;
  CoglDisplaySdl2 *sdl_display = context->display->winsys;
  CoglOnscreenSdl2 *sdl_onscreen = onscreen->winsys;

  if (sdl_context->current_window == sdl_onscreen->window)
    return;

  SDL_GL_MakeCurrent (sdl_onscreen->window, sdl_display->context);

  sdl_context->current_window = sdl_onscreen->window;

  /* It looks like SDL just directly calls a glXSwapInterval function
   * when this is called. This may be provided by either the EXT
   * extension, the SGI extension or the Mesa extension. The SGI
   * extension is per context so we can't just do this once when the
   * framebuffer is allocated. See the comments in the GLX winsys for
   * more info. */
  if (COGL_FLAGS_GET (context->winsys_features,
                      COGL_WINSYS_FEATURE_SWAP_REGION_THROTTLE))
    {
      CoglFramebuffer *fb = COGL_FRAMEBUFFER (onscreen);

      SDL_GL_SetSwapInterval (fb->config.swap_throttled ? 1 : 0);
    }
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglOnscreenSdl2 *sdl_onscreen = onscreen->winsys;

  if (sdl_onscreen->window != NULL)
    {
      CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
      CoglContextSdl2 *sdl_context = context->winsys;

      if (sdl_context->current_window == sdl_onscreen->window)
        sdl_context->current_window = NULL;

      SDL_DestroyWindow (sdl_onscreen->window);
      sdl_onscreen->window = NULL;
    }

  g_slice_free (CoglOnscreenSdl2, sdl_onscreen);
  onscreen->winsys = NULL;
}

static CoglBool
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            GError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglOnscreenSdl2 *sdl_onscreen;
  SDL_Window *window;
  int width, height;

  width = cogl_framebuffer_get_width (framebuffer);
  height = cogl_framebuffer_get_height (framebuffer);

  window = SDL_CreateWindow ("" /* title */,
                             0, 0, /* x/y */
                             width, height,
                             SDL_WINDOW_OPENGL |
                             SDL_WINDOW_HIDDEN);

  if (window == NULL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "SDL_CreateWindow failed: %s",
                   SDL_GetError ());
      return FALSE;
    }

  onscreen->winsys = g_slice_new (CoglOnscreenSdl2);
  sdl_onscreen = onscreen->winsys;
  sdl_onscreen->window = window;

  return TRUE;
}

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  CoglOnscreenSdl2 *sdl_onscreen = onscreen->winsys;

  SDL_GL_SwapWindow (sdl_onscreen->window);
}

static void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglContextSdl2 *sdl_context = context->winsys;
  CoglOnscreenSdl2 *sdl_onscreen = onscreen->winsys;

  if (sdl_context->current_window != sdl_onscreen->window)
    return;

  sdl_context->current_window = NULL;
  _cogl_winsys_onscreen_bind (onscreen);
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      CoglBool visibility)
{
  CoglOnscreenSdl2 *sdl_onscreen = onscreen->winsys;

  if (visibility)
    SDL_ShowWindow (sdl_onscreen->window);
  else
    SDL_HideWindow (sdl_onscreen->window);
}

const CoglWinsysVtable *
_cogl_winsys_sdl_get_vtable (void)
{
  static CoglBool vtable_inited = FALSE;
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
