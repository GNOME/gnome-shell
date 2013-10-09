/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2013 Intel Corporation.
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
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_TYPE_CASTS_H__
#define __COGL_TYPE_CASTS_H__

/* The various interface types in Cogl used to be more strongly typed
 * which required lots type casting by developers. We provided
 * macros for performing these casts following a widely used Gnome
 * coding style. Since we now consistently typedef these interfaces
 * as void for the public C api and use runtime type checking to
 * catch programming errors the casts have become redundant and
 * so these macros are only kept for compatibility...
 */

#define COGL_FRAMEBUFFER(X) (X)
#define COGL_BUFFER(X) (X)
#define COGL_TEXTURE(X) (X)
#define COGL_META_TEXTURE(X) (X)
#define COGL_PRIMITIVE_TEXTURE(X) (X)

#endif /* __COGL_TYPE_CASTS_H__ */
