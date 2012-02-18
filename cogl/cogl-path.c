/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-util.h"
#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl2-path.h"

#include <string.h>
#include <math.h>

#undef cogl_path_set_fill_rule
void
cogl_path_set_fill_rule (CoglPathFillRule fill_rule)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_set_fill_rule (ctx->current_path, fill_rule);
}

#undef cogl_path_get_fill_rule
CoglPathFillRule
cogl_path_get_fill_rule (void)
{
  _COGL_GET_CONTEXT (ctx, COGL_PATH_FILL_RULE_EVEN_ODD);

  return cogl2_path_get_fill_rule (ctx->current_path);
}

#undef cogl_path_fill
void
cogl_path_fill (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_fill (ctx->current_path);

  cogl_object_unref (ctx->current_path);
  ctx->current_path = cogl2_path_new ();
}

#undef cogl_path_fill_preserve
void
cogl_path_fill_preserve (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_fill (ctx->current_path);
}

#undef cogl_path_stroke
void
cogl_path_stroke (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_stroke (ctx->current_path);

  cogl_object_unref (ctx->current_path);
  ctx->current_path = cogl2_path_new ();
}

#undef cogl_path_stroke_preserve
void
cogl_path_stroke_preserve (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_stroke (ctx->current_path);
}

#undef cogl_path_move_to
void
cogl_path_move_to (float x,
                   float y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_move_to (ctx->current_path, x, y);
}

#undef cogl_path_rel_move_to
void
cogl_path_rel_move_to (float x,
                       float y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_rel_move_to (ctx->current_path, x, y);
}

#undef cogl_path_line_to
void
cogl_path_line_to (float x,
                   float y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_line_to (ctx->current_path, x, y);
}

#undef cogl_path_rel_line_to
void
cogl_path_rel_line_to (float x,
                       float y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_rel_line_to (ctx->current_path, x, y);
}

#undef cogl_path_close
void
cogl_path_close (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_close (ctx->current_path);
}

#undef cogl_path_new
void
cogl_path_new (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_object_unref (ctx->current_path);
  ctx->current_path = cogl2_path_new ();
}

#undef cogl_path_line
void
cogl_path_line (float x_1,
	        float y_1,
	        float x_2,
	        float y_2)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_line (ctx->current_path, x_1, y_1, x_2, y_2);
}

#undef cogl_path_polyline
void
cogl_path_polyline (const float *coords,
	            int num_points)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_polyline (ctx->current_path, coords, num_points);
}

#undef cogl_path_polygon
void
cogl_path_polygon (const float *coords,
	           int num_points)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_polygon (ctx->current_path, coords, num_points);
}

#undef cogl_path_rectangle
void
cogl_path_rectangle (float x_1,
                     float y_1,
                     float x_2,
                     float y_2)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_rectangle (ctx->current_path, x_1, y_1, x_2, y_2);
}

#undef cogl_path_arc
void
cogl_path_arc (float center_x,
               float center_y,
               float radius_x,
               float radius_y,
               float angle_1,
               float angle_2)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_arc (ctx->current_path,
                  center_x,
                  center_y,
                  radius_x,
                  radius_y,
                  angle_1,
                  angle_2);
}

#undef cogl_path_ellipse
void
cogl_path_ellipse (float center_x,
                   float center_y,
                   float radius_x,
                   float radius_y)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_ellipse (ctx->current_path,
                      center_x,
                      center_y,
                      radius_x,
                      radius_y);
}

#undef cogl_path_round_rectangle
void
cogl_path_round_rectangle (float x_1,
                           float y_1,
                           float x_2,
                           float y_2,
                           float radius,
                           float arc_step)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_round_rectangle (ctx->current_path,
                              x_1, y_1, x_2, y_2, radius, arc_step);
}

#undef cogl_path_curve_to
void
cogl_path_curve_to (float x_1,
                    float y_1,
                    float x_2,
                    float y_2,
                    float x_3,
                    float y_3)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_curve_to (ctx->current_path,
                       x_1, y_2, x_2, y_2, x_3, y_3);
}

#undef cogl_path_rel_curve_to
void
cogl_path_rel_curve_to (float x_1,
                        float y_1,
                        float x_2,
                        float y_2,
                        float x_3,
                        float y_3)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl2_path_rel_curve_to (ctx->current_path,
                           x_1, y_1, x_2, y_2, x_3, y_3);
}

CoglPath *
cogl_get_path (void)
{
  _COGL_GET_CONTEXT (ctx, NULL);

  return ctx->current_path;
}

void
cogl_set_path (CoglPath *path)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _COGL_RETURN_IF_FAIL (cogl_is_path (path));

  /* Reference the new object first in case it is the same as the old
     object */
  cogl_object_ref (path);
  cogl_object_unref (ctx->current_path);
  ctx->current_path = path;
}

