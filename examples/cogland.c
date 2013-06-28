#include <cogl/cogl.h>
#include <cogl/cogl-wayland-server.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

#include <wayland-server.h>

typedef struct _CoglandCompositor CoglandCompositor;

typedef struct
{
  int x1, y1, x2, y2;
} CoglandRegion;

typedef struct
{
  struct wl_resource *resource;
  CoglandRegion region;
} CoglandSharedRegion;

typedef struct
{
  struct wl_resource *resource;
  struct wl_signal destroy_signal;
  struct wl_listener destroy_listener;

  union
  {
    struct wl_shm_buffer *shm_buffer;
    struct wl_buffer *legacy_buffer;
  };

  int32_t width, height;
  uint32_t busy_count;
} CoglandBuffer;

typedef struct
{
  CoglandBuffer *buffer;
  struct wl_listener destroy_listener;
} CoglandBufferReference;

typedef struct
{
  CoglandCompositor *compositor;

  struct wl_resource *resource;
  int x;
  int y;
  CoglandBufferReference buffer_ref;
  CoglTexture2D *texture;

  CoglBool has_shell_surface;

  struct wl_signal destroy_signal;

  /* All the pending state, that wl_surface.commit will apply. */
  struct
  {
    /* wl_surface.attach */
    CoglBool newly_attached;
    CoglandBuffer *buffer;
    struct wl_listener buffer_destroy_listener;
    int32_t sx;
    int32_t sy;

    /* wl_surface.damage */
    CoglandRegion damage;

    /* wl_surface.frame */
    struct wl_list frame_callback_list;
  } pending;
} CoglandSurface;

typedef struct
{
  CoglandSurface *surface;
  struct wl_resource *resource;
  struct wl_listener surface_destroy_listener;
} CoglandShellSurface;

typedef struct
{
  uint32_t flags;
  int width;
  int height;
  int refresh;
} CoglandMode;

typedef struct
{
  struct wl_object wayland_output;

  int32_t x;
  int32_t y;
  int32_t width_mm;
  int32_t height_mm;

  CoglOnscreen *onscreen;

  GList *modes;

} CoglandOutput;

typedef struct
{
  GSource source;
  GPollFD pfd;
  struct wl_display *display;
} WaylandEventSource;

struct _CoglandCompositor
{
  struct wl_display *wayland_display;
  struct wl_event_loop *wayland_loop;

  CoglContext *cogl_context;

  int virtual_width;
  int virtual_height;
  GList *outputs;

  struct wl_list frame_callbacks;

  CoglPrimitive *triangle;
  CoglPipeline *triangle_pipeline;

  GSource *wayland_event_source;

  GList *surfaces;

  unsigned int redraw_idle;
};

static CoglBool option_multiple_outputs = FALSE;

static GOptionEntry
options[] =
  {
    {
      "multiple", 'm', 0, G_OPTION_ARG_NONE, &option_multiple_outputs,
      "Split the compositor into four outputs", NULL
    },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

static CoglBool
process_arguments (int *argc, char ***argv,
                   GError **error)
{
  GOptionContext *context;
  CoglBool ret;
  GOptionGroup *group;

  group = g_option_group_new (NULL, /* name */
                              NULL, /* description */
                              NULL, /* help_description */
                              NULL, /* user_data */
                              NULL /* destroy notify */);
  g_option_group_add_entries (group, options);
  context = g_option_context_new ("- An example Wayland compositor using Cogl");
  g_option_context_set_main_group (context, group);
  ret = g_option_context_parse (context, argc, argv, error);
  g_option_context_free (context);

  if (ret && *argc > 1)
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
                   "Unknown option '%s'", (* argv)[1]);
      ret = FALSE;
    }

  return ret;
}

