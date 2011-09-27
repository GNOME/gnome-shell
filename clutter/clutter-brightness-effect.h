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

#ifndef __CLUTTER_BRIGHTNESS_EFFECT_H__
#define __CLUTTER_BRIGHTNESS_EFFECT_H__

#include <clutter/clutter-color.h>
#include <clutter/clutter-effect.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BRIGHTNESS_EFFECT          (clutter_brightness_effect_get_type ())
#define CLUTTER_BRIGHTNESS_EFFECT(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BRIGHTNESS_EFFECT, ClutterBrightnessEffect))
#define CLUTTER_IS_BRIGHTNESS_EFFECT(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BRIGHTNESS_EFFECT))

/**
 * ClutterBrightnessEffect:
 *
 * <structname>ClutterBrightnessEffect</structname> is an opaque structure
 * whose members cannot be directly accessed
 *
 * Since: 1.10
 */
typedef struct _ClutterBrightnessEffect         ClutterBrightnessEffect;
typedef struct _ClutterBrightnessEffectClass    ClutterBrightnessEffectClass;

GType clutter_brightness_effect_get_type (void) G_GNUC_CONST;

ClutterEffect *clutter_brightness_effect_new     (void);

void    clutter_brightness_effect_set_brightness (ClutterBrightnessEffect *effect,
                                                  const ClutterColor      *brightness);
void    clutter_brightness_effect_get_brightness (ClutterBrightnessEffect *effect,
                                                  ClutterColor            *brightness);

G_END_DECLS

#endif /* __CLUTTER_BRIGHTNESS_EFFECT_H__ */
