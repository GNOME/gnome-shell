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

typedef struct _StThemeNodeTransition {
  GObject parent;

  StThemeNode *old_theme_node;
  StThemeNode *new_theme_node;

  StThemeNodePaintState old_paint_state;
  StThemeNodePaintState new_paint_state;

  CoglTexture *old_texture;
  CoglTexture *new_texture;

  CoglFramebuffer *old_offscreen;
  CoglFramebuffer *new_offscreen;

  CoglPipeline *pipeline;

  ClutterTimeline *timeline;

  gulong timeline_completed_id;
  gulong timeline_new_frame_id;

  ClutterActorBox last_allocation;
  ClutterActorBox offscreen_box;

  gboolean needs_setup;
} StThemeNodeTransition;

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_FINAL_TYPE (StThemeNodeTransition, st_theme_node_transition, G_TYPE_OBJECT);


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

  transition->old_theme_node = g_object_ref (from_node);
  transition->new_theme_node = g_object_ref (to_node);

  st_theme_node_paint_state_copy (&transition->old_paint_state,
                                  old_paint_state);

  transition->timeline = clutter_timeline_new_for_actor (actor, duration);

  transition->timeline_completed_id =
    g_signal_connect (transition->timeline, "completed",
                      G_CALLBACK (on_timeline_completed), transition);
  transition->timeline_new_frame_id =
    g_signal_connect (transition->timeline, "new-frame",
                      G_CALLBACK (on_timeline_new_frame), transition);

  clutter_timeline_set_progress_mode (transition->timeline, CLUTTER_EASE_IN_OUT_QUAD);

  clutter_timeline_start (transition->timeline);

  return transition;
}

/**
 * st_theme_node_transition_get_new_paint_state: (skip)
 *
 */
StThemeNodePaintState *
st_theme_node_transition_get_new_paint_state (StThemeNodeTransition *transition)
{
  return &transition->new_paint_state;
}