static uint32_t
get_time (void)
{
  struct timeval tv;
  gettimeofday (&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void
region_init (CoglandRegion *region)
{
  memset (region, 0, sizeof (*region));
}

static CoglBool
region_is_empty (const CoglandRegion *region)
{
  return region->x1 == region->x2 || region->y1 == region->y2;
}

static void
region_add (CoglandRegion *region,
            int x,
            int y,
            int w,
            int h)
{
  if (region_is_empty (region))
    {
      region->x1 = x;
      region->y1 = y;
      region->x2 = x + w;
      region->y2 = y + h;
    }
  else
    {
      if (x < region->x1)
        region->x1 = x;
      if (y < region->y1)
        region->y1 = y;
      if (x + w > region->x2)
        region->x2 = x + w;
      if (y + h > region->y2)
        region->y2 = y + h;
    }
}

static void
region_subtract (CoglandRegion *region,
                 int x,
                 int y,
                 int w,
                 int h)
{
  /* FIXME */
}

static CoglBool
wayland_event_source_prepare (GSource *base, int *timeout)
{
  WaylandEventSource *source = (WaylandEventSource *)base;

  *timeout = -1;

  wl_display_flush_clients (source->display);

  return FALSE;
}

static CoglBool
wayland_event_source_check (GSource *base)
{
  WaylandEventSource *source = (WaylandEventSource *)base;

  return source->pfd.revents;
}

static CoglBool
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
  struct wl_event_loop *loop =
    wl_display_get_event_loop (display);

  source = (WaylandEventSource *) g_source_new (&wayland_event_source_funcs,
                                                sizeof (WaylandEventSource));
  source->display = display;
  source->pfd.fd = wl_event_loop_get_fd (loop);
  source->pfd.events = G_IO_IN | G_IO_ERR;
  g_source_add_poll (&source->source, &source->pfd);

  return &source->source;
}

static void
cogland_buffer_destroy_handler (struct wl_listener *listener,
                                void *data)
{
  CoglandBuffer *buffer = wl_container_of (listener, buffer, destroy_listener);

  wl_signal_emit (&buffer->destroy_signal, buffer);
  g_slice_free (CoglandBuffer, buffer);
}

static CoglandBuffer *
cogland_buffer_from_resource (struct wl_resource *resource)
{
  CoglandBuffer *buffer;
  struct wl_listener *listener;

  listener = wl_resource_get_destroy_listener (resource,
                                               cogland_buffer_destroy_handler);

  if (listener)
    {
      buffer = wl_container_of (listener, buffer, destroy_listener);
    }
  else
    {
      buffer = g_slice_new0 (CoglandBuffer);

      buffer->resource = resource;
      wl_signal_init (&buffer->destroy_signal);
      buffer->destroy_listener.notify = cogland_buffer_destroy_handler;
      wl_resource_add_destroy_listener (resource, &buffer->destroy_listener);
    }

  return buffer;
}

static void
cogland_buffer_reference_handle_destroy (struct wl_listener *listener,
                                         void *data)
{
  CoglandBufferReference *ref =
    wl_container_of (listener, ref, destroy_listener);

  g_assert (data == ref->buffer);

  ref->buffer = NULL;
}

static void
cogland_buffer_reference (CoglandBufferReference *ref,
                          CoglandBuffer *buffer)
{
  if (ref->buffer && buffer != ref->buffer)
    {
      ref->buffer->busy_count--;

      if (ref->buffer->busy_count == 0)
        {
          g_assert (wl_resource_get_client (ref->buffer->resource));
          wl_resource_queue_event (ref->buffer->resource, WL_BUFFER_RELEASE);
        }

      wl_list_remove (&ref->destroy_listener.link);
    }

  if (buffer && buffer != ref->buffer)
    {
      buffer->busy_count++;
      wl_signal_add (&buffer->destroy_signal, &ref->destroy_listener);
    }

  ref->buffer = buffer;
  ref->destroy_listener.notify = cogland_buffer_reference_handle_destroy;
}

typedef struct _CoglandFrameCallback
{
  struct wl_list link;

  /* Pointer back to the compositor */
  CoglandCompositor *compositor;

  struct wl_resource *resource;
} CoglandFrameCallback;

static CoglBool
paint_cb (void *user_data)
{
  CoglandCompositor *compositor = user_data;
  GList *l;

  for (l = compositor->outputs; l; l = l->next)
    {
      CoglandOutput *output = l->data;
      CoglFramebuffer *fb = COGL_FRAMEBUFFER (output->onscreen);
      GList *l2;

      cogl_push_framebuffer (fb);

      cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);

      cogl_framebuffer_draw_primitive (fb, compositor->triangle_pipeline,
                                       compositor->triangle);

      for (l2 = compositor->surfaces; l2; l2 = l2->next)
        {
          CoglandSurface *surface = l2->data;

          if (surface->texture)
            {
              CoglTexture2D *texture = surface->texture;
              cogl_set_source_texture (COGL_TEXTURE (texture));
              cogl_rectangle (-1, 1, 1, -1);
            }
        }
      cogl_onscreen_swap_buffers (COGL_ONSCREEN (fb));

      cogl_pop_framebuffer ();
    }

  while (!wl_list_empty (&compositor->frame_callbacks))
    {
      CoglandFrameCallback *callback =
        wl_container_of (compositor->frame_callbacks.next, callback, link);

      wl_callback_send_done (callback->resource, get_time ());
      wl_resource_destroy (callback->resource);
    }

  compositor->redraw_idle = 0;

  return G_SOURCE_REMOVE;
}

