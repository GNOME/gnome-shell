#include <cogl/cogl.h>
#include <glib.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>

#include <wayland-server.h>

CoglColor black;

typedef struct _CoglandCompositor CoglandCompositor;

typedef struct
{
  struct wl_buffer *wayland_buffer;
  CoglTexture2D *texture;
  GList *surfaces_attached_to;
} CoglandBuffer;

typedef struct
{
  CoglandCompositor *compositor;

  struct wl_surface wayland_surface;
  int x;
  int y;
  CoglandBuffer *buffer;
} CoglandSurface;

typedef struct
{
  guint32 flags;
  int width;
  int height;
  int refresh;
} CoglandMode;

typedef struct
{
  struct wl_object wayland_output;

  gint32 x;
  gint32 y;
  gint32 width_mm;
  gint32 height_mm;

  CoglOnscreen *onscreen;

  GList *modes;

} CoglandOutput;

typedef struct
{
  GSource source;
  GPollFD pfd;
  struct wl_event_loop *loop;
} WaylandEventSource;

struct _CoglandCompositor
{
  struct wl_display *wayland_display;
  struct wl_shm *wayland_shm;
  struct wl_event_loop *wayland_loop;

  CoglDisplay *cogl_display;
  CoglContext *cogl_context;

  int virtual_width;
  int virtual_height;
  GList *outputs;

  GArray *frame_callbacks;

  CoglPrimitive *triangle;

  GSource *wayland_event_source;

