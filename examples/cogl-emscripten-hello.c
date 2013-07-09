#include <cogl/cogl.h>
#include <stdio.h>
#include <SDL.h>
#include <emscripten.h>
#include "emscripten-example-js.h"

/* This short example is just to demonstrate using Cogl with
 * Emscripten using SDL to receive input events */

typedef struct Data
{
  CoglPrimitive *triangle;
  CoglPipeline *pipeline;
  float center_x, center_y;
  CoglFramebuffer *fb;
  CoglBool redraw_queued;
  CoglBool ready_to_draw;
} Data;

static Data data;
static CoglContext *ctx;

static void
redraw (Data *data)
{
  CoglFramebuffer *fb = data->fb;

  cogl_framebuffer_clear4f (fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);

  cogl_framebuffer_push_matrix (fb);
  cogl_framebuffer_translate (fb, data->center_x, -data->center_y, 0.0f);

  cogl_primitive_draw (data->triangle, fb, data->pipeline);
  cogl_framebuffer_pop_matrix (fb);

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (fb));
}

static void
handle_event (Data *data, SDL_Event *event)
{
  switch (event->type)
    {
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

static void
mainloop (void)
{
  SDL_Event event;

  while (SDL_PollEvent (&event))
    {
      handle_event (&data, &event);
      cogl_sdl_handle_event (ctx, &event);
    }

  if (data.redraw_queued && data.ready_to_draw)
    {
      data.redraw_queued = FALSE;
      data.ready_to_draw = FALSE;
      redraw (&data);
    }

  /* NB: The mainloop will be automatically resumed if user input is received */
  if (!data.redraw_queued)
    emscripten_pause_main_loop ();

  cogl_sdl_idle (ctx);
}

int
main (int argc, char **argv)
{
  CoglOnscreen *onscreen;
  CoglError *error = NULL;
  CoglVertexP2C4 triangle_vertices[] = {
    {0, 0.7, 0xff, 0x00, 0x00, 0xff},
    {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
    {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
  };

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

  data.center_x = 0.0f;
  data.center_y = 0.0f;

  cogl_onscreen_show (onscreen);

  data.triangle = cogl_primitive_new_p2c4 (ctx, COGL_VERTICES_MODE_TRIANGLES,
                                           3, triangle_vertices);
  data.pipeline = cogl_pipeline_new (ctx);

  data.redraw_queued = TRUE;
  data.ready_to_draw = TRUE;

  /* The emscripten mainloop isn't event driven, it's periodic and so
   * we aim to pause the emscripten mainlooop whenever we don't have a
   * redraw queued. What we do instead is hook into the real browser
   * mainloop using this javascript binding api to add an input event
   * listener that will resume the emscripten mainloop whenever input
   * is received.
   */
  example_js_add_input_listener ();

  emscripten_set_main_loop (mainloop, -1, TRUE);

  cogl_object_unref (ctx);

  return 0;
}
