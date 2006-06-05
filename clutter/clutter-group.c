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

G_DEFINE_TYPE (ClutterGroup, clutter_group, CLUTTER_TYPE_ELEMENT);

#define CLUTTER_GROUP_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_GROUP, ClutterGroupPrivate))

struct _ClutterGroupPrivate
{
  GList *children;
};

static void
clutter_group_paint (ClutterElement *element)
{
  ClutterGroup *self = CLUTTER_GROUP(element);
  GList      *child_item;

  child_item = self->priv->children;

  glPushMatrix();

  /* Translate if parent ( i.e not stage window ).
  */
  if (clutter_element_get_parent (element) != NULL)
    {
      ClutterGeometry geom;      

      clutter_element_get_geometry (element, &geom);

      if (geom.x != 0 && geom.y != 0)
	glTranslatef(geom.x, geom.y, 0.0);

    }

#if 0
  for (child_item = self->priv->children;
       child_item != NULL;
       child_item = child_item->next)
    {
      ClutterElement *child = child_item->data;

      g_assert (child != NULL);

      if (CLUTTER_ELEMENT_IS_MAPPED (child))
	clutter_element_paint (child);
    }
#endif
  
  if (child_item)
    {
      do 
	{
	  ClutterElement *child = CLUTTER_ELEMENT(child_item->data);
	      
	  if (CLUTTER_ELEMENT_IS_MAPPED (child))
	    {
	      clutter_element_paint(child);
	    }
	}
      while ((child_item = g_list_next(child_item)) != NULL);
    }

  glPopMatrix();
}

static void
clutter_group_request_coords (ClutterElement    *self,
			      ClutterElementBox *box)
{
  /* FIXME: what to do here ?
   *        o clip if smaller ?
   *        o scale each element ? 
  */
}

