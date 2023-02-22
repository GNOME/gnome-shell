/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <GL/gl.h>
#include <cogl/cogl.h>

#include "shell-app-cache-private.h"
#include "shell-util.h"
#include <glib/gi18n-lib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <meta/display.h>
#include <meta/meta-x11-display.h>

#include <locale.h>
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
#include <langinfo.h>
#endif

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#include <systemd/sd-login.h>
#else
/* So we don't need to add ifdef's everywhere */
#define sd_notify(u, m)            do {} while (0)
#define sd_notifyf(u, m, ...)      do {} while (0)
#endif

static void
stop_pick (ClutterActor *actor)
{
  g_signal_stop_emission_by_name (actor, "pick");
}

/**
 * shell_util_set_hidden_from_pick:
 * @actor: A #ClutterActor
 * @hidden: Whether @actor should be hidden from pick
 *
 * If @hidden is %TRUE, hide @actor from pick even with a mode of
 * %CLUTTER_PICK_ALL; if @hidden is %FALSE, unhide @actor.
 */
void
shell_util_set_hidden_from_pick (ClutterActor *actor,
                                 gboolean      hidden)
{
  gpointer existing_handler_data;

  existing_handler_data = g_object_get_data (G_OBJECT (actor),
                                             "shell-stop-pick");
  if (hidden)
    {
      if (existing_handler_data != NULL)
        return;
      g_signal_connect (actor, "pick", G_CALLBACK (stop_pick), NULL);
      g_object_set_data (G_OBJECT (actor),
                         "shell-stop-pick", GUINT_TO_POINTER (1));
    }
  else
    {
      if (existing_handler_data == NULL)
        return;
      g_signal_handlers_disconnect_by_func (actor, stop_pick, NULL);
      g_object_set_data (G_OBJECT (actor), "shell-stop-pick", NULL);
    }
}

/**
 * shell_util_get_week_start:
 *
 * Gets the first week day for the current locale, expressed as a
 * number in the range 0..6, representing week days from Sunday to
 * Saturday.
 *
 * Returns: A number representing the first week day for the current
 *          locale
 */
/* Copied from gtkcalendar.c */
int
shell_util_get_week_start (void)
{
  int week_start;
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
  union { unsigned int word; char *string; } langinfo;
  int week_1stday = 0;
  int first_weekday = 1;
  guint week_origin;
#else
  char *gtk_week_start;
#endif

#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
  langinfo.string = nl_langinfo (_NL_TIME_FIRST_WEEKDAY);
  first_weekday = langinfo.string[0];
  langinfo.string = nl_langinfo (_NL_TIME_WEEK_1STDAY);
  week_origin = langinfo.word;
  if (week_origin == 19971130) /* Sunday */
    week_1stday = 0;
  else if (week_origin == 19971201) /* Monday */
    week_1stday = 1;
  else
    g_warning ("Unknown value of _NL_TIME_WEEK_1STDAY.\n");

  week_start = (week_1stday + first_weekday - 1) % 7;
#else
  /* Use a define to hide the string from xgettext */
# define GTK_WEEK_START "calendar:week_start:0"
  gtk_week_start = dgettext ("gtk30", GTK_WEEK_START);

  if (strncmp (gtk_week_start, "calendar:week_start:", 20) == 0)
    week_start = *(gtk_week_start + 20) - '0';
  else
    week_start = -1;

  if (week_start < 0 || week_start > 6)
    {
      g_warning ("Whoever translated calendar:week_start:0 for GTK+ "
                 "did so wrongly.\n");
      return 0;
    }
#endif

  return week_start;
}

/**
 * shell_util_translate_time_string:
 * @str: String to translate
 *
 * Translate @str according to the locale defined by LC_TIME; unlike
 * dcgettext(), the translations is still taken from the LC_MESSAGES
 * catalogue and not the LC_TIME one.
 *
 * Returns: the translated string
 */
const char *
shell_util_translate_time_string (const char *str)
{
  const char *locale = g_getenv ("LC_TIME");
  const char *res;
  char *sep;
  locale_t old_loc;
  locale_t loc = (locale_t) 0;

  if (locale)
    loc = newlocale (LC_MESSAGES_MASK, locale, (locale_t) 0);

  old_loc = uselocale (loc);

  sep = strchr (str, '\004');
  res = g_dpgettext (NULL, str, sep ? sep - str + 1 : 0);

  uselocale (old_loc);

  if (loc != (locale_t) 0)
    freelocale (loc);

  return res;
}

