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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __COGL_CALLBACK_LIST_H__
#define __COGL_CALLBACK_LIST_H__

#include <glib.h>

G_BEGIN_DECLS

typedef void (* CoglCallbackListFunc) (void *user_data);

typedef struct _CoglCallbackList
{
  GSList *funcs;
} CoglCallbackList;

void
_cogl_callback_list_init (CoglCallbackList *list);

void
_cogl_callback_list_add (CoglCallbackList *list,
                         CoglCallbackListFunc func,
                         void *user_data);
void
_cogl_callback_list_remove (CoglCallbackList *list,
                            CoglCallbackListFunc func,
                            void *user_data);

void
_cogl_callback_list_invoke (CoglCallbackList *list);

void
_cogl_callback_list_destroy (CoglCallbackList *list);

G_END_DECLS

#endif /* __COGL_CALLBACK_LIST_H__ */

