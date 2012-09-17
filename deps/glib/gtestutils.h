/* GLib testing utilities
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik
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

#if defined(G_DISABLE_SINGLE_INCLUDES) && !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#ifndef __G_TEST_UTILS_H__
#define __G_TEST_UTILS_H__

#include <glib/gmessages.h>
#include <glib/gstring.h>
#include <glib/gerror.h>
#include <glib/gslist.h>

G_BEGIN_DECLS

/* assertion API */
#define g_assert_cmpstr(s1, cmp, s2)    do { const char *__s1 = (s1), *__s2 = (s2); \
                                             if (g_strcmp0 (__s1, __s2) cmp 0) ; else \
                                               g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #s1 " " #cmp " " #s2, __s1, #cmp, __s2); } while (0)
#define g_assert_cmpint(n1, cmp, n2)    do { gint64 __n1 = (n1), __n2 = (n2); \
                                             if (__n1 cmp __n2) ; else \
                                               g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #n1 " " #cmp " " #n2, __n1, #cmp, __n2, 'i'); } while (0)
#define g_assert_cmpuint(n1, cmp, n2)   do { guint64 __n1 = (n1), __n2 = (n2); \
                                             if (__n1 cmp __n2) ; else \
                                               g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #n1 " " #cmp " " #n2, __n1, #cmp, __n2, 'i'); } while (0)
#define g_assert_cmphex(n1, cmp, n2)    do { guint64 __n1 = (n1), __n2 = (n2); \
                                             if (__n1 cmp __n2) ; else \
                                               g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #n1 " " #cmp " " #n2, __n1, #cmp, __n2, 'x'); } while (0)
#define g_assert_cmpfloat(n1,cmp,n2)    do { long double __n1 = (n1), __n2 = (n2); \
                                             if (__n1 cmp __n2) ; else \
                                               g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #n1 " " #cmp " " #n2, __n1, #cmp, __n2, 'f'); } while (0)
#define g_assert_no_error(err)          do { if (err) \
                                               g_assertion_message_error (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #err, err, 0, 0); } while (0)
#define g_assert_error(err, dom, c)	do { if (!err || (err)->domain != dom || (err)->code != c) \
                                               g_assertion_message_error (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #err, err, dom, c); } while (0)
#ifdef G_DISABLE_ASSERT
#define g_assert_not_reached()          do { (void) 0; } while (0)
#define g_assert(expr)                  do { (void) 0; } while (0)
#else /* !G_DISABLE_ASSERT */
#define g_assert_not_reached()          do { g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, NULL); } while (0)
#define g_assert(expr)                  do { if G_LIKELY (expr) ; else \
                                               g_assertion_message_expr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                 #expr); } while (0)
#endif /* !G_DISABLE_ASSERT */

int     g_strcmp0                       (const char     *str1,
                                         const char     *str2);

void    g_assertion_message             (const char     *domain,
                                         const char     *file,
                                         int             line,
                                         const char     *func,
                                         const char     *message) G_GNUC_NORETURN;
void    g_assertion_message_expr        (const char     *domain,
                                         const char     *file,
                                         int             line,
                                         const char     *func,
                                         const char     *expr) G_GNUC_NORETURN;
void    g_assertion_message_cmpstr      (const char     *domain,
                                         const char     *file,
                                         int             line,
                                         const char     *func,
                                         const char     *expr,
                                         const char     *arg1,
                                         const char     *cmp,
                                         const char     *arg2) G_GNUC_NORETURN;
void    g_assertion_message_cmpnum      (const char     *domain,
                                         const char     *file,
                                         int             line,
                                         const char     *func,
                                         const char     *expr,
                                         long double     arg1,
                                         const char     *cmp,
                                         long double     arg2,
                                         char            numtype) G_GNUC_NORETURN;
void    g_assertion_message_error       (const char     *domain,
                                         const char     *file,
                                         int             line,
                                         const char     *func,
                                         const char     *expr,
                                         const GError   *error,
                                         GQuark          error_domain,
                                         int             error_code) G_GNUC_NORETURN;
/* internal logging API */
typedef enum {
  G_TEST_LOG_NONE,
  G_TEST_LOG_ERROR,             /* s:msg */
} GTestLogType;

typedef struct {
  GTestLogType  log_type;
  guint         n_strings;
  gchar       **strings; /* NULL terminated */
  guint         n_nums;
  long double  *nums;
} GTestLogMsg;
typedef struct {
  /*< private >*/
  GString     *data;
  GSList      *msgs;
} GTestLogBuffer;

const char*     g_test_log_type_name    (GTestLogType    log_type);
GTestLogBuffer* g_test_log_buffer_new   (void);
void            g_test_log_buffer_free  (GTestLogBuffer *tbuffer);
void            g_test_log_buffer_push  (GTestLogBuffer *tbuffer,
                                         guint           n_bytes,
                                         const guint8   *bytes);
GTestLogMsg*    g_test_log_buffer_pop   (GTestLogBuffer *tbuffer);
void            g_test_log_msg_free     (GTestLogMsg    *tmsg);

/**
 * GTestLogFatalFunc:
 * @log_domain: the log domain of the message
 * @log_level: the log level of the message (including the fatal and recursion flags)
 * @message: the message to process
 * @user_data: user data, set in g_test_log_set_fatal_handler()
 *
 * Specifies the prototype of fatal log handler functions.
 *
 * Return value: %TRUE if the program should abort, %FALSE otherwise
 *
 * Since: 2.22
 */
typedef gboolean        (*GTestLogFatalFunc)    (const gchar    *log_domain,
                                                 GLogLevelFlags  log_level,
                                                 const gchar    *message,
                                                 gpointer        user_data);
void
g_test_log_set_fatal_handler            (GTestLogFatalFunc log_func,
                                         gpointer          user_data);

G_END_DECLS

#endif /* __G_TEST_UTILS_H__ */
