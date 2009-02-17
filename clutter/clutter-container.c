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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-container.h"
#include "clutter-child-meta.h"

#include "clutter-debug.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-enum-types.h"

#define CLUTTER_CONTAINER_WARN_NOT_IMPLEMENTED(container,vfunc) \
        G_STMT_START { \
          g_warning ("Container of type `%s' does not implement " \
                     "the required ClutterContainer::%s virtual " \
                     "function.",                                 \
                     G_OBJECT_TYPE_NAME ((container)),            \
                     (vfunc));                                    \
        } G_STMT_END

#define CLUTTER_CONTAINER_NOTE_NOT_IMPLEMENTED(container,vfunc) \
        G_STMT_START { \
          CLUTTER_NOTE (ACTOR, "Container of type `%s' does not "    \
                               "implement the ClutterContainer::%s " \
                               "virtual function.",                  \
                        G_OBJECT_TYPE_NAME ((container)),            \
                        (vfunc));                                    \
        } G_STMT_END

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
  CHILD_NOTIFY,

  LAST_SIGNAL
};

static guint container_signals[LAST_SIGNAL] = { 0, };
static GQuark quark_child_meta = 0;

static ClutterChildMeta *get_child_meta     (ClutterContainer *container,
                                             ClutterActor     *actor);
static void              create_child_meta  (ClutterContainer *container,
                                             ClutterActor     *actor);
static void              destroy_child_meta (ClutterContainer *container,
                                             ClutterActor     *actor);

static void clutter_container_create_child_meta  (ClutterContainer *container,
                                                  ClutterActor     *actor);
static void clutter_container_destroy_child_meta (ClutterContainer *container,
                                                  ClutterActor     *actor);


