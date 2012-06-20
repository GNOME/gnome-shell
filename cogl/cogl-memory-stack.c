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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 * CoglMemoryStack provides a really simple, but lightning fast
 * memory stack allocation strategy:
 *
 * - The underlying pool of memory is grow-only.
 * - The pool is considered to be a stack which may be comprised
 *   of multiple smaller stacks. Allocation is done as follows:
 *    - If there's enough memory in the current sub-stack then the
 *      stack-pointer will be returned as the allocation and the
 *      stack-pointer will be incremented by the allocation size.
 *    - If there isn't enough memory in the current sub-stack
 *      then a new sub-stack is allocated twice as big as the current
 *      sub-stack or twice as big as the requested allocation size if
 *      that's bigger and the stack-pointer is set to the start of the
 *      new sub-stack.
 * - Allocations can't be freed in a random-order, you can only
 *   rewind the entire stack back to the start. There is no
 *   the concept of stack frames to allow partial rewinds.
 *
 * For example; we plan to use this in our tesselator which has to
 * allocate lots of small vertex, edge and face structures because
 * when tesselation has been finished we just want to free the whole
 * lot in one go.
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-memory-stack-private.h"
#include "cogl-queue.h"

#include <stdint.h>

#include <glib.h>

typedef struct _CoglMemorySubStack CoglMemorySubStack;

COGL_TAILQ_HEAD (CoglMemorySubStackList, CoglMemorySubStack);

struct _CoglMemorySubStack
{
  COGL_TAILQ_ENTRY (CoglMemorySubStack) list_node;
  size_t bytes;
  uint8_t *data;
};

struct _CoglMemoryStack
{
  CoglMemorySubStackList sub_stacks;

  CoglMemorySubStack *sub_stack;
  size_t sub_stack_offset;
};

static CoglMemorySubStack *
_cogl_memory_sub_stack_alloc (size_t bytes)
{
  CoglMemorySubStack *sub_stack = g_slice_new (CoglMemorySubStack);
  sub_stack->bytes = bytes;
  sub_stack->data = g_malloc (bytes);
  return sub_stack;
}

static void
_cogl_memory_stack_add_sub_stack (CoglMemoryStack *stack,
                                  size_t sub_stack_bytes)
{
  CoglMemorySubStack *sub_stack =
    _cogl_memory_sub_stack_alloc (sub_stack_bytes);
  COGL_TAILQ_INSERT_TAIL (&stack->sub_stacks, sub_stack, list_node);
  stack->sub_stack = sub_stack;
  stack->sub_stack_offset = 0;
}

CoglMemoryStack *
_cogl_memory_stack_new (size_t initial_size_bytes)
{
  CoglMemoryStack *stack = g_slice_new0 (CoglMemoryStack);

  COGL_TAILQ_INIT (&stack->sub_stacks);

  _cogl_memory_stack_add_sub_stack (stack, initial_size_bytes);

  return stack;
}

void *
_cogl_memory_stack_alloc (CoglMemoryStack *stack, size_t bytes)
{
  CoglMemorySubStack *sub_stack;
  void *ret;

  sub_stack = stack->sub_stack;
  if (G_LIKELY (sub_stack->bytes - stack->sub_stack_offset >= bytes))
    {
      ret = sub_stack->data + stack->sub_stack_offset;
      stack->sub_stack_offset += bytes;
      return ret;
    }

  /* If the stack has been rewound and then a large initial allocation
   * is made then we may need to skip over one or more of the
   * sub-stacks that are too small for the requested allocation
   * size... */
  for (sub_stack = sub_stack->list_node.tqe_next;
       sub_stack;
       sub_stack = sub_stack->list_node.tqe_next)
    {
      if (sub_stack->bytes >= bytes)
        {
          ret = sub_stack->data;
          stack->sub_stack = sub_stack;
          stack->sub_stack_offset = bytes;
          return ret;
        }
    }

  /* Finally if we couldn't find a free sub-stack with enough space
   * for the requested allocation we allocate another sub-stack that's
   * twice as big as the last sub-stack or twice as big as the
   * requested allocation if that's bigger.
   */

  sub_stack = COGL_TAILQ_LAST (&stack->sub_stacks, CoglMemorySubStackList);

  _cogl_memory_stack_add_sub_stack (stack, MAX (sub_stack->bytes, bytes) * 2);

  sub_stack = COGL_TAILQ_LAST (&stack->sub_stacks, CoglMemorySubStackList);

  stack->sub_stack_offset += bytes;

  return sub_stack->data;
}

void
_cogl_memory_stack_rewind (CoglMemoryStack *stack)
{
  stack->sub_stack = COGL_TAILQ_FIRST (&stack->sub_stacks);
  stack->sub_stack_offset = 0;
}

static void
_cogl_memory_sub_stack_free (CoglMemorySubStack *sub_stack)
{
  g_free (sub_stack->data);
  g_slice_free (CoglMemorySubStack, sub_stack);
}

void
_cogl_memory_stack_free (CoglMemoryStack *stack)
{
  CoglMemorySubStack *sub_stack;

  while ((sub_stack = COGL_TAILQ_FIRST (&stack->sub_stacks)))
    {
      COGL_TAILQ_REMOVE (&stack->sub_stacks, sub_stack, list_node);
      _cogl_memory_sub_stack_free (sub_stack);
    }

  g_slice_free (CoglMemoryStack, stack);
}
