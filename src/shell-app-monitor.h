#ifndef __SHELL_APP_MONITOR_H__
#define __SHELL_APP_MONITOR_H__

#include <glib-object.h>

#define SHELL_TYPE_APP_MONITOR                 (shell_app_monitor_get_type ())
#define SHELL_APP_MONITOR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_APP_MONITOR, ShellAppMonitor))
#define SHELL_APP_MONITOR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_APP_MONITOR, ShellAppMonitorClass))
#define SHELL_IS_APP_MONITOR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_APP_MONITOR))
#define SHELL_IS_APP_MONITOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_APP_MONITOR))
#define SHELL_APP_MONITOR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_APP_MONITOR, ShellAppMonitorClass))

typedef struct _ShellAppMonitor ShellAppMonitor;
typedef struct _ShellAppMonitorClass ShellAppMonitorClass;
typedef struct _ShellAppMonitorPrivate ShellAppMonitorPrivate;

struct _ShellAppMonitor
{
  GObject parent;

  ShellAppMonitorPrivate *priv;
};

struct _ShellAppMonitorClass
{
  GObjectClass parent_class;

  void (*changed)(ShellAppMonitor *menuwrapper, gpointer data);
};

GType shell_app_monitor_get_type (void) G_GNUC_CONST;
ShellAppMonitor* shell_app_monitor_new(void);

#endif /* __SHELL_APP_MONITOR_H__ */
