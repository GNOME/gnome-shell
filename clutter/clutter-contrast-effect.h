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

#ifndef __CLUTTER_CONTRAST_EFFECT_H__
#define __CLUTTER_CONTRAST_EFFECT_H__

#include <clutter/clutter-color.h>
#include <clutter/clutter-effect.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_CONTRAST_EFFECT        (clutter_contrast_effect_get_type ())
#define CLUTTER_CONTRAST_EFFECT(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_CONTRAST_EFFECT, ClutterContrastEffect))
#define CLUTTER_IS_CONTRAST_EFFECT(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_CONTRAST_EFFECT))

/**
 * ClutterContrastEffect:
 *
 * <structname>ClutterContrastEffect</structname> is an opaque structure
 * whose members cannot be directly accessed
 *
 * Since: 1.10
 */
typedef struct _ClutterContrastEffect       ClutterContrastEffect;
typedef struct _ClutterContrastEffectClass  ClutterContrastEffectClass;

GType clutter_contrast_effect_get_type (void) G_GNUC_CONST;

ClutterEffect *clutter_contrast_effect_new       (void);

void        clutter_contrast_effect_set_contrast (ClutterContrastEffect *effect,
                                                  const ClutterColor    *contrast);
void        clutter_contrast_effect_get_contrast (ClutterContrastEffect *effect,
                                                  ClutterColor          *contrast);

G_END_DECLS

#endif /* __CLUTTER_CONTRAST_EFFECT_H__ */
