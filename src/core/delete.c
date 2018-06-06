/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter window deletion */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2004 Elijah Newren
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

#define _XOPEN_SOURCE /* for kill() */

#include <config.h>
#include "util-private.h"
#include "window-private.h"
#include "compositor-private.h"
#include <meta/errors.h>
#include <meta/workspace.h>
#include <errno.h>

static void
close_dialog_response_cb (MetaCloseDialog         *dialog,
                          MetaCloseDialogResponse  response,
                          MetaWindow              *window)
{
  if (response == META_CLOSE_DIALOG_RESPONSE_FORCE_CLOSE)
    meta_window_kill (window);
}

static void
meta_window_ensure_close_dialog (MetaWindow *window)
{
  MetaDisplay *display;

  if (window->close_dialog)
    return;

  display = window->display;
  window->close_dialog = meta_compositor_create_close_dialog (display->compositor,
                                                              window);
  g_signal_connect (window->close_dialog, "response",
                    G_CALLBACK (close_dialog_response_cb), window);
}

void
meta_window_set_alive (MetaWindow *window,
                       gboolean    is_alive)
{
  if (is_alive && window->close_dialog)
    {
      meta_close_dialog_hide (window->close_dialog);
    }
  else if (!is_alive)
    {
      meta_window_ensure_close_dialog (window);
      meta_close_dialog_show (window->close_dialog);

      if (window->display &&
          window->display->event_route == META_EVENT_ROUTE_NORMAL &&
          window == window->display->focus_window)
        meta_close_dialog_focus (window->close_dialog);
    }
}

void
meta_window_check_alive (MetaWindow *window,
                         guint32     timestamp)
{
  meta_display_ping_window (window, timestamp);
}

void
meta_window_delete (MetaWindow  *window,
                    guint32      timestamp)
{
  META_WINDOW_GET_CLASS (window)->delete (window, timestamp);

  meta_window_check_alive (window, timestamp);
}

void
meta_window_kill (MetaWindow *window)
{
  pid_t pid = meta_window_get_client_pid (window);

  if (pid > 0)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Killing %s with kill()\n",
                  window->desc);

      if (kill (pid, 9) == 0)
        return;

      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Failed to signal %s: %s\n",
                  window->desc, strerror (errno));
    }

  META_WINDOW_GET_CLASS (window)->kill (window);
}

void
meta_window_free_delete_dialog (MetaWindow *window)
{
  if (window->close_dialog &&
      meta_close_dialog_is_visible (window->close_dialog))
    meta_close_dialog_hide (window->close_dialog);
  g_clear_object (&window->close_dialog);
}
