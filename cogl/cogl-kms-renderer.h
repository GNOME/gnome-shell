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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_KMS_RENDERER_H__
#define __COGL_KMS_RENDERER_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-renderer.h>

COGL_BEGIN_DECLS

/**
 * cogl_kms_renderer_get_kms_fd:
 * @renderer: A #CoglRenderer
 *
 * Queries the file descriptor Cogl is using internally for
 * communicating with the kms driver.
 *
 * Return value: The kms file descriptor or -1 if no kms file
 *               desriptor has been opened by Cogl.
 * Stability: unstable
 */
int
cogl_kms_renderer_get_kms_fd (CoglRenderer *renderer);

COGL_END_DECLS
#endif /* __COGL_KMS_RENDERER_H__ */
