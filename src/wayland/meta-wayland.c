/*
 * Wayland Support
 *
 * Copyright (C) 2012,2013 Intel Corporation
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
#include <clutter/wayland/clutter-wayland-compositor.h>
#include <clutter/wayland/clutter-wayland-surface.h>

#include <glib.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include <wayland-server.h>

#include "meta-wayland-private.h"
#include "meta-xwayland-private.h"
#include "meta-wayland-stage.h"
#include "meta-window-actor-private.h"
#include "meta-wayland-seat.h"
#include "meta-wayland-keyboard.h"
#include "meta-wayland-pointer.h"
#include "meta-wayland-data-device.h"
#include "meta-cursor-tracker-private.h"
#include "display-private.h"
#include "window-private.h"
#include <meta/types.h>
#include <meta/main.h>
#include "frame.h"
#include "meta-weston-launch.h"
#include "monitor-private.h"

static MetaWaylandCompositor _meta_wayland_compositor;

MetaWaylandCompositor *
meta_wayland_compositor_get_default (void)
{
  return &_meta_wayland_compositor;
}

static guint32
get_time (void)
{
  struct timeval tv;
  gettimeofday (&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static gboolean
wayland_event_source_prepare (GSource *base, int *timeout)
{
  WaylandEventSource *source = (WaylandEventSource *)base;

  *timeout = -1;

  wl_display_flush_clients (source->display);

  return FALSE;
}

static gboolean
wayland_event_source_check (GSource *base)
{
  WaylandEventSource *source = (WaylandEventSource *)base;
  return source->pfd.revents;
}

static gboolean
wayland_event_source_dispatch (GSource *base,
                               GSourceFunc callback,
                               void *data)
{
  WaylandEventSource *source = (WaylandEventSource *)base;
  struct wl_event_loop *loop = wl_display_get_event_loop (source->display);

  wl_event_loop_dispatch (loop, 0);

  return TRUE;
}

static GSourceFuncs wayland_event_source_funcs =
{
  wayland_event_source_prepare,
  wayland_event_source_check,
  wayland_event_source_dispatch,
  NULL
};

static GSource *
wayland_event_source_new (struct wl_display *display)
{
  WaylandEventSource *source;
  struct wl_event_loop *loop = wl_display_get_event_loop (display);

  source = (WaylandEventSource *) g_source_new (&wayland_event_source_funcs,
                                                sizeof (WaylandEventSource));
  source->display = display;
  source->pfd.fd = wl_event_loop_get_fd (loop);
  source->pfd.events = G_IO_IN | G_IO_ERR;
  g_source_add_poll (&source->source, &source->pfd);

  return &source->source;
}

static void
meta_wayland_buffer_destroy_handler (struct wl_listener *listener,
                                     void *data)
{
  MetaWaylandBuffer *buffer =
    wl_container_of (listener, buffer, destroy_listener);

  wl_signal_emit (&buffer->destroy_signal, buffer);
  g_slice_free (MetaWaylandBuffer, buffer);
}

void
meta_wayland_buffer_ref (MetaWaylandBuffer *buffer)
{
  buffer->ref_count++;
}

void
meta_wayland_buffer_unref (MetaWaylandBuffer *buffer)
{
  buffer->ref_count--;
  if (buffer->ref_count == 0)
    {
      g_clear_pointer (&buffer->texture, cogl_object_unref);
      wl_resource_queue_event (buffer->resource, WL_BUFFER_RELEASE);
    }
}

MetaWaylandBuffer *
meta_wayland_buffer_from_resource (struct wl_resource *resource)
{
  MetaWaylandBuffer *buffer;
  struct wl_listener *listener;

  listener =
    wl_resource_get_destroy_listener (resource,
                                      meta_wayland_buffer_destroy_handler);

  if (listener)
    {
      buffer = wl_container_of (listener, buffer, destroy_listener);
    }
  else
    {
      buffer = g_slice_new0 (MetaWaylandBuffer);

      buffer->resource = resource;
      wl_signal_init (&buffer->destroy_signal);
      buffer->destroy_listener.notify = meta_wayland_buffer_destroy_handler;
      wl_resource_add_destroy_listener (resource, &buffer->destroy_listener);
    }

  return buffer;
}

void
meta_wayland_compositor_set_input_focus (MetaWaylandCompositor *compositor,
                                         MetaWindow            *window)
{
  MetaWaylandSurface *surface = window ? window->surface : NULL;

  meta_wayland_keyboard_set_focus (&compositor->seat->keyboard,
                                   surface);
  meta_wayland_data_device_set_keyboard_focus (compositor->seat);
}

void
meta_wayland_compositor_repick (MetaWaylandCompositor *compositor)
{
  meta_wayland_seat_repick (compositor->seat, NULL);
}

static void
meta_wayland_compositor_create_surface (struct wl_client *wayland_client,
                                        struct wl_resource *wayland_compositor_resource,
                                        guint32 id)
{
  MetaWaylandCompositor *compositor =
    wl_resource_get_user_data (wayland_compositor_resource);
  MetaWaylandSurface *surface;

  surface = meta_wayland_surface_create (compositor,
					 wayland_client, id,
					 MIN (META_WL_SURFACE_VERSION,
					      wl_resource_get_version (wayland_compositor_resource)));
  
  compositor->surfaces = g_list_prepend (compositor->surfaces, surface);
}

static void
meta_wayland_region_destroy (struct wl_client *client,
                             struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
meta_wayland_region_add (struct wl_client *client,
                         struct wl_resource *resource,
                         gint32 x,
                         gint32 y,
                         gint32 width,
                         gint32 height)
{
  MetaWaylandRegion *region = wl_resource_get_user_data (resource);
  cairo_rectangle_int_t rectangle = { x, y, width, height };

  cairo_region_union_rectangle (region->region, &rectangle);
}

static void
meta_wayland_region_subtract (struct wl_client *client,
                              struct wl_resource *resource,
                              gint32 x,
                              gint32 y,
                              gint32 width,
                              gint32 height)
{
  MetaWaylandRegion *region = wl_resource_get_user_data (resource);
  cairo_rectangle_int_t rectangle = { x, y, width, height };

  cairo_region_subtract_rectangle (region->region, &rectangle);
}

const struct wl_region_interface meta_wayland_region_interface = {
  meta_wayland_region_destroy,
  meta_wayland_region_add,
  meta_wayland_region_subtract
};

static void
meta_wayland_region_resource_destroy_cb (struct wl_resource *resource)
{
  MetaWaylandRegion *region = wl_resource_get_user_data (resource);

  cairo_region_destroy (region->region);
  g_slice_free (MetaWaylandRegion, region);
}

static void
meta_wayland_compositor_create_region (struct wl_client *wayland_client,
                                       struct wl_resource *compositor_resource,
                                       uint32_t id)
{
  MetaWaylandRegion *region = g_slice_new0 (MetaWaylandRegion);

  region->resource = wl_resource_create (wayland_client,
					 &wl_region_interface,
					 MIN (META_WL_REGION_VERSION,
					      wl_resource_get_version (compositor_resource)),
					 id);
  wl_resource_set_implementation (region->resource,
				  &meta_wayland_region_interface, region,
				  meta_wayland_region_resource_destroy_cb);

  region->region = cairo_region_create ();
}

typedef struct {
  MetaOutput               *output;
  struct wl_global         *global;
  int                       x, y;
  enum wl_output_transform  transform;

  GList                    *resources;
} MetaWaylandOutput;

static void
output_resource_destroy (struct wl_resource *res)
{
  MetaWaylandOutput *wayland_output;

  wayland_output = wl_resource_get_user_data (res);
  wayland_output->resources = g_list_remove (wayland_output->resources, res);
}

static void
bind_output (struct wl_client *client,
             void *data,
             guint32 version,
             guint32 id)
{
  MetaWaylandOutput *wayland_output = data;
  MetaOutput *output = wayland_output->output;
  struct wl_resource *resource;
  guint mode_flags;

  resource = wl_resource_create (client, &wl_output_interface,
				 MIN (META_WL_OUTPUT_VERSION, version), id);
  wayland_output->resources = g_list_prepend (wayland_output->resources, resource);

  wl_resource_set_user_data (resource, wayland_output);
  wl_resource_set_destructor (resource, output_resource_destroy);

  meta_verbose ("Binding output %p/%s (%u, %u, %u, %u) x %f\n",
                output, output->name,
                output->crtc->rect.x, output->crtc->rect.y,
                output->crtc->rect.width, output->crtc->rect.height,
                output->crtc->current_mode->refresh_rate);

  wl_resource_post_event (resource,
                          WL_OUTPUT_GEOMETRY,
                          (int)output->crtc->rect.x,
                          (int)output->crtc->rect.y,
                          output->width_mm,
                          output->height_mm,
                          /* Cogl values reflect XRandR values,
                             and so does wayland */
                          output->subpixel_order,
                          output->vendor,
                          output->product,
                          output->crtc->transform);

  g_assert (output->crtc->current_mode != NULL);

  mode_flags = WL_OUTPUT_MODE_CURRENT;
  if (output->crtc->current_mode == output->preferred_mode)
    mode_flags |= WL_OUTPUT_MODE_PREFERRED;

  wl_resource_post_event (resource,
                          WL_OUTPUT_MODE,
                          mode_flags,
                          (int)output->crtc->current_mode->width,
                          (int)output->crtc->current_mode->height,
                          (int)output->crtc->current_mode->refresh_rate);

  if (version >= META_WL_OUTPUT_HAS_DONE)
    wl_resource_post_event (resource,
                            WL_OUTPUT_DONE);
}

