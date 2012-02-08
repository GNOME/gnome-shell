#include <cogl/cogl.h>
#include <glib.h>
#include <stdio.h>
#include <SDL.h>

/* This short example is just to demonstrate mixing SDL with Cogl as a
   simple way to get portable support for events */

typedef struct Data
{
  CoglPrimitive *triangle;
  CoglPipeline *pipeline;
  float center_x, center_y;
  CoglFramebuffer *fb;
  gboolean quit;
} Data;

static void
redraw (Data *data)
{
  CoglFramebuffer *fb = data->fb;

  cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);

  cogl_framebuffer_push_matrix (fb);
  cogl_framebuffer_translate (fb, data->center_x, -data->center_y, 0.0f);

  cogl_framebuffer_draw_primitive (fb, data->pipeline, data->triangle);
  cogl_framebuffer_pop_matrix (fb);

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (fb));
}

static void
handle_event (Data *data, SDL_Event *event)
{
  switch (event->type)
    {
    case SDL_VIDEOEXPOSE:
      redraw (data);
      break;

    case SDL_MOUSEMOTION:
      {
        int width =
          cogl_framebuffer_get_width (COGL_FRAMEBUFFER (data->fb));
        int height =
          cogl_framebuffer_get_height (COGL_FRAMEBUFFER (data->fb));

        data->center_x = event->motion.x * 2.0f / width - 1.0f;
        data->center_y = event->motion.y * 2.0f / height - 1.0f;

        redraw (data);
      }
      break;

    case SDL_QUIT:
      data->quit = TRUE;
      break;
    }
}

static Uint32
timer_handler (Uint32 interval, void *user_data)
{
  static const SDL_UserEvent dummy_event =
    {
      SDL_USEREVENT
    };

  /* Post an event to wake up from SDL_WaitEvent */
  SDL_PushEvent ((SDL_Event *) &dummy_event);

  return 0;
}

static gboolean
wait_event_with_timeout (Data *data, SDL_Event *event, gint64 timeout)
{
  if (timeout == -1)
    {
      if (SDL_WaitEvent (event))
        return TRUE;
      else
        {
          data->quit = TRUE;
          return FALSE;
        }
    }
  else if (timeout == 0)
    return SDL_PollEvent (event);
  else
    {
      gboolean ret;
      /* Add a timer so that we can wake up the event loop */
      SDL_TimerID timer_id =
        SDL_AddTimer (timeout / 1000, timer_handler, data);

      if (SDL_WaitEvent (event))
        ret = TRUE;
      else
        {
          data->quit = TRUE;
          ret = FALSE;
        }

      SDL_RemoveTimer (timer_id);

      return ret;
    }
}

int
main (int argc, char **argv)
{
  CoglRenderer *renderer;
  CoglDisplay *display;
  CoglContext *ctx;
  CoglOnscreen *onscreen;
  GError *error = NULL;
  CoglVertexP2C4 triangle_vertices[] = {
    {0, 0.7, 0xff, 0x00, 0x00, 0x80},
    {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
    {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
  };
  Data data;
  SDL_Event event;

  /* Force the SDL winsys */
  renderer = cogl_renderer_new ();
  cogl_renderer_set_winsys_id (renderer, COGL_WINSYS_ID_SDL);
  display = cogl_display_new (renderer, NULL);
  ctx = cogl_context_new (display, &error);
  if (!ctx)
    {
      fprintf (stderr, "Failed to create context: %s\n", error->message);
      return 1;
    }

  SDL_InitSubSystem (SDL_INIT_TIMER);

  onscreen = cogl_onscreen_new (ctx, 800, 600);
  data.fb = COGL_FRAMEBUFFER (onscreen);

  data.center_x = 0.0f;
  data.center_y = 0.0f;
  data.quit = FALSE;

  cogl_onscreen_show (onscreen);

  data.triangle = cogl_primitive_new_p2c4 (ctx, COGL_VERTICES_MODE_TRIANGLES,
                                           3, triangle_vertices);
  data.pipeline = cogl_pipeline_new (ctx);
  while (!data.quit)
    {
      CoglPollFD *poll_fds;
      int n_poll_fds;
      gint64 timeout;

      cogl_poll_get_info (ctx, &poll_fds, &n_poll_fds, &timeout);

      /* It's difficult to wait for file descriptors using the SDL
         event mechanism, but it the SDL winsys is documented that it
         will never require this so we can assert that there are no
         fds */
      g_assert (n_poll_fds == 0);

      if (wait_event_with_timeout (&data, &event, timeout))
        do
          handle_event (&data, &event);
        while (SDL_PollEvent (&event));

      cogl_poll_dispatch (ctx, poll_fds, n_poll_fds);
    }

  cogl_object_unref (ctx);
  cogl_object_unref (display);
  cogl_object_unref (renderer);

  return 0;
}
