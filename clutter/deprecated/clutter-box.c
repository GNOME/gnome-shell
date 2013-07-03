/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009,2010  Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *      Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-box
 * @short_description: A Generic layout container
 *
 * #ClutterBox is a #ClutterActor sub-class implementing the #ClutterContainer
 * interface. A Box delegates the whole size requisition and size allocation to
 * a #ClutterLayoutManager instance.
 *
 * <example id="example-clutter-box">
 *   <title>Using ClutterBox</title>
 *   <para>The following code shows how to create a #ClutterBox with
 *   a #ClutterLayoutManager sub-class, and how to add children to
 *   it via clutter_box_pack().</para>
 *   <programlisting>
 *  ClutterActor *box;
 *  ClutterLayoutManager *layout;
 *
 *  /&ast; Create the layout manager first &ast;/
 *  layout = clutter_box_layout_new ();
 *  clutter_box_layout_set_homogeneous (CLUTTER_BOX_LAYOUT (layout), TRUE);
 *  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (layout), 12);
 *
 *  /&ast; Then create the ClutterBox actor. The Box will take
 *   &ast; ownership of the ClutterLayoutManager instance by sinking
 *   &ast; its floating reference
 *   &ast;/
 *  box = clutter_box_new (layout);
 *
 *  /&ast; Now add children to the Box using the variadic arguments
 *   &ast; function clutter_box_pack() to set layout properties
 *   &ast;/
 *  clutter_box_pack (CLUTTER_BOX (box), actor,
 *                    "x-align", CLUTTER_BOX_ALIGNMENT_CENTER,
 *                    "y-align", CLUTTER_BOX_ALIGNMENT_END,
 *                    "expand", TRUE,
 *                    NULL);
 *   </programlisting>
 * </example>
 *
 * #ClutterBox<!-- -->'s clutter_box_pack() wraps the generic
 * clutter_container_add_actor() function, but it also allows setting
 * layout properties while adding the new child to the box.
 *
 * #ClutterBox is available since Clutter 1.2
 *
 * Deprecated: 1.10: Use #ClutterActor instead.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "deprecated/clutter-container.h"

#include "clutter-box.h"

#include "clutter-actor-private.h"
#include "clutter-color.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

struct _ClutterBoxPrivate
{
  ClutterLayoutManager *manager;

  guint changed_id;
};

enum
{
  PROP_0,

  PROP_COLOR,
  PROP_COLOR_SET,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

static const ClutterColor default_box_color = { 255, 255, 255, 255 };

G_DEFINE_TYPE_WITH_PRIVATE (ClutterBox, clutter_box, CLUTTER_TYPE_ACTOR)

static inline void
clutter_box_set_color_internal (ClutterBox         *box,
                                const ClutterColor *color)
{
  clutter_actor_set_background_color (CLUTTER_ACTOR (box), color);

  g_object_notify_by_pspec (G_OBJECT (box), obj_props[PROP_COLOR_SET]);
  g_object_notify_by_pspec (G_OBJECT (box), obj_props[PROP_COLOR]);
}

static gboolean
clutter_box_real_get_paint_volume (ClutterActor       *actor,
                                   ClutterPaintVolume *volume)
{
  gboolean retval = FALSE;
  ClutterActorIter iter;
  ClutterActor *child;

  /* if we have a background color, and an allocation, then we need to
   * set it as the base of our paint volume
   */
  retval = clutter_paint_volume_set_from_allocation (volume, actor);

  /* bail out early if we don't have any child */
  if (clutter_actor_get_n_children (actor) == 0)
    return retval;

  retval = TRUE;

  /* otherwise, union the paint volumes of our children, in case
   * any one of them decides to paint outside the parent's allocation
   */
  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      const ClutterPaintVolume *child_volume;

      /* This gets the paint volume of the child transformed into the
       * group's coordinate space... */
      child_volume = clutter_actor_get_transformed_paint_volume (child, actor);
      if (!child_volume)
        return FALSE;

      clutter_paint_volume_union (volume, child_volume);
    }

  return retval;
}

