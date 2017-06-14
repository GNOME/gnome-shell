/*
 * Copyright (C) 2017 Red Hat
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
 */

#include "config.h"

#include "core/window-private.h"
#include "meta/meta-inhibit-shortcuts-dialog.h"
#include "meta/meta-enum-types.h"

enum
{
  RESPONSE,
  LAST_SIGNAL
};

static guint inhibit_dialog_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_INTERFACE (MetaInhibitShortcutsDialog, meta_inhibit_shortcuts_dialog, G_TYPE_OBJECT)

static void
meta_inhibit_shortcuts_dialog_default_init (MetaInhibitShortcutsDialogInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("window",
                                                            "Window",
                                                            "Window",
                                                            META_TYPE_WINDOW,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT_ONLY |
                                                            G_PARAM_STATIC_STRINGS));
  inhibit_dialog_signals[RESPONSE] =
    g_signal_new ("response",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, META_TYPE_INHIBIT_SHORTCUTS_DIALOG_RESPONSE);
}

/**
 * meta_inhibit_shortcuts_dialog_show:
 * @dialog: a #MetaInhibitShortcutsDialog
 *
 * Shows the inhibit shortcuts dialog.
 **/
void
meta_inhibit_shortcuts_dialog_show (MetaInhibitShortcutsDialog *dialog)
{
  MetaInhibitShortcutsDialogInterface *iface;

  g_return_if_fail (META_IS_INHIBIT_SHORTCUTS_DIALOG (dialog));

  iface = META_INHIBIT_SHORTCUTS_DIALOG_GET_IFACE (dialog);
  iface->show (dialog);
}

/**
 * meta_inhibit_shortcuts_dialog_hide:
 * @dialog: a #MetaInhibitShortcutsDialog
 *
 * Hides the inhibit shortcuts dialog.
 **/
void
meta_inhibit_shortcuts_dialog_hide (MetaInhibitShortcutsDialog *dialog)
{
  MetaInhibitShortcutsDialogInterface *iface;

  g_return_if_fail (META_IS_INHIBIT_SHORTCUTS_DIALOG (dialog));

  iface = META_INHIBIT_SHORTCUTS_DIALOG_GET_IFACE (dialog);
  iface->hide (dialog);
}

/**
 * meta_inhibit_shortcuts_dialog_response:
 * @dialog: a #MetaInhibitShortcutsDialog
 * @response: a #MetaInhibitShortcutsDialogResponse
 *
 * Responds and closes the dialog. To be called by #MetaInhibitShortcutsDialog
 * implementations.
 **/
void
meta_inhibit_shortcuts_dialog_response (MetaInhibitShortcutsDialog         *dialog,
                                        MetaInhibitShortcutsDialogResponse  response)
{
  g_signal_emit (dialog, inhibit_dialog_signals[RESPONSE], 0, response);
  meta_inhibit_shortcuts_dialog_hide (dialog);
}
