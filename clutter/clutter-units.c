/* -*- mode:C; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By: Tomas Frydrych  <tf@openedhand.com>
 *              Emmanuele Bassi  <ebassi@openedhand.com>
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
 * device units; they are used by #ClutterActorBox and the units-based family
 * of #ClutterActor functions. To convert between Clutter units and device
 * units, use %CLUTTER_UNITS_FROM_DEVICE and %CLUTTER_UNITS_TO_DEVICE macros.
 *
 * #ClutterUnit<!-- -->s can be converted from other units like millimeters,
 * typographic points (at the current resolution) and percentages. It is
 * also possible to convert fixed point values to and from #ClutterUnit
 * values.
 *
 * In order to register a #ClutterUnit property, the #ClutterParamSpecUnit
 * #GParamSpec sub-class should be used:
 *
 * |[
 *   GParamSpec *pspec;
 *
 *   pspec = clutter_param_spec_unit ("width",
 *                                    "Width",
 *                                    "Width of the actor, in units",
 *                                    0, CLUTTER_MAXUNIT,
 *                                    0,
 *                                    G_PARAM_READWRITE);
 *   g_object_class_install_property (gobject_class, PROP_WIDTH, pspec);
 * ]|
 *
 * A #GValue holding units can be manipulated using clutter_value_set_unit()
 * and clutter_value_get_unit(). #GValue<!-- -->s containing a #ClutterUnit
 * value can also be transformed to #GValue<!-- -->s containing integer
 * values - with a loss of precision:
 *
 * |[
 *   static gboolean
 *   units_to_int (const GValue *src,
 *                 GValue       *dest)
 *   {
 *     g_return_val_if_fail (CLUTTER_VALUE_HOLDS_UNIT (src), FALSE);
 *
 *     g_value_init (dest, G_TYPE_INT);
 *     return g_value_transform (src, &dest);
 *   }
 * ]|
 *
 * The code above is equivalent to:
 *
 * |[
 *   static gboolean
 *   units_to_int (const GValue *src,
 *                 GValue       *dest)
 *   {
 *     g_return_val_if_fail (CLUTTER_VALUE_HOLDS_UNIT (src), FALSE);
 *
 *     g_value_init (dest, G_TYPE_INT);
 *     g_value_set_int (dest,
 *                      CLUTTER_UNITS_TO_INT (clutter_value_get_unit (src)));
 *
 *     return TRUE;
 *   }
 * ]|
 *
 * #ClutterUnit is available since Clutter 0.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-units.h"
#include "clutter-private.h"

#define DPI_FALLBACK    (96.0)

#define FLOAT_EPSILON   (1e-30)

/**
 * clutter_units_mm:
 * @mm: millimeters to convert
 *
 * Converts a value in millimeters to #ClutterUnit<!-- -->s at
 * the current DPI.
 *
 * Return value: the value in units
 *
 * Since: 1.0
 */
ClutterUnit
clutter_units_mm (gdouble mm)
{
  ClutterBackend *backend;
  gdouble dpi;

  backend = clutter_get_default_backend ();
  dpi = clutter_backend_get_resolution (backend);
  if (dpi < 0)
    dpi = DPI_FALLBACK;

  return mm * dpi / 25.4;
}

/**
 * clutter_units_pt:
 * @pt: typographic points to convert
 *
 * Converts a value in typographic points to #ClutterUnit<!-- -->s
 * at the current DPI.
 *
 * Return value: the value in units
 *
 * Since: 1.0
 */
ClutterUnit
clutter_units_pt (gdouble pt)
{
  ClutterBackend *backend;
  gdouble dpi;

  backend = clutter_get_default_backend ();
  dpi = clutter_backend_get_resolution (backend);
  if (dpi < 0)
    dpi = DPI_FALLBACK;

  return pt * dpi / 72.0;
}

/**
 * clutter_units_em:
 * @em: em to convert
 *
 * Converts a value in em to #ClutterUnit<!-- -->s at the
 * current DPI
 *
 * Return value: the value in units
 *
 * Since: 1.0
 */
ClutterUnit
clutter_units_em (gdouble em)
{
  ClutterBackend *backend = clutter_get_default_backend ();

  return em * _clutter_backend_get_units_per_em (backend, NULL);
}

/**
 * clutter_units_em_for_font:
 * @font_name: the font name and size
 * @em: em to convert
 *
 * Converts a value in em to #ClutterUnit<!-- -->s at the
 * current DPI for the given font name.
 *
 * The @font_name string must be in a format that
 * pango_font_description_from_string() can parse, like
 * for clutter_text_set_font_name() or clutter_backend_set_font_name().
 *
 * Return value: the value in units
 *
 * Since: 1.0
 */
