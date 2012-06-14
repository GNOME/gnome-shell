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

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_SDL_H__
#define __COGL_SDL_H__

#include <cogl/cogl-context.h>
#include <SDL.h>

G_BEGIN_DECLS

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
 * GError *error = NULL;
 *
 * data.ctx = cogl_sdl_context_new (NULL, SDL_USEREVENT, &error);
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
 * @type: An SDL user event type between %SDL_USEREVENT and
 *        %SDL_NUMEVENTS - %1
 * @error: A GError return location.
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
cogl_sdl_context_new (int type, GError **error);

/**
 * cogl_sdl_renderer_set_event_type:
 * @renderer: A #CoglRenderer
 * @type: An SDL user event type between %SDL_USEREVENT and
 *        %SDL_NUMEVENTS - %1
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

G_END_DECLS

#endif /* __COGL_SDL_H__ */
