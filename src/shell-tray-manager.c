/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <clutter/clutter.h>
#include <girepository.h>
#include <meta/display.h>

#include "shell-tray-manager.h"
#include "na-tray-manager.h"

#include "shell-tray-icon-private.h"
#include "shell-global.h"

typedef struct _ShellTrayManagerPrivate ShellTrayManagerPrivate;

struct _ShellTrayManager
{
  GObject parent_instance;

  ShellTrayManagerPrivate *priv;
};

struct _ShellTrayManagerPrivate {
  NaTrayManager *na_manager;
  ClutterColor bg_color;

  GHashTable *icons;
  StWidget *theme_widget;
};

typedef struct {
  ShellTrayManager *manager;
  NaTrayChild *tray_child;
  ClutterActor *actor;
} ShellTrayManagerChild;

enum {
  PROP_0,

  PROP_BG_COLOR
};

/* Signals */
enum
{
  TRAY_ICON_ADDED,
  TRAY_ICON_REMOVED,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (ShellTrayManager, shell_tray_manager, G_TYPE_OBJECT);

static guint shell_tray_manager_signals [LAST_SIGNAL] = { 0 };

static const ClutterColor default_color = { 0x00, 0x00, 0x00, 0xff };

static void shell_tray_manager_release_resources (ShellTrayManager *manager);

static void na_tray_icon_added (NaTrayManager *na_manager,
                                NaTrayChild   *child,
                                gpointer       manager);

static void na_tray_icon_removed (NaTrayManager *na_manager,
                                  NaTrayChild   *child,
                                  gpointer       manager);

static void
free_tray_icon (gpointer data)
{
  ShellTrayManagerChild *child = data;

  if (child->actor)
    {
      g_signal_handlers_disconnect_matched (child->actor, G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL, NULL, child);
      g_object_unref (child->actor);
    }
  g_free (child);
}

static void
shell_tray_manager_set_property(GObject         *object,
                                guint            prop_id,
                                const GValue    *value,
                                GParamSpec      *pspec)
{
  ShellTrayManager *manager = SHELL_TRAY_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BG_COLOR:
      {
        ClutterColor *color = g_value_get_boxed (value);
        if (color)
          manager->priv->bg_color = *color;
        else
          manager->priv->bg_color = default_color;
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_tray_manager_get_property(GObject         *object,
                                guint            prop_id,
                                GValue          *value,
                                GParamSpec      *pspec)
{
  ShellTrayManager *manager = SHELL_TRAY_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BG_COLOR:
      g_value_set_boxed (value, &manager->priv->bg_color);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_tray_manager_init (ShellTrayManager *manager)
{
  manager->priv = shell_tray_manager_get_instance_private (manager);

  manager->priv->bg_color = default_color;
}

static void
shell_tray_manager_finalize (GObject *object)
{
  ShellTrayManager *manager = SHELL_TRAY_MANAGER (object);

  shell_tray_manager_release_resources (manager);

  G_OBJECT_CLASS (shell_tray_manager_parent_class)->finalize (object);
}

static void
shell_tray_manager_class_init (ShellTrayManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = shell_tray_manager_finalize;
  gobject_class->set_property = shell_tray_manager_set_property;
  gobject_class->get_property = shell_tray_manager_get_property;

  shell_tray_manager_signals[TRAY_ICON_ADDED] =
    g_signal_new ("tray-icon-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);
  shell_tray_manager_signals[TRAY_ICON_REMOVED] =
    g_signal_new ("tray-icon-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);

  /* Lifting the CONSTRUCT_ONLY here isn't hard; you just need to
   * iterate through the icons, reset the background pixmap, and
   * call na_tray_child_force_redraw()
   */
  g_object_class_install_property (gobject_class,
                                   PROP_BG_COLOR,
                                   g_param_spec_boxed ("bg-color",
                                                       "BG Color",
                                                       "Background color (only if we don't have transparency)",
                                                       CLUTTER_TYPE_COLOR,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

ShellTrayManager *
shell_tray_manager_new (void)
{
  return g_object_new (SHELL_TYPE_TRAY_MANAGER, NULL);
}

static void
shell_tray_manager_ensure_resources (ShellTrayManager *manager)
{
  MetaDisplay *display;
  MetaX11Display *x11_display;

  if (manager->priv->na_manager != NULL)
    return;

  manager->priv->icons = g_hash_table_new_full (NULL, NULL,
                                                NULL, free_tray_icon);

  display = shell_global_get_display (shell_global_get ());
  x11_display = meta_display_get_x11_display (display);

  manager->priv->na_manager = na_tray_manager_new (x11_display);

  g_signal_connect (manager->priv->na_manager, "tray-icon-added",
                    G_CALLBACK (na_tray_icon_added), manager);
  g_signal_connect (manager->priv->na_manager, "tray-icon-removed",
                    G_CALLBACK (na_tray_icon_removed), manager);
}

static void
shell_tray_manager_release_resources (ShellTrayManager *manager)
{
  g_clear_object (&manager->priv->na_manager);
  g_clear_pointer (&manager->priv->icons, g_hash_table_destroy);
}

static void
shell_tray_manager_style_changed (StWidget *theme_widget,
                                  gpointer  user_data)
{
  ShellTrayManager *manager = user_data;
  StThemeNode *theme_node;
  StIconColors *icon_colors;

  if (manager->priv->na_manager == NULL)
    return;

  theme_node = st_widget_get_theme_node (theme_widget);
  icon_colors = st_theme_node_get_icon_colors (theme_node);
  na_tray_manager_set_colors (manager->priv->na_manager,
                              &icon_colors->foreground, &icon_colors->warning,
                              &icon_colors->error, &icon_colors->success);
}

static void
shell_tray_manager_manage_screen_internal (ShellTrayManager *manager)
{
  shell_tray_manager_ensure_resources (manager);
  na_tray_manager_manage (manager->priv->na_manager);
}

void
shell_tray_manager_manage_screen (ShellTrayManager *manager,
                                  StWidget         *theme_widget)
{
  MetaDisplay *display = shell_global_get_display (shell_global_get ());

  g_set_weak_pointer (&manager->priv->theme_widget, theme_widget);

  if (meta_display_get_x11_display (display) != NULL)
    shell_tray_manager_manage_screen_internal (manager);

  g_signal_connect_object (display, "x11-display-setup",
                           G_CALLBACK (shell_tray_manager_manage_screen_internal),
                           manager, G_CONNECT_SWAPPED);
  g_signal_connect_object (display, "x11-display-closing",
                           G_CALLBACK (shell_tray_manager_release_resources),
                           manager, G_CONNECT_SWAPPED);

  g_signal_connect_object (theme_widget, "style-changed",
                           G_CALLBACK (shell_tray_manager_style_changed),
                           manager, 0);
  shell_tray_manager_style_changed (theme_widget, manager);
}

void
shell_tray_manager_unmanage_screen (ShellTrayManager *manager)
{
  MetaDisplay *display = shell_global_get_display (shell_global_get ());

  g_signal_handlers_disconnect_by_data (display, manager);

  if (manager->priv->theme_widget != NULL)
    {
      g_signal_handlers_disconnect_by_func (manager->priv->theme_widget,
                                            G_CALLBACK (shell_tray_manager_style_changed),
                                            manager);
    }
  g_set_weak_pointer (&manager->priv->theme_widget, NULL);

  shell_tray_manager_release_resources (manager);
}

static void
on_plug_added (NaTrayChild      *tray_child,
               ShellTrayManager *manager)
{
  ShellTrayManagerChild *child;

  g_signal_handlers_disconnect_by_func (tray_child, on_plug_added, manager);

  child = g_hash_table_lookup (manager->priv->icons, tray_child);

  child->actor = shell_tray_icon_new (tray_child);
  g_object_ref_sink (child->actor);

  na_xembed_set_background_color (NA_XEMBED (tray_child),
                                  &manager->priv->bg_color);

  g_signal_emit (manager, shell_tray_manager_signals[TRAY_ICON_ADDED], 0,
                 child->actor);
}

static void
na_tray_icon_added (NaTrayManager *na_manager,
                    NaTrayChild   *tray_child,
                    gpointer       user_data)
{
  ShellTrayManager *manager = user_data;
  ShellTrayManagerChild *child;

  child = g_new0 (ShellTrayManagerChild, 1);
  child->manager = manager;
  child->tray_child = tray_child;

  g_hash_table_insert (manager->priv->icons, tray_child, child);

  g_signal_connect (tray_child, "plug-added",
                    G_CALLBACK (on_plug_added), manager);
}

static void
na_tray_icon_removed (NaTrayManager *na_manager,
                      NaTrayChild   *tray_child,
                      gpointer       user_data)
{
  ShellTrayManager *manager = user_data;
  ShellTrayManagerChild *child;

  child = g_hash_table_lookup (manager->priv->icons, tray_child);
  g_return_if_fail (child != NULL);

  if (child->actor != NULL)
    {
      /* Only emit signal if a corresponding tray-icon-added signal was emitted,
       * that is, if embedding did not fail and we got a plug-added
       */
      g_signal_emit (manager,
                     shell_tray_manager_signals[TRAY_ICON_REMOVED], 0,
                     child->actor);
    }

  g_hash_table_remove (manager->priv->icons, tray_child);
}
