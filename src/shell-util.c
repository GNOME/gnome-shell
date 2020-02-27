/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>

#include <GL/gl.h>
#include <cogl/cogl.h>

#include "shell-util.h"
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <meta/meta-shaped-texture.h>

#include <locale.h>
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
#include <langinfo.h>
#endif

static GTimeZone *local_tz;

static void
stop_pick (ClutterActor       *actor,
           const ClutterColor *color)
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
 * shell_util_get_transformed_allocation:
 * @actor: a #ClutterActor
 * @box: (out): location to store returned box in stage coordinates
 *
 * This function is similar to a combination of clutter_actor_get_transformed_position(),
 * and clutter_actor_get_transformed_size(), but unlike
 * clutter_actor_get_transformed_size(), it always returns a transform
 * of the current allocation, while clutter_actor_get_transformed_size() returns
 * bad values (the transform of the requested size) if a relayout has been
 * queued.
 *
 * This function is more convenient to use than
 * clutter_actor_get_abs_allocation_vertices() if no transformation is in effect
 * and also works around limitations in the GJS binding of arrays.
 */
void
shell_util_get_transformed_allocation (ClutterActor    *actor,
                                       ClutterActorBox *box)
{
  /* Code adapted from clutter-actor.c:
   * Copyright 2006, 2007, 2008 OpenedHand Ltd
   */
  ClutterVertex v[4];
  gfloat x_min, x_max, y_min, y_max;
  guint i;

  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  clutter_actor_get_abs_allocation_vertices (actor, v);

  x_min = x_max = v[0].x;
  y_min = y_max = v[0].y;

  for (i = 1; i < G_N_ELEMENTS (v); ++i)
    {
      if (v[i].x < x_min)
	x_min = v[i].x;

      if (v[i].x > x_max)
	x_max = v[i].x;

      if (v[i].y < y_min)
	y_min = v[i].y;

      if (v[i].y > y_max)
	y_max = v[i].y;
    }

  box->x1 = x_min;
  box->y1 = y_min;
  box->x2 = x_max;
  box->y2 = y_max;
}

/**
 * shell_util_format_date:
 * @format: a strftime-style string format, as parsed by
 *   g_date_time_format()
 * @time_ms: milliseconds since 1970-01-01 00:00:00 UTC; the
 *   value returned by Date.getTime()
 *
 * Formats a date for the current locale. This should be
 * used instead of the Spidermonkey Date.toLocaleFormat()
 * extension because Date.toLocaleFormat() is buggy for
 * Unicode format strings:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=508783
 *
 * Return value: the formatted date. If the date is
 *  outside of the range of a GDateTime (which contains
 *  any plausible dates we actually care about), will
 *  return an empty string.
 */
char *
shell_util_format_date (const char *format,
                        gint64      time_ms)
{
  GDateTime *datetime;
  GTimeVal tv;
  char *result;

  tv.tv_sec = time_ms / 1000;
  tv.tv_usec = (time_ms % 1000) * 1000;

  datetime = g_date_time_new_from_timeval_local (&tv);
  if (!datetime) /* time_ms is out of range of GDateTime */
    return g_strdup ("");

  result = g_date_time_format (datetime, format);

  g_date_time_unref (datetime);
  return result;
}

char *
shell_util_format_now (const char *format)
{
  GDateTime *datetime;
  char *ret;

  if (local_tz == NULL)
    local_tz = g_time_zone_new_local ();

  datetime = g_date_time_new_now (local_tz);
  if (!datetime)
    return g_strdup ("");

  ret = g_date_time_format (datetime, format);
  g_date_time_unref (datetime);
  return ret;
}

void
shell_util_clear_timezone_cache (void)
{
  g_clear_pointer (&local_tz, g_time_zone_unref);
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
      week_start = 0;
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

  if (locale)
    setlocale (LC_MESSAGES, locale);

  sep = strchr (str, '\004');
  res = g_dpgettext (NULL, str, sep ? sep - str + 1 : 0);

  setlocale (LC_MESSAGES, "");

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

void
shell_util_cursor_tracker_to_clutter (MetaCursorTracker *tracker,
                                      ClutterTexture    *texture)
{
  CoglTexture *sprite;

  sprite = meta_cursor_tracker_get_sprite (tracker);
  if (sprite)
    {
      clutter_actor_show (CLUTTER_ACTOR (texture));
      clutter_texture_set_cogl_texture (texture, sprite);
    }
  else
    {
      clutter_actor_hide (CLUTTER_ACTOR (texture));
    }
}

typedef const gchar *(*ShellGLGetString) (GLenum);

static const gchar *
get_gl_vendor (void)
{
  static const gchar *vendor = NULL;

  if (!vendor)
    {
      ShellGLGetString gl_get_string;
      gl_get_string = (ShellGLGetString) cogl_get_proc_address ("glGetString");
      if (gl_get_string)
        vendor = gl_get_string (GL_VENDOR);
    }

  return vendor;
}

gboolean
shell_util_need_background_refresh (void)
{
  if (!clutter_check_windowing_backend (CLUTTER_WINDOWING_X11))
    return FALSE;

  if (g_strcmp0 (get_gl_vendor (), "NVIDIA Corporation") == 0)
    return TRUE;

  return FALSE;
}

static gboolean
canvas_draw_cb (ClutterContent *content,
                cairo_t        *cr,
                gint            width,
                gint            height,
                gpointer        user_data)
{
  cairo_surface_t *surface = user_data;

  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);

  return FALSE;
}

/**
 * shell_util_get_content_for_window_actor:
 * @window_actor: a #MetaWindowActor
 * @window_rect: a #MetaRectangle
 *
 * Returns: (transfer full): a new #ClutterContent
 */
ClutterContent *
shell_util_get_content_for_window_actor (MetaWindowActor *window_actor,
                                         MetaRectangle   *window_rect)
{
  ClutterActor *texture;
  ClutterContent *content;
  cairo_surface_t *surface;
  cairo_rectangle_int_t clip;
  gfloat actor_x, actor_y;

  texture = meta_window_actor_get_texture (window_actor);
  clutter_actor_get_position (CLUTTER_ACTOR (window_actor), &actor_x, &actor_y);

  clip.x = window_rect->x - (gint) actor_x;
  clip.y = window_rect->y - (gint) actor_y;
  clip.width = window_rect->width;
  clip.height = window_rect->height;

  surface = meta_shaped_texture_get_image (META_SHAPED_TEXTURE (texture),
                                           &clip);

  content = clutter_canvas_new ();
  clutter_canvas_set_size (CLUTTER_CANVAS (content),
                           clip.width, clip.height);
  g_signal_connect (content, "draw",
                    G_CALLBACK (canvas_draw_cb), surface);
  clutter_content_invalidate (content);
  cairo_surface_destroy (surface);

  return content;
}

cairo_surface_t *
shell_util_composite_capture_images (ClutterCapture  *captures,
                                     int              n_captures,
                                     int              x,
                                     int              y,
                                     int              width,
                                     int              height)
{
  int i;
  double target_scale;
  cairo_format_t format;
  cairo_surface_t *image;
  cairo_t *cr;

  g_assert (n_captures > 0);

  target_scale = 0.0;
  for (i = 0; i < n_captures; i++)
    {
      ClutterCapture *capture = &captures[i];
      double capture_scale = 1.0;

      cairo_surface_get_device_scale (capture->image, &capture_scale, NULL);
      target_scale = MAX (target_scale, capture_scale);
    }

  format = cairo_image_surface_get_format (captures[0].image);
  image = cairo_image_surface_create (format,
                                      width * target_scale,
                                      height * target_scale);
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
