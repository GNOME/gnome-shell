/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <meta/display.h>
#include <meta/util.h>
#include <meta/meta-plugin.h>
#include <meta/meta-cursor-tracker.h>
#include <st/st.h>

#include "shell-global.h"
#include "shell-screenshot.h"
#include "shell-util.h"

typedef enum _ShellScreenshotFlag
{
  SHELL_SCREENSHOT_FLAG_NONE,
  SHELL_SCREENSHOT_FLAG_INCLUDE_CURSOR,
} ShellScreenshotFlag;

typedef enum _ShellScreenshotMode
{
  SHELL_SCREENSHOT_SCREEN,
  SHELL_SCREENSHOT_WINDOW,
  SHELL_SCREENSHOT_AREA,
} ShellScreenshotMode;

enum
{
  SCREENSHOT_TAKEN,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

typedef struct _ShellScreenshotPrivate  ShellScreenshotPrivate;

struct _ShellScreenshot
{
  GObject parent_instance;

  ShellScreenshotPrivate *priv;
};

struct _ShellScreenshotPrivate
{
  ShellGlobal *global;

  GOutputStream *stream;
  ShellScreenshotFlag flags;
  ShellScreenshotMode mode;

  GDateTime *datetime;

  cairo_surface_t *image;
  cairo_rectangle_int_t screenshot_area;

  gboolean include_frame;

