#include "shell-global.h"

struct _ShellGlobal {
  GObject parent;

  ClutterActor *overlay_group;
};

struct _ShellGlobalClass {
  GObjectClass parent_class;
};

G_DEFINE_TYPE(ShellGlobal, shell_global, G_TYPE_OBJECT);

static void
shell_global_init(ShellGlobal *global)
{
}

static void
shell_global_class_init(ShellGlobalClass *klass)
{
}

ShellGlobal *
shell_global_get (void)
{
  static ShellGlobal *the_object = NULL;

  if (!the_object)
    the_object = g_object_new (SHELL_TYPE_GLOBAL, 0);

  return the_object;
}

void
shell_global_set_overlay_group (ShellGlobal  *global,
				ClutterActor *overlay_group)
{
  g_object_ref (overlay_group);

  if (global->overlay_group)
    g_object_unref(global->overlay_group);

  global->overlay_group = overlay_group;
}

ClutterActor *
shell_global_get_overlay_group (ShellGlobal  *global)
{
  return global->overlay_group;
}

void
shell_global_print_hello (ShellGlobal *global)
{
  g_print("Hello World!\n");
}