static void
clutter_container_base_init (gpointer g_iface)
{
  static gboolean initialised = FALSE;

  if (!initialised)
    {
      GType iface_type = G_TYPE_FROM_INTERFACE (g_iface);
      ClutterContainerIface *iface = g_iface;

      initialised = TRUE;

      quark_child_meta =
        g_quark_from_static_string ("clutter-container-child-data");

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
        g_signal_new (I_("actor-added"),
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
        g_signal_new (I_("actor-removed"),
                      iface_type,
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (ClutterContainerIface, actor_removed),
                      NULL, NULL,
                      clutter_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1,
                      CLUTTER_TYPE_ACTOR);

      /**
       * ClutterContainer::child-notify:
       * @container: the container which received the signal
       * @actor: the child that has had a property set. 
       *
       * The ::child-notify signal is emitted each time a property is
       * being set through the clutter_container_child_set() and
       * clutter_container_child_set_property() calls.
       *
       * Since: 0.8
       */
      container_signals[CHILD_NOTIFY] =
        g_signal_new (I_("child-notify"),
                      iface_type,
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (ClutterContainerIface, child_notify),
                      NULL, NULL,
                      clutter_marshal_VOID__OBJECT_OBJECT_PARAM,
                      G_TYPE_NONE, 2,
                      CLUTTER_TYPE_ACTOR, G_TYPE_PARAM);

      iface->child_meta_type    = G_TYPE_INVALID;
      iface->create_child_meta  = create_child_meta;
      iface->destroy_child_meta = destroy_child_meta;
      iface->get_child_meta     = get_child_meta;
    }
}

GType
clutter_container_get_type (void)
{
  static GType container_type = 0;

  if (G_UNLIKELY (!container_type))
    {
      const GTypeInfo container_info =
      {
        sizeof (ClutterContainerIface),
        clutter_container_base_init,
        NULL, /* iface_base_finalize */
      };

      container_type = g_type_register_static (G_TYPE_INTERFACE,
                                               I_("ClutterContainer"),
                                               &container_info, 0);

      g_type_interface_add_prerequisite (container_type, G_TYPE_OBJECT);
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
 * "actor-added" signal. The actor should be parented to
 * @container. You cannot add a #ClutterActor to more than one
 * #ClutterContainer.
 *
 * Since: 0.4
 */
void
clutter_container_add_actor (ClutterContainer *container,
                             ClutterActor     *actor)
{
  ClutterContainerIface *iface;
  ClutterActor *parent;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  iface = CLUTTER_CONTAINER_GET_IFACE (container);
  if (!iface->add)
    {
      CLUTTER_CONTAINER_WARN_NOT_IMPLEMENTED (container, "add");
      return;
    }

  parent = clutter_actor_get_parent (actor);
  if (parent)
    {
      g_warning ("Attempting to add actor of type `%s' to a "
		 "container of type `%s', but the actor has "
                 "already a parent of type `%s'.",
		 g_type_name (G_OBJECT_TYPE (actor)),
		 g_type_name (G_OBJECT_TYPE (container)),
		 g_type_name (G_OBJECT_TYPE (parent)));
      return;
    }

  clutter_container_create_child_meta (container, actor);

  iface->add (container, actor);
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
  ClutterContainerIface *iface;
  ClutterActor *parent;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  iface = CLUTTER_CONTAINER_GET_IFACE (container);
  if (!iface->remove)
    {
      CLUTTER_CONTAINER_WARN_NOT_IMPLEMENTED (container, "remove");
      return;
    }

  parent = clutter_actor_get_parent (actor);
  if (parent != CLUTTER_ACTOR (container))
    {
      g_warning ("Attempting to remove actor of type `%s' from "
		 "group of class `%s', but the container is not "
                 "the actor's parent.",
		 g_type_name (G_OBJECT_TYPE (actor)),
		 g_type_name (G_OBJECT_TYPE (container)));
      return;
    }

  clutter_container_destroy_child_meta (container, actor);

  iface->remove (container, actor);
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
 * Return value: (element-type Actor) (transfer container): a list
 *   of #ClutterActor<!-- -->s. Use g_list_free() on the returned
 *   list when done.
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
  ClutterContainerIface *iface;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (callback != NULL);

  iface = CLUTTER_CONTAINER_GET_IFACE (container);
  if (!iface->foreach)
    {
      CLUTTER_CONTAINER_WARN_NOT_IMPLEMENTED (container, "foreach");
      return;
    }

  iface->foreach (container, callback, user_data);
}

/**
 * clutter_container_raise_child:
 * @container: a #ClutterContainer
 * @actor: the actor to raise
 * @sibling: the sibling to raise to, or %NULL to raise to the top
 *
 * Raises @actor to @sibling level, in the depth ordering.
 *
 * Since: 0.6
 */
void
clutter_container_raise_child (ClutterContainer *container,
                               ClutterActor     *actor,
                               ClutterActor     *sibling)
{
  ClutterContainerIface *iface;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));
  g_return_if_fail (sibling == NULL || CLUTTER_IS_ACTOR (sibling));

  iface = CLUTTER_CONTAINER_GET_IFACE (container);
  if (!iface->raise)
    {
      CLUTTER_CONTAINER_NOTE_NOT_IMPLEMENTED (container, "raise");
      return;
    };

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

  iface->raise (container, actor, sibling);
}

/**
 * clutter_container_lower_child:
 * @container: a #ClutterContainer
 * @actor: the actor to raise
 * @sibling: the sibling to lower to, or %NULL to lower to the bottom
 *
 * Lowers @actor to @sibling level, in the depth ordering.
 *
 * Since: 0.6
 */
void
clutter_container_lower_child (ClutterContainer *container,
                               ClutterActor     *actor,
                               ClutterActor     *sibling)
{
  ClutterContainerIface *iface;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));
  g_return_if_fail (sibling == NULL || CLUTTER_IS_ACTOR (sibling));

  iface = CLUTTER_CONTAINER_GET_IFACE (container);
  if (!iface->lower)
    {
      CLUTTER_CONTAINER_NOTE_NOT_IMPLEMENTED (container, "lower");
      return;
    }

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

  iface->lower (container, actor, sibling);
}

/**
 * clutter_container_sort_depth_order:
 * @container: a #ClutterContainer
 *
 * Sorts a container's children using their depth. This function should not
 * be normally used by applications.
 *
 * Since: 0.6
 */
