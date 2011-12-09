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

static void
handle_configure (void *data,
                  struct wl_shell_surface *shell_surface,
                  uint32_t timestamp,
                  uint32_t edges,
                  int32_t width,
                  int32_t height)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL(data);
  CoglFramebuffer *fb = COGL_FRAMEBUFFER (stage_cogl->onscreen);

  if (cogl_framebuffer_get_width (fb) != width ||
      cogl_framebuffer_get_height (fb) != height)
    clutter_actor_queue_relayout (CLUTTER_ACTOR (stage_cogl->wrapper));

  clutter_actor_set_size (CLUTTER_ACTOR (stage_cogl->wrapper),
                         width, height);

  /* the resize process is complete, so we can ask the stage
   * to set up the GL viewport with the new size
   */
  clutter_stage_ensure_viewport (stage_cogl->wrapper);
}

static const struct wl_shell_surface_listener shell_surface_listener = {
       handle_configure,
};

static gboolean
clutter_stage_wayland_realize (ClutterStageWindow *stage_window)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);
  struct wl_surface *wl_surface;
  struct wl_shell_surface *wl_shell_surface;

  clutter_stage_window_parent_iface->realize (stage_window);

  wl_surface = cogl_wayland_onscreen_get_surface (stage_cogl->onscreen);
  wl_surface_set_user_data (wl_surface, stage_wayland);

  wl_shell_surface =
    cogl_wayland_onscreen_get_shell_surface (stage_cogl->onscreen);
  wl_shell_surface_add_listener (wl_shell_surface,
                                 &shell_surface_listener,
                                 stage_wayland);

  stage_wayland->wayland_surface = wl_surface;
  stage_wayland->wayland_shell_surface = wl_shell_surface;

  return TRUE;
}

static void
clutter_stage_wayland_set_fullscreen (ClutterStageWindow *stage_window,
                                      gboolean            fullscreen)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);

  if (fullscreen)
    wl_shell_surface_set_fullscreen (stage_wayland->wayland_shell_surface);
  else
    g_warning (G_STRLOC ": There is no Wayland API for un-fullscreening now");
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
  iface->set_fullscreen = clutter_stage_wayland_set_fullscreen;
}

static void
clutter_stage_wayland_class_init (ClutterStageWaylandClass *klass)
{
}
