/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <config.h>

#define _ISOC99_SOURCE /* for roundf */
#include <math.h>

#include <gdk/gdk.h> /* for gdk_rectangle_intersect() */

#include "compositor-private.h"
#include "meta-window-actor-private.h"
#include "meta-window-group.h"
#include "meta-background-actor-private.h"

struct _MetaWindowGroupClass
{
  ClutterGroupClass parent_class;
};

struct _MetaWindowGroup
{
  ClutterGroup parent;

  MetaScreen *screen;
};

G_DEFINE_TYPE (MetaWindowGroup, meta_window_group, CLUTTER_TYPE_GROUP);

/* We want to find out if the window is "close enough" to
 * 1:1 transform. We do that by converting the transformed coordinates
 * to 24.8 fixed-point before checking if they look right.
 */
static inline int
round_to_fixed (float x)
{
  return roundf (x * 256);
}

/* We can only (easily) apply our logic for figuring out what a window
 * obscures if is not transformed. This function does that check and
 * as a side effect gets the position of the upper-left corner of the
 * actors.
 *
 * (We actually could handle scaled and non-integrally positioned actors
 * too as long as they weren't shaped - no filtering is done at the
 * edges so a rectangle stays a rectangle. But the gain from that is
 * small, especally since most of our windows are shaped. The simple
 * case we handle here is the case that matters when the user is just
 * using the desktop normally.)
 *
 * If we assume that the window group is untransformed (it better not
 * be!) then we could also make this determination by checking directly
 * if the actor itself is rotated, scaled, or at a non-integral position.
 * However, the criterion for "close enough" in that case get trickier,
 * since, for example, the allowed rotation depends on the size of
 * actor. The approach we take here is to just require everything
 * to be within 1/256th of a pixel.
 */
static gboolean
actor_is_untransformed (ClutterActor *actor,
                        int          *x_origin,
                        int          *y_origin)
{
  gfloat widthf, heightf;
  int width, height;
  ClutterVertex verts[4];
  int v0x, v0y, v1x, v1y, v2x, v2y, v3x, v3y;
  int x, y;

  clutter_actor_get_size (actor, &widthf, &heightf);
  width = round_to_fixed (widthf); height = round_to_fixed (heightf);

  clutter_actor_get_abs_allocation_vertices (actor, verts);
  v0x = round_to_fixed (verts[0].x); v0y = round_to_fixed (verts[0].y);
  v1x = round_to_fixed (verts[1].x); v1y = round_to_fixed (verts[1].y);
  v2x = round_to_fixed (verts[2].x); v2y = round_to_fixed (verts[2].y);
  v3x = round_to_fixed (verts[3].x); v3y = round_to_fixed (verts[3].y);

  /* Using shifting for converting fixed => int, gets things right for
   * negative values. / 256. wouldn't do the same
   */
  x = v0x >> 8;
  y = v0y >> 8;

  /* At integral coordinates? */
  if (x * 256 != v0x || y * 256 != v0y)
    return FALSE;

  /* Not scaled? */
  if (v1x - v0x != width || v2y - v0y != height)
    return FALSE;

  /* Not rotated/skewed? */
  if (v0x != v2x || v0y != v1y ||
      v3x != v1x || v3y != v2y)
    return FALSE;

  *x_origin = x;
  *y_origin = y;

  return TRUE;
}

