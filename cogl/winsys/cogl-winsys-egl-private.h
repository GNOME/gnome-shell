/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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

#ifndef __COGL_WINSYS_EGL_PRIVATE_H
#define __COGL_WINSYS_EGL_PRIVATE_H

#include "cogl-defines.h"
#include "cogl-winsys-private.h"
#include "cogl-context.h"

const CoglWinsysVtable *
_cogl_winsys_egl_get_vtable (void);

#ifdef EGL_KHR_image_base
EGLImageKHR
_cogl_egl_create_image (CoglContext *ctx,
                        EGLenum target,
                        EGLClientBuffer buffer,
                        const EGLint *attribs);

void
_cogl_egl_destroy_image (CoglContext *ctx,
                         EGLImageKHR image);
#endif

#endif /* __COGL_WINSYS_EGL_PRIVATE_H */
