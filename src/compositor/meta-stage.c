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

#include "meta-cursor-private.h"
#include "meta-backend.h"
#include <meta/util.h>

struct _MetaStagePrivate {
  CoglPipeline *pipeline;
  gboolean should_paint_cursor;

  MetaCursorReference *cursor;

  MetaRectangle current_rect;
  MetaRectangle previous_rect;
  gboolean previous_is_valid;
};
typedef struct _MetaStagePrivate MetaStagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaStage, meta_stage, CLUTTER_TYPE_STAGE);

static void
update_pipeline (MetaStage *stage)
{
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);

  if (priv->cursor)
    {
      CoglTexture *texture = meta_cursor_reference_get_cogl_texture (priv->cursor, NULL, NULL);
      cogl_pipeline_set_layer_texture (priv->pipeline, 0, texture);
    }
  else
    cogl_pipeline_set_layer_texture (priv->pipeline, 0, NULL);
}

static void
meta_stage_finalize (GObject *object)
{
  MetaStage *stage = META_STAGE (object);
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);

  if (priv->pipeline)
    cogl_object_unref (priv->pipeline);
}

static void
paint_cursor (MetaStage *stage)
{
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);

  g_assert (meta_is_wayland_compositor ());

  if (!priv->cursor)
    return;

  cogl_framebuffer_draw_rectangle (cogl_get_draw_framebuffer (),
                                   priv->pipeline,
                                   priv->current_rect.x,
                                   priv->current_rect.y,
                                   priv->current_rect.x +
                                   priv->current_rect.width,
                                   priv->current_rect.y +
                                   priv->current_rect.height);

  priv->previous_rect = priv->current_rect;
  priv->previous_is_valid = TRUE;
}

static void
meta_stage_paint (ClutterActor *actor)
{
  MetaStage *stage = META_STAGE (actor);

  CLUTTER_ACTOR_CLASS (meta_stage_parent_class)->paint (actor);

  if (meta_is_wayland_compositor ())
    paint_cursor (stage);
}

static void
meta_stage_class_init (MetaStageClass *klass)
{
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->finalize = meta_stage_finalize;

  actor_class->paint = meta_stage_paint;
}

static void
meta_stage_init (MetaStage *stage)
{
  CoglContext *ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);

  priv->pipeline = cogl_pipeline_new (ctx);

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
queue_redraw (MetaStage *stage)
{
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);
  cairo_rectangle_int_t clip;

  /* Clear the location the cursor was at before, if we need to. */
  if (priv->previous_is_valid)
    {
      clip.x = priv->previous_rect.x;
      clip.y = priv->previous_rect.y;
      clip.width = priv->previous_rect.width;
      clip.height = priv->previous_rect.height;
      clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (stage), &clip);
      priv->previous_is_valid = FALSE;
    }

  /* And queue a redraw for the current cursor location. */
  if (priv->cursor)
    {
      clip.x = priv->current_rect.x;
      clip.y = priv->current_rect.y;
      clip.width = priv->current_rect.width;
      clip.height = priv->current_rect.height;
      clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (stage), &clip);
    }
}

void
meta_stage_set_cursor (MetaStage           *stage,
                       MetaCursorReference *cursor,
                       MetaRectangle       *rect)
{
  MetaStagePrivate *priv = meta_stage_get_instance_private (stage);

  if (priv->cursor != cursor)
    {
      priv->cursor = cursor;
      update_pipeline (stage);
    }

  priv->current_rect = *rect;
  queue_redraw (stage);
}
