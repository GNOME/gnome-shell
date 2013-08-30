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
#include "meta-wayland-private.h"
#include "meta/meta-window-actor.h"
#include "meta/meta-shaped-texture.h"
#include "meta-cursor-tracker-private.h"

G_DEFINE_TYPE (MetaWaylandStage, meta_wayland_stage, CLUTTER_TYPE_STAGE);

static void
meta_wayland_stage_paint (ClutterActor *actor)
{
  MetaWaylandCompositor *compositor;

  CLUTTER_ACTOR_CLASS (meta_wayland_stage_parent_class)->paint (actor);

  compositor = meta_wayland_compositor_get_default ();
  if (compositor->seat->cursor_tracker)
    meta_cursor_tracker_paint (compositor->seat->cursor_tracker);
}

static void
meta_wayland_stage_class_init (MetaWaylandStageClass *klass)
{
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;

  actor_class->paint = meta_wayland_stage_paint;
}

static void
meta_wayland_stage_init (MetaWaylandStage *self)
{
  clutter_stage_set_user_resizable (CLUTTER_STAGE (self), FALSE);
}

ClutterActor *
meta_wayland_stage_new (void)
{
  return g_object_new (META_WAYLAND_TYPE_STAGE,
                       "cursor-visible", FALSE,
                       NULL);
}
