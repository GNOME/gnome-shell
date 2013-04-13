/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Tomas Frydrych  <tf@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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

#include <string.h>
#include <math.h>

#include <glib.h>
#include "clutter-bezier.h"
#include "clutter-debug.h"

/****************************************************************************
 * ClutterBezier -- representation of a cubic bezier curve                  *
 * (private; a building block for the public bspline object)                *
 ****************************************************************************/

/*
 * Constants for sampling of the bezier. Float point.
 */
#define CBZ_T_SAMPLES 128.0
#define CBZ_T_STEP (1.0 / CBZ_T_SAMPLES)

/*
 * This is a private type representing a single cubic bezier
 */
struct _ClutterBezier
{
  /* bezier coefficients */
  gfloat ax;
  gfloat bx;
  gfloat cx;
  gfloat dx;

  gfloat ay;
  gfloat by;
  gfloat cy;
  gfloat dy;

  /* length of the bezier */
  gfloat length;
};

static gfloat
_clutter_bezier_t2x (const ClutterBezier *b,
		     gfloat               t)
{
  return b->ax * (1 - t) * (1 - t) * (1 - t) +
    b->bx * (1 - t) * (1 - t) * t +
    b->cx * (1 - t) * t * t +
    b->dx * t * t * t;
}

static gfloat
_clutter_bezier_t2y (const ClutterBezier *b,
		     gfloat               t)
{
  return b->ay * (1 - t) * (1 - t) * (1 - t) +
    b->by * (1 - t) * (1 - t) * t +
    b->cy * (1 - t) * t * t +
    b->dy * t * t * t;
}

/*
 * _clutter_bezier_new:
 *
 * Allocate the bezier
 *
 * Return value: The new bezier object.
 */
ClutterBezier *
_clutter_bezier_new (void)
{
  return g_slice_new0 (ClutterBezier);
}

/*
 * _clutter_bezier_free:
 * @b: The bezier to free
 *
 * Free the object
 */
void
_clutter_bezier_free (ClutterBezier *b)
{
  if (G_LIKELY (b))
    g_slice_free (ClutterBezier, b);
}

/*
 * _clutter_bezier_clone_and_move:
 * @b: A #ClutterBezier for cloning
 * @x: The x coordinates of the new end of the bezier
 * @y: The y coordinates of the new end of the bezier
 *
 * Clone the bezier and move th end-point, leaving both control
 * points in place.
 *
 * Return value: The new bezier object.
 */
ClutterBezier *
_clutter_bezier_clone_and_move (const ClutterBezier *b,
                                gfloat               x,
				gfloat               y)
{
  ClutterBezier * b2 = _clutter_bezier_new ();
  memcpy (b2, b, sizeof (ClutterBezier));

  b2->dx += x;
  b2->dy += y;

  return b2;
}

/*
 * _clutter_bezier_advance:
 * @b: A #ClutterBezier
 * @L: A relative length
 * @knot: The point whith the calculated position
 *
 * Advances along the bezier @b to relative length @L and returns the coordinances
 * in @knot
 */
void
_clutter_bezier_advance (const ClutterBezier *b,
                         gfloat               L,
                         ClutterPoint        *knot)
{
  gfloat t;
  t = L;

  knot->x = _clutter_bezier_t2x (b, t);
  knot->y = _clutter_bezier_t2y (b, t);

  CLUTTER_NOTE (MISC,
		"advancing to relative point {%d,%d} by moving a distance equals to: %f, with t: %f",
		knot->x, knot->y, L, t);
}

/*
 * _clutter_bezier_init:
 * @b: A #ClutterBezier
 * @x_0: x coordinates of the start point of the cubic bezier
 * @y_0: y coordinates of the start point of the cubic bezier
 * @x_1: x coordinates of the first control point
 * @y_1: y coordinates of the first control point
 * @x_2: x coordinates of the second control point
 * @y_2: y coordinates of the second control point
 * @x_3: x coordinates of the end point of the cubic bezier
 * @y_3: y coordinates of the end point of the cubic bezier
 *
 * Initialize the data of the bezier object @b.
 */
void
_clutter_bezier_init (ClutterBezier *b,
                      gfloat         x_0,
                      gfloat         y_0,
                      gfloat         x_1,
                      gfloat         y_1,
                      gfloat         x_2,
                      gfloat         y_2,
                      gfloat         x_3,
                      gfloat         y_3)
{
  gfloat t;
  gint i;

  gfloat xp;
  gfloat yp;

  CLUTTER_NOTE (MISC,
		"Initializing bezier at {{%f,%f},{%f,%f},{%f,%f},{%f,%f}}",
		x_0, y_0, x_1, y_1, x_2, y_2, x_3, y_3);

  b->ax = x_0;
  b->ay = y_0;
  b->bx = 3 * x_1;
  b->by = 3 * y_1;
  b->cx = 3 * x_2;
  b->cy = 3 * y_2;
  b->dx = x_3;
  b->dy = y_3;
  b->length = 0.0;

  CLUTTER_NOTE (MISC,
		"Coefficients {{%f,%f},{%f,%f},{%f,%f},{%f,%f}}",
		b->ax, b->ay, b->bx, b->by, b->cx, b->cy, b->dx, b->dy);

  xp = b->ax;
  yp = b->ay;
  for (t = CBZ_T_STEP, i = 1; i <= CBZ_T_SAMPLES; ++i, t += CBZ_T_STEP)
    {
      gfloat x = _clutter_bezier_t2x (b, t);
      gfloat y = _clutter_bezier_t2y (b, t);

      b->length += sqrtf ((y - yp)*(y - yp) + (x - xp)*(x - xp));

      xp = x;
      yp = y;
    }

  CLUTTER_NOTE (MISC, "length %f", b->length);
}

/*
 * _clutter_bezier_adjust:
 * @b: A #ClutterBezier object
 * @knot: The new position of the control point
 @ @indx: The index of the control point you want to move.
 *
 * Moves a control point at index @indx to location represented by @knot
 */
void
_clutter_bezier_adjust (ClutterBezier *b,
			ClutterPoint  *knot,
			guint          indx)
{
  gfloat x[4], y[4];

  g_assert (indx < 4);

  x[0] = b->ax;
  y[0] = b->ay;

  x[1] = b->bx;
  y[1] = b->by;

  x[2] = b->cx;
  y[2] = b->cy;

  x[3] = b->dx;
  y[3] = b->dy;

  x[indx] = knot->x;
  y[indx] = knot->y;

  _clutter_bezier_init (b, x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3]);
}

/*
 * _clutter_bezier_get_length:
 * @b: A #ClutterBezier
 *
 * Return value: Returns the length of the bezier
 */
gfloat
_clutter_bezier_get_length (const ClutterBezier *b)
{
  return b->length;
}
