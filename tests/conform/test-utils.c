#include <cogl/cogl.h>
#include <stdlib.h>

#include "test-utils.h"

#define FB_WIDTH 512
#define FB_HEIGHT 512

void
test_utils_init (TestUtilsGTestFixture *fixture,
                 const void *data)
{
  TestUtilsSharedState *state = (TestUtilsSharedState *)data;
  static int counter = 0;
  GError *error = NULL;
  CoglOnscreen *onscreen = NULL;

  if (counter != 0)
    g_critical ("We don't support running more than one test at a time\n"
                "in a single test run due to the state leakage that can\n"
                "cause subsequent tests to fail.\n"
                "\n"
                "If you want to run all the tests you should run\n"
                "$ make test-report");
  counter++;

  g_setenv ("COGL_X11_SYNC", "1", 0);

  state->ctx = cogl_context_new (NULL, &error);
  if (!state->ctx)
    g_critical ("Failed to create a CoglContext: %s", error->message);

  if (getenv  ("COGL_TEST_ONSCREEN"))
    {
      onscreen = cogl_onscreen_new (state->ctx, 640, 480);
      state->fb = COGL_FRAMEBUFFER (onscreen);
    }
  else
    {
      CoglHandle offscreen;
      CoglHandle tex = cogl_texture_2d_new_with_size (state->ctx,
                                                      FB_WIDTH, FB_HEIGHT,
                                                      COGL_PIXEL_FORMAT_ANY,
                                                      &error);
      if (!tex)
        g_critical ("Failed to allocate texture: %s", error->message);

      offscreen = cogl_offscreen_new_to_texture (tex);
      state->fb = COGL_FRAMEBUFFER (offscreen);
    }

  if (!cogl_framebuffer_allocate (state->fb, &error))
    g_critical ("Failed to allocate framebuffer: %s", error->message);

  if (onscreen)
    cogl_onscreen_show (onscreen);

  cogl_framebuffer_clear4f (state->fb,
                            COGL_BUFFER_BIT_COLOR |
                            COGL_BUFFER_BIT_DEPTH |
                            COGL_BUFFER_BIT_STENCIL,
                            0, 0, 0, 1);

  cogl_push_framebuffer (state->fb);
}

void
test_utils_fini (TestUtilsGTestFixture *fixture,
                 const void *data)
{
  const TestUtilsSharedState *state = (TestUtilsSharedState *)data;

  cogl_pop_framebuffer ();

  if (state->fb)
    cogl_object_unref (state->fb);

  if (state->ctx)
    cogl_object_unref (state->ctx);
}
