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
 *
 * ClutterContainer: Generic actor container interface.
 * Author: Emmanuele Bassi <ebassi@openedhand.com>
 */

#include "config.h"

#include <stdarg.h>
#include <glib-object.h>

#include "clutter-container.h"

#include "clutter-debug.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-enum-types.h"

/**
 * SECTION:clutter-container
 * @short_description: An interface for implementing container actors
 *
 * #ClutterContainer is an interface for writing actors containing other
 * #ClutterActor<!-- -->s. It provides a standard API for adding, removing
 * and iterating on every contained actor.
 *
 * An actor implementing #ClutterContainer is #ClutterGroup.
 *
 * #ClutterContainer is available since Clutter 0.4
 */

enum
{
  ACTOR_ADDED,
  ACTOR_REMOVED,

  LAST_SIGNAL
};

static guint container_signals[LAST_SIGNAL] = { 0, };

static void
clutter_container_base_init (gpointer g_iface)
{
  static gboolean initialised = FALSE;

  if (!initialised)
    {
      GType iface_type = G_TYPE_FROM_INTERFACE (g_iface);
      
      initialised = TRUE;

      /**
       * ClutterContainer::actor-added:
       * @container: the actor which received the signal
       * @actor: the new child that has been added to @container
       *
       * The ::actor-added signal is emitted each time an actor
       * has been added to @container.
       *
       * Since: 0.4
       */
      container_signals[ACTOR_ADDED] =
        g_signal_new ("actor-added",
                      iface_type,
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (ClutterContainerIface, actor_added),
                      NULL, NULL,
                      clutter_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1,
                      CLUTTER_TYPE_ACTOR);
      /**
       * ClutterContainer::actor-removed:
       * @container: the actor which received the signal
       * @actor: the child that has been removed from @container
       *
       * The ::actor-removed signal is emitted each time an actor
       * is removed from @container.
       *
       * Since: 0.4
       */
      container_signals[ACTOR_REMOVED] =
        g_signal_new ("actor-removed",
                      iface_type,
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (ClutterContainerIface, actor_removed),
                      NULL, NULL,
                      clutter_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1,
                      CLUTTER_TYPE_ACTOR);
    }
}

GType
clutter_container_get_type (void)
{
  static GType container_type = 0;

  if (G_UNLIKELY (!container_type))
    {
      GTypeInfo container_info =
      {
        sizeof (ClutterContainerIface),
        clutter_container_base_init,
        NULL, /* iface_finalize */
      };

      container_type = g_type_register_static (G_TYPE_INTERFACE,
                                               "ClutterContainer",
                                               &container_info, 0);

      g_type_interface_add_prerequisite (container_type, CLUTTER_TYPE_ACTOR);
    }

  return container_type;
}

/**
 * clutter_container_add:
 * @container: a #ClutterContainer
 * @first_actor: the first #ClutterActor to add
 * @Varargs: %NULL terminated list of actors to add
 *
 * Adds a list of #ClutterActor<!-- -->s to @container. Each time and
 * actor is added, the "actor-added" signal is emitted. Each actor should
 * be parented to @container, which takes a reference on the actor. You
 * cannot add a #ClutterActor to more than one #ClutterContainer.
 *
 * Since: 0.4
 */
void
clutter_container_add (ClutterContainer *container,
                       ClutterActor     *first_actor,
                       ...)
{
  va_list args;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (first_actor));

  va_start (args, first_actor);
  clutter_container_add_valist (container, first_actor, args);
  va_end (args);
}

/**
 * clutter_container_add_actor:
 * @container: a #ClutterContainer
 * @actor: the first #ClutterActor to add
 *
 * Adds a #ClutterActor to @container. This function will emit the
 * "actor-added" signal is emitted. The actor should be parented to
 * @container. You cannot add a #ClutterActor to more than one
 * #ClutterContainer.
 *
 * Since: 0.4
 */
void
clutter_container_add_actor (ClutterContainer *container,
                             ClutterActor     *actor)
{
  ClutterActor *parent;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  parent = clutter_actor_get_parent (actor);
  if (parent)
    {
      g_warning ("Attempting to add actor of type `%s' to a "
		 "group of type `%s', but the actor has already "
		 "a parent of type `%s'.",
		 g_type_name (G_OBJECT_TYPE (actor)),
		 g_type_name (G_OBJECT_TYPE (container)),
		 g_type_name (G_OBJECT_TYPE (parent)));
      return;
    }

  CLUTTER_CONTAINER_GET_IFACE (container)->add (container, actor);
}

