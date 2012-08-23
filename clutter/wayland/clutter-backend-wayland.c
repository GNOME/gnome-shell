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
#include "wayland/clutter-device-manager-wayland.h"
#include "wayland/clutter-event-wayland.h"
#include "wayland/clutter-stage-wayland.h"
#include "cogl/clutter-stage-cogl.h"

#include <wayland-client.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cogl/cogl.h>
#include <cogl/cogl-wayland-client.h>

#define clutter_backend_wayland_get_type     _clutter_backend_wayland_get_type

G_DEFINE_TYPE (ClutterBackendWayland, clutter_backend_wayland, CLUTTER_TYPE_BACKEND);

static void clutter_backend_wayland_load_cursor (ClutterBackendWayland *backend_wayland);

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
  else if (strcmp (interface, "wl_seat") == 0)
    {
      ClutterDeviceManager *device_manager = backend_wayland->device_manager;
      _clutter_device_manager_wayland_add_input_group (device_manager, id);
    }
  else if (strcmp (interface, "wl_shell") == 0)
    {
      backend_wayland->wayland_shell =
        wl_display_bind (display, id, &wl_shell_interface);
    }
  else if (strcmp (interface, "wl_shm") == 0)
    {
      backend_wayland->wayland_shm =
        wl_display_bind (display, id, &wl_shm_interface);
    }
  else if (strcmp (interface, "wl_output") == 0)
    {
      /* FIXME: Support multiple outputs */
      backend_wayland->wayland_output =
        wl_display_bind (display, id, &wl_output_interface);
      wl_output_add_listener (backend_wayland->wayland_output,
                              &wayland_output_listener,
                              backend_wayland);
    }
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

  /* We need the shm object before we can create the cursor */
  clutter_backend_wayland_load_cursor (backend_wayland);

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

  cogl_renderer_set_winsys_id (renderer, COGL_WINSYS_ID_EGL_WAYLAND);

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

/*
 * clutter_backend_wayland_load_cursor and the two functions below were copied
 * from GTK+ and adapted for clutter
 */
static void
set_pixbuf (GdkPixbuf     *pixbuf,
            unsigned char *map,
            int            width,
            int            height)
{
  int stride, i, n_channels;
  unsigned char *pixels, *end, *argb_pixels, *s, *d;

  stride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  argb_pixels = map;

#define MULT(_d,c,a,t) \
  do { t = c * a + 0x7f; _d = ((t >> 8) + t) >> 8; } while (0)

  if (n_channels == 4)
    {
      for (i = 0; i < height; i++)
        {
          s = pixels + i * stride;
          end = s + width * 4;
          d = argb_pixels + i * width * 4;
          while (s < end)
            {
              unsigned int t;

              MULT(d[0], s[2], s[3], t);
              MULT(d[1], s[1], s[3], t);
              MULT(d[2], s[0], s[3], t);
              d[3] = s[3];
              s += 4;
              d += 4;
            }
        }
    }
  else if (n_channels == 3)
    {
      for (i = 0; i < height; i++)
        {
          s = pixels + i * stride;
          end = s + width * 3;
          d = argb_pixels + i * width * 4;
          while (s < end)
            {
              d[0] = s[2];
              d[1] = s[1];
              d[2] = s[0];
              d[3] = 0xff;
              s += 3;
              d += 4;
            }
        }
    }
}

static struct wl_buffer *
create_cursor (ClutterBackendWayland *backend_wayland,
               GdkPixbuf             *pixbuf)
{
  int stride, fd;
  char *filename;
  GError *error = NULL;
  struct wl_shm_pool *pool;
  struct wl_buffer *buffer;
  gint width, height;
  gsize size;
  unsigned char *map;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  stride = width * 4;
  size = stride * height;

  fd = g_file_open_tmp ("wayland-shm-XXXXXX", &filename, &error);
  if (fd < 0)
    {
      g_critical (G_STRLOC ": Opening temporary file failed: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  unlink (filename);
  g_free (filename);

  if (ftruncate (fd, size) < 0)
    {
      g_critical (G_STRLOC ": Setting the size of temporary file failed: %s", g_strerror (errno));
      close (fd);
      return NULL;
    }

  map = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (map == MAP_FAILED)
    {
      g_critical (G_STRLOC ": Memory mapping file failed: %s", g_strerror (errno));
      close (fd);
      return NULL;
   }

  set_pixbuf (pixbuf, map, width, height);

  pool = wl_shm_create_pool (backend_wayland->wayland_shm, fd, size);
  close (fd);
  buffer = wl_shm_pool_create_buffer (pool,
                                      0,
                                      width,
                                      height,
                                      stride,
                                      WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy (pool);
  munmap (map, size);

  return buffer;
}

static void
clutter_backend_wayland_load_cursor (ClutterBackendWayland *backend_wayland)
{
  const gchar * const *directories;
  gint j;
  GdkPixbuf *pixbuf = NULL;
  GError *error = NULL;

  directories = g_get_system_data_dirs();

  for (j = 0; directories[j] != NULL; j++)
    {
      gchar *filename;
      filename = g_build_filename (directories[j],
                                   "weston",
                                   "left_ptr.png",
                                   NULL);
      if (g_file_test (filename, G_FILE_TEST_EXISTS))
        {
          pixbuf = gdk_pixbuf_new_from_file (filename, &error);

          if (error != NULL)
            {
              g_warning ("Failed to load cursor: %s: %s",
                         filename, error->message);
              g_error_free (error);
              return;
            }
          break;
        }
    }

  if (!pixbuf)
    return;

  backend_wayland->cursor_buffer = create_cursor (backend_wayland, pixbuf);

  if (backend_wayland->cursor_buffer)
    {
      backend_wayland->cursor_x = 0;
      backend_wayland->cursor_y = 0;
    }

  backend_wayland->cursor_surface =
    wl_compositor_create_surface (backend_wayland->wayland_compositor);

  g_object_unref (pixbuf);
}

static void
clutter_backend_wayland_init (ClutterBackendWayland *backend_wayland)
{
}
