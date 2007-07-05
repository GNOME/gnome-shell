/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 * SECTION:clutter-group
 * @short_description: Actor class containing multiple children.
 * actors.
 *
 * A #ClutterGroup is an Actor which contains multiple child actors positioned
 * relative to the #ClutterGroup position. Other operations such as scaling, 
 * rotating and clipping of the group will child actors.
 *
 * A #ClutterGroup's size is defined by the size and position of it
 * it children. Resize requests via parent #ClutterActor API will be
 * ignored.
 */

#include "config.h"
#include <stdarg.h>

#include "clutter-group.h"

#include "clutter-container.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-marshal.h"
#include "clutter-enum-types.h"

#include "cogl.h"

enum
{
  ADD,
  REMOVE,

  LAST_SIGNAL
};

static guint group_signals[LAST_SIGNAL] = { 0 };

static void clutter_container_iface_init (ClutterContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterGroup,
                         clutter_group,
                         CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init));

#define CLUTTER_GROUP_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_GROUP, ClutterGroupPrivate))

struct _ClutterGroupPrivate
{
  GList *children;
};


static void
clutter_group_paint (ClutterActor *actor)
{
  ClutterGroup *self = CLUTTER_GROUP(actor);
  GList        *child_item;

  CLUTTER_NOTE (PAINT, "ClutterGroup paint enter");

  cogl_push_matrix();

  for (child_item = self->priv->children;
       child_item != NULL;
       child_item = child_item->next)
    {
      ClutterActor *child = child_item->data;

      g_assert (child != NULL);

      if (CLUTTER_ACTOR_IS_MAPPED (child))
	clutter_actor_paint (child);
    }

  cogl_pop_matrix();
  
  CLUTTER_NOTE (PAINT, "ClutterGroup paint leave");
}

static void                  
clutter_group_pick (ClutterActor       *actor, 
		    const ClutterColor *color)
{
  /* Just forward to the paint call which in turn will trigger 
   * the child actors also getting 'picked'. To make ourselves
   * 'sensitive' to clicks we could also paint a bounding rect
   * but this is not currently done.  
  */
  clutter_group_paint (actor);
}


static void
clutter_group_request_coords (ClutterActor        *self,
			      ClutterActorBox     *box)
{
  ClutterActorBox cbox;

  clutter_actor_query_coords (self, &cbox);

  /* Only positioning works.
   * Sizing requests fail, use scale() instead 
  */
  box->x2 = box->x1 + (cbox.x2 - cbox.x1);
  box->y2 = box->y1 + (cbox.y2 - cbox.y1);
}

static void
clutter_group_query_coords (ClutterActor        *self,
			    ClutterActorBox     *box)
{
  ClutterGroupPrivate *priv;
  GList               *child_item;

  priv = CLUTTER_GROUP(self)->priv;

  child_item = priv->children;

  /* FIXME: Cache these values */
  box->x2 = box->x1;
  box->y2 = box->y1;
  
  if (child_item)
    {
      do 
	{
	  ClutterActor *child = CLUTTER_ACTOR(child_item->data);

	  /* Once added we include in sizing - doesn't matter if visible */
	  /* if (CLUTTER_ACTOR_IS_VISIBLE (child)) */
	    {
	      ClutterActorBox cbox;
	      
	      clutter_actor_query_coords (child, &cbox);
	      
	      /* Ignore any children with offscreen ( negaive )
               * positions.
	       *
               * Also x1 and x2 will be set by parent caller.
               *
               * FIXME: this assumes that children are not rotated in anyway.
	       */
	      if (box->x2 - box->x1 < cbox.x2)
		box->x2 = cbox.x2 + box->x1;

	      if (box->y2 - box->y1 < cbox.y2)
		box->y2 = cbox.y2 + box->y1;
	    }
	}
      while ((child_item = g_list_next(child_item)) != NULL);
    }
}

static void 
clutter_group_dispose (GObject *object)
{
  ClutterGroup *self = CLUTTER_GROUP (object);
  ClutterGroupPrivate *priv = self->priv;

  if (priv->children)
    {
      g_list_foreach (priv->children, (GFunc) clutter_actor_destroy, NULL);
      priv->children = NULL;
    }
  
  G_OBJECT_CLASS (clutter_group_parent_class)->dispose (object);
}

static void
clutter_group_real_show_all (ClutterActor *actor)
{
  clutter_container_foreach (CLUTTER_CONTAINER (actor),
                             CLUTTER_CALLBACK (clutter_actor_show),
                             NULL);
  clutter_actor_show (actor);
}

