/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <gtk/gtk.h>

#include <display.h>

#include <girepository.h>

#include "shell-tray-manager.h"
#include "na-tray-manager.h"

#include "shell-gtk-embed.h"
#include "shell-embedded-window.h"
#include "shell-global.h"

struct _ShellTrayManagerPrivate {
  NaTrayManager *na_manager;
  ClutterStage *stage;
  ClutterColor bg_color;

  GHashTable *icons;
};

typedef struct {
  ShellTrayManager *manager;
  GtkWidget *socket;
  GtkWidget *window;
  ClutterActor *actor;
  gboolean emitted_plugged;
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

G_DEFINE_TYPE (ShellTrayManager, shell_tray_manager, G_TYPE_OBJECT);

static guint shell_tray_manager_signals [LAST_SIGNAL] = { 0 };

/* Sea Green - obnoxious to force people to set the background color */
static const ClutterColor default_color = { 0xbb, 0xff, 0xaa };

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
  manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager, SHELL_TYPE_TRAY_MANAGER,
                                               ShellTrayManagerPrivate);
  manager->priv->na_manager = na_tray_manager_new ();

  manager->priv->icons = g_hash_table_new_full (NULL, NULL,
                                                NULL, free_tray_icon);
  manager->priv->bg_color = default_color;

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
  g_hash_table_destroy (manager->priv->icons);

  G_OBJECT_CLASS (shell_tray_manager_parent_class)->finalize (object);
}

