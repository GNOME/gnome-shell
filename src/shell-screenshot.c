/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <meta/display.h>
#include <meta/util.h>
#include <meta/meta-plugin.h>
#include <meta/meta-shaped-texture.h>
#include <meta/meta-cursor-tracker.h>

#include "shell-global.h"
#include "shell-screenshot.h"
#include "shell-util.h"

#define A11Y_APPS_SCHEMA "org.gnome.desktop.a11y.applications"
#define MAGNIFIER_ACTIVE_KEY "screen-magnifier-enabled"

typedef struct _ShellScreenshotPrivate  ShellScreenshotPrivate;

struct _ShellScreenshot
{
  GObject parent_instance;

  ShellScreenshotPrivate *priv;
};

struct _ShellScreenshotPrivate
{
  ShellGlobal *global;

  char *filename;
  char *filename_used;

  GDateTime *datetime;

  cairo_surface_t *image;
  cairo_rectangle_int_t screenshot_area;

  gboolean include_cursor;
  gboolean include_frame;

  ShellScreenshotCallback callback;
};

G_DEFINE_TYPE_WITH_PRIVATE (ShellScreenshot, shell_screenshot, G_TYPE_OBJECT);

static void
shell_screenshot_class_init (ShellScreenshotClass *screenshot_class)
{
  (void) screenshot_class;
}

static void
shell_screenshot_init (ShellScreenshot *screenshot)
{
  screenshot->priv = shell_screenshot_get_instance_private (screenshot);
  screenshot->priv->global = shell_global_get ();
}

static void
on_screenshot_written (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  ShellScreenshot *screenshot = SHELL_SCREENSHOT (source);
  ShellScreenshotPrivate *priv = screenshot->priv;

  if (priv->callback)
    priv->callback (screenshot,
                    g_task_propagate_boolean (G_TASK (result), NULL),
                    &priv->screenshot_area,
                    priv->filename_used);

  g_clear_pointer (&priv->image, cairo_surface_destroy);
  g_clear_pointer (&priv->filename, g_free);
  g_clear_pointer (&priv->filename_used, g_free);
  g_clear_pointer (&priv->datetime, g_date_time_unref);

  meta_enable_unredirect_for_display (shell_global_get_display (priv->global));
}

/* called in an I/O thread */
static GOutputStream *
get_stream_for_unique_path (const gchar *path,
                            const gchar *filename,
                            gchar **filename_used)
{
  GOutputStream *stream;
  GFile *file;
  gchar *real_path, *real_filename, *name, *ptr;
  gint idx;

  ptr = g_strrstr (filename, ".png");

  if (ptr != NULL)
    real_filename = g_strndup (filename, ptr - filename);
  else
    real_filename = g_strdup (filename);

  idx = 0;
  real_path = NULL;

  do
    {
      if (idx == 0)
        name = g_strdup_printf ("%s.png", real_filename);
      else
        name = g_strdup_printf ("%s - %d.png", real_filename, idx);

      real_path = g_build_filename (path, name, NULL);
      g_free (name);

      file = g_file_new_for_path (real_path);
      stream = G_OUTPUT_STREAM (g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL));
      g_object_unref (file);

      if (stream != NULL)
        *filename_used = real_path;
      else
        g_free (real_path);

      idx++;
    }
  while (stream == NULL);

  g_free (real_filename);

  return stream;
}

/* called in an I/O thread */
static GOutputStream *
get_stream_for_filename (const gchar *filename,
                         gchar **filename_used)
{
  const gchar *path;

  path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  if (!g_file_test (path, G_FILE_TEST_EXISTS))
    {
      path = g_get_home_dir ();
      if (!g_file_test (path, G_FILE_TEST_EXISTS))
        return NULL;
    }

  return get_stream_for_unique_path (path, filename, filename_used);
}

static GOutputStream *
prepare_write_stream (const gchar *filename,
                      gchar **filename_used)
{
  GOutputStream *stream;
  GFile *file;

  if (g_path_is_absolute (filename))
    {
      file = g_file_new_for_path (filename);
      *filename_used = g_strdup (filename);
      stream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL));
      g_object_unref (file);
    }
  else
    {
      stream = get_stream_for_filename (filename, filename_used);
    }

  return stream;
}