static void
clutter_group_real_hide_all (ClutterActor *actor)
{
  clutter_actor_hide (actor);
  clutter_container_foreach (CLUTTER_CONTAINER (actor),
                             CLUTTER_CALLBACK (clutter_actor_hide),
                             NULL);
}

static void
clutter_group_real_add (ClutterContainer *container,
                        ClutterActor     *actor)
{
  ClutterGroup *group = CLUTTER_GROUP (container);
  ClutterGroupPrivate *priv = group->priv;

  g_object_ref (actor);
  
  /* the old ClutterGroup::add signal was emitted before the
   * actor was added to the group, so that the class handler
   * would actually add it. we need to emit the ::add signal
   * here so that handlers expecting it will not freak out.
   */
  g_signal_emit (group, group_signals[ADD], 0, actor);

  priv->children = g_list_append (priv->children, actor);
  clutter_actor_set_parent (actor, CLUTTER_ACTOR (group));

  g_signal_emit_by_name (container, "actor-added", actor);
  
  clutter_group_sort_depth_order (group); 

  g_object_unref (actor);
}

static void
clutter_group_real_remove (ClutterContainer *container,
                           ClutterActor     *actor)
{
  ClutterGroup *group = CLUTTER_GROUP (container);
  ClutterGroupPrivate *priv = group->priv;

  g_object_ref (actor);
  
  /* the old ClutterGroup::remove signal was emitted before the
   * actor was removed from the group. see the comment in
   * clutter_group_real_add() above for why we need to emit ::remove
   * here and not later
   */
  g_signal_emit (group, group_signals[REMOVE], 0, actor);

  priv->children = g_list_remove (priv->children, actor);
  clutter_actor_unparent (actor);

  /* at this point, the actor passed to the "actor-removed" signal
   * handlers is not parented anymore to the container but since we
   * are holding a reference on it, it's still valid
   */
  g_signal_emit_by_name (container, "actor-removed", actor);
  
  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (group)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (group));

  g_object_unref (actor);
}

static void
clutter_group_real_foreach (ClutterContainer *container,
                            ClutterCallback   callback,
                            gpointer          user_data)
{
  ClutterGroup *group = CLUTTER_GROUP (container);
  ClutterGroupPrivate *priv = group->priv;
  GList *l;

  for (l = priv->children; l; l = l->next)
    (* callback) (CLUTTER_ACTOR (l->data), user_data);
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = clutter_group_real_add;
  iface->remove = clutter_group_real_remove;
  iface->foreach = clutter_group_real_foreach;
}

static void
clutter_group_class_init (ClutterGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  object_class->dispose = clutter_group_dispose;
  
  actor_class->paint           = clutter_group_paint;
  actor_class->pick            = clutter_group_pick;
  actor_class->show_all        = clutter_group_real_show_all;
  actor_class->hide_all        = clutter_group_real_hide_all;
  actor_class->request_coords  = clutter_group_request_coords;
  actor_class->query_coords    = clutter_group_query_coords;

  /**
   * ClutterGroup::add:
   * @group: the #ClutterGroup that received the signal
   * @actor: the actor added to the group
   *
   * The ::add signal is emitted each time an actor has been added
   * to the group.
   *
   * @Deprecated: 0.4: This signal is deprecated, you should connect
   *   to the ClutterContainer::actor-added signal instead.
   */
  group_signals[ADD] =
    g_signal_new ("add",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ClutterGroupClass, add),
		  NULL, NULL,
		  clutter_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_ACTOR);
  /**
   * ClutterGroup::remove:
   * @group: the #ClutterGroup that received the signal
   * @actor: the actor added to the group
   *
   * The ::remove signal is emitted each time an actor has been removed
   * from the group
   *
   * @Deprecated: 0.4: This signal is deprecated, you should connect
   *   to the ClutterContainer::actor-removed signal instead
   */
  group_signals[REMOVE] =
    g_signal_new ("remove",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ClutterGroupClass, remove),
		  NULL, NULL,
		  clutter_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_ACTOR);

  g_type_class_add_private (object_class, sizeof (ClutterGroupPrivate));
}

static void
clutter_group_init (ClutterGroup *self)
{
  self->priv = CLUTTER_GROUP_GET_PRIVATE (self);
}

/**
 * clutter_group_new:
 *
 * Create a new  #ClutterGroup instance.
 *
 * returns a new #ClutterGroup
 **/
