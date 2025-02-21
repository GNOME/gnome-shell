/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2024 Red Hat, Inc
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
 */

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <meta/display.h>
#include <meta/util.h>
#include <meta/meta-plugin.h>
#include <st/st.h>

#include <qrencode.h>

#include "shell-global.h"
#include "shell-qr-code-generator.h"

#define BYTES_PER_RGB_888 3

typedef struct _ShellQrCodeGeneratorPrivate  ShellQrCodeGeneratorPrivate;

struct _ShellQrCodeGenerator
{
  GObject parent_instance;

  ShellQrCodeGeneratorPrivate *priv;
};

struct _ShellQrCodeGeneratorPrivate
{
  char *url;
  size_t width;
  size_t height;
  GTask *image_task;
  GTask *icon_task;
};

G_DEFINE_TYPE_WITH_PRIVATE (ShellQrCodeGenerator, shell_qr_code_generator, G_TYPE_OBJECT);

static void
shell_qr_code_generator_dispose (GObject *object)
{
  ShellQrCodeGenerator *self = SHELL_QR_CODE_GENERATOR (object);
  g_clear_pointer (&self->priv->url, g_free);
}

static void
shell_qr_code_generator_class_init (ShellQrCodeGeneratorClass *qr_code_generator_class)
{
  GObjectClass *gobject_class = (GObjectClass *) qr_code_generator_class;
  gobject_class->dispose = shell_qr_code_generator_dispose;
}

static void
shell_qr_code_generator_init (ShellQrCodeGenerator *qr_code_generator)
{
  qr_code_generator->priv = shell_qr_code_generator_get_instance_private (qr_code_generator);
}

static guint8 *
generate_icon (const char   *url,
               size_t        width,
               size_t        height,
               GError      **error)
{
  QRcode *qrcode;
  g_autofree guint8 *pixel_data = NULL;
  guint8 white_pixel[BYTES_PER_RGB_888] = {255, 255, 255};
  guint8 black_pixel[BYTES_PER_RGB_888] = {0, 0, 0};
  size_t pixel_size = sizeof (white_pixel);
  size_t symbol_size;
  size_t symbols_per_row, number_of_rows;
  size_t code_width;
  size_t code_height;
  size_t offset_x;
  size_t offset_y;
  size_t row, symbol, symbol_x, symbol_y;

  qrcode = QRcode_encodeString (url, 1, QR_ECLEVEL_L, QR_MODE_8, 1);

  if (!qrcode)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "QRCode generation failed for url %s",
                   url);
      return NULL;
    }

  symbols_per_row = qrcode->width;
  number_of_rows = qrcode->width;

  symbol_size = MIN (width, height) / symbols_per_row;
  code_width = symbol_size * symbols_per_row;
  code_height = symbol_size * number_of_rows;
  offset_x = (width - code_width) / 2;
  offset_y = (height - code_height) / 2;

  pixel_data = calloc (height, width * BYTES_PER_RGB_888);

  for (row = 0; row < number_of_rows; row++)
    {
      for (symbol = 0; symbol < symbols_per_row; symbol++)
        {
          guint8 *pixel;

          if (qrcode->data[row * symbols_per_row + symbol] & 1)
            pixel = black_pixel;
          else
            pixel = white_pixel;

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
    }

  return g_steal_pointer (&pixel_data);
}

static void
qr_code_generator_thread (GTask        *task,
                          gpointer      source,
                          gpointer      task_data,
                          GCancellable *cancellable)
{
  ShellQrCodeGenerator *self = task_data;
  g_autoptr (GError) error = NULL;
  g_autofree guint8 *pixel_data = NULL;

  pixel_data = generate_icon (self->priv->url, self->priv->width, self->priv->height, &error);

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&pixel_data), NULL);
}

static void
on_image_task_complete (ShellQrCodeGenerator *self,
                        GAsyncResult         *result,
                        gpointer              user_data)
{
  ShellGlobal *global = shell_global_get ();
  ClutterStage *stage = shell_global_get_stage (global);
  ClutterContext *clutter_context =
    clutter_actor_get_context (CLUTTER_ACTOR (stage));
  ClutterBackend *backend =
    clutter_context_get_backend (clutter_context);
  CoglContext *ctx = clutter_backend_get_cogl_context (backend);
  g_autofree guint8 *pixel_data = NULL;
  g_autoptr (ClutterContent) content = NULL;
  g_autoptr (GError) error = NULL;

  pixel_data = g_task_propagate_pointer (G_TASK (result), &error);
  if (error != NULL)
    {
      g_task_return_error (self->priv->icon_task, g_steal_pointer (&error));
      return;
    }

  content = st_image_content_new_with_preferred_size (self->priv->width,
                                                      self->priv->height);
  if (!st_image_content_set_data (ST_IMAGE_CONTENT (content),
                                  ctx,
                                  pixel_data,
                                  COGL_PIXEL_FORMAT_RGB_888,
                                  self->priv->width,
                                  self->priv->height,
                                  self->priv->width * BYTES_PER_RGB_888,
                                  &error))
    {
      g_task_return_error (self->priv->icon_task, g_steal_pointer (&error));
      return;
    }

  g_task_return_pointer (self->priv->icon_task, g_steal_pointer (&content), g_object_unref);
  g_clear_object (&self->priv->image_task);
}

/**
 * shell_qr_code_generator_generate_qr_code:
 * @self: the #ShellQrCodeGenerator
 * @url: the URL of which generate the qr code
 * @width: The width of the qrcode
 * @height: The height of the qrcode
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
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data)
{
  ShellQrCodeGeneratorPrivate *priv;

  g_return_if_fail (SHELL_IS_QR_CODE_GENERATOR (self));

  priv = self->priv;

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

  if (priv->url != NULL)
    {
      if (callback)
        g_task_report_new_error (self,
                                 callback,
                                 user_data,
                                 shell_qr_code_generator_generate_qr_code,
                                 G_IO_ERROR,
                                 G_IO_ERROR_PENDING,
                                 "Only one QR code generator operation at a time "
                                 "is permitted");
      return;
    }

  priv->url = g_strdup (url);
  priv->width = width;
  priv->height = height;

  priv->icon_task = g_task_new (self, NULL, callback, user_data);
  g_task_set_source_tag (priv->icon_task, shell_qr_code_generator_generate_qr_code);
  g_task_set_task_data (priv->icon_task, self, NULL);

  priv->image_task = g_task_new (self, NULL, (GAsyncReadyCallback) on_image_task_complete, NULL);
  g_task_set_source_tag (priv->image_task, on_image_task_complete);
  g_task_set_task_data (priv->image_task, self, NULL);
  g_task_run_in_thread (priv->image_task, qr_code_generator_thread);
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

  g_clear_pointer (&self->priv->url, g_free);

  return g_task_propagate_pointer (G_TASK (result), error);
}

ShellQrCodeGenerator *
shell_qr_code_generator_new (void)
{
  return g_object_new (SHELL_TYPE_QR_CODE_GENERATOR, NULL);
}
