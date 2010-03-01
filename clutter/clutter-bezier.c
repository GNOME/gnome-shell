/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Tomas Frydrych  <tf@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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

#include <glib.h>
#include <string.h>
#include "clutter-bezier.h"
#include "clutter-debug.h"

/*
 * We have some experimental code here to allow for constant velocity
 * movement of actors along the bezier path, this macro enables it.
 */
#undef CBZ_L2T_INTERPOLATION

/****************************************************************************
 * ClutterBezier -- represenation of a cubic bezier curve                   *
 * (private; a building block for the public bspline object)                *
 ****************************************************************************/

/*
 * The t parameter of the bezier is from interval <0,1>, so we can use
 * 14.18 format and special multiplication functions that preserve
 * more of the least significant bits but would overflow if the value
 * is > 1
 */
#define CBZ_T_Q 18
#define CBZ_T_ONE (1 << CBZ_T_Q)
#define CBZ_T_MUL(x,y) ((((x) >> 3) * ((y) >> 3)) >> 12)
#define CBZ_T_POW2(x) CBZ_T_MUL (x, x)
#define CBZ_T_POW3(x) CBZ_T_MUL (CBZ_T_POW2 (x), x)
#define CBZ_T_DIV(x,y) ((((x) << 9)/(y)) << 9)

/*
 * Constants for sampling of the bezier
 */
#define CBZ_T_SAMPLES 128
#define CBZ_T_STEP (CBZ_T_ONE / CBZ_T_SAMPLES)
#define CBZ_L_STEP (CBZ_T_ONE / CBZ_T_SAMPLES)

typedef gint32 _FixedT;

/*
 * This is a private type representing a single cubic bezier
 */
struct _ClutterBezier
{
  /*
   * bezier coefficients -- these are calculated using multiplication and
   * addition from integer input, so these are also integers
   */
  gint ax;
  gint bx;
  gint cx;
  gint dx;

  gint ay;
  gint by;
  gint cy;
  gint dy;
    
  /* length of the bezier */
  guint length;

#ifdef CBZ_L2T_INTERPOLATION
  /*
   * coefficients for the L -> t bezier; these are calculated from fixed
   * point input, and more specifically numbers that have been normalised
   * to fit <0,1>, so these are also fixed point, and we can used the
   * _FixedT type here.
   */
  _FixedT La;
  _FixedT Lb;
  _FixedT Lc;
  /*  _FixedT Ld; == 0 */
#endif
};

ClutterBezier *
_clutter_bezier_new ()
{
  return g_slice_new0 (ClutterBezier);
}

void
_clutter_bezier_free (ClutterBezier * b)
{
  if (G_LIKELY (b))
    {
      g_slice_free (ClutterBezier, b);
    }
}

ClutterBezier *
_clutter_bezier_clone_and_move (const ClutterBezier *b, gint x, gint y)
{
  ClutterBezier * b2 = _clutter_bezier_new ();
  memcpy (b2, b, sizeof (ClutterBezier));

  b2->dx += x;
  b2->dy += y;

  return b2;
}

#ifdef CBZ_L2T_INTERPOLATION
/*
 * L is relative advance along the bezier curve from interval <0,1>
 */
static _FixedT
_clutter_bezier_L2t (const ClutterBezier *b, _FixedT L)
{
  _FixedT t = CBZ_T_MUL (b->La, CBZ_T_POW3(L))
    +  CBZ_T_MUL (b->Lb, CBZ_T_POW2(L))
    +  CBZ_T_MUL (b->Lc, L);
  
  if (t > CBZ_T_ONE)
    t = CBZ_T_ONE;
  else if (t < 0)
    t = 0;
  
  return t;
}
#endif

static gint
_clutter_bezier_t2x (const ClutterBezier * b, _FixedT t)
{
  /*
   * NB -- the int coefficients can be at most 8192 for the multiplication
   * to work in this fashion due to the limits of the 14.18 fixed.
   */
  return ((b->ax*CBZ_T_POW3(t) + b->bx*CBZ_T_POW2(t) + b->cx*t) >> CBZ_T_Q)
    + b->dx;
}

gint
_clutter_bezier_t2y (const ClutterBezier * b, _FixedT t)
{
  /*
   * NB -- the int coefficients can be at most 8192 for the multiplication
   * to work in this fashion due to the limits of the 14.18 fixed.
   */
  return ((b->ay*CBZ_T_POW3(t) + b->by*CBZ_T_POW2(t) + b->cy*t) >> CBZ_T_Q)
    + b->dy;
}

/*
 * Advances along the bezier to relative length L and returns the coordinances
 * in knot
 */