ClutterActor *
clutter_group_new (void)
{
  return g_object_new (CLUTTER_TYPE_GROUP, NULL);
}

/**
 * clutter_group_add:
 * @group: A #ClutterGroup
 * @actor: A #ClutterActor 
 *
 * Adds a new child #ClutterActor to the #ClutterGroup.
 *
 * @Deprecated: 0.4: This function is obsolete, use
 *   clutter_container_add_actor() instead.
 */
void
clutter_group_add (ClutterGroup *group,
                   ClutterActor *actor)
{
  clutter_container_add_actor (CLUTTER_CONTAINER (group), actor);
}

/**
 * clutter_group_add_many_valist:
 * @group: a #ClutterGroup
 * @first_actor: the #ClutterActor actor to add to the group
 * @var_args: the actors to be added
 *
 * Similar to clutter_group_add_many() but using a va_list.  Use this
 * function inside bindings.
 *
 * @Deprecated: 0.4: This function is obsolete, use
 *   clutter_container_add_valist() instead.
 */
void
clutter_group_add_many_valist (ClutterGroup *group,
			       ClutterActor *first_actor,
			       va_list       var_args)
{
  clutter_container_add_valist (CLUTTER_CONTAINER (group), first_actor, var_args);
}

/**
 * clutter_group_add_many:
 * @group: A #ClutterGroup
 * @first_actor: the #ClutterActor actor to add to the group
 * @Varargs: additional actors to add to the group
 *
 * Adds a NULL-terminated list of actors to a group.  This function is
 * equivalent to calling clutter_group_add() for each member of the list.
 *
 * @Deprecated: 0.4: This function is obsolete, use clutter_container_add()
 *   instead.
 */
void
clutter_group_add_many (ClutterGroup *group,
		        ClutterActor *first_actor,
			...)
{
  va_list args;

  va_start (args, first_actor);
  clutter_container_add_valist (CLUTTER_CONTAINER (group), first_actor, args);
  va_end (args);
}

/**
 * clutter_group_remove
 * @group: A #ClutterGroup
 * @actor: A #ClutterActor 
 *
 * Removes a child #ClutterActor from the parent #ClutterGroup.
 *
 * @Deprecated: 0.4: This function is obsolete, use
 *   clutter_container_remove_actor() instead.
 */
void
clutter_group_remove (ClutterGroup *group,
		      ClutterActor *actor)
{
  clutter_container_remove_actor (CLUTTER_CONTAINER (group), actor);
}

/**
 * clutter_group_remove_all:
 * @group: A #ClutterGroup
 *
 * Removes all children actors from the #ClutterGroup.
 */
void
clutter_group_remove_all (ClutterGroup *group)
{
  GList *children;

  g_return_if_fail (CLUTTER_IS_GROUP (group));

  children = group->priv->children;
  while (children)
    {
      ClutterActor *child = children->data;
      children = children->next;

      clutter_container_remove_actor (CLUTTER_CONTAINER (group), child);
    }
}

/**
 * clutter_group_get_children:
 * @self: A #ClutterGroup
 * 
 * Get a list containing all actors contained in the group.
 * 
 * Return value: A list of #ClutterActors. You  should free the returned
 *   list using g_list_free() when finished using it.
 *
 * @Deprecated: 0.4: This function is obsolete, use
 *   clutter_container_get_children() instead.
 */
GList*
clutter_group_get_children (ClutterGroup *self)
{
  return clutter_container_get_children (CLUTTER_CONTAINER (self));
}

/**
 * clutter_group_get_n_children:
 * @self: A #ClutterGroup
 *
 * Gets the number of actors held in the group.
 * 
 * Return value: The number of child actors held in the group.
 * 
 * Since: 0.2
 **/
gint
clutter_group_get_n_children (ClutterGroup *self)
{
  g_return_val_if_fail (CLUTTER_IS_GROUP (self), 0);

  return g_list_length (self->priv->children);
}

/**
 * clutter_group_get_nth_child:
 * @self: A #ClutterGroup
 * @index: the position of the requested actor. 
 * 
 * Gets a groups child held at position index in stack.
 *
 * Return value: A Clutter actor or NULL if index is invalid.
 *
 * Since: 0.2
 **/
ClutterActor *
clutter_group_get_nth_child (ClutterGroup *self,
			     gint          index)
{
  g_return_val_if_fail (CLUTTER_IS_GROUP (self), NULL);

  return g_list_nth_data (self->priv->children, index);
}

