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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-glib-source.h"
#include "cogl-poll.h"

typedef struct _CoglGLibSource
{
  GSource source;

  CoglContext *context;

  GArray *poll_fds;

  int64_t expiration_time;
} CoglGLibSource;

static CoglBool
cogl_glib_source_poll_fds_changed (CoglGLibSource *cogl_source,
                                   const CoglPollFD *poll_fds,
                                   int n_poll_fds)
{
  int i;

  if (cogl_source->poll_fds->len != n_poll_fds)
    return TRUE;

  for (i = 0; i < n_poll_fds; i++)
    if (g_array_index (cogl_source->poll_fds, CoglPollFD, i).fd !=
        poll_fds[i].fd)
      return TRUE;

  return FALSE;
}

static CoglBool
cogl_glib_source_prepare (GSource *source, int *timeout)
{
  CoglGLibSource *cogl_source = (CoglGLibSource *) source;
  CoglPollFD *poll_fds;
  int n_poll_fds;
  int64_t cogl_timeout;
  int i;

  cogl_poll_get_info (cogl_source->context,
                      &poll_fds,
                      &n_poll_fds,
                      &cogl_timeout);

  /* We have to be careful not to call g_source_add/remove_poll unless
     the FDs have changed because it will cause the main loop to
     immediately wake up. If we call it every time the source is
     prepared it will effectively never go idle. */
  if (cogl_glib_source_poll_fds_changed (cogl_source, poll_fds, n_poll_fds))
    {
      /* Remove any existing polls before adding the new ones */
      for (i = 0; i < cogl_source->poll_fds->len; i++)
        {
          GPollFD *poll_fd = &g_array_index (cogl_source->poll_fds, GPollFD, i);
          g_source_remove_poll (source, poll_fd);
        }

      g_array_set_size (cogl_source->poll_fds, n_poll_fds);

      for (i = 0; i < n_poll_fds; i++)
        {
          GPollFD *poll_fd = &g_array_index (cogl_source->poll_fds, GPollFD, i);
          poll_fd->fd = poll_fds[i].fd;
          g_source_add_poll (source, poll_fd);
        }
    }

  /* Update the events */
  for (i = 0; i < n_poll_fds; i++)
    {
      GPollFD *poll_fd = &g_array_index (cogl_source->poll_fds, GPollFD, i);
      poll_fd->events = poll_fds[i].events;
      poll_fd->revents = 0;
    }

  if (cogl_timeout == -1)
    {
      *timeout = -1;
      cogl_source->expiration_time = -1;
    }
  else
    {
      /* Round up to ensure that we don't try again too early */
      *timeout = (cogl_timeout + 999) / 1000;
      cogl_source->expiration_time = (g_source_get_time (source) +
                                      cogl_timeout);
    }

  return *timeout == 0;
}

static CoglBool
cogl_glib_source_check (GSource *source)
{
  CoglGLibSource *cogl_source = (CoglGLibSource *) source;
  int i;

  if (cogl_source->expiration_time >= 0 &&
      g_source_get_time (source) >= cogl_source->expiration_time)
    return TRUE;

  for (i = 0; i < cogl_source->poll_fds->len; i++)
    {
      GPollFD *poll_fd = &g_array_index (cogl_source->poll_fds, GPollFD, i);
      if (poll_fd->revents != 0)
        return TRUE;
    }

  return FALSE;
}

static CoglBool
cogl_glib_source_dispatch (GSource *source,
                           GSourceFunc callback,
                           void *user_data)
{
  CoglGLibSource *cogl_source = (CoglGLibSource *) source;
  CoglPollFD *poll_fds =
    (CoglPollFD *) &g_array_index (cogl_source->poll_fds, GPollFD, 0);

  cogl_poll_dispatch (cogl_source->context,
                      poll_fds,
                      cogl_source->poll_fds->len);

  return TRUE;
}

static void
cogl_glib_source_finalize (GSource *source)
{
  CoglGLibSource *cogl_source = (CoglGLibSource *) source;

  g_array_free (cogl_source->poll_fds, TRUE);
}

static GSourceFuncs
cogl_glib_source_funcs =
  {
    cogl_glib_source_prepare,
    cogl_glib_source_check,
    cogl_glib_source_dispatch,
    cogl_glib_source_finalize
  };

GSource *
cogl_glib_source_new (CoglContext *context,
                      int priority)
{
  GSource *source;
  CoglGLibSource *cogl_source;

  source = g_source_new (&cogl_glib_source_funcs,
                         sizeof (CoglGLibSource));
  cogl_source = (CoglGLibSource *) source;

  cogl_source->context = context;
  cogl_source->poll_fds = g_array_new (FALSE, FALSE, sizeof (GPollFD));

  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority (source, priority);

  return source;
}
