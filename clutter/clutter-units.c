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
 * @short_description: A logical distance unit
 *
 * #ClutterUnits is a structure holding a logical distance value along with
 * its type, expressed as a value of the #ClutterUnitType enumeration. It is
 * possible to use #ClutterUnits to store a position or a size in units
 * different than pixels, and convert them whenever needed (for instance
 * inside the #ClutterActor::allocate() virtual function, or inside the
 * #ClutterActor::get_preferred_width() and #ClutterActor::get_preferred_height()
 * virtual functions.
 *
 * In order to register a #ClutterUnits property, the #ClutterParamSpecUnits
 * #GParamSpec sub-class should be used:
 *
 * |[
 *   GParamSpec *pspec;
 *
 *   pspec = clutter_param_spec_units ("active-width",
 *                                     "Width",
 *                                     "Width of the active area, in millimeters",
 *                                     CLUTTER_UNIT_MM,
 *                                     0.0, 12.0,
 *                                     12.0,
 *                                     G_PARAM_READWRITE);
 *   g_object_class_install_property (gobject_class, PROP_WIDTH, pspec);
 * ]|
 *
 * A #GValue holding units can be manipulated using clutter_value_set_units()
 * and clutter_value_get_units(). #GValue<!-- -->s containing a #ClutterUnits
 * value can also be transformed to #GValue<!-- -->s initialized with
 * %G_TYPE_INT, %G_TYPE_FLOAT and %G_TYPE_STRING through implicit conversion
 * and using g_value_transform().
 *
 * #ClutterUnits is available since Clutter 1.0
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-units.h"
#include "clutter-private.h"
#include "clutter-interval.h"

#define DPI_FALLBACK    (96.0)

#define FLOAT_EPSILON   (1e-30)

static gfloat
units_mm_to_pixels (gfloat mm)
{
  ClutterBackend *backend;
  gdouble dpi;

  backend = clutter_get_default_backend ();
  dpi = clutter_backend_get_resolution (backend);
  if (dpi < 0)
    dpi = DPI_FALLBACK;

  return mm * dpi / 25.4;
}

static gfloat
units_cm_to_pixels (gfloat cm)
{
  return units_mm_to_pixels (cm * 10);
}

static gfloat
units_pt_to_pixels (gfloat pt)
{
  ClutterBackend *backend;
  gdouble dpi;

  backend = clutter_get_default_backend ();
  dpi = clutter_backend_get_resolution (backend);
  if (dpi < 0)
    dpi = DPI_FALLBACK;

  return pt * dpi / 72.0;
}

static gfloat
units_em_to_pixels (const gchar *font_name,
                    gfloat       em)
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
 * clutter_units_from_mm:
 * @units: a #ClutterUnits
 * @mm: millimeters
 *
 * Stores a value in millimiters inside @units
 *
 * Since: 1.0
 */
void
clutter_units_from_mm (ClutterUnits *units,
                       gfloat        mm)
{
  ClutterBackend *backend;

  g_return_if_fail (units != NULL);

  backend = clutter_get_default_backend ();

  units->unit_type  = CLUTTER_UNIT_MM;
  units->value      = mm;
  units->pixels     = units_mm_to_pixels (mm);
  units->pixels_set = TRUE;
  units->serial     = _clutter_backend_get_units_serial (backend);
}

/**
 * clutter_units_from_cm:
 * @units: a #ClutterUnits
 * @cm: centimeters
 *
 * Stores a value in centimeters inside @units
 *
 * Since: 1.2
 */
void
clutter_units_from_cm (ClutterUnits *units,
                       gfloat        cm)
{
  ClutterBackend *backend;

  g_return_if_fail (units != NULL);

  backend = clutter_get_default_backend ();

  units->unit_type  = CLUTTER_UNIT_CM;
  units->value      = cm;
  units->pixels     = units_cm_to_pixels (cm);
  units->pixels_set = TRUE;
  units->serial     = _clutter_backend_get_units_serial (backend);
}

/**
 * clutter_units_from_pt:
 * @units: a #ClutterUnits
 * @pt: typographic points
 *
 * Stores a value in typographic points inside @units
 *
 * Since: 1.0
 */
void
clutter_units_from_pt (ClutterUnits *units,
                       gfloat        pt)
{
  ClutterBackend *backend;

  g_return_if_fail (units != NULL);

  backend = clutter_get_default_backend ();

  units->unit_type  = CLUTTER_UNIT_POINT;
  units->value      = pt;
  units->pixels     = units_pt_to_pixels (pt);
  units->pixels_set = TRUE;
  units->serial     = _clutter_backend_get_units_serial (backend);
}

/**
 * clutter_units_from_em:
 * @units: a #ClutterUnits
 * @em: em
 *
 * Stores a value in em inside @units, using the default font
 * name as returned by clutter_backend_get_font_name()
 *
 * Since: 1.0
 */
void
clutter_units_from_em (ClutterUnits *units,
                       gfloat        em)
{
  ClutterBackend *backend;

  g_return_if_fail (units != NULL);

  backend = clutter_get_default_backend ();

  units->unit_type  = CLUTTER_UNIT_EM;
  units->value      = em;
  units->pixels     = units_em_to_pixels (NULL, em);
  units->pixels_set = TRUE;
  units->serial     = _clutter_backend_get_units_serial (backend);
}

/**
 * clutter_units_from_em_for_font:
 * @units: a #ClutterUnits
 * @font_name: the font name and size
 * @em: em
 *
 * Stores a value in em inside @units using @font_name
 *
 * Since: 1.0
 */
void
clutter_units_from_em_for_font (ClutterUnits *units,
                                const gchar  *font_name,
                                gfloat        em)
{
  ClutterBackend *backend;

  g_return_if_fail (units != NULL);

  backend = clutter_get_default_backend ();

  units->unit_type  = CLUTTER_UNIT_EM;
  units->value      = em;
  units->pixels     = units_em_to_pixels (font_name, em);
  units->pixels_set = TRUE;
  units->serial     = _clutter_backend_get_units_serial (backend);
}

/**
 * clutter_units_from_pixels:
 * @units: a #ClutterUnits
 * @px: pixels
 *
 * Stores a value in pixels inside @units
 *
 * Since: 1.0
 */
void
clutter_units_from_pixels (ClutterUnits *units,
                           gint          px)
{
  ClutterBackend *backend;

  g_return_if_fail (units != NULL);

  backend = clutter_get_default_backend ();

  units->unit_type  = CLUTTER_UNIT_PIXEL;
  units->value      = px;
  units->pixels     = px;
  units->pixels_set = TRUE;
  units->serial     = _clutter_backend_get_units_serial (backend);
}

/**
 * clutter_units_get_unit_type:
 * @units: a #ClutterUnits
 *
 * Retrieves the unit type of the value stored inside @units
 *
 * Return value: a unit type
 *
 * Since: 1.0
 */
ClutterUnitType
clutter_units_get_unit_type (const ClutterUnits *units)
{
  g_return_val_if_fail (units != NULL, CLUTTER_UNIT_PIXEL);

  return units->unit_type;
}

/**
 * clutter_units_get_unit_value:
 * @units: a #ClutterUnits
 *
 * Retrieves the value stored inside @units
 *
 * Return value: the value stored inside a #ClutterUnits
 *
 * Since: 1.0
 */
gfloat
clutter_units_get_unit_value (const ClutterUnits *units)
{
  g_return_val_if_fail (units != NULL, 0.0);

  return units->value;
}

/**
 * clutter_units_copy:
 * @units: the #ClutterUnits to copy
 *
 * Copies @units
 *
 * Return value: the newly created copy of a #ClutterUnits structure.
 *   Use clutter_units_free() to free the allocated resources
 *
 * Since: 1.0
 */
ClutterUnits *
clutter_units_copy (const ClutterUnits *units)
{
  if (units != NULL)
    return g_slice_dup (ClutterUnits, units);

  return NULL;
}

/**
 * clutter_units_free:
 * @units: the #ClutterUnits to free
 *
 * Frees the resources allocated by @units
 *
 * You should only call this function on a #ClutterUnits
 * created using clutter_units_copy()
 *
 * Since: 1.0
 */
void
clutter_units_free (ClutterUnits *units)
{
  if (units != NULL)
    g_slice_free (ClutterUnits, units);
}

/**
 * clutter_units_to_pixels:
 * @units: units to convert
 *
 * Converts a value in #ClutterUnits to pixels
 *
 * Return value: the value in pixels
 *
 * Since: 1.0
 */
gfloat
clutter_units_to_pixels (ClutterUnits *units)
{
  ClutterBackend *backend;

  g_return_val_if_fail (units != NULL, 0.0);

  /* if the backend settings changed we evict the cached value */
  backend = clutter_get_default_backend ();
  if (units->serial != _clutter_backend_get_units_serial (backend))
    units->pixels_set = FALSE;

  if (units->pixels_set)
    return units->pixels;

  switch (units->unit_type)
    {
    case CLUTTER_UNIT_MM:
      units->pixels = units_mm_to_pixels (units->value);
      break;

    case CLUTTER_UNIT_CM:
      units->pixels = units_cm_to_pixels (units->value);
      break;

    case CLUTTER_UNIT_POINT:
      units->pixels = units_pt_to_pixels (units->value);
      break;

    case CLUTTER_UNIT_EM:
      units->pixels = units_em_to_pixels (NULL, units->value);
      break;

    case CLUTTER_UNIT_PIXEL:
      units->pixels = units->value;
      break;
    }

  units->pixels_set = TRUE;
  units->serial = _clutter_backend_get_units_serial (backend);

  return units->pixels;
}

/**
 * clutter_units_from_string:
 * @units: a #ClutterUnits
 * @str: the string to convert
 *
 * Parses a value and updates @units with it
 *
 * A #ClutterUnits expressed in string should match:
 *
 * |[
 *   units: wsp* unit-value wsp* unit-name? wsp*
 *   unit-value: number
 *   unit-name: 'px' | 'pt' | 'mm' | 'em' | 'cm'
 *   number: digit+
 *           | digit* sep digit+
 *   sep: '.' | ','
 *   digit: '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'
 *   wsp: (#0x20 | #0x9 | #0xA | #0xB | #0xC | #0xD)+
 * ]|
 *
 * For instance, these are valid strings:
 *
 * |[
 *   10 px
 *   5.1 em
 *   24 pt
 *   12.6 mm
 *   .3 cm
 * ]|
 *
 * While these are not:
 *
 * |[
 *   42 cats
 *   omg!1!ponies
 * ]|
 *
 * <note>If no unit is specified, pixels are assumed.</note>
 *
 * Return value: %TRUE if the string was successfully parsed,
 *   and %FALSE otherwise
 *
 * Since: 1.0
 */
gboolean
clutter_units_from_string (ClutterUnits *units,
                           const gchar  *str)
{
  ClutterBackend *backend;
  ClutterUnitType unit_type;
  gfloat value;

  g_return_val_if_fail (units != NULL, FALSE);
  g_return_val_if_fail (str != NULL, FALSE);

  /* strip leading space */
  while (g_ascii_isspace (*str))
    str++;

  if (*str == '\0')
    return FALSE;

  /* integer part */
  value = (gfloat) strtoul (str, (char **) &str, 10);

  if (*str == '.' || *str == ',')
    {
      gfloat divisor = 0.1;

      /* 5.cm is not a valid number */
      if (!g_ascii_isdigit (*++str))
        return FALSE;

      while (g_ascii_isdigit (*str))
        {
          value += (*str - '0') * divisor;
          divisor *= 0.1;
          str++;
        }
    }

  while (g_ascii_isspace (*str))
    str++;

  /* assume pixels by default, if no unit is specified */
  if (*str == '\0')
    unit_type = CLUTTER_UNIT_PIXEL;
  else if (strncmp (str, "em", 2) == 0)
    {
      unit_type = CLUTTER_UNIT_EM;
      str += 2;
    }
  else if (strncmp (str, "mm", 2) == 0)
    {
      unit_type = CLUTTER_UNIT_MM;
      str += 2;
    }
  else if (strncmp (str, "cm", 2) == 0)
    {
      unit_type = CLUTTER_UNIT_CM;
      str += 2;
    }
  else if (strncmp (str, "pt", 2) == 0)
    {
      unit_type = CLUTTER_UNIT_POINT;
      str += 2;
    }
  else if (strncmp (str, "px", 2) == 0)
    {
      unit_type = CLUTTER_UNIT_PIXEL;
      str += 2;
    }
  else
        return FALSE;

  /* ensure the unit is only followed by white space */
  while (g_ascii_isspace (*str))
    str++;
  if (*str != '\0')
    return FALSE;

  backend = clutter_get_default_backend ();

  units->unit_type = unit_type;
  units->value = value;
  units->pixels_set = FALSE;
  units->serial = _clutter_backend_get_units_serial (backend);

  return TRUE;
}

static const gchar *
clutter_unit_type_name (ClutterUnitType unit_type)
{
  switch (unit_type)
    {
    case CLUTTER_UNIT_MM:
      return "mm";

    case CLUTTER_UNIT_CM:
      return "cm";

    case CLUTTER_UNIT_POINT:
      return "pt";

    case CLUTTER_UNIT_EM:
      return "em";

    case CLUTTER_UNIT_PIXEL:
      return "px";
    }

  g_warning ("Invalid unit type %d", (int) unit_type);

  return "<invalid>";
}

/**
 * clutter_units_to_string:
 * @units: a #ClutterUnits
 *
 * Converts @units into a string
 *
 * See clutter_units_from_string() for the units syntax and for
 * examples of output
 *
 * <note>Fractional values are truncated to the second decimal
 * position for em, mm and cm, and to the first decimal position for
 * typographic points. Pixels are integers.</note>
 *
 * Return value: a newly allocated string containing the encoded
 *   #ClutterUnits value. Use g_free() to free the string
 *
 * Since: 1.0
 */
gchar *
clutter_units_to_string (const ClutterUnits *units)
{
  const gchar *unit_name = NULL;
  const gchar *fmt = NULL;
  gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_return_val_if_fail (units != NULL, NULL);

  switch (units->unit_type)
    {
    /* special case: there is no such thing as "half a pixel", so
     * we round up to the nearest integer using C default
     */
    case CLUTTER_UNIT_PIXEL:
      return g_strdup_printf ("%d px", (int) units->value);

    case CLUTTER_UNIT_MM:
      unit_name = "mm";
      fmt = "%.2f";
      break;

    case CLUTTER_UNIT_CM:
      unit_name = "cm";
      fmt = "%.2f";
      break;

    case CLUTTER_UNIT_POINT:
      unit_name = "pt";
      fmt = "%.1f";
      break;

    case CLUTTER_UNIT_EM:
      unit_name = "em";
      fmt = "%.2f";
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, fmt, units->value);

  return g_strconcat (buf, " ", unit_name, NULL);
}

/*
 * ClutterInterval integration
 */

static gboolean
clutter_units_progress (const GValue *a,
                        const GValue *b,
                        gdouble       progress,
                        GValue       *retval)
{
  ClutterUnits *a_units = (ClutterUnits *) clutter_value_get_units (a);
  ClutterUnits *b_units = (ClutterUnits *) clutter_value_get_units (b);
  ClutterUnits  res;
  gfloat a_px, b_px, value;

  a_px = clutter_units_to_pixels (a_units);
  b_px = clutter_units_to_pixels (b_units);
  value = progress * (b_px - a_px) + a_px;

  clutter_units_from_pixels (&res, value);
  clutter_value_set_units (retval, &res);

  return TRUE;
}

/*
 * GValue and GParamSpec integration
 */

/* units to integer */
static void
clutter_value_transform_units_int (const GValue *src,
                                   GValue       *dest)
{
  dest->data[0].v_int = clutter_units_to_pixels (src->data[0].v_pointer);
}

/* integer to units */
static void
clutter_value_transform_int_units (const GValue *src,
                                   GValue       *dest)
{
  clutter_units_from_pixels (dest->data[0].v_pointer, src->data[0].v_int);
}

/* units to float */
static void
clutter_value_transform_units_float (const GValue *src,
                                     GValue       *dest)
{
  dest->data[0].v_float = clutter_units_to_pixels (src->data[0].v_pointer);
}

/* float to units */
static void
clutter_value_transform_float_units (const GValue *src,
                                     GValue       *dest)
{
  clutter_units_from_pixels (dest->data[0].v_pointer, src->data[0].v_float);
}

/* units to string */
static void
clutter_value_transform_units_string (const GValue *src,
                                      GValue       *dest)
{
  gchar *string = clutter_units_to_string (src->data[0].v_pointer);

  g_value_take_string (dest, string);
}

/* string to units */
static void
clutter_value_transform_string_units (const GValue *src,
                                      GValue       *dest)
{
  ClutterUnits units = { CLUTTER_UNIT_PIXEL, 0.0f };

  clutter_units_from_string (&units, g_value_get_string (src));

  clutter_value_set_units (dest, &units);
}

GType
clutter_units_get_type (void)
{
  static volatile gsize clutter_units_type__volatile = 0;

  if (g_once_init_enter (&clutter_units_type__volatile))
    {
      GType clutter_units_type =
        g_boxed_type_register_static (I_("ClutterUnits"),
                                      (GBoxedCopyFunc) clutter_units_copy,
                                      (GBoxedFreeFunc) clutter_units_free);

      g_value_register_transform_func (clutter_units_type, G_TYPE_INT,
                                       clutter_value_transform_units_int);
      g_value_register_transform_func (G_TYPE_INT, clutter_units_type,
                                       clutter_value_transform_int_units);

      g_value_register_transform_func (clutter_units_type, G_TYPE_FLOAT,
                                       clutter_value_transform_units_float);
      g_value_register_transform_func (G_TYPE_FLOAT, clutter_units_type,
                                       clutter_value_transform_float_units);

      g_value_register_transform_func (clutter_units_type, G_TYPE_STRING,
                                       clutter_value_transform_units_string);
      g_value_register_transform_func (G_TYPE_STRING, clutter_units_type,
                                       clutter_value_transform_string_units);

      clutter_interval_register_progress_func (clutter_units_type,
                                               clutter_units_progress);

      g_once_init_leave (&clutter_units_type__volatile, clutter_units_type);
    }

  return clutter_units_type__volatile;
}

/**
 * clutter_value_set_units:
 * @value: a #GValue initialized to #CLUTTER_TYPE_UNIT
 * @units: the units to set
 *
 * Sets @value to @units
 *
 * Since: 0.8
 */
void
clutter_value_set_units (GValue             *value,
                         const ClutterUnits *units)
{
  g_return_if_fail (CLUTTER_VALUE_HOLDS_UNITS (value));

  value->data[0].v_pointer = clutter_units_copy (units);
}

/**
 * clutter_value_get_units:
 * @value: a #GValue initialized to #CLUTTER_TYPE_UNIT
 *
 * Gets the #ClutterUnit<!-- -->s contained in @value.
 *
 * Return value: the units inside the passed #GValue
 *
 * Since: 0.8
 */
G_CONST_RETURN ClutterUnits *
clutter_value_get_units (const GValue *value)
{
  g_return_val_if_fail (CLUTTER_VALUE_HOLDS_UNITS (value), NULL);

  return value->data[0].v_pointer;
}

static void
param_units_init (GParamSpec *pspec)
{
  ClutterParamSpecUnits *uspec = CLUTTER_PARAM_SPEC_UNITS (pspec);

  uspec->minimum = -G_MAXFLOAT;
  uspec->maximum = G_MAXFLOAT;
  uspec->default_value = 0.0f;
  uspec->default_type = CLUTTER_UNIT_PIXEL;
}

static void
param_units_set_default (GParamSpec *pspec,
                         GValue     *value)
{
  ClutterParamSpecUnits *uspec = CLUTTER_PARAM_SPEC_UNITS (pspec);
  ClutterUnits units;

  units.unit_type = uspec->default_type;
  units.value = uspec->default_value;
  units.pixels_set = FALSE;

  clutter_value_set_units (value, &units);
}

static gboolean
param_units_validate (GParamSpec *pspec,
                      GValue     *value)
{
  ClutterParamSpecUnits *uspec = CLUTTER_PARAM_SPEC_UNITS (pspec);
  ClutterUnits *units = value->data[0].v_pointer;
  ClutterUnitType otype = units->unit_type;
  gfloat oval = units->value;

  g_assert (CLUTTER_IS_PARAM_SPEC_UNITS (pspec));

  if (otype != uspec->default_type)
    {
      gchar *str = clutter_units_to_string (units);

      g_warning ("The units value of '%s' does not have the same unit "
                 "type as declared by the ClutterParamSpecUnits of '%s'",
                 str,
                 clutter_unit_type_name (otype));

      g_free (str);

      return FALSE;
    }

  units->value = CLAMP (units->value,
                        uspec->minimum,
                        uspec->maximum);

  return units->value != oval;
}

static gint
param_units_values_cmp (GParamSpec   *pspec,
                        const GValue *value1,
                        const GValue *value2)
{
  ClutterUnits *units1 = value1->data[0].v_pointer;
  ClutterUnits *units2 = value2->data[0].v_pointer;
  gfloat v1, v2;

  if (units1->unit_type == units2->unit_type)
    {
      v1 = units1->value;
      v2 = units2->value;
    }
  else
    {
      v1 = clutter_units_to_pixels (units1);
      v2 = clutter_units_to_pixels (units2);
    }

  if (v1 < v2)
    return - (v2 - v1 > FLOAT_EPSILON);
  else
    return v1 - v2 > FLOAT_EPSILON;
}

GType
clutter_param_units_get_type (void)
{
  static GType pspec_type = 0;

  if (G_UNLIKELY (pspec_type == 0))
    {
      const GParamSpecTypeInfo pspec_info = {
        sizeof (ClutterParamSpecUnits),
        16,
        param_units_init,
        CLUTTER_TYPE_UNITS,
        NULL,
        param_units_set_default,
        param_units_validate,
        param_units_values_cmp,
      };

      pspec_type = g_param_type_register_static (I_("ClutterParamSpecUnit"),
                                                 &pspec_info);
    }

  return pspec_type;
}

/**
 * clutter_param_spec_units:
 * @name: name of the property
 * @nick: short name
 * @blurb: description (can be translatable)
 * @default_type: the default type for the #ClutterUnits
 * @minimum: lower boundary
 * @maximum: higher boundary
 * @default_value: default value
 * @flags: flags for the param spec
 *
 * Creates a #GParamSpec for properties using #ClutterUnits.
 *
 * Return value: the newly created #GParamSpec
 *
 * Since: 1.0
 */
GParamSpec *
clutter_param_spec_units (const gchar     *name,
                          const gchar     *nick,
                          const gchar     *blurb,
                          ClutterUnitType  default_type,
                          gfloat           minimum,
                          gfloat           maximum,
                          gfloat           default_value,
                          GParamFlags      flags)
{
  ClutterParamSpecUnits *uspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum,
                        NULL);

  uspec = g_param_spec_internal (CLUTTER_TYPE_PARAM_UNITS,
                                 name, nick, blurb,
                                 flags);

  uspec->default_type = default_type;
  uspec->minimum = minimum;
  uspec->maximum = maximum;
  uspec->default_value = default_value;

  return G_PARAM_SPEC (uspec);
}