/**
 * clutter_group_find_child_by_id:
 * @self: A #ClutterGroup
 * @id: A unique #Clutteractor ID
 *
 * Finds a child actor of a group by its unique ID. Search recurses
 * into any child groups.
 *
 * Returns: the #ClutterActor if found, or NULL.
 */
ClutterActor *
clutter_group_find_child_by_id (ClutterGroup *self,
				guint         id)
{
  ClutterActor *actor = NULL;
  GList *l;

  g_return_val_if_fail (CLUTTER_IS_GROUP (self), NULL);

  if (clutter_actor_get_id (CLUTTER_ACTOR (self)) == id)
    return CLUTTER_ACTOR (self);

  for (l = self->priv->children; l; l = l->next)
    {
      ClutterActor *child = l->data;

      if (clutter_actor_get_id (child) == id)
        {
          actor = child;
          break;
        }

      if (CLUTTER_IS_GROUP (child))
        {
          actor = clutter_group_find_child_by_id (CLUTTER_GROUP (child), id);
          if (actor)
            break;
	}
    }

  return actor;
}

/**
 * clutter_group_raise:
 * @self: a #ClutterGroup
 * @actor: a #ClutterActor
 * @sibling: a #ClutterActor
 *
 * FIXME
 */
void
clutter_group_raise (ClutterGroup   *self,
		     ClutterActor *actor, 
		     ClutterActor *sibling)
{
  ClutterGroupPrivate *priv;
  gint                 pos;

  g_return_if_fail (actor != sibling);

  priv = self->priv;

  pos = g_list_index (priv->children, actor) + 1;

  priv->children = g_list_remove (priv->children, actor); 

  if (sibling == NULL)
    {
      GList *last_item;

      /* Raise top */
      last_item = g_list_last (priv->children);

      if (last_item)
	sibling = last_item->data;
      
      priv->children = g_list_append (priv->children, actor);
    }
  else
    {
      priv->children = g_list_insert (priv->children, actor, pos);
    }

  /* set Z ordering a value below, this will then call sort
   * as values are equal ordering shouldn't change but Z
   * values will be correct.
   * FIXME: optimise
   */
  if ( sibling &&
       clutter_actor_get_depth(sibling) != clutter_actor_get_depth (actor))
    clutter_actor_set_depth (actor,
			     clutter_actor_get_depth (sibling));

}

/**
 * clutter_group_lower:
 * @self: a #ClutterGroup
 * @actor: a #ClutterActor
 * @sibling: a #ClutterActor
 *
 * FIXME
 */
void
clutter_group_lower (ClutterGroup   *self,
		     ClutterActor *actor, 
		     ClutterActor *sibling)
{
  ClutterGroupPrivate *priv;
  gint               pos;

  g_return_if_fail (actor != sibling);

  priv = self->priv;

  pos = g_list_index (priv->children, actor) - 1;

  priv->children = g_list_remove (priv->children, actor); 

  if (sibling == NULL)
    {
      GList *last_item;
      /* Raise top */
      last_item = g_list_first (priv->children);

      if (last_item)
	sibling = last_item->data;

      priv->children = g_list_prepend (priv->children, actor);
    }
  else
    priv->children = g_list_insert (priv->children, actor, pos);

  /* See comment in group_raise for this */
  if (sibling &&
      clutter_actor_get_depth(sibling) != clutter_actor_get_depth(actor))
    clutter_actor_set_depth (actor,
			     clutter_actor_get_depth(sibling));
}

static gint 
sort_z_order (gconstpointer a,
              gconstpointer b)
{
  if (clutter_actor_get_depth (CLUTTER_ACTOR(a)) 
         == clutter_actor_get_depth (CLUTTER_ACTOR(b))) 
    return 0;

  if (clutter_actor_get_depth (CLUTTER_ACTOR(a)) 
        > clutter_actor_get_depth (CLUTTER_ACTOR(b))) 
    return 1;

  return -1;
}

/**
 * clutter_group_sort_depth_order:
 * @self: A #ClutterGroup
 *
 * Sorts a #ClutterGroup's children by there depth value.  
 * This function should not be used by applications. 
 **/
void
clutter_group_sort_depth_order (ClutterGroup *self)
{
  ClutterGroupPrivate *priv;

  g_return_if_fail (CLUTTER_IS_GROUP(self));

  priv = self->priv;

  priv->children = g_list_sort (priv->children, sort_z_order);

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR(self)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR(self));
}
