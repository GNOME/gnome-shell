/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 *
 * Based on MxDeformPageTurn, written by:
 *   Chris Lord <chris@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_PAGE_TURN_EFFECT_H__
#define __CLUTTER_PAGE_TURN_EFFECT_H__

#include <clutter/clutter-deform-effect.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_PAGE_TURN_EFFECT           (clutter_page_turn_effect_get_type ())
#define CLUTTER_PAGE_TURN_EFFECT(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_PAGE_TURN_EFFECT, ClutterPageTurnEffect))
#define CLUTTER_IS_PAGE_TURN_EFFECT(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_PAGE_TURN_EFFECT))

/**
 * ClutterPageTurnEffect:
 *
 * <structname>ClutterPageTurnEffect</structname> is an opaque structure
 * whose members can only be accessed using the provided API
 *
 * Since: 1.4
 */
typedef struct _ClutterPageTurnEffect           ClutterPageTurnEffect;
typedef struct _ClutterPageTurnEffectClass      ClutterPageTurnEffectClass;

CLUTTER_AVAILABLE_IN_1_4
GType clutter_page_turn_effect_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_4
ClutterEffect *clutter_page_turn_effect_new (gdouble period,
                                             gdouble angle,
                                             gfloat  radius);

CLUTTER_AVAILABLE_IN_1_4
void    clutter_page_turn_effect_set_period (ClutterPageTurnEffect *effect,
                                             gdouble                period);
CLUTTER_AVAILABLE_IN_1_4
gdouble clutter_page_turn_effect_get_period (ClutterPageTurnEffect *effect);
CLUTTER_AVAILABLE_IN_1_4
void    clutter_page_turn_effect_set_angle  (ClutterPageTurnEffect *effect,
                                             gdouble                angle);
CLUTTER_AVAILABLE_IN_1_4
gdouble clutter_page_turn_effect_get_angle  (ClutterPageTurnEffect *effect);
CLUTTER_AVAILABLE_IN_1_4
void    clutter_page_turn_effect_set_radius (ClutterPageTurnEffect *effect,
                                             gfloat                 radius);
CLUTTER_AVAILABLE_IN_1_4
gfloat  clutter_page_turn_effect_get_radius (ClutterPageTurnEffect *effect);

G_END_DECLS

#endif /* __CLUTTER_PAGE_TURN_EFFECT_H__ */