void
clutter_container_sort_depth_order (ClutterContainer *container)
{
  ClutterContainerIface *iface;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));

  iface = CLUTTER_CONTAINER_GET_IFACE (container);
  if (iface->sort_depth_order)
    iface->sort_depth_order (container);
  else
    CLUTTER_CONTAINER_NOTE_NOT_IMPLEMENTED (container, "sort_depth_order");
}

/**
 * clutter_container_find_child_by_name:
 * @container: a #ClutterContainer
 * @child_name: the name of the requested child.
 *
 * Finds a child actor of a container by its name. Search recurses
 * into any child container.
 *
 * Return value: (transfer none): The child actor with the requested name,
 *   or %NULL if no actor with that name was found.
 *
 * Since: 0.6
 */
ClutterActor *
clutter_container_find_child_by_name (ClutterContainer *container,
                                      const gchar      *child_name)
{
  GList        *children;
  GList        *iter;
  ClutterActor *actor = NULL;

  g_return_val_if_fail (CLUTTER_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (child_name != NULL, NULL);

  children = clutter_container_get_children (container);

  for (iter = children; iter; iter = g_list_next (iter))
    {
      ClutterActor *a;
      const gchar  *iter_name;

      a = CLUTTER_ACTOR (iter->data);
      iter_name = clutter_actor_get_name (a);

      if (iter_name && !strcmp (iter_name, child_name))
        {
          actor = a;
          break;
        }

      if (CLUTTER_IS_CONTAINER (a))
        {
          ClutterContainer *c = CLUTTER_CONTAINER (a);

          actor = clutter_container_find_child_by_name (c, child_name);
          if (actor)
            break;
	}
    }

  g_list_free (children);

  return actor;
}

static ClutterChildMeta *
get_child_meta (ClutterContainer *container,
                ClutterActor     *actor)
{
  ClutterContainerIface *iface = CLUTTER_CONTAINER_GET_IFACE (container);

  if (iface->child_meta_type == G_TYPE_INVALID)
    return NULL;
  else
    {
      ClutterChildMeta *child_meta = NULL;
      GSList *list, *iter;

      list = g_object_get_qdata (G_OBJECT (container), quark_child_meta);
      for (iter = list; iter; iter = g_slist_next (iter))
        {
          child_meta = iter->data;

          if (child_meta->actor == actor)
            return child_meta;
        }
    }

  return NULL;
}

static void
create_child_meta (ClutterContainer *container,
                   ClutterActor     *actor)
{
  ClutterContainerIface *iface = CLUTTER_CONTAINER_GET_IFACE (container);
  ClutterChildMeta      *child_meta = NULL;
  GSList                *data_list = NULL;

  if (iface->child_meta_type == G_TYPE_INVALID)
    return;

  if (!g_type_is_a (iface->child_meta_type, CLUTTER_TYPE_CHILD_META))
    {
      g_warning ("%s: Child data of type `%s' is not a ClutterChildMeta",
                 G_STRLOC, g_type_name (iface->child_meta_type));
      return;
    }

  child_meta = g_object_new (iface->child_meta_type,
                             "container", container,
                             "actor", actor,
                             NULL);

  data_list = g_object_get_qdata (G_OBJECT (container), quark_child_meta);
  data_list = g_slist_prepend (data_list, child_meta);
  g_object_set_qdata (G_OBJECT (container), quark_child_meta, data_list);
}

static void
destroy_child_meta (ClutterContainer *container,
                    ClutterActor     *actor)
{
  ClutterContainerIface *iface  = CLUTTER_CONTAINER_GET_IFACE (container);
  GObject               *object = G_OBJECT (container);

  if (iface->child_meta_type == G_TYPE_INVALID)
    return;
  else
    {
      ClutterChildMeta *child_meta = NULL;
      GSList *list = g_object_get_qdata (object, quark_child_meta);
      GSList *iter;

      for (iter = list; iter; iter = g_slist_next (iter))
        {
          child_meta = iter->data;

          if (child_meta->actor == actor)
            break;
          else
            child_meta = NULL;
        }

      if (child_meta)
        {
          list = g_slist_remove (list, child_meta);
          g_object_set_qdata (object, quark_child_meta, list);

          g_object_unref (child_meta);
        }
    }
}

/**
 * clutter_container_get_child_meta:
 * @container: a #ClutterContainer
 * @actor: a #ClutterActor that is a child of @container.
 *
 * Retrieves the #ClutterChildMeta which contains the data about the
 * @container specific state for @actor.
 *
 * Return value: (transfer none): the #ClutterChildMeta for the @actor child
 *   of @container or %NULL if the specifiec actor does not exist or the
 *   container is not configured to provide #ClutterChildMeta<!-- -->s
 *
 * Since: 0.8
 */
ClutterChildMeta *
clutter_container_get_child_meta (ClutterContainer *container,
                                  ClutterActor     *actor)
{
  ClutterContainerIface *iface = CLUTTER_CONTAINER_GET_IFACE (container);

  if (iface->child_meta_type == G_TYPE_INVALID)
    return NULL;

  if (G_LIKELY (iface->get_child_meta))
    return iface->get_child_meta (container, actor);

  return NULL;
}

/*
 * clutter_container_create_child_meta:
 * @container: a #ClutterContainer
 * @actor: a #ClutterActor
 *
 * Creates the #ClutterChildMeta wrapping @actor inside the
 * @container, if the #ClutterContainerIface::child_meta_type
 * class member is not set to %G_TYPE_INVALID.
 */
static void
clutter_container_create_child_meta (ClutterContainer *container,
                                     ClutterActor     *actor)
{
  ClutterContainerIface *iface = CLUTTER_CONTAINER_GET_IFACE (container);

  if (iface->child_meta_type == G_TYPE_INVALID)
    return;

  g_assert (g_type_is_a (iface->child_meta_type, CLUTTER_TYPE_CHILD_META));

  if (G_LIKELY (iface->create_child_meta))
    iface->create_child_meta (container, actor);
}

/*
 * clutter_container_destroy_child_meta:
 * @container: a #ClutterContainer
 * @actor: a #ClutterActor
 *
 * Destroys the #ClutterChildMeta wrapping @actor inside the
 * @container, if any.
 */
static void
clutter_container_destroy_child_meta (ClutterContainer *container,
                                      ClutterActor     *actor)
{
  ClutterContainerIface *iface = CLUTTER_CONTAINER_GET_IFACE (container);

  if (iface->child_meta_type == G_TYPE_INVALID)
    return;

  if (G_LIKELY (iface->destroy_child_meta))
    iface->destroy_child_meta (container, actor);
}

/**
 * clutter_container_class_find_child_property:
 * @klass: a #GObjectClass implementing the #ClutterContainer interface.
 * @property_name: a property name.
 *
 * Looks up the #GParamSpec for a child property of @klass.
 *
 * Return value: (transfer none): The #GParamSpec for the property or %NULL
 *   if no such property exist.
 *
 * Since: 0.8
 */
GParamSpec *
clutter_container_class_find_child_property (GObjectClass *klass,
                                             const gchar  *property_name)
{
  ClutterContainerIface *iface;
  GObjectClass          *child_class;
  GParamSpec            *pspec;

  g_return_val_if_fail (G_IS_OBJECT_CLASS (klass), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);
  g_return_val_if_fail (g_type_is_a (G_TYPE_FROM_CLASS (klass),
                                     CLUTTER_TYPE_CONTAINER),
                        NULL);

  iface = g_type_interface_peek (klass, CLUTTER_TYPE_CONTAINER);
  g_return_val_if_fail (iface != NULL, NULL);

  if (iface->child_meta_type == G_TYPE_INVALID)
    return NULL;

  child_class = g_type_class_ref (iface->child_meta_type);
  pspec = g_object_class_find_property (child_class, property_name);
  g_type_class_unref (child_class);

  return pspec;
}

/**
 * clutter_container_class_list_child_properties:
 * @klass: a #GObjectClass implementing the #ClutterContainer interface.
 * @n_properties: return location for length of returned array.
 *
 * Returns an array of #GParamSpec for all child properties.
 *
 * Return value: (array length=n_properties) (transfer container): an array
 *   of #GParamSpec<!-- -->s which should be freed after use.
 *
 * Since: 0.8
 */
GParamSpec **
clutter_container_class_list_child_properties (GObjectClass *klass,
                                               guint        *n_properties)
{
  ClutterContainerIface *iface;
  GObjectClass          *child_class;
  GParamSpec           **retval;

  g_return_val_if_fail (G_IS_OBJECT_CLASS (klass), NULL);
  g_return_val_if_fail (g_type_is_a (G_TYPE_FROM_CLASS (klass),
                                     CLUTTER_TYPE_CONTAINER),
                        NULL);

  iface = g_type_interface_peek (klass, CLUTTER_TYPE_CONTAINER);
  g_return_val_if_fail (iface != NULL, NULL);

  if (iface->child_meta_type == G_TYPE_INVALID)
    return NULL;

  child_class = g_type_class_ref (iface->child_meta_type);
  retval = g_object_class_list_properties (child_class, n_properties);
  g_type_class_unref (child_class);

  return retval;
}

static inline void
container_set_child_property (ClutterContainer *container,
                              ClutterActor     *actor,
                              const GValue     *value,
                              GParamSpec       *pspec)
{
  ClutterChildMeta *data;
  ClutterContainerIface *iface;

  data = clutter_container_get_child_meta (container, actor);
  g_object_set_property (G_OBJECT (data), pspec->name, value);

  iface = CLUTTER_CONTAINER_GET_IFACE (container);
  if (G_LIKELY (iface->child_notify))
    iface->child_notify (container, actor, pspec);
}

/**
 * clutter_container_child_set_property:
 * @container: a #ClutterContainer
 * @child: a #ClutterActor that is a child of @container.
 * @property: the name of the property to set.
 * @value: the value.
 *
 * Sets a container-specific property on a child of @container.
 *
 * Since: 0.8
 */
void
clutter_container_child_set_property (ClutterContainer *container,
                                      ClutterActor     *child,
                                      const gchar      *property,
                                      const GValue     *value)
{
  GObjectClass *klass;
  GParamSpec   *pspec;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (child));
  g_return_if_fail (property != NULL);
  g_return_if_fail (value != NULL);

  klass = G_OBJECT_GET_CLASS (container);

  pspec = clutter_container_class_find_child_property (klass, property);
  if (!pspec)
    {
      g_warning ("%s: Containers of type `%s' have no child "
                 "property named `%s'",
                 G_STRLOC, G_OBJECT_TYPE_NAME (container), property);
      return;
    }

  if (!(pspec->flags & G_PARAM_WRITABLE))
    {
      g_warning ("%s: Child property `%s' of the container `%s' "
                 "is not writable",
                 G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (container));
      return;
    }

  container_set_child_property (container, child, value, pspec);
}

