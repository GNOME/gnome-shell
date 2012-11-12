/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "cogl-output-private.h"

#include <string.h>

static void _cogl_output_free (CoglOutput *output);

COGL_OBJECT_DEFINE (Output, output);

CoglOutput *
_cogl_output_new (const char *name)
{
  CoglOutput *output;

  output = g_slice_new0 (CoglOutput);
  output->name = g_strdup (name);

  return _cogl_output_object_new (output);
}

static void
_cogl_output_free (CoglOutput *output)
{
  g_free (output->name);

  g_slice_free (CoglOutput, output);
}

gboolean
_cogl_output_values_equal (CoglOutput *output,
                           CoglOutput *other)
{
  return memcmp ((const char *)output + G_STRUCT_OFFSET (CoglOutput, x),
                 (const char *)other + G_STRUCT_OFFSET (CoglOutput, x),
                 sizeof (CoglOutput) - G_STRUCT_OFFSET (CoglOutput, x)) == 0;
}

int
cogl_output_get_x (CoglOutput *output)
{
  return output->x;
}

int
cogl_output_get_y (CoglOutput *output)
{
  return output->y;
}

int
cogl_output_get_width (CoglOutput *output)
{
  return output->width;
}

int
cogl_output_get_height (CoglOutput *output)
{
  return output->height;
}

int
cogl_output_get_mm_width (CoglOutput *output)
{
  return output->mm_width;
}

int
cogl_output_get_mm_height (CoglOutput *output)
{
  return output->mm_height;
}

CoglSubpixelOrder
cogl_output_get_subpixel_order (CoglOutput *output)
{
  return output->subpixel_order;
}

float
cogl_output_get_refresh_rate (CoglOutput *output)
{
  return output->refresh_rate;
}
