/*
 * Copyright © 2020 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Florian Müllner <fmuellner@gnome.org>
 */

#include "shew-window-exporter.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

struct _ShewWindowExporter
{
  GObject parent;

  GtkWindow *window;
};

G_DEFINE_TYPE (ShewWindowExporter, shew_window_exporter, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_WINDOW,
};

ShewWindowExporter *
shew_window_exporter_new (GtkWindow *window)
{
  return g_object_new (SHEW_TYPE_WINDOW_EXPORTER,
                       "window", window,
                       NULL);
}

#ifdef GDK_WINDOWING_WAYLAND
static void
wayland_window_exported (GdkToplevel *toplevel,
                         const char  *handle,
                         gpointer     user_data)
{
  g_autoptr (GTask) task = user_data;

  g_task_return_pointer (task, g_strdup_printf ("wayland:%s", handle), g_free);
}
#endif

void
shew_window_exporter_export (ShewWindowExporter  *exporter,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  GtkWidget *widget;

  g_return_if_fail (SHEW_IS_WINDOW_EXPORTER (exporter));

  if (exporter->window == NULL)
    {
      g_task_report_new_error (exporter, callback, user_data,
                               shew_window_exporter_export,
                               G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               "No window to export");
      return;
    }

  task = g_task_new (exporter, NULL, callback, user_data);
  g_task_set_source_tag (task, shew_window_exporter_export);

  widget = GTK_WIDGET (exporter->window);

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (widget)))
    {
      GdkSurface *s = gtk_native_get_surface (GTK_NATIVE (widget));
      guint32 xid = (guint32) gdk_x11_surface_get_xid (s);

      g_task_return_pointer (task, g_strdup_printf ("x11:%x", xid), g_free);
    }
#endif

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (widget)))
    {
      GdkSurface *s = gtk_native_get_surface (GTK_NATIVE (widget));
      gdk_wayland_toplevel_export_handle (GDK_WAYLAND_TOPLEVEL (s),
                                          wayland_window_exported,
                                          g_steal_pointer (&task), NULL);
    }
#endif

  if (task != NULL && !g_task_get_completed (task))
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               "Unsupported windowing system");
    }
}

char *
shew_window_exporter_export_finish (ShewWindowExporter  *exporter,
                                    GAsyncResult        *result,
                                    GError             **error)
{
  g_return_val_if_fail (SHEW_IS_WINDOW_EXPORTER (exporter), NULL);
  g_return_val_if_fail (g_async_result_is_tagged (result, shew_window_exporter_export), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
shew_window_exporter_unexport (ShewWindowExporter *exporter)
{
  GtkWidget *widget;

  g_return_if_fail (SHEW_IS_WINDOW_EXPORTER (exporter));

  widget = GTK_WIDGET (exporter->window);

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (widget)))
    {
      GdkSurface *s = gtk_native_get_surface (GTK_NATIVE (widget));
      gdk_wayland_toplevel_unexport_handle (GDK_WAYLAND_TOPLEVEL (s));
    }
#endif
}

static void
shew_window_exporter_dispose (GObject *object)
{
  ShewWindowExporter *exporter = SHEW_WINDOW_EXPORTER (object);

  g_clear_object (&exporter->window);

  G_OBJECT_CLASS (shew_window_exporter_parent_class)->dispose (object);
}

static void
shew_window_exporter_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ShewWindowExporter *exporter = SHEW_WINDOW_EXPORTER (object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      g_set_object (&exporter->window, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
shew_window_exporter_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ShewWindowExporter *exporter = SHEW_WINDOW_EXPORTER (object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      g_value_set_object (value, exporter->window);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
shew_window_exporter_init (ShewWindowExporter *exporter)
{
}

static void
shew_window_exporter_class_init (ShewWindowExporterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = shew_window_exporter_get_property;
  object_class->set_property = shew_window_exporter_set_property;
  object_class->dispose = shew_window_exporter_dispose;

  g_object_class_install_property (object_class,
                                   PROP_WINDOW,
                                   g_param_spec_object ("window",
                                                        "GtkWindow",
                                                        "The GtkWindow to export",
                                                        GTK_TYPE_WINDOW,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}
