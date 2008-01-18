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

#ifndef _HAVE_CLUTTER_UNITS_H
#define _HAVE_CLUTTER_UNITS_H

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

#define CLUTTER_UNITS_FROM_INT(x)        CLUTTER_INT_TO_FIXED ((x))
#define CLUTTER_UNITS_TO_INT(x)          CLUTTER_FIXED_TO_INT ((x))

#define CLUTTER_UNITS_FROM_FLOAT(x)      CLUTTER_FLOAT_TO_FIXED ((x))
#define CLUTTER_UNITS_TO_FLOAT(x)        CLUTTER_FIXED_TO_FLOAT ((x))

#define CLUTTER_UNITS_FROM_FIXED(x)      (x)
#define CLUTTER_UNITS_TO_FIXED(x)        (x)

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

G_END_DECLS

#endif /* _HAVE_CLUTTER_UNITS_H */