static void
wayland_output_destroy_notify (gpointer data)
{
  MetaWaylandOutput *wayland_output = data;
  GList *resources;

  /* Make sure the destructors don't mess with the list */
  resources = wayland_output->resources;
  wayland_output->resources = NULL;

  wl_global_destroy (wayland_output->global);
  g_list_free (resources);

  g_slice_free (MetaWaylandOutput, wayland_output);
}

static void
wayland_output_update_for_output (MetaWaylandOutput *wayland_output,
                                  MetaOutput        *output)
{
  GList *iter;
  guint mode_flags;

  g_assert (output->crtc->current_mode != NULL);

  mode_flags = WL_OUTPUT_MODE_CURRENT;
  if (output->crtc->current_mode == output->preferred_mode)
    mode_flags |= WL_OUTPUT_MODE_PREFERRED;

  for (iter = wayland_output->resources; iter; iter = iter->next)
    {
      struct wl_resource *resource = iter->data;

      if (wayland_output->x != output->crtc->rect.x ||
          wayland_output->y != output->crtc->rect.y ||
          wayland_output->transform != output->crtc->transform)
        {
            wl_resource_post_event (resource,
                                    WL_OUTPUT_GEOMETRY,
                                    (int)output->crtc->rect.x,
                                    (int)output->crtc->rect.y,
                                    output->width_mm,
                                    output->height_mm,
                                    output->subpixel_order,
                                    output->vendor,
                                    output->product,
                                    output->crtc->transform);
        }

      wl_resource_post_event (resource,
                              WL_OUTPUT_MODE,
                              mode_flags,
                              (int)output->crtc->current_mode->width,
                              (int)output->crtc->current_mode->height,
                              (int)output->crtc->current_mode->refresh_rate);
    }

  /* It's very important that we change the output pointer here, as
     the old structure is about to be freed by MetaMonitorManager */
  wayland_output->output = output;
  wayland_output->x = output->crtc->rect.x;
  wayland_output->y = output->crtc->rect.y;
  wayland_output->transform = output->crtc->transform;
}

