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
#ifndef __COGL_PANGO_H_COMPAT__
#define __COGL_PANGO_H_COMPAT__

#ifdef COGL_ENABLE_EXPERIMENTAL_2_0_API
#error "#include <cogl/cogl-pango.h> is unsupported; please #include <cogl-pango/cogl-pango.h>"
#else
#warning "#include <cogl/cogl-pango.h> is deprecated; please #include <cogl-pango/cogl-pango.h>"
#include <cogl-pango/cogl-pango.h>
#endif

#endif /* __COGL_PANGO_H_COMPAT__ */
