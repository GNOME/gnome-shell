/* -*- mode:C; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Tomas Frydrych  <tf@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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

/**
 * SECTION:clutter-units
 * @short_description: A logical distance unit.
 *
 * Clutter units are logical units with granularity greater than that of the
 * device units; they are used by #ClutterActorBox and the _units() family of
 * ClutterActor functions. To convert between clutter units and device units,
 * use #CLUTTER_UNITS_FROM_DEVICE and #CLUTTER_UNITS_TO_DEVICE macros.
 *
 * Note: It is expected that as of version 0.6 all dimensions in the public
 * Clutter API will be given in clutter units. In order to ease the transition,
 * two extra macros have been provided, #CLUTTER_UNITS_TMP_TO_DEVICE and
 * #CLUTTER_UNITS_TMP_FROM_DEVICE. In version 0.4 these are identity macros,
 * but when the API transition happens will map to #CLUTTER_UNITS_TO_DEVICE and
 * #CLUTTER_UNITS_FROM_DEVICE respectively. You can use these in newly written
 * code as place holders.
 *
 * Since: 0.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-units.h"
#include "clutter-private.h"

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
clutter_value_init_unit (GValue *value)
{
  value->data[0].v_int = 0;
}

static void
clutter_value_copy_unit (const GValue *src,
                         GValue       *dest)
{
  dest->data[0].v_int = src->data[0].v_int;
}

static gchar *
clutter_value_collect_unit (GValue      *value,
                            guint        n_collect_values,
                            GTypeCValue *collect_values,
                            guint        collect_flags)
{
  value->data[0].v_int = collect_values[0].v_int;

  return NULL;
}

static gchar *
clutter_value_lcopy_unit (const GValue *value,
                          guint         n_collect_values,
                          GTypeCValue  *collect_values,
                          guint         collect_flags)
{
  gint32 *units_p = collect_values[0].v_pointer;

  if (!units_p)
    return g_strdup_printf ("value location for `%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  *units_p = value->data[0].v_int;

  return NULL;
}

static void
clutter_value_transform_unit_int (const GValue *src,
                                  GValue       *dest)
{
  dest->data[0].v_int = src->data[0].v_int;
}

static const GTypeValueTable _clutter_unit_value_table = {
  clutter_value_init_unit,
  NULL,
  clutter_value_copy_unit,
  NULL,
  "i",
  clutter_value_collect_unit,
  "p",
  clutter_value_lcopy_unit
};

GType
clutter_unit_get_type (void)
{
  static GType _clutter_unit_type = 0;

  if (G_UNLIKELY (_clutter_unit_type == 0))
    {
      _info.value_table = & _clutter_unit_value_table;
      _clutter_unit_type =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     I_("ClutterUnit"),
                                     &_info, &_finfo, 0);

      g_value_register_transform_func (_clutter_unit_type, G_TYPE_INT,
                                       clutter_value_transform_unit_int);
    }

  return _clutter_unit_type;
}

/**
 * clutter_value_set_unit:
 * @value: a #GValue initialized to #CLUTTER_TYPE_UNIT
 * @units: the units to set
 *
 * Sets @value to @units
 *
 * Since: 0.8
 */
void
clutter_value_set_unit (GValue      *value,
                        ClutterUnit  units)
{
  g_return_if_fail (CLUTTER_VALUE_HOLDS_UNIT (value));

  value->data[0].v_int = units;
}

/**
 * clutter_value_get_unit:
 * @value: a #GValue initialized to #CLUTTER_TYPE_UNIT
 *
 * Gets the #ClutterUnit<!-- -->s contained in @value.
 *
 * Return value: the units inside the passed #GValue
 *
 * Since: 0.8
 */
ClutterUnit
clutter_value_get_unit (const GValue *value)
{
  g_return_val_if_fail (CLUTTER_VALUE_HOLDS_UNIT (value), 0);

  return value->data[0].v_int;
}

static void
param_unit_init (GParamSpec *pspec)
{
  ClutterParamSpecUnit *uspec = CLUTTER_PARAM_SPEC_UNIT (pspec);

  uspec->minimum = CLUTTER_MINUNIT;
  uspec->maximum = CLUTTER_MAXUNIT;
  uspec->default_value = 0;
}

static void
param_unit_set_default (GParamSpec *pspec,
                        GValue     *value)
{
  value->data[0].v_int = CLUTTER_PARAM_SPEC_UNIT (pspec)->default_value;
}

static gboolean
param_unit_validate (GParamSpec *pspec,
                     GValue     *value)
{
  ClutterParamSpecUnit *uspec = CLUTTER_PARAM_SPEC_UNIT (pspec);
  gint oval = value->data[0].v_int;

  value->data[0].v_int = CLAMP (value->data[0].v_int,
                                uspec->minimum,
                                uspec->maximum);

  return value->data[0].v_int != oval;
}

static gint
param_unit_values_cmp (GParamSpec   *pspec,
                       const GValue *value1,
                       const GValue *value2)
{
  if (value1->data[0].v_int < value2->data[0].v_int)
    return -1;
  else
    return value1->data[0].v_int > value2->data[0].v_int;
}

GType
clutter_param_unit_get_type (void)
{
  static GType pspec_type = 0;

  if (G_UNLIKELY (pspec_type == 0))
    {
      const GParamSpecTypeInfo pspec_info = {
        sizeof (ClutterParamSpecUnit),
        16,
        param_unit_init,
        CLUTTER_TYPE_UNIT,
        NULL,
        param_unit_set_default,
        param_unit_validate,
        param_unit_values_cmp,
      };

      pspec_type = g_param_type_register_static (I_("ClutterParamSpecUnit"),
                                                 &pspec_info);
    }

  return pspec_type;
}

/**
 * clutter_param_spec_unit:
 * @name: name of the property
 * @nick: short name
 * @blurb: description (can be translatable)
 * @minimum: lower boundary
 * @maximum: higher boundary
 * @default_value: default value
 * @flags: flags for the param spec
 *
 * Creates a #GParamSpec for properties using #ClutterUnit<!-- -->s.
 *
 * Return value: the newly created #GParamSpec
 *
 * Since: 0.8
 */
GParamSpec *
clutter_param_spec_unit (const gchar *name,
                         const gchar *nick,
                         const gchar *blurb,
                         ClutterUnit  minimum,
                         ClutterUnit  maximum,
                         ClutterUnit  default_value,
                         GParamFlags  flags)
{
  ClutterParamSpecUnit *uspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum,
                        NULL);

  uspec = g_param_spec_internal (CLUTTER_TYPE_PARAM_UNIT,
                                 name, nick, blurb,
                                 flags);
  uspec->minimum = minimum;
  uspec->maximum = maximum;
  uspec->default_value = default_value;

  return G_PARAM_SPEC (uspec);
}