static GHashTable *
meta_wayland_compositor_update_outputs (MetaWaylandCompositor *compositor,
                                        MetaMonitorManager    *monitors)
{
  MetaOutput *outputs;
  unsigned int i, n_outputs;
  GHashTable *new_table;

  outputs = meta_monitor_manager_get_outputs (monitors, &n_outputs);
  new_table = g_hash_table_new_full (NULL, NULL, NULL, wayland_output_destroy_notify);

  for (i = 0; i < n_outputs; i++)
    {
      MetaOutput *output = &outputs[i];
      MetaWaylandOutput *wayland_output;

      /* wayland does not expose disabled outputs */
      if (output->crtc == NULL)
        {
          g_hash_table_remove (compositor->outputs, GSIZE_TO_POINTER (output->output_id));
          continue;
        }

      wayland_output = g_hash_table_lookup (compositor->outputs, GSIZE_TO_POINTER (output->output_id));

      if (wayland_output)
        {
          g_hash_table_steal (compositor->outputs, GSIZE_TO_POINTER (output->output_id));
        }
      else
        {
          wayland_output = g_slice_new0 (MetaWaylandOutput);
          wayland_output->global = wl_global_create (compositor->wayland_display,
                                                     &wl_output_interface,
						     META_WL_OUTPUT_VERSION,
                                                     wayland_output, bind_output);
        }

      wayland_output_update_for_output (wayland_output, output);
      g_hash_table_insert (new_table, GSIZE_TO_POINTER (output->output_id), wayland_output);
    }

  g_hash_table_destroy (compositor->outputs);
  return new_table;
}

