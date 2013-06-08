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

#ifndef _COGL_CLOSURE_LIST_PRIVATE_H_
#define _COGL_CLOSURE_LIST_PRIVATE_H_

#include "cogl-object.h"
#include "cogl-list.h"

/*
 * This implements a list of callbacks that can be used a bit like
 * signals in GObject, but that don't have any marshalling overhead.
 *
 * The idea is that any Cogl code that wants to provide a callback
 * point will provide api to add a callback for that particular point.
 * The function can take a function pointer with the correct
 * signature.  Internally the Cogl code can use _cogl_closure_list_add,
 * _cogl_closure_disconnect and _cogl_closure_list_disconnect_all
 *
 * In the future we could consider exposing the CoglClosure type which
 * would allow applications to use _cogl_closure_disconnect() directly
 * so we don't need to expose new disconnect apis for each callback
 * point.
 */

typedef struct _CoglClosure
{
  CoglList link;

  void *function;
  void *user_data;
  CoglUserDataDestroyCallback destroy_cb;
} CoglClosure;

/*
 * _cogl_closure_disconnect:
 * @closure: A closure connected to a Cogl closure list
 *
 * Removes the given closure from the callback list it is connected to
 * and destroys it. If the closure was created with a destroy function
 * then it will be invoked. */
void
_cogl_closure_disconnect (CoglClosure *closure);

void
_cogl_closure_list_disconnect_all (CoglList *list);

CoglClosure *
_cogl_closure_list_add (CoglList *list,
                        void *function,
                        void *user_data,
                        CoglUserDataDestroyCallback destroy_cb);

/*
 * _cogl_closure_list_invoke:
 * @list: A pointer to a CoglList containing CoglClosures
 * @cb_type: The name of a typedef for the closure callback function signature
 * @...: The the arguments to pass to the callback
 *
 * A convenience macro to invoke a closure list with a variable number
 * of arguments that will be passed to the closure callback functions.
 *
 * Note that the arguments will be evaluated multiple times so it is
 * not safe to pass expressions that have side-effects.
 *
 * Note also that this function ignores the return value from the
 * callbacks. If you want to handle the return value you should
 * manually iterate the list and invoke the callbacks yourself.
 */
#define _cogl_closure_list_invoke(list, cb_type, ...)   \
  G_STMT_START {                                        \
    CoglClosure *_c, *_tmp;                             \
                                                        \
    _cogl_list_for_each_safe (_c, _tmp, (list), link)   \
      {                                                 \
        cb_type _cb = _c->function;                     \
        _cb (__VA_ARGS__, _c->user_data);               \
      }                                                 \
  } G_STMT_END

#define _cogl_closure_list_invoke_no_args(list)         \
  G_STMT_START {                                        \
    CoglClosure *_c, *_tmp;                             \
                                                        \
    _cogl_list_for_each_safe (_c, _tmp, (list), link)   \
      {                                                 \
        void (*_cb)(void *) = _c->function;             \
        _cb (_c->user_data);                            \
      }                                                 \
  } G_STMT_END

#endif /* _COGL_CLOSURE_LIST_PRIVATE_H_ */
