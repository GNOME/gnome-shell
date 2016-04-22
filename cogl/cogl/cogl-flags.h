/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifndef __COGL_FLAGS_H
#define __COGL_FLAGS_H

#include <glib.h>

#include "cogl-util.h"

COGL_BEGIN_DECLS

/* These are macros used to implement a fixed-size array of bits. This
   should be used instead of CoglBitmask when the maximum bit number
   that will be set is known at compile time, for example when setting
   for recording a set of known available features */

/* The bits are stored in an array of unsigned longs. To use these
   macros, you would typically have an enum defining the available
   bits with an extra last enum to define the maximum value. Then to
   store the flags you would declare an array of unsigned longs sized
   using COGL_FLAGS_N_LONGS_FOR_SIZE, eg:

   typedef enum { FEATURE_A, FEATURE_B, FEATURE_C, N_FEATURES } Features;

   unsigned long feature_flags[COGL_FLAGS_N_LONGS_FOR_SIZE (N_FEATURES)];
*/

#define COGL_FLAGS_N_LONGS_FOR_SIZE(size)       \
  (((size) +                                    \
    (sizeof (unsigned long) * 8 - 1))           \
   / (sizeof (unsigned long) * 8))

/* @flag is expected to be constant so these should result in a
   constant expression. This means that setting a flag is equivalent
   to just setting in a bit in a global variable at a known
   location */
#define COGL_FLAGS_GET_INDEX(flag)              \
  ((flag) / (sizeof (unsigned long) * 8))
#define COGL_FLAGS_GET_MASK(flag)               \
  (1UL << ((unsigned long) (flag) &             \
           (sizeof (unsigned long) * 8 - 1)))

#define COGL_FLAGS_GET(array, flag)             \
  (!!((array)[COGL_FLAGS_GET_INDEX (flag)] &    \
      COGL_FLAGS_GET_MASK (flag)))

/* The expectation here is that @value will be constant so the if
   statement will be optimised out */
#define COGL_FLAGS_SET(array, flag, value)      \
  G_STMT_START {                                \
    if (value)                                  \
      ((array)[COGL_FLAGS_GET_INDEX (flag)] |=  \
       COGL_FLAGS_GET_MASK (flag));             \
    else                                        \
      ((array)[COGL_FLAGS_GET_INDEX (flag)] &=  \
       ~COGL_FLAGS_GET_MASK (flag));            \
  } G_STMT_END

/* Macros to help iterate an array of flags. It should be used like
 * this:
 *
 * int n_longs = COGL_FLAGS_N_LONGS_FOR_SIZE (...);
 * unsigned long flags[n_longs];
 * int bit_num;
 *
 * COGL_FLAGS_FOREACH_START (flags, n_longs, bit_num)
 *   {
 *     do_something_with_the_bit (bit_num);
 *   }
 * COGL_FLAGS_FOREACH_END;
 */
#define COGL_FLAGS_FOREACH_START(array, n_longs, bit)   \
  G_STMT_START {                                        \
  const unsigned long *_p = (array);                    \
  int _n_longs = (n_longs);                             \
  int _i;                                               \
                                                        \
  for (_i = 0; _i < _n_longs; _i++)                     \
    {                                                   \
      unsigned long _mask = *(_p++);                    \
                                                        \
      (bit) = _i * sizeof (unsigned long) * 8 - 1;      \
                                                        \
      while (_mask)                                     \
        {                                               \
          int _next_bit = _cogl_util_ffsl (_mask);      \
          (bit) += _next_bit;                           \
          /* This odd two-part shift is to avoid */     \
          /* shifting by sizeof (long)*8 which has */   \
          /* undefined results according to the */      \
          /* C spec (and seems to be a no-op in */      \
          /* practice) */                               \
          _mask = (_mask >> (_next_bit - 1)) >> 1;      \

#define COGL_FLAGS_FOREACH_END \
  } } } G_STMT_END

COGL_END_DECLS

#endif /* __COGL_FLAGS_H */

