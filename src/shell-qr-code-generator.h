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

#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <cogl/cogl.h>

/**
 * SECTION:shell-qr-code-generator
 * @short_description: Generates a QR code for a given url.
 *
 * The #ShellQrCodeGenerator object is used to generate QR codes for URLs
 *
 */
#define SHELL_TYPE_QR_CODE_GENERATOR (shell_qr_code_generator_get_type ())
G_DECLARE_FINAL_TYPE (ShellQrCodeGenerator, shell_qr_code_generator,
                      SHELL, QR_CODE_GENERATOR, GObject)

ShellQrCodeGenerator *shell_qr_code_generator_new (void);

void    shell_qr_code_generator_generate_qr_code         (ShellQrCodeGenerator   *self,
                                                          const char             *url,
                                                          size_t                  width,
                                                          size_t                  height,
                                                          const CoglColor        *bg_color,
                                                          const CoglColor        *fg_color,
                                                          GCancellable           *cancellable,
                                                          GAsyncReadyCallback     callback,
                                                          gpointer                user_data);

GIcon * shell_qr_code_generator_generate_qr_code_finish  (ShellQrCodeGenerator   *self,
                                                          GAsyncResult           *result,
                                                          GError                **error);

