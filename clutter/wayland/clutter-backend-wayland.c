/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010, 2011  Intel Corporation.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-main.h"
#include "clutter-stage-private.h"

#include "wayland/clutter-backend-wayland.h"
#include "wayland/clutter-device-manager-wayland.h"
#include "wayland/clutter-event-wayland.h"
#include "wayland/clutter-stage-wayland.h"
#include "cogl/clutter-stage-cogl.h"

#include <wayland-client.h>

#include <cogl/cogl.h>

#define clutter_backend_wayland_get_type     _clutter_backend_wayland_get_type

G_DEFINE_TYPE (ClutterBackendWayland, clutter_backend_wayland, CLUTTER_TYPE_BACKEND);

static void
clutter_backend_wayland_dispose (GObject *gobject)
{
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (gobject);

  if (backend_wayland->device_manager)
    {
      g_object_unref (backend_wayland->device_manager);
      backend_wayland->device_manager = NULL;
    }

  G_OBJECT_CLASS (clutter_backend_wayland_parent_class)->dispose (gobject);
}

static void
handle_configure (void *data,
                  struct wl_shell *shell,
                  uint32_t timestamp,
                  uint32_t edges,
                  struct wl_surface *surface,
                  int32_t width,
                  int32_t height)
{
  ClutterStageCogl *stage_cogl = wl_surface_get_user_data (surface);
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

static const struct wl_shell_listener shell_listener = {
       handle_configure,
};

static void
display_handle_global (struct wl_display *display,
                       uint32_t id,
                       const char *interface,
                       uint32_t version,
                       void *data)
{
  ClutterBackendWayland *backend_wayland = data;

  if (strcmp (interface, "wl_compositor") == 0)
    backend_wayland->wayland_compositor =
      wl_display_bind (display, id, &wl_compositor_interface);
  else if (strcmp (interface, "wl_input_device") == 0)
    {
      ClutterDeviceManager *device_manager = backend_wayland->device_manager;
      _clutter_device_manager_wayland_add_input_group (device_manager, id);
    }
  else if (strcmp (interface, "wl_shell") == 0)
    {
      backend_wayland->wayland_shell =
        wl_display_bind (display, id, &wl_shell_interface);
      wl_shell_add_listener (backend_wayland->wayland_shell,
                             &shell_listener, backend_wayland);
    }
  else if (strcmp (interface, "wl_shm") == 0)
    backend_wayland->wayland_shm =
      wl_display_bind (display, id, &wl_shm_interface);
}

static gboolean
clutter_backend_wayland_post_parse (ClutterBackend  *backend,
                                    GError         **error)
{
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);

  /* TODO: expose environment variable/commandline option for this... */
  backend_wayland->wayland_display = wl_display_connect (NULL);
  if (!backend_wayland->wayland_display)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                  CLUTTER_INIT_ERROR_BACKEND,
                  "Failed to open Wayland display socket");
      return FALSE;
    }

  backend_wayland->wayland_source =
    _clutter_event_source_wayland_new (backend_wayland->wayland_display);
  g_source_attach (backend_wayland->wayland_source, NULL);

  /* XXX: We require the device manager to exist as soon as we connect to the
   * compositor and setup an event handler because we will immediately be
   * notified of the available input devices which need to be associated with
   * the device-manager.
   *
   * FIXME: At some point we could perhaps just collapse the
   * _clutter_backend_post_parse(), and _clutter_backend_init_events()
   * functions into one called something like _clutter_backend_init() which
   * would allow the real backend to manage the precise order of
   * initialization.
   */
  backend_wayland->device_manager =
    _clutter_device_manager_wayland_new (backend);

  /* Set up listener so we'll catch all events. */
  wl_display_add_global_listener (backend_wayland->wayland_display,
                                  display_handle_global,
                                  backend_wayland);

  /* Wait until we have been notified about the compositor and shell objects */
  while (!(backend_wayland->wayland_compositor &&
           backend_wayland->wayland_shell))
    wl_display_roundtrip (backend_wayland->wayland_display);

  return TRUE;
}

static CoglRenderer *
clutter_backend_wayland_get_renderer (ClutterBackend  *backend,
                                      GError         **error)
{
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  CoglRenderer *renderer;

  CLUTTER_NOTE (BACKEND, "Creating a new wayland renderer");

  renderer = cogl_renderer_new ();

  cogl_wayland_renderer_set_foreign_display (renderer,
                                             backend_wayland->wayland_display);
  cogl_wayland_renderer_set_foreign_compositor (renderer,
                                                backend_wayland->wayland_compositor);
  cogl_wayland_renderer_set_foreign_shell (renderer,
                                           backend_wayland->wayland_shell);

  return renderer;
}

static CoglDisplay *
clutter_backend_wayland_get_display (ClutterBackend  *backend,
                                     CoglRenderer    *renderer,
                                     CoglSwapChain   *swap_chain,
                                     GError         **error)
{
  CoglOnscreenTemplate *onscreen_template = NULL;
  CoglDisplay *display;

  onscreen_template = cogl_onscreen_template_new (swap_chain);

  /* XXX: I have some doubts that this is a good design.
   * Conceptually should we be able to check an onscreen_template
   * without more details about the CoglDisplay configuration?
   */
  if (!cogl_renderer_check_onscreen_template (renderer,
                                              onscreen_template,
                                              error))
    goto error;

  display = cogl_display_new (renderer, onscreen_template);

  return display;

error:
  if (onscreen_template)
    cogl_object_unref (onscreen_template);

  return NULL;
}

static void
clutter_backend_wayland_class_init (ClutterBackendWaylandClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->dispose = clutter_backend_wayland_dispose;

  backend_class->stage_window_type = CLUTTER_TYPE_STAGE_WAYLAND;

  backend_class->post_parse = clutter_backend_wayland_post_parse;
  backend_class->get_renderer = clutter_backend_wayland_get_renderer;
  backend_class->get_display = clutter_backend_wayland_get_display;
}

static void
clutter_backend_wayland_init (ClutterBackendWayland *backend_wayland)
{
}
