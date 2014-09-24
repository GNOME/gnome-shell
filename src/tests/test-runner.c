/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat, Inc.
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

#include <gio/gio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <meta/main.h>
#include <meta/util.h>
#include <meta/window.h>
#include <ui/ui.h>
#include "meta-plugin-manager.h"
#include "wayland/meta-wayland.h"
#include "window-private.h"

#define TEST_RUNNER_ERROR test_runner_error_quark ()

typedef enum
{
  TEST_RUNNER_ERROR_BAD_COMMAND,
  TEST_RUNNER_ERROR_RUNTIME_ERROR,
  TEST_RUNNER_ERROR_ASSERTION_FAILED
} TestRunnerError;


GQuark test_runner_error_quark (void);

G_DEFINE_QUARK (test-runner-error-quark, test_runner_error)

/**********************************************************************/

typedef struct {
  XSyncCounter counter;
  int counter_value;
  XSyncAlarm alarm;

  GMainLoop *loop;
  int counter_wait_value;
} AsyncWaiter;

static AsyncWaiter *
async_waiter_new (void)
{
  AsyncWaiter *waiter = g_new0 (AsyncWaiter, 1);

  Display *xdisplay = meta_get_display ()->xdisplay;
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

static void
async_waiter_destroy (AsyncWaiter *waiter)
{
  Display *xdisplay = meta_get_display ()->xdisplay;

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

static void
async_waiter_set_and_wait (AsyncWaiter *waiter)
{
  Display *xdisplay = meta_get_display ()->xdisplay;
  int wait_value = async_waiter_next_value (waiter);

  XSyncValue sync_value;
  XSyncIntToValue (&sync_value, wait_value);

  XSyncSetCounter (xdisplay, waiter->counter, sync_value);
  async_waiter_wait (waiter, wait_value);
}

static gboolean
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

/**********************************************************************/

typedef struct {
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
} TestClient;

static char *test_client_path;

static TestClient *
test_client_new (const char          *id,
                 MetaWindowClientType type,
                 GError             **error)
{
  TestClient *client = g_new0 (TestClient, 1);
  GSubprocessLauncher *launcher;
  GSubprocess *subprocess;

  launcher =  g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  g_assert (meta_is_wayland_compositor ());
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();

  g_subprocess_launcher_setenv (launcher,
                                "WAYLAND_DISPLAY", meta_wayland_get_wayland_display_name (compositor),
                                TRUE);
  g_subprocess_launcher_setenv (launcher,
                                "DISPLAY", meta_wayland_get_xwayland_display_name (compositor),
                                TRUE);

  subprocess = g_subprocess_launcher_spawn (launcher,
                                            error,
                                            test_client_path,
                                            "--client-id",
                                            id,
                                            type == META_WINDOW_CLIENT_TYPE_WAYLAND ? "--wayland" : NULL,
                                            NULL);
  g_object_unref (launcher);

  if (!subprocess)
    return NULL;

  client->type = type;
  client->id = g_strdup (id);
  client->cancellable = g_cancellable_new ();
  client->subprocess = subprocess;
  client->in = g_data_output_stream_new (g_subprocess_get_stdin_pipe (subprocess));
  client->out = g_data_input_stream_new (g_subprocess_get_stdout_pipe (subprocess));
  client->loop = g_main_loop_new (NULL, FALSE);

  if (client->type == META_WINDOW_CLIENT_TYPE_X11)
    client->waiter = async_waiter_new ();

  return client;
}

static void
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

static void
test_client_line_read (GObject      *source,
                       GAsyncResult *result,
                       gpointer      data)
{
  TestClient *client = data;

  client->line = g_data_input_stream_read_line_finish_utf8 (client->out, result,
                                                            NULL, client->error);
  g_main_loop_quit (client->loop);
}

static gboolean test_client_do (TestClient *client,
                                GError   **error,
                                ...) G_GNUC_NULL_TERMINATED;

static gboolean
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
      if (word == NULL)
        break;

      if (command->len > 0)
        g_string_append_c (command, ' ');

      char *quoted = g_shell_quote (word);
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
  if (line)
    g_free (line);

  return *error == NULL;
}

static gboolean
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

      gboolean success = test_client_do (client, error, "set_counter", counter_str, wait_value_str, NULL);
      g_free (counter_str);
      g_free (wait_value_str);
      if (!success)
        return FALSE;

      async_waiter_wait (client->waiter, wait_value);
      return TRUE;
    }
}

