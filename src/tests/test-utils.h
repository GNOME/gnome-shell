/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat, Inc.
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

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>

#include "meta/window.h"

#define TEST_RUNNER_ERROR test_runner_error_quark ()

typedef enum
{
  TEST_RUNNER_ERROR_BAD_COMMAND,
  TEST_RUNNER_ERROR_RUNTIME_ERROR,
  TEST_RUNNER_ERROR_ASSERTION_FAILED
} TestRunnerError;

GQuark test_runner_error_quark (void);

typedef struct _AsyncWaiter AsyncWaiter;
typedef struct _TestClient TestClient;

void test_init (int    argc,
                char **argv);

gboolean async_waiter_alarm_filter (AsyncWaiter           *waiter,
                                    MetaDisplay           *display,
                                    XSyncAlarmNotifyEvent *event);

void async_waiter_set_and_wait (AsyncWaiter *waiter);

AsyncWaiter * async_waiter_new (void);

void async_waiter_destroy (AsyncWaiter *waiter);

char * test_client_get_id (TestClient *client);

gboolean test_client_alarm_filter (TestClient            *client,
                                   MetaDisplay           *display,
                                   XSyncAlarmNotifyEvent *event);

gboolean test_client_wait (TestClient *client,
                           GError    **error);

gboolean test_client_do (TestClient *client,
                         GError   **error,
                         ...) G_GNUC_NULL_TERMINATED;

MetaWindow * test_client_find_window (TestClient *client,
                                      const char *window_id,
                                      GError    **error);

gboolean test_client_quit (TestClient *client,
                           GError    **error);

TestClient * test_client_new (const char          *id,
                              MetaWindowClientType type,
                              GError             **error);

void test_client_destroy (TestClient *client);

#endif /* TEST_UTILS_H */
