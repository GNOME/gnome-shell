/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#define _ISOC99_SOURCE /* for roundf */
#include <math.h>

#include "mutter-window-private.h"
#include "mutter-window-group.h"

struct _MutterWindowGroupClass
{
  ClutterGroupClass parent_class;
};

struct _MutterWindowGroup
{
  ClutterGroup parent;

  MetaScreen *screen;
};

G_DEFINE_TYPE (MutterWindowGroup, mutter_window_group, CLUTTER_TYPE_GROUP);

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
mutter_window_group_paint (ClutterActor *actor)
{
  MutterWindowGroup *window_group = MUTTER_WINDOW_GROUP (actor);
  GdkRegion *visible_region;
  GdkRectangle screen_rect = { 0 };
  GList *children, *l;

  /* We walk the list from top to bottom (opposite of painting order),
   * and subtract the opaque area of each window out of the visible
   * region that we pass to the windows below.
   */
  children = clutter_container_get_children (CLUTTER_CONTAINER (actor));
  children = g_list_reverse (children);

  /* Start off with the full screen area (for a multihead setup, we
   * might want to use a more accurate union of the monitors to avoid
   * painting in holes from mismatched monitor sizes. That's just an
   * optimization, however.)
   */
  meta_screen_get_size (window_group->screen, &screen_rect.width, &screen_rect.height);
  visible_region = gdk_region_rectangle (&screen_rect);

  for (l = children; l; l = l->next)
    {
      MutterWindow *cw;
      gboolean x, y;

      if (!MUTTER_IS_WINDOW (l->data) || !CLUTTER_ACTOR_IS_VISIBLE (l->data))
        continue;

      cw = l->data;

      if (!actor_is_untransformed (CLUTTER_ACTOR (cw), &x, &y))
        continue;

      /* Temporarily move to the coordinate system of the actor */
      gdk_region_offset (visible_region, - x, - y);

      mutter_window_set_visible_region (cw, visible_region);

      if (clutter_actor_get_paint_opacity (CLUTTER_ACTOR (cw)) == 0xff)
        {
          GdkRegion *obscured_region = mutter_window_get_obscured_region (cw);
          if (obscured_region)
            gdk_region_subtract (visible_region, obscured_region);
        }

      mutter_window_set_visible_region_beneath (cw, visible_region);
      gdk_region_offset (visible_region, x, y);
    }

  gdk_region_destroy (visible_region);

  CLUTTER_ACTOR_CLASS (mutter_window_group_parent_class)->paint (actor);

  /* Now that we are done painting, unset the visible regions (they will
   * mess up painting clones of our actors)
   */
  for (l = children; l; l = l->next)
    {
      MutterWindow *cw;

      if (!MUTTER_IS_WINDOW (l->data))
        continue;

      cw = l->data;
      mutter_window_reset_visible_regions (cw);
    }

  g_list_free (children);
}

static void
mutter_window_group_class_init (MutterWindowGroupClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint = mutter_window_group_paint;
}

static void
mutter_window_group_init (MutterWindowGroup *window_group)
{
}

ClutterActor *
mutter_window_group_new (MetaScreen *screen)
{
  MutterWindowGroup *window_group;

  window_group = g_object_new (MUTTER_TYPE_WINDOW_GROUP, NULL);

  window_group->screen = screen;

  return CLUTTER_ACTOR (window_group);
}
