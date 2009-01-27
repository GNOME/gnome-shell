#ifndef __SHELL_GTK_WINDOW_ACTOR_H__
#define __SHELL_GTK_WINDOW_ACTOR_H__

#include <clutter/glx/clutter-glx.h>
#include <gtk/gtk.h>

#define SHELL_TYPE_GTK_WINDOW_ACTOR                 (shell_gtk_window_actor_get_type ())
#define SHELL_GTK_WINDOW_ACTOR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_GTK_WINDOW_ACTOR, ShellGtkWindowActor))
#define SHELL_GTK_WINDOW_ACTOR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_GTK_WINDOW_ACTOR, ShellGtkWindowActorClass))
#define SHELL_IS_GTK_WINDOW_ACTOR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_GTK_WINDOW_ACTOR))
#define SHELL_IS_GTK_WINDOW_ACTOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_GTK_WINDOW_ACTOR))
#define SHELL_GTK_WINDOW_ACTOR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_GTK_WINDOW_ACTOR, ShellGtkWindowActorClass))

typedef struct _ShellGtkWindowActor        ShellGtkWindowActor;
typedef struct _ShellGtkWindowActorClass   ShellGtkWindowActorClass;

typedef struct _ShellGtkWindowActorPrivate ShellGtkWindowActorPrivate;

struct _ShellGtkWindowActor
{
    ClutterGLXTexturePixmap parent;

    ShellGtkWindowActorPrivate *priv;
};

struct _ShellGtkWindowActorClass
{
    ClutterGLXTexturePixmapClass parent_class;

};

GType shell_gtk_window_actor_get_type (void) G_GNUC_CONST;
ClutterActor *shell_gtk_window_actor_new (GtkWidget *window);

#endif /* __SHELL_GTK_WINDOW_ACTOR_H__ */
