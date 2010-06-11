/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_OBJECT_PRIVATE_H
#define __COGL_OBJECT_PRIVATE_H

#include "cogl-object.h"

/* For compatability until all components have been converted */
typedef struct _CoglObjectClass CoglHandleClass;
typedef struct _CoglObject      CoglHandleObject;

typedef struct _CoglObjectClass
{
  GQuark   type;
  void    *virt_free;
} CoglObjectClass;

#define COGL_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES 2

typedef struct
{
  CoglUserDataKey *key;
  void *user_data;
  CoglUserDataDestroyCallback destroy;
} CoglUserDataEntry;

/* All Cogl objects inherit from this base object by adding a member:
 *
 *   CoglObject _parent;
 *
 * at the top of its main structure. This structure is initialized
 * when you call _cogl_#type_name#_object_new (new_object);
 */
struct _CoglObject
{
  unsigned int      ref_count;

  CoglUserDataEntry user_data_entry[
    COGL_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES];
  GArray           *user_data_array;
  int               n_user_data_entries;

  CoglObjectClass  *klass;
};

/* Helper macro to encapsulate the common code for COGL reference
   counted objects */

#ifdef COGL_OBJECT_DEBUG

#define _COGL_OBJECT_DEBUG_NEW(type_name, obj)                          \
  COGL_NOTE (HANDLE, "COGL " G_STRINGIFY (type_name) " NEW   %p %i",    \
             (obj), (obj)->ref_count)

#define _COGL_OBJECT_DEBUG_REF(type_name, object)       G_STMT_START {  \
  CoglObject *__obj = (CoglObject *)object;                             \
  COGL_NOTE (HANDLE, "COGL %s REF %p %i",                               \
             g_quark_to_string ((__obj)->klass->type),                  \
             (__obj), (__obj)->ref_count);              } G_STMT_END

#define _COGL_OBJECT_DEBUG_UNREF(type_name, object)     G_STMT_START {  \
  CoglObject *__obj = (CoglObject *)object;                             \
  COGL_NOTE (HANDLE, "COGL %s UNREF %p %i",                             \
             g_quark_to_string ((__obj)->klass->type),                  \
             (__obj), (__obj)->ref_count - 1);          } G_STMT_END

#define COGL_OBJECT_DEBUG_FREE(obj)                                     \
  COGL_NOTE (HANDLE, "COGL %s FREE %p",                                 \
             g_quark_to_string ((obj)->klass->type), (obj))

#else /* !COGL_OBJECT_DEBUG */

#define _COGL_OBJECT_DEBUG_NEW(type_name, obj)
#define _COGL_OBJECT_DEBUG_REF(type_name, obj)
#define _COGL_OBJECT_DEBUG_UNREF(type_name, obj)
#define COGL_OBJECT_DEBUG_FREE(obj)

#endif /* COGL_OBJECT_DEBUG */

/* For temporary compatability */
#define _COGL_HANDLE_DEBUG_NEW _COGL_OBJECT_DEBUG_NEW
#define _COGL_HANDLE_DEBUG_REF _COGL_OBJECT_DEBUG_REF
#define _COGL_HANDLE_DEBUG_UNREF _COGL_OBJECT_DEBUG_UNREF
#define COGL_HANDLE_DEBUG_FREE COGL_OBJECT_DEBUG_FREE

#define COGL_OBJECT_DEFINE_WITH_CODE(TypeName, type_name, code) \
                                                                \
static CoglObjectClass _cogl_##type_name##_class;               \
                                                                \
GQuark                                                          \
_cogl_object_##type_name##_get_type (void)                      \
{                                                               \
  static GQuark type = 0;                                       \
  if (!type)                                                    \
    {                                                           \
      type = g_quark_from_static_string ("Cogl"#TypeName);      \
      { code; }                                                 \
    }                                                           \
  return type;                                                  \
}                                                               \
                                                                \
GQuark                                                          \
_cogl_handle_##type_name##_get_type (void)                      \
{                                                               \
  return _cogl_object_##type_name##_get_type ();                \
}                                                               \
                                                                \
static Cogl##TypeName *                                         \
_cogl_##type_name##_object_new (Cogl##TypeName *new_obj)        \
{                                                               \
  CoglObject *obj = (CoglObject *)&new_obj->_parent;            \
  obj->ref_count = 1;                                           \
  obj->n_user_data_entries = 0;                                 \
  obj->user_data_array = NULL;                                  \
                                                                \
  obj->klass = &_cogl_##type_name##_class;                      \
  if (!obj->klass->type)                                        \
    {                                                           \
      obj->klass->type = _cogl_object_##type_name##_get_type ();\
      obj->klass->virt_free = _cogl_##type_name##_free;         \
    }                                                           \
                                                                \
  _COGL_OBJECT_DEBUG_NEW (TypeName, obj);                       \
  return new_obj;                                               \
}                                                               \
                                                                \
Cogl##TypeName *                                                \
_cogl_##type_name##_pointer_from_handle (CoglHandle handle)     \
{                                                               \
  return handle;                                                \
}                                                               \
                                                                \
gboolean                                                        \
cogl_is_##type_name (CoglHandle object)                         \
{                                                               \
  CoglObject *obj = object;                                     \
                                                                \
  if (object == NULL)                                           \
    return FALSE;                                               \
                                                                \
  return (obj->klass->type ==                                   \
          _cogl_object_##type_name##_get_type ());              \
}                                                               \
                                                                \
void * G_GNUC_DEPRECATED                                        \
cogl_##type_name##_ref (void *object)                           \
{                                                               \
  if (!cogl_is_##type_name (object))                            \
    return NULL;                                                \
                                                                \
  _COGL_OBJECT_DEBUG_REF (TypeName, object);                    \
                                                                \
  cogl_handle_ref (object);                                     \
                                                                \
  return object;                                                \
}                                                               \
                                                                \
void G_GNUC_DEPRECATED                                          \
cogl_##type_name##_unref (void *object)                         \
{                                                               \
  if (!cogl_is_##type_name (object))                            \
    {                                                           \
      g_warning (G_STRINGIFY (cogl_##type_name##_unref)         \
                 ": Ignoring unref of Cogl handle "             \
                 "due to type mismatch");                       \
      return;                                                   \
    }                                                           \
                                                                \
  _COGL_OBJECT_DEBUG_UNREF (TypeName, object);                  \
                                                                \
  cogl_handle_unref (object);                                   \
}

#define COGL_OBJECT_DEFINE(TypeName, type_name)                 \
  COGL_OBJECT_DEFINE_WITH_CODE (TypeName, type_name, (void) 0)

/* For temporary compatability */
#define COGL_HANDLE_DEFINE_WITH_CODE(TypeName, type_name, code) \
                                                                \
COGL_OBJECT_DEFINE_WITH_CODE (TypeName, type_name, code)        \
                                                                \
static Cogl##TypeName *                                         \
_cogl_##type_name##_handle_new (CoglHandle handle)              \
{                                                               \
  return _cogl_##type_name##_object_new (handle);               \
}

#define COGL_HANDLE_DEFINE(TypeName, type_name)                 \
  COGL_HANDLE_DEFINE_WITH_CODE (TypeName, type_name, (void) 0)

#endif /* __COGL_OBJECT_PRIVATE_H */

