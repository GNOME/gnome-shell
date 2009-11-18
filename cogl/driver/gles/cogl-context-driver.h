/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 */

#ifndef __COGL_CONTEXT_DRIVER_H
#define __COGL_CONTEXT_DRIVER_H

#include "cogl.h"
#include "cogl-gles2-wrapper.h"

#ifndef APIENTRY
#define APIENTRY
#endif

#define COGL_FEATURE_BEGIN(a, b, c, d, e, f)

#define COGL_FEATURE_FUNCTION(ret, name, args) \
  ret (APIENTRY * pf_ ## name) args;

#define COGL_FEATURE_END()

typedef struct _CoglContextDriver
{
  /* This defines a list of function pointers */
#include "cogl-feature-functions.h"

#ifdef HAVE_COGL_GLES2
  CoglGles2Wrapper  gles2;
#endif
} CoglContextDriver;

#undef COGL_FEATURE_BEGIN
#undef COGL_FEATURE_FUNCTION
#undef COGL_FEATURE_END

#endif /* __COGL_CONTEXT_DRIVER_H */

