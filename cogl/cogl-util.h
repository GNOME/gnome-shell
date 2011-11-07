/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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
 *
 */

#ifndef __COGL_UTIL_H
#define __COGL_UTIL_H

#include <glib.h>
#include <math.h>
#include "cogl-defines.h"

#ifndef COGL_HAS_GLIB_SUPPORT
#include <stdio.h>
#endif

int
_cogl_util_next_p2 (int a);

/* The signbit macro is defined by ISO C99 so it should be available,
   however if it's not we can fallback to an evil hack */
#ifdef signbit
#define cogl_util_float_signbit(x) signbit(x)
#else
/* This trick was stolen from here:
   http://lists.boost.org/Archives/boost/2006/08/108731.php

   It xors the integer reinterpretations of -1.0f and 1.0f. In theory
   they should only differ by the signbit so that gives a mask for the
   sign which we can just test against the value */
static inline gboolean
cogl_util_float_signbit (float x)
{
  static const union { float f; guint32 i; } negative_one = { -1.0f };
  static const union { float f; guint32 i; } positive_one = { +1.0f };
  union { float f; guint32 i; } value = { x };

  return !!((negative_one.i ^ positive_one.i) & value.i);
}
#endif

/* This is a replacement for the nearbyint function which always
   rounds to the nearest integer. nearbyint is apparently a C99
   function so it might not always be available but also it seems in
   glibc it is defined as a function call so this macro could end up
   faster anyway. We can't just add 0.5f because it will break for
   negative numbers. */
#define COGL_UTIL_NEARBYINT(x) ((int) ((x) < 0.0f ? (x) - 0.5f : (x) + 0.5f))

/* Returns whether the given integer is a power of two */
static inline gboolean
_cogl_util_is_pot (unsigned int num)
{
  /* Make sure there is only one bit set */
  return (num & (num - 1)) == 0;
}

/* Split Bob Jenkins' One-at-a-Time hash
 *
 * This uses the One-at-a-Time hash algorithm designed by Bob Jenkins
 * but the mixing step is split out so the function can be used in a
 * more incremental fashion.
 */
static inline unsigned int
_cogl_util_one_at_a_time_hash (unsigned int hash,
                               void *key,
                               size_t bytes)
{
  unsigned char *p = key;
  int i;

  for (i = 0; i < bytes; i++)
    {
      hash += p[i];
      hash += (hash << 10);
      hash ^= (hash >> 6);
    }

  return hash;
}

unsigned int
_cogl_util_one_at_a_time_mix (unsigned int hash);

/* The 'ffs' function is part of C99 so it isn't always available */
#ifdef HAVE_FFS
#define _cogl_util_ffs ffs
#else
int
_cogl_util_ffs (int num);
#endif

#ifdef COGL_HAS_GLIB_SUPPORT
#define _COGL_RETURN_IF_FAIL(EXPR) g_return_if_fail(EXPR)
#define _COGL_RETURN_VAL_IF_FAIL(EXPR, VAL) g_return_val_if_fail(EXPR, VAL)
#else
#define _COGL_RETURN_IF_FAIL(EXPR) do {	                            \
   if (!(EXPR))						            \
     {							            \
       fprintf (stderr, "file %s: line %d: assertion `%s' failed",  \
                __FILE__,					    \
                __LINE__,					    \
                #EXPR);						    \
       return;						            \
     };                                                             \
  } while(0)
#define _COGL_RETURN_VAL_IF_FAIL(EXPR, VAL) do {	                    \
   if (!(EXPR))						            \
     {							            \
       fprintf (stderr, "file %s: line %d: assertion `%s' failed",  \
                __FILE__,					    \
                __LINE__,					    \
                #EXPR);						    \
       return (VAL);						    \
     };                                                             \
  } while(0)
#endif /* COGL_HAS_GLIB_SUPPORT */

#endif /* __COGL_UTIL_H */
