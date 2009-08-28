/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_APP_MONITOR_H__
#define __SHELL_APP_MONITOR_H__

#include <glib-object.h>
#include <glib.h>

#include "window.h"
#include "shell-app-system.h"

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
  
};

GType shell_app_monitor_get_type (void) G_GNUC_CONST;

ShellAppMonitor* shell_app_monitor_get_default(void);

ShellAppInfo *shell_app_monitor_get_window_app (ShellAppMonitor *monitor, MetaWindow *metawin);

GList *shell_app_monitor_get_most_used_apps (ShellAppMonitor *monitor,
                                             const char      *context,
                                             gint             number);

GSList *shell_app_monitor_get_windows_for_app (ShellAppMonitor *monitor, const char *appid);

gboolean shell_app_monitor_is_window_usage_tracked (MetaWindow *window);

/* Get whatever's running right now */
GSList *shell_app_monitor_get_running_apps (ShellAppMonitor *monitor, const char *context);

GSList *shell_app_monitor_get_startup_sequences (ShellAppMonitor *monitor);

/* Hidden typedef for SnStartupSequence */
typedef struct _ShellStartupSequence ShellStartupSequence;
#define SHELL_TYPE_STARTUP_SEQUENCE (shell_startup_sequence_get_type ())
GType shell_startup_sequence_get_type (void);

const char *shell_startup_sequence_get_id (ShellStartupSequence *sequence);
const char *shell_startup_sequence_get_name (ShellStartupSequence *sequence);
gboolean shell_startup_sequence_get_completed (ShellStartupSequence *sequence);
ClutterActor *shell_startup_sequence_create_icon (ShellStartupSequence *sequence, guint size);

G_END_DECLS

#endif /* __SHELL_APP_MONITOR_H__ */
