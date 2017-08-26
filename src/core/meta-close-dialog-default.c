/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2004 Elijah Newren
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

#define _XOPEN_SOURCE /* for kill() */

#include <config.h>
#include "util-private.h"
#include "window-private.h"
#include <meta/meta-close-dialog.h>
#include "meta-close-dialog-default-private.h"
#include "x11/meta-x11-display-private.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

typedef struct _MetaCloseDialogDefaultPrivate MetaCloseDialogDefaultPrivate;

struct _MetaCloseDialogDefault
{
  GObject parent_instance;
  MetaWindow *window;
  int dialog_pid;
  guint child_watch_id;
};

enum {
  PROP_0,
  PROP_WINDOW,
  N_PROPS
};

GParamSpec *pspecs[N_PROPS] = { NULL };

static void meta_close_dialog_iface_init (MetaCloseDialogInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaCloseDialogDefault, meta_close_dialog_default,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (META_TYPE_CLOSE_DIALOG,
                                                meta_close_dialog_iface_init))

static void
dialog_exited (GPid     pid,
               int      status,
               gpointer user_data)
{
  MetaCloseDialogDefault *dialog = user_data;

  dialog->dialog_pid = -1;

  /* exit status of 0 means the user pressed "Force Quit" */
  if (WIFEXITED (status) && WEXITSTATUS (status) == 0)
    g_signal_emit_by_name (dialog, "response", META_CLOSE_DIALOG_RESPONSE_FORCE_CLOSE);
}

static void
present_existing_delete_dialog (MetaCloseDialogDefault *dialog)
{
  MetaWindow *window;
  GSList *windows;
  GSList *tmp;

  window = dialog->window;

  if (dialog->dialog_pid < 0)
    return;

  meta_topic (META_DEBUG_PING,
              "Presenting existing ping dialog for %s\n",
              window->desc);

  /* Activate transient for window that belongs to
   * mutter-dialog
   */
  windows = meta_display_list_windows (window->display, META_LIST_DEFAULT);
  tmp = windows;

  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;

      if (w->transient_for == window && w->res_class &&
          g_ascii_strcasecmp (w->res_class, "mutter-dialog") == 0)
        {
          meta_window_activate (w, CLUTTER_CURRENT_TIME);
          break;
        }

      tmp = tmp->next;
    }

  g_slist_free (windows);
}

static void
meta_close_dialog_default_show (MetaCloseDialog *dialog)
{
  MetaCloseDialogDefault *dialog_default = META_CLOSE_DIALOG_DEFAULT (dialog);
  MetaWindow *window = dialog_default->window;
  gchar *window_title, *window_content, *tmp;
  GPid dialog_pid;

  if (dialog_default->dialog_pid >= 0)
    {
      present_existing_delete_dialog (dialog_default);
      return;
    }

  /* This is to get a better string if the title isn't representable
   * in the locale encoding; actual conversion to UTF-8 is done inside
   * meta_show_dialog */
  if (window->title && window->title[0])
    {
      tmp = g_locale_from_utf8 (window->title, -1, NULL, NULL, NULL);
      if (tmp == NULL)
        window_title = NULL;
      else
        window_title = window->title;
      g_free (tmp);
    }
  else
    {
      window_title = NULL;
    }

  if (window_title)
    /* Translators: %s is a window title */
    tmp = g_strdup_printf (_("“%s” is not responding."), window_title);
  else
    tmp = g_strdup (_("Application is not responding."));

  window_content = g_strdup_printf (
      "<big><b>%s</b></big>\n\n%s",
      tmp,
      _("You may choose to wait a short while for it to "
        "continue or force the application to quit entirely."));

  dialog_pid =
    meta_show_dialog ("--question",
                      window_content, NULL,
                      window->display->x11_display->screen_name,
                      _("_Force Quit"), _("_Wait"),
                      "face-sad-symbolic", window->xwindow,
                      NULL, NULL);

  g_free (window_content);
  g_free (tmp);

  dialog_default->dialog_pid = dialog_pid;
  g_child_watch_add (dialog_pid, dialog_exited, dialog);
}

static void
meta_close_dialog_default_hide (MetaCloseDialog *dialog)
{
  MetaCloseDialogDefault *dialog_default;

  dialog_default = META_CLOSE_DIALOG_DEFAULT (dialog);

  if (dialog_default->child_watch_id)
    {
      g_source_remove (dialog_default->child_watch_id);
      dialog_default->child_watch_id = 0;
    }

  if (dialog_default->dialog_pid > -1)
    {
      kill (dialog_default->dialog_pid, SIGTERM);
      dialog_default->dialog_pid = -1;
    }
}

static void
meta_close_dialog_iface_init (MetaCloseDialogInterface *iface)
{
  iface->show = meta_close_dialog_default_show;
  iface->hide = meta_close_dialog_default_hide;
}

static void
meta_close_dialog_default_finalize (GObject *object)
{
  MetaCloseDialogDefault *dialog;

  dialog = META_CLOSE_DIALOG_DEFAULT (object);

  if (dialog->child_watch_id)
    g_source_remove (dialog->child_watch_id);

  if (dialog->dialog_pid > -1)
    {
      kill (dialog->dialog_pid, SIGKILL);
      dialog->dialog_pid = -1;
    }

  G_OBJECT_CLASS (meta_close_dialog_default_parent_class)->finalize (object);
}

static void
meta_close_dialog_default_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  MetaCloseDialogDefault *dialog;

  dialog = META_CLOSE_DIALOG_DEFAULT (object);

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
meta_close_dialog_default_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  MetaCloseDialogDefault *dialog;

  dialog = META_CLOSE_DIALOG_DEFAULT (object);

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
meta_close_dialog_default_class_init (MetaCloseDialogDefaultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_close_dialog_default_finalize;
  object_class->set_property = meta_close_dialog_default_set_property;
  object_class->get_property = meta_close_dialog_default_get_property;

  g_object_class_override_property (object_class, PROP_WINDOW, "window");
}

static void
meta_close_dialog_default_init (MetaCloseDialogDefault *dialog)
{
  dialog->dialog_pid = -1;
}

MetaCloseDialog *
meta_close_dialog_default_new (MetaWindow *window)
{
  return g_object_new (META_TYPE_CLOSE_DIALOG_DEFAULT,
                       "window", window,
                       NULL);
}
