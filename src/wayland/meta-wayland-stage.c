/*
 * Wayland Support
 *
 * Copyright (C) 2012 Intel Corporation
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
 */

#include <config.h>

#include <clutter/clutter.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cogl/cogl-wayland-server.h>

#include "meta-wayland-stage.h"
#include "meta/meta-window-actor.h"
#include "meta/meta-shaped-texture.h"

#define META_WAYLAND_DEFAULT_CURSOR_HOTSPOT_X 7
#define META_WAYLAND_DEFAULT_CURSOR_HOTSPOT_Y 4

G_DEFINE_TYPE (MetaWaylandStage, meta_wayland_stage, CLUTTER_TYPE_STAGE);

static void
meta_wayland_stage_finalize (GObject *object)
{
  MetaWaylandStage *self = (MetaWaylandStage *) object;

  cogl_object_unref (self->default_cursor_pipeline);
  cogl_object_unref (self->texture_cursor_pipeline);

  G_OBJECT_CLASS (meta_wayland_stage_parent_class)->finalize (object);
}

static void
get_cursor_draw_position (MetaWaylandStage *self,
                          cairo_rectangle_int_t *rect)
{
  rect->x = self->cursor_x - self->cursor_hotspot_x;
  rect->y = self->cursor_y - self->cursor_hotspot_y;
  rect->width = self->cursor_width;
  rect->height = self->cursor_height;
}

static void
draw_cursor_pipeline (MetaWaylandStage *self,
                      CoglPipeline *pipeline)
{
  cairo_rectangle_int_t rect;

  get_cursor_draw_position (self, &rect);

  cogl_framebuffer_draw_rectangle (cogl_get_draw_framebuffer (),
                                   pipeline,
                                   rect.x, rect.y,
                                   rect.x + rect.width,
                                   rect.y + rect.height);

  self->has_last_cursor_position = TRUE;
  self->last_cursor_position = rect;
}

static void
meta_wayland_stage_paint (ClutterActor *actor)
{
  MetaWaylandStage *self = META_WAYLAND_STAGE (actor);

  CLUTTER_ACTOR_CLASS (meta_wayland_stage_parent_class)->paint (actor);

  /* Make sure the cursor is always painted on top of all of the other
     actors */

  switch (self->cursor_type)
    {
    case META_WAYLAND_STAGE_CURSOR_INVISIBLE:
      break;

    case META_WAYLAND_STAGE_CURSOR_DEFAULT:
      draw_cursor_pipeline (self, self->default_cursor_pipeline);
      break;

    case META_WAYLAND_STAGE_CURSOR_TEXTURE:
      draw_cursor_pipeline (self, self->texture_cursor_pipeline);
      break;
    }
}

static void
update_cursor_position (MetaWaylandStage *self)
{
  cairo_rectangle_int_t rect;

  if (self->has_last_cursor_position)
    {
      clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (self),
                                            &self->last_cursor_position);
      self->has_last_cursor_position = FALSE;
    }

  get_cursor_draw_position (self, &rect);
  if (rect.width != 0 && rect.height != 0)
    clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (self), &rect);
}

static void
meta_wayland_stage_class_init (MetaWaylandStageClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;

  gobject_class->finalize = meta_wayland_stage_finalize;

  actor_class->paint = meta_wayland_stage_paint;
}

static void
load_default_cursor_pipeline (MetaWaylandStage *self)
{
  CoglContext *context =
    clutter_backend_get_cogl_context (clutter_get_default_backend ());
  CoglTexture *texture;
  CoglError *error = NULL;
  char *filename;

  filename = g_build_filename (MUTTER_DATADIR,
                               "mutter/cursors/left_ptr.png",
                               NULL);

  texture = cogl_texture_new_from_file (filename,
                                        COGL_TEXTURE_NONE,
                                        COGL_PIXEL_FORMAT_ANY,
                                        &error);

  g_free (filename);

  self->default_cursor_pipeline = cogl_pipeline_new (context);
  cogl_pipeline_set_layer_filters (self->default_cursor_pipeline,
                                   0, /* layer */
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  if (texture == NULL)
    {
      g_warning ("Failed to load default cursor: %s",
                 error->message);
      cogl_error_free (error);
    }
  else
    {
      self->default_cursor_width = cogl_texture_get_width (texture);
      self->default_cursor_height = cogl_texture_get_height (texture);

      cogl_pipeline_set_layer_texture (self->default_cursor_pipeline,
                                       0, /* layer */
                                       texture);
      cogl_object_unref (texture);
    }
}

static void
meta_wayland_stage_init (MetaWaylandStage *self)
{
  load_default_cursor_pipeline (self);

  self->texture_cursor_pipeline =
    cogl_pipeline_copy (self->default_cursor_pipeline);

  meta_wayland_stage_set_default_cursor (self);
}

ClutterActor *
meta_wayland_stage_new (void)
{
  return g_object_new (META_WAYLAND_TYPE_STAGE,
                       "cursor-visible", FALSE,
                       NULL);
}

void
meta_wayland_stage_set_cursor_position (MetaWaylandStage *self,
                                        int               x,
                                        int               y)
{
  self->cursor_x = x;
  self->cursor_y = y;
  update_cursor_position (self);
}

void
meta_wayland_stage_set_cursor_from_texture (MetaWaylandStage *self,
                                            CoglTexture      *texture,
                                            int               hotspot_x,
                                            int               hotspot_y)
{
  CoglPipeline *pipeline;

  self->cursor_hotspot_x = hotspot_x;
  self->cursor_hotspot_y = hotspot_y;
  self->cursor_type = META_WAYLAND_STAGE_CURSOR_TEXTURE;

  pipeline = cogl_pipeline_copy (self->texture_cursor_pipeline);
  cogl_pipeline_set_layer_texture (pipeline, 0, texture);
  cogl_object_unref (self->texture_cursor_pipeline);
  self->texture_cursor_pipeline = pipeline;

  self->cursor_width = cogl_texture_get_width (texture);
  self->cursor_height = cogl_texture_get_height (texture);

  update_cursor_position (self);
}

void
meta_wayland_stage_set_invisible_cursor (MetaWaylandStage *self)
{
  self->cursor_type = META_WAYLAND_STAGE_CURSOR_INVISIBLE;
  self->cursor_width = 0;
  self->cursor_height = 0;
  update_cursor_position (self);
}

void
meta_wayland_stage_set_default_cursor (MetaWaylandStage *self)
{
  self->cursor_type = META_WAYLAND_STAGE_CURSOR_DEFAULT;
  self->cursor_hotspot_x = META_WAYLAND_DEFAULT_CURSOR_HOTSPOT_X;
  self->cursor_hotspot_y = META_WAYLAND_DEFAULT_CURSOR_HOTSPOT_Y;
  self->cursor_width = self->default_cursor_width;
  self->cursor_height = self->default_cursor_height;
  update_cursor_position (self);
}