/**
 * shell_util_regex_escape:
 * @str: a UTF-8 string to escape
 *
 * A wrapper around g_regex_escape_string() that takes its argument as
 * \0-terminated string rather than a byte-array that confuses gjs.
 *
 * Returns: @str with all regex-special characters escaped
 */
char *
shell_util_regex_escape (const char *str)
{
  return g_regex_escape_string (str, -1);
}

/**
 * shell_write_string_to_stream:
 * @stream: a #GOutputStream
 * @str: a UTF-8 string to write to @stream
 * @error: location to store GError
 *
 * Write a string to a GOutputStream as UTF-8. This is a workaround
 * for not having binary buffers in GJS.
 *
 * Return value: %TRUE if write succeeded
 */
gboolean
shell_write_string_to_stream (GOutputStream *stream,
                              const char    *str,
                              GError       **error)
{
  return g_output_stream_write_all (stream, str, strlen (str),
                                    NULL, NULL, error);
}

/**
 * shell_get_file_contents_utf8_sync:
 * @path: UTF-8 encoded filename path
 * @error: a #GError
 *
 * Synchronously load the contents of a file as a NUL terminated
 * string, validating it as UTF-8.  Embedded NUL characters count as
 * invalid content.
 *
 * Returns: (transfer full): File contents
 */
char *
shell_get_file_contents_utf8_sync (const char *path,
                                   GError    **error)
{
  char *contents;
  gsize len;
  if (!g_file_get_contents (path, &contents, &len, error))
    return NULL;
  if (!g_utf8_validate (contents, len, NULL))
    {
      g_free (contents);
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "File %s contains invalid UTF-8",
                   path);
      return NULL;
    }
  return contents;
}

static void
touch_file (GTask        *task,
            gpointer      object,
            gpointer      task_data,
            GCancellable *cancellable)
{
  GFile *file = object;
  g_autoptr (GFile) parent = NULL;
  g_autoptr (GFileOutputStream) stream = NULL;
  GError *error = NULL;

  parent = g_file_get_parent (file);
  g_file_make_directory_with_parents (parent, cancellable, &error);

  if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
      g_task_return_error (task, error);
      return;
    }
  g_clear_error (&error);

  stream = g_file_create (file, G_FILE_CREATE_NONE, cancellable, &error);

  if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
      g_task_return_error (task, error);
      return;
    }
  g_clear_error (&error);

  if (stream)
    g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL);

  g_task_return_boolean (task, stream != NULL);
}

void
shell_util_touch_file_async (GFile               *file,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (G_IS_FILE (file));

  task = g_task_new (file, NULL, callback, user_data);
  g_task_set_source_tag (task, shell_util_touch_file_async);

  g_task_run_in_thread (task, touch_file);
}

