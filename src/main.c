/* Metacity main() */

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

#include "main.h"
#include "util.h"
#include "display.h"
#include "errors.h"
#include "ui.h"
#include "session.h"

#include <glib-object.h>

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static MetaExitCode meta_exit_code = META_EXIT_SUCCESS;
static GMainLoop *meta_main_loop = NULL;

static void
usage (void)
{
  g_print ("metacity [--disable-sm] [--sm-client-id=ID] [--display=DISPLAY]\n");
  exit (0);
}

int
main (int argc, char **argv)
{
  struct sigaction act;
  sigset_t empty_mask;
  char *display_name;
  int i;
  const char *client_id;
  gboolean disable_sm;
  const char *prev_arg;
  
  sigemptyset (&empty_mask);
  act.sa_handler = SIG_IGN;
  act.sa_mask    = empty_mask;
  act.sa_flags   = 0;
  sigaction (SIGPIPE,  &act, 0);
  
  g_set_prgname (PACKAGE);
  
  meta_set_verbose (TRUE);
  meta_set_debugging (TRUE);
  meta_set_syncing (g_getenv ("METACITY_SYNC") != NULL);

  /* Parse options lamely */

  display_name = NULL;
  client_id = NULL;
  disable_sm = FALSE;
  prev_arg = NULL;
  i = 1;
  while (i < argc)
    {
      const char *arg = argv[i];
      
      if (strcmp (arg, "--help") == 0 ||
          strcmp (arg, "-h") == 0 ||
          strcmp (arg, "-?") == 0)
        usage ();
      else if (strcmp (arg, "--sm-disable") == 0)
        disable_sm = TRUE; 
      else if (strstr (arg, "--display=") == arg)
        {
          const char *disp;

          if (display_name != NULL)
            meta_fatal ("Can't specify display twice\n");
          
          disp = strchr (arg, '=');
          ++disp;
          
          display_name =
            g_strconcat ("DISPLAY=", disp, NULL);
        }
      else if (prev_arg &&
               strcmp (prev_arg, "--display") == 0)
        {
          if (display_name != NULL)
            meta_fatal ("Can't specify display twice\n");

          display_name = g_strconcat ("DISPLAY=", arg, NULL);
        }
      else if (strcmp (arg, "--display") == 0)
        ; /* wait for next arg */
      else if (strstr (arg, "--sm-client-id=") == arg)
        {
          const char *id;

          if (client_id)
            meta_fatal ("Can't specify client ID twice\n");
          
          id = strchr (arg, '=');
          ++id;

          client_id = g_strdup (id);
        }
      else if (prev_arg &&
               strcmp (prev_arg, "--sm-client-id") == 0)
        {
          if (client_id)
            meta_fatal ("Can't specify client ID twice\n");

          client_id = g_strdup (arg);
        }
      else if (strcmp (arg, "--sm-client-id") == 0)
        ; /* wait for next arg */
      else
        usage ();
      
      prev_arg = arg;
      
      ++i;
    }
    
  meta_main_loop = g_main_loop_new (NULL, FALSE);
  
  if (display_name == NULL &&
      g_getenv ("METACITY_DISPLAY"))
    {
      meta_verbose ("Using METACITY_DISPLAY %s\n",
                    g_getenv ("METACITY_DISPLAY"));
      display_name =
        g_strconcat ("DISPLAY=", g_getenv ("METACITY_DISPLAY"), NULL);
    }

  if (display_name)
    {
      putenv (display_name);
      /* DO NOT FREE display_name, putenv() sucks */
    }
  
  /* gtk_init() below overrides this, so it can be removed */
  meta_errors_init ();
  
  g_type_init (0); /* grumble */

  if (!disable_sm)
    meta_session_init (client_id); /* client_id == NULL is fine */
  
  meta_ui_init (&argc, &argv);  
  
  if (!meta_display_open (NULL))
    meta_exit (META_EXIT_ERROR);
  
  g_main_run (meta_main_loop);

  {
    GSList *displays;
    GSList *tmp;
    
    displays = meta_displays_list ();
    tmp = displays;
    while (tmp != NULL)
      {
        meta_display_close (tmp->data);
        tmp = tmp->next;
      }
  }
  
  return meta_exit_code;
}

GMainLoop*
meta_get_main_loop (void)
{
  return meta_main_loop;
}

void
meta_quit (MetaExitCode code)
{
  meta_exit_code = code;

  if (g_main_is_running (meta_main_loop))
    g_main_quit (meta_main_loop);
}

void
meta_exit (MetaExitCode code)
{
  
  exit (code);
}

