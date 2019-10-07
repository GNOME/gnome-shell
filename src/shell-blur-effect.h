/* shell-blur-effect.h
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define SHELL_TYPE_BLUR_EFFECT (shell_blur_effect_get_type())

G_DECLARE_FINAL_TYPE (ShellBlurEffect, shell_blur_effect, SHELL, BLUR_EFFECT, ClutterShaderEffect)

ShellBlurEffect *shell_blur_effect_new (void);

int shell_blur_effect_get_blur_radius (ShellBlurEffect *self);
void shell_blur_effect_set_blur_radius (ShellBlurEffect *self,
                                        int              radius);

gboolean shell_blur_effect_get_vertical (ShellBlurEffect *self);
void shell_blur_effect_set_vertical (ShellBlurEffect *self,
                                     gboolean         vertical);

G_END_DECLS
