#include "config.h"

#include "cogl.h"

#include "clutter-box.h"
#include "clutter-container.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-private.h"

/**
 * SECTION:clutter-box
 * @short_description: Base class for layout containers
 *
 * FIXME
 *
 * #ClutterBox is available since Clutter 0.4
 */

enum
{
  PROP_0,

  PROP_SPACING
};

static void clutter_container_iface_init (ClutterContainerIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ClutterBox,
                                  clutter_box,
                                  CLUTTER_TYPE_ACTOR,
                                  G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                         clutter_container_iface_init));


static void
clutter_box_add (ClutterContainer *container,
                 ClutterActor     *actor)
{
  clutter_box_pack_start (CLUTTER_BOX (container), actor);
}

static void
clutter_box_remove (ClutterContainer *container,
                    ClutterActor     *actor)
{
  ClutterBox *box = CLUTTER_BOX (container);
  GList *l;

  g_object_ref (actor);

  for (l = box->children; l; l = l->next)
    {
      ClutterBoxChild *child = l->data;

      if (child->actor == actor)
        {
          CLUTTER_BOX_GET_CLASS (box)->unpack_child (box, child);

          clutter_actor_unparent (actor);
          
          box->children = g_list_remove_link (box->children, l);
          g_list_free (l);
          g_slice_free (ClutterBoxChild, child);

          g_signal_emit_by_name (container, "actor-removed", actor);

          if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (box)))
            clutter_actor_queue_redraw (CLUTTER_ACTOR (box));
          
          break;
        }
    }

  g_object_unref (actor);
}

static void
clutter_box_foreach (ClutterContainer *container,
                     ClutterCallback   callback,
                     gpointer          user_data)
{
  ClutterBox *box = CLUTTER_BOX (container);
  GList *l;

  for (l = box->children; l; l = l->next)
    {
      ClutterBoxChild *child = l->data;

      if (child->pack_type == CLUTTER_PACK_START)
        (* callback) (child->actor, user_data);
    }

  for (l = g_list_last (box->children); l; l = l->prev)
    {
      ClutterBoxChild *child = l->data;

      if (child->pack_type == CLUTTER_PACK_END)
        (* callback) (child->actor, user_data);
    }
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = clutter_box_add;
  iface->remove = clutter_box_remove;
  iface->foreach = clutter_box_foreach;
}

static void
clutter_box_show_all (ClutterActor *actor)
{
  ClutterBox *box = CLUTTER_BOX (actor);
  GList *l;

  for (l = box->children; l; l = l->next)
    {
      ClutterBoxChild *child = l->data;

      clutter_actor_show (child->actor);
    }

  clutter_actor_show (actor);
}

static void
clutter_box_hide_all (ClutterActor *actor)
{
  ClutterBox *box = CLUTTER_BOX (actor);
  GList *l;

  clutter_actor_hide (actor);

  for (l = box->children; l; l = l->next)
    {
      ClutterBoxChild *child = l->data;

      clutter_actor_hide (child->actor);
    }
}

static void
clutter_box_paint (ClutterActor *actor)
{
  ClutterBox *box = CLUTTER_BOX (actor);
  GList *l;

  cogl_push_matrix ();

  for (l = box->children; l; l = l->next)
    {
      ClutterBoxChild *child = l->data;

      if (CLUTTER_ACTOR_IS_MAPPED (child->actor))
        clutter_actor_paint (child->actor);
    }

  cogl_pop_matrix ();
}

static void
clutter_box_pick (ClutterActor       *actor,
                  const ClutterColor *color)
{
  /* just repaint; in the future we might enter in a "focused" status here */
  clutter_box_paint (actor);
}

static void
clutter_box_dispose (GObject *gobject)
{
  ClutterBox *box = CLUTTER_BOX (gobject);
  GList *l;

  for (l =  box->children; l; l = l->next)
    {
      ClutterBoxChild *child = l->data;

      clutter_actor_destroy (child->actor);
      g_slice_free (ClutterBoxChild, child);
    }

  g_list_free (box->children);
  box->children = NULL;

  G_OBJECT_CLASS (clutter_box_parent_class)->dispose (gobject);
}

static void
clutter_box_pack_child_unimplemented (ClutterBox      *box,
                                      ClutterBoxChild *child)
{
  g_warning ("ClutterBox of type `%s' does not implement the "
             "ClutterBox::pack_child method.",
             g_type_name (G_OBJECT_TYPE (box)));
}

static void
clutter_box_unpack_child_unimplemented (ClutterBox      *box,
                                        ClutterBoxChild *child)
{
  g_warning ("ClutterBox of type `%s' does not implement the "
             "ClutterBox::unpack_child method.",
             g_type_name (G_OBJECT_TYPE (box)));
}

