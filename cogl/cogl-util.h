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

/* Double check that config.h has been included */
#if !defined (GETTEXT_PACKAGE) && !defined (_COGL_IN_TEST_BITMASK)
#error "config.h must be included before including cogl-util.h"
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
static inline CoglBool
cogl_util_float_signbit (float x)
{
  static const union { float f; uint32_t i; } negative_one = { -1.0f };
  static const union { float f; uint32_t i; } positive_one = { +1.0f };
  union { float f; uint32_t i; } value = { x };

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
static inline CoglBool
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
#define COGL_UTIL_HAVE_BUILTIN_CLZ
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

static inline unsigned int
_cogl_util_fls (unsigned int n)
{
#ifdef COGL_UTIL_HAVE_BUILTIN_CLZ
   return n == 0 ? 0 : sizeof (unsigned int) * 8 - __builtin_clz (n);
#else
   unsigned int v = 1;

   if (n == 0)
      return 0;

   while (n >>= 1)
       v++;

   return v;
#endif
}

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
#ifdef COGL_ENABLE_DEBUG
#define _COGL_RETURN_START do {
#define _COGL_RETURN_END } while (0)
#else /* COGL_ENABLE_DEBUG */
/* If debugging is disabled then we don't actually want to do the
 * check but we still want the code for the expression to be generated
 * so that it won't give loads of warnings about unused variables.
 * Therefore we just surround the block with if(0) */
#define _COGL_RETURN_START do { if (0) {
#define _COGL_RETURN_END } } while (0)
#endif /* COGL_ENABLE_DEBUG */
#define _COGL_RETURN_IF_FAIL(EXPR) _COGL_RETURN_START {             \
   if (!(EXPR))						            \
     {							            \
       fprintf (stderr, "file %s: line %d: assertion `%s' failed",  \
                __FILE__,					    \
                __LINE__,					    \
                #EXPR);						    \
       return;						            \
     };                                                             \
  } _COGL_RETURN_END
#define _COGL_RETURN_VAL_IF_FAIL(EXPR, VAL) _COGL_RETURN_START {    \
   if (!(EXPR))						            \
     {							            \
       fprintf (stderr, "file %s: line %d: assertion `%s' failed",  \
                __FILE__,					    \
                __LINE__,					    \
                #EXPR);						    \
       return (VAL);						    \
     };                                                             \
  } _COGL_RETURN_END
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

/* Since we can't rely on _Static_assert always being available for
 * all compilers we have limited static assert that can be used in
 * C code but not in headers.
 */
#define _COGL_TYPEDEF_ASSERT(EXPRESSION) \
  typedef struct { char Compile_Time_Assertion[(EXPRESSION) ? 1 : -1]; } \
  G_PASTE (_GStaticAssert_, __LINE__)

/* _COGL_STATIC_ASSERT:
 * @expression: An expression to assert evaluates to true at compile
 *              time.
 * @message: A message to print to the console if the assertion fails
 *           at compile time.
 *
 * Allows you to assert that an expression evaluates to true at
 * compile time and aborts compilation if not. If possible message
 * will also be printed if the assertion fails.
 *
 * Note: Only Gcc >= 4.6 supports the c11 _Static_assert which lets us
 * print a nice message if the compile time assertion fails.
 */
#ifdef HAVE_STATIC_ASSERT
#define _COGL_STATIC_ASSERT(EXPRESSION, MESSAGE) \
  _Static_assert (EXPRESSION, MESSAGE);
#else
#define _COGL_STATIC_ASSERT(EXPRESSION, MESSAGE)
#endif

#ifdef HAVE_MEMMEM
#define _cogl_util_memmem memmem
#else
char *
_cogl_util_memmem (const void *haystack,
                   size_t haystack_len,
                   const void *needle,
                   size_t needle_len);
#endif

static inline void
_cogl_util_scissor_intersect (int rect_x0,
                              int rect_y0,
                              int rect_x1,
                              int rect_y1,
                              int *scissor_x0,
                              int *scissor_y0,
                              int *scissor_x1,
                              int *scissor_y1)
{
  *scissor_x0 = MAX (*scissor_x0, rect_x0);
  *scissor_y0 = MAX (*scissor_y0, rect_y0);
  *scissor_x1 = MIN (*scissor_x1, rect_x1);
  *scissor_y1 = MIN (*scissor_y1, rect_y1);
}

#endif /* __COGL_UTIL_H */
