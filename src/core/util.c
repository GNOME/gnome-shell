/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:util
 * @title: Utility functions
 * @short_description: Miscellaneous utility functions
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L /* for fdopen() */

#include <config.h>
#include <meta/common.h>
#include "util-private.h"
#include <meta/main.h>

#include <clutter/clutter.h> /* For clutter_threads_add_repaint_func() */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <X11/Xlib.h>   /* must explicitly be included for Solaris; #326746 */
#include <X11/Xutil.h>  /* Just for the definition of the various gravities */

#ifdef WITH_VERBOSE_MODE
static void
meta_topic_real_valist (MetaDebugTopic topic,
                        const char    *format,
                        va_list        args);
#endif

static gint verbose_topics = 0;
static gboolean is_debugging = FALSE;
static gboolean replace_current = FALSE;
static int no_prefix = 0;

#ifdef WITH_VERBOSE_MODE
static FILE* logfile = NULL;

static void
ensure_logfile (void)
{
  if (logfile == NULL && g_getenv ("MUTTER_USE_LOGFILE"))
    {
      char *filename = NULL;
      char *tmpl;
      int fd;
      GError *err;
      
      tmpl = g_strdup_printf ("mutter-%d-debug-log-XXXXXX",
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
  return verbose_topics != 0;
}

void
meta_set_verbose (gboolean setting)
{
#ifndef WITH_VERBOSE_MODE
  if (setting)
    meta_fatal (_("Mutter was compiled without support for verbose mode\n"));
#else 
  if (setting)
    ensure_logfile ();
#endif

  if (setting)
    meta_add_verbose_topic (META_DEBUG_VERBOSE);
  else
    meta_remove_verbose_topic (META_DEBUG_VERBOSE);
}

/**
 * meta_add_verbose_topic:
 * @topic: Topic for which logging will be started
 *
 * Ensure log messages for the given topic @topic
 * will be printed.
 */
void
meta_add_verbose_topic (MetaDebugTopic topic)
{
  if (verbose_topics == META_DEBUG_VERBOSE)
    return;
  if (topic == META_DEBUG_VERBOSE)
    verbose_topics = META_DEBUG_VERBOSE;
  else
    verbose_topics |= topic;
}

/**
 * meta_remove_verbose_topic:
 * @topic: Topic for which logging will be stopped
 *
 * Stop printing log messages for the given topic @topic.  Note
 * that this method does not stack with meta_add_verbose_topic();
 * i.e. if two calls to meta_add_verbose_topic() for the same
 * topic are made, one call to meta_remove_verbose_topic() will
 * remove it.
 */
void
meta_remove_verbose_topic (MetaDebugTopic topic)
{
  if (topic == META_DEBUG_VERBOSE)
    verbose_topics = 0;
  else
    verbose_topics &= ~topic;
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

/**
 * meta_free_gslist_and_elements: (skip)
 * @list_to_deep_free: list to deep free
 *
 */
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

  va_start (args, format);
  meta_topic_real_valist (META_DEBUG_VERBOSE, format, args);
  va_end (args);
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
    case META_DEBUG_DBUS:
      return "DBUS";
    case META_DEBUG_VERBOSE:
      return "VERBOSE";
    }

  return "WM";
}

static int sync_count = 0;

static void
meta_topic_real_valist (MetaDebugTopic topic,
                        const char    *format,
                        va_list        args)
{
  gchar *str;
  FILE *out;

  g_return_if_fail (format != NULL);

  if (verbose_topics == 0
      || (topic == META_DEBUG_VERBOSE && verbose_topics != META_DEBUG_VERBOSE)
      || (!(verbose_topics & topic)))
    return;

  str = g_strdup_vprintf (format, args);

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

void
meta_topic_real (MetaDebugTopic topic,
                 const char *format,
                 ...)
{
  va_list args;

  va_start (args, format);
  meta_topic_real_valist (topic, format, args);
  va_end (args);
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

char*
meta_external_binding_name_for_action (guint keybinding_action)
{
  return g_strdup_printf ("external-grab-%u", keybinding_action);
}

static gboolean
zenity_supports_option (const char *section, const char *option)
{
  char *command, *out;
  gboolean rv;

  command = g_strdup_printf ("zenity %s", section);
  g_spawn_command_line_sync (command, &out, NULL, NULL, NULL);

  rv = (out && strstr (out, option));

  g_free (command);
  g_free (out);

  return rv;
}

/* Command line arguments are passed in the locale encoding; in almost
 * all cases, we'd hope that is UTF-8 and no conversion is necessary.
 * If it's not UTF-8, then it's possible that the message isn't
 * representable in the locale encoding.
 */
static void
append_argument (GPtrArray  *args,
                 const char *arg)
{
  char *locale_arg = g_locale_from_utf8 (arg, -1, NULL, NULL, NULL);

  /* This is cheesy, but it's better to have a few ???'s in the dialog
   * for an unresponsive application than no dialog at all appear */
  if (!locale_arg)
    locale_arg = g_strdup ("???");

  g_ptr_array_add (args, locale_arg);
}

/**
 * meta_show_dialog: (skip)
 * @type: type of dialog
 * @message: message
 * @timeout: timeout
 * @display: display
 * @ok_text: text for Ok button
 * @cancel_text: text for Cancel button
 * @icon_name: icon name
 * @transient_for: window XID of parent
 * @columns: columns
 * @entries: entries
 *
 */
GPid
meta_show_dialog (const char *type,
                  const char *message,
                  const char *timeout,
                  const char *display,
                  const char *ok_text,
                  const char *cancel_text,
                  const char *icon_name,
                  const int transient_for,
                  GSList *columns,
                  GSList *entries)
{
  GError *error = NULL;
  GSList *tmp;
  GPid child_pid;
  GPtrArray *args;

  args = g_ptr_array_new ();

  append_argument (args, "zenity");
  append_argument (args, type);

  if (display)
    {
      append_argument (args, "--display");
      append_argument (args, display);
    }

  append_argument (args, "--class");
  append_argument (args, "mutter-dialog");
  append_argument (args, "--title");
  append_argument (args, "");
  append_argument (args, "--text");
  append_argument (args, message);

  if (timeout)
    {
      append_argument (args, "--timeout");
      append_argument (args, timeout);
    }

  if (ok_text)
    {
      append_argument (args, "--ok-label");
      append_argument (args, ok_text);
     }

  if (cancel_text)
    {
      append_argument (args, "--cancel-label");
      append_argument (args, cancel_text);
    }

  if (icon_name)
    {
      char *option = g_strdup_printf ("--help%s", type + 1);
      if (zenity_supports_option (option, "--icon-name"))
        {
          append_argument (args, "--icon-name");
          append_argument (args, icon_name);
        }
      g_free (option);
    }

  tmp = columns;
  while (tmp)
    {
      append_argument (args, "--column");
      append_argument (args, tmp->data);
      tmp = tmp->next;
    }

  tmp = entries;
  while (tmp)
    {
      append_argument (args, tmp->data);
      tmp = tmp->next;
    }

  if (transient_for)
    {
      gchar *env = g_strdup_printf("%d", transient_for);
      setenv ("WINDOWID", env, 1);
      g_free (env);

      if (zenity_supports_option ("--help-general", "--modal"))
        append_argument (args, "--modal");
    }

  g_ptr_array_add (args, NULL); /* NULL-terminate */

  g_spawn_async (
                 "/",
                 (gchar**) args->pdata,
                 NULL,
                 G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                 NULL, NULL,
                 &child_pid,
                 &error
                 );

  if (transient_for)
    unsetenv ("WINDOWID");

  g_ptr_array_free (args, TRUE);

  if (error)
    {
      meta_warning ("%s\n", error->message);
      g_error_free (error);
    }

  return child_pid;
}

/***************************************************************************
 * Later functions: like idles but integrated with the Clutter repaint loop
 ***************************************************************************/

static guint last_later_id = 0;

typedef struct
{
  guint id;
  guint ref_count;
  MetaLaterType when;
  GSourceFunc func;
  gpointer data;
  GDestroyNotify notify;
  int source;
  gboolean run_once;
} MetaLater;

static GSList *laters = NULL;
/* This is a dummy timeline used to get the Clutter master clock running */
static ClutterTimeline *later_timeline;
static guint later_repaint_func = 0;

static void ensure_later_repaint_func (void);

static void
unref_later (MetaLater *later)
{
  if (--later->ref_count == 0)
    {
      if (later->notify)
        {
          later->notify (later->data);
          later->notify = NULL;
        }
      g_slice_free (MetaLater, later);
    }
}

static void
destroy_later (MetaLater *later)
{
  if (later->source)
    {
      g_source_remove (later->source);
      later->source = 0;
    }
  later->func = NULL;
  unref_later (later);
}

/* Used to sort the list of laters with the highest priority
 * functions first.
 */
static int
compare_laters (gconstpointer a,
                gconstpointer b)
{
  return ((const MetaLater *)a)->when - ((const MetaLater *)b)->when;
}

static gboolean
run_repaint_laters (gpointer data)
{
  GSList *laters_copy;
  GSList *l;
  gboolean keep_timeline_running = FALSE;

  laters_copy = NULL;
  for (l = laters; l; l = l->next)
    {
      MetaLater *later = l->data;
      if (later->source == 0 ||
          (later->when <= META_LATER_BEFORE_REDRAW && !later->run_once))
        {
          later->ref_count++;
          laters_copy = g_slist_prepend (laters_copy, later);
        }
    }
  laters_copy = g_slist_reverse (laters_copy);

  for (l = laters_copy; l; l = l->next)
    {
      MetaLater *later = l->data;

      if (later->func && later->func (later->data))
        {
          if (later->source == 0)
            keep_timeline_running = TRUE;
        }
      else
        meta_later_remove (later->id);
      unref_later (later);
    }

  if (!keep_timeline_running)
    clutter_timeline_stop (later_timeline);

  g_slist_free (laters_copy);

  /* Just keep the repaint func around - it's cheap if the list is empty */
  return TRUE;
}

static void
ensure_later_repaint_func (void)
{
  if (!later_timeline)
    later_timeline = clutter_timeline_new (G_MAXUINT);

  if (later_repaint_func == 0)
    later_repaint_func = clutter_threads_add_repaint_func (run_repaint_laters,
                                                           NULL, NULL);

  /* Make sure the repaint function gets run */
  clutter_timeline_start (later_timeline);
}

static gboolean
call_idle_later (gpointer data)
{
  MetaLater *later = data;

  if (!later->func (later->data))
    {
      meta_later_remove (later->id);
      return FALSE;
    }
  else
    {
      later->run_once = TRUE;
      return TRUE;
    }
}

/**
 * meta_later_add:
 * @when:     enumeration value determining the phase at which to run the callback
 * @func:     callback to run later
 * @data:     data to pass to the callback
 * @notify:   function to call to destroy @data when it is no longer in use, or %NULL
 *
 * Sets up a callback  to be called at some later time. @when determines the
 * particular later occasion at which it is called. This is much like g_idle_add(),
 * except that the functions interact properly with clutter event handling.
 * If a "later" function is added from a clutter event handler, and is supposed
 * to be run before the stage is redrawn, it will be run before that redraw
 * of the stage, not the next one.
 *
 * Return value: an integer ID (guaranteed to be non-zero) that can be used
 *  to cancel the callback and prevent it from being run.
 */
guint
meta_later_add (MetaLaterType  when,
                GSourceFunc    func,
                gpointer       data,
                GDestroyNotify notify)
{
  MetaLater *later = g_slice_new0 (MetaLater);

  later->id = ++last_later_id;
  later->ref_count = 1;
  later->when = when;
  later->func = func;
  later->data = data;
  later->notify = notify;

  laters = g_slist_insert_sorted (laters, later, compare_laters);

  switch (when)
    {
    case META_LATER_RESIZE:
      /* We add this one two ways - as a high-priority idle and as a
       * repaint func. If we are in a clutter event callback, the repaint
       * handler will get hit first, and we'll take care of this function
       * there so it gets called before the stage is redrawn, even if
       * we haven't gotten back to the main loop. Otherwise, the idle
       * handler will get hit first and we want to call this function
       * there so it will happen before GTK+ repaints.
       */
      later->source = g_idle_add_full (META_PRIORITY_RESIZE, call_idle_later, later, NULL);
      ensure_later_repaint_func ();
      break;
    case META_LATER_CALC_SHOWING:
    case META_LATER_CHECK_FULLSCREEN:
    case META_LATER_SYNC_STACK:
    case META_LATER_BEFORE_REDRAW:
      ensure_later_repaint_func ();
      break;
    case META_LATER_IDLE:
      later->source = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, call_idle_later, later, NULL);
      break;
    }

  return later->id;
}

/**
 * meta_later_remove:
 * @later_id: the integer ID returned from meta_later_add()
 *
 * Removes a callback added with meta_later_add()
 */
void
meta_later_remove (guint later_id)
{
  GSList *l;

  for (l = laters; l; l = l->next)
    {
      MetaLater *later = l->data;
      if (later->id == later_id)
        {
          laters = g_slist_delete_link (laters, l);
          /* If this was a "repaint func" later, we just let the
           * repaint func run and get removed
           */
          destroy_later (later);
          return;
        }
    }
}

/* eof util.c */