  float scale;
  ClutterContent *cursor_content;
  graphene_point_t cursor_point;
  float cursor_scale;
};

G_DEFINE_TYPE_WITH_PRIVATE (ShellScreenshot, shell_screenshot, G_TYPE_OBJECT);

static void
shell_screenshot_class_init (ShellScreenshotClass *screenshot_class)
{
  signals[SCREENSHOT_TAKEN] =
    g_signal_new ("screenshot-taken",
                  G_TYPE_FROM_CLASS(screenshot_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  META_TYPE_RECTANGLE);
}

static void
shell_screenshot_init (ShellScreenshot *screenshot)
{
  screenshot->priv = shell_screenshot_get_instance_private (screenshot);
  screenshot->priv->global = shell_global_get ();
}

static void
on_screenshot_written (GObject      *source,
                       GAsyncResult *task,
                       gpointer      user_data)
{
  ShellScreenshot *screenshot = SHELL_SCREENSHOT (source);
  ShellScreenshotPrivate *priv = screenshot->priv;
  GTask *result = user_data;

  g_task_return_boolean (result, g_task_propagate_boolean (G_TASK (task), NULL));
  g_object_unref (result);

  g_clear_pointer (&priv->image, cairo_surface_destroy);
  g_clear_object (&priv->stream);
  g_clear_pointer (&priv->datetime, g_date_time_unref);
}

static cairo_format_t
util_cairo_format_for_content (cairo_content_t content)
{
  switch (content)
    {
    case CAIRO_CONTENT_COLOR:
      return CAIRO_FORMAT_RGB24;
    case CAIRO_CONTENT_ALPHA:
      return CAIRO_FORMAT_A8;
    case CAIRO_CONTENT_COLOR_ALPHA:
    default:
      return CAIRO_FORMAT_ARGB32;
    }
}

static cairo_surface_t *
util_cairo_surface_coerce_to_image (cairo_surface_t *surface,
                                    cairo_content_t  content,
                                    int              src_x,
                                    int              src_y,
                                    int              width,
                                    int              height)
{
  cairo_surface_t *copy;
  cairo_t *cr;

  copy = cairo_image_surface_create (util_cairo_format_for_content (content),
                                     width,
                                     height);

  cr = cairo_create (copy);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_surface (cr, surface, -src_x, -src_y);
  cairo_paint (cr);
  cairo_destroy (cr);

  return copy;
}

static void
convert_alpha (guchar *dest_data,
               int     dest_stride,
               guchar *src_data,
               int     src_stride,
               int     src_x,
               int     src_y,
               int     width,
               int     height)
{
  int x, y;

  src_data += src_stride * src_y + src_x * 4;

  for (y = 0; y < height; y++)
    {
      uint32_t *src = (guint32 *) src_data;

      for (x = 0; x < width; x++)
        {
          unsigned int alpha = src[x] >> 24;

          if (alpha == 0)
            {
              dest_data[x * 4 + 0] = 0;
              dest_data[x * 4 + 1] = 0;
              dest_data[x * 4 + 2] = 0;
            }
          else
            {
              dest_data[x * 4 + 0] = (((src[x] & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
              dest_data[x * 4 + 1] = (((src[x] & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
              dest_data[x * 4 + 2] = (((src[x] & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
            }
          dest_data[x * 4 + 3] = alpha;
        }

      src_data += src_stride;
      dest_data += dest_stride;
    }
}

static void
convert_no_alpha (guchar *dest_data,
                  int     dest_stride,
                  guchar *src_data,
                  int     src_stride,
                  int     src_x,
                  int     src_y,
                  int     width,
                  int     height)
{
  int x, y;

  src_data += src_stride * src_y + src_x * 4;

  for (y = 0; y < height; y++)
    {
      uint32_t *src = (uint32_t *) src_data;

      for (x = 0; x < width; x++)
        {
          dest_data[x * 3 + 0] = src[x] >> 16;
          dest_data[x * 3 + 1] = src[x] >>  8;
          dest_data[x * 3 + 2] = src[x];
        }

      src_data += src_stride;
      dest_data += dest_stride;
    }
}

static GdkPixbuf *
util_pixbuf_from_surface (cairo_surface_t *surface,
                          gint             src_x,
                          gint             src_y,
                          gint             width,
                          gint             height)
{
  cairo_content_t content;
  GdkPixbuf *dest;

  /* General sanity checks */
  g_return_val_if_fail (surface != NULL, NULL);
  g_return_val_if_fail (width > 0 && height > 0, NULL);

  content = cairo_surface_get_content (surface) | CAIRO_CONTENT_COLOR;
  dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                         !!(content & CAIRO_CONTENT_ALPHA),
                         8,
                         width, height);

  if (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE &&
      cairo_image_surface_get_format (surface) == util_cairo_format_for_content (content))
    {
      surface = cairo_surface_reference (surface);
    }
  else
    {
      surface = util_cairo_surface_coerce_to_image (surface, content,
                                                    src_x, src_y,
                                                    width, height);
      src_x = 0;
      src_y = 0;
    }
  cairo_surface_flush (surface);
  if (cairo_surface_status (surface) || dest == NULL)
    {
      cairo_surface_destroy (surface);
      g_clear_object (&dest);
      return NULL;
    }

  if (gdk_pixbuf_get_has_alpha (dest))
    {
      convert_alpha (gdk_pixbuf_get_pixels (dest),
                     gdk_pixbuf_get_rowstride (dest),
                     cairo_image_surface_get_data (surface),
                     cairo_image_surface_get_stride (surface),
                     src_x, src_y,
                     width, height);
    }
  else
    {
      convert_no_alpha (gdk_pixbuf_get_pixels (dest),
                        gdk_pixbuf_get_rowstride (dest),
                        cairo_image_surface_get_data (surface),
                        cairo_image_surface_get_stride (surface),
                        src_x, src_y,
                        width, height);
    }

  cairo_surface_destroy (surface);

  return dest;
}

static void
write_screenshot_thread (GTask        *result,
                         gpointer      object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  ShellScreenshot *screenshot = SHELL_SCREENSHOT (object);
  ShellScreenshotPrivate *priv;
  g_autoptr (GOutputStream) stream = NULL;
  g_autoptr(GdkPixbuf) pixbuf = NULL;
  g_autofree char *creation_time = NULL;
  GError *error = NULL;

  g_assert (screenshot != NULL);

  priv = screenshot->priv;

  stream = g_object_ref (priv->stream);

  pixbuf = util_pixbuf_from_surface (priv->image,
                                     0, 0,
                                     cairo_image_surface_get_width (priv->image),
                                     cairo_image_surface_get_height (priv->image));
  creation_time = g_date_time_format (priv->datetime, "%c");

  if (!creation_time)
    creation_time = g_date_time_format (priv->datetime, "%FT%T%z");

  gdk_pixbuf_save_to_stream (pixbuf, stream, "png", NULL, &error,
                             "tEXt::Software", "gnome-screenshot",
                             "tEXt::Creation Time", creation_time,
                             NULL);

  if (error)
    g_task_return_error (result, error);
  else
    g_task_return_boolean (result, TRUE);
}

static void
do_grab_screenshot (ShellScreenshot     *screenshot,
                    int                  x,
                    int                  y,
                    int                  width,
                    int                  height,
                    ShellScreenshotFlag  flags)
{
  ShellScreenshotPrivate *priv = screenshot->priv;
  ClutterStage *stage = shell_global_get_stage (priv->global);
  cairo_rectangle_int_t screenshot_rect = { x, y, width, height };
  int image_width;
  int image_height;
  float scale;
  cairo_surface_t *image;
  ClutterPaintFlag paint_flags = CLUTTER_PAINT_FLAG_NONE;
  g_autoptr (GError) error = NULL;

  clutter_stage_get_capture_final_size (stage, &screenshot_rect,
                                        &image_width,
                                        &image_height,
                                        &scale);
  image = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                      image_width, image_height);

  if (flags & SHELL_SCREENSHOT_FLAG_INCLUDE_CURSOR)
    paint_flags |= CLUTTER_PAINT_FLAG_FORCE_CURSORS;
  else
    paint_flags |= CLUTTER_PAINT_FLAG_NO_CURSORS;
  if (!clutter_stage_paint_to_buffer (stage, &screenshot_rect, scale,
                                      cairo_image_surface_get_data (image),
                                      cairo_image_surface_get_stride (image),
                                      CLUTTER_CAIRO_FORMAT_ARGB32,
                                      paint_flags,
                                      &error))
    {
      cairo_surface_destroy (image);
      g_warning ("Failed to take screenshot: %s", error->message);
      return;
    }

  priv->image = image;

  priv->datetime = g_date_time_new_now_local ();
}

static void
draw_cursor_image (cairo_surface_t       *surface,
                   cairo_rectangle_int_t  area)
{
  CoglTexture *texture;
  int width, height;
  int stride;
  guint8 *data;
  MetaDisplay *display;
  MetaCursorTracker *tracker;
  cairo_surface_t *cursor_surface;
  cairo_region_t *screenshot_region;
  cairo_t *cr;
  int x, y;
  int xhot, yhot;
  double xscale, yscale;
  graphene_point_t point;

  display = shell_global_get_display (shell_global_get ());
  tracker = meta_cursor_tracker_get_for_display (display);
  texture = meta_cursor_tracker_get_sprite (tracker);

  if (!texture)
    return;

  screenshot_region = cairo_region_create_rectangle (&area);
  meta_cursor_tracker_get_pointer (tracker, &point, NULL);
  x = point.x;
  y = point.y;

  if (!cairo_region_contains_point (screenshot_region, point.x, point.y))
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

  cairo_surface_get_device_scale (surface, &xscale, &yscale);

  if (xscale != 1.0 || yscale != 1.0)
    {
      int monitor;
      float monitor_scale;
      MetaRectangle cursor_rect = {
        .x = x, .y = y, .width = width, .height = height
      };

      monitor = meta_display_get_monitor_index_for_rect (display, &cursor_rect);
      monitor_scale = meta_display_get_monitor_scale (display, monitor);

      cairo_surface_set_device_scale (cursor_surface, monitor_scale, monitor_scale);
    }

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
grab_screenshot (ShellScreenshot     *screenshot,
                 ShellScreenshotFlag  flags,
                 GTask               *result)
{
  ShellScreenshotPrivate *priv = screenshot->priv;
  MetaDisplay *display;
  int width, height;
  GTask *task;

  display = shell_global_get_display (priv->global);
  meta_display_get_size (display, &width, &height);

  do_grab_screenshot (screenshot,
                      0, 0, width, height,
                      flags);

  priv->screenshot_area.x = 0;
  priv->screenshot_area.y = 0;
  priv->screenshot_area.width = width;
  priv->screenshot_area.height = height;

  task = g_task_new (screenshot, NULL, on_screenshot_written, result);
  g_task_run_in_thread (task, write_screenshot_thread);
  g_object_unref (task);
}

static void
grab_screenshot_content (ShellScreenshot *screenshot,
                         GTask           *result)
{
  ShellScreenshotPrivate *priv = screenshot->priv;
  MetaDisplay *display;
  int width, height;
  cairo_rectangle_int_t screenshot_rect;
  ClutterStage *stage;
  int image_width;
  int image_height;
  float scale;
  g_autoptr (GError) error = NULL;
  g_autoptr (ClutterContent) content = NULL;
  g_autoptr (GTask) task = result;
  MetaCursorTracker *tracker;
  CoglTexture *cursor_texture;
  int cursor_hot_x, cursor_hot_y;

  display = shell_global_get_display (priv->global);
  meta_display_get_size (display, &width, &height);
  screenshot_rect = (cairo_rectangle_int_t) {
      .x = 0,
      .y = 0,
      .width = width,
      .height = height,
  };

  stage = shell_global_get_stage (priv->global);

  clutter_stage_get_capture_final_size (stage, &screenshot_rect,
                                        &image_width,
                                        &image_height,
                                        &scale);

  priv->scale = scale;

  content = clutter_stage_paint_to_content (stage, &screenshot_rect, scale,
                                            CLUTTER_PAINT_FLAG_NO_CURSORS,
                                            &error);
  if (!content)
    {
      g_task_return_error (result, g_steal_pointer (&error));
      return;
    }

  tracker = meta_cursor_tracker_get_for_display (display);
  cursor_texture = meta_cursor_tracker_get_sprite (tracker);

  // If the cursor is invisible, the texture is NULL.
  if (cursor_texture)
    {
      unsigned int width, height;
      CoglContext *ctx;
      CoglPipeline *pipeline;
      CoglTexture2D *texture;
      CoglOffscreen *offscreen;
      ClutterStageView *view;

      // Copy the texture to prevent it from changing shortly after.
      width = cogl_texture_get_width (cursor_texture);
      height = cogl_texture_get_height (cursor_texture);

      ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());

      texture = cogl_texture_2d_new_with_size (ctx, width, height);
      offscreen = cogl_offscreen_new_with_texture (texture);
      cogl_framebuffer_clear4f (COGL_FRAMEBUFFER (offscreen),
                                COGL_BUFFER_BIT_COLOR,
                                0, 0, 0, 0);

      pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_layer_texture (pipeline, 0, cursor_texture);

      cogl_framebuffer_draw_textured_rectangle (COGL_FRAMEBUFFER (offscreen),
                                                pipeline,
                                                -1, 1, 1, -1,
                                                0, 0, 1, 1);
      cogl_object_unref (pipeline);
      g_object_unref (offscreen);

      priv->cursor_content =
        clutter_texture_content_new_from_texture (texture, NULL);
      cogl_object_unref (texture);

      priv->cursor_scale = meta_cursor_tracker_get_scale (tracker);

      meta_cursor_tracker_get_pointer (tracker, &priv->cursor_point, NULL);

      view = clutter_stage_get_view_at (stage,
                                        priv->cursor_point.x,
                                        priv->cursor_point.y);

      meta_cursor_tracker_get_hot (tracker, &cursor_hot_x, &cursor_hot_y);
      priv->cursor_point.x -= cursor_hot_x * priv->cursor_scale;
      priv->cursor_point.y -= cursor_hot_y * priv->cursor_scale;

      // Align the coordinates to the pixel grid the same way it's done in
      // MetaCursorRenderer.
      if (view)
        {
          cairo_rectangle_int_t view_layout;
          float view_scale;

          clutter_stage_view_get_layout (view, &view_layout);
          view_scale = clutter_stage_view_get_scale (view);

          priv->cursor_point.x -= view_layout.x;
          priv->cursor_point.y -= view_layout.y;

          priv->cursor_point.x =
              floorf (priv->cursor_point.x * view_scale) / view_scale;
          priv->cursor_point.y =
              floorf (priv->cursor_point.y * view_scale) / view_scale;

          priv->cursor_point.x += view_layout.x;
          priv->cursor_point.y += view_layout.y;
        }
    }

  g_task_return_pointer (result, g_steal_pointer (&content), g_object_unref);
}

static void
grab_window_screenshot (ShellScreenshot     *screenshot,
                        ShellScreenshotFlag  flags,
                        GTask               *result)
{
  ShellScreenshotPrivate *priv = screenshot->priv;
  GTask *task;
  MetaDisplay *display = shell_global_get_display (priv->global);
  MetaWindow *window = meta_display_get_focus_window (display);
  ClutterActor *window_actor;
  gfloat actor_x, actor_y;
  MetaRectangle rect;

  window_actor = CLUTTER_ACTOR (meta_window_get_compositor_private (window));
  clutter_actor_get_position (window_actor, &actor_x, &actor_y);

  meta_window_get_frame_rect (window, &rect);

  if (!priv->include_frame)
    meta_window_frame_rect_to_client_rect (window, &rect, &rect);

  priv->screenshot_area = rect;

  priv->image = meta_window_actor_get_image (META_WINDOW_ACTOR (window_actor),
                                             NULL);

  if (!priv->image)
    {
      g_task_report_new_error (screenshot, on_screenshot_written, result, NULL,
                               G_IO_ERROR, G_IO_ERROR_FAILED,
                               "Capturing window failed");
      return;
    }

  priv->datetime = g_date_time_new_now_local ();

  if (flags & SHELL_SCREENSHOT_FLAG_INCLUDE_CURSOR)
    {
      if (meta_window_get_client_type (window) == META_WINDOW_CLIENT_TYPE_WAYLAND)
        {
          float resource_scale;
          resource_scale = clutter_actor_get_resource_scale (window_actor);

          cairo_surface_set_device_scale (priv->image, resource_scale, resource_scale);
        }

      draw_cursor_image (priv->image, priv->screenshot_area);
    }

  g_signal_emit (screenshot, signals[SCREENSHOT_TAKEN], 0, &rect);

  task = g_task_new (screenshot, NULL, on_screenshot_written, result);
  g_task_run_in_thread (task, write_screenshot_thread);
  g_object_unref (task);
}

static gboolean
finish_screenshot (ShellScreenshot        *screenshot,
                   GAsyncResult           *result,
                   cairo_rectangle_int_t **area,
                   GError                **error)
{
  ShellScreenshotPrivate *priv = screenshot->priv;

  if (!g_task_propagate_boolean (G_TASK (result), error))
    return FALSE;

  if (area)
    *area = &priv->screenshot_area;

  return TRUE;
}

static void
on_after_paint (ClutterStage     *stage,
                ClutterStageView *view,
                ClutterFrame     *frame,
                GTask            *result)
{
  ShellScreenshot *screenshot = g_task_get_task_data (result);
  ShellScreenshotPrivate *priv = screenshot->priv;
  MetaDisplay *display = shell_global_get_display (priv->global);
  GTask *task;

  g_signal_handlers_disconnect_by_func (stage, on_after_paint, result);

  if (priv->mode == SHELL_SCREENSHOT_AREA)
    {
      do_grab_screenshot (screenshot,
                          priv->screenshot_area.x,
                          priv->screenshot_area.y,
                          priv->screenshot_area.width,
                          priv->screenshot_area.height,
                          priv->flags);

      task = g_task_new (screenshot, NULL, on_screenshot_written, result);
      g_task_run_in_thread (task, write_screenshot_thread);
    }
  else
    {
      grab_screenshot (screenshot, priv->flags, result);
    }

  g_signal_emit (screenshot, signals[SCREENSHOT_TAKEN], 0,
                 (cairo_rectangle_int_t *) &priv->screenshot_area);

  meta_enable_unredirect_for_display (display);
}

/**
 * shell_screenshot_screenshot:
 * @screenshot: the #ShellScreenshot
 * @include_cursor: Whether to include the cursor or not
 * @stream: The stream for the screenshot
 * @callback: (scope async): function to call returning success or failure
 * of the async grabbing
 * @user_data: the data to pass to callback function
 *
 * Takes a screenshot of the whole screen
 * in @stream as png image.
 *
 */
void
shell_screenshot_screenshot (ShellScreenshot     *screenshot,
                             gboolean             include_cursor,
                             GOutputStream       *stream,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  ShellScreenshotPrivate *priv;
  GTask *result;
  ShellScreenshotFlag flags;

  g_return_if_fail (SHELL_IS_SCREENSHOT (screenshot));
  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));

  priv = screenshot->priv;

  if (priv->stream != NULL) {
    if (callback)
      g_task_report_new_error (screenshot,
                               callback,
                               user_data,
                               shell_screenshot_screenshot,
                               G_IO_ERROR,
                               G_IO_ERROR_PENDING,
                               "Only one screenshot operation at a time "
                               "is permitted");
    return;
  }

  result = g_task_new (screenshot, NULL, callback, user_data);
  g_task_set_source_tag (result, shell_screenshot_screenshot);
  g_task_set_task_data (result, screenshot, NULL);

  priv->stream = g_object_ref (stream);

  flags = SHELL_SCREENSHOT_FLAG_NONE;
  if (include_cursor)
    flags |= SHELL_SCREENSHOT_FLAG_INCLUDE_CURSOR;

  if (meta_is_wayland_compositor ())
    {
      grab_screenshot (screenshot, flags, result);

      g_signal_emit (screenshot, signals[SCREENSHOT_TAKEN], 0,
                     (cairo_rectangle_int_t *) &priv->screenshot_area);
    }
  else
    {
      MetaDisplay *display = shell_global_get_display (priv->global);
      ClutterStage *stage = shell_global_get_stage (priv->global);

      meta_disable_unredirect_for_display (display);
      clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
      priv->flags = flags;
      priv->mode = SHELL_SCREENSHOT_SCREEN;
      g_signal_connect (stage, "after-paint",
                        G_CALLBACK (on_after_paint), result);
    }
}

/**
 * shell_screenshot_screenshot_finish:
 * @screenshot: the #ShellScreenshot
 * @result: the #GAsyncResult that was provided to the callback
 * @area: (out) (transfer none): the area that was grabbed in screen coordinates
 * @error: #GError for error reporting
 *
 * Finish the asynchronous operation started by shell_screenshot_screenshot()
 * and obtain its result.
 *
 * Returns: whether the operation was successful
 *
 */
gboolean
shell_screenshot_screenshot_finish (ShellScreenshot        *screenshot,
                                    GAsyncResult           *result,
                                    cairo_rectangle_int_t **area,
                                    GError                **error)
{
  g_return_val_if_fail (SHELL_IS_SCREENSHOT (screenshot), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result,
                                                  shell_screenshot_screenshot),
                        FALSE);
  return finish_screenshot (screenshot, result, area, error);
}

static void
screenshot_stage_to_content_on_after_paint (ClutterStage     *stage,
                                            ClutterStageView *view,
                                            ClutterFrame     *frame,
                                            GTask            *result)
{
  ShellScreenshot *screenshot = g_task_get_task_data (result);
  ShellScreenshotPrivate *priv = screenshot->priv;
  MetaDisplay *display = shell_global_get_display (priv->global);

  g_signal_handlers_disconnect_by_func (stage,
                                        screenshot_stage_to_content_on_after_paint,
                                        result);

  meta_enable_unredirect_for_display (display);

  grab_screenshot_content (screenshot, result);
}

/**
 * shell_screenshot_screenshot_stage_to_content:
 * @screenshot: the #ShellScreenshot
 * @callback: (scope async): function to call returning success or failure
 * of the async grabbing
 * @user_data: the data to pass to callback function
 *
 * Takes a screenshot of the whole screen as #ClutterContent.
 *
 */
void
shell_screenshot_screenshot_stage_to_content (ShellScreenshot     *screenshot,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  ShellScreenshotPrivate *priv;
  GTask *result;

  g_return_if_fail (SHELL_IS_SCREENSHOT (screenshot));

  result = g_task_new (screenshot, NULL, callback, user_data);
  g_task_set_source_tag (result, shell_screenshot_screenshot_stage_to_content);
  g_task_set_task_data (result, screenshot, NULL);

  if (meta_is_wayland_compositor ())
    {
      grab_screenshot_content (screenshot, result);
    }
  else
    {
      priv = screenshot->priv;

      MetaDisplay *display = shell_global_get_display (priv->global);
      ClutterStage *stage = shell_global_get_stage (priv->global);

      meta_disable_unredirect_for_display (display);
      clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
      g_signal_connect (stage, "after-paint",
                        G_CALLBACK (screenshot_stage_to_content_on_after_paint),
                        result);
    }
}

/**
 * shell_screenshot_screenshot_stage_to_content_finish:
 * @screenshot: the #ShellScreenshot
 * @result: the #GAsyncResult that was provided to the callback
 * @scale: (out) (optional): location to store the content scale
 * @cursor_content: (out) (optional): location to store the cursor content
 * @cursor_point: (out) (optional): location to store the point at which to
 * draw the cursor content
 * @cursor_scale: (out) (optional): location to store the cursor scale
 * @error: #GError for error reporting
 *
 * Finish the asynchronous operation started by
 * shell_screenshot_screenshot_stage_to_content() and obtain its result.
 *
 * Returns: (transfer full): the #ClutterContent, or NULL
 *
 */
ClutterContent *
shell_screenshot_screenshot_stage_to_content_finish (ShellScreenshot   *screenshot,
                                                     GAsyncResult      *result,
                                                     float             *scale,
                                                     ClutterContent   **cursor_content,
                                                     graphene_point_t  *cursor_point,
                                                     float             *cursor_scale,
                                                     GError           **error)
{
  ShellScreenshotPrivate *priv = screenshot->priv;
  ClutterContent *content;

  g_return_val_if_fail (SHELL_IS_SCREENSHOT (screenshot), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result,
                                                  shell_screenshot_screenshot_stage_to_content),
                        FALSE);

  content = g_task_propagate_pointer (G_TASK (result), error);
  if (!content)
    return NULL;

  if (scale)
    *scale = priv->scale;

  if (cursor_content)
    *cursor_content = g_steal_pointer (&priv->cursor_content);
  else
    g_clear_pointer (&priv->cursor_content, g_object_unref);

  if (cursor_point)
    *cursor_point = priv->cursor_point;

  if (cursor_scale)
    *cursor_scale = priv->cursor_scale;

  return content;
}

/**
 * shell_screenshot_screenshot_area:
 * @screenshot: the #ShellScreenshot
 * @x: The X coordinate of the area
 * @y: The Y coordinate of the area
 * @width: The width of the area
 * @height: The height of the area
 * @stream: The stream for the screenshot
 * @callback: (scope async): function to call returning success or failure
 * of the async grabbing
 * @user_data: the data to pass to callback function
 *
 * Takes a screenshot of the passed in area and saves it
 * in @stream as png image.
 *
 */
void
shell_screenshot_screenshot_area (ShellScreenshot     *screenshot,
                                  int                  x,
                                  int                  y,
                                  int                  width,
                                  int                  height,
                                  GOutputStream       *stream,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  ShellScreenshotPrivate *priv;
  GTask *result;
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (SHELL_IS_SCREENSHOT (screenshot));
  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));

  priv = screenshot->priv;

  if (priv->stream != NULL) {
    if (callback)
      g_task_report_new_error (screenshot,
                               callback,
                               NULL,
                               shell_screenshot_screenshot_area,
                               G_IO_ERROR,
                               G_IO_ERROR_PENDING,
                               "Only one screenshot operation at a time "
                               "is permitted");
    return;
  }

  result = g_task_new (screenshot, NULL, callback, user_data);
  g_task_set_source_tag (result, shell_screenshot_screenshot_area);
  g_task_set_task_data (result, screenshot, NULL);

  priv->stream = g_object_ref (stream);
  priv->screenshot_area.x = x;
  priv->screenshot_area.y = y;
  priv->screenshot_area.width = width;
  priv->screenshot_area.height = height;


  if (meta_is_wayland_compositor ())
    {
      do_grab_screenshot (screenshot,
                          priv->screenshot_area.x,
                          priv->screenshot_area.y,
                          priv->screenshot_area.width,
                          priv->screenshot_area.height,
                          SHELL_SCREENSHOT_FLAG_NONE);

      g_signal_emit (screenshot, signals[SCREENSHOT_TAKEN], 0,
                     (cairo_rectangle_int_t *) &priv->screenshot_area);

      task = g_task_new (screenshot, NULL, on_screenshot_written, result);
      g_task_run_in_thread (task, write_screenshot_thread);
    }
  else
    {
      MetaDisplay *display = shell_global_get_display (priv->global);
      ClutterStage *stage = shell_global_get_stage (priv->global);

      meta_disable_unredirect_for_display (display);
      clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
      priv->flags = SHELL_SCREENSHOT_FLAG_NONE;
      priv->mode = SHELL_SCREENSHOT_AREA;
      g_signal_connect (stage, "after-paint",
                        G_CALLBACK (on_after_paint), result);
    }
}

/**
 * shell_screenshot_screenshot_area_finish:
 * @screenshot: the #ShellScreenshot
 * @result: the #GAsyncResult that was provided to the callback
 * @area: (out) (transfer none): the area that was grabbed in screen coordinates
 * @error: #GError for error reporting
 *
 * Finish the asynchronous operation started by shell_screenshot_screenshot_area()
 * and obtain its result.
 *
 * Returns: whether the operation was successful
 *
 */
gboolean
shell_screenshot_screenshot_area_finish (ShellScreenshot        *screenshot,
                                         GAsyncResult           *result,
                                         cairo_rectangle_int_t **area,
                                         GError                **error)
{
  g_return_val_if_fail (SHELL_IS_SCREENSHOT (screenshot), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result,
                                                  shell_screenshot_screenshot_area),
                        FALSE);
  return finish_screenshot (screenshot, result, area, error);
}

/**
 * shell_screenshot_screenshot_window:
 * @screenshot: the #ShellScreenshot
 * @include_frame: Whether to include the frame or not
 * @include_cursor: Whether to include the cursor or not
 * @stream: The stream for the screenshot
 * @callback: (scope async): function to call returning success or failure
 * of the async grabbing
 * @user_data: the data to pass to callback function
 *
 * Takes a screenshot of the focused window (optionally omitting the frame)
 * in @stream as png image.
 *
 */
void
shell_screenshot_screenshot_window (ShellScreenshot     *screenshot,
                                    gboolean             include_frame,
                                    gboolean             include_cursor,
                                    GOutputStream       *stream,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  ShellScreenshotPrivate *priv;
  MetaDisplay *display;
  MetaWindow *window;
  GTask *result;

  g_return_if_fail (SHELL_IS_SCREENSHOT (screenshot));
  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));

