/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_WINDOW_TRACKER_H__
#define __SHELL_WINDOW_TRACKER_H__

#include <glib-object.h>
#include <glib.h>
#include <meta/window.h>

#include "shell-app.h"
#include "shell-app-system.h"

G_BEGIN_DECLS

#define SHELL_TYPE_WINDOW_TRACKER (shell_window_tracker_get_type ())
G_DECLARE_FINAL_TYPE (ShellWindowTracker, shell_window_tracker,
                      SHELL, WINDOW_TRACKER, GObject)

ShellWindowTracker* shell_window_tracker_get_default(void);

ShellApp *shell_window_tracker_get_window_app (ShellWindowTracker *tracker, MetaWindow *metawin);

ShellApp *shell_window_tracker_get_app_from_pid (ShellWindowTracker *tracker, int pid);

GSList *shell_window_tracker_get_startup_sequences (ShellWindowTracker *tracker);

/* Hidden typedef for SnStartupSequence */
typedef struct _ShellStartupSequence ShellStartupSequence;
#define SHELL_TYPE_STARTUP_SEQUENCE (shell_startup_sequence_get_type ())
GType shell_startup_sequence_get_type (void);

const char *shell_startup_sequence_get_id (ShellStartupSequence *sequence);
ShellApp *shell_startup_sequence_get_app (ShellStartupSequence *sequence);
const char *shell_startup_sequence_get_name (ShellStartupSequence *sequence);
gboolean shell_startup_sequence_get_completed (ShellStartupSequence *sequence);
int shell_startup_sequence_get_workspace (ShellStartupSequence *sequence);
ClutterActor *shell_startup_sequence_create_icon (ShellStartupSequence *sequence, guint size);

G_END_DECLS

#endif /* __SHELL_WINDOW_TRACKER_H__ */
