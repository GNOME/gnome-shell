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

#include <config.h>
#include "util.h"
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static gboolean is_verbose = FALSE;
static gboolean is_debugging = FALSE;
static int no_prefix = 0;
static FILE* logfile = NULL;

static void
ensure_logfile (void)
{
  if (logfile == NULL && g_getenv ("METACITY_USE_LOGFILE"))
    {
      char *filename = NULL;
      char *tmpl;
      int fd;
      GError *err;
      
      tmpl = g_strdup_printf ("metacity-%d-debug-log-XXXXXX",
                              (int) getpid ());

      err = NULL;
      fd = g_file_open_tmp (tmpl,
                            &filename,
                            &err);

      g_free (tmpl);
      
      if (err != NULL)
        {
          meta_warning (_("Failed to open debug log: %s\n"),
                        err->message);
          g_error_free (err);
          return;
        }
      
      logfile = fdopen (fd, "w");
      
      if (logfile == NULL)
        {
          meta_warning (_("Failed to fdopen() log file %s: %s\n"),
                        filename, strerror (errno));
          close (fd);
        }
      else
        {
          g_print (_("Opened log file %s\n"), filename);
        }
      
      g_free (filename);
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

  fflush (out);
  
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

  fflush (out);
  
  g_free (str);
}

static const char*
topic_name (MetaDebugTopic topic)
{
  switch (topic)
    {
    case META_DEBUG_FOCUS:
      return "FOCUS";
    case META_DEBUG_WORKAREA:
      return "WORKAREA";
    case META_DEBUG_STACK:
      return "STACK";
    case META_DEBUG_THEMES:
      return "THEMES";
    case META_DEBUG_SM:
      return "SM";
    case META_DEBUG_EVENTS:
      return "EVENTS";
    case META_DEBUG_WINDOW_STATE:
      return "WINDOW_STATE";
    case META_DEBUG_WINDOW_OPS:
      return "WINDOW_OPS";
    case META_DEBUG_PLACEMENT:
      return "PLACEMENT";
    case META_DEBUG_GEOMETRY:
      return "GEOMETRY";
    case META_DEBUG_PING:
      return "PING";
    }

  return "Window manager";
}

void
meta_topic (MetaDebugTopic topic,
            const char *format,
            ...)
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
    fprintf (out, "%s: ", topic_name (topic));
  fputs (str, out);
  
  fflush (out);
  
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

  fflush (out);
  
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
    fputs ("Window manager warning: ", out);
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
    fputs ("Window manager error: ", out);
  fputs (str, out);

  fflush (out);
  
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

void
meta_exit (MetaExitCode code)
{
  
  exit (code);
}
