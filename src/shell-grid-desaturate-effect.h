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
 */

#ifndef __SHELL_GRID_DESATURATE_EFFECT_H__
#define __SHELL_GRID_DESATURATE_EFFECT_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define SHELL_TYPE_GRID_DESATURATE_EFFECT (shell_grid_desaturate_effect_get_type ())
G_DECLARE_FINAL_TYPE (ShellGridDesaturateEffect, shell_grid_desaturate_effect,
                      SHELL, GRID_DESATURATE_EFFECT, ClutterOffscreenEffect)

ClutterEffect *    shell_grid_desaturate_effect_new        (gdouble                  factor);

void               shell_grid_desaturate_effect_set_factor (ShellGridDesaturateEffect *effect,
                                                            gdouble                    factor);
gdouble            shell_grid_desaturate_effect_get_factor (ShellGridDesaturateEffect *effect);

void               shell_grid_desaturate_effect_set_unshaded_rect (ShellGridDesaturateEffect *effect,
                                                                   graphene_rect_t           *rect);

const graphene_rect_t *
                   shell_grid_desaturate_effect_get_unshaded_rect (ShellGridDesaturateEffect *effect);

G_END_DECLS

#endif /* __SHELL_GRID_DESATURATE_EFFECT_H__ */