  priv = screenshot->priv;
  display = shell_global_get_display (priv->global);
  window = meta_display_get_focus_window (display);

  if (priv->stream != NULL || !window) {
    if (callback)
      g_task_report_new_error (screenshot,
                               callback,
                               NULL,
                               shell_screenshot_screenshot_window,
                               G_IO_ERROR,
                               G_IO_ERROR_PENDING,
                               "Only one screenshot operation at a time "
                               "is permitted");
    return;
  }

  result = g_task_new (screenshot, NULL, callback, user_data);
  g_task_set_source_tag (result, shell_screenshot_screenshot_window);

  priv->stream = g_object_ref (stream);
  priv->include_frame = include_frame;

  grab_window_screenshot (screenshot, include_cursor, result);
}

/**
 * shell_screenshot_screenshot_window_finish:
 * @screenshot: the #ShellScreenshot
 * @result: the #GAsyncResult that was provided to the callback
 * @area: (out) (transfer none): the area that was grabbed in screen coordinates
 * @error: #GError for error reporting
 *
 * Finish the asynchronous operation started by shell_screenshot_screenshot_window()
 * and obtain its result.
 *
 * Returns: whether the operation was successful
 *
 */
gboolean
shell_screenshot_screenshot_window_finish (ShellScreenshot        *screenshot,
                                           GAsyncResult           *result,
                                           cairo_rectangle_int_t **area,
                                           GError                **error)
{
  g_return_val_if_fail (SHELL_IS_SCREENSHOT (screenshot), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result,
                                                  shell_screenshot_screenshot_window),
                        FALSE);
  return finish_screenshot (screenshot, result, area, error);
}

