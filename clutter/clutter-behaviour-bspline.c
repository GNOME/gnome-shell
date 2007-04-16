/* -*- mode:C; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-behaviour-bspline
 * @short_description: A behaviour class interpolating actors along a path
 * defined by bezier spline.
 *
 * #ClutterBehaviourBspline interpolates actors along a defined bsplien path.
 *
 * A bezier spline is a set of cubic bezier curves defined by a sequence of
 * control points given when creating a new #ClutterBehaviourBspline instance.
 *
 * Additional bezier curves can be added to the end of the bspline using
 * clutter_behaviour_bspline_append() family of functions, control points can
 * be moved using clutter_behaviour_bspline_adjust(). The bspline can be split
 * into two with clutter_behaviour_bspline_split(), and bsplines can be
 * concatenated using clutter_behaviour_bspline_join().
 *
 * Each time the behaviour reaches a point on the path, the "knot-reached"
 * signal is emitted.
 *
 * Since: 0.4
 */

#include "clutter-fixed.h"
#include "clutter-marshal.h"
#include "clutter-behaviour-bspline.h"
#include <stdlib.h>
#include <memory.h>

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
 * The t parameter of the bezier is from interval <0,1>, so we use
 * 14.18 fixed format to improve precission and simplify POW3 calculation.
 */
#define CBZ_T_Q 18
#define CBZ_T_ONE (1 << CBZ_T_Q)
#define CBZ_T_POW2(x) ((x >> 9) * (x >> 9))
#define CBZ_T_POW3(x) ((x >> 12) * (x >> 12) * (x >> 12))
#define CBZ_T_MUL(x,y) ((x >> 9) * (y >> 9))
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
typedef struct _ClutterBezier
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
} ClutterBezier;

static ClutterBezier *
clutter_bezier_new ()
{
  return g_slice_new0 (ClutterBezier);
}

static void
clutter_bezier_free (ClutterBezier * b)
{
  if (G_LIKELY (b))
    {
      g_slice_free (ClutterBezier, b);
    }
}

static ClutterBezier *
clutter_bezier_clone_and_move (ClutterBezier *b, gint x, gint y)
{
  ClutterBezier * b2 = clutter_bezier_new ();
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
clutter_bezier_L2t (ClutterBezier *b, _FixedT L)
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
clutter_bezier_t2x (ClutterBezier * b, _FixedT t)
{
  /*
   * NB -- the int coefficients can be at most 8192 for the multiplication
   * to work in this fashion due to the limits of the 14.18 fixed.
   */
  return ((b->ax*CBZ_T_POW3(t) + b->bx*CBZ_T_POW2(t) + b->cx*t) >> CBZ_T_Q)
    + b->dx;
}

static gint
clutter_bezier_t2y (ClutterBezier * b, _FixedT t)
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
static void
clutter_bezier_advance (ClutterBezier *b, _FixedT L, ClutterKnot * knot)
{
#ifdef CBZ_L2T_INTERPOLATION
  _FixedT t = clutter_bezier_L2t (b, L);
#else
  _FixedT t = L;
#endif
  
  knot->x = clutter_bezier_t2x (b, t);
  knot->y = clutter_bezier_t2y (b, t);
  
#if 0
  g_debug ("advancing to relative pt %f: t %f, {%d,%d}",
           (double)L/(double)CBZ_T_ONE,
           (double)t/(double)CBZ_T_ONE,
           knot->x, knot->y);
#endif
}

static void
clutter_bezier_init (ClutterBezier * b,
		     gint x0, gint y0,
		     gint x1, gint y1,
		     gint x2, gint y2,
		     gint x3, gint y3)
{
  _FixedT t;
  int i;
  int xp = x0;
  int yp = y0;
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
  
  b->dx = x0;
  b->dy = y0;

  b->cx = 3 * (x1 - x0);
  b->cy = 3 * (y1 - y0);

  b->bx = 3 * (x2 - x1) - b->cx;
  b->by = 3 * (y2 - y1) - b->cy;

  b->ax = x3 - 3 * x2 + 3 * x1 - x0;
  b->ay = y3 - 3 * y2 + 3 * y1 - y0;

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
      int x = clutter_bezier_t2x (b, t);
      int y = clutter_bezier_t2y (b, t);
	
      guint l = clutter_sqrti ((y - yp)*(y - yp) + (x - xp)*(x - xp));

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
static void
clutter_bezier_adjust (ClutterBezier * b, ClutterKnot * knot, guint indx)
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

  clutter_bezier_init (b, x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3]);
}


