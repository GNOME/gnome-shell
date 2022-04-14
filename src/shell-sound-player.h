/*
 * Copyright (C) 2018 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */
#ifndef SHELL_SOUND_PLAYER_H
#define SHELL_SOUND_PLAYER_H

#include <gio/gio.h>

#define SHELL_TYPE_SOUND_PLAYER (shell_sound_player_get_type ())

G_DECLARE_FINAL_TYPE (ShellSoundPlayer, shell_sound_player,
                      SHELL, SOUND_PLAYER, GObject)


void shell_sound_player_play_from_theme (ShellSoundPlayer *player,
                                         const char       *name,
                                         const char       *description,
                                         GCancellable     *cancellable);

#endif /* SHELL_SOUND_PLAYER_H */
