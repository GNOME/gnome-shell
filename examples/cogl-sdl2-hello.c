#include <cogl/cogl.h>
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
  CoglBool quit;
  CoglBool redraw_queued;
  CoglBool ready_to_draw;
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
dirty_cb (CoglOnscreen *onscreen,
          const CoglOnscreenDirtyInfo *info,
          void *user_data)
{
  Data *data = user_data;

  data->redraw_queued = TRUE;
}

static void
handle_event (Data *data, SDL_Event *event)
{
  switch (event->type)
    {
    case SDL_WINDOWEVENT:
      switch (event->window.event)
        {
        case SDL_WINDOWEVENT_CLOSE:
          data->quit = TRUE;
          break;
        }
      break;

    case SDL_MOUSEMOTION:
      {
        int width =
          cogl_framebuffer_get_width (COGL_FRAMEBUFFER (data->fb));
        int height =
          cogl_framebuffer_get_height (COGL_FRAMEBUFFER (data->fb));

        data->center_x = event->motion.x * 2.0f / width - 1.0f;
        data->center_y = event->motion.y * 2.0f / height - 1.0f;

        data->redraw_queued = TRUE;
      }
      break;

    case SDL_QUIT:
      data->quit = TRUE;
      break;
    }
}

static void
frame_cb (CoglOnscreen *onscreen,
          CoglFrameEvent event,
          CoglFrameInfo *info,
          void *user_data)
{
  Data *data = user_data;

  if (event == COGL_FRAME_EVENT_SYNC)
    data->ready_to_draw = TRUE;
}

int
main (int argc, char **argv)
{
  CoglContext *ctx;
  CoglOnscreen *onscreen;
  CoglError *error = NULL;
  CoglVertexP2C4 triangle_vertices[] = {
    {0, 0.7, 0xff, 0x00, 0x00, 0xff},
    {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
    {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
  };
  Data data;
  SDL_Event event;

  ctx = cogl_sdl_context_new (SDL_USEREVENT, &error);
  if (!ctx)
    {
      fprintf (stderr, "Failed to create context: %s\n", error->message);
      return 1;
    }

  onscreen = cogl_onscreen_new (ctx, 800, 600);
  data.fb = COGL_FRAMEBUFFER (onscreen);

  cogl_onscreen_add_frame_callback (onscreen,
                                    frame_cb,
                                    &data,
                                    NULL /* destroy callback */);
  cogl_onscreen_add_dirty_callback (onscreen,
                                    dirty_cb,
                                    &data,
                                    NULL /* destroy callback */);

  data.center_x = 0.0f;
  data.center_y = 0.0f;
  data.quit = FALSE;

  /* In SDL2, setting resizable only works before allocating the
   * onscreen */
  cogl_onscreen_set_resizable (onscreen, TRUE);

  cogl_onscreen_show (onscreen);

  data.triangle = cogl_primitive_new_p2c4 (ctx, COGL_VERTICES_MODE_TRIANGLES,
                                           3, triangle_vertices);
  data.pipeline = cogl_pipeline_new (ctx);

  data.redraw_queued = FALSE;
  data.ready_to_draw = TRUE;

  while (!data.quit)
    {
      if (!SDL_PollEvent (&event))
        {
          if (data.redraw_queued && data.ready_to_draw)
            {
              redraw (&data);
              data.redraw_queued = FALSE;
              data.ready_to_draw = FALSE;
              continue;
            }

          cogl_sdl_idle (ctx);
          if (!SDL_WaitEvent (&event))
            {
              fprintf (stderr, "Error waiting for SDL events");
              return 1;
            }
        }

      handle_event (&data, &event);
      cogl_sdl_handle_event (ctx, &event);
    }

  cogl_object_unref (ctx);

  return 0;
}
