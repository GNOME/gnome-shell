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
 * @short_description: Base class for actors which contain multiple child
 * actors.
 *
 * #ClutterGroup is an Actor which can contain multiple child actors.
 */

#include "config.h"
#include <stdarg.h>

#include "clutter-group.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-enum-types.h"

enum
{
  ADD,
  REMOVE,

  LAST_SIGNAL
};

static guint group_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ClutterGroup, clutter_group, CLUTTER_TYPE_ACTOR);

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

  glPushMatrix();

  /* Translate if parent ( i.e not stage window ).
  */
  if (clutter_actor_get_parent (actor) != NULL)
    {
      ClutterGeometry geom;      

      clutter_actor_get_geometry (actor, &geom);

      if (geom.x != 0 && geom.y != 0)
	glTranslatef(geom.x, geom.y, 0.0);

    }

  for (child_item = self->priv->children;
       child_item != NULL;
       child_item = child_item->next)
    {
      ClutterActor *child = child_item->data;

      g_assert (child != NULL);

      if (CLUTTER_ACTOR_IS_MAPPED (child))
	clutter_actor_paint (child);
    }

  glPopMatrix();
}

static void
clutter_group_request_coords (ClutterActor    *self,
			      ClutterActorBox *box)
{
  ClutterGroup   *group = CLUTTER_GROUP(self);
  guint           cwidth, cheight, width ,height;
  ClutterActorBox cbox;

  clutter_actor_allocate_coords (self, &cbox);

  cwidth  = cbox.x2 - cbox.x1;
  cheight = cbox.y2 - cbox.y1;

  /* g_print("cbox x2: %i x1 %i\n", cbox.x2, cbox.x1); */

  width  = box->x2 - box->x1;
  height = box->y2 - box->y1;

  /* FIXME: below needs work */
  if (cwidth != width || cheight != height)
    {
      GList          *child_item;

      for (child_item = group->priv->children;
	   child_item != NULL;
	   child_item = child_item->next)
	{
	  ClutterActor   *child = child_item->data;
	  ClutterActorBox tbox;
	  gint            nx, ny;
	  gint            twidth, theight, nwidth, nheight; 

	  g_assert (child != NULL);

	  clutter_actor_allocate_coords (child, &tbox);

	  twidth  = tbox.x2 - tbox.x1;
	  theight = tbox.y2 - tbox.y1;
	  
	  /* g_print("getting ps %ix%i\n", tbox.x1, tbox.y1); */

	  nwidth = ( width * twidth ) / cwidth;
	  nheight = ( height * theight ) / cheight;

	  nx = ( nwidth  * tbox.x1 ) / twidth ;

	  /* g_print("n: %i t %i  x1: %i\n", nwidth, twidth, tbox.x1); */

	  ny = ( nheight  * tbox.y1 ) / theight;

	  /* g_print("n: %i t %i  x1: %i\n", nheight, theight, tbox.y1); */

	  clutter_actor_set_position (child, nx, ny);
	  clutter_actor_set_size (child, nwidth, height);

	  /* g_print("size to +%i+%x %ix%i\n", nx, ny, nwidth, nheight); */
	}
    }
}

static void
clutter_group_allocate_coords (ClutterActor    *self,
			       ClutterActorBox *box)
{
  ClutterGroupPrivate *priv;
  GList               *child_item;

  priv = CLUTTER_GROUP(self)->priv;

  child_item = priv->children;

  if (child_item)
    {
      do 
	{
	  ClutterActor *child = CLUTTER_ACTOR(child_item->data);
	      
	  /* if (CLUTTER_ACTOR_IS_VISIBLE (child)) */
	    {
	      ClutterActorBox cbox;

	      clutter_actor_allocate_coords (child, &cbox);
	      
	      /*
	      if (box->x1 == 0 || cbox.x1 < box->x1)
		box->x1 = cbox.x1;

	      if (box->y1 == 0 || cbox.y1 < box->y1)
		box->y1 = cbox.y1;
	      */		

	      if (box->x2 == 0 || cbox.x2 > box->x2)
		box->x2 = cbox.x2;

	      if (box->y2 == 0 || cbox.y2 < box->y2)
		box->y2 = cbox.y2;
	    }
	}
      while ((child_item = g_list_next(child_item)) != NULL);
    }
}

static void 
clutter_group_dispose (GObject *object)
{
  ClutterGroup *self = CLUTTER_GROUP(object); 

  if (self->priv)
    {
      /* FIXME: Do we need to actually free anything here ?
       *        Children ref us so this wont get called till
       *        they are all removed.
      */
    }

  G_OBJECT_CLASS (clutter_group_parent_class)->dispose (object);
}


