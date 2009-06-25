/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_APP_MONITOR_H__
#define __SHELL_APP_MONITOR_H__

#include <glib-object.h>
#include <glib.h>

/* 
 * This object provides monitoring of system application directories (.desktop files)
 * and activity-based statistics about applications usage 
 */

G_BEGIN_DECLS

typedef struct _ShellAppMonitor ShellAppMonitor;
typedef struct _ShellAppMonitorClass ShellAppMonitorClass;
typedef struct _ShellAppMonitorPrivate ShellAppMonitorPrivate;

#define SHELL_TYPE_APP_MONITOR              (shell_app_monitor_get_type ())
#define SHELL_APP_MONITOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), SHELL_TYPE_APP_MONITOR, ShellAppMonitor))
#define SHELL_APP_MONITOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_APP_MONITOR, ShellAppMonitorClass))
#define SHELL_IS_APP_MONITOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), SHELL_TYPE_APP_MONITOR))
#define SHELL_IS_APP_MONITOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_APP_MONITOR))
#define SHELL_APP_MONITOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_APP_MONITOR, ShellAppMonitorClass))

struct _ShellAppMonitorClass
{
  GObjectClass parent_class;
  
  void (*apps_changed)(ShellAppMonitor *menuwrapper,
                       gpointer         data);
};

GType shell_app_monitor_get_type (void) G_GNUC_CONST;

ShellAppMonitor* shell_app_monitor_get_default(void);

/* Get the most popular applications for a given activity */
GSList *shell_app_monitor_get_most_used_apps (ShellAppMonitor *monitor,
                                              int              activity,
                                              gint             number);

guint shell_app_monitor_get_window_count (ShellAppMonitor *monitor, const char *appid);

/* Get whatever's running right now */
GList *shell_app_monitor_get_running_app_ids (ShellAppMonitor *monitor);

G_END_DECLS

#endif /* __SHELL_APP_MONITOR_H__ */