void
_clutter_bezier_advance (const ClutterBezier *b, gint L, ClutterKnot * knot)
{
#ifdef CBZ_L2T_INTERPOLATION
  _FixedT t = clutter_bezier_L2t (b, L);
#else
  _FixedT t = L;
#endif
  
  knot->x = _clutter_bezier_t2x (b, t);
  knot->y = _clutter_bezier_t2y (b, t);
  
  CLUTTER_NOTE (BEHAVIOUR, "advancing to relative pt %f: t %f, {%d,%d}",
                (double) L / (double) CBZ_T_ONE,
                (double) t / (double) CBZ_T_ONE,
                knot->x, knot->y);
}

void
_clutter_bezier_init (ClutterBezier *b,
		     gint x_0, gint y_0,
		     gint x_1, gint y_1,
		     gint x_2, gint y_2,
		     gint x_3, gint y_3)
{
  _FixedT t;
  int i;
  int xp = x_0;
  int yp = y_0;
  _FixedT length [CBZ_T_SAMPLES + 1];

#ifdef CBZ_L2T_INTERPOLATION
  int j, k;
  _FixedT L;
  _FixedT t_equalized [CBZ_T_SAMPLES + 1];
#endif

#if 0
  g_debug ("Initializing bezier at {{%d,%d},{%d,%d},{%d,%d},{%d,%d}}",
           x0, y0, x1, y1, x2, y2, x3, y3);
#endif
  
  b->dx = x_0;
  b->dy = y_0;

  b->cx = 3 * (x_1 - x_0);
  b->cy = 3 * (y_1 - y_0);

  b->bx = 3 * (x_2 - x_1) - b->cx;
  b->by = 3 * (y_2 - y_1) - b->cy;

  b->ax = x_3 - 3 * x_2 + 3 * x_1 - x_0;
  b->ay = y_3 - 3 * y_2 + 3 * y_1 - y_0;

#if 0
  g_debug ("Cooeficients {{%d,%d},{%d,%d},{%d,%d},{%d,%d}}",
           b->ax, b->ay, b->bx, b->by, b->cx, b->cy, b->dx, b->dy);
#endif
  
  /*
   * Because of the way we do the multiplication in bezeir_t2x,y
   * these coefficients need to be at most 0x1fff; this should be the case,
   * I think, but have added this warning to catch any problems -- if it
   * triggers, we need to change those two functions a bit.
   */
  if (b->ax > 0x1fff || b->bx > 0x1fff || b->cx > 0x1fff)
    g_warning ("Calculated coefficents will result in multiplication "
               "overflow in clutter_bezier_t2x and clutter_bezier_t2y.");

  /*
   * Sample the bezier with CBZ_T_SAMPLES and calculate length at
   * each point.
   *
   * We are working with integers here, so we use the fast sqrti function.
   */
  length[0] = 0;
    
  for (t = CBZ_T_STEP, i = 1; i <= CBZ_T_SAMPLES; ++i, t += CBZ_T_STEP)
    {
      int x = _clutter_bezier_t2x (b, t);
      int y = _clutter_bezier_t2y (b, t);
	
      guint l = cogl_sqrti ((y - yp)*(y - yp) + (x - xp)*(x - xp));

      l += length[i-1];

      length[i] = l;

      xp = x;
      yp = y;
    }

  b->length = length[CBZ_T_SAMPLES];

#if 0
  g_debug ("length %d", b->length);
#endif
  
#ifdef CBZ_L2T_INTERPOLATION
  /*
   * Now normalize the length values, converting them into _FixedT
   */
  for (i = 0; i <= CBZ_T_SAMPLES; ++i)
    {
      length[i] = (length[i] << CBZ_T_Q) / b->length;
    }

  /*
   * Now generate a L -> t table such that the L will equidistant
   * over <0,1>
   */
  t_equalized[0] = 0;
    
  for (i = 1, j = 1, L = CBZ_L_STEP; i < CBZ_T_SAMPLES; ++i, L += CBZ_L_STEP)
    {
      _FixedT l1, l2;
      _FixedT d1, d2, d;
      _FixedT t1, t2;
	
      /* find the band for our L */
      for (k = j; k < CBZ_T_SAMPLES; ++k)
	{
          if (L < length[k])
            break;
	}

      /*
       * Now we know that L is from (length[k-1],length[k]>
       * We remember k-1 in order not to have to iterate over the
       * whole length array in the next iteration of the main loop
       */
      j = k - 1;

      /*
       * Now interpolate equlised t as a weighted average
       */
      l1 = length[k-1];
      l2 = length[k];
      d1 = l2 - L;
      d2 = L - l1;
      d = l2 - l1;
      t1 = (k - 1) * CBZ_T_STEP;
      t2 = k * CBZ_T_STEP;
	
      t_equalized[i] = (t1*d1 + t2*d2)/d;

      if (t_equalized[i] < t_equalized[i-1])
        g_debug ("wrong t: L %f, l1 %f, l2 %f, t1 %f, t2 %f",
                 (double) (L)/(double)CBZ_T_ONE,
                 (double) (l1)/(double)CBZ_T_ONE,
                 (double) (l2)/(double)CBZ_T_ONE,
                 (double) (t1)/(double)CBZ_T_ONE,                 
                 (double) (t2)/(double)CBZ_T_ONE);
      
    }

  t_equalized[CBZ_T_SAMPLES] = CBZ_T_ONE;

  /* We now fit a bezier -- at this stage, do a single fit through our values
   * at 0, 1/3, 2/3 and 1
   *
   * FIXME -- do we need to  use a better fitting approach to choose the best
   * beziere. The actual curve we acquire this way is not too bad shapwise,
   * but (probably due to rounding errors) the resulting curve no longer
   * satisfies the necessary condition that for L2 > L1, t2 > t1, which 
   * causes oscilation.
   */

#if 0
  /*
   * These are the control points we use to calculate the curve coefficients
   * for bezier t(L); these are not needed directly, but are implied in the
   * calculations below.
   *
   * (p0 is 0,0, and p3 is 1,1)
   */
  p1 = (18 * t_equalized[CBZ_T_SAMPLES/3] -
        9 * t_equalized[2*CBZ_T_SAMPLES/3] +
        2 << CBZ_T_Q) / 6;

  p2 = (18 * t_equalized[2*CBZ_T_SAMPLES/3] -
        9 * t_equalized[CBZ_T_SAMPLES/3] -
        (5 << CBZ_T_Q)) / 6;
#endif
    
  b->Lc = (18 * t_equalized[CBZ_T_SAMPLES/3] -
           9 * t_equalized[2*CBZ_T_SAMPLES/3] +
           (2 << CBZ_T_Q)) >> 1;
    
  b->Lb = (36 * t_equalized[2*CBZ_T_SAMPLES/3] -
           45 * t_equalized[CBZ_T_SAMPLES/3] -
           (9 << CBZ_T_Q)) >> 1;

  b->La = ((27 * (t_equalized[CBZ_T_SAMPLES/3] -
                 t_equalized[2*CBZ_T_SAMPLES/3]) +
            (7 << CBZ_T_Q)) >> 1) + CBZ_T_ONE;

  g_debug ("t(1/3) %f, t(2/3) %f",
           (double)t_equalized[CBZ_T_SAMPLES/3]/(double)CBZ_T_ONE,
           (double)t_equalized[2*CBZ_T_SAMPLES/3]/(double)CBZ_T_ONE);

  g_debug ("L -> t coefficients: %f, %f, %f",
           (double)b->La/(double)CBZ_T_ONE,
           (double)b->Lb/(double)CBZ_T_ONE,
           (double)b->Lc/(double)CBZ_T_ONE);


  /*
   * For debugging, you can load these values into a spreadsheet and graph
   * them to see how well the approximation matches the data
   */
  for (i = 0; i < CBZ_T_SAMPLES; ++i)
    {
      g_print ("%f, %f, %f\n",
               (double)(i*CBZ_T_STEP)/(double)CBZ_T_ONE,
               (double)(t_equalized[i])/(double)CBZ_T_ONE,
               (double)(clutter_bezier_L2t(b,i*CBZ_T_STEP))/(double)CBZ_T_ONE);
    }
#endif
}

/*
 * Moves a control point at indx to location represented by knot
 */
void
_clutter_bezier_adjust (ClutterBezier * b, ClutterKnot * knot, guint indx)
{
  guint x[4], y[4];

  g_assert (indx < 4);
    
  x[0] = b->dx;
  y[0] = b->dy;

  x[1] = b->cx / 3 + x[0];
  y[1] = b->cy / 3 + y[0];

  x[2] = b->bx / 3 + b->cx + x[1];
  y[2] = b->by / 3 + b->cy + y[1];

  x[3] = b->ax + x[0] + b->cx + b->bx;
  y[3] = b->ay + y[0] + b->cy + b->by;

  x[indx] = knot->x;
  y[indx] = knot->y;

  _clutter_bezier_init (b, x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3]);
}

guint
_clutter_bezier_get_length (const ClutterBezier *b)
{
  return b->length;
}
