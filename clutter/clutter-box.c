#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
 * #ClutterBox is a base class for containers which impose a specific layout
 * on their children, unlike #ClutterGroup which is a free-form container.
 *
 * Layout containers are expected to move and size their children depending
 * on a layout contract they establish per-class. For instance, a #ClutterHBox
 * (a subclass of #ClutterBox) lays out its children along an imaginary
 * horizontal line.
 *
 * All #ClutterBox<!-- -->es have a margin, which is decomposed in four
 * components (top, right, bottom left) and a background color. Each child
 * of a #ClutterBox has a packing type and a padding, decomposed like the
 * margin. Actors can be packed using clutter_box_pack() and providing
 * the packing type and the padding, or using clutter_box_pack_defaults()
 * and setting a default padding with clutter_box_set_default_padding().
 * A #ClutterBox implements the #ClutterContainer interface: calling
 * clutter_container_add_actor() on a #ClutterBox will automatically invoke
 * clutter_box_pack_defaults().
 *
 * Each child of a #ClutterBox has its packing information wrapped into the
 * #ClutterBoxChild structure, which can be retrieved either using the
 * clutter_box_query_child() or the clutter_box_query_nth_child() function.
 *
 * Subclasses of #ClutterBox must implement the ClutterBox::pack_child and
 * ClutterBox::unpack_child virtual functions; these functions will be called
 * when adding a child and when removing one, respectively.
 *
 * #ClutterBox is available since Clutter 0.4
 */

enum
{
  PROP_0,

  PROP_MARGIN,
  PROP_COLOR
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
  clutter_box_pack_defaults (CLUTTER_BOX (container), actor);
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
clutter_box_raise (ClutterContainer *container,
                   ClutterActor     *actor,
                   ClutterActor     *sibling)
{
  ClutterBox *box = CLUTTER_BOX (container);
  ClutterBoxChild *child = NULL, *sibling_child = NULL;
  GList *l;
  gint pos;

  for (l = box->children; l; l = l->next)
    {
      child = l->data;

      if (child->actor == actor)
        break;
    }

  box->children = g_list_remove (box->children, child);

  if (!sibling)
    {
      GList *last_item;

      /* raise to top */
      last_item = g_list_last (box->children);
      if (last_item)
        sibling_child = last_item->data;

      box->children = g_list_append (box->children, child);
    }
  else
    {
      for (pos = 1, l = box->children; l; l = l->next, pos += 1)
        {
          sibling_child = l->data;

          if (sibling_child->actor == sibling)
            break;
        }
      
      box->children = g_list_insert (box->children, child, pos);
    }

  if (sibling_child)
    {
      ClutterActor *a = child->actor;
      ClutterActor *b = sibling_child->actor;

      if (clutter_actor_get_depth (a) != clutter_actor_get_depth (b))
        clutter_actor_set_depth (a, clutter_actor_get_depth (b));
    }
}

static void
clutter_box_lower (ClutterContainer *container,
                   ClutterActor     *actor,
                   ClutterActor     *sibling)
{
  ClutterBox *box = CLUTTER_BOX (container);
  ClutterBoxChild *child = NULL, *sibling_child = NULL;
  GList *l;
  gint pos;

  for (l = box->children; l; l = l->next)
    {
      child = l->data;

      if (child->actor == actor)
        break;
    }

  box->children = g_list_remove (box->children, child);

  if (!sibling)
    {
      GList *first_item;

      /* lower to bottom */
      first_item = g_list_first (box->children);
      if (first_item)
        sibling_child = first_item->data;

      box->children = g_list_prepend (box->children, child);
    }
  else
    {
      for (pos = 1, l = box->children; l; l = l->next, pos += 1)
        {
          sibling_child = l->data;

          if (sibling_child->actor == sibling)
            break;
        }
      
      box->children = g_list_insert (box->children, child, pos);
    }

  if (sibling_child)
    {
      ClutterActor *a = child->actor;
      ClutterActor *b = sibling_child->actor;

      if (clutter_actor_get_depth (a) != clutter_actor_get_depth (b))
        clutter_actor_set_depth (a, clutter_actor_get_depth (b));
    }
}

static gint
sort_z_order (gconstpointer a,
              gconstpointer b)
{
  ClutterBoxChild *child_a = (ClutterBoxChild *) a;
  ClutterBoxChild *child_b = (ClutterBoxChild *) b;
  gint depth_a, depth_b;

  depth_a = clutter_actor_get_depth (child_a->actor);
  depth_b = clutter_actor_get_depth (child_b->actor);

  if (depth_a == depth_b)
    return 0;

  if (depth_a > depth_b)
    return 1;

  return -1;
}

static void
clutter_box_sort_depth_order (ClutterContainer *container)
{
  ClutterBox *box = CLUTTER_BOX (container);

  box->children = g_list_sort (box->children, sort_z_order);

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (box)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (box));
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = clutter_box_add;
  iface->remove = clutter_box_remove;
  iface->foreach = clutter_box_foreach;
  iface->raise = clutter_box_raise;
  iface->lower = clutter_box_lower;
  iface->sort_depth_order = clutter_box_sort_depth_order;
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

  cogl_color (&box->color);

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
    case PROP_COLOR:
      clutter_box_set_color (box, g_value_get_boxed (value));
      break;
    case PROP_MARGIN:
      clutter_box_set_margin (box, g_value_get_boxed (value));
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
    case PROP_MARGIN:
      {
        ClutterMargin margin;
        clutter_box_get_margin (box, &margin);
        g_value_set_boxed (value, &margin);
      }
      break;
    case PROP_COLOR:
      {
        ClutterColor color;
        clutter_box_get_color (box, &color);
        g_value_set_boxed (value, &color);
      }
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

  /**
   * ClutterBox:margin:
   *
   * The margin between the inner border of a #ClutterBox and its
   * children.
   *
   * Since: 0.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN,
                                   g_param_spec_boxed ("margin",
                                                       "Margin",
                                                       "Margin between the inner border of a box and its children",
                                                       CLUTTER_TYPE_MARGIN,
                                                       CLUTTER_PARAM_READWRITE));
  /**
   * ClutterBox:color:
   *
   * The background color of a #ClutterBox.
   *
   * Since: 0.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_COLOR,
                                   g_param_spec_boxed ("color",
                                                       "Color",
                                                       "Background color of a box",
                                                       CLUTTER_TYPE_COLOR,
                                                       CLUTTER_PARAM_READWRITE));
}

static void
clutter_box_init (ClutterBox *box)
{
  box->allocation.x1 = box->allocation.y1 = 0;
  box->allocation.x2 = box->allocation.y2 = -1;
}

/*
 * Public API
 */

/**
 * clutter_box_pack:
 * @box: a #ClutterBox
 * @actor: a #ClutterActor to pack into the box
 * @pack_type: Type of packing to use
 * @padding: padding to use on the actor
 *
 * Packs @actor into @box.
 *
 * Since: 0.4
 */
void
clutter_box_pack (ClutterBox           *box,
                  ClutterActor         *actor,
                  ClutterPackType       pack_type,
                  const ClutterPadding *padding)
{
  ClutterBoxChild *child;

  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));
  g_return_if_fail (padding != NULL);

  child = g_slice_new (ClutterBoxChild);
  child->actor = actor;
  child->pack_type = pack_type;
  memcpy (&(child->padding), padding, sizeof (ClutterPadding));

  CLUTTER_BOX_GET_CLASS (box)->pack_child (box, child);
  
  box->children = g_list_prepend (box->children, child);
  clutter_actor_set_parent (actor, CLUTTER_ACTOR (box));

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (box)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (box));
}

