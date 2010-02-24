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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-box.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

#define CLUTTER_BOX_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_BOX, ClutterBoxPrivate))

struct _ClutterBoxPrivate
{
  ClutterLayoutManager *manager;

  GList *children;

  guint changed_id;

  ClutterColor color;
  guint color_set : 1;
};

enum
{
  PROP_0,

  PROP_LAYOUT_MANAGER,
  PROP_COLOR,
  PROP_COLOR_SET
};

static const ClutterColor default_box_color = { 255, 255, 255, 255 };

static void clutter_container_iface_init (ClutterContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterBox, clutter_box, CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init));

static gint
sort_by_depth (gconstpointer a,
               gconstpointer b)
{
  gfloat depth_a = clutter_actor_get_depth ((ClutterActor *) a);
  gfloat depth_b = clutter_actor_get_depth ((ClutterActor *) b);

  if (depth_a < depth_b)
    return -1;

  if (depth_a > depth_b)
    return 1;

  return 0;
}

static void
clutter_box_real_add (ClutterContainer *container,
                      ClutterActor     *actor)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (container)->priv;
  GList *l, *prev = NULL;
  gfloat actor_depth;

  g_object_ref (actor);

  actor_depth = clutter_actor_get_depth (actor);

  /* Find the right place to insert the child so that it will still be
     sorted and the child will be after all of the actors at the same
     depth */
  for (l = priv->children;
       l && (clutter_actor_get_depth (l->data) <= actor_depth);
       l = l->next)
    prev = l;

  /* Insert the node before the found node */
  l = g_list_prepend (l, actor);
  /* Fixup the links */
  if (prev)
    {
      prev->next = l;
      l->prev = prev;
    }
  else
    priv->children = l;

  clutter_actor_set_parent (actor, CLUTTER_ACTOR (container));

  clutter_actor_queue_relayout (actor);

  g_signal_emit_by_name (container, "actor-added", actor);

  g_object_unref (actor);
}

static void
clutter_box_real_remove (ClutterContainer *container,
                         ClutterActor     *actor)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (container)->priv;

  g_object_ref (actor);

  priv->children = g_list_remove (priv->children, actor);
  clutter_actor_unparent (actor);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));

  g_signal_emit_by_name (container, "actor-removed", actor);

  g_object_unref (actor);
}

static void
clutter_box_real_foreach (ClutterContainer *container,
                          ClutterCallback   callback,
                          gpointer          user_data)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (container)->priv;

  /* Using g_list_foreach instead of iterating the list manually
     because it has better protection against the current node being
     removed. This will happen for example if someone calls
     clutter_container_foreach(container, clutter_actor_destroy) */
  g_list_foreach (priv->children, (GFunc) callback, user_data);
}

static void
clutter_box_real_raise (ClutterContainer *container,
                        ClutterActor     *actor,
                        ClutterActor     *sibling)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (container)->priv;

  priv->children = g_list_remove (priv->children, actor);

  if (sibling == NULL)
    priv->children = g_list_append (priv->children, actor);
  else
    {
      gint index_ = g_list_index (priv->children, sibling) + 1;

      priv->children = g_list_insert (priv->children, actor, index_);
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));
}

static void
clutter_box_real_lower (ClutterContainer *container,
                        ClutterActor     *actor,
                        ClutterActor     *sibling)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (container)->priv;

  priv->children = g_list_remove (priv->children, actor);

  if (sibling == NULL)
    priv->children = g_list_prepend (priv->children, actor);
  else
    {
      gint index_ = g_list_index (priv->children, sibling);

      priv->children = g_list_insert (priv->children, actor, index_);
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));
}

static void
clutter_box_real_sort_depth_order (ClutterContainer *container)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (container)->priv;

  priv->children = g_list_sort (priv->children, sort_by_depth);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = clutter_box_real_add;
  iface->remove = clutter_box_real_remove;
  iface->foreach = clutter_box_real_foreach;
  iface->raise = clutter_box_real_raise;
  iface->lower = clutter_box_real_lower;
  iface->sort_depth_order = clutter_box_real_sort_depth_order;
}

