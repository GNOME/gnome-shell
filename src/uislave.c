/* Metacity UI Slave */

/* 
 * Copyright (C) 2001 Havoc Pennington
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

#include "uislave.h"
#include "window.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

static void       respawn_child   (MetaUISlave   *uislave);
static gboolean   error_callback  (GIOChannel    *source,
                                   GIOCondition   condition,
                                   gpointer       data);
static void       kill_child      (MetaUISlave   *uislave);
static void       reset_vals      (MetaUISlave   *uislave);
static void       message_queue_func (MetaMessageQueue *mq,
                                      MetaMessage* message,
                                      gpointer data);


MetaUISlave*
meta_ui_slave_new (const char *display_name,
                   MetaUISlaveFunc  func,
                   gpointer         data)
{
  MetaUISlave *uislave;

  uislave = g_new (MetaUISlave, 1);
  
  uislave->display_name = g_strdup (display_name);
  uislave->func = func;
  uislave->data = data;
  uislave->no_respawn = FALSE;
  
  reset_vals (uislave);
  
  /* This may fail; all UISlave functions become no-ops
   * if uislave->disabled, and metacity just runs
   * with no UI features other than window borders.
   */
  respawn_child (uislave);  
  
  return uislave;
}

void
meta_ui_slave_free (MetaUISlave *uislave)
{

  meta_verbose ("Deleting UI slave for display '%s'\n",
                uislave->display_name);
  
  kill_child (uislave);
  
  g_free (uislave->display_name);

  g_free (uislave);
}

void
meta_ui_slave_disable (MetaUISlave *uislave)
{
  /* Change UI slave into "black hole" mode,
   * we found out it's hosed for some reason.
   */
  kill_child (uislave);
  uislave->no_respawn = TRUE;
  meta_warning ("UI slave disabled, no tooltips or window menus will work\n");
}

static void
message_queue_func (MetaMessageQueue *mq,
                    MetaMessage* message,
                    gpointer data)
{
  MetaUISlave *uislave;

  uislave = data;

  (* uislave->func) (uislave, message, uislave->data);
}

static void
respawn_child (MetaUISlave *uislave)
{
  GError *error;
  const char *uislavedir;
  char *argv[] = { NULL, NULL, NULL, NULL, NULL };
  char *envp[2] = { NULL, NULL };
  int child_pid, inpipe, outpipe, errpipe;
  char *path;
  
  if (uislave->no_respawn)
    return;

  if (uislave->child_pid != 0)
    return;
  
  uislavedir = g_getenv ("METACITY_UISLAVE_DIR");
  if (uislavedir == NULL)
    uislavedir = METACITY_LIBEXECDIR;
  
  envp[0] = g_strconcat ("DISPLAY=", uislave->display_name, NULL);

  path = g_strconcat (uislavedir, "/", "metacity-uislave", NULL);
#if 0
  argv[0] = "/usr/bin/strace";
  argv[1] = "-o";
  argv[2] = "uislave-strace.log";
#endif
  argv[0] = path;
  
  meta_verbose ("Launching UI slave in dir %s display %s\n",
                uislavedir, envp[0]);
  
  error = NULL;
  if (g_spawn_async_with_pipes (NULL,
                                argv,
                                envp,
                                /* flags */
                                0,
                                /* setup func, data */
                                NULL, NULL,
                                &child_pid,
                                &inpipe, &outpipe, &errpipe,
                                &error))
    {
      uislave->child_pid = child_pid;
      uislave->in_pipe = inpipe;
      uislave->err_pipe = errpipe;
      uislave->out_pipe = outpipe;

      uislave->err_channel = g_io_channel_unix_new (errpipe);
      
      uislave->errwatch = g_io_add_watch (uislave->err_channel,
                                          G_IO_IN,
                                          error_callback,
                                          uislave);

      uislave->mq = meta_message_queue_new (outpipe,
                                            message_queue_func,
                                            uislave);
      
      meta_verbose ("Spawned UI slave with PID %d\n", uislave->child_pid);
    }
  else
    {
      meta_warning ("Failed to create user interface process: %s\n",
                    error->message);
      g_error_free (error);
    }
  
  g_free (envp[0]);
  g_free (path);
}

static gboolean
error_callback  (GIOChannel   *source,
                 GIOCondition  condition,
                 gpointer      data)
{  
  /* Relay slave errors to WM stderr */
#define BUFSIZE 1024
  MetaUISlave *uislave;
  char buf[1024];
  int n;
  static int logfile = -1;
  
  if (meta_is_debugging () && logfile < 0)
    {
      const char *dir;
      char *str;
      
      dir = g_get_home_dir ();
      str = g_strconcat (dir, "/", "metacity-uislave.log", NULL);
      
      logfile = open (str, O_TRUNC | O_CREAT, 0644);

      if (logfile < 0)
        meta_warning ("Failed to open uislave log file %s\n", str);
      else
        meta_verbose ("Opened uislave log file %s\n", str);
      
      g_free (str);
    }

  if (logfile < 0)
    logfile = 2;
  
  uislave = data;
  
  n = read (uislave->err_pipe, buf, BUFSIZE);
  if (n > 0)
    {
      if (write (logfile, buf, n) != n)
        ; /* error, but printing a message to stderr will hardly help. */
    }
  else if (n < 0)
    meta_warning (_("Error reading errors from UI slave: %s\n"),
                  g_strerror (errno));
  
  return TRUE;
#undef BUFSIZE
}

