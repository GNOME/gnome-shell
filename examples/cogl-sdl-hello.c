#include <cogl/cogl.h>
#include <glib.h>
#include <stdio.h>
#include <SDL.h>

/* This short example is just to demonstrate mixing SDL with Cogl as a
   simple way to get portable support for events */

typedef struct Data
{
  CoglColor black;
  CoglPrimitive *triangle;
  float center_x, center_y;
  CoglFramebuffer *fb;
} Data;

static void
redraw (Data *data)
{
  cogl_clear (&data->black, COGL_BUFFER_BIT_COLOR);

  cogl_push_matrix ();
  cogl_translate (data->center_x, -data->center_y, 0.0f);

  cogl_primitive_draw (data->triangle);
  cogl_pop_matrix ();

  cogl_framebuffer_swap_buffers (data->fb);
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

  onscreen = cogl_onscreen_new (ctx, 800, 600);
  /* Eventually there will be an implicit allocate on first use so this
   * will become optional... */
  data.fb = COGL_FRAMEBUFFER (onscreen);
  if (!cogl_framebuffer_allocate (data.fb, &error))
    {
      fprintf (stderr, "Failed to allocate framebuffer: %s\n",
               error->message);
      return 1;
    }

  cogl_color_set_from_4ub (&data.black, 0, 0, 0, 255);
  data.center_x = 0.0f;
  data.center_y = 0.0f;

  cogl_onscreen_show (onscreen);

  cogl_push_framebuffer (data.fb);

  data.triangle = cogl_primitive_new_p2c4 (COGL_VERTICES_MODE_TRIANGLES,
                                           3, triangle_vertices);
  while (SDL_WaitEvent (&event))
    {
      switch (event.type)
        {
        case SDL_VIDEOEXPOSE:
          redraw (&data);
          break;

        case SDL_MOUSEMOTION:
          {
            int width =
              cogl_framebuffer_get_width (COGL_FRAMEBUFFER (data.fb));
            int height =
              cogl_framebuffer_get_height (COGL_FRAMEBUFFER (data.fb));

            data.center_x = event.motion.x * 2.0f / width - 1.0f;
            data.center_y = event.motion.y * 2.0f / height - 1.0f;

            redraw (&data);
          }
          break;

        case SDL_QUIT:
          goto done;
        }
    }

  fprintf (stderr, "Error waiting for an event: %s\n",
           SDL_GetError ());

 done:

  cogl_pop_framebuffer ();

  cogl_object_unref (ctx);
  cogl_object_unref (display);
  cogl_object_unref (renderer);

  return 0;
}