static void
shell_tray_manager_class_init (ShellTrayManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ShellTrayManagerPrivate));

  gobject_class->finalize = shell_tray_manager_finalize;
  gobject_class->set_property = shell_tray_manager_set_property;
  gobject_class->get_property = shell_tray_manager_get_property;

  shell_tray_manager_signals[TRAY_ICON_ADDED] =
    g_signal_new ("tray-icon-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ShellTrayManagerClass, tray_icon_added),
                  NULL, NULL,
                  gi_cclosure_marshal_generic,
                  G_TYPE_NONE, 2,
                  CLUTTER_TYPE_ACTOR,
                  G_TYPE_STRING);
  shell_tray_manager_signals[TRAY_ICON_REMOVED] =
    g_signal_new ("tray-icon-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ShellTrayManagerClass, tray_icon_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
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

void
shell_tray_manager_manage_stage (ShellTrayManager *manager,
                                 ClutterStage     *stage)
{
  Window stage_xwindow;
  GdkWindow *stage_window;
  GdkScreen *screen;

  g_return_if_fail (manager->priv->stage == NULL);

  manager->priv->stage = g_object_ref (stage);

  stage_xwindow = clutter_x11_get_stage_window (stage);

  /* This is a pretty ugly way to get the GdkScreen for the stage; it
   *  will normally go through the foreign_new() case with a
   *  round-trip to the X server, it might be nicer to pass the screen
   *  in in some way. (The Clutter/Mutter combo is currently incapable
   *  of multi-screen operation, so alternatively we could just assume
   *  that clutter_x11_get_default_screen() gives us the right
   *  screen.)
   */
  stage_window = gdk_window_lookup (stage_xwindow);
  if (stage_window)
    g_object_ref (stage_window);
  else
    stage_window = gdk_window_foreign_new (stage_xwindow);

  screen = gdk_drawable_get_screen (stage_window);

  g_object_unref (stage_window);

  na_tray_manager_manage_screen (manager->priv->na_manager, screen);
}

static GdkPixmap *
create_bg_pixmap (GdkColormap  *colormap,
                  ClutterColor *color)
{
  GdkScreen *screen = gdk_colormap_get_screen (colormap);
  GdkVisual *visual = gdk_colormap_get_visual (colormap);
  GdkPixmap *pixmap = gdk_pixmap_new (gdk_screen_get_root_window (screen),
                                      1, 1,
                                      visual->depth);
  cairo_t *cr;

  gdk_drawable_set_colormap (pixmap, colormap);

  cr = gdk_cairo_create (pixmap);
  cairo_set_source_rgb (cr,
                        color->red / 255.,
                        color->green / 255.,
                        color->blue / 255.);
  cairo_paint (cr);
  cairo_destroy (cr);

  return pixmap;
}

static void
shell_tray_manager_child_on_realize (GtkWidget             *widget,
                                     ShellTrayManagerChild *child)
{
  GdkPixmap *bg_pixmap;

  /* If the tray child is using an RGBA colormap (and so we have real
   * transparency), we don't need to worry about the background. If
   * not, we obey the bg-color property by creating a 1x1 pixmap of
   * that color and setting it as our background. Then "parent-relative"
   * background on the socket and the plug within that will cause
   * the icons contents to appear on top of our background color.
   */
  if (!na_tray_child_has_alpha (NA_TRAY_CHILD (child->socket)))
    {
      bg_pixmap = create_bg_pixmap (gtk_widget_get_colormap (widget),
                                    &child->manager->priv->bg_color);
      gdk_window_set_back_pixmap (widget->window, bg_pixmap, FALSE);
      g_object_unref (bg_pixmap);
    }
}

static char *
get_lowercase_wm_class_from_socket (ShellTrayManager  *manager,
                                    GtkSocket         *socket)
{
  GdkWindow *window;
  MetaScreen *screen;
  MetaDisplay *display;
  XClassHint class_hint;
  gboolean success;
  char *result;

  window = gtk_socket_get_plug_window (socket);
  g_return_val_if_fail (window != NULL, NULL);

  screen = shell_global_get_screen (shell_global_get ());
  display = meta_screen_get_display (screen);

  gdk_error_trap_push ();

  success = XGetClassHint (meta_display_get_xdisplay (display), GDK_WINDOW_XWINDOW (window), &class_hint);

  gdk_error_trap_pop ();

  if (!success)
    return NULL;

  result = g_ascii_strdown (class_hint.res_class, -1);
  XFree (class_hint.res_name);
  XFree (class_hint.res_class);
  return result;
}

static void
on_plug_added (GtkSocket        *socket,
               ShellTrayManager *manager)
{
  ShellTrayManagerChild *child;
  char *wm_class;

  child = g_hash_table_lookup (manager->priv->icons, socket);
  /* Only emit this signal once; the point of waiting until we
   * get the first plugged notification is to be able to get the WM_CLASS
   * from the child window.  But we don't want to emit this signal twice
   * if for some reason the socket gets replugged.
   */
  if (child->emitted_plugged)
    return;
  child->emitted_plugged = TRUE;

  wm_class = get_lowercase_wm_class_from_socket (manager, socket);
  if (!wm_class)
    return;

  g_signal_emit (manager, shell_tray_manager_signals[TRAY_ICON_ADDED], 0,
                 child->actor, wm_class);
  g_free (wm_class);
}

static void
na_tray_icon_added (NaTrayManager *na_manager, GtkWidget *socket,
                    gpointer user_data)
{
  ShellTrayManager *manager = user_data;
  GtkWidget *win;
  ClutterActor *icon;
  ShellTrayManagerChild *child;

  /* We don't need the NaTrayIcon to be composited on the window we
   * put it in: the window is the same size as the tray icon
   * and transparent. We can just use the default X handling of
   * subwindows as mode of SOURCE (replace the parent with the
   * child) and then composite the parent onto the stage.
   */
  na_tray_child_set_composited (NA_TRAY_CHILD (socket), FALSE);

  win = shell_embedded_window_new ();
  gtk_container_add (GTK_CONTAINER (win), socket);

  /* The colormap of the socket matches that of its contents; make
   * the window we put it in match that as well */
  gtk_widget_set_colormap (win, gtk_widget_get_colormap (socket));

  child = g_slice_new0 (ShellTrayManagerChild);
  child->manager = manager;
  child->window = win;
  child->socket = socket;

  g_signal_connect (win, "realize",
                    G_CALLBACK (shell_tray_manager_child_on_realize), child);

  gtk_widget_show_all (win);

  icon = shell_gtk_embed_new (SHELL_EMBEDDED_WINDOW (win));

  child->actor = g_object_ref (icon);
  g_hash_table_insert (manager->priv->icons, socket, child);

  g_signal_connect (socket, "plug-added", G_CALLBACK (on_plug_added), manager);
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