gboolean
shell_util_touch_file_finish (GFile         *file,
                              GAsyncResult  *res,
                              GError       **error)
{
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_TASK (res), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

/**
 * shell_util_wifexited:
 * @status: the status returned by wait() or waitpid()
 * @exit: (out): the actual exit status of the process
 *
 * Implements libc standard WIFEXITED, that cannot be used JS
 * code.
 * Returns: TRUE if the process exited normally, FALSE otherwise
 */
gboolean
shell_util_wifexited (int  status,
                      int *exit)
{
  gboolean ret;

  ret = WIFEXITED(status);

  if (ret)
    *exit = WEXITSTATUS(status);

  return ret;
}

/**
 * shell_util_create_pixbuf_from_data:
 * @data: (array length=len) (element-type guint8) (transfer full):
 * @len:
 * @colorspace:
 * @has_alpha:
 * @bits_per_sample:
 * @width:
 * @height:
 * @rowstride:
 *
 * Workaround for non-introspectability of gdk_pixbuf_from_data().
 *
 * Returns: (transfer full):
 */
GdkPixbuf *
shell_util_create_pixbuf_from_data (const guchar      *data,
                                    gsize              len,
                                    GdkColorspace      colorspace,
                                    gboolean           has_alpha,
                                    int                bits_per_sample,
                                    int                width,
                                    int                height,
                                    int                rowstride)
{
  return gdk_pixbuf_new_from_data (data, colorspace, has_alpha,
                                   bits_per_sample, width, height, rowstride,
                                   (GdkPixbufDestroyNotify) g_free, NULL);
}

typedef const gchar *(*ShellGLGetString) (GLenum);

cairo_surface_t *
shell_util_composite_capture_images (ClutterCapture  *captures,
                                     int              n_captures,
                                     int              x,
                                     int              y,
                                     int              target_width,
                                     int              target_height,
                                     float            target_scale)
{
  int i;
  cairo_format_t format;
  cairo_surface_t *image;
  cairo_t *cr;

  g_assert (n_captures > 0);
  g_assert (target_scale > 0.0f);

  format = cairo_image_surface_get_format (captures[0].image);
  image = cairo_image_surface_create (format, target_width, target_height);
  cairo_surface_set_device_scale (image, target_scale, target_scale);

  cr = cairo_create (image);

  for (i = 0; i < n_captures; i++)
    {
      ClutterCapture *capture = &captures[i];

      cairo_save (cr);

      cairo_translate (cr,
                       capture->rect.x - x,
                       capture->rect.y - y);
      cairo_set_source_surface (cr, capture->image, 0, 0);
      cairo_paint (cr);

      cairo_restore (cr);
    }
  cairo_destroy (cr);

  return image;
}

#ifndef HAVE_FDWALK
static int
fdwalk (int  (*cb)(void *data, int fd),
        void  *data)
{
  gint open_max;
  gint fd;
  gint res = 0;

#ifdef HAVE_SYS_RESOURCE_H
  struct rlimit rl;
#endif

#ifdef __linux__
  DIR *d;

  if ((d = opendir("/proc/self/fd")))
    {
      struct dirent *de;

      while ((de = readdir(d)))
        {
          glong l;
          gchar *e = NULL;

          if (de->d_name[0] == '.')
            continue;

          errno = 0;
          l = strtol(de->d_name, &e, 10);
          if (errno != 0 || !e || *e)
            continue;

          fd = (gint) l;

          if ((glong) fd != l)
            continue;

          if (fd == dirfd(d))
            continue;

          if ((res = cb (data, fd)) != 0)
            break;
        }

      closedir(d);
      return res;
  }

  /* If /proc is not mounted or not accessible we fall back to the old
   * rlimit trick */

#endif

#ifdef HAVE_SYS_RESOURCE_H
  if (getrlimit (RLIMIT_NOFILE, &rl) == 0 && rl.rlim_max != RLIM_INFINITY)
    open_max = rl.rlim_max;
  else
#endif
    open_max = sysconf (_SC_OPEN_MAX);

  for (fd = 0; fd < open_max; fd++)
    if ((res = cb (data, fd)) != 0)
      break;

  return res;
}
#endif

static int
check_cloexec (void *data,
               gint  fd)
{
  int r;

  if (fd < 3)
    return 0;

  r = fcntl (fd, F_GETFD);
  if (r < 0)
    return 0;

  if (!(r & FD_CLOEXEC))
    g_warning ("fd %d is not CLOEXEC", fd);

  return 0;
}

/**
 * shell_util_check_cloexec_fds:
 *
 * Walk over all open file descriptors. Check them for the FD_CLOEXEC flag.
 * If this flag is not set, log the offending file descriptor number.
 *
 * It is important that gnome-shell's file descriptors are all marked CLOEXEC,
 * so that the shell's open file descriptors are not passed to child processes
 * that we launch.
 */
void
shell_util_check_cloexec_fds (void)
{
  fdwalk (check_cloexec, NULL);
  g_info ("Open fd CLOEXEC check complete");
}

/**
 * shell_util_get_uid:
 *
 * A wrapper around getuid() so that it can be used from JavaScript. This
 * function will always succeed.
 *
 * Returns: the real user ID of the calling process
 */
gint
shell_util_get_uid (void)
{
  return getuid ();
}

typedef enum {
  SYSTEMD_CALL_FLAGS_NONE = 0,
  SYSTEMD_CALL_FLAGS_WATCH_JOB = 1 << 0,
} SystemdFlags;

#ifdef HAVE_SYSTEMD
typedef struct {
  GDBusConnection *connection;
  gchar           *command;
  SystemdFlags     flags;

  GCancellable *cancellable;
  gulong        cancel_id;

  guint    job_watch;
  gchar   *job;
} SystemdCall;

static void
shell_util_systemd_call_data_free (SystemdCall *data)
{
  if (data->job_watch)
    {
      g_dbus_connection_signal_unsubscribe (data->connection, data->job_watch);
      data->job_watch = 0;
    }

  if (data->cancellable)
    {
      g_cancellable_disconnect (data->cancellable, data->cancel_id);
      g_clear_object (&data->cancellable);
      data->cancel_id = 0;
    }

  g_clear_object (&data->connection);
  g_clear_pointer (&data->job, g_free);
  g_clear_pointer (&data->command, g_free);
  g_free (data);
}

static void
shell_util_systemd_call_cancelled_cb (GCancellable *cancellable,
                                      GTask        *task)
{
  SystemdCall *data = g_task_get_task_data (task);

  /* Task has returned, but data is not yet free'ed, ignore signal. */
  if (g_task_get_completed (task))
    return;

  /* We are still in the DBus call; it will return the error. */
  if (data->job == NULL)
    return;

  g_task_return_error_if_cancelled (task);
  g_object_unref (task);
}

static void
on_systemd_call_cb (GObject      *source,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;
  GTask *task = G_TASK (user_data);
  SystemdCall *data;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source),
                                         res, &error);

  data = g_task_get_task_data (task);

  if (error) {
    g_warning ("Could not issue '%s' systemd call", data->command);
    g_task_return_error (task, g_steal_pointer (&error));
    g_object_unref (task);

    return;
  }

  g_assert (data->job == NULL);
  g_variant_get (reply, "(o)", &data->job);

  /* we should either wait for the JobRemoved notification, or
   * notify here */
  if ((data->flags & SYSTEMD_CALL_FLAGS_WATCH_JOB) == 0)
    g_task_return_boolean (task, TRUE);
}

