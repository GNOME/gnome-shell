/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef META_CLOSE_DIALOG_DEFAULT_H
#define META_CLOSE_DIALOG_DEFAULT_H

#define META_TYPE_CLOSE_DIALOG_DEFAULT (meta_close_dialog_default_get_type ())
G_DECLARE_FINAL_TYPE (MetaCloseDialogDefault,
                      meta_close_dialog_default,
                      META, CLOSE_DIALOG_DEFAULT,
                      GObject)

MetaCloseDialog * meta_close_dialog_default_new (MetaWindow *window);

#endif /* META_CLOSE_DIALOG_DEFAULT_H */
