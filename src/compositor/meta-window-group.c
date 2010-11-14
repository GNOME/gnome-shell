/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <config.h>

#define _ISOC99_SOURCE /* for roundf */
#include <math.h>

#include <gdk/gdk.h> /* for gdk_rectangle_intersect() */

#include "meta-window-actor-private.h"
#include "meta-window-group.h"
#include "meta-background-actor.h"

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
  MetaWindowGroup *window_group = META_WINDOW_GROUP (actor);
  cairo_region_t *visible_region;
  GLboolean scissor_test;
  cairo_rectangle_int_t screen_rect = { 0 };
  cairo_rectangle_int_t scissor_rect;
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

  /* When doing a partial stage paint, Clutter will set the GL scissor
   * box to the clip rectangle for the partial repaint. We combine the screen
   * rectangle with the scissor box to get the region we need to
   * paint. (Strangely, the scissor box sometimes seems to be bigger
   * than the stage ... Clutter should probably be clampimg)
   */
  glGetBooleanv (GL_SCISSOR_TEST, &scissor_test);

  if (scissor_test)
    {
      GLint scissor_box[4];
      glGetIntegerv (GL_SCISSOR_BOX, scissor_box);

      scissor_rect.x = scissor_box[0];
      scissor_rect.y = screen_rect.height - (scissor_box[1] + scissor_box[3]);
      scissor_rect.width = scissor_box[2];
      scissor_rect.height = scissor_box[3];

      gdk_rectangle_intersect (&scissor_rect, &screen_rect, &scissor_rect);
    }
  else
    {
      scissor_rect = screen_rect;
    }

  visible_region = cairo_region_create_rectangle (&scissor_rect);

  for (l = children; l; l = l->next)
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE (l->data))
        continue;

      if (META_IS_WINDOW_ACTOR (l->data))
        {
          MetaWindowActor *window_actor = l->data;
          gboolean x, y;

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
          meta_background_actor_set_visible_region (background_actor, visible_region);
        }
    }

  cairo_region_destroy (visible_region);

  CLUTTER_ACTOR_CLASS (meta_window_group_parent_class)->paint (actor);

  /* Now that we are done painting, unset the visible regions (they will
   * mess up painting clones of our actors)
   */
  for (l = children; l; l = l->next)
    {
      if (META_IS_WINDOW_ACTOR (l->data))
        {
          MetaWindowActor *window_actor = l->data;
          window_actor = l->data;
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
