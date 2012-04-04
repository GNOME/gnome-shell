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

#include <cogl/cogl-defines.h>
#include "cogl-types.h"

#ifndef COGL_HAS_GLIB_SUPPORT
#include <stdio.h>
#endif

/* When compiling with Visual Studio, symbols that represent data that
   are exported out of the DLL need to be marked with the dllexport
   attribute. */
#ifdef _MSC_VER
#ifdef COGL_BUILD_EXP
#define COGL_EXPORT __declspec(dllexport)
#else
#define COGL_EXPORT __declspec(dllimport)
#endif
#else
#define COGL_EXPORT
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
                               const void *key,
                               size_t bytes)
{
  const unsigned char *p = key;
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

/* These two builtins are available since GCC 3.4 */
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define COGL_UTIL_HAVE_BUILTIN_FFSL
#define COGL_UTIL_HAVE_BUILTIN_POPCOUNTL
#endif

/* The 'ffs' function is part of C99 so it isn't always available */
#ifdef HAVE_FFS
#define _cogl_util_ffs ffs
#else
int
_cogl_util_ffs (int num);
#endif

/* The 'ffsl' function is non-standard but GCC has a builtin for it
   since 3.4 which we can use */
#ifdef COGL_UTIL_HAVE_BUILTIN_FFSL
#define _cogl_util_ffsl __builtin_ffsl
#else
/* If ints and longs are the same size we can just use ffs. Hopefully
   the compiler will optimise away this conditional */
#define _cogl_util_ffsl(x)                                              \
  (sizeof (long int) == sizeof (int) ? _cogl_util_ffs ((int) x) :       \
   _cogl_util_ffsl_wrapper (x))
int
_cogl_util_ffsl_wrapper (long int num);
#endif /* COGL_UTIL_HAVE_BUILTIN_FFSL */

#ifdef COGL_UTIL_HAVE_BUILTIN_POPCOUNTL
#define _cogl_util_popcountl __builtin_popcountl
#else
extern const unsigned char _cogl_util_popcount_table[256];

/* There are many ways of doing popcount but doing a table lookup
   seems to be the most robust against different sizes for long. Some
   pages seem to claim it's the fastest method anyway. */
static inline int
_cogl_util_popcountl (unsigned long num)
{
  int i;
  int sum = 0;

  /* Let's hope GCC will unroll this loop.. */
  for (i = 0; i < sizeof (num); i++)
    sum += _cogl_util_popcount_table[(num >> (i * 8)) & 0xff];

  return sum;
}

#endif /* COGL_UTIL_HAVE_BUILTIN_POPCOUNTL */

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

/* Match a CoglPixelFormat according to channel masks, color depth,
 * bits per pixel and byte order. These information are provided by
 * the Visual and XImage structures.
 *
 * If no specific pixel format could be found, COGL_PIXEL_FORMAT_ANY
 * is returned.
 */
CoglPixelFormat
_cogl_util_pixel_format_from_masks (unsigned long r_mask,
                                    unsigned long g_mask,
                                    unsigned long b_mask,
                                    int depth, int bpp,
                                    int byte_order);

#endif /* __COGL_UTIL_H */
