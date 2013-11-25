/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011, 2012, 2013 Intel Corporation.
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
#include "cogl-error-private.h"
#include "cogl-poll-private.h"
#include "cogl-sdl.h"

typedef struct _CoglContextSdl2
{
  SDL_Window *current_window;
} CoglContextSdl2;

typedef struct _CoglRendererSdl2
{
  CoglClosure *resize_notify_idle;
} CoglRendererSdl2;

typedef struct _CoglDisplaySdl2
{
  SDL_Window *dummy_window;
  SDL_GLContext *context;
} CoglDisplaySdl2;

typedef struct _CoglOnscreenSdl2
{
  SDL_Window *window;
  CoglBool pending_resize_notify;
} CoglOnscreenSdl2;

/* The key used to store a pointer to the CoglOnscreen in an
 * SDL_Window */
#define COGL_SDL_WINDOW_DATA_KEY "cogl-onscreen"

static CoglFuncPtr
_cogl_winsys_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name,
                                        CoglBool in_core)
{
  /* XXX: It's not totally clear whether it's safe to call this for
   * core functions. From the code it looks like the implementations
   * will fall back to using some form of dlsym if the winsys
   * GetProcAddress function returns NULL. Presumably this will work
   * in most cases apart from EGL platforms that return invalid
   * pointers for core functions. It's awkward for this code to get a
   * handle to the GL module that SDL has chosen to load so just
   * calling SDL_GL_GetProcAddress is probably the best we can do
   * here. */
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
                               CoglError **error)
{
  if (SDL_VideoInit (NULL) < 0)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
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

  if (config->swap_chain->length >= 0)
    SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER,
                         config->swap_chain->length > 1 ? 1 : 0);

  SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE,
                       config->swap_chain->has_alpha ? 1 : 0);
}

static CoglBool
_cogl_winsys_display_setup (CoglDisplay *display,
                            CoglError **error)
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
  else if (display->renderer->driver == COGL_DRIVER_GL3)
    {
      SDL_GL_SetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, 3);
      SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK,
                           SDL_GL_CONTEXT_PROFILE_CORE);
      SDL_GL_SetAttribute (SDL_GL_CONTEXT_FLAGS,
                           SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    }

  /* Create a dummy 1x1 window that never gets display so that we can
   * create a GL context */
  sdl_display->dummy_window = SDL_CreateWindow ("",
                                                0, 0, /* x/y */
                                                1, 1, /* w/h */
                                                SDL_WINDOW_OPENGL |
                                                SDL_WINDOW_HIDDEN);
  if (sdl_display->dummy_window == NULL)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "SDL_CreateWindow failed: %s",
                       SDL_GetError ());
      goto error;
    }

  sdl_display->context = SDL_GL_CreateContext (sdl_display->dummy_window);

  if (sdl_display->context == NULL)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
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
    case COGL_DRIVER_GL3:
      /* The first character of the version string will be a digit if
       * it's normal GL */
      if (!g_ascii_isdigit (gl_version[0]))
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                           COGL_WINSYS_ERROR_INIT,
                           "The GL driver was requested but SDL is using GLES");
          goto error;
        }

      if (display->renderer->driver == COGL_DRIVER_GL3 &&
          gl_version[0] < '3')
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                           COGL_WINSYS_ERROR_INIT,
                           "The GL3 driver was requested but SDL is using "
                           "GL %c", gl_version[0]);
          goto error;
        }
      break;

    case COGL_DRIVER_GLES2:
      if (!g_str_has_prefix (gl_version, "OpenGL ES 2"))
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                           COGL_WINSYS_ERROR_INIT,
                           "The GLES2 driver was requested but SDL is "
                           "not using GLES2");
          goto error;
        }
      break;

    case COGL_DRIVER_GLES1:
      if (!g_str_has_prefix (gl_version, "OpenGL ES 1"))
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
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

static void
flush_pending_notifications_cb (void *data,
                                void *user_data)
{
  CoglFramebuffer *framebuffer = data;

  if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
    {
      CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
      CoglOnscreenSdl2 *sdl_onscreen = onscreen->winsys;

      if (sdl_onscreen->pending_resize_notify)
        {
          _cogl_onscreen_notify_resize (onscreen);
          sdl_onscreen->pending_resize_notify = FALSE;
        }
    }
}

static void
flush_pending_resize_notifications_idle (void *user_data)
{
  CoglContext *context = user_data;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererSdl2 *sdl_renderer = renderer->winsys;

  /* This needs to be disconnected before invoking the callbacks in
   * case the callbacks cause it to be queued again */
  _cogl_closure_disconnect (sdl_renderer->resize_notify_idle);
  sdl_renderer->resize_notify_idle = NULL;

  g_list_foreach (context->framebuffers,
                  flush_pending_notifications_cb,
                  NULL);
}