static void 
clutter_group_finalize (GObject *object)
{
  ClutterGroup *group = CLUTTER_GROUP (object);

  /* XXX - if something survives ::dispose then there's something
   * wrong; but, at least, we won't leak stuff around.
   */
  if (group->priv->children)
    {
      g_list_foreach (group->priv->children, (GFunc) g_object_unref, NULL);
      g_list_free (group->priv->children);
    }
  
  G_OBJECT_CLASS (clutter_group_parent_class)->finalize (object);
}

static void
clutter_group_class_init (ClutterGroupClass *klass)
{
  GObjectClass        *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint      = clutter_group_paint;
  /*
  actor_class->show       = clutter_group_show_all;
  actor_class->hide       = clutter_group_hide_all;
  */
  actor_class->request_coords  = clutter_group_request_coords;
  actor_class->allocate_coords = clutter_group_allocate_coords;

  /* GObject */
  object_class->finalize     = clutter_group_finalize;
  object_class->dispose      = clutter_group_dispose;

  group_signals[ADD] =
    g_signal_new ("add",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterGroupClass, add),
		  NULL, NULL,
		  clutter_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_ACTOR);

  group_signals[REMOVE] =
    g_signal_new ("remove",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
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
ClutterGroup*
clutter_group_new (void)
{
  return g_object_new (CLUTTER_TYPE_GROUP, NULL);
}

/**
 * clutter_group_get_children:
 * @self: A #ClutterGroup
 * 
 * Get a list containing all actors contained in the group.
 * 
 * Return value: A GList containing child #ClutterActors.
 **/
GList*
clutter_group_get_children (ClutterGroup *self)
{
  /* FIXME: remane get_actors() */
  g_return_val_if_fail (CLUTTER_IS_GROUP (self), NULL);

  return g_list_copy(self->priv->children);
}

/**
 * clutter_group_forall:
 * @self: A #ClutterGroup
 * @callback: a callback
 * @user_data: callback user data 
 * 
 * Invokes callback on each child of the group.
 **/
void
clutter_group_foreach (ClutterGroup      *self,
		       ClutterCallback   callback,
		       gpointer          user_data)
{
  ClutterActor *child;
  GList          *children;

  g_return_if_fail (CLUTTER_IS_GROUP (self));
  g_return_if_fail (callback != NULL);

  children = self->priv->children;

  while (children)
    {
      child = children->data;

      (*callback) (child, user_data);

      children = g_list_next(children);
    }
}

/**
 * clutter_group_show_all:
 * @self: A #ClutterGroup
 * 
 * Show all child actors of the group. Note, does not recurse.
 **/
void
clutter_group_show_all (ClutterGroup *self)
{
  g_return_if_fail (CLUTTER_IS_GROUP (self));

  clutter_actor_show(CLUTTER_ACTOR(self));

  g_list_foreach (self->priv->children,
		  (GFunc)clutter_actor_show,
		  NULL);
}

/**
 * clutter_group_hide_all:
 * @self: A #ClutterGroup
 * 
 * Hide all child actors of the group. Note, does not recurse.
 **/
void
clutter_group_hide_all (ClutterGroup *self)
{
  g_return_if_fail (CLUTTER_IS_GROUP (self));

  clutter_actor_hide(CLUTTER_ACTOR(self));

  g_list_foreach (self->priv->children,
		  (GFunc)clutter_actor_hide,
		  NULL);
}

/**
 * clutter_group_add:
 * @self: A #ClutterGroup
 * @actor: A #ClutterActor 
 *
 * Adds a new child #ClutterActor to the #ClutterGroup.
 **/
void
clutter_group_add (ClutterGroup   *self,
		   ClutterActor *actor)
{
  ClutterActor *parent;
  
  g_return_if_fail (CLUTTER_IS_GROUP (self));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  parent = clutter_actor_get_parent (actor);
  if (parent)
    {
      g_warning ("Attempting to add actor of type `%s' to a "
		 "group of type `%s', but the actor has already "
		 "a parent of type `%s'.",
		 g_type_name (G_OBJECT_TYPE (actor)),
		 g_type_name (G_OBJECT_TYPE (self)),
		 g_type_name (G_OBJECT_TYPE (parent)));
      return;
    }

  self->priv->children = g_list_append (self->priv->children, actor);
  /* below refs */
  clutter_actor_set_parent (actor, CLUTTER_ACTOR(self));
  g_object_ref (actor);

  clutter_group_sort_depth_order (self); 

  g_signal_emit (self, group_signals[ADD], 0, actor);
}

/**
 * clutter_group_add_many_valist:
 * @group: a #ClutterGroup
 * @first_actor: the #ClutterActor actor to add to the group
 * @args: the actors to be added
 *
 * Similar to clutter_group_add_many() but using a va_list.  Use this
 * function inside bindings.
 */
void
clutter_group_add_many_valist (ClutterGroup   *group,
			       ClutterActor *first_actor,
			       va_list         args)
{
  ClutterActor *actor;
  
  g_return_if_fail (CLUTTER_IS_GROUP (group));
  g_return_if_fail (CLUTTER_IS_ACTOR (first_actor));

  actor = first_actor;
  while (actor)
    {
      clutter_group_add (group, actor);
      actor = va_arg (args, ClutterActor *);
    }
}

/**
 * clutter_group_add_many:
 * @self: A #ClutterGroup
 * @first_actor: the #ClutterActor actor to add to the group
 * @Varargs: additional actors to add to the group
 *
 * Adds a NULL-terminated list of actors to a group.  This function is
 * equivalent to calling clutter_group_add() for each member of the list.
 */
void
clutter_group_add_many (ClutterGroup   *self,
		        ClutterActor *first_actor,
			...)
{
  va_list args;

  va_start (args, first_actor);
  clutter_group_add_many_valist (self, first_actor, args);
  va_end (args);
}

/**
 * clutter_group_remove
 * @self: A #ClutterGroup
 * @actor: A #ClutterActor 
 *
 * Remove a child #ClutterActor from the #ClutterGroup.
 **/
void
clutter_group_remove (ClutterGroup *self,
		      ClutterActor *actor)
{
  ClutterActor *parent;
  
  g_return_if_fail (CLUTTER_IS_GROUP (self));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  parent = clutter_actor_get_parent (actor);
  if (parent != CLUTTER_ACTOR (self))
    {
      g_warning ("Attempting to remove actor of type `%s' from "
		 "group of class `%s', but the group is not the "
		 "actor's parent.",
		 g_type_name (G_OBJECT_TYPE (actor)),
		 g_type_name (G_OBJECT_TYPE (self)));
      return;
    }

  self->priv->children = g_list_remove (self->priv->children, actor);
  clutter_actor_set_parent (actor, NULL);
  
  g_signal_emit (self, group_signals[REMOVE], 0, actor);
  g_object_unref (actor);
}

/**
 * clutter_group_remove_all:
 * @self: A #ClutterGroup
 *
 * Remove all child #ClutterActor from the #ClutterGroup.
 */
void
clutter_group_remove_all (ClutterGroup *self)
{
  GList *child_item;

  g_return_if_fail (CLUTTER_IS_GROUP (self));

  child_item = self->priv->children;

  if (child_item)
    {
      do 
	{
	  clutter_group_remove (self, CLUTTER_ACTOR(child_item->data));
	}
      while ((child_item = g_list_next(child_item)) != NULL);
    }
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
  ClutterActor *actor = NULL, *inner_actor;
  GList          *child_item;

  g_return_val_if_fail (CLUTTER_IS_GROUP (self), NULL);

  if (clutter_actor_get_id (CLUTTER_ACTOR(self)) == id)
    return CLUTTER_ACTOR(self);

  child_item = self->priv->children;

  if (child_item)
    {
      do 
	{
	  inner_actor = (ClutterActor*)child_item->data;

	  if (clutter_actor_get_id (inner_actor) == id)
	    return inner_actor;
	  
	  if (CLUTTER_IS_GROUP(inner_actor))
	    {
	      actor = 
		clutter_group_find_child_by_id (CLUTTER_GROUP(inner_actor), 
						id);
	      if (actor)
		return actor;
	    }

	}
      while ((child_item = g_list_next(child_item)) != NULL);
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
  if (clutter_actor_get_depth(sibling) != clutter_actor_get_depth(actor))
    clutter_actor_set_depth (actor,
			       clutter_actor_get_depth(sibling));

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
      sibling = last_item->data;

      priv->children = g_list_prepend (priv->children, actor);
    }
  else
    priv->children = g_list_insert (priv->children, actor, pos);

  /* See comment in group_raise for this */
  if (clutter_actor_get_depth(sibling) != clutter_actor_get_depth(actor))
    clutter_actor_set_depth (actor,
			       clutter_actor_get_depth(sibling));
}

static gint 
sort_z_order (gconstpointer a, gconstpointer b)
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
 * clutter_group_sort_z_order:
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
