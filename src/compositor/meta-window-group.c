/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <config.h>

#define _ISOC99_SOURCE /* for roundf */
#include <math.h>

#include <gdk/gdk.h> /* for gdk_rectangle_intersect() */

#include "clutter-utils.h"
#include "compositor-private.h"
#include "meta-window-actor-private.h"
#include "meta-window-group.h"
#include "window-private.h"
#include "meta-cullable.h"

struct _MetaWindowGroupClass
{
  ClutterActorClass parent_class;
};

struct _MetaWindowGroup
{
  ClutterActor parent;

  MetaScreen *screen;
};

static void cullable_iface_init (MetaCullableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaWindowGroup, meta_window_group, CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (META_TYPE_CULLABLE, cullable_iface_init));

static void
meta_window_group_cull_out (MetaCullable   *cullable,
                            cairo_region_t *unobscured_region,
                            cairo_region_t *clip_region)
{
  meta_cullable_cull_out_children (cullable, unobscured_region, clip_region);
}

static void
meta_window_group_reset_culling (MetaCullable *cullable)
{
  meta_cullable_reset_culling_children (cullable);
}

static void
cullable_iface_init (MetaCullableInterface *iface)
{
  iface->cull_out = meta_window_group_cull_out;
  iface->reset_culling = meta_window_group_reset_culling;
}

static void
meta_window_group_paint (ClutterActor *actor)
{
  cairo_region_t *clip_region;
  cairo_region_t *unobscured_region;
  cairo_rectangle_int_t visible_rect, clip_rect;
  int paint_x_origin, paint_y_origin;
  int screen_width, screen_height;

  MetaWindowGroup *window_group = META_WINDOW_GROUP (actor);
  ClutterActor *stage = clutter_actor_get_stage (actor);

  meta_screen_get_size (window_group->screen, &screen_width, &screen_height);

  /* Normally we expect an actor to be drawn at it's position on the screen.
   * However, if we're inside the paint of a ClutterClone, that won't be the
   * case and we need to compensate. We look at the position of the window
   * group under the current model-view matrix and the position of the actor.
   * If they are both simply integer translations, then we can compensate
   * easily, otherwise we give up.
   *
   * Possible cleanup: work entirely in paint space - we can compute the
   * combination of the model-view matrix with the local matrix for each child
   * actor and get a total transformation for that actor for how we are
   * painting currently, and never worry about how actors are positioned
   * on the stage.
   */
  if (clutter_actor_is_in_clone_paint (actor))
    {
      if (!meta_actor_painting_untransformed (screen_width,
                                              screen_height,
                                              &paint_x_origin,
                                              &paint_y_origin) ||
          !meta_actor_is_untransformed (actor, NULL, NULL))
        {
          CLUTTER_ACTOR_CLASS (meta_window_group_parent_class)->paint (actor);
          return;
        }
    }
  else
    {
      paint_x_origin = 0;
      paint_y_origin = 0;
    }

  visible_rect.x = visible_rect.y = 0;
  visible_rect.width = clutter_actor_get_width (CLUTTER_ACTOR (stage));
  visible_rect.height = clutter_actor_get_height (CLUTTER_ACTOR (stage));

  unobscured_region = cairo_region_create_rectangle (&visible_rect);

  /* Get the clipped redraw bounds from Clutter so that we can avoid
   * painting shadows on windows that don't need to be painted in this
   * frame. In the case of a multihead setup with mismatched monitor
   * sizes, we could intersect this with an accurate union of the
   * monitors to avoid painting shadows that are visible only in the
   * holes. */
  clutter_stage_get_redraw_clip_bounds (CLUTTER_STAGE (stage),
                                        &clip_rect);

  clip_region = cairo_region_create_rectangle (&clip_rect);

  cairo_region_translate (clip_region, -paint_x_origin, -paint_y_origin);

  meta_cullable_cull_out (META_CULLABLE (window_group), unobscured_region, clip_region);

  cairo_region_destroy (unobscured_region);
  cairo_region_destroy (clip_region);

  CLUTTER_ACTOR_CLASS (meta_window_group_parent_class)->paint (actor);

  meta_cullable_reset_culling (META_CULLABLE (window_group));
}

/* Adapted from clutter_actor_update_default_paint_volume() */
static gboolean
meta_window_group_get_paint_volume (ClutterActor       *self,
                                    ClutterPaintVolume *volume)
{
  ClutterActorIter iter;
  ClutterActor *child;

  clutter_actor_iter_init (&iter, self);
  while (clutter_actor_iter_next (&iter, &child))
    {
      const ClutterPaintVolume *child_volume;

      if (!CLUTTER_ACTOR_IS_MAPPED (child))
        continue;

      child_volume = clutter_actor_get_transformed_paint_volume (child, self);
      if (child_volume == NULL)
        return FALSE;

      clutter_paint_volume_union (volume, child_volume);
    }

  return TRUE;
}

/* This is a workaround for Clutter's awful allocation tracking.
 * Without this, any time the window group changed size, which is
 * any time windows are dragged around, we'll do a full repaint
 * of the window group, which includes the background actor, meaning
 * a full-stage repaint.
 *
 * Since actors are allowed to paint outside their allocation, and
 * since child actors are allowed to be outside their parents, this
 * doesn't affect anything, but it means that we'll get much more
 * sane and consistent clipped repaints from Clutter. */
static void
meta_window_group_get_preferred_width (ClutterActor *actor,
                                       gfloat        for_height,
                                       gfloat       *min_width,
                                       gfloat       *nat_width)
{
  *min_width = 0;
  *nat_width = 0;
}

static void
meta_window_group_get_preferred_height (ClutterActor *actor,
                                        gfloat        for_width,
                                        gfloat       *min_height,
                                        gfloat       *nat_height)
{
  *min_height = 0;
  *nat_height = 0;
}

static void
meta_window_group_class_init (MetaWindowGroupClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint = meta_window_group_paint;
  actor_class->get_paint_volume = meta_window_group_get_paint_volume;
  actor_class->get_preferred_width = meta_window_group_get_preferred_width;
  actor_class->get_preferred_height = meta_window_group_get_preferred_height;
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