ClutterUnit
clutter_units_em_for_font (const gchar *font_name,
                           gdouble      em)
{
  ClutterBackend *backend = clutter_get_default_backend ();

  if (font_name == NULL || *font_name == '\0')
    return em * _clutter_backend_get_units_per_em (backend, NULL);
  else
    {
      PangoFontDescription *font_desc;
      gfloat res;

      font_desc = pango_font_description_from_string (font_name);
      if (font_desc == NULL)
        res = -1.0;
      else
        {
          res = em * _clutter_backend_get_units_per_em (backend, font_desc);

          pango_font_description_free (font_desc);
        }

      return res;
    }
}

/**
 * clutter_units_pixels:
 * @px: pixels to convert
 *
 * Converts a value in pixels to #ClutterUnit<!-- -->s
 *
 * Return value: the value in units
 *
 * Since: 1.0
 */
ClutterUnit
clutter_units_pixels (gint px)
{
  return CLUTTER_UNITS_FROM_INT (px);
}

/**
 * clutter_units_to_pixels:
 * @units: units to convert
 *
 * Converts a value in #ClutterUnit<!-- -->s to pixels
 *
 * Return value: the value in pixels
 *
 * Since: 1.0
 */
gint
clutter_units_to_pixels (ClutterUnit units)
{
  return CLUTTER_UNITS_TO_INT (units);
}

/*
 * GValue and GParamSpec integration
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
clutter_value_init_unit (GValue *value)
{
  value->data[0].v_float = 0.0;
}

static void
clutter_value_copy_unit (const GValue *src,
                         GValue       *dest)
{
  dest->data[0].v_float = src->data[0].v_float;
}

static gchar *
clutter_value_collect_unit (GValue      *value,
                            guint        n_collect_values,
                            GTypeCValue *collect_values,
                            guint        collect_flags)
{
  value->data[0].v_float = collect_values[0].v_double;

  return NULL;
}

static gchar *
clutter_value_lcopy_unit (const GValue *value,
                          guint         n_collect_values,
                          GTypeCValue  *collect_values,
                          guint         collect_flags)
{
  gfloat *units_p = collect_values[0].v_pointer;

  if (!units_p)
    return g_strdup_printf ("value location for '%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  *units_p = value->data[0].v_float;

  return NULL;
}

static void
clutter_value_transform_unit_int (const GValue *src,
                                  GValue       *dest)
{
  dest->data[0].v_int = CLUTTER_UNITS_TO_INT (src->data[0].v_float);
}

static void
clutter_value_transform_int_unit (const GValue *src,
                                  GValue       *dest)
{
  dest->data[0].v_float = CLUTTER_UNITS_FROM_INT (src->data[0].v_int);
}

static void
clutter_value_transform_unit_float (const GValue *src,
                                    GValue       *dest)
{
  dest->data[0].v_float = CLUTTER_UNITS_TO_FLOAT (src->data[0].v_float);
}

static void
clutter_value_transform_float_unit (const GValue *src,
                                    GValue       *dest)
{
  dest->data[0].v_float = CLUTTER_UNITS_FROM_FLOAT (src->data[0].v_float);
}

#if 0
static void
clutter_value_transform_unit_fixed (const GValue *src,
                                    GValue       *dest)
{
  dest->data[0].v_int = CLUTTER_UNITS_TO_FIXED (src->data[0].v_float);
}

static void
clutter_value_transform_fixed_unit (const GValue *src,
                                    GValue       *dest)
{
  dest->data[0].v_float = CLUTTER_UNITS_FROM_FIXED (src->data[0].v_int);
}
#endif

static const GTypeValueTable _clutter_unit_value_table = {
  clutter_value_init_unit,
  NULL,
  clutter_value_copy_unit,
  NULL,
  "d",
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
      g_value_register_transform_func (G_TYPE_INT, _clutter_unit_type,
                                       clutter_value_transform_int_unit);

      g_value_register_transform_func (_clutter_unit_type, G_TYPE_FLOAT,
                                       clutter_value_transform_unit_float);
      g_value_register_transform_func (G_TYPE_FLOAT, _clutter_unit_type,
                                       clutter_value_transform_float_unit);
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

  value->data[0].v_float = units;
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

  return value->data[0].v_float;
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
  value->data[0].v_float = CLUTTER_PARAM_SPEC_UNIT (pspec)->default_value;
}

static gboolean
param_unit_validate (GParamSpec *pspec,
                     GValue     *value)
{
  ClutterParamSpecUnit *uspec = CLUTTER_PARAM_SPEC_UNIT (pspec);
  gfloat oval = value->data[0].v_float;

  g_assert (CLUTTER_IS_PARAM_SPEC_UNIT (pspec));

  value->data[0].v_float = CLAMP (value->data[0].v_float,
                                  uspec->minimum,
                                  uspec->maximum);

  return value->data[0].v_float != oval;
}

static gint
param_unit_values_cmp (GParamSpec   *pspec,
                       const GValue *value1,
                       const GValue *value2)
{
  gfloat epsilon = FLOAT_EPSILON;

  if (value1->data[0].v_float < value2->data[0].v_float)
    return - (value2->data[0].v_float - value1->data[0].v_float > epsilon);
  else
    return value1->data[0].v_float - value2->data[0].v_float > epsilon;
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