static MetaWindow *
test_client_find_window (TestClient *client,
                         const char *window_id,
                         GError    **error)
{
  MetaDisplay *display = meta_get_display ();

  GSList *windows = meta_display_list_windows (display,
                                               META_LIST_INCLUDE_OVERRIDE_REDIRECT);
  MetaWindow *result = NULL;
  char *expected_title = g_strdup_printf ("test/%s/%s",
                                          client->id, window_id);
  GSList *l;

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

static gboolean
test_client_alarm_filter (TestClient            *client,
                          MetaDisplay           *display,
                          XSyncAlarmNotifyEvent *event)
{
  if (client->waiter)
    return async_waiter_alarm_filter (client->waiter, display, event);
  else
    return FALSE;
}

/**********************************************************************/

typedef struct {
  GHashTable *clients;
  AsyncWaiter *waiter;
  guint log_handler_id;
  GString *warning_messages;
  GMainLoop *loop;
} TestCase;

static gboolean
test_case_alarm_filter (MetaDisplay           *display,
                        XSyncAlarmNotifyEvent *event,
                        gpointer               data)
{
  TestCase *test = data;
  GHashTableIter iter;
  gpointer key, value;

  if (async_waiter_alarm_filter (test->waiter, display, event))
    return TRUE;

  g_hash_table_iter_init (&iter, test->clients);
  while (g_hash_table_iter_next (&iter, &key, &value))
    if (test_client_alarm_filter (value, display, event))
      return TRUE;

  return FALSE;
}

static gboolean
test_case_check_warnings (TestCase *test,
                          GError  **error)
{
  if (test->warning_messages != NULL)
    {
      g_set_error (error, TEST_RUNNER_ERROR, TEST_RUNNER_ERROR_RUNTIME_ERROR,
                   "Warning messages:\n   %s", test->warning_messages->str);
      g_string_free (test->warning_messages, TRUE);
      test->warning_messages = NULL;
      return FALSE;
    }

  return TRUE;
}

static void
test_case_log_func (const gchar   *log_domain,
                    GLogLevelFlags log_level,
                    const gchar   *message,
                    gpointer       user_data)
{
  TestCase *test = user_data;

  if (test->warning_messages == NULL)
    test->warning_messages = g_string_new (message);
  else
    {
      g_string_append (test->warning_messages, "\n   ");
      g_string_append (test->warning_messages, message);
    }
}

static TestCase *
test_case_new (void)
{
  TestCase *test = g_new0 (TestCase, 1);

  test->log_handler_id = g_log_set_handler ("mutter",
                                            G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING,
                                            test_case_log_func,
                                            test);

  meta_display_set_alarm_filter (meta_get_display (),
                                 test_case_alarm_filter, test);

  test->clients = g_hash_table_new (g_str_hash, g_str_equal);
  test->waiter = async_waiter_new ();
  test->loop = g_main_loop_new (NULL, FALSE);

  return test;
}

static gboolean
test_case_before_redraw (gpointer data)
{
  TestCase *test = data;

  g_main_loop_quit (test->loop);

  return FALSE;
}

static gboolean
test_case_wait (TestCase *test,
                GError  **error)
{
  GHashTableIter iter;
  gpointer key, value;

  /* First have each client set a XSync counter, and wait until
   * we receive the resulting event - so we know we've received
   * everything that the client have sent us.
   */
  g_hash_table_iter_init (&iter, test->clients);
  while (g_hash_table_iter_next (&iter, &key, &value))
    if (!test_client_wait (value, error))
      return FALSE;

  /* Then wait until we've done any outstanding queued up work.
   * Though we add this as BEFORE_REDRAW, the iteration that runs the
   * BEFORE_REDRAW idles will proceed on and do the redraw, so we're
   * waiting until after *all* frame processing.
   */
  meta_later_add (META_LATER_BEFORE_REDRAW,
                  test_case_before_redraw,
                  test,
                  NULL);
  g_main_loop_run (test->loop);

  /* Then set an XSync counter ourselves and and wait until
   * we receive the resulting event - this makes sure that we've
   * received back any X events we generated.
   */
  async_waiter_set_and_wait (test->waiter);
  return TRUE;
}

#define BAD_COMMAND(...)                                                \
  G_STMT_START {                                                        \
      g_set_error (error,                                               \
                   TEST_RUNNER_ERROR, TEST_RUNNER_ERROR_BAD_COMMAND,    \
                   __VA_ARGS__);                                        \
      return FALSE;                                                     \
  } G_STMT_END

static TestClient *
test_case_lookup_client (TestCase *test,
                         char     *client_id,
                         GError  **error)
{
  TestClient *client = g_hash_table_lookup (test->clients, client_id);
  if (!client)
    g_set_error (error, TEST_RUNNER_ERROR, TEST_RUNNER_ERROR_BAD_COMMAND,
                 "No such client %s", client_id);

  return client;
}

static gboolean
test_case_parse_window_id (TestCase    *test,
                           const char  *client_and_window_id,
                           TestClient **client,
                           const char **window_id,
                           GError     **error)
{
  const char *slash = strchr (client_and_window_id, '/');
  char *tmp;
  if (slash == NULL)
    BAD_COMMAND ("client/window ID %s doesnt' contain a /", client_and_window_id);

  *window_id = slash + 1;

  tmp = g_strndup (client_and_window_id, slash - client_and_window_id);
  *client = test_case_lookup_client (test, tmp, error);
  g_free (tmp);

  return client != NULL;
}

static gboolean
test_case_assert_stacking (TestCase *test,
                           char    **expected_windows,
                           int       n_expected_windows,
                           GError  **error)
{
  MetaDisplay *display = meta_get_display ();
  guint64 *windows;
  int n_windows;
  GString *stack_string = g_string_new (NULL);
  GString *expected_string = g_string_new (NULL);
  int i;

  meta_stack_tracker_get_stack (display->screen->stack_tracker, &windows, &n_windows);
  for (i = 0; i < n_windows; i++)
    {
      MetaWindow *window = meta_display_lookup_stack_id (display, windows[i]);
      if (window != NULL && window->title)
        {
          /* See comment in meta_ui_new() about why the dummy window for GTK+ theming
           * is managed as a MetaWindow.
           */
          if (META_STACK_ID_IS_X11 (windows[i]) &&
              meta_ui_window_is_dummy (display->screen->ui, windows[i]))
            continue;

          if (stack_string->len > 0)
            g_string_append_c (stack_string, ' ');

          if (g_str_has_prefix (window->title, "test/"))
            g_string_append (stack_string, window->title + 5);
          else
            g_string_append_printf (stack_string, "(%s)", window->title);
        }
      else if (windows[i] == display->screen->guard_window)
        {
          if (stack_string->len > 0)
            g_string_append_c (stack_string, ' ');

          g_string_append_c (stack_string, '|');
        }
    }

  for (i = 0; i < n_expected_windows; i++)
    {
      if (expected_string->len > 0)
        g_string_append_c (expected_string, ' ');

      g_string_append (expected_string, expected_windows[i]);
    }

  /* Don't require '| ' as a prefix if there are no hidden windows - we
   * remove the prefix from the actual string instead of adding it to the
   * expected string for clarity of the error message
   */
  if (index (expected_string->str, '|') == NULL && stack_string->str[0] == '|')
    {
      g_string_erase (stack_string,
                      0, stack_string->str[1] == ' ' ? 2 : 1);
    }

  if (strcmp (expected_string->str, stack_string->str) != 0)
    {
      g_set_error (error, TEST_RUNNER_ERROR, TEST_RUNNER_ERROR_ASSERTION_FAILED,
                   "stacking: expected='%s', actual='%s'",
                   expected_string->str, stack_string->str);
    }

  g_string_free (stack_string, TRUE);
  g_string_free (expected_string, TRUE);

  return *error == NULL;
}

static gboolean
test_case_check_xserver_stacking (TestCase *test,
                                  GError  **error)
{
  MetaDisplay *display = meta_get_display ();
  GString *local_string = g_string_new (NULL);
  GString *x11_string = g_string_new (NULL);
  int i;

  guint64 *windows;
  int n_windows;
  meta_stack_tracker_get_stack (display->screen->stack_tracker, &windows, &n_windows);

  for (i = 0; i < n_windows; i++)
    {
      if (META_STACK_ID_IS_X11 (windows[i]))
        {
          if (local_string->len > 0)
            g_string_append_c (local_string, ' ');

          g_string_append_printf (local_string, "%#lx", (Window)windows[i]);
        }
    }

  Window root;
  Window parent;
  Window *children;
  unsigned int n_children;
  XQueryTree (display->xdisplay,
              meta_screen_get_xroot (display->screen),
              &root, &parent, &children, &n_children);

  for (i = 0; i < (int)n_children; i++)
    {
      if (x11_string->len > 0)
        g_string_append_c (x11_string, ' ');

      g_string_append_printf (x11_string, "%#lx", (Window)children[i]);
    }

  if (strcmp (x11_string->str, local_string->str) != 0)
    g_set_error (error, TEST_RUNNER_ERROR, TEST_RUNNER_ERROR_ASSERTION_FAILED,
                 "xserver stacking: x11='%s', local='%s'",
                 x11_string->str, local_string->str);

  XFree (children);

  g_string_free (local_string, TRUE);
  g_string_free (x11_string, TRUE);

  return *error == NULL;
}

static gboolean
test_case_do (TestCase *test,
              int       argc,
              char    **argv,
              GError  **error)
{
  if (strcmp (argv[0], "new_client") == 0)
    {
      MetaWindowClientType type;

      if (argc != 3)
        BAD_COMMAND("usage: new_client <client-id> [wayland|x11]");

      if (strcmp (argv[2], "x11") == 0)
        type = META_WINDOW_CLIENT_TYPE_X11;
      else if (strcmp (argv[2], "wayland") == 0)
        type = META_WINDOW_CLIENT_TYPE_WAYLAND;
      else
        BAD_COMMAND("usage: new_client <client-id> [wayland|x11]");

      if (g_hash_table_lookup (test->clients, argv[1]))
        BAD_COMMAND("client %s already exists", argv[1]);

      TestClient *client = test_client_new (argv[1], type, error);
      if (!client)
        return FALSE;

      g_hash_table_insert (test->clients, client->id, client);
    }
  else if (strcmp (argv[0], "quit_client") == 0)
    {
      if (argc != 2)
        BAD_COMMAND("usage: quit_client <client-id>");

      TestClient *client = test_case_lookup_client (test, argv[1], error);
      if (!client)
        return FALSE;

      if (!test_client_do (client, error, "destroy_all", NULL))
        return FALSE;

      if (!test_client_wait (client, error))
        return FALSE;

      g_hash_table_remove (test->clients, client->id);
      test_client_destroy (client);
    }
  else if (strcmp (argv[0], "create") == 0)
    {
      if (!(argc == 2 ||
            (argc == 3 && strcmp (argv[2], "override") == 0) ||
            (argc == 3 && strcmp (argv[2], "csd") == 0)))
        BAD_COMMAND("usage: %s <client-id>/<window-id > [override|csd]", argv[0]);

      TestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      if (!test_client_do (client, error,
                           "create", window_id,
                           argc == 3 ? argv[2] : NULL,
                           NULL))
        return FALSE;
    }
  else if (strcmp (argv[0], "show") == 0 ||
           strcmp (argv[0], "hide") == 0 ||
           strcmp (argv[0], "activate") == 0 ||
           strcmp (argv[0], "raise") == 0 ||
           strcmp (argv[0], "lower") == 0 ||
           strcmp (argv[0], "minimize") == 0 ||
           strcmp (argv[0], "unminimize") == 0 ||
           strcmp (argv[0], "destroy") == 0)
    {
      if (argc != 2)
        BAD_COMMAND("usage: %s <client-id>/<window-id>", argv[0]);

      TestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      if (!test_client_do (client, error, argv[0], window_id, NULL))
        return FALSE;
    }
  else if (strcmp (argv[0], "local_activate") == 0)
    {
      if (argc != 2)
        BAD_COMMAND("usage: %s <client-id>/<window-id>", argv[0]);

      TestClient *client;
      const char *window_id;
      if (!test_case_parse_window_id (test, argv[1], &client, &window_id, error))
        return FALSE;

      MetaWindow *window = test_client_find_window (client, window_id, error);
      if (!window)
        return FALSE;

      meta_window_activate (window, 0);
    }
  else if (strcmp (argv[0], "wait") == 0)
    {
      if (argc != 1)
        BAD_COMMAND("usage: %s", argv[0]);

      if (!test_case_wait (test, error))
        return FALSE;
    }
  else if (strcmp (argv[0], "assert_stacking") == 0)
    {
      if (!test_case_assert_stacking (test, argv + 1, argc - 1, error))
        return FALSE;

      if (!test_case_check_xserver_stacking (test, error))
        return FALSE;
    }
  else
    {
      BAD_COMMAND("Unknown command %s", argv[0]);
    }

  return test_case_check_warnings (test, error);
}

static gboolean
test_case_destroy (TestCase *test,
                   GError  **error)
{
  /* Failures when cleaning up the test case aren't recoverable, since we'll
   * pollute the subsequent test cases, so we just return the error, and
   * skip the rest of the cleanup.
   */
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, test->clients);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (!test_client_do (value, error, "destroy_all", NULL))
        return FALSE;

    }

  if (!test_case_wait (test, error))
    return FALSE;

  if (!test_case_assert_stacking (test, NULL, 0, error))
    return FALSE;

  if (!test_case_check_warnings (test, error))
    return FALSE;

  g_hash_table_iter_init (&iter, test->clients);
  while (g_hash_table_iter_next (&iter, &key, &value))
    test_client_destroy (value);

  async_waiter_destroy (test->waiter);

  meta_display_set_alarm_filter (meta_get_display (), NULL, NULL);

  g_hash_table_destroy (test->clients);
  g_free (test);

  g_log_remove_handler ("mutter", test->log_handler_id);

  return TRUE;
}

