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
 * Device independent unit used by Clutter. The value held can be transformed
 * into other units, likes pixels.
 *
 * Since: 0.4
 */
typedef gint32 ClutterUnit;

/*
 * Currently CLUTTER_UNIT maps directly onto ClutterFixed. Nevertheless, the
 * _FROM_FIXED and _TO_FIXED macros should always be used in case that we
 * decide to change this relationship in the future.
 */

#define CLUTTER_UNITS_FROM_INT(x)        ((float)((x)))
#define CLUTTER_UNITS_TO_INT(x)          ( ((x)))

#define CLUTTER_UNITS_FROM_FLOAT(x)      ( ((x)))
#define CLUTTER_UNITS_TO_FLOAT(x)        ( ((x)))

#define CLUTTER_UNITS_FROM_FIXED(x)      (x)
#define CLUTTER_UNITS_TO_FIXED(x)        (x)

#define CLUTTER_UNITS_FORMAT            "d"

/**
 * CLUTTER_UNITS_FROM_DEVICE:
 * @x: value in pixels
 *
 * Converts @x from pixels to #ClutterUnit<!-- -->s
 *
 * Since: 0.6
 */
#define CLUTTER_UNITS_FROM_DEVICE(x)     CLUTTER_UNITS_FROM_INT ((x))

/**
 * CLUTTER_UNITS_TO_DEVICE:
 * @x: value in #ClutterUnit<!-- -->s
 *
 * Converts @x from #ClutterUnit<!-- -->s to pixels
 *
 * Since: 0.6
 */
#define CLUTTER_UNITS_TO_DEVICE(x)       CLUTTER_UNITS_TO_INT ((x))

#define CLUTTER_UNITS_TMP_FROM_DEVICE(x) (x)
#define CLUTTER_UNITS_TMP_TO_DEVICE(x)   (x)

/**
 * CLUTTER_UNITS_FROM_PANGO_UNIT:
 * @x: value in Pango units
 *
 * Converts a value in Pango units to #ClutterUnit<!-- -->s
 *
 * Since: 0.6
 */
#define CLUTTER_UNITS_FROM_PANGO_UNIT(x) ((x) << 6)

/**
 * CLUTTER_UNITS_TO_PANGO_UNIT:
 * @x: value in #ClutterUnit<!-- -->s
 *
 * Converts a value in #ClutterUnit<!-- -->s to Pango units
 *
 * Since: 0.6
 */
#define CLUTTER_UNITS_TO_PANGO_UNIT(x)   ((x) >> 6)

#define CLUTTER_UNITS_FROM_STAGE_WIDTH_PERCENTAGE(x) \
  ((clutter_actor_get_widthu (clutter_stage_get_default ()) * x) / 100)

#define CLUTTER_UNITS_FROM_STAGE_HEIGHT_PERCENTAGE(x) \
  ((clutter_actor_get_heightu (clutter_stage_get_default ()) * x) / 100)

#define CLUTTER_UNITS_FROM_PARENT_WIDTH_PERCENTAGE(a, x) \
  ((clutter_actor_get_widthu (clutter_actor_get_parent (a)) * x) / 100)

#define CLUTTER_UNITS_FROM_PARENT_HEIGHT_PERCENTAGE(a, x) \
  ((clutter_actor_get_heightu (clutter_actor_get_parent (a)) * x) / 100)

/**
 * CLUTTER_UNITS_FROM_MM:
 * @x: a value in millimeters
 *
 * Converts a value in millimeters into #ClutterUnit<!-- -->s
 *
 * Since: 0.6
 */
#define CLUTTER_UNITS_FROM_MM(x) \
  (CLUTTER_UNITS_FROM_FLOAT ((((x) * clutter_stage_get_resolution ((ClutterStage *) clutter_stage_get_default ())) / 25.4)))

#define CLUTTER_UNITS_FROM_MMX(x) \
  (CFX_DIV (CFX_MUL ((x), clutter_stage_get_resolutionx ((ClutterStage *) clutter_stage_get_default ())), 0x196666))

/**
 * CLUTTER_UNITS_FROM_POINTS:
 * @x: a value in typographic points
 *
 * Converts a value in typographic points into #ClutterUnit<!-- -->s
 *
 * Since: 0.6
 */
#define CLUTTER_UNITS_FROM_POINTS(x) \
  CLUTTER_UNITS_FROM_FLOAT ((((x) * clutter_stage_get_resolution ((ClutterStage *) clutter_stage_get_default ())) / 72.0))

#define CLUTTER_UNITS_FROM_POINTSX(x) \
  (CFX_MUL ((x), clutter_stage_get_resolutionx ((ClutterStage *) clutter_stage_get_default ())) / 72)

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
#define CLUTTER_MAXUNIT         (0x7fffffff)

/**
 * CLUTTER_MINUNIT:
 *
 * Lower boundary for a #ClutterUnit
 *
 * Since: 0.8
 */
#define CLUTTER_MINUNIT         (0x80000000)

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
