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

typedef struct _CoglHandleClass
{
  GQuark   type;
  void    *virt_free;
} CoglHandleClass;

/* All Cogl objects inherit from this base object by adding a member:
 *
 *   CoglHandleObject _parent;
 *
 * at the top of its main structure. This structure is initialized
 * when you call _cogl_#type_name#_handle_new (new_object);
 */
typedef struct _CoglHandleObject
{
  guint            ref_count;
  CoglHandleClass *klass;
} CoglHandleObject;

/* Helper macro to encapsulate the common code for COGL reference
   counted handles */

#if COGL_DEBUG

#define _COGL_HANDLE_DEBUG_NEW(type_name, obj)			\
  printf ("COGL " G_STRINGIFY (type_name) " NEW   %p %i\n",     \
	  (obj), (obj)->ref_count)

#define _COGL_HANDLE_DEBUG_REF(type_name, handle)		\
  do {                                                          \
    CoglHandleObject *obj = (CoglHandleObject *)handle;         \
    printf ("COGL %s REF %p %i\n",	                        \
	    g_quark_to_string ((obj)->klass->type),             \
            (obj), (obj)->ref_count);                           \
  } while (0)

#define _COGL_HANDLE_DEBUG_UNREF(type_name, handle)		\
  do {                                                          \
    CoglHandleObject *obj = (CoglHandleObject *)handle;         \
    printf ("COGL %s UNREF %p %i\n",	                        \
	    g_quark_to_string ((obj)->klass->type),             \
	    (obj), (obj)->ref_count - 1);                       \
  } while (0)

#define COGL_HANDLE_DEBUG_FREE(obj)			        \
  printf ("COGL %s FREE %p\n",                                  \
          g_quark_to_string ((obj)->klass->type), (obj))        \

#else /* COGL_DEBUG */

#define _COGL_HANDLE_DEBUG_NEW(type_name, obj)
#define _COGL_HANDLE_DEBUG_REF(type_name, obj)
#define _COGL_HANDLE_DEBUG_UNREF(type_name, obj)
#define COGL_HANDLE_DEBUG_FREE(obj)

#endif /* COGL_DEBUG */

#define COGL_HANDLE_DEFINE(TypeName, type_name)	                \
								\
  static CoglHandleClass _cogl_##type_name##_class;             \
								\
  static GQuark                                                 \
  _cogl_##type_name##_get_type (void)                           \
  {                                                             \
    static GQuark type = 0;                                     \
    if (!type)                                                  \
      type = g_quark_from_static_string ("Cogl"#TypeName);      \
    return type;                                                \
  }                                                             \
								\
  static CoglHandle						\
  _cogl_##type_name##_handle_new (Cogl##TypeName *new_obj)	\
  {				                                \
    CoglHandleObject *obj = &new_obj->_parent;                  \
    obj->ref_count = 1;                                         \
								\
    obj->klass = &_cogl_##type_name##_class;                    \
    if (!obj->klass->type)                                      \
      {                                                         \
        obj->klass->type =                                      \
          _cogl_##type_name##_get_type ();                      \
        obj->klass->virt_free = _cogl_##type_name##_free;       \
      }                                                         \
								\
    _COGL_HANDLE_DEBUG_NEW (TypeName, obj);                     \
    return (CoglHandle) new_obj;			        \
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
  {                                                             \
    CoglHandleObject *obj = (CoglHandleObject *)handle;         \
                                                                \
    if (handle == COGL_INVALID_HANDLE)                          \
      return FALSE;                                             \
                                                                \
    return (obj->klass->type ==                                 \
            _cogl_##type_name##_get_type ());                   \
  }								\
								\
  CoglHandle G_GNUC_DEPRECATED					\
  cogl_##type_name##_ref (CoglHandle handle)			\
  {								\
    if (!cogl_is_##type_name (handle))				\
      return COGL_INVALID_HANDLE;				\
                                                                \
    _COGL_HANDLE_DEBUG_REF (TypeName, handle);			\
								\
    cogl_handle_ref (handle);                                   \
								\
    return handle;						\
  }								\
								\
  void G_GNUC_DEPRECATED					\
  cogl_##type_name##_unref (CoglHandle handle)			\
  {								\
    if (!cogl_is_##type_name (handle))				\
      {                                                         \
        g_warning (G_STRINGIFY (cogl_##type_name##_unref)       \
                   ": Ignoring unref of Cogl handle "           \
                   "due to type missmatch");                    \
        return;							\
      }                                                         \
								\
    _COGL_HANDLE_DEBUG_UNREF (TypeName, handle);		\
                                                                \
    cogl_handle_unref (handle);                                 \
  }

#endif /* __COGL_HANDLE_H */
