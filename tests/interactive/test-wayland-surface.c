#define COGL_ENABLE_EXPERIMENTAL_2_0_API
#include <clutter/clutter.h>
#include <clutter/wayland/clutter-wayland-compositor.h>
#include <clutter/wayland/clutter-wayland-surface.h>

#include <glib.h>
#include <sys/time.h>
#include <string.h>

#include <wayland-server.h>

typedef struct _TWSCompositor TWSCompositor;

typedef struct
{
  struct wl_buffer *wayland_buffer;
  GList *surfaces_attached_to;
} TWSBuffer;

typedef struct
{
  TWSCompositor *compositor;
  struct wl_surface wayland_surface;
  int x;
  int y;
  TWSBuffer *buffer;
  ClutterActor *actor;
} TWSSurface;

typedef struct
{
  guint32 flags;
  int width;
  int height;
  int refresh;
} TWSMode;

typedef struct
{
  struct wl_object wayland_output;
  int x;
  int y;
  int width_mm;
  int height_mm;
  /* XXX: with sliced stages we'd reference a CoglFramebuffer here. */

  GList *modes;
} TWSOutput;

typedef struct
{
  GSource source;
  GPollFD pfd;
  struct wl_event_loop *loop;
} WaylandEventSource;

typedef struct
{
  struct wl_resource resource;
} TWSFrameCallback;

struct _TWSCompositor
{
  struct wl_display *wayland_display;
  struct wl_shm *wayland_shm;
  struct wl_event_loop *wayland_loop;
  ClutterActor *stage;
  GList *outputs;
  GSource *wayland_event_source;
  GList *surfaces;
  GArray *frame_callbacks;
};

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
  *timeout = -1;
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
  wl_event_loop_dispatch (source->loop, 0);
  return TRUE;
}

static GSourceFuncs wayland_event_source_funcs =
{
  wayland_event_source_prepare,
  wayland_event_source_check,
  wayland_event_source_dispatch,
  NULL
};

GSource *
wayland_event_source_new (struct wl_event_loop *loop)
{
  WaylandEventSource *source;

  source = (WaylandEventSource *) g_source_new (&wayland_event_source_funcs,
                                                sizeof (WaylandEventSource));
  source->loop = loop;
  source->pfd.fd = wl_event_loop_get_fd (loop);
  source->pfd.events = G_IO_IN | G_IO_ERR;
  g_source_add_poll (&source->source, &source->pfd);

  return &source->source;
}

static TWSBuffer *
tws_buffer_new (struct wl_buffer *wayland_buffer)
{
  TWSBuffer *buffer = g_slice_new (TWSBuffer);

  buffer->wayland_buffer = wayland_buffer;
  buffer->surfaces_attached_to = NULL;

  return buffer;
}

static void
tws_buffer_free (TWSBuffer *buffer)
{
  GList *l;

  buffer->wayland_buffer->user_data = NULL;

  for (l = buffer->surfaces_attached_to; l; l = l->next)
    {
      TWSSurface *surface = l->data;
      surface->buffer = NULL;
    }

  g_list_free (buffer->surfaces_attached_to);
  g_slice_free (TWSBuffer, buffer);
}

static void
shm_buffer_created (struct wl_buffer *wayland_buffer)
{
  wayland_buffer->user_data = tws_buffer_new (wayland_buffer);
}

static void
shm_buffer_damaged (struct wl_buffer *wayland_buffer,
		    gint32 x,
                    gint32 y,
                    gint32 width,
                    gint32 height)
{
  TWSBuffer *buffer = wayland_buffer->user_data;
  GList *l;

  for (l = buffer->surfaces_attached_to; l; l = l->next)
    {
      TWSSurface *surface = l->data;
      ClutterWaylandSurface *surface_actor =
        CLUTTER_WAYLAND_SURFACE (surface->actor);
      clutter_wayland_surface_damage_buffer (surface_actor,
                                             wayland_buffer,
                                             x, y, width, height);
    }
}

static void
shm_buffer_destroyed (struct wl_buffer *wayland_buffer)
{
  if (wayland_buffer->user_data)
    tws_buffer_free ((TWSBuffer *)wayland_buffer->user_data);
}

const static struct wl_shm_callbacks shm_callbacks = {
  shm_buffer_created,
  shm_buffer_damaged,
  shm_buffer_destroyed
};

static void
tws_surface_destroy (struct wl_client *wayland_client,
                     struct wl_resource *wayland_resource)
{
  wl_resource_destroy (wayland_resource, get_time ());
}

static void
tws_surface_detach_buffer (TWSSurface *surface)
{
  TWSBuffer *buffer = surface->buffer;

  if (buffer)
    {
      buffer->surfaces_attached_to =
        g_list_remove (buffer->surfaces_attached_to, surface);
      if (buffer->surfaces_attached_to == NULL)
        tws_buffer_free (buffer);
      surface->buffer = NULL;
    }
}

