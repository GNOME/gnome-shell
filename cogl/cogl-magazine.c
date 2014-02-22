/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
