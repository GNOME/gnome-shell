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
#include <meta/errors.h>
#include <meta/workspace.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "meta-wayland-surface.h"

static void meta_window_present_delete_dialog (MetaWindow *window,
                                               guint32     timestamp);

static void
delete_ping_reply_func (MetaWindow  *window,
                        guint32      timestamp,
                        void        *user_data)
{
  meta_topic (META_DEBUG_PING, "Got reply to delete ping for %s\n", window->desc);

  /* we do nothing */
}

static void
dialog_exited (GPid pid, int status, gpointer user_data)
{
  MetaWindow *ours = (MetaWindow*) user_data;

  ours->dialog_pid = -1;

  /* exit status of 1 means the user pressed "Force Quit" */
  if (WIFEXITED (status) && WEXITSTATUS (status) == 1)
    meta_window_kill (ours);
}

static void
delete_ping_timeout_func (MetaWindow  *window,
                          guint32      timestamp,
                          void        *user_data)
{
  char *window_title;
  gchar *window_content, *tmp;
  GPid dialog_pid;
  
  meta_topic (META_DEBUG_PING,
              "Got delete ping timeout for %s\n",
              window->desc);

  if (window->dialog_pid >= 0)
    {
      meta_window_present_delete_dialog (window, timestamp);
      return;
    }

  /* This is to get a better string if the title isn't representable
   * in the locale encoding; actual conversion to UTF-8 is done inside
   * meta_show_dialog */

  if (window->title && window->title[0])
    {
      tmp = g_locale_from_utf8 (window->title, -1, NULL, NULL, NULL);
      if (tmp == NULL)
        window_title = NULL;
      else
        window_title = window->title;
      g_free (tmp);
    }
  else
    {
      window_title = NULL;
    }

  /* Translators: %s is a window title */
  if (window_title)
    tmp = g_strdup_printf (_("“%s” is not responding."), window_title);
  else
    tmp = g_strdup (_("Application is not responding."));

  window_content = g_strdup_printf (
      "<big><b>%s</b></big>\n\n%s",
      tmp,
      _("You may choose to wait a short while for it to "
        "continue or force the application to quit entirely."));

  dialog_pid =
    meta_show_dialog ("--question",
                      window_content, NULL,
                      window->screen->screen_name,
                      _("_Wait"), _("_Force Quit"),
                      "face-sad-symbolic", window->xwindow,
                      NULL, NULL);

  g_free (window_content);
  g_free (tmp);

  window->dialog_pid = dialog_pid;
  g_child_watch_add (dialog_pid, dialog_exited, window);
}

void
meta_window_check_alive (MetaWindow *window,
                         guint32     timestamp)
{
  meta_display_ping_window (window,
                            timestamp,
                            delete_ping_reply_func,
                            delete_ping_timeout_func,
                            NULL);
}

void
meta_window_delete (MetaWindow  *window,
                    guint32      timestamp)
{
  if (window->client_type == META_WINDOW_CLIENT_TYPE_X11)
    {
      meta_error_trap_push (window->display);
      if (window->delete_window)
        {
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "Deleting %s with delete_window request\n",
                      window->desc);
          meta_window_send_icccm_message (window,
                                          window->display->atom_WM_DELETE_WINDOW,
                                          timestamp);
        }
      else
        {
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "Deleting %s with explicit kill\n",
                      window->desc);
          XKillClient (window->display->xdisplay, window->xwindow);
        }
      meta_error_trap_pop (window->display);
    }
  else
    {
      meta_wayland_surface_delete (window->surface);
    }

  meta_window_check_alive (window, timestamp);

  if (window->has_focus)
    {
      /* FIXME Clean this up someday 
       * http://bugzilla.gnome.org/show_bug.cgi?id=108706
       */
#if 0
      /* This is unfortunately going to result in weirdness
       * if the window doesn't respond to the delete event.
       * I don't know how to avoid that though.
       */
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing default window because focus window %s was deleted/killed\n",
                  window->desc);
      meta_workspace_focus_default_window (window->screen->active_workspace,
                                           window);
#else
      meta_topic (META_DEBUG_FOCUS,
                  "Not unfocusing %s on delete/kill\n",
                  window->desc);
#endif
    }
  else
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Window %s was deleted/killed but didn't have focus\n",
                  window->desc);
    }
}


void
meta_window_kill (MetaWindow *window)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Killing %s brutally\n",
              window->desc);

  if (!meta_window_is_remote (window) &&
      window->net_wm_pid > 0)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Killing %s with kill()\n",
                  window->desc);

      if (kill (window->net_wm_pid, 9) < 0)
        meta_topic (META_DEBUG_WINDOW_OPS,
                    "Failed to signal %s: %s\n",
                    window->desc, strerror (errno));
    }

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Disconnecting %s with XKillClient()\n",
              window->desc);
  meta_error_trap_push (window->display);
  XKillClient (window->display->xdisplay, window->xwindow);
  meta_error_trap_pop (window->display);
}

void
meta_window_free_delete_dialog (MetaWindow *window)
{
  if (window->dialog_pid >= 0)
    {
      kill (window->dialog_pid, 9);
      window->dialog_pid = -1;
    }
}

static void
meta_window_present_delete_dialog (MetaWindow *window, guint32 timestamp)
{
  meta_topic (META_DEBUG_PING,
              "Presenting existing ping dialog for %s\n",
              window->desc);
  
  if (window->dialog_pid >= 0)
    {
      GSList *windows;
      GSList *tmp;

      /* Activate transient for window that belongs to
       * mutter-dialog
       */
      
      windows = meta_display_list_windows (window->display, META_LIST_DEFAULT);
      tmp = windows;
      while (tmp != NULL)
        {
          MetaWindow *w = tmp->data;

          if (w->xtransient_for == window->xwindow &&
              w->res_class &&
              g_ascii_strcasecmp (w->res_class, "mutter-dialog") == 0)
            {
              meta_window_activate (w, timestamp);
              break;
            }
          
          tmp = tmp->next;
        }

      g_slist_free (windows);
    }
}
