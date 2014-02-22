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

