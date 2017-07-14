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

#ifndef META_CLOSE_DIALOG_H
#define META_CLOSE_DIALOG_H

#include <glib-object.h>
#include <meta/window.h>

#define META_TYPE_CLOSE_DIALOG (meta_close_dialog_get_type ())

G_DECLARE_INTERFACE (MetaCloseDialog, meta_close_dialog,
		     META, CLOSE_DIALOG, GObject)

typedef enum
{
  META_CLOSE_DIALOG_RESPONSE_WAIT,
  META_CLOSE_DIALOG_RESPONSE_FORCE_CLOSE,
} MetaCloseDialogResponse;

struct _MetaCloseDialogInterface
{
  GTypeInterface parent_iface;

  void (* show) (MetaCloseDialog *dialog);
  void (* hide) (MetaCloseDialog *dialog);
  void (* focus) (MetaCloseDialog *dialog);
};

void              meta_close_dialog_show (MetaCloseDialog *dialog);
void              meta_close_dialog_hide (MetaCloseDialog *dialog);
void              meta_close_dialog_focus (MetaCloseDialog *dialog);
gboolean          meta_close_dialog_is_visible (MetaCloseDialog *dialog);

void              meta_close_dialog_response (MetaCloseDialog         *dialog,
                                              MetaCloseDialogResponse  response);

#endif /* META_CLOSE_DIALOG_H */
