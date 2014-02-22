/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2013 Intel Corporation.
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

#ifndef __COGL_POLL_PRIVATE_H__
#define __COGL_POLL_PRIVATE_H__

#include "cogl-poll.h"
#include "cogl-renderer.h"
#include "cogl-closure-list-private.h"

void
_cogl_poll_renderer_remove_fd (CoglRenderer *renderer, int fd);

typedef int64_t (*CoglPollPrepareCallback) (void *user_data);
typedef void (*CoglPollDispatchCallback) (void *user_data, int revents);

void
_cogl_poll_renderer_add_fd (CoglRenderer *renderer,
                            int fd,
                            CoglPollFDEvent events,
                            CoglPollPrepareCallback prepare,
                            CoglPollDispatchCallback dispatch,
                            void *user_data);

void
_cogl_poll_renderer_modify_fd (CoglRenderer *renderer,
                               int fd,
                               CoglPollFDEvent events);

typedef struct _CoglPollSource CoglPollSource;

CoglPollSource *
_cogl_poll_renderer_add_source (CoglRenderer *renderer,
                                CoglPollPrepareCallback prepare,
                                CoglPollDispatchCallback dispatch,
                                void *user_data);

void
_cogl_poll_renderer_remove_source (CoglRenderer *renderer,
                                   CoglPollSource *source);

typedef void (*CoglIdleCallback) (void *user_data);

CoglClosure *
_cogl_poll_renderer_add_idle (CoglRenderer *renderer,
                              CoglIdleCallback idle_cb,
                              void *user_data,
                              CoglUserDataDestroyCallback destroy_cb);

#endif /* __COGL_POLL_PRIVATE_H__ */
