/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifndef __COGL_FLAGS_H
#define __COGL_FLAGS_H

#include <glib.h>

G_BEGIN_DECLS

/* These are macros used to implement a fixed-size array of bits. This
   should be used instead of CoglBitmask when the maximum bit number
   that will be set is known at compile time, for example when setting
   for recording a set of known available features */

/* The bits are stored in an array of unsigned ints. It would probably
   make sense to use unsigned long instead because then on 64-bit
   systems where it can handle 64-bits just as easily and it can test
   more bits. However GDebugKey uses a guint for the mask and we need
   to fit the masks into this */

/* To use these macros, you would typically have an enum defining the
   available bits with an extra last enum to define the maximum
   value. Then to store the flags you would declare an array of
   unsigned ints sized using COGL_FLAGS_N_INTS_FOR_SIZE, eg:

   typedef enum { FEATURE_A, FEATURE_B, FEATURE_C, N_FEATURES } Features;

   unsigned int feature_flags[COGL_FLAGS_N_INTS_FOR_SIZE (N_FEATURES)];
*/

#define COGL_FLAGS_N_INTS_FOR_SIZE(size)        \
  (((size) +                                    \
    (sizeof (unsigned int) * 8 - 1))            \
   / (sizeof (unsigned int) * 8))

/* @flag is expected to be constant so these should result in a
   constant expression. This means that setting a flag is equivalent
   to just setting in a bit in a global variable at a known
   location */
#define COGL_FLAGS_GET_INDEX(flag)              \
  ((flag) / (sizeof (unsigned int) * 8))
#define COGL_FLAGS_GET_MASK(flag)               \
  (1U << ((unsigned int) (flag) &               \
          (sizeof (unsigned int) * 8 - 1)))

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

G_END_DECLS

#endif /* __COGL_FLAGS_H */