static void
tws_surface_attach_buffer (struct wl_client *wayland_client,
                           struct wl_resource *wayland_surface_resource,
                           struct wl_resource *wayland_buffer_resource,
                           gint32 dx, gint32 dy)
{
  struct wl_buffer *wayland_buffer = wayland_buffer_resource->data;
  TWSBuffer *buffer = wayland_buffer->user_data;
  TWSSurface *surface = wayland_surface_resource->data;
  TWSCompositor *compositor = surface->compositor;
  ClutterWaylandSurface *surface_actor;

  /* XXX: in the case where we are reattaching the same buffer we can
   * simply bail out. Note this is important because if we don't bail
   * out then the _detach_buffer will actually end up destroying the
   * buffer we're trying to attach. */
  if (buffer && surface->buffer == buffer)
    return;

  tws_surface_detach_buffer (surface);

  /* XXX: we will have been notified of shm buffers already via the
   * callbacks, but this will be the first we know of drm buffers */
  if (!buffer)
    {
      buffer = tws_buffer_new (wayland_buffer);
      wayland_buffer->user_data = buffer;
    }

  g_return_if_fail (g_list_find (buffer->surfaces_attached_to, surface) == NULL);

  buffer->surfaces_attached_to = g_list_prepend (buffer->surfaces_attached_to,
                                                 surface);

  if (!surface->actor)
    {
      surface->actor = clutter_wayland_surface_new (&surface->wayland_surface);
      clutter_container_add_actor (CLUTTER_CONTAINER (compositor->stage),
                                   surface->actor);
    }

  surface_actor = CLUTTER_WAYLAND_SURFACE (surface->actor);
  if (!clutter_wayland_surface_attach_buffer (surface_actor, wayland_buffer,
                                              NULL))
    g_warning ("Failed to attach buffer to ClutterWaylandSurface");

  surface->buffer = buffer;
}

static void
tws_surface_damage (struct wl_client *client,
                    struct wl_resource *resource,
                    gint32 x,
                    gint32 y,
                    gint32 width,
                    gint32 height)
{
}

static void
destroy_frame_callback (struct wl_resource *callback_resource)
{
  TWSFrameCallback *callback = callback_resource->data;

  g_slice_free (TWSFrameCallback, callback);
}

static void
tws_surface_frame (struct wl_client *client,
                   struct wl_resource *surface_resource,
                   guint32 callback_id)
{
  TWSFrameCallback *callback;
  TWSSurface *surface = surface_resource->data;

  callback = g_slice_new0 (TWSFrameCallback);
  callback->resource.object.interface = &wl_callback_interface;
  callback->resource.object.id = callback_id;
  callback->resource.destroy = destroy_frame_callback;
  callback->resource.data = callback;

  wl_client_add_resource (client, &callback->resource);

  g_array_append_val (surface->compositor->frame_callbacks, callback);
}

const struct wl_surface_interface tws_surface_interface = {
  tws_surface_destroy,
  tws_surface_attach_buffer,
  tws_surface_damage,
  tws_surface_frame
};

static void
tws_surface_free (TWSSurface *surface)
{
  TWSCompositor *compositor = surface->compositor;
  compositor->surfaces = g_list_remove (compositor->surfaces, surface);
  tws_surface_detach_buffer (surface);

  clutter_actor_destroy (surface->actor);

  g_slice_free (TWSSurface, surface);
}

static void
tws_surface_resource_destroy_cb (struct wl_resource *wayland_surface_resource)
{
  TWSSurface *surface = wayland_surface_resource->data;
  tws_surface_free (surface);
}

static void
tws_compositor_create_surface (struct wl_client *wayland_client,
                               struct wl_resource *wayland_compositor_resource,
                               guint32 id)
{
  TWSCompositor *compositor = wayland_compositor_resource->data;
  TWSSurface *surface = g_slice_new0 (TWSSurface);

  surface->compositor = compositor;

  surface->wayland_surface.resource.destroy =
    tws_surface_resource_destroy_cb;
  surface->wayland_surface.resource.object.id = id;
  surface->wayland_surface.resource.object.interface = &wl_surface_interface;
  surface->wayland_surface.resource.object.implementation =
          (void (**)(void)) &tws_surface_interface;
  surface->wayland_surface.resource.data = surface;

  wl_client_add_resource (wayland_client, &surface->wayland_surface.resource);

  compositor->surfaces = g_list_prepend (compositor->surfaces, surface);
}

static void
bind_output (struct wl_client *client,
             void *data,
             guint32 version,
             guint32 id)
{
  TWSOutput *output = data;
  struct wl_resource *resource =
    wl_client_add_object (client, &wl_output_interface, NULL, id, data);
  GList *l;

  wl_resource_post_event (resource,
                          WL_OUTPUT_GEOMETRY,
                          output->x, output->y,
                          output->width_mm,
                          output->height_mm,
                          0, /* subpixel: unknown */
                          "unknown", /* make */
                          "unknown"); /* model */

  for (l = output->modes; l; l = l->next)
    {
      TWSMode *mode = l->data;
      wl_resource_post_event (resource,
                              WL_OUTPUT_MODE,
                              mode->flags,
                              mode->width,
                              mode->height,
                              mode->refresh);
    }
}