static void
on_systemd_job_removed_cb (GDBusConnection *connection,
                           const gchar *sender_name,
                           const gchar *object_path,
                           const gchar *interface_name,
                           const gchar *signal_name,
                           GVariant *parameters,
                           gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  SystemdCall *data;
  guint32 id;
  const char *path, *unit, *result;

  /* Task has returned, but data is not yet free'ed, ignore signal. */
  if (g_task_get_completed (task))
    return;

  data = g_task_get_task_data (task);

  /* No job information yet, ignore. */
  if (data->job == NULL)
    return;

  g_variant_get (parameters, "(u&o&s&s)", &id, &path, &unit, &result);

  /* Is it the job we are waiting for? */
  if (g_strcmp0 (path, data->job) != 0)
    return;

  /* Task has completed; return the result of the job */
  if (g_strcmp0 (result, "done") == 0)
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_FAILED,
                             "Systemd job completed with status \"%s\"",
                             result);

  g_object_unref (task);
}
#endif /* HAVE_SYSTEMD */

static void
shell_util_systemd_call (const char           *command,
                         GVariant             *params,
                         SystemdFlags          flags,
                         GCancellable         *cancellable,
                         GAsyncReadyCallback   callback,
                         gpointer              user_data)
{
  g_autoptr (GTask) task = g_task_new (NULL, cancellable, callback, user_data);

#ifdef HAVE_SYSTEMD
  g_autoptr (GDBusConnection) connection = NULL;
  GError *error = NULL;
  SystemdCall *data;
  g_autofree char *self_unit = NULL;
  int res;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

  if (connection == NULL) {
    g_task_return_error (task, error);
    return;
  }

  /* We look up the systemd unit that our own process is running in here.
   * This way we determine whether the session is managed using systemd.
   */
  res = sd_pid_get_user_unit (getpid (), &self_unit);

  if (res == -ENODATA)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Not systemd managed");
      return;
    }
  else if (res < 0)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               g_io_error_from_errno (-res),
                               "Error fetching own systemd unit: %s",
                               g_strerror (-res));
      return;
    }

  data = g_new0 (SystemdCall, 1);
  data->command = g_strdup (command);
  data->connection = g_object_ref (connection);
  data->flags = flags;

  if ((data->flags & SYSTEMD_CALL_FLAGS_WATCH_JOB) != 0)
    {
      data->job_watch = g_dbus_connection_signal_subscribe (connection,
                                                            "org.freedesktop.systemd1",
                                                            "org.freedesktop.systemd1.Manager",
                                                            "JobRemoved",
                                                            "/org/freedesktop/systemd1",
                                                            NULL,
                                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                                            on_systemd_job_removed_cb,
                                                            task,
                                                            NULL);
    }

  g_task_set_task_data (task,
                        data,
                        (GDestroyNotify) shell_util_systemd_call_data_free);

  if (cancellable)
    {
      data->cancellable = g_object_ref (cancellable);
      data->cancel_id = g_cancellable_connect (cancellable,
                                               G_CALLBACK (shell_util_systemd_call_cancelled_cb),
                                               task,
                                               NULL);
    }

  g_dbus_connection_call (connection,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          command,
                          params,
                          G_VARIANT_TYPE ("(o)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1, cancellable,
                          on_systemd_call_cb,
                          g_steal_pointer (&task));
#else /* HAVE_SYSTEMD */
  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "systemd not supported by gnome-shell");
