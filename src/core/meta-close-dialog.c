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

#include "config.h"
#include "window-private.h"
#include "meta/meta-close-dialog.h"
#include "meta/meta-enum-types.h"

enum {
  RESPONSE,
  N_SIGNALS
};

guint dialog_signals[N_SIGNALS] = { 0 };

static GQuark quark_visible = 0;

G_DEFINE_INTERFACE (MetaCloseDialog, meta_close_dialog, G_TYPE_OBJECT)

static void
meta_close_dialog_default_init (MetaCloseDialogInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("window",
                                                            "Window",
                                                            "Window",
                                                            META_TYPE_WINDOW,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT_ONLY));
  dialog_signals[RESPONSE] =
    g_signal_new ("response",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, META_TYPE_CLOSE_DIALOG_RESPONSE);

  quark_visible = g_quark_from_static_string ("meta-close-dialog-visible");
}

/**
 * meta_close_dialog_show:
 * @dialog: a #MetaCloseDialog
 *
 * Shows the close dialog.
 **/
void
meta_close_dialog_show (MetaCloseDialog *dialog)
{
  MetaCloseDialogInterface *iface;

  g_return_if_fail (META_IS_CLOSE_DIALOG (dialog));

  iface = META_CLOSE_DIALOG_GET_IFACE (dialog);
  iface->show (dialog);
  g_object_set_qdata (G_OBJECT (dialog), quark_visible, GINT_TO_POINTER (TRUE));
}

/**
 * meta_close_dialog_hide:
 * @dialog: a #MetaCloseDialog
 *
 * Hides the close dialog.
 **/
void
meta_close_dialog_hide (MetaCloseDialog *dialog)
{
  MetaCloseDialogInterface *iface;

  g_return_if_fail (META_IS_CLOSE_DIALOG (dialog));

  iface = META_CLOSE_DIALOG_GET_IFACE (dialog);
  iface->hide (dialog);
  g_object_steal_qdata (G_OBJECT (dialog), quark_visible);
}

/**
 * meta_close_dialog_response:
 * @dialog: a #MetaCloseDialog
 * @response: a #MetaCloseDialogResponse
 *
 * Responds and closes the dialog. To be called by #MetaCloseDialog
 * implementations.
 **/
void
meta_close_dialog_response (MetaCloseDialog         *dialog,
                            MetaCloseDialogResponse  response)
{
  g_signal_emit (dialog, dialog_signals[RESPONSE], 0, response);
  meta_close_dialog_hide (dialog);
}

/**
 * meta_close_dialog_is_visible:
 * @dialog: a #MetaCloseDialog
 *
 * Returns whether @dialog is currently visible.
 *
 * Returns: #TRUE if @dialog is visible.
 **/
gboolean
meta_close_dialog_is_visible (MetaCloseDialog *dialog)
{
  return GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (dialog), quark_visible));
}

/**
 * meta_close_dialog_focus:
 * @dialog: a #MetaCloseDialog
 *
 * Call whenever @dialog should receive keyboard focus,
 * usually when the window would.
 **/
void
meta_close_dialog_focus (MetaCloseDialog *dialog)
{
  MetaCloseDialogInterface *iface;

  g_return_if_fail (META_IS_CLOSE_DIALOG (dialog));

  iface = META_CLOSE_DIALOG_GET_IFACE (dialog);
  if (iface->focus)
    iface->focus (dialog);
}
