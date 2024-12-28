/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* shell-secure-text-buffer.h - secure memory clutter text buffer

   Copyright (C) 2009 Stefan Walter
   Copyright (C) 2012 Red Hat Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stefw@gnome.org>
*/

#pragma once

#include <clutter/clutter.h>
#include <clutter/clutter-pango.h>

G_BEGIN_DECLS

#define SHELL_TYPE_SECURE_TEXT_BUFFER (shell_secure_text_buffer_get_type ())
G_DECLARE_FINAL_TYPE (ShellSecureTextBuffer, shell_secure_text_buffer,
                      SHELL, SECURE_TEXT_BUFFER, ClutterTextBuffer)

ClutterTextBuffer *       shell_secure_text_buffer_new                    (void);

G_END_DECLS