/**********************************************************************/

static gboolean
run_test (const char *filename,
          int         index)
{
  TestCase *test = test_case_new ();
  GError *error = NULL;

  GFile *file = g_file_new_for_path (filename);

  GDataInputStream *in = NULL;

  GFileInputStream *in_raw = g_file_read (file, NULL, &error);
  g_object_unref (file);
  if (in_raw == NULL)
    goto out;

  in = g_data_input_stream_new (G_INPUT_STREAM (in_raw));
  g_object_unref (in_raw);

  int line_no = 0;
  while (error == NULL)
    {
      char *line = g_data_input_stream_read_line_utf8 (in, NULL, NULL, &error);
      if (line == NULL)
        break;

      line_no++;

      int argc;
      char **argv = NULL;
      if (!g_shell_parse_argv (line, &argc, &argv, &error))
        {
          if (g_error_matches (error, G_SHELL_ERROR, G_SHELL_ERROR_EMPTY_STRING))
            {
              g_clear_error (&error);
              goto next;
            }

          goto next;
        }

      test_case_do (test, argc, argv, &error);

    next:
      if (error)
        g_prefix_error (&error, "%d: ", line_no);

      g_free (line);
      g_strfreev (argv);
    }

  {
    GError *tmp_error = NULL;
    if (!g_input_stream_close (G_INPUT_STREAM (in), NULL, &tmp_error))
      {
        if (error != NULL)
          g_clear_error (&tmp_error);
        else
          g_propagate_error (&error, tmp_error);
      }
  }

 out:
  if (in != NULL)
    g_object_unref (in);

  GError *cleanup_error = NULL;
  test_case_destroy (test, &cleanup_error);

  const char *testspos = strstr (filename, "tests/");
  char *pretty_name;
  if (testspos)
    pretty_name = g_strdup (testspos + strlen("tests/"));
  else
    pretty_name = g_strdup (filename);

  if (error || cleanup_error)
    {
      g_print ("not ok %d %s\n", index, pretty_name);

      if (error)
        g_print ("   %s\n", error->message);

      if (cleanup_error)
        {
          g_print ("   Fatal Error During Cleanup\n");
          g_print ("   %s\n", cleanup_error->message);
          exit (1);
        }
    }
  else
    {
      g_print ("ok %d %s\n", index, pretty_name);
    }

  g_free (pretty_name);

  gboolean success = error == NULL;

  g_clear_error (&error);
  g_clear_error (&cleanup_error);

  return success;
}

