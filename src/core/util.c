/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity utilities */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2005 Elijah Newren
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
#define _POSIX_C_SOURCE 200112L /* for fdopen() */

#include <config.h>
#include "util.h"
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <X11/Xlib.h>   /* must explicitly be included for Solaris; #326746 */
#include <X11/Xutil.h>  /* Just for the definition of the various gravities */

MetaNexus *sigchld_nexus;

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
void
meta_print_backtrace (void)
{
  void *bt[500];
  int bt_size;
  int i;
  char **syms;
  
  bt_size = backtrace (bt, 500);

  syms = backtrace_symbols (bt, bt_size);
  
  i = 0;
  while (i < bt_size)
    {
      meta_verbose ("  %s\n", syms[i]);
      ++i;
    }

  free (syms);
}
#else
void
meta_print_backtrace (void)
{
  meta_verbose ("Not compiled with backtrace support\n");
}
#endif

static gboolean is_verbose = FALSE;
static gboolean is_debugging = FALSE;
static gboolean replace_current = FALSE;
static int no_prefix = 0;

#ifdef WITH_VERBOSE_MODE
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
          g_printerr (_("Opened log file %s\n"), filename);
        }
      
      g_free (filename);
    }
}
#endif

gboolean
meta_is_verbose (void)
{
  return is_verbose;
}

void
meta_set_verbose (gboolean setting)
{
#ifndef WITH_VERBOSE_MODE
  if (setting)
    meta_fatal (_("Metacity was compiled without support for verbose mode\n"));
#else 
  if (setting)
    ensure_logfile ();
#endif
  
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
#ifdef WITH_VERBOSE_MODE
  if (setting)
    ensure_logfile ();
#endif

  is_debugging = setting;
}

gboolean
meta_get_replace_current_wm (void)
{
  return replace_current;
}

void
meta_set_replace_current_wm (gboolean setting)
{
  replace_current = setting;
}

char *
meta_g_utf8_strndup (const gchar *src,
                     gsize        n)
{
  const gchar *s = src;
  while (n && *s)
    {
      s = g_utf8_next_char (s);
      n--;
    }

  return g_strndup (src, s - src);
}

static int
utf8_fputs (const char *str,
            FILE       *f)
{
  char *l;
  int retval;
  
  l = g_locale_from_utf8 (str, -1, NULL, NULL, NULL);

  if (l == NULL)
    retval = fputs (str, f); /* just print it anyway, better than nothing */
  else
    retval = fputs (l, f);

  g_free (l);

  return retval;
}

void
meta_free_gslist_and_elements (GSList *list_to_deep_free)
{
  g_slist_foreach (list_to_deep_free,
                   (void (*)(gpointer,gpointer))&g_free, /* ew, for ugly */
                   NULL);
  g_slist_free (list_to_deep_free);
}

#ifdef WITH_VERBOSE_MODE
void
meta_debug_spew_real (const char *format, ...)
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
    utf8_fputs (_("Window manager: "), out);
  utf8_fputs (str, out);

  fflush (out);
  
  g_free (str);
}
#endif /* WITH_VERBOSE_MODE */

#ifdef WITH_VERBOSE_MODE
void
meta_verbose_real (const char *format, ...)
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
    utf8_fputs ("Window manager: ", out);
  utf8_fputs (str, out);

  fflush (out);
  
  g_free (str);
}
#endif /* WITH_VERBOSE_MODE */

#ifdef WITH_VERBOSE_MODE
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
    case META_DEBUG_XINERAMA:
      return "XINERAMA";
    case META_DEBUG_KEYBINDINGS:
      return "KEYBINDINGS";
    case META_DEBUG_SYNC:
      return "SYNC";
    case META_DEBUG_ERRORS:
      return "ERRORS";
    case META_DEBUG_STARTUP:
      return "STARTUP";
    case META_DEBUG_PREFS:
      return "PREFS";
    case META_DEBUG_GROUPS:
      return "GROUPS";
    case META_DEBUG_RESIZING:
      return "RESIZING";
    case META_DEBUG_SHAPES:
      return "SHAPES";
    case META_DEBUG_COMPOSITOR:
      return "COMPOSITOR";
    case META_DEBUG_EDGE_RESISTANCE:
      return "EDGE_RESISTANCE";
    }

  return "WM";
}

static int sync_count = 0;

void
meta_topic_real (MetaDebugTopic topic,
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

  if (topic == META_DEBUG_SYNC)
    {
      ++sync_count;
      fprintf (out, "%d: ", sync_count);
    }
  
  utf8_fputs (str, out);
  
  fflush (out);
  
  g_free (str);
}
#endif /* WITH_VERBOSE_MODE */

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

#ifdef WITH_VERBOSE_MODE
  out = logfile ? logfile : stderr;
#else
  out = stderr;
#endif

  if (no_prefix == 0)
    utf8_fputs (_("Bug in window manager: "), out);
  utf8_fputs (str, out);

  fflush (out);
  
  g_free (str);

  meta_print_backtrace ();
  
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

