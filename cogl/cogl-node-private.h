/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
