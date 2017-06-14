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

#ifndef META_INHIBIT_SHORTCUTS_DIALOG_H
#define META_INHIBIT_SHORTCUTS_DIALOG_H

#include <glib-object.h>
#include <meta/window.h>

#define META_TYPE_INHIBIT_SHORTCUTS_DIALOG (meta_inhibit_shortcuts_dialog_get_type ())
G_DECLARE_INTERFACE (MetaInhibitShortcutsDialog, meta_inhibit_shortcuts_dialog,
		     META, INHIBIT_SHORTCUTS_DIALOG, GObject)

typedef enum
{
  META_INHIBIT_SHORTCUTS_DIALOG_RESPONSE_ALLOW,
  META_INHIBIT_SHORTCUTS_DIALOG_RESPONSE_DENY,
} MetaInhibitShortcutsDialogResponse;

struct _MetaInhibitShortcutsDialogInterface
{
  GTypeInterface parent_iface;

  void (* show) (MetaInhibitShortcutsDialog *dialog);
  void (* hide) (MetaInhibitShortcutsDialog *dialog);
};

void meta_inhibit_shortcuts_dialog_show (MetaInhibitShortcutsDialog *dialog);
void meta_inhibit_shortcuts_dialog_hide (MetaInhibitShortcutsDialog *dialog);

void meta_inhibit_shortcuts_dialog_response (MetaInhibitShortcutsDialog        *dialog,
                                             MetaInhibitShortcutsDialogResponse response);

#endif /* META_INHIBIT_SHORTCUTS_DIALOG_H */
