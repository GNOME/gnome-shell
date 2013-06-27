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

#include "clutter-wayland.h"
#include "clutter-stage-wayland.h"
#include "clutter-backend-wayland.h"
#include "clutter-backend-wayland-priv.h"
#include "clutter-stage-window.h"
#include "clutter-stage-private.h"
#include "clutter-event-private.h"
#include "clutter-wayland.h"
#include <cogl/cogl.h>
#include <cogl/cogl-wayland-client.h>

static ClutterStageWindowIface *clutter_stage_window_parent_iface = NULL;

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

#define clutter_stage_wayland_get_type _clutter_stage_wayland_get_type

G_DEFINE_TYPE_WITH_CODE (ClutterStageWayland,
                         clutter_stage_wayland,
                         CLUTTER_TYPE_STAGE_COGL,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static void
handle_ping (void *data,
             struct wl_shell_surface *shell_surface,
             uint32_t serial)
{
  wl_shell_surface_pong(shell_surface, serial);
}

static void
handle_configure (void *data,
                  struct wl_shell_surface *shell_surface,
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

static void
handle_popup_done (void *data,
                   struct wl_shell_surface *shell_surface)
{
  /* XXX: Fill me in. */
}

static const struct wl_shell_surface_listener shell_surface_listener = {
       handle_ping,
       handle_configure,
       handle_popup_done,
};

static void
clutter_stage_wayland_set_fullscreen (ClutterStageWindow *stage_window,
                                      gboolean            fullscreen);

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

  if (stage_wayland->fullscreen)
    clutter_stage_wayland_set_fullscreen (stage_window, TRUE);

  return TRUE;
}

static void
clutter_stage_wayland_show (ClutterStageWindow *stage_window,
                            gboolean            do_raise)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);

  clutter_stage_window_parent_iface->show (stage_window, do_raise);

  /* TODO: must not call this on foreign surfaces when we add that support */
  wl_shell_surface_set_toplevel (stage_wayland->wayland_shell_surface);

  /* We need to queue a redraw after the stage is shown because all of
   * the other queue redraws up to this point will have been ignored
   * because the actor was not visible. The other backends do not need
   * to do this because they will get expose events at some point, but
   * that does not happen for Wayland. */
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage_cogl->wrapper));
}

static void
clutter_stage_wayland_set_fullscreen (ClutterStageWindow *stage_window,
                                      gboolean            fullscreen)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);
  ClutterBackend *backend = CLUTTER_BACKEND (stage_cogl->backend);
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  ClutterActor *stage = _clutter_stage_window_get_wrapper (stage_window);

  stage_wayland->fullscreen = fullscreen;

  if (!stage_wayland->wayland_shell_surface) /* Not realized yet */
    return;

  if (fullscreen)
    {
      _clutter_stage_update_state (stage_cogl->wrapper,
                                   0,
                                   CLUTTER_STAGE_STATE_FULLSCREEN);

      /* FIXME: In future versions of the Wayland protocol we'll get a
       * configure with the dimensions we can use - but for now we have to
       * use the dimensions from the output's mode
       */
      clutter_actor_set_size (stage,
                              backend_wayland->output_width,
                              backend_wayland->output_height);

      /* FIXME: And we must force a redraw so that new sized buffer gets
       * attached
       */
      _clutter_stage_window_redraw (stage_window);
      wl_shell_surface_set_fullscreen (stage_wayland->wayland_shell_surface,
                                       WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                       0,
                                       NULL);
    }
  else
    {
      _clutter_stage_update_state (stage_cogl->wrapper,
                                   CLUTTER_STAGE_STATE_FULLSCREEN,
                                   0);

      wl_shell_surface_set_toplevel (stage_wayland->wayland_shell_surface);
    }
}

static void
clutter_stage_wayland_resize (ClutterStageWindow *stage_window,
                              gint                width,
                              gint                height)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  /* Resize preserving top left */
  if (stage_cogl->onscreen)
    {
      cogl_wayland_onscreen_resize (stage_cogl->onscreen, width, height, 0, 0);
      _clutter_stage_window_redraw (stage_window);
    }
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
  iface->show = clutter_stage_wayland_show;
  iface->set_fullscreen = clutter_stage_wayland_set_fullscreen;
  iface->resize = clutter_stage_wayland_resize;
}

static void
clutter_stage_wayland_class_init (ClutterStageWaylandClass *klass)
{
}

/**
 * clutter_wayland_stage_get_wl_shell_surface: (skip)
 * @stage: a #ClutterStage
 *
 * Access the underlying data structure representing the shell surface that is
 * backing the #ClutterStage
 *
 * Note: this function can only be called when running on the Wayland
 * platform. Calling this function at any other time will return %NULL.
 *
 * Returns: (transfer none): the Wayland shell surface associated with
 * @stage
 *
 * Since: 1.10
 */
struct wl_shell_surface *
clutter_wayland_stage_get_wl_shell_surface (ClutterStage *stage)
{
  ClutterStageWindow *stage_window = _clutter_stage_get_window (stage);
  ClutterStageWayland *stage_wayland;

  if (!CLUTTER_IS_STAGE_WAYLAND (stage_window))
    return NULL;

  stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);

  return stage_wayland->wayland_shell_surface;
}

/**
 * clutter_wayland_stage_get_wl_surface: (skip)
 * @stage: a #ClutterStage
 *
 * Access the underlying data structure representing the surface that is
 * backing the #ClutterStage
 *
 * Note: this function can only be called when running on the Wayland
 * platform. Calling this function at any other time will return %NULL.
 *
 * Returns: (transfer none): the Wayland surface associated with @stage
 *
 * Since: 1.10
 */
struct wl_surface *
clutter_wayland_stage_get_wl_surface (ClutterStage *stage)
{
  ClutterStageWindow *stage_window = _clutter_stage_get_window (stage);
  ClutterStageWayland *stage_wayland;

  if (!CLUTTER_IS_STAGE_WAYLAND (stage_window))
    return NULL;

  stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);

  return stage_wayland->wayland_surface;

}
