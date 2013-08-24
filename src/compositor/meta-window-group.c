/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <config.h>

#define _ISOC99_SOURCE /* for roundf */
#include <math.h>

#include <gdk/gdk.h> /* for gdk_rectangle_intersect() */

#include "clutter-utils.h"
#include "compositor-private.h"
#include "meta-window-actor-private.h"
#include "meta-window-group.h"
#include "meta-background-actor-private.h"
#include "meta-background-group-private.h"
#include "window-private.h"

struct _MetaWindowGroupClass
{
  ClutterActorClass parent_class;
};

struct _MetaWindowGroup
{
  ClutterActor parent;

  MetaScreen *screen;
};

G_DEFINE_TYPE (MetaWindowGroup, meta_window_group, CLUTTER_TYPE_ACTOR);

/* Help macros to scale from OpenGL <-1,1> coordinates system to
 * window coordinates ranging [0,window-size]. Borrowed from clutter-utils.c
 */
#define MTX_GL_SCALE_X(x,w,v1,v2) ((((((x) / (w)) + 1.0f) / 2.0f) * (v1)) + (v2))
#define MTX_GL_SCALE_Y(y,w,v1,v2) ((v1) - (((((y) / (w)) + 1.0f) / 2.0f) * (v1)) + (v2))

/* Check if we're painting the MetaWindowGroup "untransformed". This can
 * differ from the result of actor_is_untransformed(window_group) if we're
 * inside a clone paint. The integer translation, if any, is returned.
 */
static gboolean
painting_untransformed (MetaWindowGroup *window_group,
                        int             *x_origin,
                        int             *y_origin)
{
  CoglMatrix modelview, projection, modelview_projection;
  ClutterVertex vertices[4];
  int width, height;
  float viewport[4];
  int i;

  cogl_get_modelview_matrix (&modelview);
  cogl_get_projection_matrix (&projection);

  cogl_matrix_multiply (&modelview_projection,
                        &projection,
                        &modelview);

  meta_screen_get_size (window_group->screen, &width, &height);

  vertices[0].x = 0;
  vertices[0].y = 0;
  vertices[0].z = 0;
  vertices[1].x = width;
  vertices[1].y = 0;
  vertices[1].z = 0;
  vertices[2].x = 0;
  vertices[2].y = height;
  vertices[2].z = 0;
  vertices[3].x = width;
  vertices[3].y = height;
  vertices[3].z = 0;

  cogl_get_viewport (viewport);

  for (i = 0; i < 4; i++)
    {
      float w = 1;
      cogl_matrix_transform_point (&modelview_projection, &vertices[i].x, &vertices[i].y, &vertices[i].z, &w);
      vertices[i].x = MTX_GL_SCALE_X (vertices[i].x, w,
                                      viewport[2], viewport[0]);
      vertices[i].y = MTX_GL_SCALE_Y (vertices[i].y, w,
                                      viewport[3], viewport[1]);
    }

  return meta_actor_vertices_are_untransformed (vertices, width, height, x_origin, y_origin);
}