/**
 * clutter_box_pack_defaults:
 * @box: a #ClutterBox
 * @actor: a #ClutterActor
 *
 * Packs @actor into @box, using the default settings for the
 * pack type and padding.
 *
 * Since: 0.4
 */
void
clutter_box_pack_defaults (ClutterBox   *box,
                           ClutterActor *actor)
{
  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  clutter_box_pack (box, actor,
                    CLUTTER_PACK_START,
                    &box->default_padding);
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

              child->child_coords.x1 = box_child->child_coords.x1;
              child->child_coords.y1 = box_child->child_coords.y1;
              child->child_coords.x2 = box_child->child_coords.x2;
              child->child_coords.y2 = box_child->child_coords.y2;
              
              child->padding.top = box_child->padding.top;
              child->padding.right = box_child->padding.right;
              child->padding.bottom = box_child->padding.bottom;
              child->padding.left = box_child->padding.left;
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
  g_return_val_if_fail (index_ > 0, FALSE);

  box_child = g_list_nth_data (box->children, index_);
  if (!box_child)
    return FALSE;

  if (child)
    {
      child->actor = box_child->actor;

      child->pack_type = box_child->pack_type;
      
      child->child_coords.x1 = box_child->child_coords.x1;
      child->child_coords.y1 = box_child->child_coords.y1;
      child->child_coords.x2 = box_child->child_coords.x2;
      child->child_coords.y2 = box_child->child_coords.y2;
      
      child->padding.top = box_child->padding.top;
      child->padding.right = box_child->padding.right;
      child->padding.bottom = box_child->padding.bottom;
      child->padding.left = box_child->padding.left;
    }

  return TRUE;
}

