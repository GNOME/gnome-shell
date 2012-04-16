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
#include "cogl-winsys-private.h"
#include "cogl-context-private.h"

void
cogl_poll_get_info (CoglContext *context,
                    CoglPollFD **poll_fds,
                    int *n_poll_fds,
                    int64_t *timeout)
{
  const CoglWinsysVtable *winsys;

  _COGL_RETURN_IF_FAIL (cogl_is_context (context));
  _COGL_RETURN_IF_FAIL (poll_fds != NULL);
  _COGL_RETURN_IF_FAIL (n_poll_fds != NULL);
  _COGL_RETURN_IF_FAIL (timeout != NULL);

  winsys = _cogl_context_get_winsys (context);

  if (winsys->poll_get_info)
    {
      winsys->poll_get_info (context,
                             poll_fds,
                             n_poll_fds,
                             timeout);
      return;
    }

  /* By default we'll assume Cogl doesn't need to block on anything */
  *poll_fds = NULL;
  *n_poll_fds = 0;
  *timeout = -1; /* no timeout */
}

void
cogl_poll_dispatch (CoglContext *context,
                    const CoglPollFD *poll_fds,
                    int n_poll_fds)
{
  const CoglWinsysVtable *winsys;

  _COGL_RETURN_IF_FAIL (cogl_is_context (context));

  winsys = _cogl_context_get_winsys (context);

  if (winsys->poll_dispatch)
    winsys->poll_dispatch (context, poll_fds, n_poll_fds);
}