const static struct wl_compositor_interface meta_wayland_compositor_interface = {
  meta_wayland_compositor_create_surface,
  meta_wayland_compositor_create_region
};

void
meta_wayland_compositor_paint_finished (MetaWaylandCompositor *compositor)
{
  while (!wl_list_empty (&compositor->frame_callbacks))
    {
      MetaWaylandFrameCallback *callback =
        wl_container_of (compositor->frame_callbacks.next, callback, link);

      wl_callback_send_done (callback->resource, get_time ());
      wl_resource_destroy (callback->resource);
    }
}

static void
compositor_bind (struct wl_client *client,
		 void *data,
                 guint32 version,
                 guint32 id)
{
  MetaWaylandCompositor *compositor = data;
  struct wl_resource *resource;

  resource = wl_resource_create (client, &wl_compositor_interface,
				 MIN (META_WL_COMPOSITOR_VERSION, version), id);
  wl_resource_set_implementation (resource, &meta_wayland_compositor_interface, compositor, NULL);
}

static void
stage_destroy_cb (void)
{
  meta_quit (META_EXIT_SUCCESS);
}

/**
 * meta_wayland_compositor_update:
 * @compositor: the #MetaWaylandCompositor instance
 * @event: the #ClutterEvent used to update @seat's state
 *
 * This is used to update display server state like updating cursor
 * position and keeping track of buttons and keys pressed. It must be
 * called for all input events coming from the underlying devices.
 */
void
meta_wayland_compositor_update (MetaWaylandCompositor *compositor,
                                const ClutterEvent    *event)
{
  meta_wayland_seat_update (compositor->seat, event);
}

/**
 * meta_wayland_compositor_handle_event:
 * @compositor: the #MetaWaylandCompositor instance
 * @event: the #ClutterEvent to be sent
 *
 * This method sends events to the focused wayland client, if any.
 *
 * Return value: whether @event was sent to a wayland client.
 */
gboolean
meta_wayland_compositor_handle_event (MetaWaylandCompositor *compositor,
                                      const ClutterEvent    *event)
{
  return meta_wayland_seat_handle_event (compositor->seat, event);
}

static void
on_monitors_changed (MetaMonitorManager    *monitors,
                     MetaWaylandCompositor *compositor)
{
  compositor->outputs = meta_wayland_compositor_update_outputs (compositor, monitors);
}

static void
set_gnome_env (const char *name,
	       const char *value)
{
  GDBusConnection *session_bus;
  GError *error = NULL;

  setenv (name, value, TRUE);

  session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (session_bus);

  g_dbus_connection_call_sync (session_bus,
			       "org.gnome.SessionManager",
			       "/org/gnome/SessionManager",
			       "org.gnome.SessionManager",
			       "Setenv",
			       g_variant_new ("(ss)", name, value),
			       NULL,
			       G_DBUS_CALL_FLAGS_NONE,
			       -1, NULL, &error);
  if (error)
    {
      if (g_strcmp0 (g_dbus_error_get_remote_error (error), "org.gnome.SessionManager.NotInInitialization") != 0)
        meta_warning ("Failed to set environment variable %s for gnome-session: %s\n", name, error->message);

      g_error_free (error);
    }
}

static void
meta_wayland_log_func (const char *fmt,
                       va_list     arg)
{
  char *str = g_strdup_vprintf (fmt, arg);
  g_warning ("WL: %s", str);
  g_free (str);
}

