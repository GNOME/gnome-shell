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
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_UNITS_H__
#define __CLUTTER_UNITS_H__

#include <glib-object.h>

#include <cogl/cogl.h>

G_BEGIN_DECLS

/**
 * ClutterUnits:
 *
 * An opaque structure, to be used to store sizing and positioning
 * values along with their unit.
 *
 * Since: 1.0
 */
typedef struct _ClutterUnits    ClutterUnits;

struct _ClutterUnits
{
  /*< private >*/
  ClutterUnitType unit_type;

  gfloat value;

  /* pre-filled by the provided constructors */

  /* cached pixel value */
  gfloat pixels;

  /* whether the :pixels field is set */
  guint pixels_set;

  /* the serial coming from the backend, used to evict the cache */
  gint32 serial;

  /* padding for eventual expansion */
  gint32 __padding_1;
  gint64 __padding_2;
};

CLUTTER_AVAILABLE_IN_1_0
GType           clutter_units_get_type         (void) G_GNUC_CONST;
CLUTTER_AVAILABLE_IN_1_0
ClutterUnitType clutter_units_get_unit_type    (const ClutterUnits *units);
CLUTTER_AVAILABLE_IN_1_0
gfloat          clutter_units_get_unit_value   (const ClutterUnits *units);

CLUTTER_AVAILABLE_IN_1_0
ClutterUnits *  clutter_units_copy             (const ClutterUnits *units);
CLUTTER_AVAILABLE_IN_1_0
void            clutter_units_free             (ClutterUnits       *units);

CLUTTER_AVAILABLE_IN_1_0
void            clutter_units_from_pixels      (ClutterUnits       *units,
                                                gint                px);
CLUTTER_AVAILABLE_IN_1_0
void            clutter_units_from_em          (ClutterUnits       *units,
                                                gfloat              em);
CLUTTER_AVAILABLE_IN_1_0
void            clutter_units_from_em_for_font (ClutterUnits       *units,
                                                const gchar        *font_name,
                                                gfloat              em);
CLUTTER_AVAILABLE_IN_1_0
void            clutter_units_from_mm          (ClutterUnits       *units,
                                                gfloat              mm);
CLUTTER_AVAILABLE_IN_1_0
void            clutter_units_from_cm          (ClutterUnits       *units,
                                                gfloat              cm);
CLUTTER_AVAILABLE_IN_1_0
void            clutter_units_from_pt          (ClutterUnits       *units,
                                                gfloat              pt);

CLUTTER_AVAILABLE_IN_1_0
gfloat          clutter_units_to_pixels        (ClutterUnits       *units);

CLUTTER_AVAILABLE_IN_1_0
gboolean        clutter_units_from_string      (ClutterUnits       *units,
                                                const gchar        *str);
CLUTTER_AVAILABLE_IN_1_0
gchar *         clutter_units_to_string        (const ClutterUnits *units);

/* shorthands for the constructors */
#define clutter_units_pixels            clutter_units_from_pixels
#define clutter_units_em                clutter_units_from_em
#define clutter_units_em_for_font       clutter_units_from_em_for_font
#define clutter_units_mm                clutter_units_from_mm
#define clutter_units_cm                clutter_units_from_cm
#define clutter_units_pt                clutter_units_from_pt

#define CLUTTER_TYPE_UNITS                 (clutter_units_get_type ())
#define CLUTTER_TYPE_PARAM_UNITS           (clutter_param_units_get_type ())
#define CLUTTER_PARAM_SPEC_UNITS(pspec)    (G_TYPE_CHECK_INSTANCE_CAST ((pspec), CLUTTER_TYPE_PARAM_UNITS, ClutterParamSpecUnits))
#define CLUTTER_IS_PARAM_SPEC_UNITS(pspec) (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), CLUTTER_TYPE_PARAM_UNITS))

/**
 * CLUTTER_VALUE_HOLDS_UNITS:
 * @x: a #GValue
 *
 * Evaluates to %TRUE if @x holds a #ClutterUnits value
 *
 * Since: 0.8
 */
#define CLUTTER_VALUE_HOLDS_UNITS(x)    (G_VALUE_HOLDS ((x), CLUTTER_TYPE_UNITS))

typedef struct _ClutterParamSpecUnits   ClutterParamSpecUnits;

/**
 * ClutterParamSpecUnits: (skip)
 * @default_type: default type
 * @default_value: default value
 * @minimum: lower boundary
 * @maximum: higher boundary
 *
 * #GParamSpec subclass for unit based properties.
 *
 * Since: 1.0
 */
struct _ClutterParamSpecUnits
{
  /*< private >*/
  GParamSpec parent_instance;

  /*< public >*/
  ClutterUnitType default_type;

  gfloat default_value;
  gfloat minimum;
  gfloat maximum;
};

CLUTTER_AVAILABLE_IN_1_0
GType clutter_param_units_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_0
GParamSpec *            clutter_param_spec_units (const gchar        *name,
                                                  const gchar        *nick,
                                                  const gchar        *blurb,
                                                  ClutterUnitType     default_type,
                                                  gfloat              minimum,
                                                  gfloat              maximum,
                                                  gfloat              default_value,
                                                  GParamFlags         flags);

CLUTTER_AVAILABLE_IN_1_0
void                    clutter_value_set_units  (GValue             *value,
                                                  const ClutterUnits *units);
CLUTTER_AVAILABLE_IN_1_0
const ClutterUnits *    clutter_value_get_units  (const GValue       *value);

G_END_DECLS

#endif /* __CLUTTER_UNITS_H__ */
