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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-util.h"
#include "cogl-node-private.h"

void
_cogl_pipeline_node_init (CoglNode *node)
{
  node->parent = NULL;
  _cogl_list_init (&node->children);
}

void
_cogl_pipeline_node_set_parent_real (CoglNode *node,
                                     CoglNode *parent,
                                     CoglNodeUnparentVFunc unparent,
                                     CoglBool take_strong_reference)
{
  /* NB: the old parent may indirectly be keeping the new parent alive
   * so we have to ref the new parent before unrefing the old.
   *
   * Note: we take a reference here regardless of
   * take_strong_reference because weak children may need special
   * handling when the parent disposes itself which relies on a
   * consistent link to all weak nodes. Once the node is linked to its
   * parent then we remove the reference at the end if
   * take_strong_reference == FALSE. */
  cogl_object_ref (parent);

  if (node->parent)
    unparent (node);

  _cogl_list_insert (&parent->children, &node->link);

  node->parent = parent;
  node->has_parent_reference = take_strong_reference;

  /* Now that there is a consistent parent->child link we can remove
   * the parent reference if no reference was requested. If it turns
   * out that the new parent was only being kept alive by the old
   * parent then it will be disposed of here. */
  if (!take_strong_reference)
    cogl_object_unref (parent);
}

void
_cogl_pipeline_node_unparent_real (CoglNode *node)
{
  CoglNode *parent = node->parent;

  if (parent == NULL)
    return;

  _COGL_RETURN_IF_FAIL (!_cogl_list_empty (&parent->children));

  _cogl_list_remove (&node->link);

  if (node->has_parent_reference)
    cogl_object_unref (parent);

  node->parent = NULL;
}

void
_cogl_pipeline_node_foreach_child (CoglNode *node,
                                   CoglNodeChildCallback callback,
                                   void *user_data)
{
  CoglNode *child, *next;

  _cogl_list_for_each_safe (child, next, &node->children, link)
    callback (child, user_data);
}


