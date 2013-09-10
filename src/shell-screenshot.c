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

#define A11Y_APPS_SCHEMA "org.gnome.desktop.a11y.applications"
#define MAGNIFIER_ACTIVE_KEY "screen-magnifier-enabled"

struct _ShellScreenshotClass
{
  GObjectClass parent_class;
};

struct _ShellScreenshot
{
  GObject parent_instance;

  ShellGlobal *global;
};

/* Used for async screenshot grabbing */
typedef struct _screenshot_data {
  ShellScreenshot  *screenshot;

  char *filename;
  char *filename_used;

  cairo_surface_t *image;
  cairo_rectangle_int_t screenshot_area;

  gboolean include_cursor;

  ShellScreenshotCallback callback;
} _screenshot_data;

G_DEFINE_TYPE(ShellScreenshot, shell_screenshot, G_TYPE_OBJECT);

static void
shell_screenshot_class_init (ShellScreenshotClass *screenshot_class)
{
  (void) screenshot_class;
}

static void
shell_screenshot_init (ShellScreenshot *screenshot)
{
  screenshot->global = shell_global_get ();
}

static void
on_screenshot_written (GObject *source,
                       GAsyncResult *result,
                       gpointer user_data)
{
  _screenshot_data *screenshot_data = (_screenshot_data*) user_data;
  if (screenshot_data->callback)
    screenshot_data->callback (screenshot_data->screenshot,
                               g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (result)),
                               &screenshot_data->screenshot_area,
                               screenshot_data->filename_used);

  cairo_surface_destroy (screenshot_data->image);
  g_object_unref (screenshot_data->screenshot);
  g_free (screenshot_data->filename);
  g_free (screenshot_data->filename_used);
  g_free (screenshot_data);
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
write_screenshot_thread (GSimpleAsyncResult *result,
                         GObject *object,
                         GCancellable *cancellable)
{
  cairo_status_t status;
  GOutputStream *stream;
  _screenshot_data *screenshot_data = g_async_result_get_user_data (G_ASYNC_RESULT (result));

  g_assert (screenshot_data != NULL);

  stream = prepare_write_stream (screenshot_data->filename,
                                 &screenshot_data->filename_used);

  if (stream == NULL)
    status = CAIRO_STATUS_FILE_NOT_FOUND;
  else
    {
      GdkPixbuf *pixbuf;

      pixbuf = gdk_pixbuf_get_from_surface (screenshot_data->image,
                                            0, 0,
                                            cairo_image_surface_get_width (screenshot_data->image),
                                            cairo_image_surface_get_height (screenshot_data->image));

      if (gdk_pixbuf_save_to_stream (pixbuf, stream, "png", NULL, NULL,
                                    "tEXt::Software", "gnome-screenshot", NULL))
        status = CAIRO_STATUS_SUCCESS;
      else
        status = CAIRO_STATUS_WRITE_ERROR;

      g_object_unref (pixbuf);
    }


  g_simple_async_result_set_op_res_gboolean (result, status == CAIRO_STATUS_SUCCESS);

  g_clear_object (&stream);
}

static void
do_grab_screenshot (_screenshot_data *screenshot_data,
                    int               x,
                    int               y,
                    int               width,
                    int               height)
{
  CoglBitmap *bitmap;
  ClutterBackend *backend;
  CoglContext *context;
  int stride;
  guchar *data;

  backend = clutter_get_default_backend ();
  context = clutter_backend_get_cogl_context (backend);

  screenshot_data->image = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                       width, height);


  data = cairo_image_surface_get_data (screenshot_data->image);
  stride = cairo_image_surface_get_stride (screenshot_data->image);

  bitmap = cogl_bitmap_new_for_data (context,
                                     width,
                                     height,
                                     CLUTTER_CAIRO_FORMAT_ARGB32,
                                     stride,
                                     data);
  cogl_framebuffer_read_pixels_into_bitmap (cogl_get_draw_framebuffer (),
                                            x, y,
                                            COGL_READ_PIXELS_COLOR_BUFFER,
                                            bitmap);

  cairo_surface_mark_dirty (screenshot_data->image);
  cogl_object_unref (bitmap);
}

