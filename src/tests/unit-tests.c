/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015 Red Hat, Inc.
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

#include <glib.h>
#include <stdlib.h>

#include <meta/main.h>
#include <meta/util.h>

#include "compositor/meta-plugin-manager.h"

typedef struct _MetaTestLaterOrderCallbackData
{
  GMainLoop *loop; /* Loop to terminate when done. */
  int callback_num; /* Callback number integer. */
  int *expected_callback_num; /* Pointer to the expected callback number. */
} MetaTestLaterOrderCallbackData;

static gboolean
test_later_order_callback (gpointer user_data)
{
  MetaTestLaterOrderCallbackData *data = user_data;

  g_assert_cmpint (data->callback_num, ==, *data->expected_callback_num);

  if (*data->expected_callback_num == 0)
    g_main_loop_quit (data->loop);
  else
    (*data->expected_callback_num)--;

  return FALSE;
}

static void
meta_test_util_later_order (void)
{
  GMainLoop *loop;
  int expected_callback_num;
  int i;
  const int num_callbacks = 3;
  MetaTestLaterOrderCallbackData callback_data[num_callbacks];

  loop = g_main_loop_new (NULL, FALSE);

  /* Schedule three BEFORE_DRAW callbacks each with its own number associated
   * with it.
   */
  for (i = 0; i < num_callbacks; i++)
    {
      callback_data[i] = (MetaTestLaterOrderCallbackData) {
        .loop = loop,
        .callback_num = i,
        .expected_callback_num = &expected_callback_num,
      };
      meta_later_add (META_LATER_BEFORE_REDRAW,
                      test_later_order_callback,
                      &callback_data[i],
                      NULL);
    }

  /* Check that the callbacks are invoked in the opposite order that they were
   * scheduled. Each callback will decrease the number by 1 after it checks the
   * validity.
   */
  expected_callback_num = num_callbacks - 1;
  g_main_loop_run (loop);
  g_assert_cmpint (expected_callback_num, ==, 0);
  g_main_loop_unref (loop);
}

typedef enum _MetaTestLaterScheduleFromLaterState
{
  META_TEST_LATER_EXPECT_CALC_SHOWING,
  META_TEST_LATER_EXPECT_SYNC_STACK,
  META_TEST_LATER_EXPECT_BEFORE_REDRAW,
  META_TEST_LATER_FINISHED,
} MetaTestLaterScheduleFromLaterState;

typedef struct _MetaTestLaterScheduleFromLaterData
{
  GMainLoop *loop;
  MetaTestLaterScheduleFromLaterState state;
} MetaTestLaterScheduleFromLaterData;

static gboolean
test_later_schedule_from_later_sync_stack_callback (gpointer user_data);

static gboolean
test_later_schedule_from_later_calc_showing_callback (gpointer user_data)
{
  MetaTestLaterScheduleFromLaterData *data = user_data;

  g_assert_cmpint (data->state, ==, META_TEST_LATER_EXPECT_CALC_SHOWING);

  meta_later_add (META_LATER_SYNC_STACK,
                  test_later_schedule_from_later_sync_stack_callback,
                  data,
                  NULL);

  data->state = META_TEST_LATER_EXPECT_SYNC_STACK;

  return FALSE;
}

static gboolean
test_later_schedule_from_later_sync_stack_callback (gpointer user_data)
{
  MetaTestLaterScheduleFromLaterData *data = user_data;

  g_assert_cmpint (data->state, ==, META_TEST_LATER_EXPECT_SYNC_STACK);

  data->state = META_TEST_LATER_EXPECT_BEFORE_REDRAW;

  return FALSE;
}

static gboolean
test_later_schedule_from_later_before_redraw_callback (gpointer user_data)
{
  MetaTestLaterScheduleFromLaterData *data = user_data;

  g_assert_cmpint (data->state, ==, META_TEST_LATER_EXPECT_BEFORE_REDRAW);
  data->state = META_TEST_LATER_FINISHED;
  g_main_loop_quit (data->loop);

  return FALSE;
}

static void
meta_test_util_later_schedule_from_later (void)
{
  MetaTestLaterScheduleFromLaterData data;

  data.loop = g_main_loop_new (NULL, FALSE);

  /* Test that scheduling a MetaLater with 'when' being later than the one being
   * invoked causes it to be invoked before any callback with a later 'when'
   * value being invoked.
   *
   * The first and last callback is queued here. The one to be invoked in
   * between is invoked in test_later_schedule_from_later_calc_showing_callback.
   */
  meta_later_add (META_LATER_CALC_SHOWING,
                  test_later_schedule_from_later_calc_showing_callback,
                  &data,
                  NULL);
  meta_later_add (META_LATER_BEFORE_REDRAW,
                  test_later_schedule_from_later_before_redraw_callback,
                  &data,
                  NULL);

  data.state = META_TEST_LATER_EXPECT_CALC_SHOWING;

  g_main_loop_run (data.loop);
  g_main_loop_unref (data.loop);

  g_assert_cmpint (data.state, ==, META_TEST_LATER_FINISHED);
}

static gboolean
run_tests (gpointer data)
{
  gboolean ret;

  ret = g_test_run ();

  meta_quit (ret != 0);

  return FALSE;
}

static void
init_tests (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

  g_test_add_func ("/util/meta-later/order", meta_test_util_later_order);
  g_test_add_func ("/util/meta-later/schedule-from-later",
                   meta_test_util_later_schedule_from_later);
}

int
main (int argc, char *argv[])
{
  GOptionContext *ctx;
  GError *error = NULL;

  ctx = g_option_context_new (NULL);

  if (!g_option_context_parse (ctx,
                               &argc, &argv, &error))
    {
      g_printerr ("%s", error->message);
      return 1;
    }

  g_option_context_free (ctx);

  const char *fake_args[] = { NULL, "--wayland", "--nested" };
  fake_args[0] = argv[0];
  char **fake_argv = (char**)fake_args;
  int fake_argc = G_N_ELEMENTS (fake_args);

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

  init_tests (argc, argv);
  g_idle_add (run_tests, NULL);

  return meta_run ();
}
