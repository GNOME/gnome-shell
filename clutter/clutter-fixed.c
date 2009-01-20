/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Tomas Frydrych  <tf@openedhand.com>
 *
 * Copyright (C) 2006, 2007 OpenedHand
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

#define G_IMPLEMENT_INLINES

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-fixed.h"
#include "clutter-private.h"

/**
 * SECTION:clutter-fixed
 * @short_description: Fixed Point API
 *
 * Clutter has a fixed point API targeted at platforms without a
 * floating point unit, such as embedded devices. On such platforms
 * this API should be preferred to the floating point one as it does
 * not trigger the slow path of software emulation, relying on integer
 * math for fixed-to-floating and floating-to-fixed conversion.
 *
 * It is no recommened for use on platforms with a floating point unit
 * (eg desktop systems) nor for use in bindings.
 *
 * Basic rules of Fixed Point arithmethic:
 *
 * <itemizedlist>
 *   <listitem>
 *     <para>Two fixed point numbers can be directly added,
 *     subtracted and have their modulus taken.</para>
 *   </listitem>
 *   <listitem>
 *     <para>To add other numerical type to a fixed point number it has to
 *     be first converted to fixed point.</para>
 *   </listitem>
 *   <listitem>
 *     <para>A fixed point number can be directly multiplied or divided by
 *     an integer.</para>
 *   </listitem>
 *   <listitem>
 *     <para>Two fixed point numbers can only be multiplied and divided by the
 *     provided %CLUTTER_FIXED_MUL and %CLUTTER_FIXED_DIV macros.</para>
 *   </listitem>
 * </itemizedlist>
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
clutter_value_init_fixed (GValue *value)
{
  value->data[0].v_int = 0;
}

static void
clutter_value_copy_fixed (const GValue *src,
                          GValue       *dest)
{
  dest->data[0].v_int = src->data[0].v_int;
}

static gchar *
clutter_value_collect_fixed (GValue      *value,
                             guint        n_collect_values,
                             GTypeCValue *collect_values,
                             guint        collect_flags)
{
  value->data[0].v_int = collect_values[0].v_int;

  return NULL;
}

static gchar *
clutter_value_lcopy_fixed (const GValue *value,
                           guint         n_collect_values,
                           GTypeCValue  *collect_values,
                           guint         collect_flags)
{
  gint32 *fixed_p = collect_values[0].v_pointer;

  if (!fixed_p)
    return g_strdup_printf ("value location for `%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  *fixed_p = value->data[0].v_int;

  return NULL;
}

static void
clutter_value_transform_fixed_int (const GValue *src,
                                   GValue       *dest)
{
  dest->data[0].v_int =  (src->data[0].v_int);
}

static void
clutter_value_transform_fixed_double (const GValue *src,
                                      GValue       *dest)
{
  dest->data[0].v_double = CLUTTER_FIXED_TO_DOUBLE (src->data[0].v_int);
}

static void
clutter_value_transform_fixed_float (const GValue *src,
                                     GValue       *dest)
{
  dest->data[0].v_float = CLUTTER_FIXED_TO_FLOAT (src->data[0].v_int);
}

static void
clutter_value_transform_int_fixed (const GValue *src,
                                   GValue       *dest)
{
  dest->data[0].v_int = (float)(src->data[0].v_int);
}

static void
clutter_value_transform_double_fixed (const GValue *src,
                                      GValue       *dest)
{
  dest->data[0].v_int = CLUTTER_FLOAT_TO_FIXED (src->data[0].v_double);
}

static void
clutter_value_transform_float_fixed (const GValue *src,
                                     GValue       *dest)
{
  dest->data[0].v_int = CLUTTER_FLOAT_TO_FIXED (src->data[0].v_float);
}


static const GTypeValueTable _clutter_fixed_value_table = {
  clutter_value_init_fixed,
  NULL,
  clutter_value_copy_fixed,
  NULL,
  "i",
  clutter_value_collect_fixed,
  "p",
  clutter_value_lcopy_fixed
};

GType
clutter_fixed_get_type (void)
{
  static GType _clutter_fixed_type = 0;

  if (G_UNLIKELY (_clutter_fixed_type == 0))
    {
      _info.value_table = & _clutter_fixed_value_table;
      _clutter_fixed_type =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     I_("ClutterFixed"),
                                     &_info, &_finfo, 0);

      g_value_register_transform_func (_clutter_fixed_type, G_TYPE_INT,
                                       clutter_value_transform_fixed_int);
      g_value_register_transform_func (G_TYPE_INT, _clutter_fixed_type,
                                       clutter_value_transform_int_fixed);
      g_value_register_transform_func (_clutter_fixed_type, G_TYPE_FLOAT,
                                       clutter_value_transform_fixed_float);
      g_value_register_transform_func (G_TYPE_FLOAT, _clutter_fixed_type,
                                       clutter_value_transform_float_fixed);
      g_value_register_transform_func (_clutter_fixed_type, G_TYPE_DOUBLE,
                                       clutter_value_transform_fixed_double);
      g_value_register_transform_func (G_TYPE_DOUBLE, _clutter_fixed_type,
                                       clutter_value_transform_double_fixed);
    }

  return _clutter_fixed_type;
}

/**
 * clutter_value_set_fixed:
 * @value: a #GValue initialized to #CLUTTER_TYPE_FIXED
 * @fixed_: the fixed point value to set
 *
 * Sets @value to @fixed_.
 *
 * Since: 0.8
 */
