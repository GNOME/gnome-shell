/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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

#ifndef __COGL_ERROR_PRIVATE_H__
#define __COGL_ERROR_PRIVATE_H__

#include "cogl-error.h"

void
_cogl_set_error (CoglError **error,
                 uint32_t domain,
                 int code,
                 const char *format,
                 ...) G_GNUC_PRINTF (4, 5);

void
_cogl_set_error_literal (CoglError **error,
                         uint32_t domain,
                         int code,
                         const char *message);

void
_cogl_propogate_gerror (CoglError **dest,
                        GError *src);

#define _cogl_clear_error(X) g_clear_error ((GError **)X)

#endif /* __COGL_ERROR_PRIVATE_H__ */