static void
meta_window_group_paint (ClutterActor *actor)
{
  cairo_region_t *visible_region;
  cairo_region_t *unredirected_window_region = NULL;
  ClutterActor *stage;
  cairo_rectangle_int_t visible_rect, unredirected_rect;
  GList *children, *l;
  gfloat group_x, group_y;

  MetaWindowGroup *window_group = META_WINDOW_GROUP (actor);
  MetaCompScreen *info = meta_screen_get_compositor_data (window_group->screen);
  if (info->unredirected_window != NULL)
    {
      meta_window_actor_get_shape_bounds (META_WINDOW_ACTOR (info->unredirected_window), &unredirected_rect);
      unredirected_window_region = cairo_region_create_rectangle (&unredirected_rect);
    }

  clutter_actor_get_position (CLUTTER_ACTOR (window_group), &group_x, &group_y);

  /* We walk the list from top to bottom (opposite of painting order),
   * and subtract the opaque area of each window out of the visible
   * region that we pass to the windows below.
   */
  children = clutter_container_get_children (CLUTTER_CONTAINER (actor));
  children = g_list_reverse (children);

  /* Get the clipped redraw bounds from Clutter so that we can avoid
   * painting shadows on windows that don't need to be painted in this
   * frame. In the case of a multihead setup with mismatched monitor
   * sizes, we could intersect this with an accurate union of the
   * monitors to avoid painting shadows that are visible only in the
   * holes. */
  stage = clutter_actor_get_stage (actor);
  clutter_stage_get_redraw_clip_bounds (CLUTTER_STAGE (stage),
                                        &visible_rect);

  visible_region = cairo_region_create_rectangle (&visible_rect);

  if (unredirected_window_region)
    cairo_region_subtract (visible_region, unredirected_window_region);

  for (l = children; l; l = l->next)
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE (l->data))
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
      if (clutter_actor_has_effects (l->data))
        continue;

      if (META_IS_WINDOW_ACTOR (l->data))
        {
          MetaWindowActor *window_actor = l->data;
          int x, y;

          if (!actor_is_untransformed (CLUTTER_ACTOR (window_actor), &x, &y))
            continue;

          /* Temporarily move to the coordinate system of the actor */
          cairo_region_translate (visible_region, - x, - y);

          meta_window_actor_set_visible_region (window_actor, visible_region);

          if (clutter_actor_get_paint_opacity (CLUTTER_ACTOR (window_actor)) == 0xff)
            {
              cairo_region_t *obscured_region = meta_window_actor_get_obscured_region (window_actor);
              if (obscured_region)
                cairo_region_subtract (visible_region, obscured_region);
            }

          meta_window_actor_set_visible_region_beneath (window_actor, visible_region);
          cairo_region_translate (visible_region, x, y);
        }
      else if (META_IS_BACKGROUND_ACTOR (l->data))
        {
          MetaBackgroundActor *background_actor = l->data;
          int x, y;

          if (!actor_is_untransformed (CLUTTER_ACTOR (background_actor), &x, &y))
            continue;

          cairo_region_translate (visible_region, - x, - y);
          meta_background_actor_set_visible_region (background_actor, visible_region);
          cairo_region_translate (visible_region, x, y);
        }
    }

  cairo_region_destroy (visible_region);

  if (unredirected_window_region)
    cairo_region_destroy (unredirected_window_region);

  CLUTTER_ACTOR_CLASS (meta_window_group_parent_class)->paint (actor);

  /* Now that we are done painting, unset the visible regions (they will
   * mess up painting clones of our actors)
   */
  for (l = children; l; l = l->next)
    {
      if (META_IS_WINDOW_ACTOR (l->data))
        {
          MetaWindowActor *window_actor = l->data;
          meta_window_actor_reset_visible_regions (window_actor);
        }
      else if (META_IS_BACKGROUND_ACTOR (l->data))
        {
          MetaBackgroundActor *background_actor = l->data;
          meta_background_actor_set_visible_region (background_actor, NULL);
        }
    }

  g_list_free (children);
}

static void
meta_window_group_class_init (MetaWindowGroupClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint = meta_window_group_paint;
}

static void
meta_window_group_init (MetaWindowGroup *window_group)
{
}

ClutterActor *
meta_window_group_new (MetaScreen *screen)
{
  MetaWindowGroup *window_group;

  window_group = g_object_new (META_TYPE_WINDOW_GROUP, NULL);

  window_group->screen = screen;

  return CLUTTER_ACTOR (window_group);
}
