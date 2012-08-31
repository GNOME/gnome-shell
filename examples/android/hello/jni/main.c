/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2011 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/*
 * This file is derived from the "native-activity" sample of the android NDK
 * r5b. The coding style has been adapted to the code style most commonly found
 * in glib/gobject based projects.
 */

#include <android_native_app_glue.h>

#include <glib.h>
#include <glib-android/glib-android.h>
#include <cogl/cogl.h>

typedef struct
{
  struct android_app* app;

  CoglContext *context;
  CoglPrimitive *triangle;
  CoglFramebuffer *fb;
} TestData;

static int test_init (TestData* data)
{
  CoglOnscreen *onscreen;
  CoglError *error = NULL;
  CoglVertexP2C4 triangle_vertices[] = {
        {0, 0.7, 0xff, 0x00, 0x00, 0x80},
        {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
  };

  cogl_android_set_native_window (data->app->window);

  data->context = cogl_context_new (NULL, &error);
  if (!data->context)
    {
      g_critical ("Failed to create context: %s\n", error->message);
      return 1;
    }

  onscreen = cogl_onscreen_new (data->context, 320, 420);

  /* Eventually there will be an implicit allocate on first use so this
   * will become optional... */
  data->fb = COGL_FRAMEBUFFER (onscreen);
  if (!cogl_framebuffer_allocate (data->fb, &error))
    {
      if (error)
        g_critical ("Failed to allocate framebuffer: %s\n", error->message);
      else
        g_critical ("Failed to allocate framebuffer");
      return 1;
    }

  cogl_onscreen_show (onscreen);

  cogl_push_framebuffer (data->fb);

  data->triangle = cogl_primitive_new_p2c4 (COGL_VERTICES_MODE_TRIANGLES,
                                            3, triangle_vertices);

  return 0;
}

static test_draw_frame_and_swap (TestData *data)
{
  if (data->context)
    {
      cogl_primitive_draw (data->triangle);
      cogl_framebuffer_swap_buffers (data->fb);
    }
}

static void
test_fini (TestData *data)
{
  if (data->fb)
    {
      cogl_object_unref (data->triangle);
      cogl_object_unref (data->fb);
      cogl_object_unref (data->context);
      data->triangle = NULL;
      data->fb = NULL;
      data->context = NULL;
    }
}

/**
 * Process the next main command.
 */
static void
test_handle_cmd (struct android_app* app,
                 int32_t             cmd)
{
  TestData *data = (TestData *) app->userData;

  switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
      /* The window is being shown, get it ready */
      g_message ("command: INIT_WINDOW");
      if (data->app->window != NULL)
        {
          test_init (data);
          test_draw_frame_and_swap (data);
        }
      break;

    case APP_CMD_TERM_WINDOW:
      /* The window is being hidden or closed, clean it up */
      g_message ("command: TERM_WINDOW");
      test_fini (data);
      break;

    case APP_CMD_GAINED_FOCUS:
      g_message ("command: GAINED_FOCUS");
      break;

    case APP_CMD_LOST_FOCUS:
      /* When our app loses focus, we stop monitoring the accelerometer.
       * This is to avoid consuming battery while not being used. */
      g_message ("command: LOST_FOCUS");
      test_draw_frame_and_swap (data);
      break;
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void
android_main (struct android_app* application)
{
  TestData data;

  /* Make sure glue isn't stripped */
  app_dummy ();

  g_android_init ();

  memset (&data, 0, sizeof (TestData));
  application->userData = &data;
  application->onAppCmd = test_handle_cmd;
  data.app = application;

  while (1)
    {
      int events;
      struct android_poll_source* source;

      while ((ALooper_pollAll (0, NULL, &events, (void**)&source)) >= 0)
        {

          /* Process this event */
          if (source != NULL)
            source->process (application, source);

          /* Check if we are exiting */
          if  (application->destroyRequested != 0)
            {
              test_fini (&data);
              return;
            }
      }

      test_draw_frame_and_swap (&data);
    }
}