/**
 * shell_screenshot_pick_color:
 * @screenshot: the #ShellScreenshot
 * @x: The X coordinate to pick
 * @y: The Y coordinate to pick
 * @callback: (scope async): function to call returning success or failure
 * of the async grabbing
 *
 * Picks the pixel at @x, @y and returns its color as #ClutterColor.
 *
 */
void
shell_screenshot_pick_color (ShellScreenshot     *screenshot,
                             int                  x,
                             int                  y,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  ShellScreenshotPrivate *priv;
  g_autoptr (GTask) result = NULL;

  g_return_if_fail (SHELL_IS_SCREENSHOT (screenshot));

  result = g_task_new (screenshot, NULL, callback, user_data);
  g_task_set_source_tag (result, shell_screenshot_pick_color);

  priv = screenshot->priv;

  priv->screenshot_area.x = x;
  priv->screenshot_area.y = y;
  priv->screenshot_area.width = 1;
  priv->screenshot_area.height = 1;

  do_grab_screenshot (screenshot,
                      priv->screenshot_area.x,
                      priv->screenshot_area.y,
                      1,
                      1,
                      SHELL_SCREENSHOT_FLAG_NONE);

  g_task_return_boolean (result, TRUE);
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define INDEX_A 3
#define INDEX_R 2
#define INDEX_G 1
#define INDEX_B 0
#else
#define INDEX_A 0
#define INDEX_R 1
#define INDEX_G 2
#define INDEX_B 3
#endif

/**
 * shell_screenshot_pick_color_finish:
 * @screenshot: the #ShellScreenshot
 * @result: the #GAsyncResult that was provided to the callback
 * @color: (out caller-allocates): the picked color
 * @error: #GError for error reporting
 *
 * Finish the asynchronous operation started by shell_screenshot_pick_color()
 * and obtain its result.
 *
 * Returns: whether the operation was successful
 *
 */
gboolean
shell_screenshot_pick_color_finish (ShellScreenshot  *screenshot,
                                    GAsyncResult     *result,
                                    ClutterColor     *color,
                                    GError          **error)
{
  ShellScreenshotPrivate *priv;

  g_return_val_if_fail (SHELL_IS_SCREENSHOT (screenshot), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result,
                                                  shell_screenshot_pick_color),
                        FALSE);

  if (!g_task_propagate_boolean (G_TASK (result), error))
    return FALSE;

  priv = screenshot->priv;

  /* protect against mutter changing the format used for stage captures */
  g_assert (cairo_image_surface_get_format (priv->image) == CAIRO_FORMAT_ARGB32);

  if (color)
    {
      uint8_t *data = cairo_image_surface_get_data (priv->image);

      color->alpha = data[INDEX_A];
      color->red   = data[INDEX_R];
      color->green = data[INDEX_G];
      color->blue  = data[INDEX_B];
    }

  return TRUE;
}