/****************************************************************************
 *                                                                          *
 * ClutterBehaviourBspline                                                  *
 *                                                                          *
 ****************************************************************************/

G_DEFINE_TYPE (ClutterBehaviourBspline, 
               clutter_behaviour_bspline,
	       CLUTTER_TYPE_BEHAVIOUR);

#define CLUTTER_BEHAVIOUR_BSPLINE_GET_PRIVATE(obj)    \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj),    \
               CLUTTER_TYPE_BEHAVIOUR_BSPLINE,        \
               ClutterBehaviourBsplinePrivate))

enum
  {
    KNOT_REACHED,

    LAST_SIGNAL
  };

static guint bspline_signals[LAST_SIGNAL] = { 0, };

struct _ClutterBehaviourBsplinePrivate
{
  /*
   * The individual bezier curves that make up this bspline
   */
  GArray * splines;

  /*
   * The length of the bspline
   */
  guint length;

  /*
   * Bspline offsets (these allow us to move the bspline without having to
   * mess about with the individual beziers).
   *
   * NB: this is not the actual origin, but an adjustment to the origin of
   * the first bezier; it defaults to 0 unless the user explicitely changes
   * the bspline offset.
   */
  gint x;
  gint y;

  /*
   * A temporary stack of control points used by the append methods
   */
  GArray * point_stack;
};

static void 
clutter_behaviour_bspline_finalize (GObject *object)
{
  gint i;
  ClutterBehaviourBspline *self = CLUTTER_BEHAVIOUR_BSPLINE (object);

  for (i = 0; i < self->priv->splines->len; ++i)
    clutter_bezier_free (g_array_index (self->priv->splines,ClutterBezier*,i));
    
  g_array_free (self->priv->splines, TRUE);

  for (i = 0; i < self->priv->point_stack->len; ++i)
    clutter_knot_free (g_array_index (self->priv->point_stack,ClutterKnot*,i));
    
  g_array_free (self->priv->point_stack, TRUE);
  
  G_OBJECT_CLASS (clutter_behaviour_bspline_parent_class)->finalize (object);
}

static void
actor_apply_knot_foreach (ClutterBehaviour *behaviour,
                          ClutterActor     *actor,
                          gpointer          data)
{
  ClutterKnot *knot = data;
  clutter_actor_set_position (actor, knot->x, knot->y);
}

/*
 * Advances to a point that is at distance 'to' along the spline;
 *
 * returns FALSE if the length is beyond the end of the bspline.
 */
static gboolean
clutter_behaviour_bspline_advance (ClutterBehaviourBspline * bs,
				   guint                     to)
{
  gint          i;
  guint         length = 0;
  ClutterKnot   knot;
    
  if (to > bs->priv->length)
    return FALSE;

  for (i = 0; i < bs->priv->splines->len; ++i)
    {
      ClutterBezier * b = g_array_index (bs->priv->splines,ClutterBezier*,i);
	
      if (length + b->length >= to)
	{
          _FixedT L = ((to - length) << CBZ_T_Q) / b->length;
	    
          clutter_bezier_advance (b, L, &knot);

          knot.x += bs->priv->x;
          knot.y += bs->priv->y;
#if 0
          g_debug ("advancing to length %d: {%d,%d}",
                   to, knot.x, knot.y);
#endif
          clutter_behaviour_actors_foreach (CLUTTER_BEHAVIOUR (bs), 
                                            actor_apply_knot_foreach,
                                            &knot);

          g_signal_emit (bs, bspline_signals[KNOT_REACHED], 0, &knot);
	    
          return TRUE;
	}
	
      length += b->length;
    }

  /* should not be reached */
  return FALSE;
}

