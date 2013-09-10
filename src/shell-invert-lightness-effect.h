/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright © 2010-2012 Inclusive Design Research Centre, OCAD University.
 *
 * This program is free software; you can redistribute it and/or
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
#ifndef __SHELL_INVERT_LIGHTNESS_EFFECT_H__
#define __SHELL_INVERT_LIGHTNESS_EFFECT_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define SHELL_TYPE_INVERT_LIGHTNESS_EFFECT        (shell_invert_lightness_effect_get_type ())
#define SHELL_INVERT_LIGHTNESS_EFFECT(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_INVERT_LIGHTNESS_EFFECT, ShellInvertLightnessEffect))
#define SHELL_IS_INVERT_LIGHTNESS_EFFECT(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_INVERT_LIGHTNESS_EFFECT))

typedef struct _ShellInvertLightnessEffect        ShellInvertLightnessEffect;
typedef struct _ShellInvertLightnessEffectClass   ShellInvertLightnessEffectClass;

GType shell_invert_lightness_effect_get_type (void) G_GNUC_CONST;

ClutterEffect *shell_invert_lightness_effect_new (void);

G_END_DECLS

#endif /* __SHELL_INVERT_LIGHTNESS_EFFECT_H__ */