  GList *surfaces;
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

static CoglandBuffer *
cogland_buffer_new (struct wl_buffer *wayland_buffer)
{
  CoglandBuffer *buffer = g_slice_new (CoglandBuffer);

  buffer->wayland_buffer = wayland_buffer;
  buffer->texture = NULL;
  buffer->surfaces_attached_to = NULL;

  return buffer;
}

static void
cogland_buffer_free (CoglandBuffer *buffer)
{
  GList *l;

  buffer->wayland_buffer->user_data = NULL;

  for (l = buffer->surfaces_attached_to; l; l = l->next)
    {
      CoglandSurface *surface = l->data;
      surface->buffer = NULL;
    }

  if (buffer->texture)
    cogl_object_unref (buffer->texture);

  g_list_free (buffer->surfaces_attached_to);
  g_slice_free (CoglandBuffer, buffer);
}

static void
shm_buffer_created (struct wl_buffer *wayland_buffer)
{
  wayland_buffer->user_data = cogland_buffer_new (wayland_buffer);
}

static void
shm_buffer_damaged (struct wl_buffer *wayland_buffer,
		    gint32 x,
                    gint32 y,
                    gint32 width,
                    gint32 height)
{
  CoglandBuffer *buffer = wayland_buffer->user_data;

  if (buffer->texture)
    {
      CoglPixelFormat format;

      switch (wl_shm_buffer_get_format (wayland_buffer))
        {
#if G_BYTE_ORDER == G_BIG_ENDIAN
          case WL_SHM_FORMAT_PREMULTIPLIED_ARGB32:
            format = COGL_PIXEL_FORMAT_ARGB_8888_PRE;
            break;
          case WL_SHM_FORMAT_ARGB32:
          case WL_SHM_FORMAT_XRGB32:
            format = COGL_PIXEL_FORMAT_ARGB_8888;
            break;
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
          case WL_SHM_FORMAT_PREMULTIPLIED_ARGB32:
            format = COGL_PIXEL_FORMAT_BGRA_8888_PRE;
            break;
          case WL_SHM_FORMAT_ARGB32:
          case WL_SHM_FORMAT_XRGB32:
            format = COGL_PIXEL_FORMAT_BGRA_8888;
            break;
#endif
          default:
            g_warn_if_reached ();
            format = COGL_PIXEL_FORMAT_ARGB_8888;
        }

      cogl_texture_set_region (COGL_TEXTURE (buffer->texture),
                               x, y,
                               x, y,
                               width, height,
                               width, height,
                               format,
                               wl_shm_buffer_get_stride (wayland_buffer),
                               wl_shm_buffer_get_data (wayland_buffer));
    }
}

static void
shm_buffer_destroyed (struct wl_buffer *wayland_buffer)
{
  if (wayland_buffer->user_data)
    cogland_buffer_free ((CoglandBuffer *)wayland_buffer->user_data);
}

const static struct wl_shm_callbacks shm_callbacks = {
  shm_buffer_created,
  shm_buffer_damaged,
  shm_buffer_destroyed
};

static void
cogland_surface_destroy (struct wl_client *wayland_client,
                         struct wl_resource *wayland_resource)
{
  wl_resource_destroy (wayland_resource, get_time ());
}

static void
cogland_surface_detach_buffer (CoglandSurface *surface)
{
  CoglandBuffer *buffer = surface->buffer;

  if (buffer)
    {
      buffer->surfaces_attached_to =
        g_list_remove (buffer->surfaces_attached_to, surface);
      if (buffer->surfaces_attached_to == NULL)
        cogland_buffer_free (buffer);
      surface->buffer = NULL;
    }
}

static void
cogland_surface_attach_buffer (struct wl_client *wayland_client,
                               struct wl_resource *wayland_surface_resource,
                               struct wl_resource *wayland_buffer_resource,
                               gint32 dx, gint32 dy)
{
  struct wl_buffer *wayland_buffer = wayland_buffer_resource->data;
  CoglandBuffer *buffer = wayland_buffer->user_data;
  CoglandSurface *surface = wayland_surface_resource->data;
  CoglandCompositor *compositor = surface->compositor;

  /* XXX: in the case where we are reattaching the same buffer we can
   * simply bail out. Note this is important because if we don't bail
   * out then the _detach_buffer will actually end up destroying the
   * buffer we're trying to attach. */
  if (buffer && surface->buffer == buffer)
    return;

  cogland_surface_detach_buffer (surface);

  /* XXX: it seems like for shm buffers we will have been notified of
   * the buffer already via the callbacks, but for drm buffers I guess
   * this will be the first we know of them? */
  if (!buffer)
    {
      buffer = cogland_buffer_new (wayland_buffer);
      wayland_buffer->user_data = buffer;
    }

  g_return_if_fail (g_list_find (buffer->surfaces_attached_to, surface) == NULL);

  buffer->surfaces_attached_to = g_list_prepend (buffer->surfaces_attached_to,
                                                 surface);

  if (!buffer->texture)
    {
      GError *error = NULL;

      buffer->texture =
        cogl_wayland_texture_2d_new_from_buffer (compositor->cogl_context,
                                                 wayland_buffer,
                                                 &error);
      if (!buffer->texture)
        g_error ("Failed to create texture_2d from wayland buffer: %s",
                 error->message);
    }

  surface->buffer = buffer;
}

static void
cogland_surface_damage (struct wl_client *client,
                        struct wl_resource *resource,
                        gint32 x,
                        gint32 y,
                        gint32 width,
                        gint32 height)
{
}

typedef struct _CoglandFrameCallback
{
  struct wl_resource resource;
} CoglandFrameCallback;

static void
destroy_frame_callback (struct wl_resource *callback_resource)
{
  CoglandFrameCallback *callback = callback_resource->data;

  g_slice_free (CoglandFrameCallback, callback);
}

static void
cogland_surface_frame (struct wl_client *client,
                       struct wl_resource *surface_resource,
                       guint32 callback_id)
{
  CoglandFrameCallback *callback;
  CoglandSurface *surface = surface_resource->data;

  callback = g_slice_new0 (CoglandFrameCallback);
  callback->resource.object.interface = &wl_callback_interface;
  callback->resource.object.id = callback_id;
  callback->resource.destroy = destroy_frame_callback;
  callback->resource.data = callback;

  wl_client_add_resource (client, &callback->resource);

  g_array_append_val (surface->compositor->frame_callbacks, callback);
}

const struct wl_surface_interface cogland_surface_interface = {
  cogland_surface_destroy,
  cogland_surface_attach_buffer,
  cogland_surface_damage,
  cogland_surface_frame
};

static void
cogland_surface_free (CoglandSurface *surface)
{
  CoglandCompositor *compositor = surface->compositor;
  compositor->surfaces = g_list_remove (compositor->surfaces, surface);
  cogland_surface_detach_buffer (surface);
  g_slice_free (CoglandSurface, surface);
}
static void
cogland_surface_resource_destroy_cb (struct wl_resource *resource)
{
  CoglandSurface *surface = resource->data;
  cogland_surface_free (surface);
}

static void
cogland_compositor_create_surface (struct wl_client *wayland_client,
                                   struct wl_resource *wayland_compositor_resource,
                                   guint32 id)
{
  CoglandCompositor *compositor = wayland_compositor_resource->data;
  CoglandSurface *surface = g_slice_new0 (CoglandSurface);

  surface->compositor = compositor;

  surface->wayland_surface.resource.destroy =
    cogland_surface_resource_destroy_cb;
  surface->wayland_surface.resource.object.id = id;
  surface->wayland_surface.resource.object.interface = &wl_surface_interface;
  surface->wayland_surface.resource.object.implementation =
          (void (**)(void)) &cogland_surface_interface;
  surface->wayland_surface.resource.data = surface;

  wl_client_add_resource (wayland_client, &surface->wayland_surface.resource);

  compositor->surfaces = g_list_prepend (compositor->surfaces,
                                         surface);
}

static void
bind_output (struct wl_client *client,
             void *data,
             guint32 version,
             guint32 id)
{
  CoglandOutput *output = data;
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
      CoglandMode *mode = l->data;
      wl_resource_post_event (resource,
                              WL_OUTPUT_MODE,
                              mode->flags,
                              mode->width,
                              mode->height,
                              mode->refresh);
    }
}