/**
 * clutter_container_child_set:
 * @container: a #ClutterContainer
 * @actor: a #ClutterActor that is a child of @container.
 * @first_prop: name of the first property to be set.
 * @...: value for the first property, followed optionally by more name/value
 * pairs terminated with NULL.
 *
 * Sets container specific properties on the child of a container.
 *
 * Since: 0.8
 */
void
clutter_container_child_set (ClutterContainer *container,
                             ClutterActor     *actor,
                             const gchar      *first_prop,
                             ...)
{
  GObjectClass *klass;
  const gchar *name;
  va_list var_args;
  
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  klass = G_OBJECT_GET_CLASS (container);

  va_start (var_args, first_prop);

  name = first_prop;
  while (name)
    {
      GValue value = { 0, };
      gchar *error = NULL;
      GParamSpec *pspec;
    
      pspec = clutter_container_class_find_child_property (klass, name);
      if (!pspec)
        {
          g_warning ("%s: Containers of type `%s' have no child "
                     "property named `%s'",
                     G_STRLOC, G_OBJECT_TYPE_NAME (container), name);
          break;
        }

      if (!(pspec->flags & G_PARAM_WRITABLE))
        {
          g_warning ("%s: Child property `%s' of the container `%s' "
                     "is not writable",
                     G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (container));
          break;
        }

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      G_VALUE_COLLECT (&value, var_args, 0, &error);
      if (error)
        {
          /* we intentionally leak the GValue because it might
           * be in an undefined state and calling g_value_unset()
           * on it might crash
           */
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          break;
        }

      container_set_child_property (container, actor, &value, pspec);

      g_value_unset (&value);

      name = va_arg (var_args, gchar*);
    }

  va_end (var_args);
}