static void
clutter_behaviour_bspline_alpha_notify (ClutterBehaviour * behave,
					guint32            alpha)
{
  ClutterBehaviourBspline * bs = CLUTTER_BEHAVIOUR_BSPLINE (behave);
  gint to = (alpha * bs->priv->length) / CLUTTER_ALPHA_MAX_ALPHA;
  
  clutter_behaviour_bspline_advance (bs, to);
}

static void
clutter_behaviour_bspline_class_init (ClutterBehaviourBsplineClass *klass)
{
  GObjectClass          * object_class = G_OBJECT_CLASS (klass);
  ClutterBehaviourClass * behave_class = CLUTTER_BEHAVIOUR_CLASS (klass);

  object_class->finalize = clutter_behaviour_bspline_finalize;

  behave_class->alpha_notify = clutter_behaviour_bspline_alpha_notify;

  /**
   * ClutterBehaviourBspline::knot-reached:
   * @pathb: the object which received the signal
   * @knot: the #ClutterKnot reached
   *
   * This signal is emitted at the end of each frame.
   *
   * Since: 0.2
   */
  bspline_signals[KNOT_REACHED] =
    g_signal_new ("knot-reached",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterBehaviourBsplineClass, knot_reached),
                  NULL, NULL,
                  clutter_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_KNOT);
  
  g_type_class_add_private (klass, sizeof (ClutterBehaviourBsplinePrivate));
}

static void
clutter_behaviour_bspline_init (ClutterBehaviourBspline * self)
{
  ClutterBehaviourBsplinePrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_BSPLINE_GET_PRIVATE (self);
}

/**
 * clutter_behaviour_bspline_new:
 * @alpha: a #ClutterAlpha, or %NULL
 * @knots: a list of #ClutterKnots representing individual control points
 * @n_knots: the number of control points
 *
 * Creates a new bezier spline behaviour. You can use this behaviour to drive
 * actors along the bezier spline, described by the @knots control points.
 *
 * Bspline is defined by 3n + 1 points, n >=1; any trailing points passed
 * into this function are stored internally and used during any subsequent
 * clutter_behaviour_bspline_append() operations.
 *
 * Return value: a #ClutterBehaviour
 *
 * Since: 0.4
 */
ClutterBehaviour *
clutter_behaviour_bspline_new (ClutterAlpha      *alpha,
			       const ClutterKnot *knots,
			       guint              n_knots)
{
  ClutterBehaviourBspline *bs;
  gint i;

  g_return_val_if_fail (alpha == NULL || CLUTTER_IS_ALPHA (alpha), NULL);
     
  bs = g_object_new (CLUTTER_TYPE_BEHAVIOUR_BSPLINE, 
                     "alpha", alpha,
                     NULL);

  bs->priv->splines = g_array_new (FALSE, FALSE, sizeof (ClutterBezier *));
  bs->priv->point_stack = g_array_new (FALSE, FALSE, sizeof (ClutterKnot *));
  bs->priv->length  = 0;
    
  for (i = 0; i < n_knots; ++i)
    clutter_behaviour_bspline_append_knot (bs, &knots[i]);

  return CLUTTER_BEHAVIOUR (bs);
}

/*
 * Appends a single spline; knots points to 4 knots if this is first
 * bezier in the spline, 3 subsequently
 */
static void
clutter_behaviour_bspline_append_spline (ClutterBehaviourBspline  * bs,
                                         const ClutterKnot       ** knots)
{
  ClutterBehaviourBsplinePrivate *priv;
  gint            i;
  ClutterBezier * b;
  ClutterKnot     knot0;
  
  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_BSPLINE (bs));
  priv = bs->priv;

  if (priv->splines->len)
    {
      /* Get the first point from the last curve */
      ClutterBezier *b_last;
      
      b_last = g_array_index (priv->splines,
                              ClutterBezier *,
                              priv->splines->len - 1);

      knot0.x = b_last->ax + b_last->bx + b_last->cx + b_last->dx;
      knot0.y = b_last->ay + b_last->by + b_last->cy + b_last->dy;

      i = 0;
    }
  else
    {
      knot0.x = knots[0]->x;
      knot0.y = knots[0]->y;
      i = 1;
    }
  
  b = clutter_bezier_new ();
  clutter_bezier_init (b,
                       knot0.x,
                       knot0.y,
                       knots[i]->x, knots[i]->y,
                       knots[i + 1]->x, knots[i + 1]->y,
                       knots[i + 2]->x, knots[i + 2]->y);

  priv->splines = g_array_append_val (priv->splines, b);

  priv->length += b->length;
}

