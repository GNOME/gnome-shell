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
#include "cogl-config.h"
#endif

#include "cogl-util.h"
#include "cogl-point-in-poly-private.h"

#include <glib.h>

/* We've made a notable change to the original algorithm referenced
 * above to make sure we have reliable results for screen aligned
 * rectangles even though there may be some numerical in-precision in
 * how the vertices of the polygon were calculated.
 *
 * We've avoided introducing an epsilon factor to the comparisons
 * since we feel there's a risk of changing some semantics in ways that
 * might not be desirable. One of those is that if you transform two
 * polygons which share an edge and test a point close to that edge
 * then this algorithm will currently give a positive result for only
 * one polygon.
 *
 * Another concern is the way this algorithm resolves the corner case
 * where the horizontal ray being cast to count edge crossings may
 * cross directly through a vertex. The solution is based on the "idea
 * of Simulation of Simplicity" and "pretends to shift the ray
 * infinitesimally down so that it either clearly intersects, or
 * clearly doesn't touch". I'm not familiar with the idea myself so I
 * expect a misplaced epsilon is likely to break that aspect of the
 * algorithm.
 *
 * The simple solution we've gone for is to pixel align the polygon
 * vertices which should eradicate most noise due to in-precision.
 */
int
_cogl_util_point_in_screen_poly (float point_x,
                                 float point_y,
                                 void *vertices,
                                 size_t stride,
                                 int n_vertices)
{
  int i, j, c = 0;

  for (i = 0, j = n_vertices - 1; i < n_vertices; j = i++)
    {
      float vert_xi = *(float *)((uint8_t *)vertices + i * stride);
      float vert_xj = *(float *)((uint8_t *)vertices + j * stride);
      float vert_yi = *(float *)((uint8_t *)vertices + i * stride +
                                 sizeof (float));
      float vert_yj = *(float *)((uint8_t *)vertices + j * stride +
                                 sizeof (float));

      vert_xi = COGL_UTIL_NEARBYINT (vert_xi);
      vert_xj = COGL_UTIL_NEARBYINT (vert_xj);
      vert_yi = COGL_UTIL_NEARBYINT (vert_yi);
      vert_yj = COGL_UTIL_NEARBYINT (vert_yj);

      if (((vert_yi > point_y) != (vert_yj > point_y)) &&
           (point_x < (vert_xj - vert_xi) * (point_y - vert_yi) /
            (vert_yj - vert_yi) + vert_xi) )
         c = !c;
    }

  return c;
}