static void
get_pointer_coords (int *x,
                    int *y)
{
  ClutterDeviceManager *manager;
  ClutterInputDevice *device;
  ClutterPoint point;

  manager = clutter_device_manager_get_default ();
  device = clutter_device_manager_get_core_device (manager, CLUTTER_POINTER_DEVICE);

  clutter_input_device_get_coords (device, NULL, &point);
  *x = point.x;
  *y = point.y;
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

  screenshot_region = cairo_region_create_rectangle (&area);
  get_pointer_coords (&x, &y);

  if (!cairo_region_contains_point (screenshot_region, x, y))
    {
      cairo_region_destroy (screenshot_region);
      return;
    }

  texture = meta_cursor_tracker_get_sprite (tracker);
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
                 _screenshot_data *screenshot_data)
{
  MetaScreen *screen = shell_global_get_screen (screenshot_data->screenshot->global);
  MetaCursorTracker *tracker;
  int width, height;
  GSimpleAsyncResult *result;
  GSettings *settings;

  meta_screen_get_size (screen, &width, &height);

  do_grab_screenshot (screenshot_data, 0, 0, width, height);

  if (meta_screen_get_n_monitors (screen) > 1)
    {
      cairo_region_t *screen_region = cairo_region_create ();
      cairo_region_t *stage_region;
      MetaRectangle monitor_rect;
      cairo_rectangle_int_t stage_rect;
      int i;
      cairo_t *cr;

      for (i = meta_screen_get_n_monitors (screen) - 1; i >= 0; i--)
        {
          meta_screen_get_monitor_geometry (screen, i, &monitor_rect);
          cairo_region_union_rectangle (screen_region, (const cairo_rectangle_int_t *) &monitor_rect);
        }

      stage_rect.x = 0;
      stage_rect.y = 0;
      stage_rect.width = width;
      stage_rect.height = height;

      stage_region = cairo_region_create_rectangle ((const cairo_rectangle_int_t *) &stage_rect);
      cairo_region_xor (stage_region, screen_region);
      cairo_region_destroy (screen_region);

      cr = cairo_create (screenshot_data->image);

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

  screenshot_data->screenshot_area.x = 0;
  screenshot_data->screenshot_area.y = 0;
  screenshot_data->screenshot_area.width = width;
  screenshot_data->screenshot_area.height = height;

  settings = g_settings_new (A11Y_APPS_SCHEMA);
  if (screenshot_data->include_cursor &&
      !g_settings_get_boolean (settings, MAGNIFIER_ACTIVE_KEY))
    {
      tracker = meta_cursor_tracker_get_for_screen (screen);
      _draw_cursor_image (tracker, screenshot_data->image, screenshot_data->screenshot_area);
    }
  g_object_unref (settings);

  g_signal_handlers_disconnect_by_func (stage, (void *)grab_screenshot, (gpointer)screenshot_data);

  result = g_simple_async_result_new (NULL, on_screenshot_written, (gpointer)screenshot_data, grab_screenshot);
  g_simple_async_result_run_in_thread (result, write_screenshot_thread, G_PRIORITY_DEFAULT, NULL);
  g_object_unref (result);
}

static void
grab_area_screenshot (ClutterActor *stage,
                      _screenshot_data *screenshot_data)
{
  GSimpleAsyncResult *result;

  do_grab_screenshot (screenshot_data,
                      screenshot_data->screenshot_area.x,
                      screenshot_data->screenshot_area.y,
                      screenshot_data->screenshot_area.width,
                      screenshot_data->screenshot_area.height);

  g_signal_handlers_disconnect_by_func (stage, (void *)grab_area_screenshot, (gpointer)screenshot_data);
  result = g_simple_async_result_new (NULL, on_screenshot_written, (gpointer)screenshot_data, grab_area_screenshot);
  g_simple_async_result_run_in_thread (result, write_screenshot_thread, G_PRIORITY_DEFAULT, NULL);
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
  _screenshot_data *data = g_new0 (_screenshot_data, 1);

  data->screenshot = g_object_ref (screenshot);
  data->filename = g_strdup (filename);
  data->callback = callback;
  data->include_cursor = include_cursor;

  stage = CLUTTER_ACTOR (shell_global_get_stage (screenshot->global));

  g_signal_connect_after (stage, "paint", G_CALLBACK (grab_screenshot), (gpointer)data);

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
  _screenshot_data *data = g_new0 (_screenshot_data, 1);

  data->screenshot = g_object_ref (screenshot);
  data->filename = g_strdup (filename);
  data->screenshot_area.x = x;
  data->screenshot_area.y = y;
  data->screenshot_area.width = width;
  data->screenshot_area.height = height;
  data->callback = callback;

  stage = CLUTTER_ACTOR (shell_global_get_stage (screenshot->global));

  g_signal_connect_after (stage, "paint", G_CALLBACK (grab_area_screenshot), (gpointer)data);

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
  GSimpleAsyncResult *result;
  GSettings *settings;

  _screenshot_data *screenshot_data = g_new0 (_screenshot_data, 1);

  MetaScreen *screen = shell_global_get_screen (screenshot->global);
  MetaCursorTracker *tracker;
  MetaDisplay *display = meta_screen_get_display (screen);
  MetaWindow *window = meta_display_get_focus_window (display);
  ClutterActor *window_actor;
  gfloat actor_x, actor_y;
  MetaShapedTexture *stex;
  MetaRectangle rect;
  cairo_rectangle_int_t clip;

  screenshot_data->screenshot = g_object_ref (screenshot);
  screenshot_data->filename = g_strdup (filename);
  screenshot_data->callback = callback;

  if (!window)
    {
      screenshot_data->filename_used = g_strdup ("");
      result = g_simple_async_result_new (NULL, on_screenshot_written, (gpointer)screenshot_data, shell_screenshot_screenshot_window);
      g_simple_async_result_set_op_res_gboolean (result, FALSE);
      g_simple_async_result_complete (result);
      g_object_unref (result);

      return;
    }

  window_actor = CLUTTER_ACTOR (meta_window_get_compositor_private (window));
  clutter_actor_get_position (window_actor, &actor_x, &actor_y);

  if (include_frame || !meta_window_get_frame (window))
    {
      meta_window_get_outer_rect (window, &rect);

      screenshot_data->screenshot_area.x = rect.x;
      screenshot_data->screenshot_area.y = rect.y;

      clip.x = rect.x - (gint) actor_x;
      clip.y = rect.y - (gint) actor_y;
    }
  else
    {
      rect = *meta_window_get_rect (window);

      screenshot_data->screenshot_area.x = (gint) actor_x + rect.x;
      screenshot_data->screenshot_area.y = (gint) actor_y + rect.y;

      clip.x = rect.x;
      clip.y = rect.y;
    }

  clip.width = screenshot_data->screenshot_area.width = rect.width;
  clip.height = screenshot_data->screenshot_area.height = rect.height;

  stex = META_SHAPED_TEXTURE (meta_window_actor_get_texture (META_WINDOW_ACTOR (window_actor)));
  screenshot_data->image = meta_shaped_texture_get_image (stex, &clip);

  settings = g_settings_new (A11Y_APPS_SCHEMA);
  if (include_cursor && !g_settings_get_boolean (settings, MAGNIFIER_ACTIVE_KEY))
    {
      tracker = meta_cursor_tracker_get_for_screen (screen);
      _draw_cursor_image (tracker, screenshot_data->image, screenshot_data->screenshot_area);
    }
  g_object_unref (settings);

  result = g_simple_async_result_new (NULL, on_screenshot_written, (gpointer)screenshot_data, shell_screenshot_screenshot_window);
  g_simple_async_result_run_in_thread (result, write_screenshot_thread, G_PRIORITY_DEFAULT, NULL);
  g_object_unref (result);
}

ShellScreenshot *
shell_screenshot_new (void)
{
  return g_object_new (SHELL_TYPE_SCREENSHOT, NULL);
}
