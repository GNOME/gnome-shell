/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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

#ifndef __COGL_GTYPE_PRIVATE_H__
#define __COGL_GTYPE_PRIVATE_H__

#include "config.h"

#include <glib.h>
#include <glib-object.h>

#include "cogl-object-private.h"

/* Move this to public headers? */
typedef struct _CoglGtypeObject CoglGtypeObject;
typedef struct _CoglGtypeClass  CoglGtypeClass;

struct _CoglGtypeObject
{
  GTypeInstance parent_instance;

  guint dummy;
};

struct _CoglGtypeClass
{
  GTypeClass base_class;

  guint dummy;
};

#define I_(str)  (g_intern_static_string ((str)))

/**/

#define COGL_GTYPE_DEFINE_BOXED(Name,underscore_name,copy_func,free_func) \
GType \
cogl_##underscore_name##_get_gtype (void) \
{ \
   static volatile size_t type_volatile = 0; \
   if (g_once_init_enter (&type_volatile)) \
     { \
       GType type = \
         g_boxed_type_register_static (g_intern_static_string (I_("Cogl" # Name)), \
                                       (GBoxedCopyFunc)copy_func, \
                                       (GBoxedFreeFunc)free_func); \
       g_once_init_leave (&type_volatile, type); \
     } \
   return type_volatile; \
}

#define COGL_GTYPE_IMPLEMENT_INTERFACE(name) {                          \
    const GInterfaceInfo g_implement_interface_info = {                 \
      (GInterfaceInitFunc) _cogl_gtype_dummy_iface_init, NULL, NULL     \
    };                                                                  \
    g_type_add_interface_static (fundamental_type_id,                   \
                                 cogl_##name##_get_gtype(),         \
                                 &g_implement_interface_info);          \
  }

#define _COGL_GTYPE_DEFINE_BASE_CLASS_BEGIN(Name,name)                  \
GType                                                                   \
cogl_##name##_get_gtype (void)                                      \
{                                                                       \
  static volatile gsize type_id__volatile = 0;                          \
  if (g_once_init_enter (&type_id__volatile))                           \
    {                                                                   \
      static const GTypeFundamentalInfo finfo = {                       \
        (G_TYPE_FLAG_CLASSED |                                          \
         G_TYPE_FLAG_INSTANTIATABLE |                                   \
         G_TYPE_FLAG_DERIVABLE |                                        \
         G_TYPE_FLAG_DEEP_DERIVABLE),                                   \
      };                                                                \
      static const GTypeValueTable value_table = {                      \
        _cogl_gtype_object_init_value,                                  \
        _cogl_gtype_object_free_value,                                  \
        _cogl_gtype_object_copy_value,                                  \
        _cogl_gtype_object_peek_pointer,                                \
        "p",                                                            \
        _cogl_gtype_object_collect_value,                               \
        "p",                                                            \
        _cogl_gtype_object_lcopy_value,                                 \
      };                                                                \
      const GTypeInfo node_info = {                                     \
        sizeof (CoglObjectClass),                                       \
        (GBaseInitFunc) _cogl_gtype_object_class_base_init,             \
        (GBaseFinalizeFunc) _cogl_gtype_object_class_base_finalize,     \
        (GClassInitFunc) _cogl_gtype_object_class_init,                 \
        (GClassFinalizeFunc) NULL,                                      \
        NULL,                                                           \
        sizeof (CoglObject),                                            \
        0,                                                              \
        (GInstanceInitFunc) _cogl_gtype_object_init,                    \
        &value_table,                                                   \
      };                                                                \
      GType fundamental_type_id =                                       \
        g_type_register_fundamental (g_type_fundamental_next (),        \
                                     I_("Cogl" # Name),                 \
                                     &node_info, &finfo,                \
                                     G_TYPE_FLAG_ABSTRACT);             \
      g_once_init_leave (&type_id__volatile,                            \
                         fundamental_type_id);

#define _COGL_GTYPE_DEFINE_BASE_CLASS_END()                             \
    }                                                                   \
    return type_id__volatile;                                           \
  }

#define COGL_GTYPE_DEFINE_BASE_CLASS(Name,name,interfaces...)      \
  _COGL_GTYPE_DEFINE_BASE_CLASS_BEGIN(Name,name)                   \
  {interfaces;}                                                    \
  _COGL_GTYPE_DEFINE_BASE_CLASS_END()

#define _COGL_GTYPE_DEFINE_INTERFACE_EXTENDED_BEGIN(Name,name)          \
                                                                        \
  static void name##_default_init (Name##Interface *klass);             \
  GType                                                                 \
  name##_get_gtype (void)                                               \
  {                                                                     \
    static volatile gsize type_id__volatile = 0;                        \
    if (g_once_init_enter (&type_id__volatile))                         \
      {                                                                 \
        GType fundamental_type_id =                                     \
          g_type_register_static_simple (G_TYPE_INTERFACE,              \
                                         g_intern_static_string (#Name), \
                                         sizeof (Name##Interface),    \
                                         (GClassInitFunc)name##_default_init, \
                                         0,                             \
                                         (GInstanceInitFunc)NULL,       \
                                         (GTypeFlags) 0);               \
        g_type_interface_add_prerequisite (fundamental_type_id,         \
                                           cogl_object_get_gtype());    \
        { /* custom code follows */

#define _COGL_GTYPE_DEFINE_INTERFACE_EXTENDED_END()                     \
  /* following custom code */                                           \
  }                                                                     \
    g_once_init_leave (&type_id__volatile,                              \
                       fundamental_type_id);                            \
    }                                                                   \
    return type_id__volatile;                                           \
    } /* closes name##_get_type() */


#define COGL_GTYPE_DEFINE_INTERFACE(Name,name)                          \
  typedef struct _Cogl##Name##Iface Cogl##Name##Iface;                  \
  typedef Cogl##Name##Iface  Cogl##Name##Interface;                     \
  struct _Cogl##Name##Iface                                             \
  {                                                                     \
    /*< private >*/                                                     \
    GTypeInterface g_iface;                                             \
  };                                                                    \
  _COGL_GTYPE_DEFINE_INTERFACE_EXTENDED_BEGIN (Cogl##Name, cogl_##name) \
  _COGL_GTYPE_DEFINE_INTERFACE_EXTENDED_END ()                          \
  static void                                                           \
  cogl_##name##_default_init (Cogl##Name##Interface *iface)             \
  {                                                                     \
  }

#define _COGL_GTYPE_DEFINE_TYPE_EXTENDED_BEGIN(Name,name,parent,flags)  \
                                                                        \
  static void     name##_init              (Name        *self);         \
  static void     name##_class_init        (Name##Class *klass);        \
  static gpointer name##_parent_class = NULL;                           \
  static gint     Name##_private_offset;                                \
                                                                        \
  static void                                                           \
  name##_class_intern_init (gpointer klass)                             \
  {                                                                     \
    name##_parent_class = g_type_class_peek_parent (klass);             \
    name##_class_init ((Name##Class*) klass);                           \
  }                                                                     \
                                                                        \
  static inline gpointer                                                \
  name##_get_instance_private (Name *self)                              \
    {                                                                   \
      return (G_STRUCT_MEMBER_P (self, Name ##_private_offset));        \
    }                                                                   \
                                                                        \
  GType                                                                 \
  name##_get_gtype (void)                                               \
  {                                                                     \
    static volatile gsize type_id__volatile = 0;                        \
    if (g_once_init_enter (&type_id__volatile))                         \
      {                                                                 \
        GType fundamental_type_id =                                     \
          g_type_register_static_simple (parent,                        \
                                         g_intern_static_string (#Name), \
                                         sizeof (Name##Class),          \
                                         (GClassInitFunc) name##_class_intern_init, \
                                         sizeof (Name),                 \
                                         (GInstanceInitFunc) name##_init, \
                                         (GTypeFlags) flags);           \
        { /* custom code follows */

#define _COGL_GTYPE_DEFINE_TYPE_EXTENDED_END()                          \
  /* following custom code */                                           \
  }                                                                     \
    g_once_init_leave (&type_id__volatile,                              \
                       fundamental_type_id);                            \
    }                                                                   \
    return type_id__volatile;                                           \
    } /* closes name##_get_type() */


#define COGL_GTYPE_DEFINE_CLASS(Name,name,interfaces...)                \
  typedef struct _Cogl##Name##Class Cogl##Name##Class;                  \
  struct _Cogl##Name##Class {                                           \
    CoglObjectClass parent_class;                                       \
  };                                                                    \
  _COGL_GTYPE_DEFINE_TYPE_EXTENDED_BEGIN(Cogl##Name,                    \
                                         cogl_##name,                   \
                                         cogl_object_get_gtype(),       \
                                         0)                             \
  {interfaces;}                                                         \
  _COGL_GTYPE_DEFINE_TYPE_EXTENDED_END()                                \
  static void                                                           \
  cogl_##name##_init (Cogl##Name *instance)                             \
  {                                                                     \
  }                                                                     \
  static void                                                           \
  cogl_##name##_class_init (Cogl##Name##Class *klass)                   \
  {                                                                     \
  }

void _cogl_gtype_object_init_value (GValue *value);
void _cogl_gtype_object_free_value (GValue *value);
void _cogl_gtype_object_copy_value (const GValue *src,
                                    GValue       *dst);
gpointer _cogl_gtype_object_peek_pointer (const GValue *value);
gchar *_cogl_gtype_object_collect_value (GValue      *value,
                                         guint        n_collect_values,
                                         GTypeCValue *collect_values,
                                         guint        collect_flags);
gchar *_cogl_gtype_object_lcopy_value (const GValue *value,
                                       guint         n_collect_values,
                                       GTypeCValue  *collect_values,
                                       guint         collect_flags);

void _cogl_gtype_object_class_base_init (CoglObjectClass *klass);
void _cogl_gtype_object_class_base_finalize (CoglObjectClass *klass);
void _cogl_gtype_object_class_init (CoglObjectClass *klass);
void _cogl_gtype_object_init (CoglObject *object);

void cogl_object_value_set_object (GValue   *value,
                                   gpointer  object);
gpointer cogl_object_value_get_object (const GValue *value);

void _cogl_gtype_dummy_iface_init (gpointer iface);

#endif /* __COGL_GTYPE_PRIVATE_H__ */