static void
meta_window_group_paint (ClutterActor *actor)
{
  cairo_region_t *visible_region;
  ClutterActor *stage;
  ClutterActorIter iter;
  ClutterActor *child;
  cairo_rectangle_int_t visible_rect;
  int paint_x_origin, paint_y_origin;
  int actor_x_origin, actor_y_origin;
  int paint_x_offset, paint_y_offset;

  MetaWindowGroup *window_group = META_WINDOW_GROUP (actor);
  MetaCompScreen *info;

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
  if (!painting_untransformed (window_group, &paint_x_origin, &paint_y_origin) ||
      !meta_actor_is_untransformed (actor, &actor_x_origin, &actor_y_origin))
    {
      CLUTTER_ACTOR_CLASS (meta_window_group_parent_class)->paint (actor);
      return;
    }

  paint_x_offset = paint_x_origin - actor_x_origin;
  paint_y_offset = paint_y_origin - actor_y_origin;

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

  if (!meta_is_wayland_compositor ())
    {
      info = meta_screen_get_compositor_data (window_group->screen);
      if (info->unredirected_window != NULL)
        {
          cairo_rectangle_int_t unredirected_rect;
          MetaWindow *window = meta_window_actor_get_meta_window (info->unredirected_window);

          meta_window_get_outer_rect (window, (MetaRectangle *)&unredirected_rect);
          cairo_region_subtract_rectangle (visible_region, &unredirected_rect);
        }
    }

  /* We walk the list from top to bottom (opposite of painting order),
   * and subtract the opaque area of each window out of the visible
   * region that we pass to the windows below.
   */
  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_prev (&iter, &child))
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      if (!meta_is_wayland_compositor () &&
          info->unredirected_window != NULL &&
          child == CLUTTER_ACTOR (info->unredirected_window))
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

      if (META_IS_WINDOW_ACTOR (child))
        {
          MetaWindow *meta_window;
          MetaWindowActor *window_actor = META_WINDOW_ACTOR (child);
          int x, y;

          if (!meta_actor_is_untransformed (CLUTTER_ACTOR (window_actor), &x, &y))
            continue;

          x += paint_x_offset;
          y += paint_y_offset;

          /* Temporarily move to the coordinate system of the actor */
          cairo_region_translate (visible_region, - x, - y);

          meta_window_actor_set_visible_region (window_actor, visible_region);

          /* TODO: Track the opaque regions of wayland clients.
           * Although wayland clients can report opaque window
           * regions, for now we assume that all wayland clients are
           * transparent... */
          meta_window = meta_window_actor_get_meta_window (window_actor);

          if (meta_window->client_type != META_WINDOW_CLIENT_TYPE_WAYLAND &&
              clutter_actor_get_paint_opacity (CLUTTER_ACTOR (window_actor)) == 0xff)
            {
              cairo_region_t *obscured_region = meta_window_actor_get_obscured_region (window_actor);
              if (obscured_region)
                cairo_region_subtract (visible_region, obscured_region);
            }

          meta_window_actor_set_visible_region_beneath (window_actor, visible_region);
          cairo_region_translate (visible_region, x, y);
        }
      else if (META_IS_BACKGROUND_ACTOR (child) ||
               META_IS_BACKGROUND_GROUP (child))
        {
          int x, y;

          if (!meta_actor_is_untransformed (child, &x, &y))
            continue;

          x += paint_x_offset;
          y += paint_y_offset;

          cairo_region_translate (visible_region, - x, - y);

          if (META_IS_BACKGROUND_GROUP (child))
            meta_background_group_set_visible_region (META_BACKGROUND_GROUP (child), visible_region);
          else
            meta_background_actor_set_visible_region (META_BACKGROUND_ACTOR (child), visible_region);
          cairo_region_translate (visible_region, x, y);
        }
    }

  cairo_region_destroy (visible_region);

  CLUTTER_ACTOR_CLASS (meta_window_group_parent_class)->paint (actor);

  /* Now that we are done painting, unset the visible regions (they will
   * mess up painting clones of our actors)
   */
  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      if (META_IS_WINDOW_ACTOR (child))
        {
          MetaWindowActor *window_actor = META_WINDOW_ACTOR (child);
          meta_window_actor_reset_visible_regions (window_actor);
        }
      else if (META_IS_BACKGROUND_ACTOR (child))
        {
          MetaBackgroundActor *background_actor = META_BACKGROUND_ACTOR (child);
          meta_background_actor_set_visible_region (background_actor, NULL);
        }
    }
}

static gboolean
meta_window_group_get_paint_volume (ClutterActor       *actor,
                                    ClutterPaintVolume *volume)
{
  return clutter_paint_volume_set_from_allocation (volume, actor);
}

static void
meta_window_group_class_init (MetaWindowGroupClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint = meta_window_group_paint;
  actor_class->get_paint_volume = meta_window_group_get_paint_volume;
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
