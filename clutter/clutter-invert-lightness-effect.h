/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010, 2011 Inclusive Design Research Centre, OCAD University.
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
 *   Joseph Scheuhammer <clown@alum.mit.edu>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_INVERT_LIGHTNESS_EFFECT_H__
#define __CLUTTER_INVERT_LIGHTNESS_EFFECT_H__

#include <clutter/clutter-effect.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_INVERT_LIGHTNESS_EFFECT        (clutter_invert_lightness_effect_get_type ())
#define CLUTTER_INVERT_LIGHTNESS_EFFECT(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_INVERT_LIGHTNESS_EFFECT, ClutterInvertLightnessEffect))
#define CLUTTER_IS_INVERT_LIGHTNESS_EFFECT(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_INVERT_LIGHTNESS_EFFECT))

/**
 * ClutterInvertLightnessEffect:
 *
 * <structname>ClutterInvertLightnessEffect</structname> is an opaque structure
 * whose members cannot be directly accessed
 *
 * Since: 1.10
 */
typedef struct _ClutterInvertLightnessEffect        ClutterInvertLightnessEffect;
typedef struct _ClutterInvertLightnessEffectClass   ClutterInvertLightnessEffectClass;

GType clutter_invert_lightness_effect_get_type (void) G_GNUC_CONST;

ClutterEffect *clutter_invert_lightness_effect_new (void);

G_END_DECLS

#endif /* __CLUTTER_INVERT_LIGHTNESS_EFFECT_H__ */
