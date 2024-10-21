/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#pragma once

#include <mtk/mtk.h>

/**
 * ShellScreenshot:
 *
 * Grabs screenshots of areas and/or windows
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
gboolean shell_screenshot_screenshot_area_finish (ShellScreenshot  *screenshot,
                                                  GAsyncResult     *result,
                                                  MtkRectangle    **area,
                                                  GError          **error);

void    shell_screenshot_screenshot_window    (ShellScreenshot     *screenshot,
                                               gboolean             include_frame,
                                               gboolean             include_cursor,
                                               GOutputStream       *stream,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data);
gboolean shell_screenshot_screenshot_window_finish (ShellScreenshot  *screenshot,
                                                    GAsyncResult     *result,
                                                    MtkRectangle    **area,
                                                    GError          **error);

void    shell_screenshot_screenshot           (ShellScreenshot     *screenshot,
                                               gboolean             include_cursor,
                                               GOutputStream       *stream,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data);
gboolean shell_screenshot_screenshot_finish   (ShellScreenshot  *screenshot,
                                               GAsyncResult     *result,
                                               MtkRectangle    **area,
                                               GError          **error);

void     shell_screenshot_screenshot_stage_to_content (ShellScreenshot     *screenshot,
                                                       GAsyncReadyCallback  callback,
                                                       gpointer             user_data);
ClutterContent *shell_screenshot_screenshot_stage_to_content_finish (ShellScreenshot   *screenshot,
                                                                     GAsyncResult      *result,
                                                                     float             *scale,
                                                                     ClutterContent   **cursor_content,
                                                                     graphene_point_t  *cursor_point,
                                                                     float             *cursor_scale,
                                                                     GError           **error);

void     shell_screenshot_pick_color        (ShellScreenshot      *screenshot,
                                             int                   x,
                                             int                   y,
                                             GAsyncReadyCallback   callback,
                                             gpointer              user_data);
gboolean shell_screenshot_pick_color_finish (ShellScreenshot      *screenshot,
                                             GAsyncResult         *result,
                                             CoglColor            *color,
                                             GError              **error);

void shell_screenshot_composite_to_stream (CoglTexture         *texture,
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
                                           gpointer             user_data);
GdkPixbuf *shell_screenshot_composite_to_stream_finish (GAsyncResult  *result,
                                                        GError       **error);
