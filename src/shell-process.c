#include "shell-process.h"

#include <sys/types.h>
#include <sys/wait.h>

struct _ShellProcessPrivate {
  char **args;
  GPid pid;
};

enum {
  PROP_0,
  PROP_ARGS,
};

static void shell_process_dispose (GObject *object);
static void shell_process_finalize (GObject *object);
static void shell_process_set_property ( GObject *object,
					 guint property_id,
					 const GValue *value,
					 GParamSpec *pspec );
static void shell_process_get_property( GObject *object,
					guint property_id,
					GValue *value,
					GParamSpec *pspec );

G_DEFINE_TYPE( ShellProcess, shell_process, G_TYPE_OBJECT);

static void shell_process_class_init( ShellProcessClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->dispose = shell_process_dispose;
  gobject_class->finalize = shell_process_finalize;
  gobject_class->set_property = shell_process_set_property;
  gobject_class->get_property = shell_process_get_property;

  g_object_class_install_property (gobject_class,
				   PROP_ARGS,
				   g_param_spec_boxed ("args",
						       "Arguments",
						       "",
						       G_TYPE_STRV,
						       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

}

static void shell_process_init (ShellProcess *self)
{
  self->priv = g_new0 (ShellProcessPrivate, 1);
}

static void shell_process_dispose (GObject *object)
{
  ShellProcess *self = (ShellProcess*)object;

  G_OBJECT_CLASS (shell_process_parent_class)->dispose(object);
}

static void shell_process_finalize (GObject *object)
{
  ShellProcess *self = (ShellProcess*)object;

  g_free (self->priv);
  g_signal_handlers_destroy(object);
  G_OBJECT_CLASS (shell_process_parent_class)->finalize(object);
}
static void shell_process_set_property ( GObject *object,
                                         guint property_id,
                                         const GValue *value,
                                         GParamSpec *pspec ) 
{
  ShellProcess* self = SHELL_PROCESS(object);
  switch (property_id) {
  case PROP_ARGS:
    self->priv->args = (char**) g_value_dup_boxed (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void shell_process_get_property ( GObject *object,
                                         guint property_id,
                                         GValue *value,
                                         GParamSpec *pspec ) 
{
  ShellProcess* self = SHELL_PROCESS(object);
  switch (property_id) {
  case PROP_ARGS:
    g_value_set_boxed (value, self->priv->args);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

ShellProcess* shell_process_new(char **args)
{
  return (ShellProcess*) g_object_new(SHELL_TYPE_PROCESS,
				      "args", args,
				      NULL);
}

gboolean
shell_process_run (ShellProcess *self,
		   GError      **error)
{
  return g_spawn_async (NULL, self->priv->args, NULL,
			G_SPAWN_SEARCH_PATH, NULL, NULL,
			&self->priv->pid,
			error);
}
/*
int
shell_process_wait (ShellProcess *self)
{
  int status;

  waitpid ((pid_t) self->priv->pid, &status, 0);

  return status;
}
*/
