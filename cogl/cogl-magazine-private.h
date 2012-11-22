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
