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
 * Authors:
 *  Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-poll.h"
#include "cogl-poll-private.h"
#include "cogl-winsys-private.h"
#include "cogl-renderer-private.h"
#include "cogl-context-private.h"

int
cogl_poll_renderer_get_info (CoglRenderer *renderer,
                             CoglPollFD **poll_fds,
                             int *n_poll_fds,
                             int64_t *timeout)
{
  const CoglWinsysVtable *winsys;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), 0);
  _COGL_RETURN_VAL_IF_FAIL (poll_fds != NULL, 0);
  _COGL_RETURN_VAL_IF_FAIL (n_poll_fds != NULL, 0);
  _COGL_RETURN_VAL_IF_FAIL (timeout != NULL, 0);

  *poll_fds = (void *)renderer->poll_fds->data;
  *n_poll_fds = renderer->poll_fds->len;

  /* NB: This will be NULL until the renderer has been connected,
   * associated with a CoglDisplay and then a CoglContext is
   * created from that display. */
  if (renderer->context)
    {
      if (!COGL_TAILQ_EMPTY (&renderer->context->onscreen_events_queue))
        {
          *timeout = 0;
          return renderer->poll_fds_age;
        }
    }

  winsys = renderer->winsys_vtable;

  if (winsys->get_dispatch_timeout)
    *timeout = winsys->get_dispatch_timeout (renderer);
  else
    *timeout = -1;

  return renderer->poll_fds_age;
}

void
cogl_poll_renderer_dispatch (CoglRenderer *renderer,
                             const CoglPollFD *poll_fds,
                             int n_poll_fds)
{
  const CoglWinsysVtable *winsys;

  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));

  /* FIXME: arbitrary cogl components should just be able to queue
   * idle functions so that we don't have to explicitly poke into
   * CoglContext here and understand about the CoglOnscreen event
   * queue... */
  if (renderer->context)
    {
      CoglContext *context = renderer->context;

      if (!COGL_TAILQ_EMPTY (&context->onscreen_events_queue))
        _cogl_dispatch_onscreen_events (context);
    }

  winsys = renderer->winsys_vtable;

  if (winsys->poll_dispatch)
    winsys->poll_dispatch (renderer, poll_fds, n_poll_fds);
}

static int
find_pollfd (CoglRenderer *renderer, int fd)
{
  int i;

  for (i = 0; i < renderer->poll_fds->len; i++)
    {
      CoglPollFD *pollfd = &g_array_index (renderer->poll_fds, CoglPollFD, i);

      if (pollfd->fd == fd)
        return i;
    }

  return -1;
}

void
_cogl_poll_renderer_remove_fd (CoglRenderer *renderer, int fd)
{
  int i = find_pollfd (renderer, fd);

  if (i < 0)
    return;

  g_array_remove_index_fast (renderer->poll_fds, i);
  renderer->poll_fds_age++;
}

void
_cogl_poll_renderer_add_fd (CoglRenderer *renderer,
                            int fd,
                            CoglPollFDEvent events)
{
  CoglPollFD pollfd = {
    fd,
    events
  };

  _cogl_poll_renderer_remove_fd (renderer, fd);

  g_array_append_val (renderer->poll_fds, pollfd);
  renderer->poll_fds_age++;
}
