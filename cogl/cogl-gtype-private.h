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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 */

#include <glib.h>
#include <glib-object.h>

#ifndef __COGL_GTYPE_PRIVATE_H__
#define __COGL_GTYPE_PRIVATE_H__

#define COGL_GTYPE_DEFINE_BOXED(Name, underscore_name, copy_func, free_func) \
GType \
cogl_gtype_ ## underscore_name ## _get_type (void) \
{ \
   static volatile size_t type_volatile = 0; \
   if (g_once_init_enter (&type_volatile)) \
     { \
       GType type = \
         g_boxed_type_register_static (g_intern_static_string ("Cogl" Name), \
                                       (GBoxedCopyFunc)copy_func, \
                                       (GBoxedFreeFunc)free_func); \
       g_once_init_leave (&type_volatile, type); \
     } \
   return type_volatile; \
}

#endif /* __COGL_GTYPE_PRIVATE_H__ */

