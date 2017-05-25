/*
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2002 Thomas Vander Stichele <thomas@apestaart.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Fraction utility functions in this file comes from gstutils.c in gstreamer.
 */

#include "config.h"

#include "core/meta-fraction.h"

#include <glib.h>
#include <math.h>

#define MAX_TERMS       30
#define MIN_DIVISOR     1.0e-10
#define MAX_ERROR       1.0e-20

static int
greatest_common_divisor (int a,
                         int b)
{
  while (b != 0)
    {
      int temp = a;

      a = b;
      b = temp % b;
    }

  return ABS (a);
}

MetaFraction
meta_fraction_from_double (double src)
{
  double V, F;                 /* double being converted */
  int N, D;                    /* will contain the result */
  int A;                       /* current term in continued fraction */
  int64_t N1, D1;              /* numerator, denominator of last approx */
  int64_t N2, D2;              /* numerator, denominator of previous approx */
  int i;
  int gcd;
  gboolean negative = FALSE;

  /* initialize fraction being converted */
  F = src;
  if (F < 0.0)
    {
      F = -F;
      negative = TRUE;
    }

  V = F;
  /* initialize fractions with 1/0, 0/1 */
  N1 = 1;
  D1 = 0;
  N2 = 0;
  D2 = 1;
  N = 1;
  D = 1;

  for (i = 0; i < MAX_TERMS; i++)
    {
      /* get next term */
      A = (gint) F;               /* no floor() needed, F is always >= 0 */
      /* get new divisor */
      F = F - A;

      /* calculate new fraction in temp */
      N2 = N1 * A + N2;
      D2 = D1 * A + D2;

      /* guard against overflow */
      if (N2 > G_MAXINT || D2 > G_MAXINT)
        break;

      N = N2;
      D = D2;

      /* save last two fractions */
      N2 = N1;
      D2 = D1;
      N1 = N;
      D1 = D;

      /* quit if dividing by zero or close enough to target */
      if (F < MIN_DIVISOR || fabs (V - ((gdouble) N) / D) < MAX_ERROR)
        break;

      /* Take reciprocal */
      F = 1 / F;
    }

  /* fix for overflow */
  if (D == 0)
    {
      N = G_MAXINT;
      D = 1;
    }

  /* fix for negative */
  if (negative)
    N = -N;

  /* simplify */
  gcd = greatest_common_divisor (N, D);
  if (gcd)
    {
      N /= gcd;
      D /= gcd;
    }

  return (MetaFraction) {
    .num = N,
    .denom = D
  };
}
