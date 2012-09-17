/* GLib testing utilities
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik, Sven Herzberg
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gtestutils.h"

#include <sys/types.h>
#ifdef G_OS_UNIX
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif
#include <errno.h>
#include <signal.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#include "gmain.h"
#include "gstrfuncs.h"


/* Global variable for storing assertion messages; this is the counterpart to
 * glibc's (private) __abort_msg variable, and allows developers and crash
 * analysis systems like Apport and ABRT to fish out assertion messages from
 * core dumps, instead of having to catch them on screen output. */
char *__glib_assert_msg = NULL;

static guint8*  g_test_log_dump                 (GTestLogMsg *msg,
                                                 guint       *len);

/* --- variables --- */
static int         test_log_fd = -1;
static int         test_trap_last_pid = 0;
static gboolean    test_debug_log = FALSE;

/* --- functions --- */
const char*
g_test_log_type_name (GTestLogType log_type)
{
  switch (log_type)
    {
    case G_TEST_LOG_NONE:               return "none";
    case G_TEST_LOG_ERROR:              return "error";
    }
  return "???";
}

static void
g_test_log_send (guint         n_bytes,
                 const guint8 *buffer)
{
  if (test_log_fd >= 0)
    {
      int r;
      do
        r = write (test_log_fd, buffer, n_bytes);
      while (r < 0 && errno == EINTR);
    }
  if (test_debug_log)
    {
      GTestLogBuffer *lbuffer = g_test_log_buffer_new ();
      GTestLogMsg *msg;
      guint ui;
      g_test_log_buffer_push (lbuffer, n_bytes, buffer);
      msg = g_test_log_buffer_pop (lbuffer);
      g_warn_if_fail (msg != NULL);
      g_warn_if_fail (lbuffer->data->len == 0);
      g_test_log_buffer_free (lbuffer);
      /* print message */
      g_printerr ("{*LOG(%s)", g_test_log_type_name (msg->log_type));
      for (ui = 0; ui < msg->n_strings; ui++)
        g_printerr (":{%s}", msg->strings[ui]);
      if (msg->n_nums)
        {
          g_printerr (":(");
          for (ui = 0; ui < msg->n_nums; ui++)
            g_printerr ("%s%.16Lg", ui ? ";" : "", msg->nums[ui]);
          g_printerr (")");
        }
      g_printerr (":LOG*}\n");
      g_test_log_msg_free (msg);
    }
}

static void
g_test_log (GTestLogType lbit,
            const gchar *string1,
            const gchar *string2,
            guint        n_args,
            long double *largs)
{
  GTestLogMsg msg;
  gchar *astrings[3] = { NULL, NULL, NULL };
  guint8 *dbuffer;
  guint32 dbufferlen;

  msg.log_type = lbit;
  msg.n_strings = (string1 != NULL) + (string1 && string2);
  msg.strings = astrings;
  astrings[0] = (gchar*) string1;
  astrings[1] = astrings[0] ? (gchar*) string2 : NULL;
  msg.n_nums = n_args;
  msg.nums = largs;
  dbuffer = g_test_log_dump (&msg, &dbufferlen);
  g_test_log_send (dbufferlen, dbuffer);
  g_free (dbuffer);
}

void
g_assertion_message (const char     *domain,
                     const char     *file,
                     int             line,
                     const char     *func,
                     const char     *message)
{
  char lstr[32];
  char *s;

  if (!message)
    message = "code should not be reached";
  g_snprintf (lstr, 32, "%d", line);
  s = g_strconcat (domain ? domain : "", domain && domain[0] ? ":" : "",
                   "ERROR:", file, ":", lstr, ":",
                   func, func[0] ? ":" : "",
                   " ", message, NULL);
  g_printerr ("**\n%s\n", s);

  /* store assertion message in global variable, so that it can be found in a
   * core dump */
  if (__glib_assert_msg != NULL)
      /* free the old one */
      free (__glib_assert_msg);
  __glib_assert_msg = (char*) malloc (strlen (s) + 1);
  strcpy (__glib_assert_msg, s);

  g_test_log (G_TEST_LOG_ERROR, s, NULL, 0, NULL);
  g_free (s);
  abort();
}

void
g_assertion_message_expr (const char     *domain,
                          const char     *file,
                          int             line,
                          const char     *func,
                          const char     *expr)
{
  char *s = g_strconcat ("assertion failed: (", expr, ")", NULL);
  g_assertion_message (domain, file, line, func, s);
  g_free (s);
}

void
g_assertion_message_cmpnum (const char     *domain,
                            const char     *file,
                            int             line,
                            const char     *func,
                            const char     *expr,
                            long double     arg1,
                            const char     *cmp,
                            long double     arg2,
                            char            numtype)
{
  char *s = NULL;
  switch (numtype)
    {
    case 'i':   s = g_strdup_printf ("assertion failed (%s): (%.0Lf %s %.0Lf)", expr, arg1, cmp, arg2); break;
    case 'x':   s = g_strdup_printf ("assertion failed (%s): (0x%08" G_GINT64_MODIFIER "x %s 0x%08" G_GINT64_MODIFIER "x)", expr, (guint64) arg1, cmp, (guint64) arg2); break;
    case 'f':   s = g_strdup_printf ("assertion failed (%s): (%.9Lg %s %.9Lg)", expr, arg1, cmp, arg2); break;
      /* ideally use: floats=%.7g double=%.17g */
    }
  g_assertion_message (domain, file, line, func, s);
  g_free (s);
}