void
clutter_value_set_fixed (GValue       *value,
                         ClutterFixed  fixed_)
{
  g_return_if_fail (CLUTTER_VALUE_HOLDS_FIXED (value));

  value->data[0].v_int = fixed_;
}

/**
 * clutter_value_get_fixed:
 * @value: a #GValue initialized to #CLUTTER_TYPE_FIXED
 *
 * Gets the fixed point value stored inside @value.
 *
 * Return value: the value inside the passed #GValue
 *
 * Since: 0.8
 */
ClutterFixed
clutter_value_get_fixed (const GValue *value)
{
  g_return_val_if_fail (CLUTTER_VALUE_HOLDS_FIXED (value), 0);

  return value->data[0].v_int;
}

static void
param_fixed_init (GParamSpec *pspec)
{
  ClutterParamSpecFixed *fspec = CLUTTER_PARAM_SPEC_FIXED (pspec);

  fspec->minimum = COGL_FIXED_MIN;
  fspec->maximum = COGL_FIXED_MAX;
  fspec->default_value = 0;
}

static void
param_fixed_set_default (GParamSpec *pspec,
                         GValue     *value)
{
  value->data[0].v_int = CLUTTER_PARAM_SPEC_FIXED (pspec)->default_value;
}

static gboolean
param_fixed_validate (GParamSpec *pspec,
                      GValue     *value)
{
  ClutterParamSpecFixed *fspec = CLUTTER_PARAM_SPEC_FIXED (pspec);
  gint oval =  (value->data[0].v_int);
  gint min, max, val;

  g_assert (CLUTTER_IS_PARAM_SPEC_FIXED (pspec));

  /* we compare the integer part of the value because the minimum
   * and maximum values cover just that part of the representation
   */

  min = fspec->minimum;
  max = fspec->maximum;
  val =  (value->data[0].v_int);

  val = CLAMP (val, min, max);
  if (val != oval)
    {
      value->data[0].v_int = val;
      return TRUE;
    }

  return FALSE;
}

static gint
param_fixed_values_cmp (GParamSpec   *pspec,
                        const GValue *value1,
                        const GValue *value2)
{
  if (value1->data[0].v_int < value2->data[0].v_int)
    return -1;
  else
    return value1->data[0].v_int > value2->data[0].v_int;
}

GType
clutter_param_fixed_get_type (void)
{
  static GType pspec_type = 0;

  if (G_UNLIKELY (pspec_type == 0))
    {
      const GParamSpecTypeInfo pspec_info = {
        sizeof (ClutterParamSpecFixed),
        16,
        param_fixed_init,
        CLUTTER_TYPE_FIXED,
        NULL,
        param_fixed_set_default,
        param_fixed_validate,
        param_fixed_values_cmp,
      };

      pspec_type = g_param_type_register_static (I_("ClutterParamSpecFixed"),
                                                 &pspec_info);
    }

  return pspec_type;
}

/**
 * clutter_param_spec_fixed:
 * @name: name of the property
 * @nick: short name
 * @blurb: description (can be translatable)
 * @minimum: lower boundary
 * @maximum: higher boundary
 * @default_value: default value
 * @flags: flags for the param spec
 *
 * Creates a #GParamSpec for properties using #ClutterFixed values
 *
 * Return value: the newly created #GParamSpec
 *
 * Since: 0.8
 */
GParamSpec *
clutter_param_spec_fixed (const gchar  *name,
                          const gchar  *nick,
                          const gchar  *blurb,
                          ClutterFixed  minimum,
                          ClutterFixed  maximum,
                          ClutterFixed  default_value,
                          GParamFlags  flags)
{
  ClutterParamSpecFixed *fspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum,
                        NULL);

  fspec = g_param_spec_internal (CLUTTER_TYPE_PARAM_FIXED,
                                 name, nick, blurb,
                                 flags);
  fspec->minimum = minimum;
  fspec->maximum = maximum;
  fspec->default_value = default_value;

  return G_PARAM_SPEC (fspec);
}
