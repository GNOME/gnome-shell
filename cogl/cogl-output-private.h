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

#ifndef __COGL_OUTPUT_PRIVATE_H
#define __COGL_OUTPUT_PRIVATE_H

#include "cogl-output.h"
#include "cogl-object-private.h"

struct _CoglOutput
{
  CoglObject _parent;

  char *name;

  int x; /* Must be first field for _cogl_output_values_equal() */
  int y;
  int width;
  int height;
  int mm_width;
  int mm_height;
  float refresh_rate;
  CoglSubpixelOrder subpixel_order;
};

CoglOutput *_cogl_output_new (const char *name);
CoglBool _cogl_output_values_equal (CoglOutput *output,
                                    CoglOutput *other);

#endif /* __COGL_OUTPUT_PRIVATE_H */