static inline void
container_get_child_property (ClutterContainer *container,
                              ClutterActor     *actor,
                              GValue           *value,
                              GParamSpec       *pspec)
{
  ClutterChildMeta *data;

  data = clutter_container_get_child_meta (container, actor);
  g_object_get_property (G_OBJECT (data), pspec->name, value);
}

/**
 * clutter_container_child_get_property:
 * @container: a #ClutterContainer
 * @child: a #ClutterActor that is a child of @container.
 * @property: the name of the property to set.
 * @value: the value.
 *
 * Gets a container specific property of a child of @container, In general,
 * a copy is made of the property contents and the caller is responsible for
 * freeing the memory by calling g_value_unset().
 *
 * Note that clutter_container_child_set_property() is really intended for
 * language bindings, clutter_container_child_set() is much more convenient
 * for C programming.
 *
 * Since: 0.8
 */
void
clutter_container_child_get_property (ClutterContainer *container,
                                      ClutterActor     *child,
                                      const gchar      *property,
                                      GValue           *value)
{
  GObjectClass *klass;
  GParamSpec   *pspec;

  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (child));
  g_return_if_fail (property != NULL);
  g_return_if_fail (value != NULL);

  klass = G_OBJECT_GET_CLASS (container);

  pspec = clutter_container_class_find_child_property (klass, property);
  if (!pspec)
    {
      g_warning ("%s: Containers of type `%s' have no child "
                 "property named `%s'",
                 G_STRLOC, G_OBJECT_TYPE_NAME (container), property);
      return;
    }

  if (!(pspec->flags & G_PARAM_READABLE))
    {
      g_warning ("%s: Child property `%s' of the container `%s' "
                 "is not writable",
                 G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (container));
      return;
    }

  container_get_child_property (container, child, value, pspec);
}