static void
cogland_compositor_create_output (CoglandCompositor *compositor,
                                  int x,
                                  int y,
                                  int width_mm,
                                  int height_mm)
{
  CoglandOutput *output = g_slice_new0 (CoglandOutput);
  CoglFramebuffer *fb;
  GError *error = NULL;
  CoglandMode *mode;

  output->x = x;
  output->y = y;
  output->width_mm = width_mm;
  output->height_mm = height_mm;

  output->wayland_output.interface = &wl_output_interface;

  wl_display_add_global (compositor->wayland_display,
                         &wl_output_interface,
                         output,
                         bind_output);

  output->onscreen = cogl_onscreen_new (compositor->cogl_context,
                                        width_mm, height_mm);
  /* Eventually there will be an implicit allocate on first use so this
   * will become optional... */
  fb = COGL_FRAMEBUFFER (output->onscreen);
  if (!cogl_framebuffer_allocate (fb, &error))
    g_error ("Failed to allocate framebuffer: %s\n", error->message);

  cogl_onscreen_show (output->onscreen);
#if 0
  cogl_framebuffer_set_viewport (fb, x, y, width, height);
#else
  cogl_push_framebuffer (fb);
  cogl_set_viewport (-x, -y,
                     compositor->virtual_width,
                     compositor->virtual_height);
  cogl_pop_framebuffer ();
#endif

  mode = g_slice_new0 (CoglandMode);
  mode->flags = 0;
  mode->width = width_mm;
  mode->height = height_mm;
  mode->refresh = 60;

  output->modes = g_list_prepend (output->modes, mode);

  compositor->outputs = g_list_prepend (compositor->outputs, output);
}

static gboolean
paint_cb (void *user_data)
{
  CoglandCompositor *compositor = user_data;
  GList *l;
  int i;

  for (l = compositor->outputs; l; l = l->next)
    {
      CoglandOutput *output = l->data;
      CoglFramebuffer *fb = COGL_FRAMEBUFFER (output->onscreen);
      GList *l2;

      cogl_push_framebuffer (fb);

#if 0
      cogl_framebuffer_clear (fb, COGL_BUFFER_BIT_COLOR);
#else
      cogl_clear (&black, COGL_BUFFER_BIT_COLOR);
#endif

      cogl_primitive_draw (compositor->triangle);

      for (l2 = compositor->surfaces; l2; l2 = l2->next)
        {
          CoglandSurface *surface = l2->data;

          if (surface->buffer)
            {
              CoglTexture2D *texture = surface->buffer->texture;
              cogl_set_source_texture (COGL_TEXTURE (texture));
              cogl_rectangle (-1, 1, 1, -1);
            }
        }
      cogl_framebuffer_swap_buffers (fb);

      cogl_pop_framebuffer ();
    }

  for (i = 0; i < compositor->frame_callbacks->len; i++)
    {
      CoglandFrameCallback *callback =
        g_array_index (compositor->frame_callbacks, CoglandFrameCallback *, i);

      wl_resource_post_event (&callback->resource,
                              WL_CALLBACK_DONE, get_time ());
      wl_resource_destroy (&callback->resource, 0);
    }
  g_array_set_size (compositor->frame_callbacks, 0);

  return TRUE;
}

const static struct wl_compositor_interface cogland_compositor_interface =
{
  cogland_compositor_create_surface,
};

static void
compositor_bind (struct wl_client *client,
		 void *data,
                 guint32 version,
                 guint32 id)
{
  CoglandCompositor *compositor = data;

  wl_client_add_object (client, &wl_compositor_interface,
                        &cogland_compositor_interface, id, compositor);
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

static const struct wl_shell_interface cogland_shell_interface =
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
                        &cogland_shell_interface, id, data);
}

int
main (int argc, char **argv)
{
  CoglandCompositor compositor;
  GMainLoop *loop;
  GError *error = NULL;
  CoglVertexP2C4 triangle_vertices[] = {
      {0, 0.7, 0xff, 0x00, 0x00, 0x80},
      {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
      {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
  };

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

  compositor.cogl_display = cogl_display_new (NULL, NULL);
  cogl_wayland_display_set_compositor_display (compositor.cogl_display,
                                               compositor.wayland_display);

  compositor.cogl_context = cogl_context_new (compositor.cogl_display, &error);
  if (!compositor.cogl_context)
    g_error ("Failed to create a Cogl context: %s\n", error->message);

  compositor.virtual_width = 640;
  compositor.virtual_height = 480;

  /* Emulate compositing with multiple monitors... */
  cogland_compositor_create_output (&compositor, 0, 0, 320, 240);
  cogland_compositor_create_output (&compositor, 320, 0, 320, 240);
  cogland_compositor_create_output (&compositor, 0, 240, 320, 240);
  cogland_compositor_create_output (&compositor, 320, 240, 320, 240);

  if (wl_display_add_global (compositor.wayland_display, &wl_shell_interface,
                             &compositor, bind_shell) == NULL)
    g_error ("Failed to register a global shell object");

  if (wl_display_add_socket (compositor.wayland_display, "wayland-0"))
    g_error ("Failed to create socket");

  compositor.triangle = cogl_primitive_new_p2c4 (COGL_VERTICES_MODE_TRIANGLES,
                                                 3, triangle_vertices);

  g_timeout_add (16, paint_cb, &compositor);

  g_main_loop_run (loop);

  return 0;
}
