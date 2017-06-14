/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2004 Elijah Newren
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

#include <config.h>

#include "util-private.h"
#include "window-private.h"
#include "meta/meta-inhibit-shortcuts-dialog.h"
#include "meta-inhibit-shortcuts-dialog-default-private.h"

typedef struct _MetaInhibitShortcutsDialogDefaultPrivate MetaInhibitShortcutsDialogDefaultPrivate;

struct _MetaInhibitShortcutsDialogDefault
{
  GObject parent_instance;
  MetaWindow *window;
};

enum {
  PROP_0,
  PROP_WINDOW,
  N_PROPS
};

static void meta_inhibit_shortcuts_dialog_iface_init (MetaInhibitShortcutsDialogInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaInhibitShortcutsDialogDefault, meta_inhibit_shortcuts_dialog_default,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (META_TYPE_INHIBIT_SHORTCUTS_DIALOG,
                                                meta_inhibit_shortcuts_dialog_iface_init))

static void
meta_inhibit_shortcuts_dialog_default_show (MetaInhibitShortcutsDialog *dialog)
{
  /* Default to allow shortcuts inhibitor, but complain that no dialog is implemented */
  g_warning ("No MetaInhibitShortcutDialog implementation, falling back on allowing");
  meta_inhibit_shortcuts_dialog_response (dialog, META_INHIBIT_SHORTCUTS_DIALOG_RESPONSE_ALLOW);
}

static void
meta_inhibit_shortcuts_dialog_default_hide (MetaInhibitShortcutsDialog *dialog)
{
}

static void
meta_inhibit_shortcuts_dialog_iface_init (MetaInhibitShortcutsDialogInterface *iface)
{
  iface->show = meta_inhibit_shortcuts_dialog_default_show;
  iface->hide = meta_inhibit_shortcuts_dialog_default_hide;
}

static void
meta_inhibit_shortcuts_dialog_default_set_property (GObject      *object,
                                                    guint         prop_id,
                                                    const GValue *value,
                                                    GParamSpec   *pspec)
{
  MetaInhibitShortcutsDialogDefault *dialog;

  dialog = META_INHIBIT_SHORTCUTS_DIALOG_DEFAULT (object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      dialog->window = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_inhibit_shortcuts_dialog_default_get_property (GObject    *object,
                                                    guint       prop_id,
                                                    GValue     *value,
                                                    GParamSpec *pspec)
{
  MetaInhibitShortcutsDialogDefault *dialog;

  dialog = META_INHIBIT_SHORTCUTS_DIALOG_DEFAULT (object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      g_value_set_object (value, dialog->window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_inhibit_shortcuts_dialog_default_class_init (MetaInhibitShortcutsDialogDefaultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = meta_inhibit_shortcuts_dialog_default_set_property;
  object_class->get_property = meta_inhibit_shortcuts_dialog_default_get_property;

  g_object_class_override_property (object_class, PROP_WINDOW, "window");
}

static void
meta_inhibit_shortcuts_dialog_default_init (MetaInhibitShortcutsDialogDefault *dialog)
{
}

MetaInhibitShortcutsDialog *
meta_inhibit_shortcuts_dialog_default_new (MetaWindow *window)
{
  return g_object_new (META_TYPE_INHIBIT_SHORTCUTS_DIALOG_DEFAULT,
                       "window", window,
                       NULL);
}
