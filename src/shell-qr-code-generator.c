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
#include <gnome-qr/gnome-qr.h>
#include <st/st.h>

#include "shell-global.h"

struct _ShellQrCodeGenerator
{
  GObject parent_instance;
};

G_DEFINE_TYPE (ShellQrCodeGenerator, shell_qr_code_generator, G_TYPE_OBJECT);

static void
shell_qr_code_generator_class_init (ShellQrCodeGeneratorClass *qr_code_generator_class)
{
}

static void
shell_qr_code_generator_init (ShellQrCodeGenerator *qr_code_generator)
{
}

static GnomeQrColor *
get_gnome_qr_color (const CoglColor *color)
{
  GnomeQrColor *gnome_color;

  if (!color)
    return NULL;

  gnome_color = g_new (GnomeQrColor, 1);
  *gnome_color = (GnomeQrColor) {
    color->red,
    color->green,
    color->blue,
    color->alpha,
  };

  return gnome_color;
}

static CoglPixelFormat
get_cogl_pixel_format (GnomeQrPixelFormat format)
{
  switch (format)
    {
    case GNOME_QR_PIXEL_FORMAT_RGB_888:
      return COGL_PIXEL_FORMAT_RGB_888;
    case GNOME_QR_PIXEL_FORMAT_RGBA_8888:
      return COGL_PIXEL_FORMAT_RGBA_8888;
    }
  g_assert_not_reached ();
}

static void
on_qr_code_generated (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      data)
{
  GTask *task = data;
  g_autoptr (ClutterContent) content = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GBytes) icon = NULL;
  GnomeQrPixelFormat format;
  size_t size;
  ShellGlobal *global;
  ClutterStage *stage;
  ClutterContext *clutter_context;
  ClutterBackend *backend;
  CoglContext *ctx;

  icon = gnome_qr_generate_qr_code_finish (res,
                                           &size,
                                           &format,
                                           &error);
  if (!icon)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  global = shell_global_get ();
  stage = shell_global_get_stage (global);
  clutter_context = clutter_actor_get_context (CLUTTER_ACTOR (stage));
  backend = clutter_context_get_backend (clutter_context);
  ctx = clutter_backend_get_cogl_context (backend);

  content = st_image_content_new_with_preferred_size (size, size);
  if (!st_image_content_set_data (ST_IMAGE_CONTENT (content),
                                  ctx,
                                  g_bytes_get_data (icon, NULL),
                                  get_cogl_pixel_format (format),
                                  size,
                                  size,
                                  size * GNOME_QR_BYTES_PER_FORMAT (format),
                                  &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_pointer (task, g_steal_pointer (&content), g_object_unref);
}

/**
 * shell_qr_code_generator_generate_qr_code:
 * @self: the #ShellQrCodeGenerator
 * @url: the URL for which to generate the QR code
 * @size: The size of the QR code (width and height)
 * @bg_color: (nullable): The background color of the code
 *   or %NULL to use default (white)
 * @fg_color: (nullable): The foreground color of the code
 *   or %NULL to use default (black)
 * @cancellable: (nullable): A #GCancellable to cancel the operation
 * @callback: (scope async): function to call with the result
 * @user_data: the data to pass to callback function
 *
 * Generates the QR code asynchronously.
 *
 * Use shell_qr_code_generator_generate_qr_code_finish() to complete it.
 */
void
shell_qr_code_generator_generate_qr_code (ShellQrCodeGenerator *self,
                                          const char           *url,
                                          size_t                size,
                                          const CoglColor      *bg_color,
                                          const CoglColor      *fg_color,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autofree GnomeQrColor *bg = NULL;
  g_autofree GnomeQrColor *fg = NULL;

  g_return_if_fail (SHELL_IS_QR_CODE_GENERATOR (self));

  bg = get_gnome_qr_color (bg_color);
  fg = get_gnome_qr_color (fg_color);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, shell_qr_code_generator_generate_qr_code);
  g_task_set_return_on_cancel (task, TRUE);

  gnome_qr_generate_qr_code_async (url,
                                   size,
                                   bg,
                                   fg,
                                   cancellable,
                                   on_qr_code_generated,
                                   g_steal_pointer (&task));
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