void
st_theme_node_transition_update (StThemeNodeTransition *transition,
                                 StThemeNode           *new_node)
{
  StThemeNode *old_node;
  ClutterTimelineDirection direction;

  g_return_if_fail (ST_IS_THEME_NODE_TRANSITION (transition));
  g_return_if_fail (ST_IS_THEME_NODE (new_node));

  direction = clutter_timeline_get_direction (transition->timeline);
  old_node = (direction == CLUTTER_TIMELINE_FORWARD) ? transition->old_theme_node
                                                     : transition->new_theme_node;

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
        st_theme_node_paint_state_copy (&tmp, &transition->old_paint_state);
        st_theme_node_paint_state_copy (&transition->old_paint_state, &transition->new_paint_state);
        st_theme_node_paint_state_copy (&transition->new_paint_state, &tmp);
        st_theme_node_paint_state_free (&tmp);
      }

      if (clutter_timeline_get_elapsed_time (transition->timeline) > 0)
        {
          if (direction == CLUTTER_TIMELINE_FORWARD)
            clutter_timeline_set_direction (transition->timeline,
                                            CLUTTER_TIMELINE_BACKWARD);
          else
            clutter_timeline_set_direction (transition->timeline,
                                            CLUTTER_TIMELINE_FORWARD);
        }
      else
        {
          clutter_timeline_stop (transition->timeline);
          g_signal_emit (transition, signals[COMPLETED], 0);
        }
    }
  else
    {
      if (clutter_timeline_get_elapsed_time (transition->timeline) > 0)
        {
          clutter_timeline_stop (transition->timeline);
          g_signal_emit (transition, signals[COMPLETED], 0);
        }
      else
        {
          guint new_duration = st_theme_node_get_transition_duration (new_node);

          clutter_timeline_set_duration (transition->timeline, new_duration);

          g_object_unref (transition->new_theme_node);
          transition->new_theme_node = g_object_ref (new_node);

          st_theme_node_paint_state_invalidate (&transition->new_paint_state);
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
  transition->offscreen_box.x1 = paint_box.x1 - allocation->x1;
  transition->offscreen_box.y1 = paint_box.y1 - allocation->y1;
  transition->offscreen_box.x2 = paint_box.x2 - allocation->x1;
  transition->offscreen_box.y2 = paint_box.y2 - allocation->y1;
}

void
st_theme_node_transition_get_paint_box (StThemeNodeTransition *transition,
                                        const ClutterActorBox *allocation,
                                        ClutterActorBox       *paint_box)
{
  ClutterActorBox old_node_box, new_node_box;

  st_theme_node_get_paint_box (transition->old_theme_node,
                               allocation,
                               &old_node_box);

  st_theme_node_get_paint_box (transition->new_theme_node,
                               allocation,
                               &new_node_box);

  paint_box->x1 = MIN (old_node_box.x1, new_node_box.x1);
  paint_box->y1 = MIN (old_node_box.y1, new_node_box.y1);
  paint_box->x2 = MAX (old_node_box.x2, new_node_box.x2);
  paint_box->y2 = MAX (old_node_box.y2, new_node_box.y2);
}

static gboolean
setup_framebuffers (StThemeNodeTransition *transition,
                    CoglContext           *ctx,
                    ClutterPaintContext   *paint_context,
                    ClutterPaintNode      *node,
                    const ClutterActorBox *allocation,
                    float                  resource_scale)
{
  g_autoptr (ClutterPaintNode) old_layer_node = NULL;
  g_autoptr (ClutterPaintNode) new_layer_node = NULL;
  CoglPipeline *noop_pipeline;
  guint width, height;
  GError *catch_error = NULL;

  /* template pipeline to avoid unnecessary shader compilation */
  static CoglPipeline *pipeline_template = NULL;

  width  = ceilf ((transition->offscreen_box.x2 - transition->offscreen_box.x1) * resource_scale);
  height = ceilf ((transition->offscreen_box.y2 - transition->offscreen_box.y1) * resource_scale);

  g_return_val_if_fail (width  > 0, FALSE);
  g_return_val_if_fail (height > 0, FALSE);

  g_clear_object (&transition->old_texture);
  transition->old_texture = cogl_texture_2d_new_with_size (ctx, width, height);

  g_clear_object (&transition->new_texture);
  transition->new_texture = cogl_texture_2d_new_with_size (ctx, width, height);

  if (transition->old_texture == NULL)
    return FALSE;

  if (transition->new_texture == NULL)
    return FALSE;

  g_clear_object (&transition->old_offscreen);
  transition->old_offscreen = COGL_FRAMEBUFFER (cogl_offscreen_new_with_texture (transition->old_texture));
  if (!cogl_framebuffer_allocate (transition->old_offscreen, &catch_error))
    {
      g_error_free (catch_error);
      g_clear_object (&transition->old_offscreen);
      return FALSE;
    }

  g_clear_object (&transition->new_offscreen);
  transition->new_offscreen = COGL_FRAMEBUFFER (cogl_offscreen_new_with_texture (transition->new_texture));
  if (!cogl_framebuffer_allocate (transition->new_offscreen, &catch_error))
    {
      g_error_free (catch_error);
      g_clear_object (&transition->new_offscreen);
      return FALSE;
    }

  if (transition->pipeline == NULL)
    {
      if (G_UNLIKELY (pipeline_template == NULL))
        {
          pipeline_template = cogl_pipeline_new (ctx);

          cogl_pipeline_set_layer_combine (pipeline_template, 0,
                                           "RGBA = REPLACE (TEXTURE)",
                                           NULL);
          cogl_pipeline_set_layer_combine (pipeline_template, 1,
                                           "RGBA = INTERPOLATE (PREVIOUS, "
                                                               "TEXTURE, "
                                                               "CONSTANT[A])",
                                           NULL);
          cogl_pipeline_set_layer_combine (pipeline_template, 2,
                                           "RGBA = MODULATE (PREVIOUS, "
                                                            "PRIMARY)",
                                           NULL);
        }
      transition->pipeline = cogl_pipeline_copy (pipeline_template);
    }

  cogl_pipeline_set_layer_texture (transition->pipeline, 0, transition->new_texture);
  cogl_pipeline_set_layer_texture (transition->pipeline, 1, transition->old_texture);

  noop_pipeline = cogl_pipeline_new (ctx);
  cogl_framebuffer_orthographic (transition->old_offscreen,
                                 transition->offscreen_box.x1,
                                 transition->offscreen_box.y1,
                                 transition->offscreen_box.x2,
                                 transition->offscreen_box.y2, 0.0, 1.0);

  old_layer_node = clutter_layer_node_new_to_framebuffer (transition->old_offscreen,
                                                          noop_pipeline);
  clutter_paint_node_add_child (node, old_layer_node);

  st_theme_node_paint (transition->old_theme_node, &transition->old_paint_state,
                       ctx, paint_context,
                       old_layer_node, allocation, 255, resource_scale);

  new_layer_node = clutter_layer_node_new_to_framebuffer (transition->new_offscreen,
                                                          noop_pipeline);
  clutter_paint_node_add_child (node, new_layer_node);
  cogl_framebuffer_orthographic (transition->new_offscreen,
                                 transition->offscreen_box.x1,
                                 transition->offscreen_box.y1,
                                 transition->offscreen_box.x2,
                                 transition->offscreen_box.y2, 0.0, 1.0);
  st_theme_node_paint (transition->new_theme_node, &transition->new_paint_state,
                       ctx, paint_context,
                       new_layer_node, allocation, 255, resource_scale);

  g_clear_object (&noop_pipeline);

  return TRUE;
}

void
st_theme_node_transition_paint (StThemeNodeTransition *transition,
                                CoglContext           *cogl_context,
                                ClutterPaintContext   *paint_context,
                                ClutterPaintNode      *node,
                                ClutterActorBox       *allocation,
                                guint8                 paint_opacity,
                                float                  resource_scale)
{
  g_autoptr (ClutterPaintNode) pipeline_node = NULL;

  CoglColor constant, pipeline_color;
  float tex_coords[] = {
    0.0, 0.0, 1.0, 1.0,
    0.0, 0.0, 1.0, 1.0,
  };

  g_return_if_fail (ST_IS_THEME_NODE (transition->old_theme_node));
  g_return_if_fail (ST_IS_THEME_NODE (transition->new_theme_node));

  if (!clutter_actor_box_equal (allocation, &transition->last_allocation))
    transition->needs_setup = TRUE;

  if (transition->needs_setup)
    {
      transition->last_allocation = *allocation;

      calculate_offscreen_box (transition, allocation);
      transition->needs_setup = clutter_actor_box_get_area (&transition->offscreen_box) == 0 ||
                                !setup_framebuffers (transition,
                                                     cogl_context,
                                                     paint_context,
                                                     node,
                                                     allocation,
                                                     resource_scale);

      if (transition->needs_setup) /* setting up framebuffers failed */
        return;
    }

  cogl_color_init_from_4f (&constant, 0., 0., 0.,
                           clutter_timeline_get_progress (transition->timeline));
  cogl_pipeline_set_layer_combine_constant (transition->pipeline, 1, &constant);

  cogl_color_init_from_4f (&pipeline_color,
                           paint_opacity / 255.0, paint_opacity / 255.0,
                           paint_opacity / 255.0, paint_opacity / 255.0);
  cogl_pipeline_set_color (transition->pipeline, &pipeline_color);

  pipeline_node = clutter_pipeline_node_new (transition->pipeline);
  clutter_paint_node_add_child (node, pipeline_node);
  clutter_paint_node_add_multitexture_rectangle (pipeline_node,
                                                 &transition->offscreen_box,
                                                 tex_coords,
                                                 8);
}

static void
st_theme_node_transition_dispose (GObject *object)
{
  StThemeNodeTransition *self = ST_THEME_NODE_TRANSITION (object);

  g_clear_object (&self->old_theme_node);
  g_clear_object (&self->new_theme_node);

  g_clear_object (&self->old_texture);
  g_clear_object (&self->new_texture);

  g_clear_object (&self->old_offscreen);
  g_clear_object (&self->new_offscreen);

  g_clear_object (&self->pipeline);

  if (self->timeline)
    {
      g_clear_signal_handler (&self->timeline_completed_id, self->timeline);
      g_clear_signal_handler (&self->timeline_new_frame_id, self->timeline);

      g_clear_object (&self->timeline);
    }

  self->timeline_completed_id = 0;
  self->timeline_new_frame_id = 0;

  st_theme_node_paint_state_free (&self->old_paint_state);
  st_theme_node_paint_state_free (&self->new_paint_state);

  G_OBJECT_CLASS (st_theme_node_transition_parent_class)->dispose (object);
}

static void
st_theme_node_transition_init (StThemeNodeTransition *transition)
{
  transition->old_theme_node = NULL;
  transition->new_theme_node = NULL;

  transition->old_texture = NULL;
  transition->new_texture = NULL;

  transition->old_offscreen = NULL;
  transition->new_offscreen = NULL;

  st_theme_node_paint_state_init (&transition->old_paint_state);
  st_theme_node_paint_state_init (&transition->new_paint_state);

  transition->needs_setup = TRUE;
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
