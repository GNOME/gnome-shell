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
 * Authors:
 *  Neil Roberts <neil@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_POLL_H__
#define __COGL_POLL_H__

#include <cogl/cogl-defines.h>
#include <cogl/cogl-context.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-poll
 * @short_description: Functions for integrating Cogl with an
 *   application's main loop
 *
 * Cogl needs to integrate with the application's main loop so that it
 * can internally handle some events from the driver. All Cogl
 * applications must use these functions. They provide enough
 * information to describe the state that Cogl will need to wake up
 * on. An application using the GLib main loop can instead use
 * cogl_glib_source_new() which provides a #GSource ready to be added
 * to the main loop.
 */

/**
 * CoglPollFDEvent:
 * @COGL_POLL_FD_EVENT_IN: there is data to read
 * @COGL_POLL_FD_EVENT_PRI: data can be written (without blocking)
 * @COGL_POLL_FD_EVENT_OUT: there is urgent data to read.
 * @COGL_POLL_FD_EVENT_ERR: error condition
 * @COGL_POLL_FD_EVENT_HUP: hung up (the connection has been broken, usually
 *                          for pipes and sockets).
 * @COGL_POLL_FD_EVENT_NVAL: invalid request. The file descriptor is not open.
 *
 * A bitmask of events that Cogl may need to wake on for a file
 * descriptor. Note that these all have the same values as the
 * corresponding defines for the poll function call on Unix so they
 * may be directly passed to poll.
 *
 * Since: 1.10
 * Stability: unstable
 */
typedef enum
{
  COGL_POLL_FD_EVENT_IN = COGL_SYSDEF_POLLIN,
  COGL_POLL_FD_EVENT_PRI = COGL_SYSDEF_POLLPRI,
  COGL_POLL_FD_EVENT_OUT = COGL_SYSDEF_POLLOUT,
  COGL_POLL_FD_EVENT_ERR = COGL_SYSDEF_POLLERR,
  COGL_POLL_FD_EVENT_HUP = COGL_SYSDEF_POLLHUP,
  COGL_POLL_FD_EVENT_NVAL = COGL_SYSDEF_POLLNVAL
} CoglPollFDEvent;

/**
 * CoglPollFD:
 * @fd: The file descriptor to block on
 * @events: A bitmask of events to block on
 * @revents: A bitmask of returned events
 *
 * A struct for describing the state of a file descriptor that Cogl
 * needs to block on. The @events field contains a bitmask of
 * #CoglPollFDEvent<!-- -->s that should cause the application to wake
 * up. After the application is woken up from idle it should pass back
 * an array of #CoglPollFD<!-- -->s to Cogl and update the @revents
 * mask to the actual events that occurred on the file descriptor.
 *
 * Note that CoglPollFD is deliberately exactly the same as struct
 * pollfd on Unix so that it can simply be cast when calling poll.
 *
 * Since: 1.10
 * Stability: unstable
 */
typedef struct {
  int fd;
  short int events;
  short int revents;
} CoglPollFD;

/**
 * cogl_poll_get_info:
 * @context: A #CoglContext
 * @poll_fds: A return location for a pointer to an array
 *            of #CoglPollFD<!-- -->s
 * @n_poll_fds: A return location for the number of entries in *@poll_fds
 * @timeout: A return location for the maximum length of time to wait
 *           in microseconds, or -1 to wait indefinitely.
 *
 * This should be called whenever an application is about to go idle
 * so that Cogl has a chance to describe what state it needs to be
 * woken up on. The assumption is that the application is using a main
 * loop with something like the poll function call on Unix or the GLib
 * main loop.
 *
 * After the function is called *@poll_fds will contain a pointer to
 * an array of #CoglPollFD structs describing the file descriptors
 * that Cogl expects. The fd and events members will be updated
 * accordingly. After the application has completed its idle it is
 * expected to either update the revents members directly in this
 * array or to create a copy of the array and update them
 * there. Either way it should pass a pointer to either array back to
 * Cogl when calling cogl_poll_dispatch().
 *
 * When using the %COGL_WINSYS_ID_WGL winsys (where file descriptors
 * don't make any sense) or %COGL_WINSYS_ID_SDL (where the event
 * handling functions of SDL don't allow blocking on a file
 * descriptor) *n_poll_fds is guaranteed to be zero.
 *
 * @timeout will contain a maximum amount of time to wait in
 * microseconds before the application should wake up or -1 if the
 * application should wait indefinitely. This can also be 0 zero if
 * Cogl needs to be woken up immediately.
 *
 * Stability: unstable
 * Since: 1.10
 */
void
cogl_poll_get_info (CoglContext *context,
                    CoglPollFD **poll_fds,
                    int *n_poll_fds,
                    int64_t *timeout);

/**
 * cogl_poll_dispatch:
 * @context: A #CoglContext
 * @poll_fds: An array of #CoglPollFD<!-- -->s describing the events
 *            that have occurred since the application went idle.
 * @n_poll_fds: The length of the @poll_fds array.
 *
 * This should be called whenever an application is woken up from
 * going idle in its main loop. The @poll_fds array should contain a
 * list of file descriptors matched with the events that occurred in
 * revents. The events field is ignored. It is safe to pass in extra
 * file descriptors that Cogl didn't request from
 * cogl_context_begin_idle() or a shorter array missing some file
 * descriptors that Cogl requested.
 *
 * Stability: unstable
 * Since: 1.10
 */
void
cogl_poll_dispatch (CoglContext *context,
                    const CoglPollFD *poll_fds,
                    int n_poll_fds);

COGL_END_DECLS

#endif /* __COGL_POLL_H__ */
