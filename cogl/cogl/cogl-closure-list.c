/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012,2013 Intel Corporation.
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
 */

#include <config.h>

#include <glib.h>

#include "cogl-closure-list-private.h"

void
_cogl_closure_disconnect (CoglClosure *closure)
{
  _cogl_list_remove (&closure->link);

  if (closure->destroy_cb)
    closure->destroy_cb (closure->user_data);

  g_slice_free (CoglClosure, closure);
}

void
_cogl_closure_list_disconnect_all (CoglList *list)
{
  CoglClosure *closure, *next;

  _cogl_list_for_each_safe (closure, next, list, link)
    _cogl_closure_disconnect (closure);
}

CoglClosure *
_cogl_closure_list_add (CoglList *list,
                        void *function,
                        void *user_data,
                        CoglUserDataDestroyCallback destroy_cb)
{
  CoglClosure *closure = g_slice_new (CoglClosure);

  closure->function = function;
  closure->user_data = user_data;
  closure->destroy_cb = destroy_cb;

  _cogl_list_insert (list, &closure->link);

  return closure;
}