void
g_assertion_message_cmpstr (const char     *domain,
                            const char     *file,
                            int             line,
                            const char     *func,
                            const char     *expr,
                            const char     *arg1,
                            const char     *cmp,
                            const char     *arg2)
{
  char *a1, *a2, *s, *t1 = NULL, *t2 = NULL;
  a1 = arg1 ? g_strconcat ("\"", t1 = g_strescape (arg1, NULL), "\"", NULL) : g_strdup ("NULL");
  a2 = arg2 ? g_strconcat ("\"", t2 = g_strescape (arg2, NULL), "\"", NULL) : g_strdup ("NULL");
  g_free (t1);
  g_free (t2);
  s = g_strdup_printf ("assertion failed (%s): (%s %s %s)", expr, a1, cmp, a2);
  g_free (a1);
  g_free (a2);
  g_assertion_message (domain, file, line, func, s);
  g_free (s);
}

void
g_assertion_message_error (const char     *domain,
			   const char     *file,
			   int             line,
			   const char     *func,
			   const char     *expr,
			   const GError   *error,
			   GQuark          error_domain,
			   int             error_code)
{
  GString *gstring;

  /* This is used by both g_assert_error() and g_assert_no_error(), so there
   * are three cases: expected an error but got the wrong error, expected
   * an error but got no error, and expected no error but got an error.
   */

  gstring = g_string_new ("assertion failed ");
  if (error_domain)
      g_string_append_printf (gstring, "(%s == (%s, %d)): ", expr,
			      g_quark_to_string (error_domain), error_code);
  else
    g_string_append_printf (gstring, "(%s == NULL): ", expr);

  if (error)
      g_string_append_printf (gstring, "%s (%s, %d)", error->message,
			      g_quark_to_string (error->domain), error->code);
  else
    g_string_append_printf (gstring, "%s is NULL", expr);

  g_assertion_message (domain, file, line, func, gstring->str);
  g_string_free (gstring, TRUE);
}

/**
 * g_strcmp0:
 * @str1: a C string or %NULL
 * @str2: another C string or %NULL
 *
 * Compares @str1 and @str2 like strcmp(). Handles %NULL 
 * gracefully by sorting it before non-%NULL strings.
 * Comparing two %NULL pointers returns 0.
 *
 * Returns: -1, 0 or 1, if @str1 is <, == or > than @str2.
 *
 * Since: 2.16
 */
int
g_strcmp0 (const char     *str1,
           const char     *str2)
{
  if (!str1)
    return -(str1 != str2);
  if (!str2)
    return str1 != str2;
  return strcmp (str1, str2);
}

static inline int
g_string_must_read (GString *gstring,
                    int      fd)
{
#define STRING_BUFFER_SIZE     4096
  char buf[STRING_BUFFER_SIZE];
  gssize bytes;
 again:
  bytes = read (fd, buf, sizeof (buf));
  if (bytes == 0)
    return 0; /* EOF, calling this function assumes data is available */
  else if (bytes > 0)
    {
      g_string_append_len (gstring, buf, bytes);
      return 1;
    }
  else if (bytes < 0 && errno == EINTR)
    goto again;
  else /* bytes < 0 */
    {
      g_warning ("failed to read() from child process (%d): %s", test_trap_last_pid, g_strerror (errno));
      return 1; /* ignore error after warning */
    }
}

static inline void
g_string_write_out (GString *gstring,
                    int      outfd,
                    int     *stringpos)
{
  if (*stringpos < gstring->len)
    {
      int r;
      do
        r = write (outfd, gstring->str + *stringpos, gstring->len - *stringpos);
      while (r < 0 && errno == EINTR);
      *stringpos += MAX (r, 0);
    }
}

static void
gstring_overwrite_int (GString *gstring,
                       guint    pos,
                       guint32  vuint)
{
  vuint = g_htonl (vuint);
  g_string_overwrite_len (gstring, pos, (const gchar*) &vuint, 4);
}

static void
gstring_append_int (GString *gstring,
                    guint32  vuint)
{
  vuint = g_htonl (vuint);
  g_string_append_len (gstring, (const gchar*) &vuint, 4);
}

static void
gstring_append_double (GString *gstring,
                       double   vdouble)
{
  union { double vdouble; guint64 vuint64; } u;
  u.vdouble = vdouble;
  u.vuint64 = GUINT64_TO_BE (u.vuint64);
  g_string_append_len (gstring, (const gchar*) &u.vuint64, 8);
}