static void
clutter_box_real_paint (ClutterActor *actor)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (actor)->priv;

  if (priv->color_set)
    {
      ClutterActorBox box = { 0, };
      gfloat width, height;
      guint8 tmp_alpha;

      clutter_actor_get_allocation_box (actor, &box);
      clutter_actor_box_get_size (&box, &width, &height);

      tmp_alpha = clutter_actor_get_paint_opacity (actor)
                * priv->color.alpha
                / 255;

      cogl_set_source_color4ub (priv->color.red,
                                priv->color.green,
                                priv->color.blue,
                                tmp_alpha);

      cogl_rectangle (0, 0, width, height);
    }

  g_list_foreach (priv->children, (GFunc) clutter_actor_paint, NULL);
}

static void
clutter_box_real_pick (ClutterActor       *actor,
                       const ClutterColor *pick)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (actor)->priv;

  CLUTTER_ACTOR_CLASS (clutter_box_parent_class)->pick (actor, pick);

  g_list_foreach (priv->children, (GFunc) clutter_actor_paint, NULL);
}

static void
clutter_box_real_get_preferred_width (ClutterActor *actor,
                                      gfloat        for_height,
                                      gfloat       *min_width,
                                      gfloat       *natural_width)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (actor)->priv;

  clutter_layout_manager_get_preferred_width (priv->manager,
                                              CLUTTER_CONTAINER (actor),
                                              for_height,
                                              min_width, natural_width);
}

static void
clutter_box_real_get_preferred_height (ClutterActor *actor,
                                       gfloat        for_width,
                                       gfloat       *min_height,
                                       gfloat       *natural_height)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (actor)->priv;

  clutter_layout_manager_get_preferred_height (priv->manager,
                                               CLUTTER_CONTAINER (actor),
                                               for_width,
                                               min_height, natural_height);
}

static void
clutter_box_real_allocate (ClutterActor           *actor,
                           const ClutterActorBox  *allocation,
                           ClutterAllocationFlags  flags)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (actor)->priv;
  ClutterActorClass *klass;

  klass = CLUTTER_ACTOR_CLASS (clutter_box_parent_class);
  klass->allocate (actor, allocation, flags);

  clutter_layout_manager_allocate (priv->manager,
                                   CLUTTER_CONTAINER (actor),
                                   allocation, flags);
}

static void
clutter_box_destroy (ClutterActor *actor)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (actor)->priv;

  /* destroy all our children */
  g_list_foreach (priv->children, (GFunc) clutter_actor_destroy, NULL);

  if (CLUTTER_ACTOR_CLASS (clutter_box_parent_class)->destroy)
    CLUTTER_ACTOR_CLASS (clutter_box_parent_class)->destroy (actor);
}

static inline void
set_layout_manager (ClutterBox           *self,
                    ClutterLayoutManager *manager)
{
  ClutterBoxPrivate *priv = self->priv;

  if (priv->manager == manager)
    return;

  if (priv->manager != NULL)
    {
      if (priv->changed_id != 0)
        g_signal_handler_disconnect (priv->manager, priv->changed_id);

      clutter_layout_manager_set_container (priv->manager, NULL);
      g_object_unref (priv->manager);

      priv->manager = NULL;
      priv->changed_id = 0;
    }

  if (manager != NULL)
    {
      priv->manager = g_object_ref_sink (manager);
      clutter_layout_manager_set_container (manager,
                                            CLUTTER_CONTAINER (self));

      priv->changed_id =
        g_signal_connect_swapped (priv->manager, "layout-changed",
                                  G_CALLBACK (clutter_actor_queue_relayout),
                                  self);
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));

  g_object_notify (G_OBJECT (self), "layout-manager");
}

