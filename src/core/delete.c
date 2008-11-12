/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity window deletion */

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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#define _GNU_SOURCE
#define _SVID_SOURCE /* for gethostname() */

#include <config.h>
#include "util.h"
#include "window-private.h"
#include "errors.h"
#include "workspace.h"

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void meta_window_present_delete_dialog (MetaWindow *window,
                                               guint32     timestamp);

static void
delete_ping_reply_func (MetaDisplay *display,
                        Window       xwindow,
                        guint32      timestamp,
                        void        *user_data)
{
  meta_topic (META_DEBUG_PING,
              "Got reply to delete ping for %s\n",
              ((MetaWindow*)user_data)->desc);

  /* we do nothing */
}

static Window
window_from_string (const char *str)
{
  char *end;
  unsigned long l;

  end = NULL;
  
  l = strtoul (str, &end, 16);

  if (end == NULL || end == str)
    {
      meta_warning (_("Could not parse \"%s\" as an integer"),
                    str);
      return None;
    }

  if (*end != '\0')
    {
      meta_warning (_("Did not understand trailing characters \"%s\" in string \"%s\""),
                    end, str);
      return None;
    }

  return l;
}

static int
pid_from_string (const char *str)
{
  char *end;
  long l;

  end = NULL;
  
  l = strtol (str, &end, 10);

  if (end == NULL || end == str)
    {
      meta_warning (_("Could not parse \"%s\" as an integer"),
                    str);
      return None;
    }

  if (*end != '\0')
    {
      meta_warning (_("Did not understand trailing characters \"%s\" in string \"%s\""),
                    end, str);
      return None;
    }

  return l;
}

static gboolean
parse_dialog_output (const char *str,
                     int        *pid_out,
                     Window     *win_out)
{
  char **split;

  split = g_strsplit (str, "\n", 2);
  if (split && split[0] && split[1])
    {
      g_strchomp (split[0]);
      g_strchomp (split[1]);

      *pid_out = pid_from_string (split[0]);
      *win_out = window_from_string (split[1]);

      g_strfreev (split);
      
      return TRUE;
    }
  else
    {
      g_strfreev (split);
      meta_warning (_("Failed to parse message \"%s\" from dialog process\n"),
                    str);
      return FALSE;
    }
}

static void
search_and_destroy_window (int    pid,
                           Window xwindow)
{
  /* Find the window with the given dialog PID,
   * double check that it matches "xwindow", then
   * kill the window.
   */
  GSList *tmp;
  gboolean found = FALSE;
  GSList *windows;

  if (xwindow == None)
    {
      meta_topic (META_DEBUG_PING,
                  "Window to destroy is None, doing nothing\n");
      return;
    }

  windows = meta_display_list_windows (meta_get_display ());
  tmp = windows;

  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;

      if (w->dialog_pid == pid)
        {
          if (w->xwindow != xwindow)
            meta_topic (META_DEBUG_PING,
                        "Dialog pid matches but not xwindow (0x%lx vs. 0x%lx)\n",
                        w->xwindow, xwindow);
          else
            {
              meta_window_kill (w);
              found = TRUE;
            }
        }
          
      tmp = tmp->next;
    }

  g_slist_free (windows);
  
  if (!found)
    meta_topic (META_DEBUG_PING,
                "Did not find a window with dialog pid %d xwindow 0x%lx\n",
                pid, xwindow);
}

static void
release_window_with_fd (int fd)
{
  /* Find the window with the given dialog PID,
   * double check that it matches "xwindow", then
   * kill the window.
   */
  gboolean found = FALSE;
  
  GSList *windows = meta_display_list_windows (meta_get_display ());
  GSList *tmp = windows;

  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;

      if (w->dialog_pid >= 0 &&
          w->dialog_pipe == fd)
        {
          meta_topic (META_DEBUG_PING,
                      "Removing dialog with fd %d pid %d from window %s\n",
                      fd, w->dialog_pid, w->desc);
          meta_window_free_delete_dialog (w);
          found = TRUE;
        }
       
      tmp = tmp->next;
    }

  g_slist_free (windows);
      
  if (!found)
    meta_topic (META_DEBUG_PING,
                "Did not find a window with a dialog pipe %d\n",
                fd);
}

static gboolean  
io_from_ping_dialog (GIOChannel   *channel,
                     GIOCondition  condition,
                     gpointer      data)
{  
  meta_topic (META_DEBUG_PING,
              "IO handler from ping dialog, condition = %x\n",
              condition);
  
  if (condition & G_IO_IN)
    {
      char *str;
      gsize len;
      GError *err;

      /* Go ahead and block for all data from child */
      str = NULL;
      len = 0;
      err = NULL;
      g_io_channel_read_to_end (channel,
                                &str, &len,
                                &err);
      
      if (err)
        {
          meta_warning (_("Error reading from dialog display process: %s\n"),
                        err->message);
          g_error_free (err);
        }

      meta_topic (META_DEBUG_PING,
                  "Read %" G_GSIZE_FORMAT " bytes strlen %d \"%s\" from child\n",
                  len, str ? (int) strlen (str) : 0, str ? str : "NULL");
      
      if (len > 0)
        {
          /* We're supposed to kill the given window */
          int pid;
          Window xwindow;

          if (parse_dialog_output (str, &pid, &xwindow))
            search_and_destroy_window (pid, xwindow);
        }

      g_free (str);
    }

  release_window_with_fd (g_io_channel_unix_get_fd (channel));
  
  /* Remove the callback */
  return FALSE; 
}

