/*
 * shell-code-view-effect.h
 *
 * Based on clutter-desaturate-effect.h.
 *
 * Copyright (C) 2010  Intel Corporation.
 * Copyright (C) 2018  Endless Mobile, Inc.
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
 * Authors:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 *   Cosimo Cecchi <cosimo@endlessm.com>
 */

#ifndef __SHELL_CODE_VIEW_EFFECT_H__
#define __SHELL_CODE_VIEW_EFFECT_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define SHELL_TYPE_CODE_VIEW_EFFECT (shell_code_view_effect_get_type ())
G_DECLARE_DERIVABLE_TYPE (ShellCodeViewEffect, shell_code_view_effect,
                          SHELL, CODE_VIEW_EFFECT, ClutterOffscreenEffect)

ClutterEffect *shell_code_view_effect_new        (void);

void shell_code_view_effect_set_gradient_stops (ShellCodeViewEffect *effect,
                                                gchar **gradient_colors,
                                                gfloat *gradient_points,
                                                gsize gradient_len);

G_END_DECLS

#endif /* __SHELL_CODE_VIEW_EFFECT_H__ */
