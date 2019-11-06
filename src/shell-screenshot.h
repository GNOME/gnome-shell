/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_SCREENSHOT_H__
#define __SHELL_SCREENSHOT_H__

/**
 * SECTION:shell-screenshot
 * @short_description: Grabs screenshots of areas and/or windows
 *
 * The #ShellScreenshot object is used to take screenshots of screen
 * areas or windows and write them out as png files.
 *
 */
#define SHELL_TYPE_SCREENSHOT (shell_screenshot_get_type ())
G_DECLARE_FINAL_TYPE (ShellScreenshot, shell_screenshot,
                      SHELL, SCREENSHOT, GObject)

ShellScreenshot *shell_screenshot_new (void);

void    shell_screenshot_screenshot_area      (ShellScreenshot      *screenshot,
                                               int                   x,
                                               int                   y,
                                               int                   width,
                                               int                   height,
                                               GOutputStream        *stream,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
gboolean shell_screenshot_screenshot_area_finish (ShellScreenshot       *screenshot,
                                                  GAsyncResult          *result,
                                                  cairo_rectangle_int_t **area,
                                                  GError                **error);

void    shell_screenshot_screenshot_window    (ShellScreenshot     *screenshot,
                                               gboolean             include_frame,
                                               gboolean             include_cursor,
                                               GOutputStream       *stream,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data);
gboolean shell_screenshot_screenshot_window_finish (ShellScreenshot        *screenshot,
                                                    GAsyncResult           *result,
                                                    cairo_rectangle_int_t **area,
                                                    GError                **error);

void    shell_screenshot_screenshot           (ShellScreenshot     *screenshot,
                                               gboolean             include_cursor,
                                               GOutputStream       *stream,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data);
gboolean shell_screenshot_screenshot_finish   (ShellScreenshot        *screenshot,
                                               GAsyncResult           *result,
                                               cairo_rectangle_int_t **area,
                                               GError                **error);

void     shell_screenshot_pick_color        (ShellScreenshot      *screenshot,
                                             int                   x,
                                             int                   y,
                                             GAsyncReadyCallback   callback,
                                             gpointer              user_data);
gboolean shell_screenshot_pick_color_finish (ShellScreenshot      *screenshot,
                                             GAsyncResult         *result,
                                             ClutterColor         *color,
                                             GError              **error);

#endif /* ___SHELL_SCREENSHOT_H__ */
