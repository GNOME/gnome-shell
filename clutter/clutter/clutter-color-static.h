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
 */

#ifndef __CLUTTER_COLOR_STATIC_H__
#define __CLUTTER_COLOR_STATIC_H__

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#define __CLUTTER_COLOR_SYM(x)          (clutter_color_get_static (CLUTTER_COLOR_##x))

#define CLUTTER_COLOR_White             (__CLUTTER_COLOR_SYM (WHITE))
#define CLUTTER_COLOR_Black             (__CLUTTER_COLOR_SYM (BLACK))
#define CLUTTER_COLOR_Red               (__CLUTTER_COLOR_SYM (RED))
#define CLUTTER_COLOR_DarkRed           (__CLUTTER_COLOR_SYM (DARK_RED))
#define CLUTTER_COLOR_Green             (__CLUTTER_COLOR_SYM (GREEN))
#define CLUTTER_COLOR_DarkGreen         (__CLUTTER_COLOR_SYM (DARK_GREEN))
#define CLUTTER_COLOR_Blue              (__CLUTTER_COLOR_SYM (BLUE))
#define CLUTTER_COLOR_DarkBlue          (__CLUTTER_COLOR_SYM (DARK_BLUE))
#define CLUTTER_COLOR_Cyan              (__CLUTTER_COLOR_SYM (CYAN))
#define CLUTTER_COLOR_DarkCyan          (__CLUTTER_COLOR_SYM (DARK_CYAN))
#define CLUTTER_COLOR_Magenta           (__CLUTTER_COLOR_SYM (MAGENTA))
#define CLUTTER_COLOR_DarkMagenta       (__CLUTTER_COLOR_SYM (DARK_MAGENTA))
#define CLUTTER_COLOR_Yellow            (__CLUTTER_COLOR_SYM (YELLOW))
#define CLUTTER_COLOR_DarkYellow        (__CLUTTER_COLOR_SYM (DARK_YELLOW))
#define CLUTTER_COLOR_Gray              (__CLUTTER_COLOR_SYM (GRAY))
#define CLUTTER_COLOR_DarkGray          (__CLUTTER_COLOR_SYM (DARK_GRAY))
#define CLUTTER_COLOR_LightGray         (__CLUTTER_COLOR_SYM (LIGHT_GRAY))

#define CLUTTER_COLOR_Butter            (__CLUTTER_COLOR_SYM (BUTTER))
#define CLUTTER_COLOR_LightButter       (__CLUTTER_COLOR_SYM (BUTTER_LIGHT))
#define CLUTTER_COLOR_DarkButter        (__CLUTTER_COLOR_SYM (BUTTER_DARK))
#define CLUTTER_COLOR_Orange            (__CLUTTER_COLOR_SYM (ORANGE))
#define CLUTTER_COLOR_LightOrange       (__CLUTTER_COLOR_SYM (ORANGE_LIGHT))
#define CLUTTER_COLOR_DarkOrange        (__CLUTTER_COLOR_SYM (ORANGE_DARK))
#define CLUTTER_COLOR_Chocolate         (__CLUTTER_COLOR_SYM (CHOCOLATE))
#define CLUTTER_COLOR_LightChocolate    (__CLUTTER_COLOR_SYM (CHOCOLATE_LIGHT))
#define CLUTTER_COLOR_DarkChocolate     (__CLUTTER_COLOR_SYM (CHOCOLATE_DARK))
#define CLUTTER_COLOR_Chameleon         (__CLUTTER_COLOR_SYM (CHAMELEON))
#define CLUTTER_COLOR_LightChameleon    (__CLUTTER_COLOR_SYM (CHAMELEON_LIGHT))
#define CLUTTER_COLOR_DarkChameleon     (__CLUTTER_COLOR_SYM (CHAMELEON_DARK))
#define CLUTTER_COLOR_SkyBlue           (__CLUTTER_COLOR_SYM (SKY_BLUE))
#define CLUTTER_COLOR_LightSkyBlue      (__CLUTTER_COLOR_SYM (SKY_BLUE_LIGHT))
#define CLUTTER_COLOR_DarkSkyBlue       (__CLUTTER_COLOR_SYM (SKY_BLUE_DARK))
#define CLUTTER_COLOR_Plum              (__CLUTTER_COLOR_SYM (PLUM))
#define CLUTTER_COLOR_LightPlum         (__CLUTTER_COLOR_SYM (PLUM_LIGHT))
#define CLUTTER_COLOR_DarkPlum          (__CLUTTER_COLOR_SYM (PLUM_DARK))
#define CLUTTER_COLOR_ScarletRed        (__CLUTTER_COLOR_SYM (SCARLET_RED))
#define CLUTTER_COLOR_LightScarletRed   (__CLUTTER_COLOR_SYM (SCARLET_RED_LIGHT))
#define CLUTTER_COLOR_DarkScarletRed    (__CLUTTER_COLOR_SYM (SCARLET_RED_DARK))
#define CLUTTER_COLOR_Aluminium1        (__CLUTTER_COLOR_SYM (ALUMINIUM_1))
#define CLUTTER_COLOR_Aluminium2        (__CLUTTER_COLOR_SYM (ALUMINIUM_2))
#define CLUTTER_COLOR_Aluminium3        (__CLUTTER_COLOR_SYM (ALUMINIUM_3))
#define CLUTTER_COLOR_Aluminium4        (__CLUTTER_COLOR_SYM (ALUMINIUM_4))
#define CLUTTER_COLOR_Aluminium5        (__CLUTTER_COLOR_SYM (ALUMINIUM_5))
#define CLUTTER_COLOR_Aluminium6        (__CLUTTER_COLOR_SYM (ALUMINIUM_6))

#define CLUTTER_COLOR_Transparent       (__CLUTTER_COLOR_SYM (TRANSPARENT))

#endif /* __CLUTTER_COLOR_STATIC_H__ */
