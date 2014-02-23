/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_OBJECT_PRIVATE_H
#define __COGL_OBJECT_PRIVATE_H

#include <glib.h>

#include "cogl-types.h"
#include "cogl-object.h"
#include "cogl-debug.h"

/* For compatability until all components have been converted */
typedef struct _CoglObjectClass CoglHandleClass;
typedef struct _CoglObject      CoglHandleObject;

/* XXX: sadly we didn't fully consider when we copied the cairo API
 * for _set_user_data that the callback doesn't get a pointer to the
 * instance which is desired in most cases. This means you tend to end
 * up creating micro allocations for the private data just so you can
 * pair up the data of interest with the original instance for
 * identification when it is later destroyed.
 *
 * Internally we use a small hack to avoid needing these micro
 * allocations by actually passing the instance as a second argument
 * to the callback */
typedef void (*CoglUserDataDestroyInternalCallback) (void *user_data,
                                                     void *instance);

typedef struct _CoglObjectClass
{
#ifdef COGL_HAS_GTYPE_SUPPORT
  GTypeClass base_class;
#endif
  const char *name;
  void *virt_free;
  void *virt_unref;
} CoglObjectClass;

#define COGL_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES 2

typedef struct
{
  CoglUserDataKey *key;
  void *user_data;
  CoglUserDataDestroyInternalCallback destroy;
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
  CoglObjectClass  *klass; /* equivalent to GTypeInstance */

  CoglUserDataEntry user_data_entry[
    COGL_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES];
  GArray           *user_data_array;
  int               n_user_data_entries;

  unsigned int      ref_count;
};

/* Helper macro to encapsulate the common code for COGL reference
   counted objects */

#ifdef COGL_OBJECT_DEBUG

#define _COGL_OBJECT_DEBUG_NEW(type_name, obj)                          \
  COGL_NOTE (OBJECT, "COGL " G_STRINGIFY (type_name) " NEW   %p %i",    \
             (obj), (obj)->ref_count)

#define _COGL_OBJECT_DEBUG_REF(type_name, object)       G_STMT_START {  \
  CoglObject *__obj = (CoglObject *)object;                             \
  COGL_NOTE (OBJECT, "COGL %s REF %p %i",                               \
             (__obj)->klass->name,                                      \
             (__obj), (__obj)->ref_count);              } G_STMT_END

#define _COGL_OBJECT_DEBUG_UNREF(type_name, object)     G_STMT_START {  \
  CoglObject *__obj = (CoglObject *)object;                             \
  COGL_NOTE (OBJECT, "COGL %s UNREF %p %i",                             \
             (__obj)->klass->name,                                      \
             (__obj), (__obj)->ref_count - 1);          } G_STMT_END

#define COGL_OBJECT_DEBUG_FREE(obj)                                     \
  COGL_NOTE (OBJECT, "COGL %s FREE %p",                                 \
             (obj)->klass->name, (obj))

#else /* !COGL_OBJECT_DEBUG */

#define _COGL_OBJECT_DEBUG_NEW(type_name, obj)
#define _COGL_OBJECT_DEBUG_REF(type_name, obj)
#define _COGL_OBJECT_DEBUG_UNREF(type_name, obj)
#define COGL_OBJECT_DEBUG_FREE(obj)

#endif /* COGL_OBJECT_DEBUG */

#ifdef COGL_HAS_GTYPE_SUPPORT
#define _COGL_GTYPE_INIT_CLASS(type_name) do {                                   \
  _cogl_##type_name##_class.base_class.g_type = cogl_##type_name##_get_gtype (); \
} while (0)
#else
#define _COGL_GTYPE_INIT_CLASS(type_name)
#endif

#define COGL_OBJECT_COMMON_DEFINE_WITH_CODE(TypeName, type_name, code)  \
                                                                        \
CoglObjectClass _cogl_##type_name##_class;                              \
static unsigned long _cogl_object_##type_name##_count;                  \
                                                                        \
static inline void                                                      \
_cogl_object_##type_name##_inc (void)                                   \
{                                                                       \
  _cogl_object_##type_name##_count++;                                   \
}                                                                       \
                                                                        \
static inline void                                                      \
_cogl_object_##type_name##_dec (void)                                   \
{                                                                       \
  _cogl_object_##type_name##_count--;                                   \
}                                                                       \
                                                                        \
static void                                                             \
_cogl_object_##type_name##_indirect_free (CoglObject *obj)              \
{                                                                       \
  _cogl_##type_name##_free ((Cogl##TypeName *) obj);                    \
  _cogl_object_##type_name##_dec ();                                    \
}                                                                       \
                                                                        \
