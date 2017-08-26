/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014-2017 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "tests/test-utils.h"

#include <gio/gio.h>
#include <string.h>

#include "core/display-private.h"
#include "core/window-private.h"
#include "wayland/meta-wayland.h"
#include "x11/meta-x11-display-private.h"

struct _TestClient {
  char *id;
  MetaWindowClientType type;
  GSubprocess *subprocess;
  GCancellable *cancellable;
  GMainLoop *loop;
  GDataOutputStream *in;
  GDataInputStream *out;

  char *line;
  GError **error;

  AsyncWaiter *waiter;
};

struct _AsyncWaiter {
  XSyncCounter counter;
  int counter_value;
  XSyncAlarm alarm;

  GMainLoop *loop;
  int counter_wait_value;
};

G_DEFINE_QUARK (test-runner-error-quark, test_runner_error)

static char *test_client_path;

void
test_init (int    argc,
           char **argv)
{
  char *basename = g_path_get_basename (argv[0]);
  char *dirname = g_path_get_dirname (argv[0]);

  if (g_str_has_prefix (basename, "lt-"))
    test_client_path = g_build_filename (dirname,
                                         "../mutter-test-client", NULL);
  else
    test_client_path = g_build_filename (dirname,
                                         "mutter-test-client", NULL);
  g_free (basename);
  g_free (dirname);
}

AsyncWaiter *
async_waiter_new (void)
{
  AsyncWaiter *waiter = g_new0 (AsyncWaiter, 1);

  MetaDisplay *display = meta_get_display ();
  Display *xdisplay = display->x11_display->xdisplay;
  XSyncValue value;
  XSyncAlarmAttributes attr;

  waiter->counter_value = 0;
  XSyncIntToValue (&value, waiter->counter_value);

  waiter->counter = XSyncCreateCounter (xdisplay, value);

  attr.trigger.counter = waiter->counter;
  attr.trigger.test_type = XSyncPositiveComparison;

  /* Initialize to one greater than the current value */
  attr.trigger.value_type = XSyncRelative;
  XSyncIntToValue (&attr.trigger.wait_value, 1);

  /* After triggering, increment test_value by this until
   * until the test condition is false */
  XSyncIntToValue (&attr.delta, 1);

  /* we want events (on by default anyway) */
  attr.events = True;

  waiter->alarm = XSyncCreateAlarm (xdisplay,
                                    XSyncCACounter |
                                    XSyncCAValueType |
                                    XSyncCAValue |
                                    XSyncCATestType |
                                    XSyncCADelta |
                                    XSyncCAEvents,
                                    &attr);

  waiter->loop = g_main_loop_new (NULL, FALSE);

  return waiter;
}

void
async_waiter_destroy (AsyncWaiter *waiter)
{
  MetaDisplay *display = meta_get_display ();
  Display *xdisplay = display->x11_display->xdisplay;

  XSyncDestroyAlarm (xdisplay, waiter->alarm);
  XSyncDestroyCounter (xdisplay, waiter->counter);
  g_main_loop_unref (waiter->loop);
}

static int
async_waiter_next_value (AsyncWaiter *waiter)
{
  return waiter->counter_value + 1;
}

static void
async_waiter_wait (AsyncWaiter *waiter,
                   int          wait_value)
{
  if (waiter->counter_value < wait_value)
    {
      waiter->counter_wait_value = wait_value;
      g_main_loop_run (waiter->loop);
      waiter->counter_wait_value = 0;
    }
}

void
async_waiter_set_and_wait (AsyncWaiter *waiter)
{
  MetaDisplay *display = meta_get_display ();
  Display *xdisplay = display->x11_display->xdisplay;
  int wait_value = async_waiter_next_value (waiter);

  XSyncValue sync_value;
  XSyncIntToValue (&sync_value, wait_value);

  XSyncSetCounter (xdisplay, waiter->counter, sync_value);
  async_waiter_wait (waiter, wait_value);
}

gboolean
async_waiter_alarm_filter (AsyncWaiter           *waiter,
                           MetaDisplay           *display,
                           XSyncAlarmNotifyEvent *event)
{
  if (event->alarm != waiter->alarm)
    return FALSE;

  waiter->counter_value = XSyncValueLow32 (event->counter_value);

  if (waiter->counter_wait_value != 0 &&
      waiter->counter_value >= waiter->counter_wait_value)
    g_main_loop_quit (waiter->loop);

  return TRUE;
}

char *
test_client_get_id (TestClient *client)
{
  return client->id;
}

static void
test_client_line_read (GObject      *source,
                       GAsyncResult *result,
                       gpointer      data)
{
  TestClient *client = data;

  client->line = g_data_input_stream_read_line_finish_utf8 (client->out,
                                                            result,
                                                            NULL,
                                                            client->error);
  g_main_loop_quit (client->loop);
}

gboolean
test_client_do (TestClient *client,
                GError    **error,
                ...)
{
  GString *command = g_string_new (NULL);
  char *line = NULL;
  va_list vap;

  va_start (vap, error);

  while (TRUE)
    {
      char *word = va_arg (vap, char *);
      char *quoted;

      if (word == NULL)
        break;

      if (command->len > 0)
        g_string_append_c (command, ' ');

      quoted = g_shell_quote (word);
      g_string_append (command, quoted);
      g_free (quoted);
    }

  va_end (vap);

  g_string_append_c (command, '\n');

  if (!g_data_output_stream_put_string (client->in, command->str,
                                        client->cancellable, error))
    goto out;

  g_data_input_stream_read_line_async (client->out,
                                       G_PRIORITY_DEFAULT,
                                       client->cancellable,
                                       test_client_line_read,
                                       client);

  client->error = error;
  g_main_loop_run (client->loop);
  line = client->line;
  client->line = NULL;
  client->error = NULL;

  if (!line)
    {
      if (*error == NULL)
        g_set_error (error, TEST_RUNNER_ERROR, TEST_RUNNER_ERROR_RUNTIME_ERROR,
                     "test client exited");
      goto out;
    }

  if (strcmp (line, "OK") != 0)
    {
      g_set_error (error, TEST_RUNNER_ERROR, TEST_RUNNER_ERROR_RUNTIME_ERROR,
                   "%s", line);
      goto out;
    }

 out:
  g_string_free (command, TRUE);
  g_free (line);

  return *error == NULL;
}