/**
 * clutter_container_add_valist:
 * @container: a #ClutterContainer
 * @first_actor: the first #ClutterActor to add
 * @var_args: list of actors to add, followed by %NULL
 *
 * Alternative va_list version of clutter_container_add().
 *
 * Since: 0.4
 */
void
clutter_container_add_valist (ClutterContainer *container,
                              ClutterActor     *first_actor,
                              va_list           var_args)
{
  ClutterActor *actor;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (first_actor));

  actor = first_actor;
  while (actor)
    {
      clutter_container_add_actor (container, actor);
      actor = va_arg (var_args, ClutterActor*);
    }
}

/**
 * clutter_container_remove:
 * @container: a #ClutterContainer
 * @first_actor: first #ClutterActor to remove
 * @Varargs: a %NULL-terminated list of actors to remove
 *
 * Removes a %NULL terminated list of #ClutterActor<!-- -->s from
 * @container. Each actor should be unparented, so if you want to keep it
 * around you must hold a reference to it yourself, using g_object_ref().
 * Each time an actor is removed, the "actor-removed" signal is
 * emitted by @container.
 *
 * Since: 0.4
 */
void
clutter_container_remove (ClutterContainer *container,
                          ClutterActor     *first_actor,
                          ...)
{
  va_list var_args;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (first_actor));

  va_start (var_args, first_actor);
  clutter_container_remove_valist (container, first_actor, var_args);
  va_end (var_args);
}

/**
 * clutter_container_remove_actor:
 * @container: a #ClutterContainer
 * @actor: a #ClutterActor
 *
 * Removes @actor from @container. The actor should be unparented, so
 * if you want to keep it around you must hold a reference to it
 * yourself, using g_object_ref(). When the actor has been removed,
 * the "actor-removed" signal is emitted by @container.
 *
 * Since: 0.4
 */
void
clutter_container_remove_actor (ClutterContainer *container,
                                ClutterActor     *actor)
{
  ClutterActor *parent;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  parent = clutter_actor_get_parent (actor);
  if (parent != CLUTTER_ACTOR (container))
    {
      g_warning ("Attempting to remove actor of type `%s' from "
		 "group of class `%s', but the group is not the "
		 "actor's parent.",
		 g_type_name (G_OBJECT_TYPE (actor)),
		 g_type_name (G_OBJECT_TYPE (container)));
      return;
    }

  CLUTTER_CONTAINER_GET_IFACE (container)->remove (container, actor);
}

/**
 * clutter_container_remove_valist:
 * @container: a #ClutterContainer
 * @first_actor: the first #ClutterActor to add
 * @var_args: list of actors to remove, followed by %NULL
 *
 * Alternative va_list version of clutter_container_remove().
 *
 * Since: 0.4
 */
void
clutter_container_remove_valist (ClutterContainer *container,
                                 ClutterActor     *first_actor,
                                 va_list           var_args)
{
  ClutterActor *actor;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (first_actor));

  actor = first_actor;
  while (actor)
    {
      clutter_container_remove_actor (container, actor);

      actor = va_arg (var_args, ClutterActor*);
    }
}

static void
get_children_cb (ClutterActor *child,
                 gpointer      data)
{
  GList **children = data;

  *children = g_list_prepend (*children, child);
}

/**
 * clutter_container_get_children:
 * @container: a #ClutterContainer
 *
 * Retrieves all the children of @container.
 *
 * Return value: a list of #ClutterActor<!-- -->s. Use g_list_free()
 *   on the returned list when done.
 *
 * Since: 0.4
 */
GList *
clutter_container_get_children (ClutterContainer *container)
{
  GList *retval;

  g_return_val_if_fail (CLUTTER_IS_CONTAINER (container), NULL);

  retval = NULL;
  clutter_container_foreach (container, get_children_cb, &retval);

  return g_list_reverse (retval);
}

