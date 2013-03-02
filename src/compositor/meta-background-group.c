/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <config.h>

#include "compositor-private.h"
#include "clutter-utils.h"
#include "meta-background-actor-private.h"
#include "meta-background-group-private.h"

G_DEFINE_TYPE (MetaBackgroundGroup, meta_background_group, CLUTTER_TYPE_GROUP);

struct _MetaBackgroundGroupPrivate
{
  ClutterLayoutManager *layout_manager;
};

static void
meta_background_group_dispose (GObject *object)
{
  G_OBJECT_CLASS (meta_background_group_parent_class)->dispose (object);
}

static gboolean
meta_background_group_get_paint_volume (ClutterActor       *actor,
                                        ClutterPaintVolume *volume)
{
  return clutter_paint_volume_set_from_allocation (volume, actor);
}

static void
meta_background_group_class_init (MetaBackgroundGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->get_paint_volume = meta_background_group_get_paint_volume;
  object_class->dispose = meta_background_group_dispose;

  g_type_class_add_private (klass, sizeof (MetaBackgroundGroupPrivate));
}

static void
meta_background_group_init (MetaBackgroundGroup *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            META_TYPE_BACKGROUND_GROUP,
                                            MetaBackgroundGroupPrivate);

  self->priv->layout_manager = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FIXED,
                                                       CLUTTER_BIN_ALIGNMENT_FIXED);

  clutter_actor_set_layout_manager (CLUTTER_ACTOR (self), self->priv->layout_manager);
}

/**
 * meta_background_group_set_visible_region:
 * @self: a #MetaBackgroundGroup
 * @visible_region: (allow-none): the parts of the background to paint
 *
 * Sets the area of the backgrounds that is unobscured by overlapping windows.
 * This is used to optimize and only paint the visible portions.
 */
void
meta_background_group_set_visible_region (MetaBackgroundGroup *self,
                                          cairo_region_t      *region)
{
  GList *children, *l;

  children = clutter_actor_get_children (CLUTTER_ACTOR (self));
  for (l = children; l; l = l->next)
    {
      ClutterActor *actor = l->data;

      if (META_IS_BACKGROUND_ACTOR (actor))
        {
          meta_background_actor_set_visible_region (META_BACKGROUND_ACTOR (actor), region);
        }
      else if (META_IS_BACKGROUND_GROUP (actor))
        {
          int x, y;

          if (!meta_actor_is_untransformed (actor, &x, &y))
            continue;

          cairo_region_translate (region, -x, -y);
          meta_background_group_set_visible_region (META_BACKGROUND_GROUP (actor), region);
          cairo_region_translate (region, x, y);
        }
    }
  g_list_free (children);
}

ClutterActor *
meta_background_group_new (void)
{
  MetaBackgroundGroup *background_group;

  background_group = g_object_new (META_TYPE_BACKGROUND_GROUP, NULL);

  return CLUTTER_ACTOR (background_group);
}
