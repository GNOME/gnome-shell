/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include <clutter/clutter.h>
#include <clutter/glx/clutter-glx.h>
#include <clutter/x11/clutter-x11.h>
#include <gtk/gtk.h>

#include "shell-tray-manager.h"
#include "na-tray-manager.h"

struct _ShellTrayManagerPrivate {
  NaTrayManager *na_manager;
  ClutterStage *stage;
  GdkWindow *stage_window;

  GHashTable *icons;
};

typedef struct {
  ShellTrayManager *manager;
  GtkWidget *socket;
  GtkWidget *window;
  ClutterActor *actor;
} ShellTrayManagerChild;

/* Signals */
enum
{
  TRAY_ICON_ADDED,
  TRAY_ICON_REMOVED,
  LAST_SIGNAL
};

G_DEFINE_TYPE (ShellTrayManager, shell_tray_manager, G_TYPE_OBJECT);

static guint shell_tray_manager_signals [LAST_SIGNAL] = { 0 };

static void na_tray_icon_added (NaTrayManager *na_manager, GtkWidget *child, gpointer manager);
static void na_tray_icon_removed (NaTrayManager *na_manager, GtkWidget *child, gpointer manager);

static void
free_tray_icon (gpointer data)
{
  ShellTrayManagerChild *child = data;

  gtk_widget_hide (child->window);
  gtk_widget_destroy (child->window);
  g_signal_handlers_disconnect_matched (child->actor, G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL, NULL, child);
  g_object_unref (child->actor);
  g_slice_free (ShellTrayManagerChild, child);
}

static void
shell_tray_manager_init (ShellTrayManager *manager)
{
  manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager, SHELL_TYPE_TRAY_MANAGER,
                                               ShellTrayManagerPrivate);
  manager->priv->na_manager = na_tray_manager_new ();

  manager->priv->icons = g_hash_table_new_full (NULL, NULL,
                                                NULL, free_tray_icon);

  g_signal_connect (manager->priv->na_manager, "tray-icon-added",
                    G_CALLBACK (na_tray_icon_added), manager);
  g_signal_connect (manager->priv->na_manager, "tray-icon-removed",
                    G_CALLBACK (na_tray_icon_removed), manager);
}

static void
shell_tray_manager_finalize (GObject *object)
{
  ShellTrayManager *manager = SHELL_TRAY_MANAGER (object);

  g_object_unref (manager->priv->na_manager);
  g_object_unref (manager->priv->stage);
  g_object_unref (manager->priv->stage_window);
  g_hash_table_destroy (manager->priv->icons);

  G_OBJECT_CLASS (shell_tray_manager_parent_class)->finalize (object);
}

static void
shell_tray_manager_class_init (ShellTrayManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ShellTrayManagerPrivate));

  gobject_class->finalize = shell_tray_manager_finalize;

  shell_tray_manager_signals[TRAY_ICON_ADDED] =
    g_signal_new ("tray-icon-added",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ShellTrayManagerClass, tray_icon_added),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);
  shell_tray_manager_signals[TRAY_ICON_REMOVED] =
    g_signal_new ("tray-icon-removed",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ShellTrayManagerClass, tray_icon_removed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);
}

ShellTrayManager *
shell_tray_manager_new (void)
{
  return g_object_new (SHELL_TYPE_TRAY_MANAGER, NULL);
}

void
shell_tray_manager_manage_stage (ShellTrayManager *manager,
                                 ClutterStage     *stage)
{
  Window stage_xwin;

  g_return_if_fail (manager->priv->stage == NULL);

  manager->priv->stage = g_object_ref (stage);
  stage_xwin = clutter_x11_get_stage_window (stage);
  manager->priv->stage_window = gdk_window_lookup (stage_xwin);
  if (manager->priv->stage_window)
    g_object_ref (manager->priv->stage_window);
  else
    manager->priv->stage_window = gdk_window_foreign_new (stage_xwin);

  na_tray_manager_manage_screen (manager->priv->na_manager,
                                 gdk_drawable_get_screen (GDK_DRAWABLE (manager->priv->stage_window)));
}

static void
actor_moved (GObject *object, GParamSpec *param, gpointer user_data)
{
  ShellTrayManagerChild *child = user_data;
  ClutterActor *actor = child->actor;
  int wx = 0, wy = 0, x, y, ax, ay;

  /* Find the actor's new coordinates in terms of the stage (which is
   * child->window's parent window.
   */
  while (actor)
    {
      clutter_actor_get_position (actor, &x, &y);
      clutter_actor_get_anchor_point (actor, &ax, &ay);

      wx += x - ax;
      wy += y - ay;

      actor = clutter_actor_get_parent (actor);
    }

  gtk_window_move (GTK_WINDOW (child->window), wx, wy);
}

static void
na_tray_icon_added (NaTrayManager *na_manager, GtkWidget *socket,
                    gpointer user_data)
{
  ShellTrayManager *manager = user_data;
  GtkWidget *win;
  ClutterActor *icon;
  ShellTrayManagerChild *child;

  win = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_container_add (GTK_CONTAINER (win), socket);

  gtk_widget_set_size_request (win, 24, 24);
  gtk_widget_realize (win);

  gtk_widget_set_parent_window (win, manager->priv->stage_window);
  gdk_window_reparent (win->window, manager->priv->stage_window, 0, 0);
  gtk_widget_show_all (win);

  icon = clutter_glx_texture_pixmap_new_with_window (GDK_WINDOW_XWINDOW (win->window));
  clutter_x11_texture_pixmap_set_automatic (CLUTTER_X11_TEXTURE_PIXMAP (icon), TRUE);
  clutter_actor_set_size (icon, 24, 24);

  child = g_slice_new (ShellTrayManagerChild);
  child->window = win;
  child->socket = socket;
  child->actor = g_object_ref (icon);
  g_hash_table_insert (manager->priv->icons, socket, child);

  g_signal_connect (child->actor, "notify::x",
                    G_CALLBACK (actor_moved), child);
  g_signal_connect (child->actor, "notify::y",
                    G_CALLBACK (actor_moved), child);

  g_signal_emit (manager,
                 shell_tray_manager_signals[TRAY_ICON_ADDED], 0,
                 icon);
}

static void
na_tray_icon_removed (NaTrayManager *na_manager, GtkWidget *socket,
                      gpointer user_data)
{
  ShellTrayManager *manager = user_data;
  ShellTrayManagerChild *child;

  child = g_hash_table_lookup (manager->priv->icons, socket);
  g_return_if_fail (child != NULL);

  g_signal_emit (manager,
                 shell_tray_manager_signals[TRAY_ICON_REMOVED], 0,
                 child->actor);
  g_hash_table_remove (manager->priv->icons, socket);
}
