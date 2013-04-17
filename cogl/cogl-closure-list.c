/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012,2013 Intel Corporation.
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
 */

#include <config.h>

#include <glib.h>

#include "cogl-closure-list-private.h"

void
_cogl_closure_disconnect (CoglClosure *closure)
{
  COGL_LIST_REMOVE (closure, list_node);

  if (closure->destroy_cb)
    closure->destroy_cb (closure->user_data);

  g_slice_free (CoglClosure, closure);
}

void
_cogl_closure_list_disconnect_all (CoglClosureList *list)
{
  CoglClosure *closure, *next;

  COGL_LIST_FOREACH_SAFE (closure, list, list_node, next)
    _cogl_closure_disconnect (closure);
}

CoglClosure *
_cogl_closure_list_add (CoglClosureList *list,
                        void *function,
                        void *user_data,
                        CoglUserDataDestroyCallback destroy_cb)
{
  CoglClosure *closure = g_slice_new (CoglClosure);

  closure->function = function;
  closure->user_data = user_data;
  closure->destroy_cb = destroy_cb;

  COGL_LIST_INSERT_HEAD (list, closure, list_node);

  return closure;
}