static void
delete_ping_timeout_func (MetaDisplay *display,
                          Window       xwindow,
                          guint32      timestamp,
                          void        *user_data)
{
  MetaWindow *window = user_data;
  GError *err;
  int child_pid;
  int outpipe;
  char *argv[9];
  char numbuf[32];
  char timestampbuf[32];
  char *window_id_str;
  char *window_title;
  GIOChannel *channel;
  
  meta_topic (META_DEBUG_PING,
              "Got delete ping timeout for %s\n",
              window->desc);

  if (window->dialog_pid >= 0)
    {
      meta_window_present_delete_dialog (window, timestamp);
      return;
    }
  
  window_id_str = g_strdup_printf ("0x%lx", window->xwindow);
  window_title = g_locale_from_utf8 (window->title, -1, NULL, NULL, NULL);

  sprintf (numbuf, "%d", window->screen->number);
  sprintf (timestampbuf, "%u", timestamp);
  
  argv[0] = METACITY_LIBEXECDIR"/metacity-dialog";
  argv[1] = "--screen";
  argv[2] = numbuf;
  argv[3] = "--timestamp";
  argv[4] = timestampbuf;
  argv[5] = "--kill-window-question";
  argv[6] = window_title;
  argv[7] = window_id_str;
  argv[8] = NULL;
  
  err = NULL;
  if (!g_spawn_async_with_pipes ("/",
                                 argv,
                                 NULL,
                                 0,
                                 NULL, NULL,
                                 &child_pid,
                                 NULL,
                                 &outpipe,
                                 NULL,
                                 &err))
    {
      meta_warning (_("Error launching metacity-dialog to ask about killing an application: %s\n"),
                    err->message);
      g_error_free (err);
      goto out;
    }

  window->dialog_pid = child_pid;
  window->dialog_pipe = outpipe;  

  channel = g_io_channel_unix_new (window->dialog_pipe);
  g_io_add_watch_full (channel, G_PRIORITY_DEFAULT,
                       G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
                       io_from_ping_dialog,
                       NULL, NULL);
  g_io_channel_unref (channel);
  
 out:
  g_free (window_title);
  g_free (window_id_str);
}

void
meta_window_delete (MetaWindow  *window,
                    guint32      timestamp)
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
  meta_error_trap_pop (window->display, FALSE);

  meta_display_ping_window (window->display,
                            window,
                            timestamp,
                            delete_ping_reply_func,
                            delete_ping_timeout_func,
                            window);
  
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
  char buf[257];
  
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Killing %s brutally\n",
              window->desc);

  if (window->wm_client_machine != NULL &&
      window->net_wm_pid > 0)
    {
      if (gethostname (buf, sizeof(buf)-1) == 0)
        {
          if (strcmp (buf, window->wm_client_machine) == 0)
            {
              meta_topic (META_DEBUG_WINDOW_OPS,
                          "Killing %s with kill()\n",
                          window->desc);

              if (kill (window->net_wm_pid, 9) < 0)
                meta_topic (META_DEBUG_WINDOW_OPS,
                            "Failed to signal %s: %s\n",
                            window->desc, strerror (errno));
            }
        }
      else
        {
          meta_warning (_("Failed to get hostname: %s\n"),
                        strerror (errno));
        }
    }
  
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Disconnecting %s with XKillClient()\n",
              window->desc);
  meta_error_trap_push (window->display);
  XKillClient (window->display->xdisplay, window->xwindow);
  meta_error_trap_pop (window->display, FALSE);
}

void
meta_window_free_delete_dialog (MetaWindow *window)
{
  if (window->dialog_pid >= 0)
    {
      kill (window->dialog_pid, 9);
      close (window->dialog_pipe);
      window->dialog_pid = -1;
      window->dialog_pipe = -1;
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
       * metacity-dialog
       */
      
      windows = meta_display_list_windows (window->display);
      tmp = windows;
      while (tmp != NULL)
        {
          MetaWindow *w = tmp->data;

          if (w->xtransient_for == window->xwindow &&
              w->res_class &&
              g_ascii_strcasecmp (w->res_class, "metacity-dialog") == 0)
            {
              meta_window_activate (w, timestamp);
              break;
            }
          
          tmp = tmp->next;
        }

      g_slist_free (windows);
    }
}