static void                                                             \
_cogl_object_##type_name##_class_init (void)                            \
{                                                                       \
  _cogl_object_##type_name##_count = 0;                                 \
                                                                        \
    if (_cogl_debug_instances == NULL)                                  \
      _cogl_debug_instances =                                           \
        g_hash_table_new (g_str_hash, g_str_equal);                     \
                                                                        \
    _cogl_##type_name##_class.virt_free =                               \
      _cogl_object_##type_name##_indirect_free;                         \
    _cogl_##type_name##_class.virt_unref =                              \
      _cogl_object_default_unref;                                       \
    _cogl_##type_name##_class.name = "Cogl"#TypeName;                   \
                                                                        \
    g_hash_table_insert (_cogl_debug_instances,                         \
                         (void *) _cogl_##type_name##_class.name,       \
                         &_cogl_object_##type_name##_count);            \
                                                                        \
    { code; }                                                           \
}                                                                       \
                                                                        \
static Cogl##TypeName *                                                 \
_cogl_##type_name##_object_new (Cogl##TypeName *new_obj)                \
{                                                                       \
  CoglObject *obj = (CoglObject *)&new_obj->_parent;                    \
  obj->ref_count = 0;                                                   \
  cogl_object_ref (obj);                                                \
  obj->n_user_data_entries = 0;                                         \
  obj->user_data_array = NULL;                                          \
                                                                        \
  obj->klass = &_cogl_##type_name##_class;                              \
  if (!obj->klass->virt_free)                                           \
    {                                                                   \
      _cogl_object_##type_name##_class_init ();                         \
    }                                                                   \
                                                                        \
  _cogl_object_##type_name##_inc ();                                    \
  _COGL_OBJECT_DEBUG_NEW (TypeName, obj);                               \
  return new_obj;                                                       \
}

#define COGL_OBJECT_DEFINE_WITH_CODE_GTYPE(TypeName, type_name, code)   \
                                                                        \
COGL_OBJECT_COMMON_DEFINE_WITH_CODE(TypeName,                           \
                                    type_name,                          \
                                    do { code; } while (0);             \
                                    _COGL_GTYPE_INIT_CLASS (type_name)) \
                                                                        \
CoglBool                                                                \
cogl_is_##type_name (void *object)                                      \
{                                                                       \
  CoglObject *obj = object;                                             \
                                                                        \
  if (object == NULL)                                                   \
    return FALSE;                                                       \
                                                                        \
  return obj->klass == &_cogl_##type_name##_class;                      \
}

#define COGL_OBJECT_DEFINE_WITH_CODE(TypeName, type_name, code)         \
                                                                        \
COGL_OBJECT_COMMON_DEFINE_WITH_CODE(TypeName, type_name, code)          \
                                                                        \
CoglBool                                                                \
cogl_is_##type_name (void *object)                                      \
{                                                                       \
  CoglObject *obj = object;                                             \
                                                                        \
  if (object == NULL)                                                   \
    return FALSE;                                                       \
                                                                        \
  return obj->klass == &_cogl_##type_name##_class;                      \
}

#define COGL_OBJECT_INTERNAL_DEFINE_WITH_CODE(TypeName, type_name, code) \
                                                                        \
COGL_OBJECT_COMMON_DEFINE_WITH_CODE(TypeName, type_name, code)          \
                                                                        \
CoglBool                                                                \
_cogl_is_##type_name (void *object)                                     \
{                                                                       \
  CoglObject *obj = object;                                             \
                                                                        \
  if (object == NULL)                                                   \
    return FALSE;                                                       \
                                                                        \
  return obj->klass == &_cogl_##type_name##_class;                      \
}

#define COGL_OBJECT_DEFINE_DEPRECATED_REF_COUNTING(type_name)   \
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
  COGL_OBJECT_DEFINE_WITH_CODE_GTYPE (TypeName, type_name, (void) 0)

#define COGL_OBJECT_INTERNAL_DEFINE(TypeName, type_name)         \
  COGL_OBJECT_INTERNAL_DEFINE_WITH_CODE (TypeName, type_name, (void) 0)

/* For temporary compatability */
#define COGL_HANDLE_INTERNAL_DEFINE_WITH_CODE(TypeName, type_name, code) \
                                                                         \
COGL_OBJECT_INTERNAL_DEFINE_WITH_CODE (TypeName, type_name, code)        \
                                                                         \
static Cogl##TypeName *                                                  \
_cogl_##type_name##_handle_new (CoglHandle handle)                       \
{                                                                        \
  return _cogl_##type_name##_object_new (handle);                        \
}

#define COGL_HANDLE_DEFINE_WITH_CODE(TypeName, type_name, code)          \
                                                                         \
COGL_OBJECT_DEFINE_WITH_CODE (TypeName, type_name, code)                 \
                                                                         \
static Cogl##TypeName *                                                  \
_cogl_##type_name##_handle_new (CoglHandle handle)                       \
{                                                                        \
  return _cogl_##type_name##_object_new (handle);                        \
}

#define COGL_HANDLE_DEFINE(TypeName, type_name)                 \
  COGL_HANDLE_DEFINE_WITH_CODE (TypeName, type_name, (void) 0)

void
_cogl_object_set_user_data (CoglObject *object,
                            CoglUserDataKey *key,
                            void *user_data,
                            CoglUserDataDestroyInternalCallback destroy);

void
_cogl_object_default_unref (void *obj);

#endif /* __COGL_OBJECT_PRIVATE_H */

