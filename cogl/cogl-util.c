/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "cogl.h"

#include "cogl-fixed.h"
#include "cogl-internal.h"
#include "cogl-material.h"
#include "cogl-offscreen.h"
#include "cogl-shader.h"
#include "cogl-texture.h"
#include "cogl-types.h"
#include "cogl-handle.h"
#include "cogl-util.h"

/**
 * cogl_util_next_p2:
 * @a: Value to get the next power
 *
 * Calculates the next power greater than @a.
 *
 * Return value: The next power after @a.
 */
int
cogl_util_next_p2 (int a)
{
  int rval = 1;

  while (rval < a)
    rval <<= 1;

  return rval;
}

/* gtypes */

CoglHandle
cogl_handle_ref (CoglHandle handle)
{
  CoglHandleObject *obj = (CoglHandleObject *)handle;

  g_return_val_if_fail (handle != COGL_INVALID_HANDLE, COGL_INVALID_HANDLE);

  obj->ref_count++;
  return handle;
}

void
cogl_handle_unref (CoglHandle handle)
{
  CoglHandleObject *obj = (CoglHandleObject *)handle;

  g_return_if_fail (handle != COGL_INVALID_HANDLE);

  if (--obj->ref_count < 1)
    {
      void (*free_func)(void *obj);

      COGL_HANDLE_DEBUG_FREE (obj);
      free_func = obj->klass->virt_free;
      free_func (obj);
    }
}

GType
cogl_handle_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type = g_boxed_type_register_static (g_intern_static_string ("CoglHandle"),
                                            (GBoxedCopyFunc) cogl_handle_ref,
                                            (GBoxedFreeFunc) cogl_handle_unref);

  return our_type;
}

/*
 * CoglFixed
 */

static GTypeInfo _info = {
  0,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  0,
  NULL,
  NULL,
};

static GTypeFundamentalInfo _finfo = { 0, };

static void
cogl_value_init_fixed (GValue *value)
{
  value->data[0].v_int = 0;
}

static void
cogl_value_copy_fixed (const GValue *src,
                       GValue       *dest)
{
  dest->data[0].v_int = src->data[0].v_int;
}

static char *
cogl_value_collect_fixed (GValue       *value,
                          unsigned int  n_collect_values,
                          GTypeCValue  *collect_values,
                          unsigned int  collect_flags)
{
  value->data[0].v_int = collect_values[0].v_int;

  return NULL;
}

static char *
cogl_value_lcopy_fixed (const GValue *value,
                        unsigned int  n_collect_values,
                        GTypeCValue  *collect_values,
                        unsigned int  collect_flags)
{
  gint32 *fixed_p = collect_values[0].v_pointer;

  if (!fixed_p)
    return g_strdup_printf ("value location for '%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  *fixed_p = value->data[0].v_int;

  return NULL;
}

static void
cogl_value_transform_fixed_int (const GValue *src,
                                GValue       *dest)
{
  dest->data[0].v_int = COGL_FIXED_TO_INT (src->data[0].v_int);
}

static void
cogl_value_transform_fixed_double (const GValue *src,
                                      GValue       *dest)
{
  dest->data[0].v_double = COGL_FIXED_TO_DOUBLE (src->data[0].v_int);
}

static void
cogl_value_transform_fixed_float (const GValue *src,
                                  GValue       *dest)
{
  dest->data[0].v_float = COGL_FIXED_TO_FLOAT (src->data[0].v_int);
}

static void
cogl_value_transform_int_fixed (const GValue *src,
                                GValue       *dest)
{
  dest->data[0].v_int = COGL_FIXED_FROM_INT (src->data[0].v_int);
}

static void
cogl_value_transform_double_fixed (const GValue *src,
                                   GValue       *dest)
{
  dest->data[0].v_int = COGL_FIXED_FROM_DOUBLE (src->data[0].v_double);
}

static void
cogl_value_transform_float_fixed (const GValue *src,
                                  GValue       *dest)
{
  dest->data[0].v_int = COGL_FIXED_FROM_FLOAT (src->data[0].v_float);
}


static const GTypeValueTable _cogl_fixed_value_table = {
  cogl_value_init_fixed,
  NULL,
  cogl_value_copy_fixed,
  NULL,
  "i",
  cogl_value_collect_fixed,
  "p",
  cogl_value_lcopy_fixed
};

GType
cogl_fixed_get_type (void)
{
  static GType _cogl_fixed_type = 0;

  if (G_UNLIKELY (_cogl_fixed_type == 0))
    {
      _info.value_table = & _cogl_fixed_value_table;
      _cogl_fixed_type =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     g_intern_static_string ("CoglFixed"),
                                     &_info, &_finfo, 0);

      g_value_register_transform_func (_cogl_fixed_type, G_TYPE_INT,
                                       cogl_value_transform_fixed_int);
      g_value_register_transform_func (G_TYPE_INT, _cogl_fixed_type,
                                       cogl_value_transform_int_fixed);

      g_value_register_transform_func (_cogl_fixed_type, G_TYPE_FLOAT,
                                       cogl_value_transform_fixed_float);
      g_value_register_transform_func (G_TYPE_FLOAT, _cogl_fixed_type,
                                       cogl_value_transform_float_fixed);

      g_value_register_transform_func (_cogl_fixed_type, G_TYPE_DOUBLE,
                                       cogl_value_transform_fixed_double);
      g_value_register_transform_func (G_TYPE_DOUBLE, _cogl_fixed_type,
                                       cogl_value_transform_double_fixed);
    }

  return _cogl_fixed_type;
}


