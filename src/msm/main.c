/* msm main() */

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

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "server.h"
#include "util.h"

static GMainLoop *main_loop = NULL;

static void
usage (void)
{
  g_print ("msm [--choose-session=NAME] [--failsafe]\n");
  exit (1);
}

static void
shutdown_cleanly_on_signal (int signo)
{
  if (main_loop && g_main_is_running (main_loop))
    g_main_quit (main_loop);
}

void
msm_quit (void)
{
  if (main_loop && g_main_is_running (main_loop))
    g_main_quit (main_loop);
}

int
main (int argc, char **argv)
{
  int i;
  const char *prev_arg;
  char *session_name;
  gboolean failsafe;
  struct sigaction act;
  sigset_t empty_mask;
  MsmServer *server;
  
  sigemptyset (&empty_mask);
  act.sa_handler = SIG_IGN;
  act.sa_mask    = empty_mask;
  act.sa_flags   = 0;
  sigaction (SIGPIPE, &act, 0);

  act.sa_handler = shutdown_cleanly_on_signal;
  sigaction (SIGHUP, &act, 0);
  sigaction (SIGINT, &act, 0);
  
  /* connect to display */
  gtk_init (&argc, &argv);

  /* Parse options lamely */
  session_name = NULL;
  failsafe = FALSE;
  prev_arg = NULL;
  i = 1;
  while (i < argc)
    {
      const char *arg = argv[i];
      
      if (strcmp (arg, "--help") == 0 ||
          strcmp (arg, "-h") == 0 ||
          strcmp (arg, "-?") == 0)
        usage ();
      else if (strcmp (arg, "--failsafe") == 0)
        failsafe = TRUE; 
      else if (strstr (arg, "--choose-session=") == arg)
        {
          const char *name;

          if (session_name != NULL)
            msm_fatal ("Can't specify session name twice\n");
          
          name = strchr (arg, '=');
          ++name;
          
          session_name = g_strdup (name);
        }
      else if (prev_arg &&
               strcmp (prev_arg, "--choose-session") == 0)
        {
          if (session_name != NULL)
            msm_fatal ("Can't specify session name twice\n");

          session_name = g_strdup (arg);
        }
      else if (strcmp (arg, "--choose-session") == 0)
        ; /* wait for next arg */
      else
        usage ();
      
      prev_arg = arg;
      
      ++i;
    }

  if (failsafe)
    server = msm_server_new_failsafe ();
  else
    server = msm_server_new (session_name);

  msm_server_launch_session (server);
  
  main_loop = g_main_loop_new (NULL, FALSE);
  
  g_main_run (main_loop);
  
  return 0;
}
