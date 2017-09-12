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
 */

#include <config.h>

#include "wayland/meta-window-wayland.h"
#include "wayland/meta-wayland.h"
#include "core/window-private.h"
#include "compositor/compositor-private.h"
#include "meta-wayland-inhibit-shortcuts-dialog.h"

static GQuark quark_surface_inhibit_shortcuts_data = 0;

typedef struct _InhibitShortcutsData
{
  MetaWaylandSurface                *surface;
  MetaWaylandSeat                   *seat;
  MetaInhibitShortcutsDialog        *dialog;
  gulong                             response_handler_id;
  gboolean                           has_last_response;
  gboolean                           request_canceled;
  MetaInhibitShortcutsDialogResponse last_response;
} InhibitShortcutsData;

static InhibitShortcutsData *
surface_inhibit_shortcuts_data_get (MetaWaylandSurface *surface)
{
  return g_object_get_qdata (G_OBJECT (surface),
                             quark_surface_inhibit_shortcuts_data);
}

static void
surface_inhibit_shortcuts_data_set (MetaWaylandSurface   *surface,
                                    InhibitShortcutsData *data)
{
  g_object_set_qdata (G_OBJECT (surface),
                      quark_surface_inhibit_shortcuts_data,
                      data);
}

static void
surface_inhibit_shortcuts_data_destroy_dialog (InhibitShortcutsData *data)
{
  g_signal_handler_disconnect (data->dialog, data->response_handler_id);
  meta_inhibit_shortcuts_dialog_hide (data->dialog);
  g_clear_object (&data->dialog);
}

static void
surface_inhibit_shortcuts_data_free (InhibitShortcutsData *data)
{
  if (data->dialog)
    surface_inhibit_shortcuts_data_destroy_dialog (data);
  g_free (data);
}

static void
on_surface_destroyed (MetaWaylandSurface   *surface,
                      InhibitShortcutsData *data)
{
  surface_inhibit_shortcuts_data_free (data);
  g_object_set_qdata (G_OBJECT (surface),
                      quark_surface_inhibit_shortcuts_data,
                      NULL);
}

static void
inhibit_shortcuts_dialog_response_apply (InhibitShortcutsData *data)
{
  if (data->last_response == META_INHIBIT_SHORTCUTS_DIALOG_RESPONSE_ALLOW)
    meta_wayland_surface_inhibit_shortcuts (data->surface, data->seat);
  else if (meta_wayland_surface_is_shortcuts_inhibited (data->surface, data->seat))
    meta_wayland_surface_restore_shortcuts (data->surface, data->seat);
}

static void
inhibit_shortcuts_dialog_response_cb (MetaInhibitShortcutsDialog        *dialog,
                                      MetaInhibitShortcutsDialogResponse response,
                                      InhibitShortcutsData              *data)
{
  data->last_response = response;
  data->has_last_response = TRUE;

  /* If the request was canceled, we don't need to apply the choice made */
  if (!data->request_canceled)
    inhibit_shortcuts_dialog_response_apply (data);

  meta_inhibit_shortcuts_dialog_hide (data->dialog);
  surface_inhibit_shortcuts_data_destroy_dialog (data);
}

static InhibitShortcutsData *
meta_wayland_surface_ensure_inhibit_shortcuts_dialog (MetaWaylandSurface *surface,
                                                      MetaWaylandSeat    *seat)
{
  InhibitShortcutsData *data;
  MetaWindow *window;
  MetaDisplay *display;
  MetaInhibitShortcutsDialog *dialog;

  data = surface_inhibit_shortcuts_data_get (surface);
  if (data)
    return data;

  data = g_new0 (InhibitShortcutsData, 1);
  surface_inhibit_shortcuts_data_set (surface, data);
  g_signal_connect (surface, "destroy",
                    G_CALLBACK (on_surface_destroyed),
                    data);

  window = meta_wayland_surface_get_toplevel_window (surface);
  display = window->display;
  dialog =
    meta_compositor_create_inhibit_shortcuts_dialog (display->compositor,
                                                     window);

  data->surface = surface;
  data->seat = seat;
  data->dialog = dialog;
  data->response_handler_id =
    g_signal_connect (dialog, "response",
                      G_CALLBACK (inhibit_shortcuts_dialog_response_cb),
                      data);

  return data;
}

void
meta_wayland_surface_show_inhibit_shortcuts_dialog (MetaWaylandSurface *surface,
                                                    MetaWaylandSeat    *seat)
{
  InhibitShortcutsData *data;

  g_return_if_fail (META_IS_WAYLAND_SURFACE (surface));

  data = surface_inhibit_shortcuts_data_get (surface);
  if (data && data->has_last_response)
    {
      /* The dialog was shown before for this surface but is not showing
       * anymore, reuse the last user response.
       */
      inhibit_shortcuts_dialog_response_apply (data);
      return;
    }

  data = meta_wayland_surface_ensure_inhibit_shortcuts_dialog (surface, seat);
  /* This is a new request */
  data->request_canceled = FALSE;

  meta_inhibit_shortcuts_dialog_show (data->dialog);
}

void
meta_wayland_surface_cancel_inhibit_shortcuts_dialog (MetaWaylandSurface *surface)
{
  InhibitShortcutsData *data;

  g_return_if_fail (META_IS_WAYLAND_SURFACE (surface));

  /* The closure notify will take care of actually hiding the dialog */
  data = surface_inhibit_shortcuts_data_get (surface);
  g_return_if_fail (data);

  /* Keep the dialog on screen, but mark the request as canceled */
  data->request_canceled = TRUE;
}

void
meta_wayland_surface_inhibit_shortcuts_dialog_init (void)
{
  quark_surface_inhibit_shortcuts_data =
    g_quark_from_static_string ("-meta-wayland-surface-inhibit-shortcuts-data");
}
