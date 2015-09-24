/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* shell-keyring-prompt.c - prompt handler for gnome-keyring-daemon

   Copyright (C) 2011 Stefan Walter

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Author: Stef Walter <stef@thewalter.net>
*/

#ifndef __SHELL_KEYRING_PROMPT_H__
#define __SHELL_KEYRING_PROMPT_H__

#include <glib-object.h>
#include <glib.h>

#include <clutter/clutter.h>

G_BEGIN_DECLS

typedef struct _ShellKeyringPrompt         ShellKeyringPrompt;

#define SHELL_TYPE_KEYRING_PROMPT (shell_keyring_prompt_get_type ())
G_DECLARE_FINAL_TYPE (ShellKeyringPrompt, shell_keyring_prompt,
                      SHELL, KEYRING_PROMPT, GObject)

ShellKeyringPrompt * shell_keyring_prompt_new                  (void);

ClutterText *        shell_keyring_prompt_get_password_actor   (ShellKeyringPrompt *self);

void                 shell_keyring_prompt_set_password_actor   (ShellKeyringPrompt *self,
                                                                ClutterText *password_actor);

ClutterText *        shell_keyring_prompt_get_confirm_actor    (ShellKeyringPrompt *self);

void                 shell_keyring_prompt_set_confirm_actor    (ShellKeyringPrompt *self,
                                                                ClutterText *confirm_actor);

gboolean             shell_keyring_prompt_complete             (ShellKeyringPrompt *self);

void                 shell_keyring_prompt_cancel               (ShellKeyringPrompt *self);

G_END_DECLS

#endif /* __SHELL_KEYRING_PROMPT_H__ */