gboolean
test_client_wait (TestClient *client,
                  GError    **error)
{
  if (client->type == META_WINDOW_CLIENT_TYPE_WAYLAND)
    {
      return test_client_do (client, error, "sync", NULL);
    }
  else
    {
      int wait_value = async_waiter_next_value (client->waiter);
      char *counter_str = g_strdup_printf ("%lu", client->waiter->counter);
      char *wait_value_str = g_strdup_printf ("%d", wait_value);
      gboolean success;

      success = test_client_do (client, error,
                                "set_counter", counter_str, wait_value_str,
                                NULL);
      g_free (counter_str);
      g_free (wait_value_str);
      if (!success)
        return FALSE;

      async_waiter_wait (client->waiter, wait_value);
      return TRUE;
    }
}

MetaWindow *
test_client_find_window (TestClient *client,
                         const char *window_id,
                         GError    **error)
{
  MetaDisplay *display = meta_get_display ();
  GSList *windows;
  GSList *l;
  MetaWindow *result;
  char *expected_title;

  windows =
    meta_display_list_windows (display,
                               META_LIST_INCLUDE_OVERRIDE_REDIRECT);

  expected_title = g_strdup_printf ("test/%s/%s", client->id, window_id);

  result = NULL;
  for (l = windows; l; l = l->next)
    {
      MetaWindow *window = l->data;

      if (g_strcmp0 (window->title, expected_title) == 0)
        {
          result = window;
          break;
        }
    }

  g_slist_free (windows);
  g_free (expected_title);

  if (result == NULL)
    g_set_error (error, TEST_RUNNER_ERROR, TEST_RUNNER_ERROR_RUNTIME_ERROR,
                 "window %s/%s isn't known to Mutter", client->id, window_id);

  return result;
}

gboolean
test_client_alarm_filter (TestClient            *client,
                          MetaDisplay           *display,
                          XSyncAlarmNotifyEvent *event)
{
  if (client->waiter)
    return async_waiter_alarm_filter (client->waiter, display, event);
  else
    return FALSE;
}

TestClient *
test_client_new (const char          *id,
                 MetaWindowClientType type,
                 GError             **error)
{
  TestClient *client = g_new0 (TestClient, 1);
  GSubprocessLauncher *launcher;
  GSubprocess *subprocess;
  MetaWaylandCompositor *compositor;
  const char *wayland_display_name;
  const char *x11_display_name;

  launcher =  g_subprocess_launcher_new ((G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                          G_SUBPROCESS_FLAGS_STDOUT_PIPE));

  g_assert (meta_is_wayland_compositor ());
  compositor = meta_wayland_compositor_get_default ();
  wayland_display_name = meta_wayland_get_wayland_display_name (compositor);
  x11_display_name = meta_wayland_get_xwayland_display_name (compositor);

  g_subprocess_launcher_setenv (launcher,
                                "WAYLAND_DISPLAY", wayland_display_name,
                                TRUE);
  g_subprocess_launcher_setenv (launcher,
                                "DISPLAY", x11_display_name,
                                TRUE);

  subprocess = g_subprocess_launcher_spawn (launcher,
                                            error,
                                            test_client_path,
                                            "--client-id",
                                            id,
                                            (type == META_WINDOW_CLIENT_TYPE_WAYLAND ?
                                             "--wayland" : NULL),
                                            NULL);
  g_object_unref (launcher);

  if (!subprocess)
    return NULL;

  client->type = type;
  client->id = g_strdup (id);
  client->cancellable = g_cancellable_new ();
  client->subprocess = subprocess;
  client->in =
    g_data_output_stream_new (g_subprocess_get_stdin_pipe (subprocess));
  client->out =
    g_data_input_stream_new (g_subprocess_get_stdout_pipe (subprocess));
  client->loop = g_main_loop_new (NULL, FALSE);

  if (client->type == META_WINDOW_CLIENT_TYPE_X11)
    client->waiter = async_waiter_new ();

  return client;
}

gboolean
test_client_quit (TestClient *client,
                  GError    **error)
{
  if (!test_client_do (client, error, "destroy_all", NULL))
    return FALSE;

  if (!test_client_wait (client, error))
    return FALSE;

  return TRUE;
}

void
test_client_destroy (TestClient *client)
{
  GError *error = NULL;

  if (client->waiter)
    async_waiter_destroy (client->waiter);

  g_output_stream_close (G_OUTPUT_STREAM (client->in), NULL, &error);
  if (error)
    {
      g_warning ("Error closing client stdin: %s", error->message);
      g_clear_error (&error);
    }
  g_object_unref (client->in);

  g_input_stream_close (G_INPUT_STREAM (client->out), NULL, &error);
  if (error)
    {
      g_warning ("Error closing client stdout: %s", error->message);
      g_clear_error (&error);
    }
  g_object_unref (client->out);

  g_object_unref (client->cancellable);
  g_object_unref (client->subprocess);
  g_main_loop_unref (client->loop);
  g_free (client->id);
  g_free (client);
}