/**
 * clutter_container_foreach:
 * @container: a #ClutterContainer
 * @callback: a function to be called for each child
 * @user_data: data to be passed to the function, or %NULL
 *
 * Calls @callback for each child of @container.
 *
 * Since: 0.4
 */
void
clutter_container_foreach (ClutterContainer *container,
                           ClutterCallback   callback,
                           gpointer          user_data)
{
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

  CLUTTER_CONTAINER_GET_IFACE (container)->foreach (container, callback, user_data);
}

/**
 * clutter_container_find_child_by_id:
 * @container: a #ClutterContainer
 * @child_id: the unique id of an actor
 *
 * Finds a child actor of a container by its unique ID. Search recurses
 * into any child container.
 *
 * Return value: The child actor with the requested id, or %NULL if no
 *   actor with that id was found
 *
 * Since: 0.6
 */
ClutterActor *
clutter_container_find_child_by_id (ClutterContainer *container,
                                    guint             child_id)
{
  g_return_val_if_fail (CLUTTER_IS_CONTAINER (container), NULL);

  return CLUTTER_CONTAINER_GET_IFACE (container)->find_child_by_id (container,
                                                                    child_id);
}

/**
 * clutter_container_raise_child:
 * @container: a #ClutterContainer
 * @actor: the actor to raise
 * @sibling: the sibling to raise to, or %NULL to raise at the top
 *
 * Raises @actor at @sibling level, in the depth ordering.
 *
 * Since: 0.6
 */
void
clutter_container_raise_child (ClutterContainer *container,
                               ClutterActor     *actor,
                         ClutterActor     *sibling)
{
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));
  g_return_if_fail (sibling == NULL || CLUTTER_IS_ACTOR (sibling));

  if (actor == sibling)
    return;

  if (clutter_actor_get_parent (actor) != CLUTTER_ACTOR (container))
    {
      g_warning ("Actor of type `%s' is not a child of the container "
                 "of type `%s'",
                 g_type_name (G_OBJECT_TYPE (actor)),
                 g_type_name (G_OBJECT_TYPE (container)));
      return;
    }

  if (sibling &&
      clutter_actor_get_parent (sibling) != CLUTTER_ACTOR (container))
    {
      g_warning ("Actor of type `%s' is not a child of the container "
                 "of type `%s'",
                 g_type_name (G_OBJECT_TYPE (sibling)),
                 g_type_name (G_OBJECT_TYPE (container)));
      return;
    }
  
  CLUTTER_CONTAINER_GET_IFACE (container)->raise (container, actor, sibling);
}

/**
 * clutter_container_lower_child:
 * @container: a #ClutterContainer
 * @actor: the actor to raise
 * @sibling: the sibling to lower to, or %NULL to lower at the bottom
 *
 * Lowers @actor at @sibling level, in the depth ordering.
 *
 * Since: 0.6
 */
void
clutter_container_lower_child (ClutterContainer *container,
                               ClutterActor     *actor,
                         ClutterActor     *sibling)
{
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));
  g_return_if_fail (sibling == NULL || CLUTTER_IS_ACTOR (sibling));

  if (actor == sibling)
    return;

  if (clutter_actor_get_parent (actor) != CLUTTER_ACTOR (container))
    {
      g_warning ("Actor of type `%s' is not a child of the container "
                 "of type `%s'",
                 g_type_name (G_OBJECT_TYPE (actor)),
                 g_type_name (G_OBJECT_TYPE (container)));
      return;
    }

  if (sibling &&
      clutter_actor_get_parent (sibling) != CLUTTER_ACTOR (container))
    {
      g_warning ("Actor of type `%s' is not a child of the container "
                 "of type `%s'",
                 g_type_name (G_OBJECT_TYPE (sibling)),
                 g_type_name (G_OBJECT_TYPE (container)));
      return;
    }
  
  CLUTTER_CONTAINER_GET_IFACE (container)->raise (container, actor, sibling);
}

/**
 * clutter_container_sort_depth_order:
 * @container: a #ClutterContainer
 *
 * Sorts a container children using their depth. This function should not
 * be normally used by applications.
 *
 * Since: 0.6
 */
void
clutter_container_sort_depth_order (ClutterContainer *container)
{
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));

  CLUTTER_CONTAINER_GET_IFACE (container)->sort_depth_order (container);
}
