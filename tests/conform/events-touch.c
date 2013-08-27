/*
 * Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2012 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include <clutter/clutter.h>

#if defined CLUTTER_WINDOWING_X11 && OS_LINUX && HAVE_XINPUT_2_2

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <dlfcn.h>

#include <clutter/x11/clutter-x11.h>

#include "test-conform-common.h"

#define ABS_MAX_X 32768
#define ABS_MAX_Y 32768

#define TOUCH_POINTS 10

static ClutterPoint gesture_points[] = {
  { 100., 100. },
  { 110., 100. },
  { 120., 100. },
  { 130., 100. },
  { 140., 100. },
  { 150., 100. },
  { 160., 100. },
  { 170., 100. },
  { 180., 100. },
  { 190., 100. },
};

typedef struct _State State;

struct _State
{
  gboolean      pass;
  ClutterPoint  gesture_points_to_check[TOUCH_POINTS];
  int           gesture_points;
};

static int fd = -1;

static void send_event(int fd, int type, int code, int value, int sec, int usec)
{
    static int sec_offset = -1;
    static long last_time = -1;
    long newtime;
    struct input_event event;

    event.type  = type;
    event.code  = code;
    event.value = value;

    if (sec_offset == -1)
        sec_offset = sec;

    sec -= sec_offset;
    newtime = sec * 1000000 + usec;

    if (last_time > 0)
        usleep(newtime - last_time);

    gettimeofday(&event.time, NULL);

    if (write(fd, &event, sizeof(event)) < sizeof(event))
        perror("Send event failed.");

    last_time = newtime;
}

static gboolean
event_cb (ClutterActor *actor, ClutterEvent *event, State *state)
{
  int i;

  if (event->type != CLUTTER_TOUCH_BEGIN &&
      event->type != CLUTTER_TOUCH_UPDATE)
    return FALSE;

  state->gesture_points_to_check[state->gesture_points].x = ceil (event->touch.x);
  state->gesture_points_to_check[state->gesture_points].y = ceil (event->touch.y);
  state->gesture_points++;

  if (state->gesture_points == TOUCH_POINTS)
    {
      for (i = 0; i < TOUCH_POINTS; i++)
        {
          if (state->gesture_points_to_check[i].x != gesture_points[i].x ||
              state->gesture_points_to_check[i].y != gesture_points[i].y)
            {
              if (g_test_verbose ())
                g_print ("error: expected (%d, %d) but found (%d, %d) at position %d\n",
                         (int) gesture_points[i].x, (int) gesture_points[i].y,
                         (int) state->gesture_points_to_check[i].x,
                         (int) state->gesture_points_to_check[i].y,
                         i);
              state->pass = FALSE;
              break;
            }
        }

      clutter_main_quit ();
    }

  return TRUE;
}

static void
screen_coords_to_device (int screen_x, int screen_y,
                         int *device_x, int *device_y)
{
  int display_width = DisplayWidth (clutter_x11_get_default_display (),
                                    clutter_x11_get_default_screen ());
  int display_height = DisplayHeight (clutter_x11_get_default_display (),
                                      clutter_x11_get_default_screen ());

  *device_x = (screen_x * ABS_MAX_X) / display_width;
  *device_y = (screen_y * ABS_MAX_Y) / display_height;
}

static gboolean
perform_gesture (gpointer data)
{
  int i;

  for (i = 0; i < TOUCH_POINTS; i++)
    {
      int x = gesture_points[i].x;
      int y = gesture_points[i].y;

      screen_coords_to_device (x, y, &x, &y);

      send_event(fd, EV_ABS, ABS_MT_SLOT, 0, 1, i * 100);
      send_event(fd, EV_ABS, ABS_MT_TRACKING_ID, 1, 1, i * 100 + 10);

      send_event(fd, EV_ABS, ABS_MT_POSITION_X, x,  1, i * 100 + 20);
      send_event(fd, EV_ABS, ABS_MT_POSITION_Y, y, 1, i * 100 + 30);
      send_event(fd, EV_SYN, SYN_MT_REPORT, 0, 1, i * 100 + 40);
      send_event(fd, EV_SYN, SYN_REPORT, 0, 1, i * 100 + 50);
    }

  send_event(fd, EV_ABS, ABS_MT_TRACKING_ID, -1, 1, TOUCH_POINTS * 100 + 10);
  send_event(fd, EV_SYN, SYN_MT_REPORT, 0, 1, TOUCH_POINTS * 100 + 20);
  send_event(fd, EV_SYN, SYN_REPORT, 0, 1, TOUCH_POINTS * 100 + 30);

  return G_SOURCE_REMOVE;
}

static int
setup (struct uinput_user_dev *dev, int fd)
{
  strcpy (dev->name, "eGalax Touch Screen");
  dev->id.bustype = 0x18;
  dev->id.vendor = 0xeef;
  dev->id.product = 0x20;
  dev->id.version = 0x1;

  if (ioctl (fd, UI_SET_EVBIT, EV_SYN) == -1)
    goto error;

  if (ioctl (fd, UI_SET_EVBIT, EV_KEY) == -1)
    goto error;

  if (ioctl (fd, UI_SET_KEYBIT, BTN_TOUCH) == -1)
    goto error;

  if (ioctl (fd, UI_SET_EVBIT, EV_ABS) == -1)
    goto error;

  if (ioctl (fd, UI_SET_ABSBIT, ABS_X) == -1)
    goto error;
  else
    {
      int idx = ABS_X;
      dev->absmin[idx] = 0;
      dev->absmax[idx] = ABS_MAX_X;
      dev->absfuzz[idx] = 1;
      dev->absflat[idx] = 0;

      if (dev->absmin[idx] == dev->absmax[idx])
        dev->absmax[idx]++;
    }

  if (ioctl (fd, UI_SET_ABSBIT, ABS_Y) == -1)
    goto error;
  else
    {
      int idx = ABS_Y;
      dev->absmin[idx] = 0;
      dev->absmax[idx] = ABS_MAX_Y;
      dev->absfuzz[idx] = 1;
      dev->absflat[idx] = 0;

      if (dev->absmin[idx] == dev->absmax[idx])
        dev->absmax[idx]++;
    }

  if (ioctl (fd, UI_SET_ABSBIT, ABS_PRESSURE) == -1)
    goto error;
  else
    {
      int idx = ABS_PRESSURE;
      dev->absmin[idx] = 0;
      dev->absmax[idx] = 0;
      dev->absfuzz[idx] = 0;
      dev->absflat[idx] = 0;

      if (dev->absmin[idx] == dev->absmax[idx])
        dev->absmax[idx]++;
    }

  if (ioctl (fd, UI_SET_ABSBIT, ABS_MT_TOUCH_MAJOR) == -1)
    goto error;
  else
    {
      int idx = ABS_MT_TOUCH_MAJOR;
      dev->absmin[idx] = 0;
      dev->absmax[idx] = 255;
      dev->absfuzz[idx] = 1;
      dev->absflat[idx] = 0;

      if (dev->absmin[idx] == dev->absmax[idx])
        dev->absmax[idx]++;
    }

  if (ioctl (fd, UI_SET_ABSBIT, ABS_MT_WIDTH_MAJOR) == -1)
    goto error;
  else
    {
      int idx = ABS_MT_WIDTH_MAJOR;
      dev->absmin[idx] = 0;
      dev->absmax[idx] = 255;
      dev->absfuzz[idx] = 1;
      dev->absflat[idx] = 0;

      if (dev->absmin[idx] == dev->absmax[idx])
        dev->absmax[idx]++;
    }

  if (ioctl (fd, UI_SET_ABSBIT, ABS_MT_POSITION_X) == -1)
    goto error;
  else
    {
      int idx = ABS_MT_POSITION_X;
      dev->absmin[idx] = 0;
      dev->absmax[idx] = ABS_MAX_X;
      dev->absfuzz[idx] = 1;
      dev->absflat[idx] = 0;

      if (dev->absmin[idx] == dev->absmax[idx])
        dev->absmax[idx]++;
    }

  if (ioctl (fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y) == -1)
    goto error;
  else
    {
      int idx = ABS_MT_POSITION_Y;
      dev->absmin[idx] = 0;
      dev->absmax[idx] = ABS_MAX_Y;
      dev->absfuzz[idx] = 1;
      dev->absflat[idx] = 0;

      if (dev->absmin[idx] == dev->absmax[idx])
        dev->absmax[idx]++;
    }

  if (ioctl (fd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID) == -1)
    goto error;
  else
    {
      int idx = ABS_MT_TRACKING_ID;
      dev->absmin[idx] = 0;
      dev->absmax[idx] = 5;
      dev->absfuzz[idx] = 0;
      dev->absflat[idx] = 0;

      if (dev->absmin[idx] == dev->absmax[idx])
        dev->absmax[idx]++;
    }



  return 0;
error:
  perror ("ioctl failed.");
  return -1;
}

static int
init_uinput (void)
{
  struct uinput_user_dev dev;

  fd = open ("/dev/uinput", O_RDWR);
  if (fd < 0 && errno == ENODEV)
    fd = open ("/dev/input/uinput", O_RDWR);
  if (fd < 0)
    {
      if (g_test_verbose ())
        perror ("open");

      return 0;
    };

  memset (&dev, 0, sizeof (dev));
  setup (&dev, fd);

  if (write (fd, &dev, sizeof (dev)) < sizeof (dev))
    {
      if (g_test_verbose ())
        perror ("write");

      goto error;
    }

  if (ioctl (fd, UI_DEV_CREATE, NULL) == -1)
    {
      if (g_test_verbose ())
        perror ("ioctl");

      goto error;
    }

  return 1;

error:
  if (fd != -1)
    close (fd);

  return 0;
}

#endif /* defined CLUTTER_WINDOWING_X11 && OS_LINUX && HAVE_XINPUT_2_2 */

void
events_touch (void)
{
#if defined CLUTTER_WINDOWING_X11 && OS_LINUX && HAVE_XINPUT_2_2
  ClutterActor *stage;
  State state;

  /* bail out if we could not initialize evdev */
  if (!init_uinput ())
    return;

  state.pass = TRUE;
  state.gesture_points = 0;

  stage = clutter_stage_new ();
  g_signal_connect (stage, "event", G_CALLBACK (event_cb), &state);
  clutter_stage_set_fullscreen (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_show (stage);

  clutter_threads_add_timeout (500, perform_gesture, &state);

  clutter_main ();

  if (g_test_verbose ())
    g_print ("end result: %s\n", state.pass ? "pass" : "FAIL");

  g_assert (state.pass);

  clutter_actor_destroy (stage);
#endif /* defined CLUTTER_WINDOWING_X11 && OS_LINUX && HAVE_XINPUT_2_2 */
}
