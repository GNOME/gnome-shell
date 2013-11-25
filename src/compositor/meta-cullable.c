/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2013 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Owen Taylor <otaylor@redhat.com>
 *     Ray Strode <rstrode@redhat.com>
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"
#include "meta-cullable.h"
#include "clutter-utils.h"

G_DEFINE_INTERFACE (MetaCullable, meta_cullable, CLUTTER_TYPE_ACTOR);

/**
 * SECTION:meta-cullable
 * @title: MetaCullable
 * @short_description: CPU culling operations for efficient drawing
 *
 * When we are painting a stack of 5-10 large actors, the standard
 * bottom-to-top method of drawing every actor results in a tremendous
 * amount of overdraw. If these actors are painting textures like
 * windows, it can easily max out the available memory bandwidth on a
 * low-end graphics chipset. It's even worse if window textures are
 * being accessed over the AGP bus.
 *
 * #MetaCullable is our solution. The basic technique applied here is to
 * do a pre-pass before painting where we walk each actor from top to bottom
 * and ask each actor to "cull itself out". We pass in a region it can copy
 * to clip its drawing to, and the actor can subtract its fully opaque pixels
 * so that actors underneath know not to draw there as well.
 */

/**
 * meta_cullable_cull_out_children:
 * @cullable: The #MetaCullable
 * @unobscured_region: The unobscured region, as passed into cull_out()
 * @clip_region: The clip region, as passed into cull_out()
 *
 * This is a helper method for actors that want to recurse over their
 * child actors, and cull them out.
 *
 * See #MetaCullable and meta_cullable_cull_out() for more details.
 */
void
meta_cullable_cull_out_children (MetaCullable   *cullable,
                                 cairo_region_t *unobscured_region,
                                 cairo_region_t *clip_region)
{
  ClutterActor *actor = CLUTTER_ACTOR (cullable);
  ClutterActor *child;
  ClutterActorIter iter;

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_prev (&iter, &child))
    {
      float x, y;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      /* If an actor has effects applied, then that can change the area
       * it paints and the opacity, so we no longer can figure out what
       * portion of the actor is obscured and what portion of the screen
       * it obscures, so we skip the actor.
       *
       * This has a secondary beneficial effect: if a ClutterOffscreenEffect
       * is applied to an actor, then our clipped redraws interfere with the
       * caching of the FBO - even if we only need to draw a small portion
       * of the window right now, ClutterOffscreenEffect may use other portions
       * of the FBO later. So, skipping actors with effects applied also
       * prevents these bugs.
       *
       * Theoretically, we should check clutter_actor_get_offscreen_redirect()
       * as well for the same reason, but omitted for simplicity in the
       * hopes that no-one will do that.
       */
      if (clutter_actor_has_effects (child))
        continue;

      if (!META_IS_CULLABLE (child))
        continue;

      if (!meta_actor_is_untransformed (child, NULL, NULL))
        continue;

      clutter_actor_get_position (child, &x, &y);

      /* Temporarily move to the coordinate system of the actor */
      cairo_region_translate (unobscured_region, - x, - y);
      cairo_region_translate (clip_region, - x, - y);

      meta_cullable_cull_out (META_CULLABLE (child), unobscured_region, clip_region);

      cairo_region_translate (unobscured_region, x, y);
      cairo_region_translate (clip_region, x, y);
    }
}

/**
 * meta_cullable_reset_culling_children:
 * @cullable: The #MetaCullable
 *
 * This is a helper method for actors that want to recurse over their
 * child actors, and cull them out.
 *
 * See #MetaCullable and meta_cullable_reset_culling() for more details.
 */
void
meta_cullable_reset_culling_children (MetaCullable *cullable)
{
  ClutterActor *actor = CLUTTER_ACTOR (cullable);
  ClutterActor *child;
  ClutterActorIter iter;

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      if (!META_IS_CULLABLE (child))
        continue;

      meta_cullable_reset_culling (META_CULLABLE (child));
    }
}

static void
meta_cullable_default_init (MetaCullableInterface *iface)
{
}

/**
 * meta_cullable_cull_out:
 * @cullable: The #MetaCullable
 * @unobscured_region: The unobscured region, in @cullable's space.
 * @clip_region: The clip region, in @cullable's space.
 *
 * When #MetaWindowGroup is painted, we walk over its direct cullable
 * children from top to bottom and ask themselves to "cull out". Cullables
 * can use @unobscured_region and @clip_region to clip their drawing. Actors
 * interested in eliminating overdraw should copy the @clip_region and only
 * paint those parts, as everything else has been obscured by actors above it.
 *
 * Actors that may have fully opaque parts should also subtract out a region
 * that is fully opaque from @unobscured_region and @clip_region.
 *
 * @unobscured_region and @clip_region are extremely similar. The difference
 * is that @clip_region starts off with the stage's clip, if Clutter detects
 * that we're doing a clipped redraw. @unobscured_region, however, starts off
 * with the full stage size, so actors that may want to record what parts of
 * their window are unobscured for e.g. scheduling repaints can do so.
 *
 * Actors that have children can also use the meta_cullable_cull_out_children()
 * helper method to do a simple cull across all their children.
 */
void
meta_cullable_cull_out (MetaCullable   *cullable,
                        cairo_region_t *unobscured_region,
                        cairo_region_t *clip_region)
{
  META_CULLABLE_GET_IFACE (cullable)->cull_out (cullable, unobscured_region, clip_region);
}

/**
 * meta_cullable_reset_culling:
 * @cullable: The #MetaCullable
 *
 * Actors that copied data in their cull_out() implementation can now
 * reset their data, as the paint is now over. Additional paints may be
 * done by #ClutterClone or similar, and they should not be affected by
 * the culling operation.
 */
void
meta_cullable_reset_culling (MetaCullable *cullable)
{
  META_CULLABLE_GET_IFACE (cullable)->reset_culling (cullable);
}
