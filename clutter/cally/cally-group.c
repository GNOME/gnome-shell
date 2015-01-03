/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2008 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 *
 * Based on GailContainer from GAIL
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:cally-group
 * @Title: CallyGroup
 * @short_description: Implementation of the ATK interfaces for a #ClutterGroup
 * @see_also: #ClutterGroup
 *
 * #CallyGroup implements the required ATK interfaces of #ClutterGroup
 * In particular it exposes each of the Clutter actors contained in the
 * group.
 */

#include "config.h"

#include "cally-group.h"
#include "cally-actor-private.h"

static gint       cally_group_get_n_children  (AtkObject *obj);
static AtkObject* cally_group_ref_child       (AtkObject *obj,
                                              gint       i);
static void       cally_group_real_initialize (AtkObject *obj,
                                              gpointer   data);

G_DEFINE_TYPE (CallyGroup, cally_group, CALLY_TYPE_ACTOR)

static void
cally_group_class_init (CallyGroupClass *klass)
{
/*   GObjectClass   *gobject_class = G_OBJECT_CLASS (klass); */
  AtkObjectClass *class         = ATK_OBJECT_CLASS (klass);

  class->get_n_children = cally_group_get_n_children;
  class->ref_child      = cally_group_ref_child;
  class->initialize     = cally_group_real_initialize;
}

static void
cally_group_init (CallyGroup      *group)
{
  /* nothing to do yet */
}

/**
 * cally_group_new:
 * @actor: a #ClutterGroup
 *
 * Creates a #CallyGroup for @actor
 *
 * Return value: the newly created #CallyGroup
 *
 * Since: 1.4
 */
AtkObject *
cally_group_new (ClutterActor *actor)
{
  GObject   *object     = NULL;
  AtkObject *accessible = NULL;

  g_return_val_if_fail (CLUTTER_IS_GROUP (actor), NULL);

  object = g_object_new (CALLY_TYPE_GROUP, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, actor);

  return accessible;
}

static gint
cally_group_get_n_children (AtkObject *obj)
{
  ClutterActor *actor = NULL;
  gint          count = 0;

  g_return_val_if_fail (CALLY_IS_GROUP (obj), count);

  actor = CALLY_GET_CLUTTER_ACTOR (obj);

  if (actor == NULL) /* defunct */
    return 0;

  g_return_val_if_fail (CLUTTER_IS_GROUP(actor), count);

  count = clutter_actor_get_n_children (actor);

  return count;
}

static AtkObject*
cally_group_ref_child (AtkObject *obj,
                       gint       i)
{
  AtkObject    *accessible = NULL;
  ClutterActor *actor      = NULL;
  ClutterActor *child      = NULL;

  g_return_val_if_fail (CALLY_IS_GROUP (obj), NULL);
  g_return_val_if_fail ((i >= 0), NULL);

  actor = CALLY_GET_CLUTTER_ACTOR (obj);

  g_return_val_if_fail (CLUTTER_IS_GROUP(actor), NULL);
  child = clutter_actor_get_child_at_index (actor, i);

  if (!child)
    return NULL;

  accessible = clutter_actor_get_accessible (child);

  if (accessible != NULL)
    g_object_ref (accessible);

  return accessible;
}

static void
cally_group_real_initialize (AtkObject *obj,
                            gpointer   data)
{
  ATK_OBJECT_CLASS (cally_group_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_PANEL;
}