/**
 * clutter_container_child_get:
 * @container: a #ClutterContainer
 * @actor: a #ClutterActor that is a child of @container.
 * @first_prop: name of the first property to be set.
 * @...: value for the first property, followed optionally by more name/value
 * pairs terminated with NULL.
 *
 * Gets @container specific properties of an actor.
 *
 * In general, a copy is made of the property contents and the caller is
 * responsible for freeing the memory in the appropriate manner for the type, for
 * instance by calling g_free() or g_object_unref(). 
 *
 * Since: 0.8
 */
void
clutter_container_child_get (ClutterContainer *container,
                             ClutterActor     *actor,
                             const gchar      *first_prop,
                             ...)
{
  GObjectClass *klass;
  const gchar *name;
  va_list var_args;
  
  g_return_if_fail (CLUTTER_IS_CONTAINER (container));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  klass = G_OBJECT_GET_CLASS (container);

  va_start (var_args, first_prop);

  name = first_prop;
  while (name)
    {
      GValue value = { 0, };
      gchar *error = NULL;
      GParamSpec *pspec;
    
      pspec = clutter_container_class_find_child_property (klass, name);
      if (!pspec)
        {
          g_warning ("%s: container `%s' has no child property named `%s'",
                     G_STRLOC, G_OBJECT_TYPE_NAME (container), name);
          break;
        }

      if (!(pspec->flags & G_PARAM_READABLE))
        {
          g_warning ("%s: child property `%s' of container `%s' is not readable",
                     G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (container));
          break;
        }

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));

      container_get_child_property (container, actor, &value, pspec);

      G_VALUE_LCOPY (&value, var_args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          g_value_unset (&value);
          break;
        }

      g_value_unset (&value);

      name = va_arg (var_args, gchar*);
    }

  va_end (var_args);
}
