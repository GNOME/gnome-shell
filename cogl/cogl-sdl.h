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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_SDL_H__
#define __COGL_SDL_H__

/* NB: this is a top-level header that can be included directly but we
 * want to be careful not to define __COGL_H_INSIDE__ when this is
 * included internally while building Cogl itself since
 * __COGL_H_INSIDE__ is used in headers to guard public vs private api
 * definitions
 */
#ifndef COGL_COMPILATION

/* Note: When building Cogl .gir we explicitly define
 * __COGL_H_INSIDE__ */
#ifndef __COGL_H_INSIDE__
#define __COGL_H_INSIDE__
#define __COGL_SDL_H_MUST_UNDEF_COGL_H_INSIDE__
#endif

#endif /* COGL_COMPILATION */

#include <cogl/cogl-context.h>
#include <cogl/cogl-onscreen.h>
#include <SDL.h>

#ifdef _MSC_VER
/* We need to link to SDL.lib/SDLmain.lib
 * if we are using Cogl
 * that uses the SDL winsys
 */
#pragma comment (lib, "SDL.lib")
#pragma comment (lib, "SDLmain.lib")
#endif

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-sdl
 * @short_description: Integration api for the Simple DirectMedia
 *                     Layer library.
 *
 * Cogl is a portable graphics api that can either be used standalone
 * or alternatively integrated with certain existing frameworks. This
 * api enables Cogl to be used in conjunction with the Simple
 * DirectMedia Layer library.
 *
 * Using this API a typical SDL application would look something like
 * this:
 * |[
 * MyAppData data;
 * CoglError *error = NULL;
 *
 * data.ctx = cogl_sdl_context_new (SDL_USEREVENT, &error);
 * if (!data.ctx)
 *   {
 *     fprintf (stderr, "Failed to create context: %s\n",
 *              error->message);
 *     return 1;
 *   }
 *
 * my_application_setup (&data);
 *
 * data.redraw_queued = TRUE;
 * while (!data.quit)
 *   {
 *     while (!data.quit)
 *       {
 *         if (!SDL_PollEvent (&event))
 *           {
 *             if (data.redraw_queued)
 *               break;
 *
 *             cogl_sdl_idle (ctx);
 *             if (!SDL_WaitEvent (&event))
 *               {
 *                 fprintf (stderr, "Error waiting for SDL events");
 *                 return 1;
 *               }
 *           }
 *
 *          handle_event (&data, &event);
 *          cogl_sdl_handle_event (ctx, &event);
 *        }
 *
 *     data.redraw_queued = redraw (&data);
 *   }
 * ]|
 */

/**
 * cogl_sdl_context_new:
 * @type: An SDL user event type between <constant>SDL_USEREVENT</constant> and
 *        <constant>SDL_NUMEVENTS</constant> - 1
 * @error: A CoglError return location.
 *
 * This is a convenience function for creating a new #CoglContext for
 * use with SDL and specifying what SDL user event type Cogl can use
 * as a way to interrupt SDL_WaitEvent().
 *
 * This function is equivalent to the following code:
 * |[
 * CoglRenderer *renderer = cogl_renderer_new ();
 * CoglDisplay *display;
 *
 * cogl_renderer_set_winsys_id (renderer, COGL_WINSYS_ID_SDL);
 *
 * cogl_sdl_renderer_set_event_type (renderer, type);
 *
 * if (!cogl_renderer_connect (renderer, error))
 *   return NULL;
 *
 * display = cogl_display_new (renderer, NULL);
 * if (!cogl_display_setup (display, error))
 *   return NULL;
 *
 * return cogl_context_new (display, error);
 * ]|
 *
 * <note>SDL applications are required to either use this API or
 * to manually create a #CoglRenderer and call
 * cogl_sdl_renderer_set_event_type().</note>
 *
 * Since: 2.0
 * Stability: unstable
 */
CoglContext *
cogl_sdl_context_new (int type, CoglError **error);

/**
 * cogl_sdl_renderer_set_event_type:
 * @renderer: A #CoglRenderer
 * @type: An SDL user event type between <constant>SDL_USEREVENT</constant> and
 *        <constant>SDL_NUMEVENTS</constant> - 1
 *
 * Tells Cogl what SDL user event type it can use as a way to
 * interrupt SDL_WaitEvent() to ensure that cogl_sdl_handle_event()
 * will be called in a finite amount of time.
 *
 * <note>This should only be called on an un-connected
 * @renderer.</note>
 *
 * <note>For convenience most simple applications can use
 * cogl_sdl_context_new() if they don't want to manually create
 * #CoglRenderer and #CoglDisplay objects during
 * initialization.</note>
 *
 * Since: 2.0
 * Stability: unstable
 */
void
cogl_sdl_renderer_set_event_type (CoglRenderer *renderer, int type);

/**
 * cogl_sdl_renderer_get_event_type:
 * @renderer: A #CoglRenderer
 *
 * Queries what SDL user event type Cogl is using as a way to
 * interrupt SDL_WaitEvent(). This is set either using
 * cogl_sdl_context_new or by using
 * cogl_sdl_renderer_set_event_type().
 *
 * Since: 2.0
 * Stability: unstable
 */
int
cogl_sdl_renderer_get_event_type (CoglRenderer *renderer);

/**
 * cogl_sdl_handle_event:
 * @context: A #CoglContext
 * @event: An SDL event
 *
 * Passes control to Cogl so that it may dispatch any internal event
 * callbacks in response to the given SDL @event. This function must
 * be called for every SDL event.
 *
 * Since: 2.0
 * Stability: unstable
 */
void
cogl_sdl_handle_event (CoglContext *context, SDL_Event *event);

/**
 * cogl_sdl_idle:
 * @context: A #CoglContext
 *
 * Notifies Cogl that the application is idle and about to call
 * SDL_WaitEvent(). Cogl may use this to run low priority book keeping
 * tasks.
 *
 * Since: 2.0
 * Stability: unstable
 */
void
cogl_sdl_idle (CoglContext *context);

#if SDL_MAJOR_VERSION >= 2

/**
 * cogl_sdl_onscreen_get_window:
 * @onscreen: A #CoglOnscreen
 *
 * Returns: the underlying SDL_Window associated with an onscreen framebuffer.
 *
 * Since: 2.0
 * Stability: unstable
 */
SDL_Window *
cogl_sdl_onscreen_get_window (CoglOnscreen *onscreen);

#endif /* SDL_MAJOR_VERSION */

COGL_END_DECLS

/* The gobject introspection scanner seems to parse public headers in
 * isolation which means we need to be extra careful about how we
 * define and undefine __COGL_H_INSIDE__ used to detect when internal
 * headers are incorrectly included by developers. In the gobject
 * introspection case we have to manually define __COGL_H_INSIDE__ as
 * a commandline argument for the scanner which means we must be
 * careful not to undefine it in a header...
 */
#ifdef __COGL_SDL_H_MUST_UNDEF_COGL_H_INSIDE__
#undef __COGL_H_INSIDE__
#undef __COGL_SDL_H_MUST_UNDEF_COGL_H_INSIDE__
#endif

#endif /* __COGL_SDL_H__ */