static void
clutter_box_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ClutterBox *self = CLUTTER_BOX (gobject);

  switch (prop_id)
    {
    case PROP_COLOR:
      clutter_box_set_color_internal (self, clutter_value_get_color (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_box_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_COLOR:
      {
        ClutterColor color;

        clutter_actor_get_background_color (CLUTTER_ACTOR (gobject),
                                            &color);
        clutter_value_set_color (value, &color);
      }
      break;

    case PROP_COLOR_SET:
      {
        gboolean color_set;

        g_object_get (gobject, "background-color-set", &color_set, NULL);
        g_value_set_boolean (value, color_set);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_box_real_destroy (ClutterActor *actor)
{
  ClutterActor *iter;

  iter = clutter_actor_get_first_child (actor);
  while (iter != NULL)
    {
      ClutterActor *next = clutter_actor_get_next_sibling (iter);

      clutter_actor_destroy (iter);

      iter = next;
    }
}

static void
clutter_box_class_init (ClutterBoxClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->destroy = clutter_box_real_destroy;
  actor_class->get_paint_volume = clutter_box_real_get_paint_volume;

  gobject_class->set_property = clutter_box_set_property;
  gobject_class->get_property = clutter_box_get_property;

  /**
   * ClutterBox:color:
   *
   * The color to be used to paint the background of the
   * #ClutterBox. Setting this property will set the
   * #ClutterBox:color-set property as a side effect
   *
   * Since: 1.2
   */
  obj_props[PROP_COLOR] =
    clutter_param_spec_color ("color",
                              P_("Color"),
                              P_("The background color of the box"),
                              &default_box_color,
                              CLUTTER_PARAM_READWRITE);

  /**
   * ClutterBox:color-set:
   *
   * Whether the #ClutterBox:color property has been set
   *
   * Since: 1.2
   */
  obj_props[PROP_COLOR_SET] =
    g_param_spec_boolean ("color-set",
                          P_("Color Set"),
                          P_("Whether the background color is set"),
                          FALSE,
                          CLUTTER_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
clutter_box_init (ClutterBox *self)
{
  self->priv = clutter_box_get_instance_private (self);
}

/**
 * clutter_box_new:
 * @manager: a #ClutterLayoutManager
 *
 * Creates a new #ClutterBox. The children of the box will be layed
 * out by the passed @manager
 *
 * Return value: the newly created #ClutterBox actor
 *
 * Since: 1.2
 *
 * Deprecated: 1.10: Use clutter_actor_new() instead.
 */
ClutterActor *
clutter_box_new (ClutterLayoutManager *manager)
{
  g_return_val_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager), NULL);

  return g_object_new (CLUTTER_TYPE_BOX,
                       "layout-manager", manager,
                       NULL);
}

/**
 * clutter_box_set_layout_manager:
 * @box: a #ClutterBox
 * @manager: a #ClutterLayoutManager
 *
 * Sets the #ClutterLayoutManager for @box
 *
 * A #ClutterLayoutManager is a delegate object that controls the
 * layout of the children of @box
 *
 * Since: 1.2
 *
 * Deprecated: 1.10: Use clutter_actor_set_layout_manager() instead.
 */
void
clutter_box_set_layout_manager (ClutterBox           *box,
                                ClutterLayoutManager *manager)
{
  clutter_actor_set_layout_manager (CLUTTER_ACTOR (box), manager);
}

/**
 * clutter_box_get_layout_manager:
 * @box: a #ClutterBox
 *
 * Retrieves the #ClutterLayoutManager instance used by @box
 *
 * Return value: (transfer none): a #ClutterLayoutManager. The returned
 *   #ClutterLayoutManager is owned by the #ClutterBox and it should not
 *   be unreferenced
 *
 * Since: 1.2
 *
 * Deprecated: 1.10: Use clutter_actor_get_layout_manager() instead.
 */
ClutterLayoutManager *
clutter_box_get_layout_manager (ClutterBox *box)
{
  return clutter_actor_get_layout_manager (CLUTTER_ACTOR (box));
}

/**
 * clutter_box_packv:
 * @box: a #ClutterBox
 * @actor: a #ClutterActor
 * @n_properties: the number of properties to set
 * @properties: (array length=n_properties) (element-type utf8): a vector
 *   containing the property names to set
 * @values: (array length=n_properties): a vector containing the property
 *   values to set
 *
 * Vector-based variant of clutter_box_pack(), intended for language
 * bindings to use
 *
 * Since: 1.2
 *
 * Deprecated: 1.10: Use clutter_actor_add_child() instead.
 */
void
clutter_box_packv (ClutterBox          *box,
                   ClutterActor        *actor,
                   guint                n_properties,
                   const gchar * const  properties[],
                   const GValue        *values)
{
  ClutterLayoutManager *manager;
  ClutterContainer *container;
  ClutterLayoutMeta *meta;
  GObjectClass *klass;
  gint i;

  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  container = CLUTTER_CONTAINER (box);
  clutter_container_add_actor (container, actor);

  manager = clutter_actor_get_layout_manager (CLUTTER_ACTOR (box));
  if (manager == NULL)
    return;

  meta = clutter_layout_manager_get_child_meta (manager,
                                                container,
                                                actor);

  if (meta == NULL)
    return;

  klass = G_OBJECT_GET_CLASS (meta);

  for (i = 0; i < n_properties; i++)
    {
      const gchar *pname = properties[i];
      GParamSpec *pspec;

      pspec = g_object_class_find_property (klass, pname);
      if (pspec == NULL)
        {
          g_warning ("%s: the layout property '%s' for managers "
                     "of type '%s' (meta type '%s') does not exist",
                     G_STRLOC,
                     pname,
                     G_OBJECT_TYPE_NAME (manager),
                     G_OBJECT_TYPE_NAME (meta));
          break;
        }

      if (!(pspec->flags & G_PARAM_WRITABLE))
        {
          g_warning ("%s: the layout property '%s' for managers "
                     "of type '%s' (meta type '%s') is not writable",
                     G_STRLOC,
                     pspec->name,
                     G_OBJECT_TYPE_NAME (manager),
                     G_OBJECT_TYPE_NAME (meta));
          break;
        }

      clutter_layout_manager_child_set_property (manager,
                                                 container, actor,
                                                 pname, &values[i]);
    }
}

static inline void
clutter_box_set_property_valist (ClutterBox   *box,
                                 ClutterActor *actor,
                                 const gchar  *first_property,
                                 va_list       var_args)
{
  ClutterContainer *container = CLUTTER_CONTAINER (box);
  ClutterLayoutManager *manager;
  ClutterLayoutMeta *meta;
  GObjectClass *klass;
  const gchar *pname;

  manager = clutter_actor_get_layout_manager (CLUTTER_ACTOR (box));
  if (manager == NULL)
    return;

  meta = clutter_layout_manager_get_child_meta (manager,
                                                container,
                                                actor);

  if (meta == NULL)
    return;

  klass = G_OBJECT_GET_CLASS (meta);

  pname = first_property;
  while (pname)
    {
      GValue value = { 0, };
      GParamSpec *pspec;
      gchar *error;

      pspec = g_object_class_find_property (klass, pname);
      if (pspec == NULL)
        {
          g_warning ("%s: the layout property '%s' for managers "
                     "of type '%s' (meta type '%s') does not exist",
                     G_STRLOC,
                     pname,
                     G_OBJECT_TYPE_NAME (manager),
                     G_OBJECT_TYPE_NAME (meta));
          break;
        }

      if (!(pspec->flags & G_PARAM_WRITABLE))
        {
          g_warning ("%s: the layout property '%s' for managers "
                     "of type '%s' (meta type '%s') is not writable",
                     G_STRLOC,
                     pspec->name,
                     G_OBJECT_TYPE_NAME (manager),
                     G_OBJECT_TYPE_NAME (meta));
          break;
        }

      G_VALUE_COLLECT_INIT (&value, G_PARAM_SPEC_VALUE_TYPE (pspec),
                            var_args, 0,
                            &error);

      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          break;
        }

      clutter_layout_manager_child_set_property (manager,
                                                 container, actor,
                                                 pspec->name, &value);

      g_value_unset (&value);

      pname = va_arg (var_args, gchar*);
    }
}

/**
 * clutter_box_pack:
 * @box: a #ClutterBox
 * @actor: a #ClutterActor
 * @first_property: the name of the first property to set, or %NULL
 * @...: a list of property name and value pairs, terminated by %NULL
 *
 * Adds @actor to @box and sets layout properties at the same time,
 * if the #ClutterLayoutManager used by @box has them
 *
 * This function is a wrapper around clutter_container_add_actor()
 * and clutter_layout_manager_child_set()
 *
 * Language bindings should use the vector-based clutter_box_packv()
 * variant instead
 *
 * Since: 1.2
 *
 * Deprecated: 1.10: Use clutter_actor_add_child() instead.
 */
void
clutter_box_pack (ClutterBox   *box,
                  ClutterActor *actor,
                  const gchar  *first_property,
                  ...)
{
  va_list var_args;

  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  clutter_container_add_actor (CLUTTER_CONTAINER (box), actor);

  if (first_property == NULL || *first_property == '\0')
    return;

  va_start (var_args, first_property);
  clutter_box_set_property_valist (box, actor, first_property, var_args);
  va_end (var_args);
}

/**
 * clutter_box_pack_after:
 * @box: a #ClutterBox
 * @actor: a #ClutterActor
 * @sibling: (allow-none): a #ClutterActor or %NULL
 * @first_property: the name of the first property to set, or %NULL
 * @...: a list of property name and value pairs, terminated by %NULL
 *
 * Adds @actor to @box, placing it after @sibling, and sets layout
 * properties at the same time, if the #ClutterLayoutManager used by
 * @box supports them
 *
 * If @sibling is %NULL then @actor is placed at the end of the
 * list of children, to be allocated and painted after every other child
 *
 * This function is a wrapper around clutter_container_add_actor(),
 * clutter_container_raise_child() and clutter_layout_manager_child_set()
 *
 * Since: 1.2
 *
 * Deprecated: 1.10: Use clutter_actor_insert_child_above() instead.
 */
void
clutter_box_pack_after (ClutterBox   *box,
                        ClutterActor *actor,
                        ClutterActor *sibling,
                        const gchar  *first_property,
                        ...)
{
  va_list var_args;

  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));
  g_return_if_fail (sibling == NULL || CLUTTER_IS_ACTOR (sibling));

  clutter_container_add_actor (CLUTTER_CONTAINER (box), actor);
  clutter_container_raise_child (CLUTTER_CONTAINER (box), actor, sibling);

  if (first_property == NULL || *first_property == '\0')
    return;

  va_start (var_args, first_property);
  clutter_box_set_property_valist (box, actor, first_property, var_args);
  va_end (var_args);
}

/**
 * clutter_box_pack_before:
 * @box: a #ClutterBox
 * @actor: a #ClutterActor
 * @sibling: (allow-none): a #ClutterActor or %NULL
 * @first_property: the name of the first property to set, or %NULL
 * @...: a list of property name and value pairs, terminated by %NULL
 *
 * Adds @actor to @box, placing it before @sibling, and sets layout
 * properties at the same time, if the #ClutterLayoutManager used by
 * @box supports them
 *
 * If @sibling is %NULL then @actor is placed at the beginning of the
 * list of children, to be allocated and painted below every other child
 *
 * This function is a wrapper around clutter_container_add_actor(),
 * clutter_container_lower_child() and clutter_layout_manager_child_set()
 *
 * Since: 1.2
 *
 * Deprecated: 1.10: Use clutter_actor_insert_child_below() instead.
 */
void
clutter_box_pack_before (ClutterBox   *box,
                         ClutterActor *actor,
                         ClutterActor *sibling,
                         const gchar  *first_property,
                         ...)
{
  va_list var_args;

  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));
  g_return_if_fail (sibling == NULL || CLUTTER_IS_ACTOR (sibling));

  clutter_container_add_actor (CLUTTER_CONTAINER (box), actor);
  clutter_container_lower_child (CLUTTER_CONTAINER (box), actor, sibling);

  if (first_property == NULL || *first_property == '\0')
    return;

  va_start (var_args, first_property);
  clutter_box_set_property_valist (box, actor, first_property, var_args);
  va_end (var_args);
}