static void
tws_compositor_create_output (TWSCompositor *compositor,
                              int x,
                              int y,
                              int width_mm,
                              int height_mm)
{
  TWSOutput *output = g_slice_new0 (TWSOutput);

  output->wayland_output.interface = &wl_output_interface;

  output->x = x;
  output->y = y;
  output->width_mm = width_mm;
  output->height_mm = height_mm;

  wl_display_add_global (compositor->wayland_display,
                         &wl_output_interface,
                         output,
                         bind_output);

  /* XXX: eventually we will support sliced stages and an output should
   * correspond to a slice/CoglFramebuffer, but for now we only support
   * one output so we make sure it always matches the size of the stage
   */
  clutter_actor_set_size (compositor->stage, width_mm, height_mm);

  compositor->outputs = g_list_prepend (compositor->outputs, output);
}

const static struct wl_compositor_interface tws_compositor_interface = {
  tws_compositor_create_surface,
};

static void
paint_finished_cb (ClutterActor *self, void *user_data)
{
  TWSCompositor *compositor = user_data;
  int i;

  for (i = 0; i < compositor->frame_callbacks->len; i++)
    {
      TWSFrameCallback *callback =
        g_array_index (compositor->frame_callbacks, TWSFrameCallback *, i);

      wl_resource_post_event (&callback->resource,
                              WL_CALLBACK_DONE, get_time ());
      wl_resource_destroy (&callback->resource, 0);
    }
  g_array_set_size (compositor->frame_callbacks, 0);
}

static void
compositor_bind (struct wl_client *client,
		 void *data,
                 guint32 version,
                 guint32 id)
{
  TWSCompositor *compositor = data;

  wl_client_add_object (client, &wl_compositor_interface,
                        &tws_compositor_interface, id, compositor);
}

static void
shell_move(struct wl_client *client,
           struct wl_resource *resource,
           struct wl_resource *surface_resource,
           struct wl_resource *input_resource,
           guint32 time)
{
}

static void
shell_resize (struct wl_client *client,
              struct wl_resource *resource,
              struct wl_resource *surface_resource,
              struct wl_resource *input_resource,
              guint32 time,
              guint32 edges)
{
}

static void
shell_set_toplevel (struct wl_client *client,
		    struct wl_resource *resource,
		    struct wl_resource *surface_resource)
{
}

static void
shell_set_transient (struct wl_client *client,
		     struct wl_resource *resource,
		     struct wl_resource *surface_resource,
		     struct wl_resource *parent_resource,
		     int x, int y, uint32_t flags)
{
}

static void
shell_set_fullscreen (struct wl_client *client,
		      struct wl_resource *resource,
		      struct wl_resource *surface_resource)
{
}

static const struct wl_shell_interface tws_shell_interface =
{
  shell_move,
  shell_resize,
  shell_set_toplevel,
  shell_set_transient,
  shell_set_fullscreen
};

static void
bind_shell (struct wl_client *client,
            void *data,
            guint32 version,
            guint32 id)
{
  wl_client_add_object (client, &wl_shell_interface,
                        &tws_shell_interface, id, data);
}

G_MODULE_EXPORT int
test_wayland_surface_main (int argc, char **argv)
{
  TWSCompositor compositor;
  GMainLoop *loop;

  memset (&compositor, 0, sizeof (compositor));

  compositor.wayland_display = wl_display_create ();
  if (compositor.wayland_display == NULL)
    g_error ("failed to create wayland display");

  compositor.frame_callbacks = g_array_new (FALSE, FALSE, sizeof (void *));

  if (!wl_display_add_global (compositor.wayland_display,
                              &wl_compositor_interface,
			      &compositor,
                              compositor_bind))
    g_error ("Failed to register wayland compositor object");

  compositor.wayland_shm = wl_shm_init (compositor.wayland_display,
                                        &shm_callbacks);
  if (!compositor.wayland_shm)
    g_error ("Failed to allocate setup wayland shm callbacks");

  loop = g_main_loop_new (NULL, FALSE);
  compositor.wayland_loop =
    wl_display_get_event_loop (compositor.wayland_display);
  compositor.wayland_event_source =
    wayland_event_source_new (compositor.wayland_loop);
  g_source_attach (compositor.wayland_event_source, NULL);

  clutter_wayland_set_compositor_display (compositor.wayland_display);

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  compositor.stage = clutter_stage_get_default ();
  clutter_stage_set_user_resizable (CLUTTER_STAGE (compositor.stage), FALSE);
  g_signal_connect_after (compositor.stage, "paint",
                          G_CALLBACK (paint_finished_cb), &compositor);

  tws_compositor_create_output (&compositor, 0, 0, 800, 600);

  if (wl_display_add_global (compositor.wayland_display, &wl_shell_interface,
                             &compositor, bind_shell) == NULL)
    g_error ("Failed to register a global shell object");

  clutter_actor_show (compositor.stage);

  if (wl_display_add_socket (compositor.wayland_display, "wayland-0"))
    g_error ("Failed to create socket");

  g_main_loop_run (loop);

  return 0;
}