typedef struct {
  int n_tests;
  char **tests;
} RunTestsInfo;

static gboolean
run_tests (gpointer data)
{
  RunTestsInfo *info = data;
  int i;
  gboolean success = TRUE;

  g_print ("1..%d\n", info->n_tests);

  for (i = 0; i < info->n_tests; i++)
    if (!run_test (info->tests[i], i + 1))
      success = FALSE;

  meta_quit (success ? 0 : 1);

  return FALSE;
}

/**********************************************************************/

static gboolean
find_metatests_in_directory (GFile     *directory,
                             GPtrArray *results,
                             GError   **error)
{
  GFileEnumerator *enumerator = g_file_enumerate_children (directory,
                                                           "standard::name,standard::type",
                                                           G_FILE_QUERY_INFO_NONE,
                                                           NULL, error);
  if (!enumerator)
    return FALSE;

  while (*error == NULL)
    {
      GFileInfo *info = g_file_enumerator_next_file (enumerator, NULL, error);
      if (info == NULL)
        break;

      GFile *child = g_file_enumerator_get_child (enumerator, info);
      switch (g_file_info_get_file_type (info))
        {
        case G_FILE_TYPE_REGULAR:
          {
            const char *name = g_file_info_get_name (info);
            if (g_str_has_suffix (name, ".metatest"))
              g_ptr_array_add (results, g_file_get_path (child));
            break;
          }
        case G_FILE_TYPE_DIRECTORY:
          find_metatests_in_directory (child, results, error);
          break;
        default:
          break;
        }

      g_object_unref (child);
      g_object_unref (info);
    }

  {
    GError *tmp_error = NULL;
    if (!g_file_enumerator_close (enumerator, NULL, &tmp_error))
      {
        if (*error != NULL)
          g_clear_error (&tmp_error);
        else
          g_propagate_error (error, tmp_error);
      }
  }

  g_object_unref (enumerator);
  return *error == NULL;
}

