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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

/* This is just a wrapper to use our simplified version of glu.h so
   that the tesselator code can still #include <GL/glu.h> */

#include "../tesselator.h"

/* These aren't defined on GLES and we don't really want the
   tesselator code to use them but we're also trying to avoid
   modifying the C files so we just force them to be empty here */

#undef GLAPI
#define GLAPI

#undef GLAPIENTRY
#define GLAPIENTRY

/* GLES doesn't define a GLdouble type so lets just force it to a
   regular double */
#define GLdouble double
