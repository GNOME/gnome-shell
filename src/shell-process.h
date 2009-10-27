/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_PROCESS_H__
#define __SHELL_PROCESS_H__

#include <glib-object.h>

#define SHELL_TYPE_PROCESS                 (shell_process_get_type ())
#define SHELL_PROCESS(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_PROCESS, ShellProcess))
#define SHELL_PROCESS_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_PROCESS, ShellProcessClass))
#define SHELL_IS_PROCESS(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_PROCESS))
#define SHELL_IS_PROCESS_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_PROCESS))
#define SHELL_PROCESS_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_PROCESS, ShellProcessClass))

typedef struct _ShellProcess ShellProcess;
typedef struct _ShellProcessClass ShellProcessClass;

typedef struct _ShellProcessPrivate ShellProcessPrivate;

struct _ShellProcess
{
  GObject parent;

  ShellProcessPrivate *priv;
};

struct _ShellProcessClass
{
  GObjectClass parent_class;

};

GType shell_process_get_type (void) G_GNUC_CONST;
ShellProcess* shell_process_new(char **args);

gboolean shell_process_run (ShellProcess *process, GError **error);

#endif /* __SHELL_PROCESS_H__ */
