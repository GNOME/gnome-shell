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

#include <glib-object.h>

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static MetaExitCode meta_exit_code = META_EXIT_SUCCESS;
static GMainLoop *meta_main_loop = NULL;

int
main (int argc, char **argv)
{
  struct sigaction act;
  sigset_t empty_mask;
  char *display_name;

  sigemptyset (&empty_mask);
  act.sa_handler = SIG_IGN;
  act.sa_mask    = empty_mask;
  act.sa_flags   = 0;
  sigaction (SIGPIPE,  &act, 0);
  
  g_set_prgname (PACKAGE);
  
  meta_main_loop = g_main_loop_new (NULL, FALSE);
  
  meta_set_verbose (TRUE);
  meta_set_debugging (TRUE);
  meta_set_syncing (g_getenv ("METACITY_SYNC") != NULL);

  if (g_getenv ("METACITY_DISPLAY"))
    {
      meta_verbose ("Using METACITY_DISPLAY %s\n",
                    g_getenv ("METACITY_DISPLAY"));
      display_name =
        g_strconcat ("DISPLAY=", g_getenv ("METACITY_DISPLAY"), NULL);
      putenv (display_name);
      /* DO NOT FREE display_name, putenv() sucks */
    }


  /* gtk_init() below overrides this, so it can be removed */
  meta_errors_init ();
  
  g_type_init (0); /* grumble */
  meta_ui_init (&argc, &argv);
  
  if (!meta_display_open (NULL))
    meta_exit (META_EXIT_ERROR);
  
  g_main_run (meta_main_loop);

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
  
  g_main_quit (meta_main_loop);
}

void
meta_exit (MetaExitCode code)
{
  
  exit (code);
}

