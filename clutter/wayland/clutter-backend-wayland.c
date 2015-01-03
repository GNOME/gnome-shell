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
#include <sys/mman.h>

#include <errno.h>

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-main.h"
#include "clutter-stage-private.h"

#include "wayland/clutter-backend-wayland.h"
#include "wayland/clutter-backend-wayland-priv.h"
#include "wayland/clutter-device-manager-wayland.h"
#include "wayland/clutter-event-wayland.h"
#include "wayland/clutter-stage-wayland.h"
#include "wayland/clutter-wayland.h"
#include "cogl/clutter-stage-cogl.h"

#include <wayland-client.h>
#include <wayland-cursor.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cogl/cogl.h>
#include <cogl/cogl-wayland-client.h>

#define clutter_backend_wayland_get_type     _clutter_backend_wayland_get_type

G_DEFINE_TYPE (ClutterBackendWayland, clutter_backend_wayland, CLUTTER_TYPE_BACKEND);

static struct wl_display *_foreign_display = NULL;
static gboolean _no_event_dispatch = FALSE;

static void
clutter_backend_wayland_dispose (GObject *gobject)
{
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (gobject);

  if (backend_wayland->device_manager)
    {
      g_object_unref (backend_wayland->device_manager);
      backend_wayland->device_manager = NULL;
    }

  if (backend_wayland->cursor_buffer)
    {
      wl_buffer_destroy (backend_wayland->cursor_buffer);
      backend_wayland->cursor_buffer = NULL;
    }

  if (backend_wayland->cursor_theme)
    {
      wl_cursor_theme_destroy (backend_wayland->cursor_theme);
      backend_wayland->cursor_theme = NULL;
    }

  G_OBJECT_CLASS (clutter_backend_wayland_parent_class)->dispose (gobject);
}


static void
output_handle_mode (void             *data,
                    struct wl_output *wl_output,
                    uint32_t          flags,
                    int               width,
                    int               height,
                    int               refresh)
{
  ClutterBackendWayland *backend_wayland = data;

  if (flags & WL_OUTPUT_MODE_CURRENT)
    {
      backend_wayland->output_width = width;
      backend_wayland->output_height = height;
    }
}

static void
output_handle_geometry (void             *data,
                        struct wl_output *wl_output,
                        int               x,
                        int               y,
                        int               physical_width,
                        int               physical_height,
                        int               subpixel,
                        const char       *make,
                        const char       *model,
                        int32_t transform)
{
}


static const struct wl_output_listener wayland_output_listener = {
  output_handle_geometry,
  output_handle_mode,
};


static void
registry_handle_global (void *data,
                        struct wl_registry *registry,
                        uint32_t id,
                        const char *interface,
                        uint32_t version)
{
  ClutterBackendWayland *backend_wayland = data;

  if (strcmp (interface, "wl_compositor") == 0)
    backend_wayland->wayland_compositor =
      wl_registry_bind (registry, id, &wl_compositor_interface, 1);
  else if (strcmp (interface, "wl_seat") == 0)
    {
      ClutterDeviceManager *device_manager = backend_wayland->device_manager;
      _clutter_device_manager_wayland_add_input_group (device_manager, id);
    }
  else if (strcmp (interface, "wl_shell") == 0)
    {
      backend_wayland->wayland_shell =
        wl_registry_bind (registry, id, &wl_shell_interface, 1);
    }
  else if (strcmp (interface, "wl_shm") == 0)
    {
      backend_wayland->wayland_shm =
        wl_registry_bind (registry, id, &wl_shm_interface, 1);
    }
  else if (strcmp (interface, "wl_output") == 0)
    {
      /* FIXME: Support multiple outputs */
      backend_wayland->wayland_output =
        wl_registry_bind (registry, id, &wl_output_interface, 1);
      wl_output_add_listener (backend_wayland->wayland_output,
                              &wayland_output_listener,
                              backend_wayland);
    }
}