/**
 * clutter_behaviour_bspline_append_knot:
 * @bs:      a #ClutterBehaviourBspline
 * @knot:    a #ClutterKnot control point to append.
 *
 * Appends a #ClutterKnot control point to the bezier spline bs. Note, that
 * since a bezier is defined by 4 control points, the point gets stored in
 * a temporary chache, and only when there are enough control points to
 * create a new bezier curve will the bspline extended.
 *
 * Since: 0.4
 */
void
clutter_behaviour_bspline_append_knot (ClutterBehaviourBspline * bs,
                                       const ClutterKnot       * knot)
{
  ClutterBehaviourBsplinePrivate *priv;
  ClutterKnot * k = clutter_knot_copy (knot);
  guint needed = 3;
  guint i;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_BSPLINE (bs));
  priv = bs->priv;
  
  g_array_append_val (priv->point_stack, k);

  if (priv->splines->len == 0)
    needed = 4;

  if (priv->point_stack->len == needed)
    {
      clutter_behaviour_bspline_append_spline (bs,
                                               (const ClutterKnot**) priv->point_stack->data);

      for (i = 0; i < needed; ++i)
        {
          clutter_knot_free (g_array_index (priv->point_stack,
                                            ClutterKnot *,
                                            i));
        }
        
      g_array_set_size (priv->point_stack, 0);
    }
}

static void
clutter_behaviour_bspline_append_knots_valist (ClutterBehaviourBspline *bs,
                                               const ClutterKnot   *first_knot,
                                               va_list              args)
{
  const ClutterKnot * knot;

  knot = first_knot;
  while (knot)
    {
      clutter_behaviour_bspline_append_knot (bs, knot);
      knot = va_arg (args, ClutterKnot*);
    }
}

/**
 * clutter_behaviour_bspline_append:
 * @bs: a #ClutterBehaviourBspline
 * @first_knot: first #ClutterKnot
 * @VarArgs: a NULL-terminated array of #ClutterKnot control points.
 *
 * Appends a bezier spline defined by the last control point of bezier spline
 * bs and the array of #ClutterKnot control points to the orginal bezier spline
 * bs.
 *
 * Since: 0.4
 */
void
clutter_behaviour_bspline_append (ClutterBehaviourBspline * bs,
				  const ClutterKnot       * first_knot,
                                  ...)
{
  va_list args;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_BSPLINE (bs));
  g_return_if_fail (first_knot != NULL);

  va_start (args, first_knot);
  clutter_behaviour_bspline_append_knots_valist (bs, first_knot, args);
  va_end (args);
}

/**
 * clutter_behaviour_bspline_truncate:
 * @bs:      a #ClutterBehaviourBspline
 * @offset:  offset of control where the bspline should be truncated  
 *
 * Truncates the bezier spline at the control point; if the control point at
 * offset is not one of the on-curve points, the bspline will be
 * truncated at the nearest preceeding on-curve point.
 *
 * Since: 0.4
 */
void
clutter_behaviour_bspline_truncate (ClutterBehaviourBspline *bs,
                                    guint                    offset)
{
  guint i;
  
  if (offset == 0)
    {
      clutter_behaviour_bspline_clear (bs);
      return;
    }

  /* convert control point offset to the offset of last spline to keep */
  offset = (offset-1) / 3;

  bs->priv->splines = g_array_set_size (bs->priv->splines, offset+1);
  bs->priv->length = 0;
  
  for (i = 0; i < bs->priv->splines->len; ++i)
    {
      ClutterBezier * b = g_array_index (bs->priv->splines,
                                         ClutterBezier*,
                                         i);
      
      bs->priv->length += b->length;
    }
  
}