#ifdef WITH_VERBOSE_MODE
  out = logfile ? logfile : stderr;
#else
  out = stderr;
#endif

  if (no_prefix == 0)
    utf8_fputs (_("Window manager warning: "), out);
  utf8_fputs (str, out);

  fflush (out);
  
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

#ifdef WITH_VERBOSE_MODE
  out = logfile ? logfile : stderr;
#else
  out = stderr;
#endif

  if (no_prefix == 0)
    utf8_fputs (_("Window manager error: "), out);
  utf8_fputs (str, out);

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

gint
meta_unsigned_long_equal (gconstpointer v1,
                          gconstpointer v2)
{
  return *((const gulong*) v1) == *((const gulong*) v2);
}

guint
meta_unsigned_long_hash  (gconstpointer v)
{
  gulong val = * (const gulong *) v;

  /* I'm not sure this works so well. */
#if GLIB_SIZEOF_LONG > 4
  return (guint) (val ^ (val >> 32));
#else
  return val;
#endif
}

const char*
meta_gravity_to_string (int gravity)
{
  switch (gravity)
    {
    case NorthWestGravity:
      return "NorthWestGravity";
      break;
    case NorthGravity:
      return "NorthGravity";
      break;
    case NorthEastGravity:
      return "NorthEastGravity";
      break;
    case WestGravity:
      return "WestGravity";
      break;
    case CenterGravity:
      return "CenterGravity";
      break;
    case EastGravity:
      return "EastGravity";
      break;
    case SouthWestGravity:
      return "SouthWestGravity";
      break;
    case SouthGravity:
      return "SouthGravity";
      break;
    case SouthEastGravity:
      return "SouthEastGravity";
      break;
    case StaticGravity:
      return "StaticGravity";
      break;
    default:
      return "NorthWestGravity";
      break;
    }
}

GPid
meta_show_dialog (const char *type,
                  const char *message,
                  const char *timeout,
                  const gint screen_number,
                  const char *ok_text,
                  const char *cancel_text,
                  const int transient_for,
                  const char **columns,
                  const char **entries)
{
  GError *error = NULL;
  char *screen_number_text = g_strdup_printf("%d", screen_number);

  /*
  We want:
  
zenity --display X --screen S --title Metacity --error --text "There was an error running <tt>terminal</tt>:\n\nYour computer is on fire."
  ** with no pipes
  
zenity --display X --screen S --title Metacity --question --text "<big><b><tt>%s</tt> is not responding.</b></big>\n\n<i>You may choose to wait a short while for it to continue or force the application to quit entirely.</i>"

zenity --display X --screen S --title Metacity --list --timeout 240 --text "These windows do not support \"save current setup\" and will have to be restarted manually next time you log in." --column "Window" --column "Class" "Firefox" "foo" "Duke Nukem Forever" "bar"
  */

  const char **argvl;
  char **envl;
  int i=0;
  GPid child_pid;

  argvl = g_malloc(sizeof (char*) * 15);

  argvl[i++] = "zenity";
  argvl[i++] = type;
  argvl[i++] = "--screen";
  argvl[i++] = screen_number_text;
  argvl[i++] = "--title";
  /* Translators: This is the title used on dialog boxes */
  argvl[i++] = _("Metacity");
  argvl[i++] = "--text";
  argvl[i++] = message;
  
  if (timeout)
    {
      argvl[i++] = "--timeout";
      argvl[i++] = timeout;
    }

  if (ok_text)
    {
      argvl[i++] = "--ok-label";
      argvl[i++] = ok_text;
     }

   if (cancel_text)
    {
      argvl[i++] = "--cancel-label";
      argvl[i++] = cancel_text;
     }

  argvl[i] = NULL;

  if (transient_for)
    {
        gchar *env = g_strdup_printf("%d", transient_for);
        setenv ("WINDOWID", env, 1);
        g_free (env);
    }
  else
    envl = NULL;

  g_spawn_async (
                 "/",
                 (gchar**) argvl, /* ugh */
                 NULL,
                 G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                 NULL, NULL,
                 &child_pid,
                 &error
                 );

  g_free (argvl);
  g_free (screen_number_text);

  if (error)
    {
      meta_warning ("%s\n", error->message);
      g_error_free (error);
    }

  return child_pid;
}

GType
meta_nexus_get_type (void)
{
  static GType nexus_type = 0;

  if (!nexus_type)
    {
      static const GTypeInfo nexus_info =
      {
        sizeof (MetaNexusClass),
	NULL, NULL, NULL, NULL, NULL,
	sizeof (MetaNexus),
	0,
	NULL, NULL
      };

      nexus_type = g_type_register_static (G_TYPE_OBJECT,
					   "MetaNexus",
					   &nexus_info,
					   0);
    }

  return nexus_type;
}

/* eof util.c */

