/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#ifndef __COGL_CLIP_STACK_H
#define __COGL_CLIP_STACK_H

#include "cogl-matrix.h"
#include "cogl-primitive.h"
#include "cogl-framebuffer.h"
#include "cogl-matrix-stack.h"

/* The clip stack works like a GSList where only a pointer to the top
   of the stack is stored. The empty clip stack is represented simply
   by the NULL pointer. When an entry is added to or removed from the
   stack the new top of the stack is returned. When an entry is pushed
   a new clip stack entry is created which effectively takes ownership
   of the reference on the old entry. Therefore unrefing the top entry
   effectively loses ownership of all entries in the stack */

typedef struct _CoglClipStack CoglClipStack;
typedef struct _CoglClipStackRect CoglClipStackRect;
typedef struct _CoglClipStackWindowRect CoglClipStackWindowRect;
typedef struct _CoglClipStackPrimitive CoglClipStackPrimitive;

typedef enum
  {
    COGL_CLIP_STACK_RECT,
    COGL_CLIP_STACK_WINDOW_RECT,
    COGL_CLIP_STACK_PRIMITIVE
  } CoglClipStackType;

/* A clip stack consists a list of entries. Each entry has a reference
 * count and a link to its parent node. The child takes a reference on
 * the parent and the CoglClipStack holds a reference to the top of
 * the stack. There are no links back from the parent to the
 * children. This allows stacks that have common ancestry to share the
 * entries.
 *
 * For example, the following sequence of operations would generate
 * the tree below:
 *
 * CoglClipStack *stack_a = NULL;
 * stack_a = _cogl_clip_stack_push_rectangle (stack_a, ...);
 * stack_a = _cogl_clip_stack_push_rectangle (stack_a, ...);
 * stack_a = _cogl_clip_stack_push_primitive (stack_a, ...);
 * CoglClipStack *stack_b = NULL;
 * stack_b = cogl_clip_stack_push_window_rectangle (stack_b, ...);
 *
 *  stack_a
 *         \ holds a ref to
 *          +-----------+
 *          | prim node |
 *          |ref count 1|
 *          +-----------+
 *                       \
 *                        +-----------+  +-----------+
 *       both tops hold   | rect node |  | rect node |
 *       a ref to the     |ref count 2|--|ref count 1|
 *       same rect node   +-----------+  +-----------+
 *                       /
 *          +-----------+
 *          | win. rect |
 *          |ref count 1|
 *          +-----------+
 *         / holds a ref to
 *  stack_b
 *
 */

struct _CoglClipStack
{
  /* This will be null if there is no parent. If it is not null then
     this node must be holding a reference to the parent */
  CoglClipStack     *parent;

  CoglClipStackType  type;

  /* All clip entries have a window-space bounding box which we can
     use to calculate a scissor. The scissor limits the clip so that
     we don't need to do a full stencil clear if the stencil buffer is
     needed. This is stored in Cogl's coordinate space (ie, 0,0 is the
     top left) */
  int                     bounds_x0;
  int                     bounds_y0;
  int                     bounds_x1;
  int                     bounds_y1;

  unsigned int            ref_count;
};

struct _CoglClipStackRect
{
  CoglClipStack _parent_data;

  /* The rectangle for this clip */
  float x0;
  float y0;
  float x1;
  float y1;

  /* The matrix that was current when the clip was set */
  CoglMatrixEntry *matrix_entry;

  /* If this is true then the clip for this rectangle is entirely
     described by the scissor bounds. This implies that the rectangle
     is screen aligned and we don't need to use the stencil buffer to
     set the clip. We keep the entry as a rect entry rather than a
     window rect entry so that it will be easier to detect if the
     modelview matrix is that same as when a rectangle is added to the
     journal. In that case we can use the original clip coordinates
     and modify the rectangle instead. */
  CoglBool can_be_scissor;
};

struct _CoglClipStackWindowRect
{
  CoglClipStack _parent_data;

  /* The window rect clip doesn't need any specific data because it
     just adds to the scissor clip */
};

struct _CoglClipStackPrimitive
{
  CoglClipStack _parent_data;

  /* The matrix that was current when the clip was set */
  CoglMatrixEntry *matrix_entry;

  CoglPrimitive *primitive;

  float bounds_x1;
  float bounds_y1;
  float bounds_x2;
  float bounds_y2;
};

CoglClipStack *
_cogl_clip_stack_push_window_rectangle (CoglClipStack *stack,
                                        int x_offset,
                                        int y_offset,
                                        int width,
                                        int height);

CoglClipStack *
_cogl_clip_stack_push_rectangle (CoglClipStack *stack,
                                 float x_1,
                                 float y_1,
                                 float x_2,
                                 float y_2,
                                 CoglMatrixEntry *modelview_entry,
                                 CoglMatrixEntry *projection_entry,
                                 const float *viewport);

CoglClipStack *
_cogl_clip_stack_push_primitive (CoglClipStack *stack,
                                 CoglPrimitive *primitive,
                                 float bounds_x1,
                                 float bounds_y1,
                                 float bounds_x2,
                                 float bounds_y2,
                                 CoglMatrixEntry *modelview_entry,
                                 CoglMatrixEntry *projection_entry,
                                 const float *viewport);

CoglClipStack *
_cogl_clip_stack_pop (CoglClipStack *stack);

void
_cogl_clip_stack_get_bounds (CoglClipStack *stack,
                             int *scissor_x0,
                             int *scissor_y0,
                             int *scissor_x1,
                             int *scissor_y1);

void
_cogl_clip_stack_flush (CoglClipStack *stack,
                        CoglFramebuffer *framebuffer);

CoglClipStack *
_cogl_clip_stack_ref (CoglClipStack *stack);

void
_cogl_clip_stack_unref (CoglClipStack *stack);

#endif /* __COGL_CLIP_STACK_H */