static void
write_screenshot_thread (GTask        *result,
                         gpointer      object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  cairo_status_t status;
  GOutputStream *stream;
  ShellScreenshot *screenshot = SHELL_SCREENSHOT (object);
  ShellScreenshotPrivate *priv;
  char *creation_time;

  g_assert (screenshot != NULL);

  priv = screenshot->priv;

  stream = prepare_write_stream (priv->filename,
                                 &priv->filename_used);

  if (stream == NULL)
    status = CAIRO_STATUS_FILE_NOT_FOUND;
  else
    {
      GdkPixbuf *pixbuf;

      pixbuf = gdk_pixbuf_get_from_surface (priv->image,
                                            0, 0,
                                            cairo_image_surface_get_width (priv->image),
                                            cairo_image_surface_get_height (priv->image));
      creation_time = g_date_time_format (priv->datetime, "%c");

      if (gdk_pixbuf_save_to_stream (pixbuf, stream, "png", NULL, NULL,
                                     "tEXt::Software", "gnome-screenshot",
                                     "tEXt::Creation Time", creation_time,
                                     NULL))
        status = CAIRO_STATUS_SUCCESS;
      else
        status = CAIRO_STATUS_WRITE_ERROR;

      g_object_unref (pixbuf);
      g_free (creation_time);
    }


  g_task_return_boolean (result, status == CAIRO_STATUS_SUCCESS);

  g_clear_object (&stream);
}

static void
do_grab_screenshot (ShellScreenshot *screenshot,
                    ClutterStage    *stage,
                    int               x,
                    int               y,
                    int               width,
                    int               height)
{
  ShellScreenshotPrivate *priv = screenshot->priv;
  ClutterCapture *captures;
  int n_captures;
  int i;

  clutter_stage_capture (stage, FALSE,
                         &(cairo_rectangle_int_t) {
                           .x = x,
                           .y = y,
                           .width = width,
                           .height = height
                         },
                         &captures,
                         &n_captures);

  if (n_captures == 0)
    return;
  else if (n_captures == 1)
    priv->image = cairo_surface_reference (captures[0].image);
  else
    priv->image = shell_util_composite_capture_images (captures,
                                                       n_captures,
                                                       x, y,
                                                       width, height);
  priv->datetime = g_date_time_new_now_local ();

  for (i = 0; i < n_captures; i++)
    cairo_surface_destroy (captures[i].image);

  g_free (captures);
}

static void
_draw_cursor_image (MetaCursorTracker     *tracker,
                    cairo_surface_t       *surface,
                    cairo_rectangle_int_t  area)
{
  CoglTexture *texture;
  int width, height;
  int stride;
  guint8 *data;
  cairo_surface_t *cursor_surface;
  cairo_region_t *screenshot_region;
  cairo_t *cr;
  int x, y;
  int xhot, yhot;

  texture = meta_cursor_tracker_get_sprite (tracker);
  if (!texture)
    return;

  screenshot_region = cairo_region_create_rectangle (&area);
  meta_cursor_tracker_get_pointer (tracker, &x, &y, NULL);

  if (!cairo_region_contains_point (screenshot_region, x, y))
    {
      cairo_region_destroy (screenshot_region);
      return;
    }

  meta_cursor_tracker_get_hot (tracker, &xhot, &yhot);
  width = cogl_texture_get_width (texture);
  height = cogl_texture_get_height (texture);
  stride = 4 * width;
  data = g_new (guint8, stride * height);
  cogl_texture_get_data (texture, CLUTTER_CAIRO_FORMAT_ARGB32, stride, data);

  /* FIXME: cairo-gl? */
  cursor_surface = cairo_image_surface_create_for_data (data,
                                                        CAIRO_FORMAT_ARGB32,
                                                        width, height,
                                                        stride);

  cr = cairo_create (surface);
  cairo_set_source_surface (cr,
                            cursor_surface,
                            x - xhot - area.x,
                            y - yhot - area.y);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (cursor_surface);
  cairo_region_destroy (screenshot_region);
  g_free (data);
}

