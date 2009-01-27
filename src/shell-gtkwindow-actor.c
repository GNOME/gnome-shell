/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "shell-gtkwindow-actor.h"

#include <clutter/glx/clutter-glx.h>
#include <clutter/x11/clutter-x11.h>
#include <gdk/gdkx.h>

enum {
   PROP_0,

   PROP_WINDOW
};

G_DEFINE_TYPE (ShellGtkWindowActor, shell_gtk_window_actor, CLUTTER_GLX_TYPE_TEXTURE_PIXMAP);

struct _ShellGtkWindowActorPrivate {
  GtkWidget *window;
};

static void
shell_gtk_window_actor_set_property (GObject         *object,
                                     guint            prop_id,
                                     const GValue    *value,
                                     GParamSpec      *pspec)
{
  ShellGtkWindowActor *wactor = SHELL_GTK_WINDOW_ACTOR (object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      wactor->priv->window = g_value_dup_object (value);

      /* Here automatic=FALSE means to use CompositeRedirectManual.
       * That is, the X server shouldn't draw the window onto the
       * screen.
       */
      clutter_x11_texture_pixmap_set_window (CLUTTER_X11_TEXTURE_PIXMAP (wactor),
                                             GDK_WINDOW_XWINDOW (wactor->priv->window->window),
                                             FALSE);
      /* Here automatic has a different meaning--whether
       * ClutterX11TexturePixmap should process damage update and
       * refresh the pixmap itself.
       */
      clutter_x11_texture_pixmap_set_automatic (CLUTTER_X11_TEXTURE_PIXMAP (wactor), TRUE);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_gtk_window_actor_get_property (GObject         *object,
                                     guint            prop_id,
                                     GValue          *value,
                                     GParamSpec      *pspec)
{
  ShellGtkWindowActor *wactor = SHELL_GTK_WINDOW_ACTOR (object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      g_value_set_object (value, wactor->priv->window);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_gtk_window_actor_allocate (ClutterActor          *actor,
                                 const ClutterActorBox *box,
                                 gboolean               absolute_origin_changed)
{
  ShellGtkWindowActor *wactor = SHELL_GTK_WINDOW_ACTOR (actor);
  int wx = 0, wy = 0, x, y, ax, ay;

  CLUTTER_ACTOR_CLASS (shell_gtk_window_actor_parent_class)->
    allocate (actor, box, absolute_origin_changed);

  /* Find the actor's new coordinates in terms of the stage (which is
   * priv->window's parent window.
   */
  while (actor)
    {
      clutter_actor_get_position (actor, &x, &y);
      clutter_actor_get_anchor_point (actor, &ax, &ay);

      wx += x - ax;
      wy += y - ay;

      actor = clutter_actor_get_parent (actor);
    }

  gtk_window_move (GTK_WINDOW (wactor->priv->window), wx, wy);
}

static void
shell_gtk_window_actor_show (ClutterActor *actor)
{
  ShellGtkWindowActor *wactor = SHELL_GTK_WINDOW_ACTOR (actor);

  gtk_widget_show (wactor->priv->window);

  CLUTTER_ACTOR_CLASS (shell_gtk_window_actor_parent_class)->show (actor);
}

static void
shell_gtk_window_actor_hide (ClutterActor *actor)
{
  ShellGtkWindowActor *wactor = SHELL_GTK_WINDOW_ACTOR (actor);

  gtk_widget_hide (wactor->priv->window);

  CLUTTER_ACTOR_CLASS (shell_gtk_window_actor_parent_class)->hide (actor);
}

static void
shell_gtk_window_actor_dispose (GObject *object)
{
  ShellGtkWindowActor *wactor = SHELL_GTK_WINDOW_ACTOR (object);

  if (wactor->priv->window)
    {
      gtk_widget_destroy (wactor->priv->window);
      wactor->priv->window = NULL;
    }

  G_OBJECT_CLASS (shell_gtk_window_actor_parent_class)->dispose (object);
}

static void
shell_gtk_window_actor_class_init (ShellGtkWindowActorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ShellGtkWindowActorPrivate));

  object_class->get_property = shell_gtk_window_actor_get_property;
  object_class->set_property = shell_gtk_window_actor_set_property;
  object_class->dispose      = shell_gtk_window_actor_dispose;

  actor_class->allocate = shell_gtk_window_actor_allocate;
  actor_class->show     = shell_gtk_window_actor_show;
  actor_class->hide     = shell_gtk_window_actor_hide;

  g_object_class_install_property (object_class,
                                   PROP_WINDOW,
                                   g_param_spec_object ("window",
                                                        "Window",
                                                        "GtkWindow to wrap",
                                                        GTK_TYPE_WINDOW,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
shell_gtk_window_actor_init (ShellGtkWindowActor *actor)
{
  actor->priv = G_TYPE_INSTANCE_GET_PRIVATE (actor, SHELL_TYPE_GTK_WINDOW_ACTOR,
                                             ShellGtkWindowActorPrivate);
}

ClutterActor *
shell_gtk_window_actor_new (GtkWidget *window)
{
  return g_object_new (SHELL_TYPE_GTK_WINDOW_ACTOR,
                       "window", window,
                       NULL);
}