#undef INDEX_A
#undef INDEX_R
#undef INDEX_G
#undef INDEX_B

static void
composite_to_stream_on_png_saved (GObject      *pixbuf,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  GError *error = NULL;

  if (!gdk_pixbuf_save_to_stream_finish (result, &error))
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, g_object_ref (pixbuf), g_object_unref);

  g_object_unref (task);
}

/**
 * shell_screenshot_composite_to_stream:
 * @texture: the source texture
 * @x: x coordinate of the rectangle
 * @y: y coordinate of the rectangle
 * @width: width of the rectangle, or -1 to use the full texture
 * @height: height of the rectangle, or -1 to use the full texture
 * @scale: scale of the source texture
 * @cursor: (nullable): the cursor texture
 * @cursor_x: x coordinate to put the cursor texture at, relative to the full
 * source texture
 * @cursor_y: y coordinate to put the cursor texture at, relative to the full
 * source texture
 * @cursor_scale: scale of the cursor texture
 * @stream: the stream to write the PNG image into
 * @callback: (scope async): function to call returning success or failure
 * @user_data: the data to pass to callback function
 *
 * Composite a rectangle defined by x, y, width, height from the texture to a
 * pixbuf and write it as a PNG image into the stream.
 *
 */
void
shell_screenshot_composite_to_stream (CoglTexture         *texture,
                                      int                  x,
                                      int                  y,
                                      int                  width,
                                      int                  height,
                                      float                scale,
                                      CoglTexture         *cursor,
                                      int                  cursor_x,
                                      int                  cursor_y,
                                      float                cursor_scale,
                                      GOutputStream       *stream,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  CoglContext *ctx;
  CoglTexture *sub_texture;
  cairo_surface_t *surface;
  cairo_surface_t *cursor_surface;
  cairo_t *cr;
  g_autoptr (GTask) task = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autofree char *creation_time = NULL;
  g_autoptr (GDateTime) date_time = NULL;

  task = g_task_new (NULL, NULL, callback, user_data);
  g_task_set_source_tag (task, shell_screenshot_composite_to_stream);

  if (width == -1 || height == -1)
    {
      x = 0;
      y = 0;
      width = cogl_texture_get_width (texture);
      height = cogl_texture_get_height (texture);
    }

  ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  sub_texture = cogl_sub_texture_new (ctx, texture, x, y, width, height);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        cogl_texture_get_width (sub_texture),
                                        cogl_texture_get_height (sub_texture));

  cogl_texture_get_data (sub_texture, CLUTTER_CAIRO_FORMAT_ARGB32,
                         cairo_image_surface_get_stride (surface),
                         cairo_image_surface_get_data (surface));
  cairo_surface_mark_dirty (surface);

  cogl_object_unref (sub_texture);

  cairo_surface_set_device_scale (surface, scale, scale);

  if (cursor != NULL)
    {
      // Paint the cursor on top.
      cursor_surface =
        cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                    cogl_texture_get_width (cursor),
                                    cogl_texture_get_height (cursor));
      cogl_texture_get_data (cursor, CLUTTER_CAIRO_FORMAT_ARGB32,
                             cairo_image_surface_get_stride (cursor_surface),
                             cairo_image_surface_get_data (cursor_surface));
      cairo_surface_mark_dirty (cursor_surface);

      cairo_surface_set_device_scale (cursor_surface,
                                      1 / cursor_scale,
                                      1 / cursor_scale);

      cr = cairo_create (surface);
      cairo_set_source_surface (cr, cursor_surface,
                                (cursor_x - x) / scale,
                                (cursor_y - y) / scale);
      cairo_paint (cr);
      cairo_destroy (cr);

      cairo_surface_destroy (cursor_surface);
    }

  /* Save to an image. */
  pixbuf = util_pixbuf_from_surface (surface,
                                     0, 0,
                                     cairo_image_surface_get_width (surface),
                                     cairo_image_surface_get_height (surface));
  cairo_surface_destroy (surface);

  date_time = g_date_time_new_now_local ();
  creation_time = g_date_time_format (date_time, "%c");

  if (!creation_time)
    creation_time = g_date_time_format (date_time, "%FT%T%z");

  gdk_pixbuf_save_to_stream_async (pixbuf, stream, "png", NULL,
                                   composite_to_stream_on_png_saved,
                                   g_steal_pointer (&task),
                                   "tEXt::Software", "gnome-screenshot",
                                   "tEXt::Creation Time", creation_time,
                                   NULL);
}

/**
 * shell_screenshot_composite_to_stream_finish:
 * @result: the #GAsyncResult that was provided to the callback
 * @error: #GError for error reporting
 *
 * Finish the asynchronous operation started by
 * shell_screenshot_composite_to_stream () and obtain its result.
 *
 * Returns: (transfer full) (nullable): a GdkPixbuf with the final image if the
 * operation was successful, or NULL on error.
 *
 */
GdkPixbuf *
shell_screenshot_composite_to_stream_finish (GAsyncResult  *result,
                                             GError       **error)
{
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result,
                                                  shell_screenshot_composite_to_stream),
                        FALSE);

  return g_task_propagate_pointer (G_TASK (result), error);
}

ShellScreenshot *
shell_screenshot_new (void)
{
  return g_object_new (SHELL_TYPE_SCREENSHOT, NULL);
}