void
meta_wayland_init (void)
{
  MetaWaylandCompositor *compositor = &_meta_wayland_compositor;
  MetaMonitorManager *monitors;

  memset (compositor, 0, sizeof (MetaWaylandCompositor));

  compositor->wayland_display = wl_display_create ();
  if (compositor->wayland_display == NULL)
    g_error ("failed to create wayland display");

  wl_display_init_shm (compositor->wayland_display);
  wl_log_set_handler_server(meta_wayland_log_func);

  wl_list_init (&compositor->frame_callbacks);

  if (!wl_global_create (compositor->wayland_display,
			 &wl_compositor_interface,
			 META_WL_COMPOSITOR_VERSION,
			 compositor, compositor_bind))
    g_error ("Failed to register wayland compositor object");

  compositor->wayland_loop =
    wl_display_get_event_loop (compositor->wayland_display);
  compositor->wayland_event_source =
    wayland_event_source_new (compositor->wayland_display);

  /* XXX: Here we are setting the wayland event source to have a
   * slightly lower priority than the X event source, because we are
   * much more likely to get confused being told about surface changes
   * relating to X clients when we don't know what's happened to them
   * according to the X protocol.
   *
   * At some point we could perhaps try and get the X protocol proxied
   * over the wayland protocol so that we don't have to worry about
   * synchronizing the two command streams. */
  g_source_set_priority (compositor->wayland_event_source,
                         GDK_PRIORITY_EVENTS + 1);
  g_source_attach (compositor->wayland_event_source, NULL);

  clutter_wayland_set_compositor_display (compositor->wayland_display);

  /* If we're running on bare metal, we're a display server,
   * so start talking to weston-launch. */
#if defined(CLUTTER_WINDOWING_EGL)
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_EGL))
    compositor->launcher = meta_launcher_new ();
#endif

  if (clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
    g_error ("Failed to initialize Clutter");

  meta_monitor_manager_initialize ();
  monitors = meta_monitor_manager_get ();
  g_signal_connect (monitors, "monitors-changed",
                    G_CALLBACK (on_monitors_changed), compositor);

  compositor->outputs = g_hash_table_new_full (NULL, NULL, NULL, wayland_output_destroy_notify);
  compositor->outputs = meta_wayland_compositor_update_outputs (compositor, monitors);

  compositor->stage = meta_wayland_stage_new ();
  g_signal_connect (compositor->stage, "destroy",
                    G_CALLBACK (stage_destroy_cb), NULL);

  meta_wayland_data_device_manager_init (compositor->wayland_display);

  compositor->seat = meta_wayland_seat_new (compositor->wayland_display);

  meta_wayland_init_shell (compositor);

  clutter_actor_show (compositor->stage);

  /* FIXME: find the first free name instead */
  compositor->display_name = g_strdup ("wayland-0");
  if (wl_display_add_socket (compositor->wayland_display, compositor->display_name))
    g_error ("Failed to create socket");

  /* XXX: It's important that we only try and start xwayland after we
   * have initialized EGL because EGL implements the "wl_drm"
   * interface which xwayland requires to determine what drm device
   * name it should use.
   *
   * By waiting until we've shown the stage above we ensure that the
   * underlying GL resources for the surface have also been allocated
   * and so EGL must be initialized by this point.
   */

  if (!meta_xwayland_start (&compositor->xwayland_manager, compositor->wayland_display))
    g_error ("Failed to start X Wayland");

  set_gnome_env ("DISPLAY", compositor->xwayland_manager.display_name);
  set_gnome_env ("WAYLAND_DISPLAY", compositor->display_name);
}

void
meta_wayland_finalize (void)
{
  MetaWaylandCompositor *compositor;

  compositor = meta_wayland_compositor_get_default ();

  meta_xwayland_stop (&compositor->xwayland_manager);

  if (compositor->launcher)
    meta_launcher_free (compositor->launcher);
}

gboolean
meta_wayland_compositor_activate_vt (MetaWaylandCompositor  *compositor,
                                     int                     vt,
                                     GError                **error)
{
  if (compositor->launcher)
    {
      return meta_launcher_activate_vt (compositor->launcher, vt, error);
    }
  else
    {
      g_debug ("Ignoring VT switch keybinding, not running as display server");
      return TRUE;
    }
}

gboolean
meta_wayland_compositor_activate_session (MetaWaylandCompositor  *compositor,
                                          GError                **error)
{
  if (compositor->launcher)
    {
      return meta_launcher_activate_vt (compositor->launcher, -1, error);
    }
  else
    {
      g_debug ("Ignoring activate_session, not running as display server");
      return TRUE;
    }
}
