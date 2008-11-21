/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "shell-app-monitor.h"

#include <gio/gio.h>

enum {
   PROP_0,

};

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _ShellAppMonitorPrivate {
  GList *desktop_dir_monitors;
};

static void shell_app_monitor_finalize (GObject *object);
static void on_monitor_changed (GFileMonitor *monitor, GFile *file,
				GFile *other_file, GFileMonitorEvent event_type,
				gpointer user_data);

G_DEFINE_TYPE(ShellAppMonitor, shell_app_monitor, G_TYPE_OBJECT);

static void shell_app_monitor_class_init(ShellAppMonitorClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = shell_app_monitor_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
		  SHELL_TYPE_APP_MONITOR,
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ShellAppMonitorClass, changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (ShellAppMonitorPrivate));
}

static void
shell_app_monitor_init (ShellAppMonitor *self)
{
  const gchar *const *iter;
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
					    SHELL_TYPE_APP_MONITOR,
					    ShellAppMonitorPrivate);
  for (iter = g_get_system_data_dirs (); *iter; iter++) 
    {
      GFile *dir;
      GFileMonitor *monitor;
      GError *error = NULL;

      dir = g_file_new_for_path (*iter);
      monitor = g_file_monitor_directory (dir, 0, NULL, &error);
      if (!monitor) {
        g_warning ("failed to monitor %s", error->message);
        g_clear_error (&error);
        continue;
      }
      g_signal_connect (monitor, "changed", G_CALLBACK (on_monitor_changed), self);
      self->priv->desktop_dir_monitors
        = g_list_prepend (self->priv->desktop_dir_monitors,
                          monitor);
      g_object_unref (dir);
    }
}

static void
shell_app_monitor_finalize (GObject *object)
{
  ShellAppMonitor *self = SHELL_APP_MONITOR (object);

  g_list_foreach (self->priv->desktop_dir_monitors, (GFunc) g_object_unref, NULL);
  g_list_free (self->priv->desktop_dir_monitors);

  G_OBJECT_CLASS (shell_app_monitor_parent_class)->finalize(object);
}

static void
on_monitor_changed (GFileMonitor *monitor, GFile *file,
		    GFile *other_file, GFileMonitorEvent event_type,
		    gpointer user_data)
{
  ShellAppMonitor *self = SHELL_APP_MONITOR (user_data);

  g_signal_emit (self, signals[CHANGED], 0);
}
