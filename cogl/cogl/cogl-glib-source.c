/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-glib-source.h"
#include "cogl-poll.h"

typedef struct _CoglGLibSource
{
  GSource source;

  CoglRenderer *renderer;

  GArray *poll_fds;
  int poll_fds_age;

  int64_t expiration_time;
} CoglGLibSource;

static CoglBool
cogl_glib_source_prepare (GSource *source, int *timeout)
{
  CoglGLibSource *cogl_source = (CoglGLibSource *) source;
  CoglPollFD *poll_fds;
  int n_poll_fds;
  int64_t cogl_timeout;
  int age;
  int i;

  age = cogl_poll_renderer_get_info (cogl_source->renderer,
                                     &poll_fds,
                                     &n_poll_fds,
                                     &cogl_timeout);

  /* We have to be careful not to call g_source_add/remove_poll unless
   * the FDs have changed because it will cause the main loop to
   * immediately wake up. If we call it every time the source is
   * prepared it will effectively never go idle. */
  if (age != cogl_source->poll_fds_age)
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

  cogl_source->poll_fds_age = age;

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

  cogl_poll_renderer_dispatch (cogl_source->renderer,
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
cogl_glib_renderer_source_new (CoglRenderer *renderer,
                               int priority)
{
  GSource *source;
  CoglGLibSource *cogl_source;

  source = g_source_new (&cogl_glib_source_funcs,
                         sizeof (CoglGLibSource));
  cogl_source = (CoglGLibSource *) source;

  cogl_source->renderer = renderer;
  cogl_source->poll_fds = g_array_new (FALSE, FALSE, sizeof (GPollFD));

  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority (source, priority);

  return source;
}

GSource *
cogl_glib_source_new (CoglContext *context,
                      int priority)
{
  return cogl_glib_renderer_source_new (cogl_context_get_renderer (context),
                                        priority);
}