static guint8*
g_test_log_dump (GTestLogMsg *msg,
                 guint       *len)
{
  GString *gstring = g_string_sized_new (1024);
  guint ui;
  gstring_append_int (gstring, 0);              /* message length */
  gstring_append_int (gstring, msg->log_type);
  gstring_append_int (gstring, msg->n_strings);
  gstring_append_int (gstring, msg->n_nums);
  gstring_append_int (gstring, 0);      /* reserved */
  for (ui = 0; ui < msg->n_strings; ui++)
    {
      guint l = strlen (msg->strings[ui]);
      gstring_append_int (gstring, l);
      g_string_append_len (gstring, msg->strings[ui], l);
    }
  for (ui = 0; ui < msg->n_nums; ui++)
    gstring_append_double (gstring, msg->nums[ui]);
  *len = gstring->len;
  gstring_overwrite_int (gstring, 0, *len);     /* message length */
  return (guint8*) g_string_free (gstring, FALSE);
}

static inline long double
net_double (const gchar **ipointer)
{
  union { guint64 vuint64; double vdouble; } u;
  guint64 aligned_int64;
  memcpy (&aligned_int64, *ipointer, 8);
  *ipointer += 8;
  u.vuint64 = GUINT64_FROM_BE (aligned_int64);
  return u.vdouble;
}

static inline guint32
net_int (const gchar **ipointer)
{
  guint32 aligned_int;
  memcpy (&aligned_int, *ipointer, 4);
  *ipointer += 4;
  return g_ntohl (aligned_int);
}

static gboolean
g_test_log_extract (GTestLogBuffer *tbuffer)
{
  const gchar *p = tbuffer->data->str;
  GTestLogMsg msg;
  guint mlength;
  if (tbuffer->data->len < 4 * 5)
    return FALSE;
  mlength = net_int (&p);
  if (tbuffer->data->len < mlength)
    return FALSE;
  msg.log_type = net_int (&p);
  msg.n_strings = net_int (&p);
  msg.n_nums = net_int (&p);
  if (net_int (&p) == 0)
    {
      guint ui;
      msg.strings = g_new0 (gchar*, msg.n_strings + 1);
      msg.nums = g_new0 (long double, msg.n_nums);
      for (ui = 0; ui < msg.n_strings; ui++)
        {
          guint sl = net_int (&p);
          msg.strings[ui] = g_strndup (p, sl);
          p += sl;
        }
      for (ui = 0; ui < msg.n_nums; ui++)
        msg.nums[ui] = net_double (&p);
      if (p <= tbuffer->data->str + mlength)
        {
          g_string_erase (tbuffer->data, 0, mlength);
          tbuffer->msgs = g_slist_prepend (tbuffer->msgs, g_memdup (&msg, sizeof (msg)));
          return TRUE;
        }
    }
  g_free (msg.nums);
  g_strfreev (msg.strings);
  g_error ("corrupt log stream from test program");
  return FALSE;
}

/**
 * g_test_log_buffer_new:
 *
 * Internal function for gtester to decode test log messages, no ABI guarantees provided.
 */
GTestLogBuffer*
g_test_log_buffer_new (void)
{
  GTestLogBuffer *tb = g_new0 (GTestLogBuffer, 1);
  tb->data = g_string_sized_new (1024);
  return tb;
}

/**
 * g_test_log_buffer_free
 *
 * Internal function for gtester to free test log messages, no ABI guarantees provided.
 */
void
g_test_log_buffer_free (GTestLogBuffer *tbuffer)
{
  g_return_if_fail (tbuffer != NULL);
  while (tbuffer->msgs)
    g_test_log_msg_free (g_test_log_buffer_pop (tbuffer));
  g_string_free (tbuffer->data, TRUE);
  g_free (tbuffer);
}

/**
 * g_test_log_buffer_push
 *
 * Internal function for gtester to decode test log messages, no ABI guarantees provided.
 */
void
g_test_log_buffer_push (GTestLogBuffer *tbuffer,
                        guint           n_bytes,
                        const guint8   *bytes)
{
  g_return_if_fail (tbuffer != NULL);
  if (n_bytes)
    {
      gboolean more_messages;
      g_return_if_fail (bytes != NULL);
      g_string_append_len (tbuffer->data, (const gchar*) bytes, n_bytes);
      do
        more_messages = g_test_log_extract (tbuffer);
      while (more_messages);
    }
}

/**
 * g_test_log_buffer_pop:
 *
 * Internal function for gtester to retrieve test log messages, no ABI guarantees provided.
 */
GTestLogMsg*
g_test_log_buffer_pop (GTestLogBuffer *tbuffer)
{
  GTestLogMsg *msg = NULL;
  g_return_val_if_fail (tbuffer != NULL, NULL);
  if (tbuffer->msgs)
    {
      GSList *slist = g_slist_last (tbuffer->msgs);
      msg = slist->data;
      tbuffer->msgs = g_slist_delete_link (tbuffer->msgs, slist);
    }
  return msg;
}

/**
 * g_test_log_msg_free:
 *
 * Internal function for gtester to free test log messages, no ABI guarantees provided.
 */
void
g_test_log_msg_free (GTestLogMsg *tmsg)
{
  g_return_if_fail (tmsg != NULL);
  g_strfreev (tmsg->strings);
  g_free (tmsg->nums);
  g_free (tmsg);
}