/**
 * clutter_behaviour_bspline_clear:
 * @bs:      a #ClutterBehaviourBspline
 *
 * Empties a bspline.
 *
 * Since: 0.4
 */
void
clutter_behaviour_bspline_clear (ClutterBehaviourBspline * bs)
{
  gint i;

  for (i = 0; i < bs->priv->splines->len; ++i)
    {
      clutter_bezier_free (g_array_index (bs->priv->splines,
                                          ClutterBezier*, i));
    }
    
  g_array_set_size (bs->priv->splines, 0);

  for (i = 0; i < bs->priv->point_stack->len; ++i)
    {
      clutter_knot_free (g_array_index (bs->priv->point_stack,
                                        ClutterKnot*, i));
    }
    
  g_array_set_size (bs->priv->point_stack, 0);

  bs->priv->x = 0;
  bs->priv->y = 0;
  bs->priv->length = 0;
}

/**
 * clutter_behaviour_bspline_join:
 * @bs1:      a #ClutterBehaviourBspline
 * @bs2:      a #ClutterBehaviourBspline
 *
 * Joins a copy of bezier spline bs2 onto the end of bezier spline bs1; bs2 is
 * not modified.
 *
 * Since: 0.4
 */
void
clutter_behaviour_bspline_join (ClutterBehaviourBspline * bs1,
                                ClutterBehaviourBspline * bs2)
{
  gint i, x1, y1;
  ClutterKnot knot;
  ClutterBezier * b, *b2;
  
  clutter_behaviour_bspline_get_origin (bs2, &knot);
  
  b = g_array_index (bs1->priv->splines,ClutterBezier*,
                     bs1->priv->splines->len-1);

  x1 = clutter_bezier_t2x (b, CBZ_T_ONE);
  y1 = clutter_bezier_t2y (b, CBZ_T_ONE);

  /*
   * need to move bs2 so it joins bs1
   */
  x1 -= knot.x;
  y1 -= knot.y;

  for (i = 0; i < bs1->priv->splines->len; ++i)
    {
      b = g_array_index (bs2->priv->splines, ClutterBezier*, i);
      b2 = clutter_bezier_clone_and_move (b, x1, y1);

      bs1->priv->length += b2->length;
      g_array_append_val (bs1->priv->splines, b2);
    }
}

/**
 * clutter_behaviour_bspline_split:
 * @bs:      a #ClutterBehaviourBspline
 * @offset:  an offset of the control point at which to split the spline.
 *
 * Return: new ClutterBehaviourBspline.
 * 
 * Splits a bezier spline into two at the control point at offset; if the
 * control point at offset is not one of the on-curve bezier points, the
 * bspline will be split at the nearest on-curve point before the offset.
 * The original bspline is shortened appropriately.
 *
 * Since: 0.4
 */
ClutterBehaviourBspline *
clutter_behaviour_bspline_split (ClutterBehaviourBspline * bs, guint offset)
{
  ClutterBehaviourBspline * bs2 = NULL;
  ClutterAlpha * alpha;
  guint i, split, length2 = 0;

  split = offset / 3;
  
  if (split == 0 || split >= bs->priv->splines->len)
    return NULL;

  alpha = clutter_behaviour_get_alpha (CLUTTER_BEHAVIOUR (bs));
    
  bs2 = g_object_new (CLUTTER_TYPE_BEHAVIOUR_BSPLINE, 
                      "alpha", alpha,
                      NULL);

  bs2->priv->splines = g_array_new (FALSE, FALSE, sizeof (ClutterBezier *));
  bs2->priv->length  = 0;

  bs2->priv->x = bs->priv->x;
  bs2->priv->y = bs->priv->y;
    
  for (i = split; i < bs->priv->splines->len; ++i)
    {
      ClutterBezier * b = g_array_index (bs->priv->splines,ClutterBezier*,i);
      g_array_append_val (bs2->priv->splines, b);
      length2 += b->length;
    }

  bs->priv->length -= length2;
  bs2->priv->length = length2;

  g_array_set_size (bs->priv->splines, split);

  return bs2;
}

