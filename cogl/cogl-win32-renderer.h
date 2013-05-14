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
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_WIN32_RENDERER_H__
#define __COGL_WIN32_RENDERER_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-renderer.h>

COGL_BEGIN_DECLS

/**
 * cogl_win32_renderer_handle_event:
 * @renderer: a #CoglRenderer
 * @message: A pointer to a win32 MSG struct
 *
 * This function processes a single event; it can be used to hook into
 * external event retrieval (for example that done by Clutter or
 * GDK).
 *
 * Return value: #CoglFilterReturn. %COGL_FILTER_REMOVE indicates that
 * Cogl has internally handled the event and the caller should do no
 * further processing. %COGL_FILTER_CONTINUE indicates that Cogl is
 * either not interested in the event, or has used the event to update
 * internal state without taking any exclusive action.
 */
CoglFilterReturn
cogl_win32_renderer_handle_event (CoglRenderer *renderer,
                                  MSG *message);

/**
 * CoglWin32FilterFunc:
 * @message: A pointer to a win32 MSG struct
 * @data: The data that was given when the filter was added
 *
 * A callback function that can be registered with
 * cogl_win32_renderer_add_filter(). The function should return
 * %COGL_FILTER_REMOVE if it wants to prevent further processing or
 * %COGL_FILTER_CONTINUE otherwise.
 */
typedef CoglFilterReturn (* CoglWin32FilterFunc) (MSG *message,
                                                  void *data);

/**
 * cogl_win32_renderer_add_filter:
 * @renderer: a #CoglRenderer
 * @func: the callback function
 * @data: user data passed to @func when called
 *
 * Adds a callback function that will receive all native events. The
 * function can stop further processing of the event by return
 * %COGL_FILTER_REMOVE.
 */
void
cogl_win32_renderer_add_filter (CoglRenderer *renderer,
                                CoglWin32FilterFunc func,
                                void *data);

/**
 * cogl_win32_renderer_remove_filter:
 * @renderer: a #CoglRenderer
 * @func: the callback function
 * @data: user data given when the callback was installed
 *
 * Removes a callback that was previously added with
 * cogl_win32_renderer_add_filter().
 */
void
cogl_win32_renderer_remove_filter (CoglRenderer *renderer,
                                   CoglWin32FilterFunc func,
                                   void *data);

/**
 * cogl_win32_renderer_set_event_retrieval_enabled:
 * @renderer: a #CoglRenderer
 * @enable: The new value
 *
 * Sets whether Cogl should automatically retrieve messages from
 * Windows. It defaults to %TRUE. It can be set to %FALSE if the
 * application wants to handle its own message retrieval. Note that
 * Cogl still needs to see all of the messages to function properly so
 * the application should call cogl_win32_renderer_handle_event() for
 * each message if it disables automatic event retrieval.
 *
 * Since: 1.16
 * Stability: unstable
 */
void
cogl_win32_renderer_set_event_retrieval_enabled (CoglRenderer *renderer,
                                                 CoglBool enable);

COGL_END_DECLS

#endif /* __COGL_WIN32_RENDERER_H__ */