static void
grab_screenshot (ClutterActor *stage,
                 ShellScreenshot *screenshot)
{
  MetaDisplay *display;
  MetaCursorTracker *tracker;
  int width, height;
  GTask *result;
  GSettings *settings;
  ShellScreenshotPrivate *priv = screenshot->priv;

  display = shell_global_get_display (priv->global);
  meta_display_get_size (display, &width, &height);

  do_grab_screenshot (screenshot, CLUTTER_STAGE (stage), 0, 0, width, height);

  if (meta_display_get_n_monitors (display) > 1)
    {
      cairo_region_t *screen_region = cairo_region_create ();
      cairo_region_t *stage_region;
      MetaRectangle monitor_rect;
      cairo_rectangle_int_t stage_rect;
      int i;
      cairo_t *cr;

      for (i = meta_display_get_n_monitors (display) - 1; i >= 0; i--)
        {
          meta_display_get_monitor_geometry (display, i, &monitor_rect);
          cairo_region_union_rectangle (screen_region,
                                        (const cairo_rectangle_int_t *) &monitor_rect);
        }

      stage_rect.x = 0;
      stage_rect.y = 0;
      stage_rect.width = width;
      stage_rect.height = height;

      stage_region = cairo_region_create_rectangle ((const cairo_rectangle_int_t *) &stage_rect);
      cairo_region_xor (stage_region, screen_region);
      cairo_region_destroy (screen_region);

      cr = cairo_create (priv->image);

      for (i = 0; i < cairo_region_num_rectangles (stage_region); i++)
        {
          cairo_rectangle_int_t rect;
          cairo_region_get_rectangle (stage_region, i, &rect);
          cairo_rectangle (cr, (double) rect.x, (double) rect.y, (double) rect.width, (double) rect.height);
          cairo_fill (cr);
        }

      cairo_destroy (cr);
      cairo_region_destroy (stage_region);
    }

  priv->screenshot_area.x = 0;
  priv->screenshot_area.y = 0;
  priv->screenshot_area.width = width;
  priv->screenshot_area.height = height;

  settings = g_settings_new (A11Y_APPS_SCHEMA);
  if (priv->include_cursor &&
      !g_settings_get_boolean (settings, MAGNIFIER_ACTIVE_KEY))
    {
      tracker = meta_cursor_tracker_get_for_display (display);
      _draw_cursor_image (tracker, priv->image, priv->screenshot_area);
    }
  g_object_unref (settings);

  g_signal_handlers_disconnect_by_func (stage, (void *)grab_screenshot, (gpointer)screenshot);

  result = g_task_new (screenshot, NULL, on_screenshot_written, NULL);
  g_task_run_in_thread (result, write_screenshot_thread);
  g_object_unref (result);
}

static void
grab_area_screenshot (ClutterActor *stage,
                      ShellScreenshot *screenshot)
{
  GTask *result;
  ShellScreenshotPrivate *priv = screenshot->priv;

  do_grab_screenshot (screenshot,
                      CLUTTER_STAGE (stage),
                      priv->screenshot_area.x,
                      priv->screenshot_area.y,
                      priv->screenshot_area.width,
                      priv->screenshot_area.height);

  g_signal_handlers_disconnect_by_func (stage, (void *)grab_area_screenshot, (gpointer)screenshot);
  result = g_task_new (screenshot, NULL, on_screenshot_written, NULL);
  g_task_run_in_thread (result, write_screenshot_thread);
  g_object_unref (result);
}

static void
grab_window_screenshot (ClutterActor *stage,
                        ShellScreenshot *screenshot)
{
  ShellScreenshotPrivate *priv = screenshot->priv;
  GTask *result;
  GSettings *settings;
  MetaDisplay *display = shell_global_get_display (priv->global);
  MetaCursorTracker *tracker;
  MetaWindow *window = meta_display_get_focus_window (display);
  ClutterActor *window_actor;
  gfloat actor_x, actor_y;
  MetaShapedTexture *stex;
  MetaRectangle rect;
  cairo_rectangle_int_t clip;

  window_actor = CLUTTER_ACTOR (meta_window_get_compositor_private (window));
  clutter_actor_get_position (window_actor, &actor_x, &actor_y);

  meta_window_get_frame_rect (window, &rect);

  if (!priv->include_frame)
    meta_window_frame_rect_to_client_rect (window, &rect, &rect);

  priv->screenshot_area.x = rect.x;
  priv->screenshot_area.y = rect.y;
  clip.x = rect.x - (gint) actor_x;
  clip.y = rect.y - (gint) actor_y;

  clip.width = priv->screenshot_area.width = rect.width;
  clip.height = priv->screenshot_area.height = rect.height;

  stex = META_SHAPED_TEXTURE (meta_window_actor_get_texture (META_WINDOW_ACTOR (window_actor)));
  priv->image = meta_shaped_texture_get_image (stex, &clip);
  priv->datetime = g_date_time_new_now_local ();

  settings = g_settings_new (A11Y_APPS_SCHEMA);
  if (priv->include_cursor && !g_settings_get_boolean (settings, MAGNIFIER_ACTIVE_KEY))
    {
      tracker = meta_cursor_tracker_get_for_display (display);
      _draw_cursor_image (tracker, priv->image, priv->screenshot_area);
    }
  g_object_unref (settings);

  g_signal_handlers_disconnect_by_func (stage, (void *)grab_window_screenshot, (gpointer)screenshot);
  result = g_task_new (screenshot, NULL, on_screenshot_written, NULL);
  g_task_run_in_thread (result, write_screenshot_thread);
  g_object_unref (result);
}