#endif /* !HAVE_SYSTEMD */
}

void
shell_util_start_systemd_unit (const char           *unit,
                               const char           *mode,
                               GCancellable         *cancellable,
                               GAsyncReadyCallback   callback,
                               gpointer              user_data)
{
  shell_util_systemd_call ("StartUnit",
                           g_variant_new ("(ss)", unit, mode),
                           SYSTEMD_CALL_FLAGS_WATCH_JOB,
                           cancellable, callback, user_data);
}

gboolean
shell_util_start_systemd_unit_finish (GAsyncResult  *res,
                                      GError       **error)
{
  return g_task_propagate_boolean (G_TASK (res), error);
}

void
shell_util_stop_systemd_unit (const char           *unit,
                              const char           *mode,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data)
{
  shell_util_systemd_call ("StopUnit",
                           g_variant_new ("(ss)", unit, mode),
                           SYSTEMD_CALL_FLAGS_WATCH_JOB,
                           cancellable, callback, user_data);
}

gboolean
shell_util_stop_systemd_unit_finish (GAsyncResult  *res,
                                     GError       **error)
{
  return g_task_propagate_boolean (G_TASK (res), error);
}

void
shell_util_systemd_unit_exists (const gchar         *unit,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  shell_util_systemd_call ("GetUnit",
                           g_variant_new ("(s)", unit),
                           SYSTEMD_CALL_FLAGS_NONE,
                           cancellable, callback, user_data);
}

gboolean
shell_util_systemd_unit_exists_finish (GAsyncResult  *res,
                                       GError       **error)
{
  return g_task_propagate_boolean (G_TASK (res), error);
}

void
shell_util_sd_notify (void)
{
  /* We only use NOTIFY_SOCKET exactly once; unset it so it doesn't remain in
   * our environment. */
  sd_notify (1, "READY=1");
}

/**
 * shell_util_has_x11_display_extension:
 * @display: A #MetaDisplay
 * @extension: An X11 extension
 *
 * If the corresponding X11 display provides the passed extension, return %TRUE,
 * otherwise %FALSE. If there is no X11 display, %FALSE is passed.
 */
gboolean
shell_util_has_x11_display_extension (MetaDisplay *display,
                                      const char  *extension)
{
  MetaX11Display *x11_display;
  Display *xdisplay;
  int op, event, error;

  x11_display = meta_display_get_x11_display (display);
  if (!x11_display)
    return FALSE;

  xdisplay = meta_x11_display_get_xdisplay (x11_display);
  return XQueryExtension (xdisplay, extension, &op, &event, &error);
}

/**
 * shell_util_get_translated_folder_name:
 * @name: the untranslated folder name
 *
 * Attempts to translate the folder @name using translations provided
 * by .directory files.
 *
 * Returns: (nullable): a translated string or %NULL
 */
char *
shell_util_get_translated_folder_name (const char *name)
{
  return shell_app_cache_translate_folder (shell_app_cache_get_default (), name);
}
