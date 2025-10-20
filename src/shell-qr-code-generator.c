/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2024 Red Hat, Inc
 * Copyright 2024-2025 Canonical Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ray Strode <rstrode@redhat.com>
 *         Marco Trevisan <marco.trevisan@canonical.com>
 */

#include "config.h"

#include "shell-qr-code-generator.h"

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <qrencode.h>
#include <st/st.h>

#include "shell-global.h"

#define BYTES_PER_R8G8B8 3
#define BYTES_PER_R8G8B8A8 4
#define BYTES_PER_FORMAT(format) \
  ((format) == COGL_PIXEL_FORMAT_RGB_888 ? BYTES_PER_R8G8B8 : BYTES_PER_R8G8B8A8)

struct _ShellQrCodeGenerator
{
  GObject parent_instance;
};

typedef struct
{
  uint8_t *icon;
  char *uri;
  size_t width;
  size_t height;
  CoglColor *bg_color;
  CoglColor *fg_color;
  CoglPixelFormat format;
} QrCodeGenerationData;

static void
qr_code_generation_data_free (QrCodeGenerationData *data)
{
  g_clear_pointer (&data->bg_color, cogl_color_free);
  g_clear_pointer (&data->fg_color, cogl_color_free);
  g_clear_pointer (&data->icon, g_free);
  g_clear_pointer (&data->uri, g_free);
  g_free (data);
}

G_DEFINE_TYPE (ShellQrCodeGenerator, shell_qr_code_generator, G_TYPE_OBJECT);

static void
shell_qr_code_generator_class_init (ShellQrCodeGeneratorClass *qr_code_generator_class)
{
}

static void
shell_qr_code_generator_init (ShellQrCodeGenerator *qr_code_generator)
{
}

static guint8 *
colored_pixel (const CoglColor *color,
               CoglPixelFormat  pixel_format)
{
  guint8 *pixel = g_new (guint8, BYTES_PER_FORMAT (pixel_format));

  g_return_val_if_fail (pixel_format == COGL_PIXEL_FORMAT_RGB_888 ||
                        pixel_format == COGL_PIXEL_FORMAT_RGBA_8888, NULL);

  pixel[0] = color->red;
  pixel[1] = color->green;
  pixel[2] = color->blue;

  if (pixel_format == COGL_PIXEL_FORMAT_RGBA_8888)
    pixel[3] = color->alpha;

  return pixel;
}

static guint8 *
generate_icon (const char       *url,
               size_t            width,
               size_t            height,
               const CoglColor  *bg_color,
               const CoglColor  *fg_color,
               GCancellable     *cancellable,
               CoglPixelFormat  *out_pixel_format,
               GError          **error)
{
  static const CoglColor white_color = COGL_COLOR_INIT (255, 255, 255, 255);
  static const CoglColor black_color = COGL_COLOR_INIT (0, 0, 0, 255);
  QRcode *qrcode;
  g_autofree guint8 *pixel_data = NULL;
  g_autofree guint8 *bg_pixel = NULL;
  g_autofree guint8 *fg_pixel = NULL;
  CoglPixelFormat pixel_format;
  size_t pixel_size;
  size_t symbol_size;
  size_t symbols_per_row, number_of_rows;
  size_t code_width;
  size_t code_height;
  size_t offset_x;
  size_t offset_y;
  size_t row, symbol, symbol_x, symbol_y;

  g_assert (out_pixel_format);

  qrcode = QRcode_encodeString (url, 1, QR_ECLEVEL_L, QR_MODE_8, 1);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  if (!qrcode)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "QRCode generation failed for url %s",
                   url);
      return NULL;
    }

  if (!bg_color)
    bg_color = &white_color;

  if (!fg_color)
    fg_color = &black_color;

  if (bg_color->alpha == 255 && fg_color->alpha == 255)
    pixel_format = COGL_PIXEL_FORMAT_RGB_888;
  else
    pixel_format = COGL_PIXEL_FORMAT_RGBA_8888;

  pixel_size = BYTES_PER_FORMAT (pixel_format);
  symbols_per_row = qrcode->width;
  number_of_rows = qrcode->width;

  symbol_size = MIN (width, height) / symbols_per_row;
  code_width = symbol_size * symbols_per_row;
  code_height = symbol_size * number_of_rows;
  offset_x = (width - code_width) / 2;
  offset_y = (height - code_height) / 2;

  bg_pixel = colored_pixel (bg_color, pixel_format);
  fg_pixel = colored_pixel (fg_color, pixel_format);

  pixel_data = calloc (height, width * pixel_size);

  for (row = 0; row < number_of_rows; row++)
    {
      for (symbol = 0; symbol < symbols_per_row; symbol++)
        {
          guint8 *pixel;

          if (qrcode->data[row * symbols_per_row + symbol] & 1)
            pixel = fg_pixel;
          else
            pixel = bg_pixel;

          for (symbol_y = 0; symbol_y < symbol_size; symbol_y++)
            {
              for (symbol_x = 0; symbol_x < symbol_size; symbol_x++)
                {
                  size_t x, y;
                  y = offset_y + (row * symbol_size) + symbol_y;
                  x = offset_x + (symbol * symbol_size) + symbol_x;

                  memcpy (&pixel_data[(y * width + x) * pixel_size],
                          pixel,
                          pixel_size);
                }
            }
        }

      if (g_cancellable_set_error_if_cancelled (cancellable, error))
        return NULL;
    }

  *out_pixel_format = pixel_format;

  return g_steal_pointer (&pixel_data);
}

