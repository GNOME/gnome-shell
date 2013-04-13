/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Tomas Frydrych  <tf@openedhand.com>
 *
 * Copyright (C) 2006, 2007 OpenedHand
 * Copyright (C) 2013 Erick Pérez Castellanos <erick.red@gmail.com>
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

#ifndef __CLUTTER_BEZIER_H__
#define __CLUTTER_BEZIER_H__

#include <glib.h>
#include "clutter-types.h"

G_BEGIN_DECLS

typedef struct _ClutterBezier ClutterBezier;

ClutterBezier *_clutter_bezier_new            ();

void           _clutter_bezier_free           (ClutterBezier       *b);

ClutterBezier *_clutter_bezier_clone_and_move (const ClutterBezier *b,
                                               gfloat               x,
                                               gfloat               y);

void           _clutter_bezier_advance        (const ClutterBezier *b,
                                               gfloat               L,
                                               ClutterPoint        *knot);

void           _clutter_bezier_init           (ClutterBezier       *b,
                                               gfloat               x_0,
                                               gfloat               y_0,
                                               gfloat               x_1,
                                               gfloat               y_1,
                                               gfloat               x_2,
                                               gfloat               y_2,
                                               gfloat               x_3,
                                               gfloat               y_3);

void           _clutter_bezier_adjust         (ClutterBezier       *b,
                                               ClutterPoint        *knot,
                                               guint                indx);

gfloat         _clutter_bezier_get_length     (const ClutterBezier *b);

G_END_DECLS

#endif /* __CLUTTER_BEZIER_H__ */
