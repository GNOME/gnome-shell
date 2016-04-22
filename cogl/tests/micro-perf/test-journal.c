#include <glib.h>
#include <cogl/cogl2-experimental.h>
#include <math.h>

#include "cogl/cogl-profile.h"

#define FRAMEBUFFER_WIDTH 800
#define FRAMEBUFFER_HEIGHT 600

CoglBool run_all = FALSE;

typedef struct _Data
{
  CoglContext *ctx;
  CoglFramebuffer *fb;
  CoglPipeline *pipeline;
  CoglPipeline *alpha_pipeline;
  GTimer *timer;
  int frame;
} Data;

static void
test_rectangles (Data *data)
{
#define RECT_WIDTH 5
#define RECT_HEIGHT 5
  int x;
  int y;

  cogl_framebuffer_clear4f (data->fb, COGL_BUFFER_BIT_COLOR, 1, 1, 1, 1);

  cogl_framebuffer_push_rectangle_clip (data->fb,
                                        10,
                                        10,
                                        FRAMEBUFFER_WIDTH - 10,
                                        FRAMEBUFFER_HEIGHT - 10);

  /* Should the rectangles be randomly positioned/colored/rotated?
   *
   * It could be good to develop equivalent GL and Cairo tests so we can
   * have a sanity check for our Cogl performance.
   *
   * The color should vary to check that we correctly batch color changes
   * The use of alpha should vary so we have a variation of which rectangles
   * require blending.
   *  Should this be a random variation?
   *  It could be good to experiment with focibly enabling blending for
   *  rectangles that don't technically need it for the sake of extending
   *  batching. E.g. if you a long run of interleved rectangles with every
   *  other rectangle needing blending then it may be worth enabling blending
   *  for all the rectangles to avoid the state changes.
   * The modelview should change between rectangles to check the software
   * transform codepath.
   *  Should we group some rectangles under the same modelview? Potentially
   *  we could avoid software transform for long runs of rectangles with the
   *  same modelview.
   *
   */
  for (y = 0; y < FRAMEBUFFER_HEIGHT; y += RECT_HEIGHT)
    {
      for (x = 0; x < FRAMEBUFFER_WIDTH; x += RECT_WIDTH)
        {
          cogl_framebuffer_push_matrix (data->fb);
          cogl_framebuffer_translate (data->fb, x, y, 0);
          cogl_framebuffer_rotate (data->fb, 45, 0, 0, 1);

          cogl_pipeline_set_color4f (data->pipeline,
                                     1,
                                     (1.0f/FRAMEBUFFER_WIDTH)*y,
                                     (1.0f/FRAMEBUFFER_HEIGHT)*x,
                                     1);
          cogl_framebuffer_draw_rectangle (data->fb,
                                           data->pipeline,
                                           0, 0, RECT_WIDTH, RECT_HEIGHT);

          cogl_framebuffer_pop_matrix (data->fb);
        }
    }

  for (y = 0; y < FRAMEBUFFER_HEIGHT; y += RECT_HEIGHT)
    {
      for (x = 0; x < FRAMEBUFFER_WIDTH; x += RECT_WIDTH)
        {
          cogl_framebuffer_push_matrix (data->fb);
          cogl_framebuffer_translate (data->fb, x, y, 0);

          cogl_pipeline_set_color4f (data->alpha_pipeline,
                                     1,
                                     (1.0f/FRAMEBUFFER_WIDTH)*x,
                                     (1.0f/FRAMEBUFFER_HEIGHT)*y,
                                     (1.0f/FRAMEBUFFER_WIDTH)*x);
          cogl_framebuffer_draw_rectangle (data->fb,
                                           data->alpha_pipeline,
                                           0, 0, RECT_WIDTH, RECT_HEIGHT);

          cogl_framebuffer_pop_matrix (data->fb);
        }
    }

  cogl_framebuffer_pop_clip (data->fb);
}

static CoglBool
paint_cb (void *user_data)
{
  Data *data = user_data;
  double elapsed;

  data->frame++;

  test_rectangles (data);

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (data->fb));

  elapsed = g_timer_elapsed (data->timer, NULL);
  if (elapsed > 1.0)
    {
      g_print ("fps = %f\n", data->frame / elapsed);
      g_timer_start (data->timer);
      data->frame = 0;
    }

  return FALSE; /* remove the callback */
}

static void
frame_event_cb (CoglOnscreen *onscreen,
                CoglFrameEvent event,
                CoglFrameInfo *info,
                void *user_data)
{
  if (event == COGL_FRAME_EVENT_SYNC)
    paint_cb (user_data);
}

int
main (int argc, char **argv)
{
  Data data;
  CoglOnscreen *onscreen;
  GSource *cogl_source;
  GMainLoop *loop;
  COGL_STATIC_TIMER (mainloop_timer,
                      NULL, //no parent
                      "Mainloop",
                      "The time spent in the glib mainloop",
                      0);  // no application private data

  data.ctx = cogl_context_new (NULL, NULL);

  onscreen = cogl_onscreen_new (data.ctx,
                                FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);
  cogl_onscreen_set_swap_throttled (onscreen, FALSE);
  cogl_onscreen_show (onscreen);

  data.fb = onscreen;
  cogl_framebuffer_orthographic (data.fb,
                                 0, 0,
                                 FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT,
                                 -1,
                                 100);

  data.pipeline = cogl_pipeline_new (data.ctx);
  cogl_pipeline_set_color4f (data.pipeline, 1, 1, 1, 1);
  data.alpha_pipeline = cogl_pipeline_new (data.ctx);
  cogl_pipeline_set_color4f (data.alpha_pipeline, 1, 1, 1, 0.5);

  cogl_source = cogl_glib_source_new (data.ctx, G_PRIORITY_DEFAULT);

  g_source_attach (cogl_source, NULL);

  cogl_onscreen_add_frame_callback (COGL_ONSCREEN (data.fb),
                                    frame_event_cb,
                                    &data,
                                    NULL); /* destroy notify */

  g_idle_add (paint_cb, &data);

  data.frame = 0;
  data.timer = g_timer_new ();
  g_timer_start (data.timer);

  loop = g_main_loop_new (NULL, TRUE);
  COGL_TIMER_START (uprof_get_mainloop_context (), mainloop_timer);
  g_main_loop_run (loop);
  COGL_TIMER_STOP (uprof_get_mainloop_context (), mainloop_timer);

  return 0;
}