static void
clutter_group_allocate_coords (ClutterElement    *self,
			       ClutterElementBox *box)
{
  ClutterGroupPrivate *priv;
  GList               *child_item;

  priv = CLUTTER_GROUP(self)->priv;

  child_item = priv->children;

  if (child_item)
    {
      do 
	{
	  ClutterElement *child = CLUTTER_ELEMENT(child_item->data);
	      
	  if (CLUTTER_ELEMENT_IS_VISIBLE (child))
	    {
	      ClutterElementBox cbox;

	      clutter_element_allocate_coords (child, &cbox);
	      
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
  ClutterElementClass *element_class = CLUTTER_ELEMENT_CLASS (klass);

  element_class->paint      = clutter_group_paint;
  /*
  element_class->show       = clutter_group_show_all;
  element_class->hide       = clutter_group_hide_all;
  */
  element_class->request_coords  = clutter_group_request_coords;
  element_class->allocate_coords = clutter_group_allocate_coords;

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
		  CLUTTER_TYPE_ELEMENT);

  group_signals[REMOVE] =
    g_signal_new ("remove",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterGroupClass, remove),
		  NULL, NULL,
		  clutter_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_ELEMENT);

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
 * Get a list containing all elements contained in the group.
 * 
 * Return value: A GList containing child #ClutterElements.
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
  ClutterElement *child;
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
 * Show all child elements of the group. Note, does not recurse.
 **/
void
clutter_group_show_all (ClutterGroup *self)
{
  g_return_if_fail (CLUTTER_IS_GROUP (self));

  clutter_element_show(CLUTTER_ELEMENT(self));

  g_list_foreach (self->priv->children,
		  (GFunc)clutter_element_show,
		  NULL);
}

/**
 * clutter_group_hide_all:
 * @self: A #ClutterGroup
 * 
 * Hide all child elements of the group. Note, does not recurse.
 **/
void
clutter_group_hide_all (ClutterGroup *self)
{
  g_return_if_fail (CLUTTER_IS_GROUP (self));

  clutter_element_hide(CLUTTER_ELEMENT(self));

  g_list_foreach (self->priv->children,
		  (GFunc)clutter_element_hide,
		  NULL);
}

/**
 * clutter_group_add:
 * @self: A #ClutterGroup
 * @element: A #ClutterElement 
 *
 * Adds a new child #ClutterElement to the #ClutterGroup.
 **/
void
clutter_group_add (ClutterGroup   *self,
		   ClutterElement *element)
{
  ClutterElement *parent;
  
  g_return_if_fail (CLUTTER_IS_GROUP (self));
  g_return_if_fail (CLUTTER_IS_ELEMENT (element));

  parent = clutter_element_get_parent (element);
  if (parent)
    {
      g_warning ("Attempting to add element of type `%s' to a "
		 "group of type `%s', but the element has already "
		 "a parent of type `%s'.",
		 g_type_name (G_OBJECT_TYPE (element)),
		 g_type_name (G_OBJECT_TYPE (self)),
		 g_type_name (G_OBJECT_TYPE (parent)));
      return;
    }

  self->priv->children = g_list_append (self->priv->children, element);
  /* below refs */
  clutter_element_set_parent (element, CLUTTER_ELEMENT(self));
  g_object_ref (element);

  clutter_group_sort_depth_order (self); 

  g_signal_emit (self, group_signals[ADD], 0, element);
}

/**
 * clutter_group_add_manyv:
 * @self: a #ClutterGroup
 * @first_element: the #ClutterElement element to add to the group
 * @args: the elements to be added
 *
 * Similar to clutter_group_add_many() but using a va_list.  Use this
 * function inside bindings.
 */
void
clutter_group_add_many_valist (ClutterGroup   *group,
			       ClutterElement *first_element,
			       va_list         args)
{
  ClutterElement *element;
  
  g_return_if_fail (CLUTTER_IS_GROUP (group));
  g_return_if_fail (CLUTTER_IS_ELEMENT (first_element));

  element = first_element;
  while (element)
    {
      clutter_group_add (group, element);
      element = va_arg (args, ClutterElement *);
    }
}

/**
 * clutter_group_add_many:
 * @self: A #ClutterGroup
 * @first_element: the #ClutterElement element to add to the group
 * @Varargs: additional elements to add to the group
 *
 * Adds a NULL-terminated list of elements to a group.  This function is
 * equivalent to calling clutter_group_add() for each member of the list.
 */
void
clutter_group_add_many (ClutterGroup   *self,
		        ClutterElement *first_element,
			...)
{
  va_list args;

  va_start (args, first_element);
  clutter_group_add_many_valist (self, first_element, args);
  va_end (args);
}

/**
 * clutter_group_remove
 * @self: A #ClutterGroup
 * @element: A #ClutterElement 
 *
 * Remove a child #ClutterElement from the #ClutterGroup.
 **/
void
clutter_group_remove (ClutterGroup   *self,
		      ClutterElement *element)
{
  ClutterElement *parent;
  
  g_return_if_fail (CLUTTER_IS_GROUP (self));
  g_return_if_fail (CLUTTER_IS_ELEMENT (element));

  parent = clutter_element_get_parent (element);
  if (parent != CLUTTER_ELEMENT (self))
    {
      g_warning ("Attempting to remove element of type `%s' from "
		 "group of class `%s', but the group is not the "
		 "element's parent.",
		 g_type_name (G_OBJECT_TYPE (element)),
		 g_type_name (G_OBJECT_TYPE (self)));
      return;
    }

  self->priv->children = g_list_remove (self->priv->children, element);
  clutter_element_set_parent (element, NULL);
  
  g_signal_emit (self, group_signals[REMOVE], 0, element);
  g_object_unref (element);
}

/**
 * clutter_group_remove_all:
 * @self: A #ClutterGroup
 *
 * Remove all child #ClutterElement from the #ClutterGroup.
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
	  clutter_group_remove (self, CLUTTER_ELEMENT(child_item->data));
	}
      while ((child_item = g_list_next(child_item)) != NULL);
    }
}

/**
 * clutter_group_find_child_by_id:
 * @self: A #ClutterGroup
 * @id: A unique #Clutterelement ID
 *
 * Finds a child element of a group by its unique ID. Search recurses
 * into any child groups. 
 */
ClutterElement *
clutter_group_find_child_by_id (ClutterGroup *self,
				guint         id)
{
  ClutterElement *element = NULL, *inner_element;
  GList          *child_item;

  g_return_val_if_fail (CLUTTER_IS_GROUP (self), NULL);

  if (clutter_element_get_id (CLUTTER_ELEMENT(self)) == id)
    return CLUTTER_ELEMENT(self);

  child_item = self->priv->children;

  if (child_item)
    {
      do 
	{
	  inner_element = (ClutterElement*)child_item->data;

	  if (clutter_element_get_id (inner_element) == id)
	    return inner_element;
	  
	  if (CLUTTER_IS_GROUP(inner_element))
	    {
	      element = 
		clutter_group_find_child_by_id (CLUTTER_GROUP(inner_element), 
						id);
	      if (element)
		return element;
	    }

	}
      while ((child_item = g_list_next(child_item)) != NULL);
    }

  return element;
}

/**
 * clutter_group_raise:
 * @self: a #ClutterGroup
 * @element: a #ClutterElement
 * @sibling: a #ClutterElement
 *
 * FIXME
 */
void
clutter_group_raise (ClutterGroup   *self,
		     ClutterElement *element, 
		     ClutterElement *sibling)
{
  ClutterGroupPrivate *priv;
  gint                 pos;

  g_return_if_fail (element != sibling);

  priv = self->priv;

  pos = g_list_index (priv->children, element) + 1;

  priv->children = g_list_remove (priv->children, element); 

  if (sibling == NULL)
    {
      GList *last_item;
      /* Raise top */
      last_item = g_list_last (priv->children);
      sibling = last_item->data;
      priv->children = g_list_append (priv->children, element);
    }
  else
    {
      priv->children = g_list_insert (priv->children, element, pos);
    }

  /* set Z ordering a value below, this will then call sort
   * as values are equal ordering shouldn't change but Z
   * values will be correct.
   * FIXME: optimise
   */
  if (clutter_element_get_depth(sibling) != clutter_element_get_depth(element))
    clutter_element_set_depth (element,
			       clutter_element_get_depth(sibling));

}

/**
 * clutter_group_lower:
 * @self: a #ClutterGroup
 * @element: a #ClutterElement
 * @sibling: a #ClutterElement
 *
 * FIXME
 */
void
clutter_group_lower (ClutterGroup   *self,
		     ClutterElement *element, 
		     ClutterElement *sibling)
{
  ClutterGroupPrivate *priv;
  gint               pos;

  g_return_if_fail (element != sibling);

  priv = self->priv;

  pos = g_list_index (priv->children, element) - 1;

  priv->children = g_list_remove (priv->children, element); 

  if (sibling == NULL)
    {
      GList *last_item;
      /* Raise top */
      last_item = g_list_first (priv->children);
      sibling = last_item->data;

      priv->children = g_list_prepend (priv->children, element);
    }
  else
    priv->children = g_list_insert (priv->children, element, pos);

  /* See comment in group_raise for this */
  if (clutter_element_get_depth(sibling) != clutter_element_get_depth(element))
    clutter_element_set_depth (element,
			       clutter_element_get_depth(sibling));
}

static gint 
sort_z_order (gconstpointer a, gconstpointer b)
{
  if (clutter_element_get_depth (CLUTTER_ELEMENT(a)) 
         == clutter_element_get_depth (CLUTTER_ELEMENT(b))) 
    return 0;

  if (clutter_element_get_depth (CLUTTER_ELEMENT(a)) 
        > clutter_element_get_depth (CLUTTER_ELEMENT(b))) 
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

  if (CLUTTER_ELEMENT_IS_VISIBLE (CLUTTER_ELEMENT(self)))
    clutter_element_queue_redraw (CLUTTER_ELEMENT(self));
}