static const struct wl_registry_listener wayland_registry_listener = {
  registry_handle_global,
};

static gboolean
clutter_backend_wayland_post_parse (ClutterBackend  *backend,
                                    GError         **error)
{
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);

  /* TODO: expose environment variable/commandline option for this... */
  backend_wayland->wayland_display = _foreign_display;
  if (backend_wayland->wayland_display == NULL)
    backend_wayland->wayland_display = wl_display_connect (NULL);

  if (!backend_wayland->wayland_display)
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                  CLUTTER_INIT_ERROR_BACKEND,
                  "Failed to open Wayland display socket");
      return FALSE;
    }

  backend_wayland->wayland_registry =
    wl_display_get_registry (backend_wayland->wayland_display);

  backend_wayland->wayland_source =
    _clutter_event_source_wayland_new (backend_wayland->wayland_display);
  g_source_attach (backend_wayland->wayland_source, NULL);

  g_object_set (clutter_settings_get_default (), "font-dpi", 96 * 1024, NULL);

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
  wl_registry_add_listener (backend_wayland->wayland_registry,
                            &wayland_registry_listener,
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

  cogl_wayland_renderer_set_event_dispatch_enabled (renderer, !_no_event_dispatch);
  cogl_renderer_set_winsys_id (renderer, COGL_WINSYS_ID_EGL_WAYLAND);

  cogl_wayland_renderer_set_foreign_display (renderer,
                                             backend_wayland->wayland_display);

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

void
_clutter_backend_wayland_ensure_cursor (ClutterBackendWayland *backend_wayland)
{
  struct wl_cursor *cursor;

  backend_wayland->cursor_theme =
    wl_cursor_theme_load (NULL, /* default */
                          32,
                          backend_wayland->wayland_shm);

  cursor = wl_cursor_theme_get_cursor (backend_wayland->cursor_theme,
                                       "left_ptr");

  backend_wayland->cursor_buffer =
    wl_cursor_image_get_buffer (cursor->images[0]);

  if (backend_wayland->cursor_buffer)
    {
      backend_wayland->cursor_x = cursor->images[0]->hotspot_x;
      backend_wayland->cursor_y = cursor->images[0]->hotspot_y;
    }

  backend_wayland->cursor_surface =
    wl_compositor_create_surface (backend_wayland->wayland_compositor);
}

static void
clutter_backend_wayland_init (ClutterBackendWayland *backend_wayland)
{
}

/**
 * clutter_wayland_set_display
 * @display: pointer to a wayland display
 *
 * Sets the display connection Clutter should use; must be called
 * before clutter_init(), clutter_init_with_args() or other functions
 * pertaining Clutter's initialization process.
 *
 * If you are parsing the command line arguments by retrieving Clutter's
 * #GOptionGroup with clutter_get_option_group() and calling
 * g_option_context_parse() yourself, you should also call
 * clutter_wayland_set_display() before g_option_context_parse().
 *
 * Since: 1.16
 */
void
clutter_wayland_set_display (struct wl_display *display)
{
  if (_clutter_context_is_initialized ())
    {
      g_warning ("%s() can only be used before calling clutter_init()",
                 G_STRFUNC);
      return;
    }

  _foreign_display = display;
}

/**
 * clutter_wayland_disable_event_retrieval:
 *
 * Disables the dispatch of the events in the main loop.
 *
 * This is useful for integrating Clutter with another library that will do the
 * event dispatch; in general only a single source should be acting on changes
 * on the Wayland file descriptor.
 *
 * This function can only be called before calling clutter_init().
 *
 * This function should not be normally used by applications.
 *
 * Since: 1.16
 */
void
clutter_wayland_disable_event_retrieval (void)
{
  if (_clutter_context_is_initialized ())
    {
      g_warning ("%s() can only be used before calling clutter_init()",
                 G_STRFUNC);
      return;
    }

  _no_event_dispatch = TRUE;
}
