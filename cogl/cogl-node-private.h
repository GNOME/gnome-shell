/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_NODE_PRIVATE_H
#define __COGL_NODE_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-list.h"

typedef struct _CoglNode CoglNode;

/* Pipelines and layers represent their state in a tree structure where
 * some of the state relating to a given pipeline or layer may actually
 * be owned by one if is ancestors in the tree. We have a common data
 * type to track the tree heirachy so we can share code... */
struct _CoglNode
{
  /* the parent in terms of class hierarchy, so anything inheriting
   * from CoglNode also inherits from CoglObject. */
  CoglObject _parent;

  /* The parent pipeline/layer */
  CoglNode *parent;

  /* The list entry here contains pointers to the node's siblings */
  CoglList link;

  /* List of children */
  CoglList children;

  /* TRUE if the node took a strong reference on its parent. Weak
   * pipelines for instance don't take a reference on their parent. */
  CoglBool has_parent_reference;
};

#define COGL_NODE(X) ((CoglNode *)(X))

void
_cogl_pipeline_node_init (CoglNode *node);

typedef void (*CoglNodeUnparentVFunc) (CoglNode *node);

void
_cogl_pipeline_node_set_parent_real (CoglNode *node,
                                     CoglNode *parent,
                                     CoglNodeUnparentVFunc unparent,
                                     CoglBool take_strong_reference);

void
_cogl_pipeline_node_unparent_real (CoglNode *node);

typedef CoglBool (*CoglNodeChildCallback) (CoglNode *child, void *user_data);

void
_cogl_pipeline_node_foreach_child (CoglNode *node,
                                   CoglNodeChildCallback callback,
                                   void *user_data);

#endif /* __COGL_NODE_PRIVATE_H */
