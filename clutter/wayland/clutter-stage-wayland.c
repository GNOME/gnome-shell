/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include "clutter-stage-wayland.h"

#include "clutter-stage-window.h"

#include <cogl/cogl.h>

static ClutterStageWindowIface *clutter_stage_window_parent_iface = NULL;

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStageWayland,
                         clutter_stage_wayland,
                         CLUTTER_TYPE_STAGE_COGL,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static gboolean
clutter_stage_wayland_realize (ClutterStageWindow *stage_window)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);
  struct wl_surface *wl_surface;

  clutter_stage_window_parent_iface->realize (stage_window);

  wl_surface = cogl_wayland_onscreen_get_surface (stage_cogl->onscreen);

  wl_surface_set_user_data (wl_surface, stage_wayland);

  return TRUE;
}

static void
clutter_stage_wayland_init (ClutterStageWayland *stage_wayland)
{
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  clutter_stage_window_parent_iface = g_type_interface_peek_parent (iface);

  iface->realize = clutter_stage_wayland_realize;
}

static void
clutter_stage_wayland_class_init (ClutterStageWaylandClass *klass)
{
}