static void
cogland_queue_redraw (CoglandCompositor *compositor)
{
  if (compositor->redraw_idle == 0)
    compositor->redraw_idle = g_idle_add (paint_cb, compositor);
}

static void
surface_damaged (CoglandSurface *surface,
                 int32_t x,
                 int32_t y,
                 int32_t width,
                 int32_t height)
{
  if (surface->buffer_ref.buffer &&
      surface->texture)
    {
      struct wl_shm_buffer *shm_buffer =
        wl_shm_buffer_get (surface->buffer_ref.buffer->resource);

      if (shm_buffer)
        {
          CoglPixelFormat format;
          int stride = wl_shm_buffer_get_stride (shm_buffer);
          const uint8_t *data = wl_shm_buffer_get_data (shm_buffer);

          switch (wl_shm_buffer_get_format (shm_buffer))
            {
#if G_BYTE_ORDER == G_BIG_ENDIAN
            case WL_SHM_FORMAT_ARGB8888:
              format = COGL_PIXEL_FORMAT_ARGB_8888_PRE;
              break;
            case WL_SHM_FORMAT_XRGB8888:
              format = COGL_PIXEL_FORMAT_ARGB_8888;
              break;
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
            case WL_SHM_FORMAT_ARGB8888:
              format = COGL_PIXEL_FORMAT_BGRA_8888_PRE;
              break;
            case WL_SHM_FORMAT_XRGB8888:
              format = COGL_PIXEL_FORMAT_BGRA_8888;
              break;
#endif
            default:
              g_warn_if_reached ();
              format = COGL_PIXEL_FORMAT_ARGB_8888;
            }

          cogl_texture_set_region (COGL_TEXTURE (surface->texture),
                                   x, y, /* src_x/y */
                                   x, y, /* dst_x/y */
                                   width, height, /* dst_width/height */
                                   width, height, /* width/height */
                                   format,
                                   stride,
                                   data);
        }
    }

  cogland_queue_redraw (surface->compositor);
}

static void
cogland_surface_destroy (struct wl_client *wayland_client,
                         struct wl_resource *wayland_resource)
{
  wl_resource_destroy (wayland_resource);
}

static void
cogland_surface_attach (struct wl_client *wayland_client,
                        struct wl_resource *wayland_surface_resource,
                        struct wl_resource *wayland_buffer_resource,
                        int32_t sx, int32_t sy)
{
  CoglandSurface *surface = wayland_surface_resource->data;
  CoglandBuffer *buffer;

  if (wayland_buffer_resource)
    buffer = cogland_buffer_from_resource (wayland_buffer_resource);
  else
    buffer = NULL;

  /* Attach without commit in between does not went wl_buffer.release */
  if (surface->pending.buffer)
    wl_list_remove (&surface->pending.buffer_destroy_listener.link);

  surface->pending.sx = sx;
  surface->pending.sy = sy;
  surface->pending.buffer = buffer;
  surface->pending.newly_attached = TRUE;

  if (buffer)
    wl_signal_add (&buffer->destroy_signal,
                   &surface->pending.buffer_destroy_listener);
}

static void
cogland_surface_damage (struct wl_client *client,
                        struct wl_resource *resource,
                        int32_t x,
                        int32_t y,
                        int32_t width,
                        int32_t height)
{
  CoglandSurface *surface = resource->data;

  region_add (&surface->pending.damage, x, y, width, height);
}

static void
destroy_frame_callback (struct wl_resource *callback_resource)
{
  CoglandFrameCallback *callback =
    wl_resource_get_user_data (callback_resource);

  wl_list_remove (&callback->link);
  g_slice_free (CoglandFrameCallback, callback);
}

