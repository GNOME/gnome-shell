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

#ifndef __COGL_MEMORY_STACK__
#define __COGL_MEMORY_STACK__

#include <glib.h>

typedef struct _CoglMemoryStack CoglMemoryStack;

CoglMemoryStack *
_cogl_memory_stack_new (size_t initial_size_bytes);

void *
_cogl_memory_stack_alloc (CoglMemoryStack *stack, size_t bytes);

void
_cogl_memory_stack_rewind (CoglMemoryStack *stack);

void
_cogl_memory_stack_free (CoglMemoryStack *stack);

#endif /* __COGL_MEMORY_STACK__ */