static void
clutter_box_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ClutterBox *box = CLUTTER_BOX (gobject);

  switch (prop_id)
    {
    case PROP_SPACING:
      box->spacing = g_value_get_uint (value);
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
  ClutterBox *box = CLUTTER_BOX (gobject);

  switch (prop_id)
    {
    case PROP_SPACING:
      g_value_set_uint (value, box->spacing);
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

  gobject_class->set_property = clutter_box_set_property;
  gobject_class->get_property = clutter_box_get_property;
  gobject_class->dispose = clutter_box_dispose;

  actor_class->show_all = clutter_box_show_all;
  actor_class->hide_all = clutter_box_hide_all;
  actor_class->paint = clutter_box_paint;
  actor_class->pick = clutter_box_pick;

  klass->pack_child = clutter_box_pack_child_unimplemented;
  klass->unpack_child = clutter_box_unpack_child_unimplemented;

  g_object_class_install_property (gobject_class,
                                   PROP_SPACING,
                                   g_param_spec_uint ("spacing",
                                                      "Spacing",
                                                      "Space between each child actor",
                                                      0, G_MAXUINT, 0,
                                                      CLUTTER_PARAM_READWRITE));
}

static void
clutter_box_init (ClutterBox *box)
{

}

/*
 * Public API
 */

/**
 * clutter_box_pack_start:
 * @box: a #ClutterBox
 * @actor: a #ClutterActor
 *
 * Packs @actor into @box
 *
 * Since: 0.4
 */
void
clutter_box_pack_start (ClutterBox   *box,
                        ClutterActor *actor)
{
  ClutterBoxChild *child;

  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  child = g_slice_new (ClutterBoxChild);
  child->actor = actor;
  child->pack_type = CLUTTER_PACK_START;

  CLUTTER_BOX_GET_CLASS (box)->pack_child (box, child);
  
  box->children = g_list_append (box->children, child);
  clutter_actor_set_parent (actor, CLUTTER_ACTOR (box));

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (box)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (box));
}

/**
 * clutter_box_pack_end:
 * @box: a #ClutterBox
 * @actor: a #ClutterActor
 *
 * Packs @actor into @box
 *
 * Since: 0.4
 */
void
clutter_box_pack_end (ClutterBox   *box,
                      ClutterActor *actor)
{
  ClutterBoxChild *child;

  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  child = g_slice_new (ClutterBoxChild);
  child->actor = actor;
  child->pack_type = CLUTTER_PACK_END;

  box->children = g_list_append (box->children, child);
  clutter_actor_set_parent (actor, CLUTTER_ACTOR (box));

  CLUTTER_BOX_GET_CLASS (box)->pack_child (box, child);

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (box)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (box));
}

/**
 * clutter_box_query_child:
 * @box: a #ClutterBox
 * @actor: child to query
 * @child: return location for a #ClutterBoxChild or %NULL
 *
 * Queries @box for the packing data of @actor.
 *
 * Return value: %TRUE if @actor is a child of @box
 *
 * Since: 0.4
 */
gboolean
clutter_box_query_child (ClutterBox      *box,
                         ClutterActor    *actor,
                         ClutterBoxChild *child)
{
  GList *l;

  g_return_val_if_fail (CLUTTER_IS_BOX (box), FALSE);
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), FALSE);

  for (l = box->children; l; l = l->next)
    {
      ClutterBoxChild *box_child = l->data;

      if (box_child->actor == actor)
        {
          if (child)
            {
              child->actor = actor;
              child->pack_type = box_child->pack_type;
            }

          return TRUE;
        }
    }

  return FALSE;
}

/**
 * clutter_box_query_nth_child:
 * @box: a #ClutterBox
 * @index_: position of the child
 * @child: return value for a #ClutterBoxChild, or %NULL
 *
 * Queries the child of @box at @index_ and puts the packing informations
 * inside @child.
 *
 * Return value: %TRUE if an actor was found at @index_
 *
 * Since: 0.4
 */
gboolean
clutter_box_query_nth_child (ClutterBox      *box,
                             gint             index_,
                             ClutterBoxChild *child)
{
  ClutterBoxChild *box_child;

  g_return_val_if_fail (CLUTTER_IS_BOX (box), FALSE);
  g_return_val_if_fail (index > 0, FALSE);

  box_child = g_list_nth_data (box->children, index_);
  if (!box_child)
    return FALSE;

  if (child)
    {
      child->actor = box_child->actor;
      child->pack_type = box_child->pack_type;
    }

  return TRUE;
}

/**
 * clutter_box_get_spacing:
 * @box: a #ClutterBox
 *
 * Gets the value set using clutter_box_set_spacing().
 *
 * Return value: the spacing between children of the box
 *
 * Since: 0.4
 */
guint
clutter_box_get_spacing (ClutterBox *box)
{
  g_return_val_if_fail (CLUTTER_IS_BOX (box), 0);

  return box->spacing;
}

/**
 * clutter_box_set_spacing:
 * @box: a #ClutterBox
 * @spacing: the spacing between children
 *
 * Sets the spacing, in pixels, between the children of @box.
 *
 * Since: 0.4
 */
void
clutter_box_set_spacing (ClutterBox *box,
                         guint       spacing)
{
  g_return_if_fail (CLUTTER_IS_BOX (box));

  if (box->spacing != spacing)
    {
      box->spacing = spacing;

      g_object_notify (G_OBJECT (box), "spacing");
    }
}
