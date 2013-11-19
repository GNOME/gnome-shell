/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:meta-background-group
 * @title: MetaBackgroundGroup
 * @short_description: Container for background actors
 *
 * This class is a subclass of ClutterActor with special handling for
 * MetaBackgroundActor/MetaBackgroundGroup when painting children.
 * It makes sure to only draw the parts of the backgrounds not
 * occluded by opaque windows.
 *
 * See #MetaWindowGroup for more information behind the motivation,
 * and details on implementation.
 */

#include <config.h>

#include "compositor-private.h"
#include "clutter-utils.h"
#include "meta-background-actor-private.h"
#include "meta-background-group-private.h"

G_DEFINE_TYPE (MetaBackgroundGroup, meta_background_group, CLUTTER_TYPE_ACTOR);

static void
meta_background_group_class_init (MetaBackgroundGroupClass *klass)
{
}

static void
meta_background_group_init (MetaBackgroundGroup *self)
{
}

/**
 * meta_background_group_set_clip_region:
 * @self: a #MetaBackgroundGroup
 * @region: (allow-none): the parts of the background to paint
 *
 * Sets the area of the backgrounds that is unobscured by overlapping windows.
 * This is used to optimize and only paint the visible portions.
 */
void
meta_background_group_set_clip_region (MetaBackgroundGroup *self,
                                       cairo_region_t      *region)
{
  ClutterActor *child;
  for (child = clutter_actor_get_first_child (self);
       child != NULL;
       child = clutter_actor_get_next_sibling (child))
    {
      if (META_IS_BACKGROUND_ACTOR (child))
        {
          meta_background_actor_set_clip_region (META_BACKGROUND_ACTOR (child), region);
        }
      else if (META_IS_BACKGROUND_GROUP (child))
        {
          int x, y;

          if (!meta_actor_is_untransformed (child, &x, &y))
            continue;

          cairo_region_translate (region, -x, -y);
          meta_background_group_set_clip_region (META_BACKGROUND_GROUP (child), region);
          cairo_region_translate (region, x, y);
        }
    }
}

ClutterActor *
meta_background_group_new (void)
{
  MetaBackgroundGroup *background_group;

  background_group = g_object_new (META_TYPE_BACKGROUND_GROUP, NULL);

  return CLUTTER_ACTOR (background_group);
}
