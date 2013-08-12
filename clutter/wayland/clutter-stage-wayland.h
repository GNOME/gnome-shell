/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010,2011  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.

 * Authors:
 *  Matthew Allum
 *  Robert Bragg
 *  Kristian Høgsberg
 */

#ifndef __CLUTTER_STAGE_WAYLAND_H__
#define __CLUTTER_STAGE_WAYLAND_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <clutter/clutter-stage.h>

#include "cogl/clutter-stage-cogl.h"

#define CLUTTER_TYPE_STAGE_WAYLAND                  (_clutter_stage_wayland_get_type ())
#define CLUTTER_STAGE_WAYLAND(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_WAYLAND, ClutterStageWayland))
#define CLUTTER_IS_STAGE_WAYLAND(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_WAYLAND))
#define CLUTTER_STAGE_WAYLAND_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_STAGE_WAYLAND, ClutterStageWaylandClass))
#define CLUTTER_IS_STAGE_WAYLAND_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_STAGE_WAYLAND))
#define CLUTTER_STAGE_WAYLAND_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_STAGE_WAYLAND, ClutterStageWaylandClass))

typedef struct _ClutterStageWayland         ClutterStageWayland;
typedef struct _ClutterStageWaylandClass    ClutterStageWaylandClass;

struct _ClutterStageWayland
{
  ClutterStageCogl parent_instance;

  struct wl_surface *wayland_surface;
  struct wl_shell_surface *wayland_shell_surface;
  gboolean fullscreen;
  gboolean foreign_wl_surface;
  gboolean shown;
};

struct _ClutterStageWaylandClass
{
  ClutterStageCoglClass parent_class;
};

GType _clutter_stage_wayland_get_type (void) G_GNUC_CONST;

#endif /* __CLUTTER_STAGE_WAYLAND_H__ */
