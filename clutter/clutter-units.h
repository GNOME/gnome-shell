/* -*- mode:C; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By: Tomas Frydrych  <tf@openedhand.com>
 *              Emmanuele Bassu  <ebassi@linux.intel.com>
 *
 * Copyright (C) 2007, 2008 OpenedHand
 * Copyright (C) 2009 Intel Corp.
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
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly.h"
#endif

#ifndef __CLUTTER_UNITS_H__
#define __CLUTTER_UNITS_H__

#include <glib-object.h>
#include <clutter/clutter-fixed.h>

G_BEGIN_DECLS

/**
 * ClutterUnit:
 *
 * Device independent unit used by Clutter. The value held can be
 * transformed into other units, likes pixels.
 *
 * Since: 0.4
 */
typedef float ClutterUnit;

#define CLUTTER_UNITS_FROM_INT(x)        ((float)(x))
#define CLUTTER_UNITS_TO_INT(x)          ((int)(x))

#define CLUTTER_UNITS_FROM_FLOAT(x)      (x)
#define CLUTTER_UNITS_TO_FLOAT(x)        (x)

#define CLUTTER_UNITS_FROM_FIXED(x)      (COGL_FIXED_TO_FLOAT (x))
#define CLUTTER_UNITS_TO_FIXED(x)        (COGL_FIXED_FROM_FLOAT (x))

/**
 * CLUTTER_UNITS_FORMAT:
 *
 * Format string that should be used for scanning and printing units.
 * It is a string literal, but it does not include the percent sign to
 * allow precision and length modifiers between the percent sign and
 * the format:
 *
 * |[
 *   g_print ("%" CLUTTER_UNITS_FORMAT, units);
 * ]|
 *
 * Since: 1.0
 */
#define CLUTTER_UNITS_FORMAT             "f"

/**
 * CLUTTER_UNITS_FROM_DEVICE:
 * @x: value in pixels
 *
 * Converts @x from pixels to #ClutterUnit<!-- -->s
 *
 * Since: 0.6
 */
#define CLUTTER_UNITS_FROM_DEVICE(x)    (clutter_units_pixels ((x)))

/**
 * CLUTTER_UNITS_TO_DEVICE:
 * @x: value in #ClutterUnit<!-- -->s
 *
 * Converts @x from #ClutterUnit<!-- -->s to pixels
 *
 * Since: 0.6
 */
#define CLUTTER_UNITS_TO_DEVICE(x)      (clutter_units_to_pixels ((x)))

/**
 * CLUTTER_UNITS_FROM_PANGO_UNIT:
 * @x: value in Pango units
 *
 * Converts a value in Pango units to #ClutterUnit<!-- -->s
 *
 * Since: 0.6
 */
#define CLUTTER_UNITS_FROM_PANGO_UNIT(x) ((float)((x) / 1024.0))

/**
 * CLUTTER_UNITS_TO_PANGO_UNIT:
 * @x: value in #ClutterUnit<!-- -->s
 *
 * Converts a value in #ClutterUnit<!-- -->s to Pango units
 *
 * Since: 0.6
 */
#define CLUTTER_UNITS_TO_PANGO_UNIT(x)   ((int)((x) * 1024))

/**
 * CLUTTER_UNITS_FROM_MM:
 * @x: a value in millimeters
 *
 * Converts a value in millimeters into #ClutterUnit<!-- -->s
 *
 * Since: 0.6
 */
#define CLUTTER_UNITS_FROM_MM(x)        (clutter_units_mm (x))

/**
 * CLUTTER_UNITS_FROM_POINTS:
 * @x: a value in typographic points
 *
 * Converts a value in typographic points into #ClutterUnit<!-- -->s
 *
 * Since: 0.6
 */
#define CLUTTER_UNITS_FROM_POINTS(x)    (clutter_units_pt (x))

/**
 * CLUTTER_UNITS_FROM_EM:
 * @x: a value in em
 *
 * Converts a value in em into #ClutterUnit<!-- -->s
 *
 * Since: 1.0
 */
#define CLUTTER_UNITS_FROM_EM(x)        (clutter_units_em (x))

ClutterUnit clutter_units_mm        (gdouble     mm);
ClutterUnit clutter_units_pt        (gdouble     pt);
ClutterUnit clutter_units_em        (gdouble     em);
ClutterUnit clutter_units_pixels    (gint        px);

gint        clutter_units_to_pixels (ClutterUnit units);

#define CLUTTER_TYPE_UNIT                 (clutter_unit_get_type ())
#define CLUTTER_TYPE_PARAM_UNIT           (clutter_param_unit_get_type ())
#define CLUTTER_PARAM_SPEC_UNIT(pspec)    (G_TYPE_CHECK_INSTANCE_CAST ((pspec), CLUTTER_TYPE_PARAM_UNIT, ClutterParamSpecUnit))
#define CLUTTER_IS_PARAM_SPEC_UNIT(pspec) (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), CLUTTER_TYPE_PARAM_UNIT))

/**
 * CLUTTER_MAXUNIT:
 *
 * Higher boundary for a #ClutterUnit
 *
 * Since: 0.8
 */
#define CLUTTER_MAXUNIT         (G_MAXFLOAT)

/**
 * CLUTTER_MINUNIT:
 *
 * Lower boundary for a #ClutterUnit
 *
 * Since: 0.8
 */
#define CLUTTER_MINUNIT         (-G_MAXFLOAT)

/**
 * CLUTTER_VALUE_HOLDS_UNIT:
 * @x: a #GValue
 *
 * Evaluates to %TRUE if @x holds #ClutterUnit<!-- -->s.
 *
 * Since: 0.8
 */
#define CLUTTER_VALUE_HOLDS_UNIT(x)       (G_VALUE_HOLDS ((x), CLUTTER_TYPE_UNIT))

typedef struct _ClutterParamSpecUnit    ClutterParamSpecUnit;

/**
 * ClutterParamSpecUnit:
 * @minimum: lower boundary
 * @maximum: higher boundary
 * @default_value: default value
 *
 * #GParamSpec subclass for unit based properties.
 *
 * Since: 0.8
 */
struct _ClutterParamSpecUnit
{
  /*< private >*/
  GParamSpec    parent_instance;

  /*< public >*/
  ClutterUnit   minimum;
  ClutterUnit   maximum;
  ClutterUnit   default_value;
};

GType       clutter_unit_get_type       (void) G_GNUC_CONST;
GType       clutter_param_unit_get_type (void) G_GNUC_CONST;

void        clutter_value_set_unit (GValue       *value,
                                    ClutterUnit   units);
ClutterUnit clutter_value_get_unit (const GValue *value);

GParamSpec *clutter_param_spec_unit (const gchar *name,
                                     const gchar *nick,
                                     const gchar *blurb,
                                     ClutterUnit  minimum,
                                     ClutterUnit  maximum,
                                     ClutterUnit  default_value,
                                     GParamFlags  flags);

G_END_DECLS

#endif /* __CLUTTER_UNITS_H__ */
