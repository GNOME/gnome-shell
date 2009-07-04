/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "shell-overflow-list.h"

G_DEFINE_TYPE (ShellOverflowList,
               shell_overflow_list,
               CLUTTER_TYPE_GROUP);

enum {
  PROP_0,
  PROP_SPACING,
  PROP_ITEM_HEIGHT,
  PROP_PAGE,
  PROP_N_PAGES
};

struct _ShellOverflowListPrivate {
  guint item_height;
  guint spacing;
  guint page;
  guint n_pages;
  guint items_per_page;
};

static void
shell_overflow_list_set_property(GObject         *object,
                                 guint            prop_id,
                                 const GValue    *value,
                                 GParamSpec      *pspec)
{
  ShellOverflowList *self = SHELL_OVERFLOW_LIST (object);
  ShellOverflowListPrivate *priv = self->priv;

  switch (prop_id)
    {
      case PROP_SPACING:
        priv->spacing = g_value_get_float (value);
        clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
      break;
      case PROP_ITEM_HEIGHT:
        priv->item_height = g_value_get_float (value);
        clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
        break;
      case PROP_PAGE:
        priv->page = g_value_get_uint (value);
        clutter_actor_queue_redraw (CLUTTER_ACTOR (self));
        break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_overflow_list_get_property(GObject         *object,
                                 guint            prop_id,
                                 GValue          *value,
                                 GParamSpec      *pspec)
{
  ShellOverflowList *self = SHELL_OVERFLOW_LIST (object);
  ShellOverflowListPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_SPACING:
      g_value_set_float (value, priv->spacing);
      break;
    case PROP_ITEM_HEIGHT:
      g_value_set_float (value, priv->spacing);
      break;
    case PROP_PAGE:
      g_value_set_uint (value, priv->page);
      break;
    case PROP_N_PAGES:
      g_value_set_uint (value, priv->n_pages);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_overflow_list_allocate (ClutterActor           *actor,
                              const ClutterActorBox  *box,
                              ClutterAllocationFlags  flags)
{
  ShellOverflowList *self = SHELL_OVERFLOW_LIST (actor);
  ShellOverflowListPrivate *priv = self->priv;
  GList *children, *iter;
  int n_pages;
  int n_children;
  int n_fits;
  float width;
  float curheight;
  float avail_height;
  gboolean overflow;

  /* chain up to set actor->allocation */
  (CLUTTER_ACTOR_CLASS (g_type_class_peek (clutter_actor_get_type ())))->allocate (actor, box, flags);

  width = box->x2 - box->x1;
  curheight = 0;
  avail_height = box->y2 - box->y1;

  children = clutter_container_get_children (CLUTTER_CONTAINER (self));
  n_children = g_list_length (children);

  n_fits = 0;
  n_pages = 1;
  overflow = FALSE;
  for (iter = children; iter; iter = iter->next)
    {
      ClutterActor *actor = CLUTTER_ACTOR (iter->data);
      ClutterActorBox child_box;

      if ((curheight + priv->item_height) > avail_height)
        {
          overflow = TRUE;
          curheight = 0;
          n_pages++;
        }
      else if (!overflow)
        n_fits++;

      child_box.x1 = 0;
      child_box.x2 = width;
      child_box.y1 = curheight;
      child_box.y2 = child_box.y1 + priv->item_height;
      clutter_actor_allocate (actor, &child_box, flags);

      curheight += priv->item_height;
      if (iter != children)
        curheight += priv->spacing;
    }

  priv->items_per_page = n_fits;
  if (n_pages != priv->n_pages)
    {
      priv->n_pages = n_pages;
      g_object_notify (G_OBJECT (self), "n-pages");
    }

  g_list_free (children);
}

void
shell_overflow_list_paint (ClutterActor *actor)
{
  ShellOverflowList *self = SHELL_OVERFLOW_LIST (actor);
  ShellOverflowListPrivate *priv = self->priv;
  GList *children, *iter;
  int i;

  children = clutter_container_get_children (CLUTTER_CONTAINER (self));

  if (children == NULL)
    return;

  iter = g_list_nth (children, (priv->page) * priv->items_per_page);

  i = 0;
  for (;iter && i < priv->items_per_page; iter = iter->next, i++)
    {
      ClutterActor *actor = CLUTTER_ACTOR (iter->data);

      clutter_actor_paint (actor);
    }
  g_list_free (children);
}

static void
shell_overflow_list_get_preferred_height (ClutterActor *actor,
                                          gfloat for_width,
                                          gfloat *min_height_p,
                                          gfloat *natural_height_p)
{
  ShellOverflowList *self = SHELL_OVERFLOW_LIST (actor);
  ShellOverflowListPrivate *priv = self->priv;
  GList *children;

  if (min_height_p)
    *min_height_p = 0;

  if (natural_height_p)
    {
      int n_children;
      children = clutter_container_get_children (CLUTTER_CONTAINER (self));
      n_children = g_list_length (children);
      if (n_children == 0)
        *natural_height_p = 0;
      else
        *natural_height_p = (n_children - 1) * (priv->item_height + priv->spacing) + priv->item_height;
      g_list_free (children);
    }
}

static void
shell_overflow_list_get_preferred_width (ClutterActor *actor,
                                         gfloat for_height,
                                         gfloat *min_width_p,
                                         gfloat *natural_width_p)
{
  ShellOverflowList *self = SHELL_OVERFLOW_LIST (actor);
  gboolean first = TRUE;
  float min = 0, natural = 0;
  GList *iter;
  GList *children;

  children = clutter_container_get_children (CLUTTER_CONTAINER (self));

  for (iter = children; iter; iter = iter->next)
    {
      ClutterActor *child = iter->data;
      float child_min, child_natural;

      clutter_actor_get_preferred_width (child,
                                         for_height,
                                         &child_min,
                                         &child_natural);

      if (first)
        {
          first = FALSE;
          min = child_min;
          natural = child_natural;
        }
      else
        {
          if (child_min > min)
            min = child_min;

          if (child_natural > natural)
            natural = child_natural;
        }
    }

  if (min_width_p)
    *min_width_p = min;

  if (natural_width_p)
    *natural_width_p = natural;
  g_list_free (children);
}

static void
shell_overflow_list_class_init (ShellOverflowListClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->get_property = shell_overflow_list_get_property;
  gobject_class->set_property = shell_overflow_list_set_property;

  actor_class->get_preferred_width = shell_overflow_list_get_preferred_width;
  actor_class->get_preferred_height = shell_overflow_list_get_preferred_height;
  actor_class->allocate = shell_overflow_list_allocate;
  actor_class->paint = shell_overflow_list_paint;

  g_object_class_install_property (gobject_class,
                                   PROP_SPACING,
                                   g_param_spec_float ("spacing",
                                                        "Spacing",
                                                        "Space between items",
                                                        0.0, G_MAXFLOAT, 0.0,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ITEM_HEIGHT,
                                   g_param_spec_float ("item-height",
                                                       "Item height",
                                                       "Fixed item height value",
                                                        0.0, G_MAXFLOAT, 0.0,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_PAGE,
                                   g_param_spec_uint ("page",
                                                      "Page number",
                                                      "Page number",
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_N_PAGES,
                                   g_param_spec_uint ("n-pages",
                                                      "Number of pages",
                                                      "Number of pagest",
                                                       0, G_MAXUINT, 0,
                                                       G_PARAM_READABLE));

  g_type_class_add_private (gobject_class, sizeof (ShellOverflowListPrivate));
}


static void
shell_overflow_list_init (ShellOverflowList *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            SHELL_TYPE_OVERFLOW_LIST,
                                            ShellOverflowListPrivate);
  self->priv->n_pages = 1;
  self->priv->page = 0;
}