static void
cogland_surface_frame (struct wl_client *client,
                       struct wl_resource *surface_resource,
                       uint32_t callback_id)
{
  CoglandFrameCallback *callback;
  CoglandSurface *surface = surface_resource->data;

  callback = g_slice_new0 (CoglandFrameCallback);
  callback->compositor = surface->compositor;
  callback->resource = wl_client_add_object (client,
                                             &wl_callback_interface,
                                             NULL, /* no implementation */
                                             callback_id,
                                             callback);
  wl_resource_set_destructor (callback->resource, destroy_frame_callback);

  wl_list_insert (surface->pending.frame_callback_list.prev, &callback->link);
}

static void
cogland_surface_set_opaque_region (struct wl_client *client,
                                   struct wl_resource *resource,
                                   struct wl_resource *region)
{
}

static void
cogland_surface_set_input_region (struct wl_client *client,
                                  struct wl_resource *resource,
                                  struct wl_resource *region)
{
}

static void
cogland_surface_commit (struct wl_client *client,
                        struct wl_resource *resource)
{
  CoglandSurface *surface = resource->data;
  CoglandCompositor *compositor = surface->compositor;

  /* wl_surface.attach */
  if (surface->pending.newly_attached &&
      surface->buffer_ref.buffer != surface->pending.buffer)
    {
      CoglError *error = NULL;

      if (surface->texture)
        {
          cogl_object_unref (surface->texture);
          surface->texture = NULL;
        }

      cogland_buffer_reference (&surface->buffer_ref, surface->pending.buffer);

      if (surface->pending.buffer)
        {
          struct wl_resource *buffer_resource =
            surface->pending.buffer->resource;

          surface->texture =
            cogl_wayland_texture_2d_new_from_buffer (compositor->cogl_context,
                                                     buffer_resource,
                                                     &error);

          if (!surface->texture)
            {
              g_error ("Failed to create texture_2d from wayland buffer: %s",
                       error->message);
              cogl_error_free (error);
            }
        }
    }
  if (surface->pending.buffer)
    {
      wl_list_remove (&surface->pending.buffer_destroy_listener.link);
      surface->pending.buffer = NULL;
    }
  surface->pending.sx = 0;
  surface->pending.sy = 0;
  surface->pending.newly_attached = FALSE;

  /* wl_surface.damage */
  if (surface->buffer_ref.buffer &&
      surface->texture &&
      !region_is_empty (&surface->pending.damage))
    {
      CoglandRegion *region = &surface->pending.damage;
      CoglTexture *texture = COGL_TEXTURE (surface->texture);

      if (region->x2 > cogl_texture_get_width (texture))
        region->x2 = cogl_texture_get_width (texture);
      if (region->y2 > cogl_texture_get_height (texture))
        region->y2 = cogl_texture_get_height (texture);
      if (region->x1 < 0)
        region->x1 = 0;
      if (region->y1 < 0)
        region->y1 = 0;

      surface_damaged (surface,
                       region->x1,
                       region->y1,
                       region->x2 - region->x1,
                       region->y2 - region->y1);
    }
  region_init (&surface->pending.damage);

  /* wl_surface.frame */
  wl_list_insert_list (&compositor->frame_callbacks,
                       &surface->pending.frame_callback_list);
  wl_list_init (&surface->pending.frame_callback_list);
}

static void
cogland_surface_set_buffer_transform (struct wl_client *client,
                                      struct wl_resource *resource,
                                      int32_t transform)
{
}

const struct wl_surface_interface cogland_surface_interface = {
  cogland_surface_destroy,
  cogland_surface_attach,
  cogland_surface_damage,
  cogland_surface_frame,
  cogland_surface_set_opaque_region,
  cogland_surface_set_input_region,
  cogland_surface_commit,
  cogland_surface_set_buffer_transform
};

static void
cogland_surface_free (CoglandSurface *surface)
{
  CoglandCompositor *compositor = surface->compositor;
  CoglandFrameCallback *cb, *next;

  wl_signal_emit (&surface->destroy_signal, &surface->resource);

  compositor->surfaces = g_list_remove (compositor->surfaces, surface);

  cogland_buffer_reference (&surface->buffer_ref, NULL);
  if (surface->texture)
    cogl_object_unref (surface->texture);

  if (surface->pending.buffer)
    wl_list_remove (&surface->pending.buffer_destroy_listener.link);

  wl_list_for_each_safe (cb, next,
                         &surface->pending.frame_callback_list, link)
    wl_resource_destroy (cb->resource);

  g_slice_free (CoglandSurface, surface);

  cogland_queue_redraw (compositor);
}

