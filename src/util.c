/* Metacity utilities */

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

#include "util.h"
#include "main.h"
#include "display.h"

#include <stdio.h>
#include <stdlib.h>

static gboolean is_verbose = FALSE;
static gboolean is_debugging = FALSE;
static gboolean is_syncing = FALSE;
static int no_prefix = 0;
static FILE* logfile = NULL;

static void
ensure_logfile (void)
{
  if (logfile == NULL)
    {
      const char *dir;
      char *str;
      
      dir = g_get_home_dir ();
      str = g_strconcat (dir, "/", "metacity.log", NULL);
      
      logfile = fopen (str, "w");

      if (logfile == NULL)
        meta_warning ("Failed to open log file %s\n", str);
      else
        meta_verbose ("Opened log file %s\n", str);
      
      g_free (str);
    }
}

gboolean
meta_is_verbose (void)
{
  return is_verbose;
}

void
meta_set_verbose (gboolean setting)
{
  if (setting)
    ensure_logfile ();
  
  is_verbose = setting;
}

gboolean
meta_is_debugging (void)
{
  return is_debugging;
}

void
meta_set_debugging (gboolean setting)
{
  if (setting)
    ensure_logfile ();
  
  is_debugging = setting;
}

gboolean
meta_is_syncing (void)
{
  return is_syncing;
}

void
meta_set_syncing (gboolean setting)
{
  if (setting != is_syncing)
    {
      GSList *tmp;
      
      is_syncing = setting;

      tmp = meta_displays_list ();
      while (tmp != NULL)
        {
          MetaDisplay *display = tmp->data;
          XSynchronize (display->xdisplay, is_syncing);
          tmp = tmp->next;
        }
    }
}

void
meta_debug_spew (const char *format, ...)
{
  va_list args;
  gchar *str;
  FILE *out;
  
  g_return_if_fail (format != NULL);

  if (!is_debugging)
    return;
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  out = logfile ? logfile : stderr;
  
  if (no_prefix == 0)
    fputs ("Window manager: ", out);
  fputs (str, out);
  
  g_free (str);
}

void
meta_verbose (const char *format, ...)
{
  va_list args;
  gchar *str;
  FILE *out;

  g_return_if_fail (format != NULL);

  if (!is_verbose)
    return;
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  out = logfile ? logfile : stderr;
  
  if (no_prefix == 0)
    fputs ("Window manager: ", out);
  fputs (str, out);
  
  g_free (str);
}

void
meta_bug (const char *format, ...)
{
  va_list args;
  gchar *str;
  FILE *out;

  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  out = logfile ? logfile : stderr;
  
  if (no_prefix == 0)
    fputs ("Bug in window manager: ", out);
  fputs (str, out);
  
  g_free (str);

  /* stop us in a debugger */
  abort ();
}

void
meta_warning (const char *format, ...)
{
  va_list args;
  gchar *str;
  FILE *out;
  
  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  out = logfile ? logfile : stderr;
  
  if (no_prefix == 0)
    fputs ("Window manager: ", out);
  fputs (str, out);
  
  g_free (str);
}

void
meta_fatal (const char *format, ...)
{
  va_list args;
  gchar *str;
  FILE *out;
  
  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  out = logfile ? logfile : stderr;
  
  if (no_prefix == 0)
    fputs ("Window manager: ", out);
  fputs (str, out);
  
  g_free (str);

  meta_exit (META_EXIT_ERROR);
}

void
meta_push_no_msg_prefix (void)
{
  ++no_prefix;
}

void
meta_pop_no_msg_prefix (void)
{
  g_return_if_fail (no_prefix > 0);

  --no_prefix;
}
