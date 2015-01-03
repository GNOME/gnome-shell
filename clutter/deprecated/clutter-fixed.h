/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Tomas Frydrych <tf@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

#ifndef __CLUTTER_FIXED_H__
#define __CLUTTER_FIXED_H__

#include <cogl/cogl.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_PARAM_FIXED           (clutter_param_fixed_get_type ())
#define CLUTTER_PARAM_SPEC_FIXED(pspec)    (G_TYPE_CHECK_INSTANCE_CAST ((pspec), CLUTTER_TYPE_PARAM_FIXED, ClutterParamSpecFixed))
#define CLUTTER_IS_PARAM_SPEC_FIXED(pspec) (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), CLUTTER_TYPE_PARAM_FIXED))

/**
 * CLUTTER_VALUE_HOLDS_FIXED:
 * @x: a #GValue
 *
 * Evaluates to %TRUE if @x holds a #CoglFixed   .
 *
 * Since: 0.8
 *
 * Deprecated: 1.10: Use %G_VALUE_HOLDS_INT instead
 */
#define CLUTTER_VALUE_HOLDS_FIXED(x)    (G_VALUE_HOLDS ((x), COGL_TYPE_FIXED))

typedef struct _ClutterParamSpecFixed   ClutterParamSpecFixed;

/**
 * ClutterParamSpecFixed: (skip)
 * @minimum: lower boundary
 * @maximum: higher boundary
 * @default_value: default value
 *
 * #GParamSpec subclass for fixed point based properties
 *
 * Since: 0.8
 *
 * Deprecated: Use #GParamSpecInt instead
 */
struct _ClutterParamSpecFixed
{
  /*< private >*/
  GParamSpec parent_instance;

  /*< public >*/
  CoglFixed minimum;
  CoglFixed maximum;
  CoglFixed default_value;
};

CLUTTER_DEPRECATED_IN_1_10
GType        clutter_param_fixed_get_type (void) G_GNUC_CONST;

CLUTTER_DEPRECATED_IN_1_10_FOR(g_value_set_int)
void         clutter_value_set_fixed      (GValue       *value,
                                           CoglFixed     fixed_);
CLUTTER_DEPRECATED_IN_1_10_FOR(g_value_get_int)
CoglFixed    clutter_value_get_fixed      (const GValue *value);

CLUTTER_DEPRECATED_IN_1_10_FOR(g_param_spec_int)
GParamSpec * clutter_param_spec_fixed     (const gchar  *name,
                                           const gchar  *nick,
                                           const gchar  *blurb,
                                           CoglFixed     minimum,
                                           CoglFixed     maximum,
                                           CoglFixed     default_value,
                                           GParamFlags   flags);

G_END_DECLS

#endif /* __CLUTTER_FIXED_H__ */
