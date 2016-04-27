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
#include "cogl-list.h"

#include <stdint.h>

#include <glib.h>

typedef struct _CoglMemorySubStack
{
  CoglList link;
  size_t bytes;
  uint8_t *data;
} CoglMemorySubStack;

struct _CoglMemoryStack
{
  CoglList sub_stacks;

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
  _cogl_list_insert (stack->sub_stacks.prev, &sub_stack->link);
  stack->sub_stack = sub_stack;
  stack->sub_stack_offset = 0;
}

CoglMemoryStack *
_cogl_memory_stack_new (size_t initial_size_bytes)
{
  CoglMemoryStack *stack = g_slice_new0 (CoglMemoryStack);

  _cogl_list_init (&stack->sub_stacks);

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
  for (_cogl_list_set_iterator (sub_stack->link.next, sub_stack, link);
       &sub_stack->link != &stack->sub_stacks;
       _cogl_list_set_iterator (sub_stack->link.next, sub_stack, link))
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

  sub_stack = _cogl_container_of (stack->sub_stacks.prev,
                                  CoglMemorySubStack,
                                  link);

  _cogl_memory_stack_add_sub_stack (stack, MAX (sub_stack->bytes, bytes) * 2);

  sub_stack = _cogl_container_of (stack->sub_stacks.prev,
                                  CoglMemorySubStack,
                                  link);

  stack->sub_stack_offset += bytes;

  return sub_stack->data;
}

void
_cogl_memory_stack_rewind (CoglMemoryStack *stack)
{
  stack->sub_stack = _cogl_container_of (stack->sub_stacks.next,
                                         CoglMemorySubStack,
                                         link);
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

  while (!_cogl_list_empty (&stack->sub_stacks))
    {
      CoglMemorySubStack *sub_stack =
        _cogl_container_of (stack->sub_stacks.next, CoglMemorySubStack, link);
      _cogl_list_remove (&sub_stack->link);
      _cogl_memory_sub_stack_free (sub_stack);
    }

  g_slice_free (CoglMemoryStack, stack);
}
