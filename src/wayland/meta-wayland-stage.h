/*
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

#ifndef META_WAYLAND_STAGE_H
#define META_WAYLAND_STAGE_H

#include <clutter/clutter.h>
#include <wayland-server.h>

#include "window-private.h"

G_BEGIN_DECLS

#define META_WAYLAND_TYPE_STAGE                                         \
  (meta_wayland_stage_get_type())
#define META_WAYLAND_STAGE(obj)                                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               META_WAYLAND_TYPE_STAGE,                 \
                               MetaWaylandStage))
#define META_WAYLAND_STAGE_CLASS(klass)                                 \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            META_WAYLAND_TYPE_STAGE,                    \
                            MetaWaylandStageClass))
#define META_WAYLAND_IS_STAGE(obj)                                      \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               META_WAYLAND_TYPE_STAGE))
#define META_WAYLAND_IS_STAGE_CLASS(klass)                              \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            META_WAYLAND_TYPE_STAGE))
#define META_WAYLAND_STAGE_GET_CLASS(obj)                               \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              META_WAYLAND_STAGE,                       \
                              MetaWaylandStageClass))

typedef struct _MetaWaylandStage      MetaWaylandStage;
typedef struct _MetaWaylandStageClass MetaWaylandStageClass;

struct _MetaWaylandStageClass
{
  ClutterStageClass parent_class;
};

struct _MetaWaylandStage
{
  ClutterStage parent;
};

GType             meta_wayland_stage_get_type                (void) G_GNUC_CONST;

ClutterActor     *meta_wayland_stage_new                     (void);

G_END_DECLS

#endif /* META_WAYLAND_STAGE_H */
