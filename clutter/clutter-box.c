/**
 * SECTION:clutter-box
 * @short_description: A Generic layout container
 *
 * #ClutterBox is a FIXME
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
};

enum
{
  PROP_0,

  PROP_LAYOUT_MANAGER
};

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

  g_object_ref (actor);

  priv->children = g_list_insert_sorted (priv->children,
                                         actor,
                                         sort_by_depth);

  clutter_actor_set_parent (actor, CLUTTER_ACTOR (container));

  clutter_layout_manager_add_child_meta (priv->manager,
                                         container,
                                         actor);

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

  clutter_layout_manager_remove_child_meta (priv->manager,
                                            container,
                                            actor);

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
  GList *l;

  for (l = priv->children; l != NULL; l = l->next)
    (* callback) (l->data, user_data);
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

  clutter_actor_queue_redraw (CLUTTER_ACTOR (container));
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

  clutter_actor_queue_redraw (CLUTTER_ACTOR (container));
}

static void
clutter_box_real_sort_depth_order (ClutterContainer *container)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (container)->priv;

  priv->children = g_list_sort (priv->children, sort_by_depth);

  clutter_actor_queue_redraw (CLUTTER_ACTOR (container));
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

  if (priv->children == NULL)
    {
      if (min_width)
        *min_width = 0.0;

      if (natural_width)
        *natural_width = 0.0;

      return;
    }

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

  if (priv->children == NULL)
    {
      if (min_height)
        *min_height = 0.0;

      if (natural_height)
        *natural_height = 0.0;

      return;
    }

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

  if (priv->children == NULL)
    return;

  clutter_layout_manager_allocate (priv->manager,
                                   CLUTTER_CONTAINER (actor),
                                   allocation, flags);
}

static void
clutter_box_destroy (ClutterActor *actor)
{
  ClutterBoxPrivate *priv = CLUTTER_BOX (actor)->priv;
  GList *l;

  for (l = priv->children; l != NULL; l = l->next)
    clutter_actor_destroy (l->data);

  CLUTTER_ACTOR_CLASS (clutter_box_parent_class)->destroy (actor);
}

static void
on_layout_changed (ClutterLayoutManager *manager,
                   ClutterActor         *self)
{
  clutter_actor_queue_relayout (self);
}

static void
set_layout_manager (ClutterBox           *self,
                    ClutterLayoutManager *manager)
{
  ClutterBoxPrivate *priv = self->priv;

  if (priv->manager != NULL)
    {
      if (priv->changed_id != 0)
        g_signal_handler_disconnect (priv->manager, priv->changed_id);

      g_object_unref (priv->manager);

      priv->manager = NULL;
      priv->changed_id = 0;
    }

  if (manager != NULL)
    {
      priv->manager = g_object_ref_sink (manager);
      priv->changed_id =
        g_signal_connect (priv->manager, "layout-changed",
                          G_CALLBACK (on_layout_changed),
                          self);
    }
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

  pspec = g_param_spec_object ("layout-manager",
                               "Layout Manager",
                               "The layout manager used by the box",
                               CLUTTER_TYPE_LAYOUT_MANAGER,
                               CLUTTER_PARAM_READWRITE |
                               G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class,
                                   PROP_LAYOUT_MANAGER,
                                   pspec);
}

static void
clutter_box_init (ClutterBox *self)
{
  self->priv = CLUTTER_BOX_GET_PRIVATE (self);
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
  ClutterBoxPrivate *priv;
  ClutterContainer *container;
  ClutterLayoutMeta *meta;
  GObjectClass *klass;
  const gchar *pname;
  va_list var_args;

  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  container = CLUTTER_CONTAINER (box);
  clutter_container_add_actor (container, actor);

  if (first_property == NULL || *first_property == '\0')
    return;

  priv = box->priv;

  meta = clutter_layout_manager_get_child_meta (priv->manager,
                                                container,
                                                actor);

  if (meta == NULL)
    return;

  klass = G_OBJECT_GET_CLASS (meta);

  va_start (var_args, first_property);

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

  va_end (var_args);
}