static void
cogland_surface_resource_destroy_cb (struct wl_resource *resource)
{
  CoglandSurface *surface = resource->data;
  cogland_surface_free (surface);
}

static void
surface_handle_pending_buffer_destroy (struct wl_listener *listener,
                                       void *data)
{
  CoglandSurface *surface =
    wl_container_of (listener, surface, pending.buffer_destroy_listener);

  surface->pending.buffer = NULL;
}

static void
cogland_compositor_create_surface (struct wl_client *wayland_client,
                                   struct wl_resource *wayland_compositor_resource,
                                   uint32_t id)
{
  CoglandCompositor *compositor = wayland_compositor_resource->data;
  CoglandSurface *surface = g_slice_new0 (CoglandSurface);

  surface->compositor = compositor;

  wl_signal_init (&surface->destroy_signal);

  surface->resource = wl_client_add_object (wayland_client,
                                            &wl_surface_interface,
                                            &cogland_surface_interface,
                                            id,
                                            surface);
  wl_resource_set_destructor (surface->resource,
                              cogland_surface_resource_destroy_cb);

  surface->pending.buffer_destroy_listener.notify =
    surface_handle_pending_buffer_destroy;
  wl_list_init (&surface->pending.frame_callback_list);
  region_init (&surface->pending.damage);

  compositor->surfaces = g_list_prepend (compositor->surfaces,
                                         surface);
}

