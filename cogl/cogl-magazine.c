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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * CoglMagazine provides a really light weight allocator for chunks
 * of memory with a pre-determined size.
 *
 * This allocator builds on CoglMemoryStack for making all initial
 * allocations but never frees memory back to the stack.
 *
 * Memory chunks that haven't been allocated yet are stored in a
 * singly linked, fifo, list.
 *
 * Allocating from a magazine is simply a question of popping an entry
 * from the head of the fifo list. If no entries are available then
 * instead allocate from the memory stack instead.
 *
 * When an entry is freed, it is put back into the fifo list for
 * re-use.
 *
 * No attempt is ever made to shrink the amount of memory associated
 * with a CoglMagazine.
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-memory-stack-private.h"
#include "cogl-magazine-private.h"
#include <glib.h>

#define ROUND_UP_8(X) ((X + (8 - 1)) & ~(8 - 1))

CoglMagazine *
_cogl_magazine_new (size_t chunk_size, int initial_chunk_count)
{
  CoglMagazine *magazine = g_new0 (CoglMagazine, 1);

  chunk_size = MAX (chunk_size, sizeof (CoglMagazineChunk));
  chunk_size = ROUND_UP_8 (chunk_size);

  magazine->chunk_size = chunk_size;
  magazine->stack = _cogl_memory_stack_new (chunk_size * initial_chunk_count);
  magazine->head = NULL;

  return magazine;
}

void
_cogl_magazine_free (CoglMagazine *magazine)
{
  _cogl_memory_stack_free (magazine->stack);
  g_free (magazine);
}