static gboolean
generate_content_from_icon_on_main_thread (gpointer data)
{
  GTask *task = data;
  QrCodeGenerationData *qr_code_data = g_task_get_task_data (task);
  ShellGlobal *global = shell_global_get ();
  ClutterStage *stage = shell_global_get_stage (global);
  ClutterContext *clutter_context =
    clutter_actor_get_context (CLUTTER_ACTOR (stage));
  ClutterBackend *backend =
    clutter_context_get_backend (clutter_context);
  CoglContext *ctx = clutter_backend_get_cogl_context (backend);
  g_autoptr (ClutterContent) content = NULL;
  g_autoptr (GError) error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return G_SOURCE_REMOVE;

  content = st_image_content_new_with_preferred_size (qr_code_data->width,
                                                      qr_code_data->height);
  if (!st_image_content_set_data (ST_IMAGE_CONTENT (content),
                                  ctx,
                                  qr_code_data->icon,
                                  qr_code_data->format,
                                  qr_code_data->width,
                                  qr_code_data->height,
                                  qr_code_data->width *
                                  BYTES_PER_FORMAT (qr_code_data->format),
                                  &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return G_SOURCE_REMOVE;
    }

  g_task_return_pointer (task, g_steal_pointer (&content), g_object_unref);

  return G_SOURCE_REMOVE;
}

static void
qr_code_generator_thread (GTask        *task,
                          gpointer      source,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  QrCodeGenerationData *data = task_data;
  g_autoptr (GError) error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  data->format = COGL_PIXEL_FORMAT_RGB_888;
  data->icon = generate_icon (data->uri, data->width, data->height,
                             data->bg_color, data->fg_color,
                             cancellable, &data->format, &error);
  if (!data->icon)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_main_context_invoke (NULL,
                         generate_content_from_icon_on_main_thread,
                         task);
}

/**
 * shell_qr_code_generator_generate_qr_code:
 * @self: the #ShellQrCodeGenerator
 * @url: the URL of which generate the qr code
 * @width: The width of the qrcode
 * @height: The height of the qrcode
 * @bg_color: (nullable): The background color of the code
 *   or %NULL to use default (white)
 * @fg_color: (nullable): The foreground color of the code
 *   or %NULL to use default (black)
 * @cancellable: (nullable): A #GCancellable to cancel the operation
 * @callback: (scope async): function to call returning success or failure
 *   of the async grabbing
 * @user_data: the data to pass to callback function
 *
 * Generates the QrCode asynchronously.
 *
 * Use shell_qr_code_generator_generate_qr_code_finish() to complete it.
 */
void
shell_qr_code_generator_generate_qr_code (ShellQrCodeGenerator *self,
                                          const char           *url,
                                          size_t                width,
                                          size_t                height,
                                          const CoglColor      *bg_color,
                                          const CoglColor      *fg_color,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data)
{
  g_autoptr (GTask) task = NULL;
  QrCodeGenerationData *data;

  g_return_if_fail (SHELL_IS_QR_CODE_GENERATOR (self));

  if (!url || *url == '\0')
    {
      if (callback)
        g_task_report_new_error (self,
                                 callback,
                                 user_data,
                                 shell_qr_code_generator_generate_qr_code,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "No valid QR code URI provided");
      return;
    }

  data = g_new0 (QrCodeGenerationData, 1);
  data->uri = g_strdup (url);
  data->bg_color = cogl_color_copy (bg_color);
  data->fg_color = cogl_color_copy (fg_color);
  data->width = width;
  data->height = height;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, shell_qr_code_generator_generate_qr_code);
  g_task_set_task_data (task, data,
                        (GDestroyNotify) qr_code_generation_data_free);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, qr_code_generator_thread);
}

/**
 * shell_qr_code_generator_generate_qr_code_finish:
 * @self: the #ShellQrCodeGenerator
 * @result: the #GAsyncResult that was provided to the callback
 * @error: #GError for error reporting
 *
 * Finish the asynchronous operation started by
 * shell_qr_code_generator_generate_qr_code() and obtain its result.
 *
 * Returns: (transfer full): a #GIcon of the QR code
 *
 */
GIcon *
shell_qr_code_generator_generate_qr_code_finish (ShellQrCodeGenerator   *self,
                                                 GAsyncResult           *result,
                                                 GError                **error)
{
  g_return_val_if_fail (SHELL_IS_QR_CODE_GENERATOR (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result,
                                                  shell_qr_code_generator_generate_qr_code),
                        NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

ShellQrCodeGenerator *
shell_qr_code_generator_new (void)
{
  return g_object_new (SHELL_TYPE_QR_CODE_GENERATOR, NULL);
}
