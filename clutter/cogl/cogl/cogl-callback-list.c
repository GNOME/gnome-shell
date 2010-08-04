/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-callback-list.h"

typedef struct _CoglCallbackListClosure
{
  CoglCallbackListFunc func;
  void *user_data;
} CoglCallbackListClosure;

void
_cogl_callback_list_init (CoglCallbackList *list)
{
  list->funcs = NULL;
}

void
_cogl_callback_list_destroy (CoglCallbackList *list)
{
  while (list->funcs)
    {
      CoglCallbackListClosure *closure = list->funcs->data;

      _cogl_callback_list_remove (list, closure->func, closure->user_data);
    }
}

void
_cogl_callback_list_add (CoglCallbackList *list,
                         CoglCallbackListFunc func,
                         void *user_data)
{
  CoglCallbackListClosure *closure = g_slice_new (CoglCallbackListClosure);

  closure->func = func;
  closure->user_data = user_data;
  list->funcs = g_slist_prepend (list->funcs, closure);
}

void
_cogl_callback_list_remove (CoglCallbackList *list,
                            CoglCallbackListFunc func,
                            void *user_data)
{
  GSList *prev = NULL, *l;

  for (l = list->funcs; l; l = l->next)
    {
      CoglCallbackListClosure *closure = l->data;

      if (closure->func == func && closure->user_data == user_data)
        {
          g_slice_free (CoglCallbackListClosure, closure);

          if (prev)
            prev->next = g_slist_delete_link (prev->next, l);
          else
            list->funcs = g_slist_delete_link (list->funcs, l);

          break;
        }

      prev = l;
    }
}

void
_cogl_callback_list_invoke (CoglCallbackList *list)
{
  GSList *l;

  for (l = list->funcs; l; l = l->next)
    {
      CoglCallbackListClosure *closure = l->data;

      closure->func (closure->user_data);
    }
}