/**
 * shell_screenshot_screenshot:
 * @screenshot: the #ShellScreenshot
 * @include_cursor: Whether to include the cursor or not
 * @filename: The filename for the screenshot
 * @callback: (scope async): function to call returning success or failure
 * of the async grabbing
 *
 * Takes a screenshot of the whole screen
 * in @filename as png image.
 *
 */
void
shell_screenshot_screenshot (ShellScreenshot *screenshot,
                             gboolean include_cursor,
                             const char *filename,
                             ShellScreenshotCallback callback)
{
  ClutterActor *stage;
  ShellScreenshotPrivate *priv = screenshot->priv;

  if (priv->filename != NULL) {
    if (callback)
      callback (screenshot, FALSE, NULL, "");
    return;
  }

  priv->filename = g_strdup (filename);
  priv->callback = callback;
  priv->include_cursor = include_cursor;

  stage = CLUTTER_ACTOR (shell_global_get_stage (priv->global));

  meta_disable_unredirect_for_display (shell_global_get_display (priv->global));

  g_signal_connect_after (stage, "paint", G_CALLBACK (grab_screenshot), (gpointer)screenshot);

  clutter_actor_queue_redraw (stage);
}

/**
 * shell_screenshot_screenshot_area:
 * @screenshot: the #ShellScreenshot
 * @x: The X coordinate of the area
 * @y: The Y coordinate of the area
 * @width: The width of the area
 * @height: The height of the area
 * @filename: The filename for the screenshot
 * @callback: (scope async): function to call returning success or failure
 * of the async grabbing
 *
 * Takes a screenshot of the passed in area and saves it
 * in @filename as png image.
 *
 */
void
shell_screenshot_screenshot_area (ShellScreenshot *screenshot,
                                  int x,
                                  int y,
                                  int width,
                                  int height,
                                  const char *filename,
                                  ShellScreenshotCallback callback)
{
  ClutterActor *stage;
  ShellScreenshotPrivate *priv = screenshot->priv;

  if (priv->filename != NULL) {
    if (callback)
      callback (screenshot, FALSE, NULL, "");
    return;
  }

  priv->filename = g_strdup (filename);
  priv->screenshot_area.x = x;
  priv->screenshot_area.y = y;
  priv->screenshot_area.width = width;
  priv->screenshot_area.height = height;
  priv->callback = callback;

  stage = CLUTTER_ACTOR (shell_global_get_stage (priv->global));

  meta_disable_unredirect_for_display (shell_global_get_display (shell_global_get ()));

  g_signal_connect_after (stage, "paint", G_CALLBACK (grab_area_screenshot), (gpointer)screenshot);

  clutter_actor_queue_redraw (stage);
}

/**
 * shell_screenshot_screenshot_window:
 * @screenshot: the #ShellScreenshot
 * @include_frame: Whether to include the frame or not
 * @include_cursor: Whether to include the cursor or not
 * @filename: The filename for the screenshot
 * @callback: (scope async): function to call returning success or failure
 * of the async grabbing
 *
 * Takes a screenshot of the focused window (optionally omitting the frame)
 * in @filename as png image.
 *
 */
void
shell_screenshot_screenshot_window (ShellScreenshot *screenshot,
                                    gboolean include_frame,
                                    gboolean include_cursor,
                                    const char *filename,
                                    ShellScreenshotCallback callback)
{
  ShellScreenshotPrivate *priv = screenshot->priv;
  MetaDisplay *display = shell_global_get_display (priv->global);
  ClutterActor *stage;
  MetaWindow *window = meta_display_get_focus_window (display);

  if (priv->filename != NULL || !window) {
    if (callback)
      callback (screenshot, FALSE, NULL, "");
    return;
  }

  priv->filename = g_strdup (filename);
  priv->callback = callback;
  priv->include_frame = include_frame;
  priv->include_cursor = include_cursor;

  stage = CLUTTER_ACTOR (shell_global_get_stage (priv->global));

  meta_disable_unredirect_for_display (shell_global_get_display (shell_global_get ()));

  g_signal_connect_after (stage, "paint", G_CALLBACK (grab_window_screenshot), (gpointer)screenshot);

  clutter_actor_queue_redraw (stage);
}

ShellScreenshot *
shell_screenshot_new (void)
{
  return g_object_new (SHELL_TYPE_SCREENSHOT, NULL);
}