static CoglFilterReturn
sdl_window_event_filter (SDL_WindowEvent *event,
                         CoglContext *context)
{
  SDL_Window *window;
  CoglFramebuffer *framebuffer;

  window = SDL_GetWindowFromID (event->windowID);

  if (window == NULL)
    return COGL_FILTER_CONTINUE;

  framebuffer = SDL_GetWindowData (window, COGL_SDL_WINDOW_DATA_KEY);

  if (framebuffer == NULL || framebuffer->context != context)
    return COGL_FILTER_CONTINUE;

  if (event->event == SDL_WINDOWEVENT_SIZE_CHANGED)
    {
      CoglDisplay *display = context->display;
      CoglRenderer *renderer = display->renderer;
      CoglRendererSdl2 *sdl_renderer = renderer->winsys;
      float width = event->data1;
      float height = event->data2;
      CoglOnscreenSdl2 *sdl_onscreen;

      _cogl_framebuffer_winsys_update_size (framebuffer, width, height);

      /* We only want to notify that a resize happened when the
       * application calls cogl_context_dispatch so instead of
       * immediately notifying we queue an idle callback */
      if (!sdl_renderer->resize_notify_idle)
        {
          sdl_renderer->resize_notify_idle =
            _cogl_poll_renderer_add_idle (renderer,
                                          flush_pending_resize_notifications_idle,
                                          context,
                                          NULL);
        }

      sdl_onscreen = COGL_ONSCREEN (framebuffer)->winsys;
      sdl_onscreen->pending_resize_notify = TRUE;
    }
  else if (event->event == SDL_WINDOWEVENT_EXPOSED)
    {
      CoglOnscreenDirtyInfo info;

      /* Sadly SDL doesn't seem to report the rectangle of the expose
       * event so we'll just queue the whole window */
      info.x = 0;
      info.y = 0;
      info.width = framebuffer->width;
      info.height = framebuffer->height;

      _cogl_onscreen_queue_dirty (COGL_ONSCREEN (framebuffer), &info);
    }

  return COGL_FILTER_CONTINUE;
}

static CoglFilterReturn
sdl_event_filter_cb (SDL_Event *event, void *data)
{
  CoglContext *context = data;

  switch (event->type)
    {
    case SDL_WINDOWEVENT:
      return sdl_window_event_filter (&event->window, context);

    default:
      return COGL_FILTER_CONTINUE;
    }
}

static CoglBool
_cogl_winsys_context_init (CoglContext *context, CoglError **error)
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

  /* We'll manually handle queueing dirty events in response to
   * SDL_WINDOWEVENT_EXPOSED events */
  COGL_FLAGS_SET (context->private_features,
                  COGL_PRIVATE_FEATURE_DIRTY_EVENTS,
                  TRUE);

  _cogl_renderer_add_native_filter (renderer,
                                    (CoglNativeFilterFunc) sdl_event_filter_cb,
                                    context);

  return TRUE;
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
  CoglRenderer *renderer = context->display->renderer;

  _cogl_renderer_remove_native_filter (renderer,
                                       (CoglNativeFilterFunc)
                                       sdl_event_filter_cb,
                                       context);

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
        {
          CoglDisplaySdl2 *sdl_display = context->display->winsys;

          /* SDL explicitly unbinds the context when the currently
           * bound window is destroyed. Cogl always needs a context
           * bound so that for example it can create texture resources
           * at any time even without flushing a framebuffer.
           * Therefore we'll bind the dummy window. */
          SDL_GL_MakeCurrent (sdl_display->dummy_window,
                              sdl_display->context);
          sdl_context->current_window = sdl_display->dummy_window;
        }

      SDL_DestroyWindow (sdl_onscreen->window);
      sdl_onscreen->window = NULL;
    }

  g_slice_free (CoglOnscreenSdl2, sdl_onscreen);
  onscreen->winsys = NULL;
}

static CoglBool
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            CoglError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglOnscreenSdl2 *sdl_onscreen;
  SDL_Window *window;
  int width, height;
  SDL_WindowFlags flags;

  width = cogl_framebuffer_get_width (framebuffer);
  height = cogl_framebuffer_get_height (framebuffer);

  flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;

  /* The resizable property on SDL window apparently can only be set
   * on creation */
  if (onscreen->resizable)
    flags |= SDL_WINDOW_RESIZABLE;

  window = SDL_CreateWindow ("" /* title */,
                             0, 0, /* x/y */
                             width, height,
                             flags);

  if (window == NULL)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "SDL_CreateWindow failed: %s",
                       SDL_GetError ());
      return FALSE;
    }

  SDL_SetWindowData (window, COGL_SDL_WINDOW_DATA_KEY, onscreen);

  onscreen->winsys = g_slice_new (CoglOnscreenSdl2);
  sdl_onscreen = onscreen->winsys;
  sdl_onscreen->window = window;

  return TRUE;
}

static void
_cogl_winsys_onscreen_swap_buffers_with_damage (CoglOnscreen *onscreen,
                                                const int *rectangles,
                                                int n_rectangles)
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

SDL_Window *
cogl_sdl_onscreen_get_window (CoglOnscreen *onscreen)
{
  CoglOnscreenSdl2 *sdl_onscreen;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_onscreen (onscreen), NULL);

  if (!cogl_framebuffer_allocate (COGL_FRAMEBUFFER (onscreen), NULL))
    return NULL;

  sdl_onscreen = onscreen->winsys;

  return sdl_onscreen->window;
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
      vtable.onscreen_swap_buffers_with_damage =
        _cogl_winsys_onscreen_swap_buffers_with_damage;
      vtable.onscreen_update_swap_throttled =
        _cogl_winsys_onscreen_update_swap_throttled;
      vtable.onscreen_set_visibility = _cogl_winsys_onscreen_set_visibility;

      vtable_inited = TRUE;
    }

  return &vtable;
}