static gboolean all_tests = FALSE;

const GOptionEntry options[] = {
  {
    "all", 0, 0, G_OPTION_ARG_NONE,
    &all_tests,
    "Run all installed tests",
    NULL
  },
  { NULL }
};

int
main (int argc, char **argv)
{
  GOptionContext *ctx;
  GError *error = NULL;

  /* First parse the arguments that are passed to us */

  ctx = g_option_context_new (NULL);
  g_option_context_add_main_entries (ctx, options, NULL);

  if (!g_option_context_parse (ctx,
                               &argc, &argv, &error))
    {
      g_printerr ("%s", error->message);
      return 1;
    }

  g_option_context_free (ctx);

  GPtrArray *tests = g_ptr_array_new ();

  if (all_tests)
    {
      GFile *test_dir = g_file_new_for_path (MUTTER_PKGDATADIR "/tests");
      GError *error = NULL;

      if (!find_metatests_in_directory (test_dir, tests, &error))
        {
          g_printerr ("Error enumerating tests: %s\n", error->message);
          return 1;
        }
    }
  else
    {
      int i;
      char *curdir = g_get_current_dir ();

      for (i = 1; i < argc; i++)
        {
          if (g_path_is_absolute (argv[i]))
            g_ptr_array_add (tests, g_strdup (argv[i]));
          else
            g_ptr_array_add (tests, g_build_filename (curdir, argv[i], NULL));
        }

      g_free (curdir);
    }

  /* Then initalize mutter with a different set of arguments */

  char *fake_args[] = { NULL, "--wayland" };
  fake_args[0] = argv[0];
  char **fake_argv = fake_args;
  int fake_argc = 2;

  char *basename = g_path_get_basename (argv[0]);
  char *dirname = g_path_get_dirname (argv[0]);
  if (g_str_has_prefix (basename, "lt-"))
    test_client_path = g_build_filename (dirname, "../mutter-test-client", NULL);
  else
    test_client_path = g_build_filename (dirname, "mutter-test-client", NULL);
  g_free (basename);
  g_free (dirname);

  ctx = meta_get_option_context ();
  if (!g_option_context_parse (ctx, &fake_argc, &fake_argv, &error))
    {
      g_printerr ("mutter: %s\n", error->message);
      exit (1);
    }
  g_option_context_free (ctx);

  meta_plugin_manager_load ("default");

  meta_init ();
  meta_register_with_session ();

  RunTestsInfo info;
  info.tests = (char **)tests->pdata;
  info.n_tests = tests->len;

  g_idle_add (run_tests, &info);

  return meta_run ();
}