static void
cogland_region_destroy (struct wl_client *client,
			struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
cogland_region_add (struct wl_client *client,
		    struct wl_resource *resource,
		    int32_t x,
		    int32_t y,
		    int32_t width,
		    int32_t height)
{
  CoglandSharedRegion *shared_region = resource->data;

  region_add (&shared_region->region, x, y, width, height);
}

static void
cogland_region_subtract (struct wl_client *client,
			 struct wl_resource *resource,
			 int32_t x,
			 int32_t y,
			 int32_t width,
			 int32_t height)
{
  CoglandSharedRegion *shared_region = resource->data;

  region_subtract (&shared_region->region, x, y, width, height);
}

const struct wl_region_interface cogland_region_interface = {
  cogland_region_destroy,
  cogland_region_add,
  cogland_region_subtract
};

static void
cogland_region_resource_destroy_cb (struct wl_resource *resource)
{
  CoglandSharedRegion *region = resource->data;

  g_slice_free (CoglandSharedRegion, region);
}

static void
cogland_compositor_create_region (struct wl_client *wayland_client,
                                  struct wl_resource *compositor_resource,
                                  uint32_t id)
{
  CoglandSharedRegion *region = g_slice_new0 (CoglandSharedRegion);

  region->resource = wl_client_add_object (wayland_client,
                                           &wl_region_interface,
                                           &cogland_region_interface,
                                           id,
                                           region);
  wl_resource_set_destructor (region->resource,
                              cogland_region_resource_destroy_cb);

  region_init (&region->region);
}

static void
bind_output (struct wl_client *client,
             void *data,
             uint32_t version,
             uint32_t id)
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
dirty_cb (CoglOnscreen *onscreen,
          const CoglOnscreenDirtyInfo *info,
          void *user_data)
{
  CoglandCompositor *compositor = user_data;

  cogland_queue_redraw (compositor);
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
  CoglError *error = NULL;
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

  cogl_onscreen_add_dirty_callback (output->onscreen,
                                    dirty_cb,
                                    compositor,
                                    NULL /* destroy */);

  cogl_onscreen_show (output->onscreen);
  cogl_framebuffer_set_viewport (fb, -x, -y,
                                 compositor->virtual_width,
                                 compositor->virtual_height);

  mode = g_slice_new0 (CoglandMode);
  mode->flags = 0;
  mode->width = width_mm;
  mode->height = height_mm;
  mode->refresh = 60;

  output->modes = g_list_prepend (output->modes, mode);

  compositor->outputs = g_list_prepend (compositor->outputs, output);
}

const static struct wl_compositor_interface cogland_compositor_interface =
{
  cogland_compositor_create_surface,
  cogland_compositor_create_region
};

static void
compositor_bind (struct wl_client *client,
                 void *data,
                 uint32_t version,
                 uint32_t id)
{
  CoglandCompositor *compositor = data;

  wl_client_add_object (client, &wl_compositor_interface,
                        &cogland_compositor_interface, id, compositor);
}

static void
shell_surface_pong (struct wl_client *client,
                    struct wl_resource *resource,
                    uint32_t serial)
{
}

static void
shell_surface_move (struct wl_client *client,
                    struct wl_resource *resource,
                    struct wl_resource *seat,
                    uint32_t serial)
{
}

static void
shell_surface_resize (struct wl_client *client,
                      struct wl_resource *resource,
                      struct wl_resource *seat,
                      uint32_t serial,
                      uint32_t edges)
{
}

static void
shell_surface_set_toplevel (struct wl_client *client,
                            struct wl_resource *resource)
{
}

static void
shell_surface_set_transient (struct wl_client *client,
                             struct wl_resource *resource,
                             struct wl_resource *parent,
                             int32_t x,
                             int32_t y,
                             uint32_t flags)
{
}

static void
shell_surface_set_fullscreen (struct wl_client *client,
                              struct wl_resource *resource,
                              uint32_t method,
                              uint32_t framerate,
                              struct wl_resource *output)
{
}

static void
shell_surface_set_popup (struct wl_client *client,
                         struct wl_resource *resource,
                         struct wl_resource *seat,
                         uint32_t serial,
                         struct wl_resource *parent,
                         int32_t x,
                         int32_t y,
                         uint32_t flags)
{
}

static void
shell_surface_set_maximized (struct wl_client *client,
                             struct wl_resource *resource,
                             struct wl_resource *output)
{
}

static void
shell_surface_set_title (struct wl_client *client,
                         struct wl_resource *resource,
                         const char *title)
{
}

static void
shell_surface_set_class (struct wl_client *client,
                         struct wl_resource *resource,
                         const char *class_)
{
}

static const struct wl_shell_surface_interface cogl_shell_surface_interface =
{
  shell_surface_pong,
  shell_surface_move,
  shell_surface_resize,
  shell_surface_set_toplevel,
  shell_surface_set_transient,
  shell_surface_set_fullscreen,
  shell_surface_set_popup,
  shell_surface_set_maximized,
  shell_surface_set_title,
  shell_surface_set_class
};

static void
destroy_shell_surface (CoglandShellSurface *shell_surface)
{
  /* In case cleaning up a dead client destroys shell_surface first */
  if (shell_surface->surface)
    {
      wl_list_remove (&shell_surface->surface_destroy_listener.link);
      shell_surface->surface->has_shell_surface = FALSE;
    }

  g_free (shell_surface);
}

static void
destroy_shell_surface_cb (struct wl_resource *resource)
{
  destroy_shell_surface (resource->data);
}

static void
shell_handle_surface_destroy (struct wl_listener *listener,
                              void *data)
{
  CoglandShellSurface *shell_surface =
    wl_container_of (listener, shell_surface, surface_destroy_listener);

  shell_surface->surface->has_shell_surface = FALSE;
  shell_surface->surface = NULL;

  if (shell_surface->resource)
    wl_resource_destroy (shell_surface->resource);
  else
    destroy_shell_surface (shell_surface);
}

static void
get_shell_surface (struct wl_client *client,
                   struct wl_resource *resource,
                   uint32_t id,
                   struct wl_resource *surface_resource)
{
  CoglandSurface *surface = surface_resource->data;
  CoglandShellSurface *shell_surface;

  if (surface->has_shell_surface)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "wl_shell::get_shell_surface already requested");
      return;
    }

  shell_surface = g_new0 (CoglandShellSurface, 1);

  shell_surface->surface = surface;
  shell_surface->surface_destroy_listener.notify =
    shell_handle_surface_destroy;
  wl_signal_add (&surface->destroy_signal,
                 &shell_surface->surface_destroy_listener);

  surface->has_shell_surface = TRUE;

  shell_surface->resource = wl_client_add_object (client,
                                                  &wl_shell_surface_interface,
                                                  &cogl_shell_surface_interface,
                                                  id,
                                                  shell_surface);
  wl_resource_set_destructor (shell_surface->resource,
                              destroy_shell_surface_cb);
}

static const struct wl_shell_interface cogland_shell_interface =
{
  get_shell_surface
};

static void
bind_shell (struct wl_client *client,
            void *data,
            uint32_t version,
            uint32_t id)
{
  wl_client_add_object (client, &wl_shell_interface,
                        &cogland_shell_interface, id, data);
}

