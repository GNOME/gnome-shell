/*
 * Point Inclusion in Polygon Test
 *
 * Copyright (c) 1970-2003, Wm. Randolph Franklin
 * Copyright (C) 2011 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimers.
 *    2. Redistributions in binary form must reproduce the above
 *    copyright notice in the documentation and/or other materials
 *    provided with the distribution.
 * 3. The name of W. Randolph Franklin may not be used to endorse or
 *    promote products derived from this Software without specific
 *    prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Note:
 * The algorithm for this point_in_poly() function was learnt from:
 * http://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/pnpoly.html
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

int
_cogl_util_point_in_poly (float point_x,
                          float point_y,
                          void *vertices,
                          size_t stride,
                          int n_vertices)
{
  int i, j, c = 0;

  for (i = 0, j = n_vertices - 1; i < n_vertices; j = i++)
    {
      float vert_xi = *(float *)((guint8 *)vertices + i * stride);
      float vert_xj = *(float *)((guint8 *)vertices + j * stride);
      float vert_yi = *(float *)((guint8 *)vertices + i * stride +
                                 sizeof (float));
      float vert_yj = *(float *)((guint8 *)vertices + j * stride +
                                 sizeof (float));

      if (((vert_yi > point_y) != (vert_yj > point_y)) &&
           (point_x < (vert_xj - vert_xi) * (point_y - vert_yi) /
            (vert_yj - vert_yi) + vert_xi) )
         c = !c;
    }

  return c;
}

