/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
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
 */

#ifndef __COGL_HANDLE_H
#define __COGL_HANDLE_H

/* Helper macro to encapsulate the common code for COGL reference
   counted handles */

#if COGL_DEBUG

#define COGL_HANDLE_DEBUG_NEW(type_name, obj)			\
  printf ("COGL " G_STRINGIFY (type_name) " NEW   %p %i\n",     \
	  (obj), (obj)->ref_count)

#define COGL_HANDLE_DEBUG_REF(type_name, obj)			\
  printf ("COGL " G_STRINGIFY (type_name) " REF   %p %i\n",	\
	  (obj), (obj)->ref_count)

#define COGL_HANDLE_DEBUG_UNREF(type_name, obj)			\
  printf ("COGL " G_STRINGIFY (type_name) " UNREF %p %i\n",	\
	  (obj), (obj)->ref_count - 1)

#define COGL_HANDLE_DEBUG_FREE(type_name, obj)			\
  printf ("COGL " G_STRINGIFY (type_name) " FREE  %p\n", (obj))

#else /* COGL_DEBUG */

#define COGL_HANDLE_DEBUG_NEW(type_name, obj) (void) 0
#define COGL_HANDLE_DEBUG_REF(type_name, obj) (void) 0
#define COGL_HANDLE_DEBUG_UNREF(type_name, obj) (void) 0
#define COGL_HANDLE_DEBUG_FREE(type_name, obj) (void) 0

#endif /* COGL_DEBUG */

#define COGL_HANDLE_DEFINE(TypeName, type_name, handle_array)	\
								\
  static CoglHandle *					        \
  _cogl_##type_name##_handle_from_pointer (Cogl##TypeName *obj)	\
  {								\
    return (CoglHandle)obj;				        \
  }								\
								\
  static gint							\
  _cogl_##type_name##_handle_find (CoglHandle handle)		\
  {								\
    gint i;							\
								\
    _COGL_GET_CONTEXT (ctx, -1);				\
								\
    if (ctx->handle_array == NULL)				\
      return -1;						\
								\
    for (i=0; i < ctx->handle_array->len; ++i)			\
      if (g_array_index (ctx->handle_array,			\
			 CoglHandle, i) == handle)		\
	return i;						\
								\
    return -1;							\
  }								\
								\
  static CoglHandle						\
  _cogl_##type_name##_handle_new (Cogl##TypeName *obj)		\
  {								\
    CoglHandle handle = (CoglHandle) obj;			\
								\
    _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);		\
								\
    if (ctx->handle_array == NULL)				\
      ctx->handle_array						\
	= g_array_new (FALSE, FALSE, sizeof (CoglHandle));	\
    								\
    g_array_append_val (ctx->handle_array, handle);		\
    								\
    return handle;						\
  }								\
								\
  static void							\
  _cogl_##type_name##_handle_release (CoglHandle handle)	\
  {								\
    gint i;							\
								\
    _COGL_GET_CONTEXT (ctx, NO_RETVAL);				\
								\
    if ( (i = _cogl_##type_name##_handle_find (handle)) == -1)	\
      return;							\
								\
    g_array_remove_index_fast (ctx->handle_array, i);		\
  }								\
								\
  Cogl##TypeName *						\
  _cogl_##type_name##_pointer_from_handle (CoglHandle handle)	\
  {								\
    return (Cogl##TypeName *) handle;				\
  }								\
								\
  gboolean							\
  cogl_is_##type_name (CoglHandle handle)			\
  {								\
    if (handle == COGL_INVALID_HANDLE)				\
      return FALSE;						\
								\
    return _cogl_##type_name##_handle_find (handle) >= 0;	\
  }								\
								\
  CoglHandle							\
  cogl_##type_name##_ref (CoglHandle handle)			\
  {								\
    Cogl##TypeName *obj;					\
								\
    if (!cogl_is_##type_name (handle))				\
      return COGL_INVALID_HANDLE;				\
								\
    obj = _cogl_##type_name##_pointer_from_handle (handle);	\
								\
    obj->ref_count++;						\
								\
    COGL_HANDLE_DEBUG_REF (type_name, obj);			\
								\
    return handle;						\
  }								\
								\
  void								\
  cogl_##type_name##_unref (CoglHandle handle)			\
  {								\
    Cogl##TypeName *obj;					\
								\
    if (!cogl_is_##type_name (handle))				\
      return;							\
								\
    obj = _cogl_##type_name##_pointer_from_handle (handle);	\
								\
    COGL_HANDLE_DEBUG_UNREF (type_name, obj);			\
								\
    if (--obj->ref_count < 1)					\
      {								\
	COGL_HANDLE_DEBUG_FREE (type_name, obj);		\
								\
	_cogl_##type_name##_handle_release (obj);		\
	_cogl_##type_name##_free (obj);				\
      }								\
  }

#endif /* __COGL_HANDLE_H */