/**
 * clutter_behaviour_bspline_adjust:
 * @bs: a #ClutterBehaviourBspline
 * @offset: an index of control point to ajdust
 * @knot: a #ClutterKnot with new coordinances for the control point.
 *
 * Change the coordinaces of control point at index to those represented by
 * the knot.
 *
 * Since: 0.4
 */
void
clutter_behaviour_bspline_adjust (ClutterBehaviourBspline  * bs,
				  guint                      offset,
				  ClutterKnot              * knot)
{
  ClutterBezier * b1 = NULL;
  ClutterBezier * b2 = NULL;
  guint           p1_indx = 0;
  guint           p2_indx = 0;
  guint           old_length;
    
  /*
   * Find the bezier(s) affected by change of this control point
   * and the relative position of the control point within them
   */
    
  if (offset == 0)
    {
      b1 = g_array_index (bs->priv->splines, ClutterBezier*, 0);;
    }
  else if (offset + 1 == bs->priv->splines->len)
    {
      b2 = g_array_index (bs->priv->splines, ClutterBezier*,
                          bs->priv->splines->len-1);
      p2_indx = 3;
    }
  else 
    {
      guint mod3 = offset % 3;
      guint i = offset / 3;
	
      if (mod3 == 0)
	{
          /* on-curve point, i.e., two beziers */
          b1 = g_array_index (bs->priv->splines, ClutterBezier*, i-1);
          b2 = g_array_index (bs->priv->splines, ClutterBezier*, i);
          p1_indx = 3;
	}
      else
	{
          b1 = g_array_index (bs->priv->splines,ClutterBezier*,i);
          p1_indx = mod3;
	}
    }

  /*
   * Adjust the bezier(s) and total bspline length
   */
  if (b1)
    {
      old_length = b1->length;
      clutter_bezier_adjust (b1, knot, p1_indx);
      bs->priv->length = bs->priv->length - old_length + b1->length;
    }
    
  if (b2)
    {
      old_length = b2->length;
      clutter_bezier_adjust (b2, knot, p2_indx);
      bs->priv->length = bs->priv->length - old_length + b2->length;
    }
}

/**
 * clutter_behaviour_bspline_set_origin
 * @bs:   a #ClutterBehaviourBspline
 * @knot: a #ClutterKnot origin for the bezier
 *
 * Sets the origin of the bezier to the point represented by knot. (Initially
 * the origin of a bspline is given by the position of the first control point
 * of the first bezier curve.)
 * 
 * Since: 0.4
 */
void
clutter_behaviour_bspline_set_origin (ClutterBehaviourBspline * bs,
				      ClutterKnot             * knot)
{
  if (bs->priv->splines->len == 0)
    {
      bs->priv->x = knot->x;
      bs->priv->y = knot->y;
    }
  else
    {
      ClutterBezier * b = g_array_index (bs->priv->splines, ClutterBezier*,
                                         0);

      bs->priv->x = knot->x - b->dx;
      bs->priv->y = knot->y - b->dy;
#if 0
      g_debug ("setting origin to {%d,%d}: b {%d,%d}, adjustment {%d,%d}",
               knot->x, knot->y,
               b->dx, b->dy,
               bs->priv->x, bs->priv->y);
#endif
    }
}

/**
 * clutter_behaviour_bspline_get_origin
 * @bs:   a #ClutterBehaviourBspline
 * @knot: a #ClutterKnot where to store the origin of the bezier
 *
 * Gets the origin of the bezier.
 * 
 * Since: 0.4
 */
void
clutter_behaviour_bspline_get_origin (ClutterBehaviourBspline * bs,
				      ClutterKnot             * knot)
{
  if (bs->priv->splines->len == 0)
    {
      knot->x = bs->priv->x;
      knot->y = bs->priv->y;
    }
  else
    {
      ClutterBezier * b = g_array_index (bs->priv->splines, ClutterBezier*,
                                         0);
	
      knot->x = bs->priv->x + b->dx;
      knot->y = bs->priv->y + b->dy;
    }
}