static void
clutter_box_dispose (GObject *gobject)
{
  ClutterBox *self = CLUTTER_BOX (gobject);

  set_layout_manager (self, NULL);

  G_OBJECT_CLASS (clutter_box_parent_class)->dispose (gobject);
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
    case PROP_LAYOUT_MANAGER:
      set_layout_manager (self, g_value_get_object (value));
      break;

    case PROP_COLOR:
      clutter_box_set_color (self, clutter_value_get_color (value));
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
  ClutterBoxPrivate *priv = CLUTTER_BOX (gobject)->priv;

  switch (prop_id)
    {
    case PROP_LAYOUT_MANAGER:
      g_value_set_object (value, priv->manager);
      break;

    case PROP_COLOR:
      clutter_value_set_color (value, &priv->color);
      break;

    case PROP_COLOR_SET:
      g_value_set_boolean (value, priv->color_set);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_box_class_init (ClutterBoxClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (ClutterBoxPrivate));

  actor_class->get_preferred_width = clutter_box_real_get_preferred_width;
  actor_class->get_preferred_height = clutter_box_real_get_preferred_height;
  actor_class->allocate = clutter_box_real_allocate;
  actor_class->paint = clutter_box_real_paint;
  actor_class->pick = clutter_box_real_pick;
  actor_class->destroy = clutter_box_destroy;

  gobject_class->set_property = clutter_box_set_property;
  gobject_class->get_property = clutter_box_get_property;
  gobject_class->dispose = clutter_box_dispose;

  /**
   * ClutterBox:layout-manager:
   *
   * The #ClutterLayoutManager used by the #ClutterBox
   *
   * Since: 1.2
   */
  pspec = g_param_spec_object ("layout-manager",
                               "Layout Manager",
                               "The layout manager used by the box",
                               CLUTTER_TYPE_LAYOUT_MANAGER,
                               CLUTTER_PARAM_READWRITE |
                               G_PARAM_CONSTRUCT);
  g_object_class_install_property (gobject_class,
                                   PROP_LAYOUT_MANAGER,
                                   pspec);

  /**
   * ClutterBox:color:
   *
   * The color to be used to paint the background of the
   * #ClutterBox. Setting this property will set the
   * #ClutterBox:color-set property as a side effect
   *
   * Since: 1.2
   */
  pspec = clutter_param_spec_color ("color",
                                    "Color",
                                    "The background color of the box",
                                    &default_box_color,
                                    CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_COLOR, pspec);

  /**
   * ClutterBox:color-set:
   *
   * Whether the #ClutterBox:color property has been set
   *
   * Since: 1.2
   */
  pspec = g_param_spec_boolean ("color-set",
                                "Color Set",
                                "Whether the background color is set",
                                FALSE,
                                CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_COLOR_SET, pspec);
}

static void
clutter_box_init (ClutterBox *self)
{
  self->priv = CLUTTER_BOX_GET_PRIVATE (self);

  self->priv->color = default_box_color;
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
 * Since: 1.0
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
 */
void
clutter_box_set_layout_manager (ClutterBox           *box,
                                ClutterLayoutManager *manager)
{
  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_LAYOUT_MANAGER (manager));

  set_layout_manager (box, manager);
}

/**
 * clutter_box_get_layout_manager:
 * @box: a #ClutterBox
 *
 * Retrieves the #ClutterLayoutManager instance used by @box
 *
 * Return value: a #ClutterLayoutManager
 *
 * Since: 1.2
 */
ClutterLayoutManager *
clutter_box_get_layout_manager (ClutterBox *box)
{
  g_return_val_if_fail (CLUTTER_IS_BOX (box), NULL);

  return box->priv->manager;
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
 */
void
clutter_box_packv (ClutterBox          *box,
                   ClutterActor        *actor,
                   guint                n_properties,
                   const gchar * const  properties[],
                   const GValue        *values)
{
  ClutterContainer *container;
  ClutterBoxPrivate *priv;
  ClutterLayoutMeta *meta;
  GObjectClass *klass;
  gint i;

  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  container = CLUTTER_CONTAINER (box);
  clutter_container_add_actor (container, actor);

  priv = box->priv;

  meta = clutter_layout_manager_get_child_meta (priv->manager,
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
                     G_OBJECT_TYPE_NAME (priv->manager),
                     G_OBJECT_TYPE_NAME (meta));
          break;
        }

      if (!(pspec->flags & G_PARAM_WRITABLE))
        {
          g_warning ("%s: the layout property '%s' for managers "
                     "of type '%s' (meta type '%s') is not writable",
                     G_STRLOC,
                     pspec->name,
                     G_OBJECT_TYPE_NAME (priv->manager),
                     G_OBJECT_TYPE_NAME (meta));
          break;
        }

      clutter_layout_manager_child_set_property (priv->manager,
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
  ClutterBoxPrivate *priv = box->priv;
  ClutterLayoutMeta *meta;
  GObjectClass *klass;
  const gchar *pname;

  if (priv->manager == NULL)
    return;

  meta = clutter_layout_manager_get_child_meta (priv->manager,
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
                     G_OBJECT_TYPE_NAME (priv->manager),
                     G_OBJECT_TYPE_NAME (meta));
          break;
        }

      if (!(pspec->flags & G_PARAM_WRITABLE))
        {
          g_warning ("%s: the layout property '%s' for managers "
                     "of type '%s' (meta type '%s') is not writable",
                     G_STRLOC,
                     pspec->name,
                     G_OBJECT_TYPE_NAME (priv->manager),
                     G_OBJECT_TYPE_NAME (meta));
          break;
        }

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      G_VALUE_COLLECT (&value, var_args, 0, &error);
      if (error)
        {
          g_warning ("%s: %s", G_STRLOC, error);
          g_free (error);
          break;
        }

      clutter_layout_manager_child_set_property (priv->manager,
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
 * @Varargs: a list of property name and value pairs, terminated by %NULL
 *
 * Adds @actor to @box and sets layout properties at the same time,
 * if the #ClutterLayoutManager used by @box has them
 *
 * This function is a wrapper around clutter_container_add_actor()
 * and clutter_layout_manager_child_set()
 *
 * Language bindings should use the vector-based clutter_box_addv()
 * variant instead
 *
 * Since: 1.2
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
 * @sibling: (allow none): a #ClutterActor or %NULL
 * @first_property: the name of the first property to set, or %NULL
 * @Varargs: a list of property name and value pairs, terminated by %NULL
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
 * @sibling: (allow none): a #ClutterActor or %NULL
 * @first_property: the name of the first property to set, or %NULL
 * @Varargs: a list of property name and value pairs, terminated by %NULL
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
 * @Varargs: a list of property name and value pairs, terminated by %NULL
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
 */
void
clutter_box_pack_at (ClutterBox   *box,
                     ClutterActor *actor,
                     gint          position,
                     const gchar  *first_property,
                     ...)
{
  ClutterBoxPrivate *priv;
  va_list var_args;

  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = box->priv;

  /* this is really clutter_box_add() with a different insert() */
  priv->children = g_list_insert (priv->children,
                                  actor,
                                  position);

  clutter_actor_set_parent (actor, CLUTTER_ACTOR (box));
  clutter_actor_queue_relayout (actor);

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
 */
void
clutter_box_set_color (ClutterBox         *box,
                       const ClutterColor *color)
{
  ClutterBoxPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BOX (box));

  priv = box->priv;

  if (color)
    {
      priv->color = *color;
      priv->color_set = TRUE;
    }
  else
    priv->color_set = FALSE;

  clutter_actor_queue_redraw (CLUTTER_ACTOR (box));

  g_object_notify (G_OBJECT (box), "color-set");
  g_object_notify (G_OBJECT (box), "color");
}

/**
 * clutter_box_get_color:
 * @box: a #ClutterBox
 * @color: (out): return location for a #ClutterColor
 *
 * Retrieves the background color of @box
 *
 * If the #ClutterBox:color-set property is set to %FALSE the
 * returned #ClutterColor is undefined
 *
 * Since: 1.2
 */
void
clutter_box_get_color (ClutterBox   *box,
                       ClutterColor *color)
{
  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (color != NULL);

  *color = box->priv->color;
}
