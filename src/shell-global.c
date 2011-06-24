/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <X11/extensions/Xfixes.h>
#include <canberra.h>
#include <clutter/glx/clutter-glx.h>
#include <clutter/x11/clutter-x11.h>
#include <dbus/dbus-glib.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <gjs/gjs-module.h>
#include <girepository.h>
#include <meta/display.h>
#include <meta/util.h>

/* Memory report bits */
#ifdef HAVE_MALLINFO
#include <malloc.h>
#endif

#include "shell-enum-types.h"
#include "shell-global-private.h"
#include "shell-jsapi-compat-private.h"
#include "shell-marshal.h"
#include "shell-perf-log.h"
#include "shell-window-tracker.h"
#include "shell-wm.h"
#include "st.h"

static ShellGlobal *the_object = NULL;

static void grab_notify (GtkWidget *widget, gboolean is_grab, gpointer user_data);

struct _ShellGlobal {
  GObject parent;
  
  /* We use this window to get a notification from GTK+ when
   * a widget in our process does a GTK+ grab.  See
   * http://bugzilla.gnome.org/show_bug.cgi?id=570641
   * 
   * This window is never mapped or shown.
   */
  GtkWindow *grab_notifier;
  gboolean gtk_grab_active;

  ShellStageInputMode input_mode;
  XserverRegion input_region;

  GjsContext *js_context;
  MetaPlugin *plugin;
  ShellWM *wm;
  GSettings *settings;
  const char *datadir;
  const char *imagedir;
  const char *userdatadir;
  StFocusManager *focus_manager;

  GdkWindow    *stage_window;

  guint work_count;
  GSList *leisure_closures;
  guint leisure_function_id;

  /* For sound notifications */
  ca_context *sound_context;

  guint32 xdnd_timestamp;
};

enum {
  PROP_0,

  PROP_OVERLAY_GROUP,
  PROP_SCREEN,
  PROP_GDK_SCREEN,
  PROP_SCREEN_WIDTH,
  PROP_SCREEN_HEIGHT,
  PROP_STAGE,
  PROP_STAGE_INPUT_MODE,
  PROP_WINDOW_GROUP,
  PROP_BACKGROUND_ACTOR,
  PROP_WINDOW_MANAGER,
  PROP_SETTINGS,
  PROP_DATADIR,
  PROP_IMAGEDIR,
  PROP_USERDATADIR,
  PROP_FOCUS_MANAGER,
};

/* Signals */
enum
{
 XDND_POSITION_CHANGED,
 XDND_LEAVE,
 XDND_ENTER,
 NOTIFY_ERROR,
 LAST_SIGNAL
};

G_DEFINE_TYPE(ShellGlobal, shell_global, G_TYPE_OBJECT);

static guint shell_global_signals [LAST_SIGNAL] = { 0 };

