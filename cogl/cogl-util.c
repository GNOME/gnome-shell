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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-util.h"

/*
 * cogl_util_next_p2:
 * @a: Value to get the next power of two
 *
 * Calculates the next power of two greater than or equal to @a.
 *
 * Return value: @a if @a is already a power of two, otherwise returns
 *   the next nearest power of two.
 */
int
_cogl_util_next_p2 (int a)
{
  int rval = 1;

  while (rval < a)
    rval <<= 1;

  return rval;
}

unsigned int
_cogl_util_one_at_a_time_mix (unsigned int hash)
{
  hash += ( hash << 3 );
  hash ^= ( hash >> 11 );
  hash += ( hash << 15 );

  return hash;
}

/* The 'ffs' function is part of C99 so it isn't always available */
#ifndef HAVE_FFS

int
_cogl_util_ffs (int num)
{
  int i = 1;

  if (num == 0)
    return 0;

  while ((num & 1) == 0)
    {
      num >>= 1;
      i++;
    }

  return i;
}
#endif /* HAVE_FFS */
