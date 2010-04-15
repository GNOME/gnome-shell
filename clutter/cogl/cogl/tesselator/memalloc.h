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
 *
 */

/* This is a simple replacement for memalloc from the SGI tesselator
   code to force it to use glib's allocation instead */

#ifndef __MEMALLOC_H__
#define __MEMALLOC_H__

#include <glib.h>

#define memRealloc g_realloc
#define memAlloc   g_malloc
#define memFree    g_free
#define memInit(x) 1

/* tess.c defines TRUE and FALSE itself unconditionally so we need to
   undefine it from the glib headers */
#undef TRUE
#undef FALSE

#endif /* __MEMALLOC_H__ */