static CoglContext *
create_cogl_context (CoglandCompositor *compositor,
                     CoglBool use_egl_constraint,
                     CoglError **error)
{
  CoglRenderer *renderer = renderer = cogl_renderer_new ();
  CoglDisplay *display;
  CoglContext *context;

  if (use_egl_constraint)
    cogl_renderer_add_constraint (renderer, COGL_RENDERER_CONSTRAINT_USES_EGL);

  if (!cogl_renderer_connect (renderer, error))
    {
      cogl_object_unref (renderer);
      return NULL;
    }

  display = cogl_display_new (renderer, NULL);
  cogl_wayland_display_set_compositor_display (display,
                                               compositor->wayland_display);

  context = cogl_context_new (display, error);

  cogl_object_unref (renderer);
  cogl_object_unref (display);

  return context;
}

int
main (int argc, char **argv)
{
  CoglandCompositor compositor;
  GMainLoop *loop;
  CoglError *error = NULL;
  GError *gerror = NULL;
  CoglVertexP2C4 triangle_vertices[] = {
      {0, 0.7, 0xff, 0x00, 0x00, 0xff},
      {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
      {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
  };
  GSource *cogl_source;

  if (!process_arguments (&argc, &argv, &gerror))
    {
      fprintf (stderr, "%s\n", gerror->message);
      return EXIT_FAILURE;
    }

  memset (&compositor, 0, sizeof (compositor));

  compositor.wayland_display = wl_display_create ();
  if (compositor.wayland_display == NULL)
    g_error ("failed to create wayland display");

  wl_list_init (&compositor.frame_callbacks);

  if (!wl_display_add_global (compositor.wayland_display,
                              &wl_compositor_interface,
                              &compositor,
                              compositor_bind))
    g_error ("Failed to register wayland compositor object");

  wl_display_init_shm (compositor.wayland_display);

  loop = g_main_loop_new (NULL, FALSE);
  compositor.wayland_loop =
    wl_display_get_event_loop (compositor.wayland_display);
  compositor.wayland_event_source =
    wayland_event_source_new (compositor.wayland_display);
  g_source_attach (compositor.wayland_event_source, NULL);

  /* We want Cogl to use an EGL renderer because otherwise it won't
   * set up the wl_drm object and only SHM buffers will work. */
  compositor.cogl_context =
    create_cogl_context (&compositor,
                         TRUE /* use EGL constraint */,
                         &error);
  if (compositor.cogl_context == NULL)
    {
      /* If we couldn't get an EGL context then try any type of
       * context */
      cogl_error_free (error);
      error = NULL;

      compositor.cogl_context =
        create_cogl_context (&compositor,
                             FALSE, /* don't set EGL constraint */
                             &error);

      if (compositor.cogl_context)
        g_warning ("Failed to create context with EGL constraint, "
                   "falling back");
      else
        g_error ("Failed to create a Cogl context: %s\n", error->message);
    }

  compositor.virtual_width = 800;
  compositor.virtual_height = 600;

  if (option_multiple_outputs)
    {
      int hw = compositor.virtual_width / 2;
      int hh = compositor.virtual_height / 2;
      /* Emulate compositing with multiple monitors... */
      cogland_compositor_create_output (&compositor, 0, 0, hw, hh);
      cogland_compositor_create_output (&compositor, hw, 0, hw, hh);
      cogland_compositor_create_output (&compositor, 0, hh, hw, hh);
      cogland_compositor_create_output (&compositor, hw, hh, hw, hh);
    }
  else
    {
      cogland_compositor_create_output (&compositor,
                                        0, 0,
                                        compositor.virtual_width,
                                        compositor.virtual_height);
    }

  if (wl_display_add_global (compositor.wayland_display, &wl_shell_interface,
                             &compositor, bind_shell) == NULL)
    g_error ("Failed to register a global shell object");

  if (wl_display_add_socket (compositor.wayland_display, "wayland-0"))
    g_error ("Failed to create socket");

  compositor.triangle = cogl_primitive_new_p2c4 (compositor.cogl_context,
                                                 COGL_VERTICES_MODE_TRIANGLES,
                                                 3, triangle_vertices);
  compositor.triangle_pipeline = cogl_pipeline_new (compositor.cogl_context);

  cogl_source = cogl_glib_source_new (compositor.cogl_context,
                                      G_PRIORITY_DEFAULT);

  g_source_attach (cogl_source, NULL);

  g_main_loop_run (loop);

  return 0;
}
