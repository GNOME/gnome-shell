/*
 * Copyright (C) 2014 Red Hat
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include <config.h>

#include "meta-stage.h"

#include <meta/meta-backend.h>
#include <meta/util.h>

typedef struct {
  gboolean enabled;

  CoglPipeline *pipeline;
  CoglTexture *texture;

  MetaRectangle current_rect;
  MetaRectangle previous_rect;
  gboolean previous_is_valid;
} MetaOverlay;

struct _MetaStagePrivate {
  MetaOverlay cursor_overlay;
  gboolean is_active;
};
typedef struct _MetaStagePrivate MetaStagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaStage, meta_stage, CLUTTER_TYPE_STAGE);

static void
meta_overlay_init (MetaOverlay *overlay)
{
  CoglContext *ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());

  overlay->pipeline = cogl_pipeline_new (ctx);
}

static void
meta_overlay_free (MetaOverlay *overlay)
{
  if (overlay->pipeline)
    cogl_object_unref (overlay->pipeline);
}

static void
meta_overlay_set (MetaOverlay   *overlay,
                  CoglTexture   *texture,
                  MetaRectangle *rect)
{
  if (overlay->texture != texture)
    {
      overlay->texture = texture;

      if (texture)
        {
          cogl_pipeline_set_layer_texture (overlay->pipeline, 0, texture);
          overlay->enabled = TRUE;
        }
      else
        {
          cogl_pipeline_set_layer_texture (overlay->pipeline, 0, NULL);
          overlay->enabled = FALSE;
        }
    }

  overlay->current_rect = *rect;
}

static void
meta_overlay_paint (MetaOverlay *overlay)
{
  if (!overlay->enabled)
    return;

  g_assert (meta_is_wayland_compositor ());

  cogl_framebuffer_draw_rectangle (cogl_get_draw_framebuffer (),
                                   overlay->pipeline,
                                   overlay->current_rect.x,
                                   overlay->current_rect.y,
                                   overlay->current_rect.x +
                                   overlay->current_rect.width,
                                   overlay->current_rect.y +
                                   overlay->current_rect.height);

  overlay->previous_rect = overlay->current_rect;
  overlay->previous_is_valid = TRUE;
}

static void
meta_stage_finalize (GObject *object)
{
  MetaStage *stage = META_STAGE (object);
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);

  meta_overlay_free (&priv->cursor_overlay);
}

static void
meta_stage_paint (ClutterActor *actor)
{
  MetaStage *stage = META_STAGE (actor);
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);

  CLUTTER_ACTOR_CLASS (meta_stage_parent_class)->paint (actor);

  meta_overlay_paint (&priv->cursor_overlay);
}

static void
meta_stage_activate (ClutterStage *actor)
{
  MetaStage *stage = META_STAGE (actor);
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);

  CLUTTER_STAGE_CLASS (meta_stage_parent_class)->activate (actor);

  priv->is_active = TRUE;
}

static void
meta_stage_deactivate (ClutterStage *actor)
{
  MetaStage *stage = META_STAGE (actor);
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);

  CLUTTER_STAGE_CLASS (meta_stage_parent_class)->deactivate (actor);

  priv->is_active = FALSE;
}

static void
meta_stage_class_init (MetaStageClass *klass)
{
  ClutterStageClass *stage_class = (ClutterStageClass *) klass;
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->finalize = meta_stage_finalize;

  actor_class->paint = meta_stage_paint;

  stage_class->activate = meta_stage_activate;
  stage_class->deactivate = meta_stage_deactivate;
}

static void
meta_stage_init (MetaStage *stage)
{
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);

  meta_overlay_init (&priv->cursor_overlay);

  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), FALSE);
}

ClutterActor *
meta_stage_new (void)
{
  return g_object_new (META_TYPE_STAGE,
                       "cursor-visible", FALSE,
                       NULL);
}

static void
queue_redraw_for_overlay (MetaStage   *stage,
                          MetaOverlay *overlay)
{
  cairo_rectangle_int_t clip;

  /* Clear the location the overlay was at before, if we need to. */
  if (overlay->previous_is_valid)
    {
      clip.x = overlay->previous_rect.x;
      clip.y = overlay->previous_rect.y;
      clip.width = overlay->previous_rect.width;
      clip.height = overlay->previous_rect.height;
      clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (stage), &clip);
      overlay->previous_is_valid = FALSE;
    }

  /* Draw the overlay at the new position */
  if (overlay->enabled)
    {
      clip.x = overlay->current_rect.x;
      clip.y = overlay->current_rect.y;
      clip.width = overlay->current_rect.width;
      clip.height = overlay->current_rect.height;
      clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (stage), &clip);
    }
}

void
meta_stage_set_cursor (MetaStage     *stage,
                       CoglTexture   *texture,
                       MetaRectangle *rect)
{
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);

  g_assert (meta_is_wayland_compositor () || texture == NULL);

  meta_overlay_set (&priv->cursor_overlay, texture, rect);
  queue_redraw_for_overlay (stage, &priv->cursor_overlay);
}

void
meta_stage_set_active (MetaStage *stage,
                       gboolean   is_active)
{
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);
  ClutterEvent event = { 0 };

  /* Used by the native backend to inform accessibility technologies
   * about when the stage loses and gains input focus.
   *
   * For the X11 backend, clutter transparently takes care of this
   * for us.
   */

  if (priv->is_active == is_active)
    return;

  event.type = CLUTTER_STAGE_STATE;
  clutter_event_set_stage (&event, CLUTTER_STAGE (stage));
  event.stage_state.changed_mask = CLUTTER_STAGE_STATE_ACTIVATED;

  if (is_active)
    event.stage_state.new_state = CLUTTER_STAGE_STATE_ACTIVATED;

  /* Emitting this StageState event will result in the stage getting
   * activated or deactivated (with the activated or deactivated signal
   * getting emitted from the stage)
   *
   * FIXME: This won't update ClutterStage's own notion of its
   * activeness. For that we would need to somehow trigger a
   * _clutter_stage_update_state call, which will probably
   * require new API in clutter. In practice, nothing relies
   * on the ClutterStage's own notion of activeness when using
   * the EGL backend.
   *
   * See http://bugzilla.gnome.org/746670
   */
  clutter_stage_event (CLUTTER_STAGE (stage), &event);
}