/**
 * clutter_box_get_margin:
 * @box: a #ClutterBox
 * @margin: return location for a #ClutterMargin
 *
 * Gets the value set using clutter_box_set_margin().
 *
 * Since: 0.4
 */
void
clutter_box_get_margin (ClutterBox    *box,
                        ClutterMargin *margin)
{
  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (margin != NULL);

  margin->top = box->margin.top;
  margin->right = box->margin.right;
  margin->bottom = box->margin.bottom;
  margin->left = box->margin.left;
}

/**
 * clutter_box_set_margin:
 * @box: a #ClutterBox
 * @margin: a #ClutterMargin, or %NULL to unset the margin
 *
 * Sets the margin, in #ClutterUnit<!-- -->s, between the inner border
 * of the box and the children of the box.
 *
 * Since: 0.4
 */
void
clutter_box_set_margin (ClutterBox          *box,
                        const ClutterMargin *margin)
{
  g_return_if_fail (CLUTTER_IS_BOX (box));

  if (margin)
    {
      box->margin.top = margin->top;
      box->margin.right = margin->right;
      box->margin.bottom = margin->bottom;
      box->margin.left = margin->left;
    }
  else
    {
      box->margin.top = 0;
      box->margin.right = 0;
      box->margin.bottom = 0;
      box->margin.left = 0;
    }

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (box)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (box));

  g_object_notify (G_OBJECT (box), "margin");
}

/**
 * clutter_box_get_color:
 * @box: a #ClutterBox
 * @color: return location for the color
 *
 * Gets the background color of the box set with clutter_box_set_color().
 *
 * Since: 0.4
 */
void
clutter_box_get_color (ClutterBox   *box,
                       ClutterColor *color)
{
  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (color != NULL);

  color->red = box->color.red;
  color->green = box->color.green;
  color->blue = box->color.blue;
  color->alpha = box->color.alpha;
}

/**
 * clutter_box_set_color:
 * @box: a #ClutterBox
 * @color: the background color of the box
 *
 * Sets the background color of the box.
 *
 * Since: 0.4
 */
void
clutter_box_set_color (ClutterBox         *box,
                       const ClutterColor *color)
{
  g_return_if_fail (CLUTTER_IS_BOX (box));
  g_return_if_fail (color != NULL);

  box->color.red = color->red;
  box->color.green = color->green;
  box->color.blue = color->blue;
  box->color.alpha = color->alpha;

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (box)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (box));

  g_object_notify (G_OBJECT (box), "color");
}

/**
 * clutter_box_remove_all:
 * @box: a #ClutterBox
 *
 * Removes all children actors from the #ClutterBox
 *
 * Since: 0.4
 */
void
clutter_box_remove_all (ClutterBox *box)
{
  GList *children;

  g_return_if_fail (CLUTTER_IS_BOX (box));

  children = box->children;
  while (children)
    {
      ClutterBoxChild *child = children->data;
      children = children->next;

      clutter_container_remove_actor (CLUTTER_CONTAINER (box), child->actor);
    }
}