static void
kill_child (MetaUISlave *uislave)
{
  if (uislave->mq)
    meta_message_queue_free (uislave->mq);
  
  if (uislave->errwatch != 0)
    g_source_remove (uislave->errwatch);  

  if (uislave->err_channel)
    g_io_channel_unref (uislave->err_channel);

  if (uislave->out_pipe >= 0)
    close (uislave->out_pipe);

  if (uislave->in_pipe >= 0)
    close (uislave->in_pipe);

  if (uislave->err_pipe >= 0)
    close (uislave->err_pipe);
  
  if (uislave->child_pid > 0)
    {
      /* don't care if this fails except in verbose mode */
      if (kill (uislave->child_pid, SIGTERM) != 0)
        {
          meta_verbose ("Kill of UI slave process %d failed: %s\n",
                        uislave->child_pid, g_strerror (errno));
        }
      
      uislave->child_pid = 0;
    }
  
  reset_vals (uislave);
}

static void
reset_vals (MetaUISlave *uislave)
{
  uislave->mq = NULL;
  uislave->child_pid = 0;
  uislave->in_pipe = -1;
  uislave->err_pipe = -1;
  uislave->out_pipe = -1;
  uislave->err_channel = NULL;
  uislave->errwatch = 0;
  /* don't reset no_respawn, it's a permanent thing. */
}

/*
 * Message delivery
 */

static int
write_bytes (int fd, void *buf, int bytes)
{
  const char *p;
  int left;

  left = bytes;
  p = (char*) buf;
  while (left > 0)
    {
      int written;

      written = write (fd, p, left);

      if (written < 0)
        return -1;

      left -= written;
      p += written;
    }

  g_assert (p == ((char*)buf) + bytes); 
  
  return 0;
}

static void
send_message (MetaUISlave *uislave, MetaMessage *message)
{
  static int serial = 0;
  MetaMessageFooter *footer;

  if (uislave->no_respawn)
    return;
  
  respawn_child (uislave);
  
  message->header.serial = serial;
  footer = META_MESSAGE_FOOTER (message);
  
  footer->checksum = META_MESSAGE_CHECKSUM (message);
  ++serial;
  
  if (write_bytes (uislave->in_pipe,
                   META_MESSAGE_ESCAPE, META_MESSAGE_ESCAPE_LEN) < 0)
    {
      meta_warning ("Failed to write escape sequence: %s\n",
                    g_strerror (errno));
      kill_child (uislave);
    }
  if (write_bytes (uislave->in_pipe,
                   message, message->header.length) < 0)
    {
      meta_warning ("Failed to write message: %s\n",
                    g_strerror (errno));
      kill_child (uislave);
    }
}

void
meta_ui_slave_show_tip (MetaUISlave    *uislave,
                        int             root_x,
                        int             root_y,
                        const char     *markup_text)
{
  MetaMessageShowTip showtip;

  memset (&showtip, 0, META_MESSAGE_LENGTH (MetaMessageShowTip));
  showtip.header.message_code = MetaMessageShowTipCode;
  showtip.header.length = META_MESSAGE_LENGTH (MetaMessageShowTip);

  showtip.root_x = root_x;
  showtip.root_y = root_y;  
  strncpy (showtip.markup, markup_text, META_MESSAGE_MAX_TIP_LEN);
  showtip.markup[META_MESSAGE_MAX_TIP_LEN] = '\0';

  send_message (uislave, (MetaMessage*)&showtip);
}

void
meta_ui_slave_hide_tip (MetaUISlave *uislave)
{
  MetaMessageHideTip hidetip;

  memset (&hidetip, 0, META_MESSAGE_LENGTH (MetaMessageHideTip));
  hidetip.header.message_code = MetaMessageHideTipCode;
  hidetip.header.length = META_MESSAGE_LENGTH (MetaMessageHideTip);

  send_message (uislave, (MetaMessage*)&hidetip);
}

void
meta_ui_slave_show_window_menu (MetaUISlave             *uislave,
                                MetaWindow              *window,
                                int                      root_x,
                                int                      root_y,
                                int                      button,
                                MetaMessageWindowMenuOps ops,
                                MetaMessageWindowMenuOps insensitive,
                                Time                     timestamp)
{
  MetaMessageShowWindowMenu showmenu;

  memset (&showmenu, 0, META_MESSAGE_LENGTH (MetaMessageShowWindowMenu));
  showmenu.header.message_code = MetaMessageShowWindowMenuCode;
  showmenu.header.length = META_MESSAGE_LENGTH (MetaMessageShowWindowMenu);

  showmenu.window = window->xwindow;
  showmenu.root_x = root_x;
  showmenu.root_y = root_y;
  showmenu.button = button;
  showmenu.ops = ops;
  showmenu.insensitive = insensitive;
  showmenu.timestamp = timestamp;
  
  send_message (uislave, (MetaMessage*)&showmenu);
}

void
meta_ui_slave_hide_window_menu (MetaUISlave *uislave)
{
  MetaMessageHideWindowMenu hidemenu;

  memset (&hidemenu, 0, META_MESSAGE_LENGTH (MetaMessageHideWindowMenu));
  hidemenu.header.message_code = MetaMessageHideWindowMenuCode;
  hidemenu.header.length = META_MESSAGE_LENGTH (MetaMessageHideWindowMenu);

  send_message (uislave, (MetaMessage*)&hidemenu);
}
