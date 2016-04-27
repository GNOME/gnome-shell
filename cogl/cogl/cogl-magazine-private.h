/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
 */

#ifndef __COGL_MAGAZINE_PRIVATE_H__
#define __COGL_MAGAZINE_PRIVATE_H__

#include <glib.h>

#include "cogl-memory-stack-private.h"

typedef struct _CoglMagazineChunk CoglMagazineChunk;

struct _CoglMagazineChunk
{
  CoglMagazineChunk *next;
};

typedef struct _CoglMagazine
{
  size_t chunk_size;

  CoglMemoryStack *stack;
  CoglMagazineChunk *head;
} CoglMagazine;

CoglMagazine *
_cogl_magazine_new (size_t chunk_size, int initial_chunk_count);

static inline void *
_cogl_magazine_chunk_alloc (CoglMagazine *magazine)
{
  if (G_LIKELY (magazine->head))
    {
      CoglMagazineChunk *chunk = magazine->head;
      magazine->head = chunk->next;
      return chunk;
    }
  else
    return _cogl_memory_stack_alloc (magazine->stack, magazine->chunk_size);
}

static inline void
_cogl_magazine_chunk_free (CoglMagazine *magazine, void *data)
{
  CoglMagazineChunk *chunk = data;

  chunk->next = magazine->head;
  magazine->head = chunk;
}

void
_cogl_magazine_free (CoglMagazine *magazine);

#endif /* __COGL_MAGAZINE_PRIVATE_H__ */
