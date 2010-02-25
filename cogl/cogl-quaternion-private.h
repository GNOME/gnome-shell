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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_QUATERNION_PRIVATE_H__
#define __COGL_QUATERNION_PRIVATE_H__

#include <glib.h>

/* squared length */
#define _COGL_QUATERNION_NORM(Q) \
  ((Q)->x*(Q)->x + (Q)->y*(Q)->y + (Q)->z*(Q)->z + (Q)->w*(Q)->w)

#define _COGL_QUATERNION_DEGREES_TO_RADIANS (G_PI / 180.0f)
#define _COGL_QUATERNION_RADIANS_TO_DEGREES (180.0f / G_PI)

#endif /* __COGL_QUATERNION_PRIVATE_H__ */
