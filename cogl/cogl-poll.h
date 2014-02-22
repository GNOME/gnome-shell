/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
 * cogl_poll_renderer_get_info:
 * @renderer: A #CoglRenderer
 * @poll_fds: A return location for a pointer to an array
 *            of #CoglPollFD<!-- -->s
 * @n_poll_fds: A return location for the number of entries in *@poll_fds
 * @timeout: A return location for the maximum length of time to wait
 *           in microseconds, or -1 to wait indefinitely.
 *
 * Is used to integrate Cogl with an application mainloop that is based
 * on the unix poll(2) api (or select() or something equivalent). This
 * api should be called whenever an application is about to go idle so
 * that Cogl has a chance to describe what file descriptor events it
 * needs to be woken up for.
 *
 * <note>If your application is using the Glib mainloop then you
 * should jump to the cogl_glib_source_new() api as a more convenient
 * way of integrating Cogl with the mainloop.</note>
 *
 * After the function is called *@poll_fds will contain a pointer to
 * an array of #CoglPollFD structs describing the file descriptors
 * that Cogl expects. The fd and events members will be updated
 * accordingly. After the application has completed its idle it is
 * expected to either update the revents members directly in this
 * array or to create a copy of the array and update them
 * there.
 *
 * When the application mainloop returns from calling poll(2) (or its
 * equivalent) then it should call cogl_poll_renderer_dispatch()
 * passing a pointer the array of CoglPollFD<!-- -->s with updated
 * revent values.
 *
 * When using the %COGL_WINSYS_ID_WGL winsys (where file descriptors
 * don't make any sense) or %COGL_WINSYS_ID_SDL (where the event
 * handling functions of SDL don't allow blocking on a file
 * descriptor) *n_poll_fds is guaranteed to be zero.
 *
 * @timeout will contain a maximum amount of time to wait in
 * microseconds before the application should wake up or -1 if the
 * application should wait indefinitely. This can also be 0 if
 * Cogl needs to be woken up immediately.
 *
 * Return value: A "poll fd state age" that changes whenever the set
 *               of poll_fds has changed. If this API is being used to
 *               integrate with another system mainloop api then
 *               knowing if the set of file descriptors and events has
 *               really changed can help avoid redundant work
 *               depending the api. The age isn't guaranteed to change
 *               when the timeout changes.
 *
 * Stability: unstable
 * Since: 1.16
 */
int
cogl_poll_renderer_get_info (CoglRenderer *renderer,
                             CoglPollFD **poll_fds,
                             int *n_poll_fds,
                             int64_t *timeout);

/**
 * cogl_poll_renderer_dispatch:
 * @renderer: A #CoglRenderer
 * @poll_fds: An array of #CoglPollFD<!-- -->s describing the events
 *            that have occurred since the application went idle.
 * @n_poll_fds: The length of the @poll_fds array.
 *
 * This should be called whenever an application is woken up from
 * going idle in its main loop. The @poll_fds array should contain a
 * list of file descriptors matched with the events that occurred in
 * revents. The events field is ignored. It is safe to pass in extra
 * file descriptors that Cogl didn't request when calling
 * cogl_poll_renderer_get_info() or a shorter array missing some file
 * descriptors that Cogl requested.
 *
 * <note>If your application didn't originally create a #CoglRenderer
 * manually then you can easily get a #CoglRenderer pointer by calling
 * cogl_get_renderer().</note>
 *
 * Stability: unstable
 * Since: 1.16
 */
void
cogl_poll_renderer_dispatch (CoglRenderer *renderer,
                             const CoglPollFD *poll_fds,
                             int n_poll_fds);

COGL_END_DECLS

#endif /* __COGL_POLL_H__ */