static void
shell_global_set_property(GObject         *object,
                          guint            prop_id,
                          const GValue    *value,
                          GParamSpec      *pspec)
{
  ShellGlobal *global = SHELL_GLOBAL (object);

  switch (prop_id)
    {
    case PROP_STAGE_INPUT_MODE:
      shell_global_set_stage_input_mode (global, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_global_get_property(GObject         *object,
                          guint            prop_id,
                          GValue          *value,
                          GParamSpec      *pspec)
{
  ShellGlobal *global = SHELL_GLOBAL (object);

  switch (prop_id)
    {
    case PROP_OVERLAY_GROUP:
      g_value_set_object (value, meta_plugin_get_overlay_group (global->plugin));
      break;
    case PROP_SCREEN:
      g_value_set_object (value, shell_global_get_screen (global));
      break;
    case PROP_GDK_SCREEN:
      g_value_set_object (value, shell_global_get_gdk_screen (global));
      break;
    case PROP_SCREEN_WIDTH:
      {
        int width, height;

        meta_plugin_query_screen_size (global->plugin, &width, &height);
        g_value_set_int (value, width);
      }
      break;
    case PROP_SCREEN_HEIGHT:
      {
        int width, height;

        meta_plugin_query_screen_size (global->plugin, &width, &height);
        g_value_set_int (value, height);
      }
      break;
    case PROP_STAGE:
      g_value_set_object (value, meta_plugin_get_stage (global->plugin));
      break;
    case PROP_STAGE_INPUT_MODE:
      g_value_set_enum (value, global->input_mode);
      break;
    case PROP_WINDOW_GROUP:
      g_value_set_object (value, meta_plugin_get_window_group (global->plugin));
      break;
    case PROP_BACKGROUND_ACTOR:
      g_value_set_object (value, meta_plugin_get_background_actor (global->plugin));
      break;
    case PROP_WINDOW_MANAGER:
      g_value_set_object (value, global->wm);
      break;
    case PROP_SETTINGS:
      g_value_set_object (value, global->settings);
      break;
    case PROP_DATADIR:
      g_value_set_string (value, global->datadir);
      break;
    case PROP_IMAGEDIR:
      g_value_set_string (value, global->imagedir);
      break;
    case PROP_USERDATADIR:
      g_value_set_string (value, global->userdatadir);
      break;
    case PROP_FOCUS_MANAGER:
      g_value_set_object (value, global->focus_manager);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_global_init (ShellGlobal *global)
{
  const char *datadir = g_getenv ("GNOME_SHELL_DATADIR");
  const char *shell_js = g_getenv("GNOME_SHELL_JS");
  char *imagedir, **search_path;

  if (!datadir)
    datadir = GNOME_SHELL_DATADIR;
  global->datadir = datadir;

  /* We make sure imagedir ends with a '/', since the JS won't have
   * access to g_build_filename() and so will end up just
   * concatenating global.imagedir to a filename.
   */
  imagedir = g_build_filename (datadir, "images/", NULL);
  if (g_file_test (imagedir, G_FILE_TEST_IS_DIR))
    global->imagedir = imagedir;
  else
    {
      g_free (imagedir);
      global->imagedir = g_strdup_printf ("%s/", datadir);
    }

  /* Ensure config dir exists for later use */
  global->userdatadir = g_build_filename (g_get_user_data_dir (), "gnome-shell", NULL);
  g_mkdir_with_parents (global->userdatadir, 0700);

  global->settings = g_settings_new ("org.gnome.shell");
  
  global->grab_notifier = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
  g_signal_connect (global->grab_notifier, "grab-notify", G_CALLBACK (grab_notify), global);
  global->gtk_grab_active = FALSE;

  global->stage_window = NULL;

  global->input_mode = SHELL_STAGE_INPUT_MODE_NORMAL;

  ca_context_create (&global->sound_context);
  ca_context_change_props (global->sound_context, CA_PROP_APPLICATION_NAME, PACKAGE_NAME, CA_PROP_APPLICATION_ID, "org.gnome.Shell", NULL);
  ca_context_open (global->sound_context);

  if (!shell_js)
    shell_js = JSDIR;
  search_path = g_strsplit (shell_js, ":", -1);
  global->js_context = g_object_new (GJS_TYPE_CONTEXT,
                                     "search-path", search_path,
                                     "js-version", "1.8",
                                     NULL);
  g_strfreev (search_path);
}

static void
shell_global_finalize (GObject *object)
{
  ShellGlobal *global = SHELL_GLOBAL (object);

  g_object_unref (global->js_context);
  gtk_widget_destroy (GTK_WIDGET (global->grab_notifier));
  g_object_unref (global->settings);

  the_object = NULL;

  G_OBJECT_CLASS(shell_global_parent_class)->finalize (object);
}

static void
shell_global_class_init (ShellGlobalClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shell_global_get_property;
  gobject_class->set_property = shell_global_set_property;
  gobject_class->finalize = shell_global_finalize;

  /* Emitted from gnome-shell-plugin.c during event handling */
  shell_global_signals[XDND_POSITION_CHANGED] =
      g_signal_new ("xdnd-position-changed",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    _shell_marshal_VOID__INT_INT,
                    G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

  /* Emitted from gnome-shell-plugin.c during event handling */
  shell_global_signals[XDND_LEAVE] =
      g_signal_new ("xdnd-leave",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

  /* Emitted from gnome-shell-plugin.c during event handling */
  shell_global_signals[XDND_ENTER] =
      g_signal_new ("xdnd-enter",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

  shell_global_signals[NOTIFY_ERROR] =
      g_signal_new ("notify-error",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    gi_cclosure_marshal_generic,
                    G_TYPE_NONE, 2,
                    G_TYPE_STRING,
                    G_TYPE_STRING);

  g_object_class_install_property (gobject_class,
                                   PROP_OVERLAY_GROUP,
                                   g_param_spec_object ("overlay-group",
                                                        "Overlay Group",
                                                        "Actor holding objects that appear above the desktop contents",
                                                        CLUTTER_TYPE_ACTOR,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_SCREEN,
                                   g_param_spec_object ("screen",
                                                        "Screen",
                                                        "Metacity screen object for the shell",
                                                        META_TYPE_SCREEN,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_GDK_SCREEN,
                                   g_param_spec_object ("gdk-screen",
                                                        "GdkScreen",
                                                        "Gdk screen object for the shell",
                                                        GDK_TYPE_SCREEN,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_SCREEN_WIDTH,
                                   g_param_spec_int ("screen-width",
                                                     "Screen Width",
                                                     "Screen width, in pixels",
                                                     0, G_MAXINT, 1,
                                                     G_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_SCREEN_HEIGHT,
                                   g_param_spec_int ("screen-height",
                                                     "Screen Height",
                                                     "Screen height, in pixels",
                                                     0, G_MAXINT, 1,
                                                     G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_STAGE,
                                   g_param_spec_object ("stage",
                                                        "Stage",
                                                        "Stage holding the desktop scene graph",
                                                        CLUTTER_TYPE_ACTOR,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_STAGE_INPUT_MODE,
                                   g_param_spec_enum ("stage-input-mode",
                                                      "Stage input mode",
                                                      "The stage input mode",
                                                      SHELL_TYPE_STAGE_INPUT_MODE,
                                                      SHELL_STAGE_INPUT_MODE_NORMAL,
                                                      G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_WINDOW_GROUP,
                                   g_param_spec_object ("window-group",
                                                        "Window Group",
                                                        "Actor holding window actors",
                                                        CLUTTER_TYPE_ACTOR,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_BACKGROUND_ACTOR,
                                   g_param_spec_object ("background-actor",
                                                        "Background Actor",
                                                        "Actor drawing root window background",
                                                        CLUTTER_TYPE_ACTOR,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_WINDOW_MANAGER,
                                   g_param_spec_object ("window-manager",
                                                        "Window Manager",
                                                        "Window management interface",
                                                        SHELL_TYPE_WM,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_SETTINGS,
                                   g_param_spec_object ("settings",
                                                        "Settings",
                                                        "GSettings instance for gnome-shell configuration",
                                                        G_TYPE_SETTINGS,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_DATADIR,
                                   g_param_spec_string ("datadir",
                                                        "Data directory",
                                                        "Directory containing gnome-shell data files",
                                                        NULL,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_IMAGEDIR,
                                   g_param_spec_string ("imagedir",
                                                        "Image directory",
                                                        "Directory containing gnome-shell image files",
                                                        NULL,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_USERDATADIR,
                                   g_param_spec_string ("userdatadir",
                                                        "User data directory",
                                                        "Directory containing gnome-shell user data",
                                                        NULL,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_FOCUS_MANAGER,
                                   g_param_spec_object ("focus-manager",
                                                        "Focus manager",
                                                        "The shell's StFocusManager",
                                                        ST_TYPE_FOCUS_MANAGER,
                                                        G_PARAM_READABLE));
}

/**
 * shell_global_get:
 *
 * Gets the singleton global object that represents the desktop.
 *
 * Return value: (transfer none): the singleton global object
 */
ShellGlobal *
shell_global_get (void)
{
  if (!the_object)
    the_object = g_object_new (SHELL_TYPE_GLOBAL, 0);

  return the_object;
}

static void
focus_window_changed (MetaDisplay *display,
                      GParamSpec  *param,
                      gpointer     user_data)
{
  ShellGlobal *global = user_data;

  if (global->input_mode == SHELL_STAGE_INPUT_MODE_FOCUSED &&
      meta_display_get_focus_window (display) != NULL)
    shell_global_set_stage_input_mode (global, SHELL_STAGE_INPUT_MODE_NORMAL);
}

static void
shell_global_focus_stage (ShellGlobal *global)
{
  Display *xdpy;
  ClutterActor *stage;
  Window xstage;

  stage = meta_plugin_get_stage (global->plugin);
  xstage = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));
  xdpy = meta_plugin_get_xdisplay (global->plugin);
  XSetInputFocus (xdpy, xstage, RevertToPointerRoot,
                  shell_global_get_current_time (global));
}

/**
 * shell_global_set_stage_input_mode:
 * @global: the #ShellGlobal
 * @mode: the stage input mode
 *
 * Sets the input mode of the stage; when @mode is
 * %SHELL_STAGE_INPUT_MODE_NONREACTIVE, then the stage does not absorb
 * any clicks, but just passes them through to underlying windows.
 * When it is %SHELL_STAGE_INPUT_MODE_NORMAL, then the stage accepts
 * clicks in the region defined by
 * shell_global_set_stage_input_region() but passes through clicks
 * outside that region. When it is %SHELL_STAGE_INPUT_MODE_FULLSCREEN,
 * the stage absorbs all input.
 *
 * When the input mode is %SHELL_STAGE_INPUT_MODE_FOCUSED, the pointer
 * is handled as with %SHELL_STAGE_INPUT_MODE_NORMAL, but additionally
 * the stage window has the keyboard focus. If the stage loses the
 * focus (eg, because the user clicked into a window) the input mode
 * will revert to %SHELL_STAGE_INPUT_MODE_NORMAL.
 *
 * Note that whenever a mutter-internal Gtk widget has a pointer grab,
 * the shell behaves as though it was in
 * %SHELL_STAGE_INPUT_MODE_NONREACTIVE, to ensure that the widget gets
 * any clicks it is expecting.
 */
void
shell_global_set_stage_input_mode (ShellGlobal         *global,
                                   ShellStageInputMode  mode)
{
  g_return_if_fail (SHELL_IS_GLOBAL (global));

  if (mode == SHELL_STAGE_INPUT_MODE_NONREACTIVE || global->gtk_grab_active)
    meta_plugin_set_stage_reactive (global->plugin, FALSE);
  else if (mode == SHELL_STAGE_INPUT_MODE_FULLSCREEN || !global->input_region)
    meta_plugin_set_stage_reactive (global->plugin, TRUE);
  else
    meta_plugin_set_stage_input_region (global->plugin, global->input_region);

  if (mode == SHELL_STAGE_INPUT_MODE_FOCUSED)
    shell_global_focus_stage (global);

  if (mode != global->input_mode)
    {
      global->input_mode = mode;
      g_object_notify (G_OBJECT (global), "stage-input-mode");
    }
}

/**
 * shell_global_set_cursor:
 * @global: A #ShellGlobal
 * @type: the type of the cursor
 *
 * Set the cursor on the stage window.
 */
void
shell_global_set_cursor (ShellGlobal *global,
                         ShellCursor type)
{
  const char *name;
  GdkCursor *cursor;

  switch (type)
    {
    case SHELL_CURSOR_DND_IN_DRAG:
      name = "dnd-none";
      break;
    case SHELL_CURSOR_DND_MOVE:
      name = "dnd-move";
      break;
    case SHELL_CURSOR_DND_COPY:
      name = "dnd-copy";
      break;
    case SHELL_CURSOR_DND_UNSUPPORTED_TARGET:
      name = "dnd-none";
      break;
    case SHELL_CURSOR_POINTING_HAND:
      name = "hand";
      break;
    default:
      g_return_if_reached ();
    }

  cursor = gdk_cursor_new_from_name (gdk_display_get_default (), name);
  if (!cursor)
    {
      GdkCursorType cursor_type;
      switch (type)
        {
        case SHELL_CURSOR_DND_IN_DRAG:
          cursor_type = GDK_FLEUR;
          break;
        case SHELL_CURSOR_DND_MOVE:
          cursor_type = GDK_TARGET;
          break;
        case SHELL_CURSOR_DND_COPY:
          cursor_type = GDK_PLUS;
          break;
        case SHELL_CURSOR_POINTING_HAND:
          cursor_type = GDK_HAND2;
        case SHELL_CURSOR_DND_UNSUPPORTED_TARGET:
          cursor_type = GDK_X_CURSOR;
          break;
        default:
          g_return_if_reached ();
        }
      cursor = gdk_cursor_new (cursor_type);
    }
  if (!global->stage_window)
    {
      ClutterStage *stage = CLUTTER_STAGE (meta_plugin_get_stage (global->plugin));

      global->stage_window = gdk_x11_window_foreign_new_for_display (gdk_display_get_default (),
                                                                     clutter_x11_get_stage_window (stage));
    }

  gdk_window_set_cursor (global->stage_window, cursor);

  gdk_cursor_unref (cursor);
}

/**
 * shell_global_unset_cursor:
 * @global: A #ShellGlobal
 *
 * Unset the cursor on the stage window.
 */
void
shell_global_unset_cursor (ShellGlobal  *global)
{
  if (!global->stage_window) /* cursor has never been set */
    return;

  gdk_window_set_cursor (global->stage_window, NULL);
}

/**
 * shell_global_set_stage_input_region:
 * @global: the #ShellGlobal
 * @rectangles: (element-type Meta.Rectangle): a list of #MetaRectangle
 * describing the input region.
 *
 * Sets the area of the stage that is responsive to mouse clicks when
 * the stage mode is %SHELL_STAGE_INPUT_MODE_NORMAL (but does not change the
 * current stage mode).
 */
void
shell_global_set_stage_input_region (ShellGlobal *global,
                                     GSList      *rectangles)
{
  MetaScreen *screen = meta_plugin_get_screen (global->plugin);
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdpy = meta_display_get_xdisplay (display);
  MetaRectangle *rect;
  XRectangle *rects;
  int nrects, i;
  GSList *r;

  g_return_if_fail (SHELL_IS_GLOBAL (global));

  nrects = g_slist_length (rectangles);
  rects = g_new (XRectangle, nrects);
  for (r = rectangles, i = 0; r; r = r->next, i++)
    {
      rect = (MetaRectangle *)r->data;
      rects[i].x = rect->x;
      rects[i].y = rect->y;
      rects[i].width = rect->width;
      rects[i].height = rect->height;
    }

  if (global->input_region)
    XFixesDestroyRegion (xdpy, global->input_region);

  global->input_region = XFixesCreateRegion (xdpy, rects, nrects);
  g_free (rects);

  /* set_stage_input_mode() will figure out whether or not we
   * should actually change the input region right now.
   */
  shell_global_set_stage_input_mode (global, global->input_mode);
}

/**
 * shell_global_get_screen:
 *
 * Return value: (transfer none): The default #MetaScreen
 */
MetaScreen *
shell_global_get_screen (ShellGlobal  *global)
{
  return meta_plugin_get_screen (global->plugin);
}

/**
 * shell_global_get_gdk_screen:
 *
 * Return value: (transfer none): Gdk screen object for the shell
 */
GdkScreen *
shell_global_get_gdk_screen (ShellGlobal *global)
{
  g_return_val_if_fail (SHELL_IS_GLOBAL (global), NULL);

  return gdk_screen_get_default ();
}

/**
 * shell_global_get_window_actors:
 *
 * Gets the list of #MetaWindowActor for the plugin's screen
 *
 * Return value: (element-type Meta.WindowActor) (transfer none): the list of windows
 */
GList *
shell_global_get_window_actors (ShellGlobal *global)
{
  g_return_val_if_fail (SHELL_IS_GLOBAL (global), NULL);

  return meta_plugin_get_window_actors (global->plugin);
}

static void
global_stage_notify_width (GObject    *gobject,
                           GParamSpec *pspec,
                           gpointer    data)
{
  ShellGlobal *global = SHELL_GLOBAL (data);

  g_object_notify (G_OBJECT (global), "screen-width");
}

static void
global_stage_notify_height (GObject    *gobject,
                            GParamSpec *pspec,
                            gpointer    data)
{
  ShellGlobal *global = SHELL_GLOBAL (data);

  g_object_notify (G_OBJECT (global), "screen-height");
}

static void
global_stage_before_paint (ClutterStage *stage,
                           ShellGlobal  *global)
{
  shell_perf_log_event (shell_perf_log_get_default (),
                        "clutter.stagePaintStart");
}

static void
global_stage_after_paint (ClutterStage *stage,
                          ShellGlobal  *global)
{
  shell_perf_log_event (shell_perf_log_get_default (),
                        "clutter.stagePaintDone");
}

void
_shell_global_set_plugin (ShellGlobal *global,
                          MetaPlugin  *plugin)
{
  ClutterActor *stage;
  MetaScreen *screen;
  MetaDisplay *display;

  g_return_if_fail (SHELL_IS_GLOBAL (global));
  g_return_if_fail (global->plugin == NULL);

  global->plugin = plugin;
  global->wm = shell_wm_new (plugin);

  stage = meta_plugin_get_stage (plugin);

  g_signal_connect (stage, "notify::width",
                    G_CALLBACK (global_stage_notify_width), global);
  g_signal_connect (stage, "notify::height",
                    G_CALLBACK (global_stage_notify_height), global);

  g_signal_connect (stage, "paint",
                    G_CALLBACK (global_stage_before_paint), global);
  g_signal_connect_after (stage, "paint",
                          G_CALLBACK (global_stage_after_paint), global);

  shell_perf_log_define_event (shell_perf_log_get_default(),
                               "clutter.stagePaintStart",
                               "Start of stage page repaint",
                               "");
  shell_perf_log_define_event (shell_perf_log_get_default(),
                               "clutter.stagePaintDone",
                               "End of stage page repaint",
                               "");

  screen = meta_plugin_get_screen (global->plugin);
  display = meta_screen_get_display (screen);
  g_signal_connect (display, "notify::focus-window",
                    G_CALLBACK (focus_window_changed), global);

  global->focus_manager = st_focus_manager_get_for_stage (CLUTTER_STAGE (stage));
}

GjsContext *
_shell_global_get_gjs_context (ShellGlobal *global)
{
  return global->js_context;
}

/**
 * shell_global_begin_modal:
 * @global: a #ShellGlobal
 *
 * Grabs the keyboard and mouse to the stage window. The stage will
 * receive all keyboard and mouse events until shell_global_end_modal()
 * is called. This is used to implement "modes" for the shell, such as the
 * overview mode or the "looking glass" debug overlay, that block
 * application and normal key shortcuts.
 *
 * Returns value: %TRUE if we succesfully entered the mode. %FALSE if we couldn't
 *  enter the mode. Failure may occur because an application has the pointer
 *  or keyboard grabbed, because Mutter is in a mode itself like moving a
 *  window or alt-Tab window selection, or because shell_global_begin_modal()
 *  was previouly called.
 */
gboolean
shell_global_begin_modal (ShellGlobal *global,
                          guint32      timestamp)
{
  ClutterStage *stage = CLUTTER_STAGE (meta_plugin_get_stage (global->plugin));
  Window stagewin = clutter_x11_get_stage_window (stage);

  return meta_plugin_begin_modal (global->plugin, stagewin, None, 0, timestamp);
}

/**
 * shell_global_end_modal:
 * @global: a #ShellGlobal
 *
 * Undoes the effect of shell_global_begin_modal().
 */
void
shell_global_end_modal (ShellGlobal *global,
                        guint32      timestamp)
{
  meta_plugin_end_modal (global->plugin, timestamp);
}

/**
 * shell_global_create_pointer_barrier
 * @global: a #ShellGlobal
 * @x1: left X coordinate
 * @y1: top Y coordinate
 * @x2: right X coordinate
 * @y2: bottom Y coordinate
 * @directions: The directions we're allowed to pass through
 *
 * If supported by X creates a pointer barrier.
 *
 * Return value: value you can pass to shell_global_destroy_pointer_barrier()
 */
guint32
shell_global_create_pointer_barrier (ShellGlobal *global,
                                     int x1, int y1, int x2, int y2,
                                     int directions)
{
#if HAVE_XFIXESCREATEPOINTERBARRIER
  Display *xdpy;

  xdpy = meta_plugin_get_xdisplay (global->plugin);

  return (guint32)
    XFixesCreatePointerBarrier (xdpy, DefaultRootWindow(xdpy),
                                x1, y1,
                                x2, y2,
                                directions,
                                0, NULL);
#else
  return 0;
#endif
}

/**
 * shell_global_destroy_pointer_barrier
 * @global: a #ShellGlobal
 * @barrier: a pointer barrier
 *
 * Destroys the @barrier created by shell_global_create_pointer_barrier().
 */
void
shell_global_destroy_pointer_barrier (ShellGlobal *global, guint32 barrier)
{
#if HAVE_XFIXESCREATEPOINTERBARRIER
  Display *xdpy;

  g_return_if_fail (barrier > 0);

  xdpy = meta_plugin_get_xdisplay (global->plugin);
  XFixesDestroyPointerBarrier (xdpy, (PointerBarrier)barrier);
#endif
}


/**
 * shell_global_add_extension_importer:
 * @target_object_script: JavaScript code evaluating to a target object
 * @target_property: Name of property to use for importer
 * @directory: Source directory:
 * @error: A #GError
 *
 * This function sets a property named @target_property on the object
 * resulting from the evaluation of @target_object_script code, which
 * acts as a GJS importer for directory @directory.
 *
 * Returns: %TRUE on success
 */
gboolean
shell_global_add_extension_importer (ShellGlobal *global,
                                     const char  *target_object_script,
                                     const char  *target_property,
                                     const char  *directory,
                                     GError     **error)
{
  jsval target_object;
  JSContext *context = gjs_context_get_native_context (global->js_context);
  char *search_path[2] = { 0, 0 };

  JS_BeginRequest (context);

  // This is a bit of a hack; ideally we'd be able to pass our target
  // object directly into this function, but introspection doesn't
  // support that at the moment.  Instead evaluate a string to get it.
  if (!JS_EvaluateScript(context,
                         JS_GetGlobalObject(context),
                         target_object_script,
                         strlen (target_object_script),
                         "<target_object_script>",
                         0,
                         &target_object))
    {
      char *message;
      gjs_log_exception(context,
                        &message);
      g_set_error(error,
                  G_IO_ERROR,
                  G_IO_ERROR_FAILED,
                  "%s", message ? message : "(unknown)");
      g_free(message);
      goto out_error;
    }

  if (!JSVAL_IS_OBJECT (target_object))
    {
      g_error ("shell_global_add_extension_importer: invalid target object");
      goto out_error;
    }

  search_path[0] = (char*)directory;
  gjs_define_importer (context, JSVAL_TO_OBJECT (target_object), target_property, (const char **)search_path, FALSE);
  JS_EndRequest (context);
  return TRUE;
 out_error:
  JS_EndRequest (context);
  return FALSE;
}

/* Code to close all file descriptors before we exec; copied from gspawn.c in GLib.
 *
 * Authors: Padraig O'Briain, Matthias Clasen, Lennart Poettering
 *
 * http://bugzilla.gnome.org/show_bug.cgi?id=469231
 * http://bugzilla.gnome.org/show_bug.cgi?id=357585
 */

static int
set_cloexec (void *data, gint fd)
{
  if (fd >= GPOINTER_TO_INT (data))
    fcntl (fd, F_SETFD, FD_CLOEXEC);

  return 0;
}

#ifndef HAVE_FDWALK
static int
fdwalk (int (*cb)(void *data, int fd), void *data)
{
  gint open_max;
  gint fd;
  gint res = 0;

#ifdef HAVE_SYS_RESOURCE_H
  struct rlimit rl;
#endif

#ifdef __linux__
  DIR *d;

  if ((d = opendir("/proc/self/fd"))) {
      struct dirent *de;

      while ((de = readdir(d))) {
          glong l;
          gchar *e = NULL;

          if (de->d_name[0] == '.')
              continue;

          errno = 0;
          l = strtol(de->d_name, &e, 10);
          if (errno != 0 || !e || *e)
              continue;

          fd = (gint) l;

          if ((glong) fd != l)
              continue;

          if (fd == dirfd(d))
              continue;

          if ((res = cb (data, fd)) != 0)
              break;
        }

      closedir(d);
      return res;
  }

  /* If /proc is not mounted or not accessible we fall back to the old
   * rlimit trick */

#endif

#ifdef HAVE_SYS_RESOURCE_H
  if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_max != RLIM_INFINITY)
      open_max = rl.rlim_max;
  else
#endif
      open_max = sysconf (_SC_OPEN_MAX);

  for (fd = 0; fd < open_max; fd++)
      if ((res = cb (data, fd)) != 0)
          break;

  return res;
}
#endif

static void
pre_exec_close_fds(void)
{
  fdwalk (set_cloexec, GINT_TO_POINTER(3));
}

/**
 * shell_global_reexec_self:
 * @global: A #ShellGlobal
 * 
 * Restart the current process.  Only intended for development purposes. 
 */
void 
shell_global_reexec_self (ShellGlobal *global)
{
  GPtrArray *arr;
  gsize len;
  char *buf;
  char *buf_p;
  char *buf_end;
  GError *error = NULL;
  
  /* Linux specific (I think, anyways). */
  if (!g_file_get_contents ("/proc/self/cmdline", &buf, &len, &error))
    {
      g_warning ("failed to get /proc/self/cmdline: %s", error->message);
      return;
    }
      
  buf_end = buf+len;
  arr = g_ptr_array_new ();
  /* The cmdline file is NUL-separated */
  for (buf_p = buf; buf_p < buf_end; buf_p = buf_p + strlen (buf_p) + 1)
    g_ptr_array_add (arr, buf_p);
  
  g_ptr_array_add (arr, NULL);

  /* Close all file descriptors other than stdin/stdout/stderr, otherwise
   * they will leak and stay open after the exec. In particular, this is
   * important for file descriptors that represent mapped graphics buffer
   * objects.
   */
  pre_exec_close_fds ();

  execvp (arr->pdata[0], (char**)arr->pdata);
  g_warning ("failed to reexec: %s", g_strerror (errno));
  g_ptr_array_free (arr, TRUE);
}

/**
 * shell_global_gc:
 * @global: A #ShellGlobal
 *
 * Start a garbage collection process.  For more information, see
 * https://developer.mozilla.org/En/JS_GC
 */
void
shell_global_gc (ShellGlobal *global)
{
  JSContext *context = gjs_context_get_native_context (global->js_context);

  JS_GC (context);
}

/**
 * shell_global_maybe_gc:
 * @global: A #ShellGlobal
 *
 * Start a garbage collection process when it would free up enough memory
 * to be worth the amount of time it would take
 * https://developer.mozilla.org/en/SpiderMonkey/JSAPI_Reference/JS_MaybeGC
 */
void
shell_global_maybe_gc (ShellGlobal *global)
{
  gjs_context_maybe_gc (global->js_context);
}

/**
 * shell_global_get_memory_info:
 * @global:
 * @meminfo: (out caller-allocates): Output location for memory information
 *
 * Load process-global data about memory usage.
 */
void
shell_global_get_memory_info (ShellGlobal        *global,
                              ShellMemoryInfo    *meminfo)
{
  JSContext *context;

  memset (meminfo, 0, sizeof (meminfo));
#ifdef HAVE_MALLINFO
  {
    struct mallinfo info = mallinfo ();
    meminfo->glibc_uordblks = info.uordblks;
  }
#endif

  context = gjs_context_get_native_context (global->js_context);

  meminfo->js_bytes = JS_GetGCParameter (JS_GetRuntime (context), JSGC_BYTES);

  meminfo->gjs_boxed = (unsigned int) gjs_counter_boxed.value;
  meminfo->gjs_gobject = (unsigned int) gjs_counter_object.value;
  meminfo->gjs_function = (unsigned int) gjs_counter_function.value;
  meminfo->gjs_closure = (unsigned int) gjs_counter_closure.value;
}


/**
 * shell_global_notify_error:
 * @global: a #ShellGlobal
 * @msg: Error message
 * @details: Error details
 *
 * Show a system error notification.  Use this function
 * when a user-initiated action results in a non-fatal problem
 * from causes that may not be under system control.  For
 * example, an application crash.
 */
void
shell_global_notify_error (ShellGlobal  *global,
                           const char   *msg,
                           const char   *details)
{
  g_signal_emit_by_name (global, "notify-error", msg, details);
}

static void
grab_notify (GtkWidget *widget, gboolean was_grabbed, gpointer user_data)
{
  ShellGlobal *global = SHELL_GLOBAL (user_data);
  
  global->gtk_grab_active = !was_grabbed;

  /* Update for the new setting of gtk_grab_active */
  shell_global_set_stage_input_mode (global, global->input_mode);
}

/**
 * shell_global_init_xdnd:
 * @global: the #ShellGlobal
 *
 * Enables tracking of Xdnd events
 */
void shell_global_init_xdnd (ShellGlobal *global)
{
  long xdnd_version = 5;

  MetaScreen *screen = shell_global_get_screen (global);
  Window output_window = meta_get_overlay_window (screen);

  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);

  ClutterStage *stage = CLUTTER_STAGE(meta_plugin_get_stage (global->plugin));
  Window stage_win = clutter_x11_get_stage_window (stage);

  XChangeProperty (xdisplay, stage_win, gdk_x11_get_xatom_by_name ("XdndAware"), XA_ATOM,
                  32, PropModeReplace, (const unsigned char *)&xdnd_version, 1);

  XChangeProperty (xdisplay, output_window, gdk_x11_get_xatom_by_name ("XdndProxy"), XA_WINDOW,
                  32, PropModeReplace, (const unsigned char *)&stage_win, 1);

  /*
   * XdndProxy is additionally set on the proxy window as verification that the
   * XdndProxy property on the target window isn't a left-over
   */
  XChangeProperty (xdisplay, stage_win, gdk_x11_get_xatom_by_name ("XdndProxy"), XA_WINDOW,
                  32, PropModeReplace, (const unsigned char *)&stage_win, 1);
}

/**
 * shell_global_get_monitors:
 * @global: the #ShellGlobal
 *
 * Gets a list of the bounding boxes of the active screen's monitors.
 *
 * Return value: (transfer full) (element-type Meta.Rectangle): a list
 * of monitor bounding boxes.
 */
GSList *
shell_global_get_monitors (ShellGlobal *global)
{
  MetaScreen *screen = shell_global_get_screen (global);
  GSList *monitors = NULL;
  MetaRectangle rect;
  int i;

  for (i = meta_screen_get_n_monitors (screen) - 1; i >= 0; i--)
    {
      meta_screen_get_monitor_geometry (screen, i, &rect);
      monitors = g_slist_prepend (monitors,
                                  meta_rectangle_copy (&rect));
    }
  return monitors;
}

/**
 * shell_global_get_primary_monitor:
 * @global: the #ShellGlobal
 *
 * Gets the bounding box of the primary monitor (the one that the
 * panel is on).
 *
 * Return value: the bounding box of the primary monitor
 */
MetaRectangle *
shell_global_get_primary_monitor (ShellGlobal  *global)
{
  MetaScreen *screen = shell_global_get_screen (global);
  MetaRectangle rect;
  gint primary = 0;

  primary = meta_screen_get_primary_monitor (screen);
  meta_screen_get_monitor_geometry (screen, primary, &rect);

  return meta_rectangle_copy (&rect);
}

/**
 * shell_global_get_primary_monitor_index:
 * @global: the #ShellGlobal
 *
 * Gets the index of the primary monitor (the one that the
 * panel is on).
 *
 * Return value: the index of the primary monitor
 */
int
shell_global_get_primary_monitor_index (ShellGlobal  *global)
{
  MetaScreen *screen = shell_global_get_screen (global);

  return meta_screen_get_primary_monitor (screen);
}

/**
 * shell_global_get_focus_monitor:
 * @global: the #ShellGlobal
 *
 * Gets the bounding box of the monitor containing the window that
 * currently contains the keyboard focus.
 *
 * Return value: the bounding box of the focus monitor
 */
MetaRectangle *
shell_global_get_focus_monitor (ShellGlobal  *global)
{
  MetaScreen *screen = shell_global_get_screen (global);
  MetaDisplay *display = meta_screen_get_display (screen);
  MetaWindow *focus = meta_display_get_focus_window (display);
  MetaRectangle rect, wrect;
  int nmonitors, i;

  if (focus)
    {
      meta_window_get_outer_rect (focus, &wrect);
      nmonitors = meta_screen_get_n_monitors (screen);

      /* Find the monitor that the top-left corner of @focus is on. */
      for (i = 0; i < nmonitors; i++)
        {
          meta_screen_get_monitor_geometry (screen, i, &rect);

          if (rect.x <= wrect.x && rect.y <= wrect.y &&
              rect.x + rect.width > wrect.x &&
              rect.y + rect.height > wrect.y)
            return meta_rectangle_copy (&rect);
        }
    }

  return shell_global_get_primary_monitor (global);
}

/**
 * shell_global_get_pointer:
 * @global: the #ShellGlobal
 * @x: (out): the X coordinate of the pointer, in global coordinates
 * @y: (out): the Y coordinate of the pointer, in global coordinates
 * @mods: (out): the current set of modifier keys that are pressed down
 *
 * Gets the pointer coordinates and current modifier key state.
 * This is a wrapper around gdk_display_get_pointer() that strips
 * out any un-declared modifier flags, to make gjs happy; see
 * https://bugzilla.gnome.org/show_bug.cgi?id=597292.
 */
void
shell_global_get_pointer (ShellGlobal         *global,
                          int                 *x,
                          int                 *y,
                          ClutterModifierType *mods)
{
  GdkModifierType raw_mods;

  gdk_display_get_pointer (gdk_display_get_default (), NULL, x, y, &raw_mods);
  *mods = raw_mods & GDK_MODIFIER_MASK;
}

/**
 * shell_global_sync_pointer:
 * @global: the #ShellGlobal
 *
 * Ensures that clutter is aware of the current pointer position,
 * causing enter and leave events to be emitted if the pointer moved
 * behind our back (ie, during a pointer grab).
 */
void
shell_global_sync_pointer (ShellGlobal *global)
{
  int x, y;
  GdkModifierType mods;
  ClutterMotionEvent event;

  gdk_display_get_pointer (gdk_display_get_default (), NULL, &x, &y, &mods);

  event.type = CLUTTER_MOTION;
  event.time = shell_global_get_current_time (global);
  event.flags = 0;
  /* This is wrong: we should be setting event.stage to NULL if the
   * pointer is not inside the bounds of the stage given the current
   * stage_input_mode. For our current purposes however, this works.
   */
  event.stage = CLUTTER_STAGE (meta_plugin_get_stage (global->plugin));
  event.x = x;
  event.y = y;
  event.modifier_state = mods;
  event.axes = NULL;
  event.device = clutter_device_manager_get_core_device (clutter_device_manager_get_default (),
                                                         CLUTTER_POINTER_DEVICE);

  /* Leaving event.source NULL will force clutter to look it up, which
   * will generate enter/leave events as a side effect, if they are
   * needed. We need a better way to do this though... see
   * http://bugzilla.clutter-project.org/show_bug.cgi?id=2615.
   */
  event.source = NULL;

  clutter_event_put ((ClutterEvent *)&event);
}

/**
 * shell_global_get_settings:
 * @global: A #ShellGlobal
 *
 * Get the global GSettings instance.
 *
 * Return value: (transfer none): The GSettings object
 */
GSettings *
shell_global_get_settings (ShellGlobal *global)
{
  return global->settings;
}

/**
 * shell_global_get_current_time:
 * @global: A #ShellGlobal
 *
 * Returns: the current X server time from the current Clutter, Gdk, or X
 * event. If called from outside an event handler, this may return
 * %Clutter.CURRENT_TIME (aka 0), or it may return a slightly
 * out-of-date timestamp.
 */
guint32
shell_global_get_current_time (ShellGlobal *global)
{
  guint32 time;
  MetaDisplay *display;
  const ClutterEvent *clutter_event;

  /* In case we have a xdnd timestamp use it */
  if (global->xdnd_timestamp != 0)
    return global->xdnd_timestamp;

  /* meta_display_get_current_time() will return the correct time
     when handling an X or Gdk event, but will return CurrentTime
     from some Clutter event callbacks.

     clutter_get_current_event_time() will return the correct time
     from a Clutter event callback, but may return an out-of-date
     timestamp if called at other times.

     So we try meta_display_get_current_time() first, since we
     can recognize a "wrong" answer from that, and then fall back
     to clutter_get_current_event_time().
   */

  display = meta_screen_get_display (shell_global_get_screen (global));
  time = meta_display_get_current_time (display);
  if (time != CLUTTER_CURRENT_TIME)
      return time;
  /*
   * We don't use clutter_get_current_event_time as it can give us a
   * too old timestamp if there is no current event.
   */
  clutter_event = clutter_get_current_event ();

  if (clutter_event != NULL)
    return clutter_event_get_time (clutter_event);
  else
    return CLUTTER_CURRENT_TIME;
}

/**
 * shell_global_create_app_launch_context:
 * @global: A #ShellGlobal
 *
 * Create a #GAppLaunchContext set up with the correct timestamp, and
 * targeted to activate on the current workspace.
 *
 * Return value: (transfer full): A new #GAppLaunchContext
 */
GAppLaunchContext *
shell_global_create_app_launch_context (ShellGlobal *global)
{
  GdkAppLaunchContext *context;

  context = gdk_app_launch_context_new ();
  gdk_app_launch_context_set_timestamp (context, shell_global_get_current_time (global));

  // Make sure that the app is opened on the current workspace even if
  // the user switches before it starts
  gdk_app_launch_context_set_desktop (context, meta_screen_get_active_workspace_index (shell_global_get_screen (global)));

  return (GAppLaunchContext *)context;
}

typedef struct
{
  ShellLeisureFunction func;
  gpointer user_data;
  GDestroyNotify notify;
} LeisureClosure;

static gboolean
run_leisure_functions (gpointer data)
{
  ShellGlobal *global = data;
  GSList *closures;
  GSList *iter;

  global->leisure_function_id = 0;

  /* We started more work since we scheduled the idle */
  if (global->work_count > 0)
    return FALSE;

  /*
   * We do call MAYBE_GC() here to free up some memory and
   * prevent the GC from running when we are busy doing other things.
   */
  shell_global_maybe_gc (global);

  /* No leisure closures, so we are done */
  if (global->leisure_closures == NULL)
    return FALSE;

  closures = global->leisure_closures;
  global->leisure_closures = NULL;

  for (iter = closures; iter; iter = iter->next)
    {
      LeisureClosure *closure = closures->data;
      closure->func (closure->user_data);

      if (closure->notify)
        closure->notify (closure->user_data);

      g_slice_free (LeisureClosure, closure);
    }

  g_slist_free (closures);

  return FALSE;
}

static void
schedule_leisure_functions (ShellGlobal *global)
{
  /* This is called when we think we are ready to run leisure functions
   * by our own accounting. We try to handle other types of business
   * (like ClutterAnimation) by adding a low priority idle function.
   *
   * This won't work properly if the mainloop goes idle waiting for
   * the vertical blanking interval or waiting for work being done
   * in another thread.
   */
  if (!global->leisure_function_id)
    global->leisure_function_id = g_idle_add_full (G_PRIORITY_LOW,
                                                   run_leisure_functions,
                                                   global, NULL);
}

/**
 * shell_global_begin_work:
 * @global: the #ShellGlobal
 *
 * Marks that we are currently doing work. This is used to to track
 * whether we are busy for the purposes of shell_global_run_at_leisure().
 * A count is kept and shell_global_end_work() must be called exactly
 * as many times as shell_global_begin_work().
 */
void
shell_global_begin_work (ShellGlobal *global)
{
  global->work_count++;
}

/**
 * shell_global_end_work:
 * @global: the #ShellGlobal
 *
 * Marks the end of work that we started with shell_global_begin_work().
 * If no other work is ongoing and functions have been added with
 * shell_global_run_at_leisure(), they will be run at the next
 * opportunity.
 */
void
shell_global_end_work (ShellGlobal *global)
{
  g_return_if_fail (global->work_count > 0);

  global->work_count--;
  if (global->work_count == 0)
    schedule_leisure_functions (global);

}

/**
 * shell_global_run_at_leisure:
 * @global: the #ShellGlobal
 * @func: function to call at leisure
 * @user_data: data to pass to @func
 * @notify: function to call to free @user_data
 *
 * Schedules a function to be called the next time the shell is idle.
 * Idle means here no animations, no redrawing, and no ongoing background
 * work. Since there is currently no way to hook into the Clutter master
 * clock and know when is running, the implementation here is somewhat
 * approximation. Animations done through the shell's Tweener module will
 * be handled properly, but other animations may be detected as terminating
 * early if they can be drawn fast enough so that the event loop goes idle
 * between frames.
 *
 * The intent of this function is for performance measurement runs
 * where a number of actions should be run serially and each action is
 * timed individually. Using this function for other purposes will
 * interfere with the ability to use it for performance measurement so
 * should be avoided.
 */
void
shell_global_run_at_leisure (ShellGlobal         *global,
                             ShellLeisureFunction func,
                             gpointer             user_data,
                             GDestroyNotify       notify)
{
  LeisureClosure *closure = g_slice_new (LeisureClosure);
  closure->func = func;
  closure->user_data = user_data;
  closure->notify = notify;

  global->leisure_closures = g_slist_append (global->leisure_closures,
                                             closure);

  if (global->work_count == 0)
    schedule_leisure_functions (global);
}

/**
 * shell_global_play_theme_sound:
 * @global: the #ShellGlobal
 * @id: an id, used to cancel later (0 if not needed)
 * @name: the sound name
 *
 * Plays a simple sound picked according to Freedesktop sound theme.
 * Really just a workaround for libcanberra not being introspected.
 */
void
shell_global_play_theme_sound (ShellGlobal *global,
                               guint        id,
                               const char  *name)
{
  ca_context_play (global->sound_context, id, CA_PROP_EVENT_ID, name, NULL);
}

/**
 * shell_global_cancel_theme_sound:
 * @global: the #ShellGlobal
 * @id: the id previously passed to shell_global_play_theme_sound()
 *
 * Cancels a sound notification.
 */
void
shell_global_cancel_theme_sound (ShellGlobal *global,
                                 guint id)
{
  ca_context_cancel (global->sound_context, id);
}

/*
 * Process Xdnd events
 *
 * We pass the position and leave events to JS via a signal
 * where the actual drag & drop handling happens.
 *
 * http://www.freedesktop.org/wiki/Specifications/XDND
 */
gboolean _shell_global_check_xdnd_event (ShellGlobal  *global,
                                         XEvent       *xev)
{
  MetaScreen *screen = meta_plugin_get_screen (global->plugin);
  Window output_window = meta_get_overlay_window (screen);
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);

  ClutterStage *stage = CLUTTER_STAGE (meta_plugin_get_stage (global->plugin));
  Window stage_win = clutter_x11_get_stage_window (stage);

  if (xev->xany.window != output_window && xev->xany.window != stage_win)
    return FALSE;

  if (xev->xany.type == ClientMessage && xev->xclient.message_type == gdk_x11_get_xatom_by_name ("XdndPosition"))
    {
      XEvent xevent;
      Window src = xev->xclient.data.l[0];

      memset (&xevent, 0, sizeof(xevent));
      xevent.xany.type = ClientMessage;
      xevent.xany.display = xdisplay;
      xevent.xclient.window = src;
      xevent.xclient.message_type = gdk_x11_get_xatom_by_name ("XdndStatus");
      xevent.xclient.format = 32;
      xevent.xclient.data.l[0] = output_window;
      /* flags: bit 0: will we accept the drop? bit 1: do we want more position messages */
      xevent.xclient.data.l[1] = 2;
      xevent.xclient.data.l[4] = None;

      XSendEvent (xdisplay, src, False, 0, &xevent);

      /* Store the timestamp of the xdnd position event */
      global->xdnd_timestamp = xev->xclient.data.l[3];
      g_signal_emit_by_name (G_OBJECT (global), "xdnd-position-changed",
                            (int)(xev->xclient.data.l[2] >> 16), (int)(xev->xclient.data.l[2] & 0xFFFF));
      global->xdnd_timestamp = 0;

      return TRUE;
    }
   else if (xev->xany.type == ClientMessage && xev->xclient.message_type == gdk_x11_get_xatom_by_name ("XdndLeave"))
    {
      g_signal_emit_by_name (G_OBJECT (global), "xdnd-leave");

      return TRUE;
    }
   else if (xev->xany.type == ClientMessage && xev->xclient.message_type == gdk_x11_get_xatom_by_name ("XdndEnter"))
    {
      g_signal_emit_by_name (G_OBJECT (global), "xdnd-enter");

      return TRUE;
    }

    return FALSE;
}

/**
 * shell_global_launch_calendar_server:
 * @global: The #ShellGlobal.
 *
 * Launch the gnome-shell-calendar-server helper.
 */
void
shell_global_launch_calendar_server (ShellGlobal *global)
{
  const gchar *bin_dir;
  gchar *calendar_server_exe;
  GError *error;
  gchar *argv[2];
  gint child_standard_input;

  /* launch calendar-server */
  bin_dir = g_getenv ("GNOME_SHELL_BINDIR");
  if (bin_dir != NULL)
    calendar_server_exe = g_strdup_printf ("%s/gnome-shell-calendar-server", bin_dir);
  else
    calendar_server_exe = g_strdup_printf (GNOME_SHELL_LIBEXECDIR "/gnome-shell-calendar-server");

  argv[0] = calendar_server_exe;
  argv[1] = NULL;
  error = NULL;
  if (!g_spawn_async_with_pipes (NULL, /* working_directory */
                                 argv,
                                 NULL, /* envp */
                                 0, /* GSpawnFlags */
                                 NULL, /* child_setup */
                                 NULL, /* user_data */
                                 NULL, /* GPid *child_pid */
                                 &child_standard_input,
                                 NULL, /* gint *stdout */
                                 NULL, /* gint *stderr */
                                 &error))
    {
      g_warning ("Error launching `%s': %s (%s %d)",
                 calendar_server_exe,
                 error->message,
                 g_quark_to_string (error->domain),
                 error->code);
      g_error_free (error);
    }
  /* Note that gnome-shell-calendar-server exits whenever its stdin
   * file descriptor is HUP'ed. This means that whenever the the shell
   * process exits or is being replaced, the calendar server is also
   * exits...and if the shell is being replaced, a new copy of the
   * calendar server is launched...
   */

  g_free (calendar_server_exe);
}