/**
 * clutter_box_set_default_padding:
 * @box: a #ClutterBox
 * @padding_top: top padding, in pixels
 * @padding_right: right padding, in pixels
 * @padding_bottom: bottom padding, in pixels
 * @padding_left: left padding, in pixels
 *
 * Sets the default padding for children, which will be used when
 * packing actors with clutter_box_pack_defaults(). The padding is
 * given in pixels.
 *
 * Since: 0.4
 */
void
clutter_box_set_default_padding (ClutterBox *box,
                                 gint        padding_top,
                                 gint        padding_right,
                                 gint        padding_bottom,
                                 gint        padding_left)
{
  g_return_if_fail (CLUTTER_IS_BOX (box));

  box->default_padding.top = CLUTTER_UNITS_FROM_INT (padding_top);
  box->default_padding.right = CLUTTER_UNITS_FROM_INT (padding_right);
  box->default_padding.bottom = CLUTTER_UNITS_FROM_INT (padding_bottom);
  box->default_padding.left = CLUTTER_UNITS_FROM_INT (padding_left);
}

/**
 * clutter_box_get_default_padding:
 * @box: a #ClutterBox
 * @padding_top: return location for the top padding, or %NULL
 * @padding_right: return location for the right padding, or %NULL
 * @padding_bottom: return location for the bottom padding, or %NULL
 * @padding_left: return location for the left padding, or %NULL
 *
 * Gets the default padding set with clutter_box_set_default_padding().
 *
 * Since: 0.4
 */
void
clutter_box_get_default_padding (ClutterBox *box,
                                 gint       *padding_top,
                                 gint       *padding_right,
                                 gint       *padding_bottom,
                                 gint       *padding_left)
{
  g_return_if_fail (CLUTTER_IS_BOX (box));

  if (padding_top)
    *padding_top = CLUTTER_UNITS_TO_INT (box->default_padding.top);
  if (padding_right)
    *padding_right = CLUTTER_UNITS_TO_INT (box->default_padding.right);
  if (padding_bottom)
    *padding_bottom = CLUTTER_UNITS_TO_INT (box->default_padding.bottom);
  if (padding_left)
    *padding_left = CLUTTER_UNITS_TO_INT (box->default_padding.left);
}

/*
 * Boxed types
 */

static void
clutter_margin_free (ClutterMargin *margin)
{
  if (G_LIKELY (margin))
    {
      g_slice_free (ClutterMargin, margin);
    }
}

static ClutterMargin *
clutter_margin_copy (const ClutterMargin *margin)
{
  ClutterMargin *copy;

  g_return_val_if_fail (margin != NULL, NULL);

  copy = g_slice_new (ClutterMargin);
  *copy = *margin;

  return copy;
}

GType
clutter_margin_get_type (void)
{
  static GType gtype = 0;

  if (G_UNLIKELY (gtype == 0))
    gtype = g_boxed_type_register_static (g_intern_static_string ("ClutterMargin"),
                                          (GBoxedCopyFunc) clutter_margin_copy,
                                          (GBoxedFreeFunc) clutter_margin_free);

  return gtype;
}

static void
clutter_padding_free (ClutterPadding *padding)
{
  if (G_LIKELY (padding))
    {
      g_slice_free (ClutterPadding, padding);
    }
}

static ClutterPadding *
clutter_padding_copy (const ClutterPadding *padding)
{
  ClutterPadding *copy;

  g_return_val_if_fail (padding != NULL, NULL);

  copy = g_slice_new (ClutterPadding);
  *copy = *padding;

  return copy;
}

GType
clutter_padding_get_type (void)
{
  static GType gtype = 0;

  if (G_UNLIKELY (gtype == 0))
    gtype = g_boxed_type_register_static (g_intern_static_string ("ClutterPadding"),
                                          (GBoxedCopyFunc) clutter_padding_copy,
                                          (GBoxedFreeFunc) clutter_padding_free);

  return gtype;
}
