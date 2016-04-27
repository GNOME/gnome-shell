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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-poll.h"
#include "cogl-poll-private.h"
#include "cogl-winsys-private.h"
#include "cogl-renderer-private.h"

struct _CoglPollSource
{
  int fd;
  CoglPollPrepareCallback prepare;
  CoglPollDispatchCallback dispatch;
  void *user_data;
};

int
cogl_poll_renderer_get_info (CoglRenderer *renderer,
                             CoglPollFD **poll_fds,
                             int *n_poll_fds,
                             int64_t *timeout)
{
  GList *l, *next;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), 0);
  _COGL_RETURN_VAL_IF_FAIL (poll_fds != NULL, 0);
  _COGL_RETURN_VAL_IF_FAIL (n_poll_fds != NULL, 0);
  _COGL_RETURN_VAL_IF_FAIL (timeout != NULL, 0);

  *timeout = -1;

  if (!_cogl_list_empty (&renderer->idle_closures))
    *timeout = 0;

  /* This loop needs to cope with the prepare callback removing its
   * own fd */
  for (l = renderer->poll_sources; l; l = next)
    {
      CoglPollSource *source = l->data;

      next = l->next;

      if (source->prepare)
        {
          int64_t source_timeout = source->prepare (source->user_data);
          if (source_timeout >= 0 &&
              (*timeout == -1 || *timeout > source_timeout))
            *timeout = source_timeout;
        }
    }

  /* This is deliberately set after calling the prepare callbacks in
   * case one of them removes its fd */
  *poll_fds = (void *)renderer->poll_fds->data;
  *n_poll_fds = renderer->poll_fds->len;

  return renderer->poll_fds_age;
}

void
cogl_poll_renderer_dispatch (CoglRenderer *renderer,
                             const CoglPollFD *poll_fds,
                             int n_poll_fds)
{
  GList *l, *next;

  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));

  _cogl_closure_list_invoke_no_args (&renderer->idle_closures);

  /* This loop needs to cope with the dispatch callback removing its
   * own fd */
  for (l = renderer->poll_sources; l; l = next)
    {
      CoglPollSource *source = l->data;
      int i;

      next = l->next;

      if (source->fd == -1)
        {
          source->dispatch (source->user_data, 0);
          continue;
        }

      for (i = 0; i < n_poll_fds; i++)
        {
          const CoglPollFD *pollfd = &poll_fds[i];

          if (pollfd->fd == source->fd)
            {
              source->dispatch (source->user_data, pollfd->revents);
              break;
            }
        }
    }
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
  GList *l;

  if (i < 0)
    return;

  g_array_remove_index_fast (renderer->poll_fds, i);
  renderer->poll_fds_age++;

  for (l = renderer->poll_sources; l; l = l->next)
    {
      CoglPollSource *source = l->data;
      if (source->fd == fd)
        {
          renderer->poll_sources =
            g_list_delete_link (renderer->poll_sources, l);
          g_slice_free (CoglPollSource, source);
          break;
        }
    }
}

void
_cogl_poll_renderer_modify_fd (CoglRenderer *renderer,
                               int fd,
                               CoglPollFDEvent events)
{
  int fd_index = find_pollfd (renderer, fd);

  if (fd_index == -1)
    g_warn_if_reached ();
  else
    {
      CoglPollFD *pollfd =
        &g_array_index (renderer->poll_sources, CoglPollFD, fd_index);

      pollfd->events = events;
      renderer->poll_fds_age++;
    }
}

void
_cogl_poll_renderer_add_fd (CoglRenderer *renderer,
                            int fd,
                            CoglPollFDEvent events,
                            CoglPollPrepareCallback prepare,
                            CoglPollDispatchCallback dispatch,
                            void *user_data)
{
  CoglPollFD pollfd = {
    fd,
    events
  };
  CoglPollSource *source;

  _cogl_poll_renderer_remove_fd (renderer, fd);

  source = g_slice_new0 (CoglPollSource);
  source->fd = fd;
  source->prepare = prepare;
  source->dispatch = dispatch;
  source->user_data = user_data;

  renderer->poll_sources = g_list_prepend (renderer->poll_sources, source);

  g_array_append_val (renderer->poll_fds, pollfd);
  renderer->poll_fds_age++;
}

CoglPollSource *
_cogl_poll_renderer_add_source (CoglRenderer *renderer,
                                CoglPollPrepareCallback prepare,
                                CoglPollDispatchCallback dispatch,
                                void *user_data)
{
  CoglPollSource *source;

  source = g_slice_new0 (CoglPollSource);
  source->fd = -1;
  source->prepare = prepare;
  source->dispatch = dispatch;
  source->user_data = user_data;

  renderer->poll_sources = g_list_prepend (renderer->poll_sources, source);

  return source;
}

void
_cogl_poll_renderer_remove_source (CoglRenderer *renderer,
                                   CoglPollSource *source)
{
  GList *l;

  for (l = renderer->poll_sources; l; l = l->next)
    {
      if (l->data == source)
        {
          renderer->poll_sources =
            g_list_delete_link (renderer->poll_sources, l);
          g_slice_free (CoglPollSource, source);
          break;
        }
    }
}

CoglClosure *
_cogl_poll_renderer_add_idle (CoglRenderer *renderer,
                              CoglIdleCallback idle_cb,
                              void *user_data,
                              CoglUserDataDestroyCallback destroy_cb)
{
  return _cogl_closure_list_add (&renderer->idle_closures,
                                idle_cb,
                                user_data,
                                destroy_cb);
}