/**
 * clutter_box_pack_at:
 * @box: a #ClutterBox
 * @actor: a #ClutterActor
 * @position: the position to insert the @actor at
 * @first_property: the name of the first property to set, or %NULL
 * @...: a list of property name and value pairs, terminated by %NULL
 *
 * Adds @actor to @box, placing it at @position, and sets layout
 * properties at the same time, if the #ClutterLayoutManager used by
 * @box supports them
 *
 * If @position is a negative number, or is larger than the number of
 * children of @box, the new child is added at the end of the list of
 * children
 *
 * Since: 1.2
 *
 * Deprecated: 1.10: Use clutter_actor_insert_child_at_index() instead.
 */
void
clutter_box_pack_at (ClutterBox   *box,
                     ClutterActor *actor,
                     gint          position,
                     const gchar  *first_property,
                     ...)
{
  va_list var_args;

  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  clutter_actor_insert_child_at_index (CLUTTER_ACTOR (box),
                                       actor,
                                       position);

  /* we need to explicitly call this, because we're not going through
   * the default code paths provided by clutter_container_add()
   */
  clutter_container_create_child_meta (CLUTTER_CONTAINER (box), actor);

  g_signal_emit_by_name (box, "actor-added", actor);

  if (first_property == NULL || *first_property == '\0')
    return;

  va_start (var_args, first_property);
  clutter_box_set_property_valist (box, actor, first_property, var_args);
  va_end (var_args);
}

/**
 * clutter_box_set_color:
 * @box: a #ClutterBox
 * @color: (allow-none): the background color, or %NULL to unset
 *
 * Sets (or unsets) the background color for @box
 *
 * Since: 1.2
 *
 * Deprecated: 1.10: Use clutter_actor_set_background_color() instead.
 */
void
clutter_box_set_color (ClutterBox         *box,
                       const ClutterColor *color)
{
  g_return_if_fail (CLUTTER_IS_BOX (box));

  clutter_box_set_color_internal (box, color);
}

/**
 * clutter_box_get_color:
 * @box: a #ClutterBox
 * @color: (out caller-allocates): return location for a #ClutterColor
 *
 * Retrieves the background color of @box
 *
 * If the #ClutterBox:color-set property is set to %FALSE the
 * returned #ClutterColor is undefined
 *
 * Since: 1.2
 *
 * Deprecated: 1.10: Use clutter_actor_get_background_color() instead.
 */
void
clutter_box_get_color (ClutterBox   *box,
                       ClutterColor *color)
{
  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (color != NULL);

  clutter_actor_get_background_color (CLUTTER_ACTOR (box), color);
}
