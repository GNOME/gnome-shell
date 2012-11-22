/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_MATRIX_PRIVATE_H
#define __COGL_MATRIX_PRIVATE_H

#include <glib.h>

COGL_BEGIN_DECLS

#define _COGL_MATRIX_DEBUG_PRINT(MATRIX) \
  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_MATRICES))) \
    { \
      g_print ("%s:\n", G_STRFUNC); \
      cogl_debug_matrix_print (MATRIX); \
    }

void
_cogl_matrix_prefix_print (const char *prefix, const CoglMatrix *matrix);

void
_cogl_matrix_init_from_matrix_without_inverse (CoglMatrix *matrix,
                                               const CoglMatrix *src);

COGL_END_DECLS

#endif /* __COGL_MATRIX_PRIVATE_H */

