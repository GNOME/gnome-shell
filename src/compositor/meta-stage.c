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

#include "meta-backend.h"
#include <meta/util.h>

G_DEFINE_TYPE (MetaStage, meta_stage, CLUTTER_TYPE_STAGE);

static void
meta_stage_paint (ClutterActor *actor)
{
  CLUTTER_ACTOR_CLASS (meta_stage_parent_class)->paint (actor);

  if (meta_is_wayland_compositor ())
    {
      MetaBackend *backend = meta_get_backend ();
      MetaCursorRenderer *renderer = meta_backend_get_cursor_renderer (backend);
      meta_cursor_renderer_paint (renderer);
    }
}

static void
meta_stage_class_init (MetaStageClass *klass)
{
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;

  actor_class->paint = meta_stage_paint;
}

static void
meta_stage_init (MetaStage *self)
{
  clutter_stage_set_user_resizable (CLUTTER_STAGE (self), FALSE);
}

ClutterActor *
meta_stage_new (void)
{
  return g_object_new (META_TYPE_STAGE,
                       "cursor-visible", FALSE,
                       NULL);
}
