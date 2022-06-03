/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-theme-node-transition.c: Theme node transitions for StWidget.
 *
 * Copyright 2010 Florian Müllner
 * Copyright 2010 Adel Gadllah
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>

#include "st-theme-node-transition.h"

enum {
  COMPLETED,
  NEW_FRAME,
  LAST_SIGNAL
};

typedef struct _StThemeNodeTransitionPrivate StThemeNodeTransitionPrivate;

struct _StThemeNodeTransition {
  GObject parent;

  StThemeNodeTransitionPrivate *priv;
};

struct _StThemeNodeTransitionPrivate {
  StThemeNode *old_theme_node;
  StThemeNode *new_theme_node;

  StThemeNodePaintState old_paint_state;
  StThemeNodePaintState new_paint_state;

  CoglTexture *old_texture;
  CoglTexture *new_texture;

  CoglFramebuffer *old_offscreen;
  CoglFramebuffer *new_offscreen;

  CoglPipeline *material;

  ClutterTimeline *timeline;

  gulong timeline_completed_id;
  gulong timeline_new_frame_id;

  ClutterActorBox last_allocation;
  ClutterActorBox offscreen_box;

  gboolean needs_setup;
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (StThemeNodeTransition, st_theme_node_transition, G_TYPE_OBJECT);


static void
on_timeline_completed (ClutterTimeline       *timeline,
                       StThemeNodeTransition *transition)
{
  g_signal_emit (transition, signals[COMPLETED], 0);
}

static void
on_timeline_new_frame (ClutterTimeline       *timeline,
                       gint                   frame_num,
                       StThemeNodeTransition *transition)
{
  g_signal_emit (transition, signals[NEW_FRAME], 0);
}

StThemeNodeTransition *
st_theme_node_transition_new (ClutterActor          *actor,
                              StThemeNode           *from_node,
                              StThemeNode           *to_node,
                              StThemeNodePaintState *old_paint_state,
                              unsigned int           duration)
{
  StThemeNodeTransition *transition;
  g_return_val_if_fail (ST_IS_THEME_NODE (from_node), NULL);
  g_return_val_if_fail (ST_IS_THEME_NODE (to_node), NULL);

  duration = st_theme_node_get_transition_duration (to_node);

  transition = g_object_new (ST_TYPE_THEME_NODE_TRANSITION, NULL);

  transition->priv->old_theme_node = g_object_ref (from_node);
  transition->priv->new_theme_node = g_object_ref (to_node);

  st_theme_node_paint_state_copy (&transition->priv->old_paint_state,
                                  old_paint_state);

  transition->priv->timeline = clutter_timeline_new_for_actor (actor, duration);

  transition->priv->timeline_completed_id =
    g_signal_connect (transition->priv->timeline, "completed",
                      G_CALLBACK (on_timeline_completed), transition);
  transition->priv->timeline_new_frame_id =
    g_signal_connect (transition->priv->timeline, "new-frame",
                      G_CALLBACK (on_timeline_new_frame), transition);

  clutter_timeline_set_progress_mode (transition->priv->timeline, CLUTTER_EASE_IN_OUT_QUAD);

  clutter_timeline_start (transition->priv->timeline);

  return transition;
}

/**
 * st_theme_node_transition_get_new_paint_state: (skip)
 *
 */
StThemeNodePaintState *
st_theme_node_transition_get_new_paint_state (StThemeNodeTransition *transition)
{
  return &transition->priv->new_paint_state;
}

void
st_theme_node_transition_update (StThemeNodeTransition *transition,
                                 StThemeNode           *new_node)
{
  StThemeNodeTransitionPrivate *priv;
  StThemeNode *old_node;
  ClutterTimelineDirection direction;

  g_return_if_fail (ST_IS_THEME_NODE_TRANSITION (transition));
  g_return_if_fail (ST_IS_THEME_NODE (new_node));

  priv = transition->priv;
  direction = clutter_timeline_get_direction (priv->timeline);
  old_node = (direction == CLUTTER_TIMELINE_FORWARD) ? priv->old_theme_node
                                                     : priv->new_theme_node;

  /* If the update is the reversal of the current transition,
   * we reverse the timeline.
   * Otherwise, we should initiate a new transition from the
   * current state to the new one; this is hard to do if the
   * transition is in an intermediate state, so we just cancel
   * the ongoing transition in that case.
   * Note that reversing a timeline before any time elapsed
   * results in the timeline's time position being set to the
   * full duration - this is not what we want, so we cancel the
   * transition as well in that case.
   */
  if (st_theme_node_equal (new_node, old_node))
    {
      {
        StThemeNodePaintState tmp;

        st_theme_node_paint_state_init (&tmp);
        st_theme_node_paint_state_copy (&tmp, &priv->old_paint_state);
        st_theme_node_paint_state_copy (&priv->old_paint_state, &priv->new_paint_state);
        st_theme_node_paint_state_copy (&priv->new_paint_state, &tmp);
        st_theme_node_paint_state_free (&tmp);
      }

      if (clutter_timeline_get_elapsed_time (priv->timeline) > 0)
        {
          if (direction == CLUTTER_TIMELINE_FORWARD)
            clutter_timeline_set_direction (priv->timeline,
                                            CLUTTER_TIMELINE_BACKWARD);
          else
            clutter_timeline_set_direction (priv->timeline,
                                            CLUTTER_TIMELINE_FORWARD);
        }
      else
        {
          clutter_timeline_stop (priv->timeline);
          g_signal_emit (transition, signals[COMPLETED], 0);
        }
    }
  else
    {
      if (clutter_timeline_get_elapsed_time (priv->timeline) > 0)
        {
          clutter_timeline_stop (priv->timeline);
          g_signal_emit (transition, signals[COMPLETED], 0);
        }
      else
        {
          guint new_duration = st_theme_node_get_transition_duration (new_node);

          clutter_timeline_set_duration (priv->timeline, new_duration);

          g_object_unref (priv->new_theme_node);
          priv->new_theme_node = g_object_ref (new_node);

          st_theme_node_paint_state_invalidate (&priv->new_paint_state);
        }
    }
}

static void
calculate_offscreen_box (StThemeNodeTransition *transition,
                         const ClutterActorBox *allocation)
{
  ClutterActorBox paint_box;

  st_theme_node_transition_get_paint_box (transition,
                                          allocation,
                                          &paint_box);
  transition->priv->offscreen_box.x1 = paint_box.x1 - allocation->x1;
  transition->priv->offscreen_box.y1 = paint_box.y1 - allocation->y1;
  transition->priv->offscreen_box.x2 = paint_box.x2 - allocation->x1;
  transition->priv->offscreen_box.y2 = paint_box.y2 - allocation->y1;
}

void
st_theme_node_transition_get_paint_box (StThemeNodeTransition *transition,
                                        const ClutterActorBox *allocation,
                                        ClutterActorBox       *paint_box)
{
  StThemeNodeTransitionPrivate *priv = transition->priv;
  ClutterActorBox old_node_box, new_node_box;

  st_theme_node_get_paint_box (priv->old_theme_node,
                               allocation,
                               &old_node_box);

  st_theme_node_get_paint_box (priv->new_theme_node,
                               allocation,
                               &new_node_box);

  paint_box->x1 = MIN (old_node_box.x1, new_node_box.x1);
  paint_box->y1 = MIN (old_node_box.y1, new_node_box.y1);
  paint_box->x2 = MAX (old_node_box.x2, new_node_box.x2);
  paint_box->y2 = MAX (old_node_box.y2, new_node_box.y2);
}

static gboolean
setup_framebuffers (StThemeNodeTransition *transition,
                    const ClutterActorBox *allocation,
                    float                  resource_scale)
{
  StThemeNodeTransitionPrivate *priv = transition->priv;
  CoglContext *ctx;
  guint width, height;
  GError *catch_error = NULL;

  /* template material to avoid unnecessary shader compilation */
  static CoglPipeline *material_template = NULL;

  ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  width  = ceilf ((priv->offscreen_box.x2 - priv->offscreen_box.x1) * resource_scale);
  height = ceilf ((priv->offscreen_box.y2 - priv->offscreen_box.y1) * resource_scale);

  g_return_val_if_fail (width  > 0, FALSE);
  g_return_val_if_fail (height > 0, FALSE);

  cogl_clear_object (&priv->old_texture);
  priv->old_texture = COGL_TEXTURE (cogl_texture_2d_new_with_size (ctx, width, height));

  cogl_clear_object (&priv->new_texture);
  priv->new_texture = COGL_TEXTURE (cogl_texture_2d_new_with_size (ctx, width, height));

  if (priv->old_texture == NULL)
    return FALSE;

  if (priv->new_texture == NULL)
    return FALSE;

  g_clear_object (&priv->old_offscreen);
  priv->old_offscreen = COGL_FRAMEBUFFER (cogl_offscreen_new_with_texture (priv->old_texture));
  if (!cogl_framebuffer_allocate (priv->old_offscreen, &catch_error))
    {
      g_error_free (catch_error);
      g_clear_object (&priv->old_offscreen);
      return FALSE;
    }

  g_clear_object (&priv->new_offscreen);
  priv->new_offscreen = COGL_FRAMEBUFFER (cogl_offscreen_new_with_texture (priv->new_texture));
  if (!cogl_framebuffer_allocate (priv->new_offscreen, &catch_error))
    {
      g_error_free (catch_error);
      g_clear_object (&priv->new_offscreen);
      return FALSE;
    }

  if (priv->material == NULL)
    {
      if (G_UNLIKELY (material_template == NULL))
        {
          CoglContext *ctx =
            clutter_backend_get_cogl_context (clutter_get_default_backend ());
          material_template = cogl_pipeline_new (ctx);

          cogl_pipeline_set_layer_combine (material_template, 0,
                                           "RGBA = REPLACE (TEXTURE)",
                                           NULL);
          cogl_pipeline_set_layer_combine (material_template, 1,
                                           "RGBA = INTERPOLATE (PREVIOUS, "
                                                               "TEXTURE, "
                                                               "CONSTANT[A])",
                                           NULL);
          cogl_pipeline_set_layer_combine (material_template, 2,
                                           "RGBA = MODULATE (PREVIOUS, "
                                                            "PRIMARY)",
                                           NULL);
        }
      priv->material = cogl_pipeline_copy (material_template);
    }

  cogl_pipeline_set_layer_texture (priv->material, 0, priv->new_texture);
  cogl_pipeline_set_layer_texture (priv->material, 1, priv->old_texture);

  cogl_framebuffer_clear4f (priv->old_offscreen, COGL_BUFFER_BIT_COLOR,
                            0, 0, 0, 0);
  cogl_framebuffer_orthographic (priv->old_offscreen,
                                 priv->offscreen_box.x1,
                                 priv->offscreen_box.y1,
                                 priv->offscreen_box.x2,
                                 priv->offscreen_box.y2, 0.0, 1.0);

  st_theme_node_paint (priv->old_theme_node, &priv->old_paint_state,
                       priv->old_offscreen, allocation, 255, resource_scale);

  cogl_framebuffer_clear4f (priv->new_offscreen, COGL_BUFFER_BIT_COLOR,
                            0, 0, 0, 0);
  cogl_framebuffer_orthographic (priv->new_offscreen,
                                 priv->offscreen_box.x1,
                                 priv->offscreen_box.y1,
                                 priv->offscreen_box.x2,
                                 priv->offscreen_box.y2, 0.0, 1.0);
  st_theme_node_paint (priv->new_theme_node, &priv->new_paint_state,
                       priv->new_offscreen, allocation, 255, resource_scale);

  return TRUE;
}

void
st_theme_node_transition_paint (StThemeNodeTransition *transition,
                                CoglFramebuffer       *framebuffer,
                                ClutterActorBox       *allocation,
                                guint8                 paint_opacity,
                                float                  resource_scale)
{
  StThemeNodeTransitionPrivate *priv = transition->priv;

  CoglColor constant;
  float tex_coords[] = {
    0.0, 0.0, 1.0, 1.0,
    0.0, 0.0, 1.0, 1.0,
  };

  g_return_if_fail (ST_IS_THEME_NODE (priv->old_theme_node));
  g_return_if_fail (ST_IS_THEME_NODE (priv->new_theme_node));

  if (!clutter_actor_box_equal (allocation, &priv->last_allocation))
    priv->needs_setup = TRUE;

  if (priv->needs_setup)
    {
      priv->last_allocation = *allocation;

      calculate_offscreen_box (transition, allocation);
      priv->needs_setup = clutter_actor_box_get_area (&priv->offscreen_box) == 0 ||
                          !setup_framebuffers (transition, allocation,
                                               resource_scale);

      if (priv->needs_setup) /* setting up framebuffers failed */
        return;
    }

  cogl_color_init_from_4f (&constant, 0., 0., 0.,
                           clutter_timeline_get_progress (priv->timeline));
  cogl_pipeline_set_layer_combine_constant (priv->material, 1, &constant);

  cogl_pipeline_set_color4ub (priv->material,
                              paint_opacity, paint_opacity,
                              paint_opacity, paint_opacity);

  cogl_framebuffer_draw_multitextured_rectangle (framebuffer,
                                                 priv->material,
                                                 priv->offscreen_box.x1,
                                                 priv->offscreen_box.y1,
                                                 priv->offscreen_box.x2,
                                                 priv->offscreen_box.y2,
                                                 tex_coords, 8);
}

static void
st_theme_node_transition_dispose (GObject *object)
{
  StThemeNodeTransitionPrivate *priv = ST_THEME_NODE_TRANSITION (object)->priv;

  g_clear_object (&priv->old_theme_node);
  g_clear_object (&priv->new_theme_node);

  cogl_clear_object (&priv->old_texture);
  cogl_clear_object (&priv->new_texture);

  g_clear_object (&priv->old_offscreen);
  g_clear_object (&priv->new_offscreen);

  cogl_clear_object (&priv->material);

  if (priv->timeline)
    {
      g_clear_signal_handler (&priv->timeline_completed_id, priv->timeline);
      g_clear_signal_handler (&priv->timeline_new_frame_id, priv->timeline);

      g_clear_object (&priv->timeline);
    }

  priv->timeline_completed_id = 0;
  priv->timeline_new_frame_id = 0;

  st_theme_node_paint_state_free (&priv->old_paint_state);
  st_theme_node_paint_state_free (&priv->new_paint_state);

  G_OBJECT_CLASS (st_theme_node_transition_parent_class)->dispose (object);
}

static void
st_theme_node_transition_init (StThemeNodeTransition *transition)
{
  transition->priv = st_theme_node_transition_get_instance_private (transition);

  transition->priv->old_theme_node = NULL;
  transition->priv->new_theme_node = NULL;

  transition->priv->old_texture = NULL;
  transition->priv->new_texture = NULL;

  transition->priv->old_offscreen = NULL;
  transition->priv->new_offscreen = NULL;

  st_theme_node_paint_state_init (&transition->priv->old_paint_state);
  st_theme_node_paint_state_init (&transition->priv->new_paint_state);

  transition->priv->needs_setup = TRUE;
}

static void
st_theme_node_transition_class_init (StThemeNodeTransitionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = st_theme_node_transition_dispose;

  signals[COMPLETED] =
    g_signal_new ("completed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[NEW_FRAME] =
    g_signal_new ("new-frame",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}
