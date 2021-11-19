/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include <locale.h>

#include <X11/extensions/Xfixes.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <girepository.h>
#include <meta/meta-backend.h>
#include <meta/meta-context.h>
#include <meta/display.h>
#include <meta/util.h>
#include <meta/meta-shaped-texture.h>
#include <meta/meta-cursor-tracker.h>
#include <meta/meta-settings.h>
#include <meta/meta-workspace-manager.h>
#include <meta/meta-x11-display.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-systemd.h>

#if defined __OpenBSD__ || defined __FreeBSD__
#include <sys/sysctl.h>
#endif

#include "shell-enum-types.h"
#include "shell-global-private.h"
#include "shell-perf-log.h"
#include "shell-window-tracker.h"
#include "shell-wm.h"
#include "shell-util.h"
#include "st.h"
#include "switcheroo-control.h"

static ShellGlobal *the_object = NULL;

struct _ShellGlobal {
  GObject parent;

  ClutterStage *stage;

  MetaBackend *backend;
  MetaContext *meta_context;
  MetaDisplay *meta_display;
  MetaWorkspaceManager *workspace_manager;
  Display *xdisplay;

  char *session_mode;

  XserverRegion input_region;

  GjsContext *js_context;
  MetaPlugin *plugin;
  ShellWM *wm;
  GSettings *settings;
  const char *datadir;
  char *imagedir;
  char *userdatadir;
  GFile *userdatadir_path;
  GFile *runtime_state_path;

  StFocusManager *focus_manager;

  guint work_count;
  GSList *leisure_closures;
  guint leisure_function_id;

  GHashTable *save_ops;

  gboolean frame_timestamps;
  gboolean frame_finish_timestamp;

  GDBusProxy *switcheroo_control;
  GCancellable *switcheroo_cancellable;
};

enum {
  PROP_0,

  PROP_SESSION_MODE,
  PROP_BACKEND,
  PROP_CONTEXT,
  PROP_DISPLAY,
  PROP_WORKSPACE_MANAGER,
  PROP_SCREEN_WIDTH,
  PROP_SCREEN_HEIGHT,
  PROP_STAGE,
  PROP_WINDOW_GROUP,
  PROP_TOP_WINDOW_GROUP,
  PROP_WINDOW_MANAGER,
  PROP_SETTINGS,
  PROP_DATADIR,
  PROP_IMAGEDIR,
  PROP_USERDATADIR,
  PROP_FOCUS_MANAGER,
  PROP_FRAME_TIMESTAMPS,
  PROP_FRAME_FINISH_TIMESTAMP,
  PROP_SWITCHEROO_CONTROL,
};

/* Signals */
enum
{
 NOTIFY_ERROR,
 LOCATE_POINTER,
 LAST_SIGNAL
};

G_DEFINE_TYPE(ShellGlobal, shell_global, G_TYPE_OBJECT);

static guint shell_global_signals [LAST_SIGNAL] = { 0 };

static void
got_switcheroo_control_gpus_property_cb (GObject      *source_object,
                                         GAsyncResult *res,
                                         gpointer      user_data)
{
  ShellGlobal *global;
  GError *error = NULL;
  GVariant *gpus;

  gpus = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                        res, &error);
  if (!gpus)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("Could not get GPUs property from switcheroo-control: %s", error->message);
      g_clear_error (&error);
      return;
    }

  global = user_data;
  g_dbus_proxy_set_cached_property (global->switcheroo_control, "GPUs", gpus);
  g_object_notify (G_OBJECT (global), "switcheroo-control");
}

static void
switcheroo_control_ready_cb (GObject      *source_object,
                             GAsyncResult *res,
                             gpointer      user_data)
{
  ShellGlobal *global;
  GError *error = NULL;
  ShellNetHadessSwitcherooControl *control;
  g_auto(GStrv) cached_props = NULL;

  control = shell_net_hadess_switcheroo_control_proxy_new_for_bus_finish (res, &error);
  if (!control)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("Could not get switcheroo-control GDBusProxy: %s", error->message);
      g_clear_error (&error);
      return;
    }

  global = user_data;
  global->switcheroo_control = G_DBUS_PROXY (control);
  g_debug ("Got switcheroo-control proxy successfully");

  cached_props = g_dbus_proxy_get_cached_property_names (global->switcheroo_control);
  if (cached_props != NULL && g_strv_contains ((const gchar * const *) cached_props, "GPUs"))
    {
      g_object_notify (G_OBJECT (global), "switcheroo-control");
      return;
    }
  /* Delay property notification until we have all the properties gathered */

  g_dbus_connection_call (g_dbus_proxy_get_connection (global->switcheroo_control),
                          g_dbus_proxy_get_name (global->switcheroo_control),
                          g_dbus_proxy_get_object_path (global->switcheroo_control),
                          "org.freedesktop.DBus.Properties",
                          "Get",
                          g_variant_new ("(ss)",
                                         g_dbus_proxy_get_interface_name (global->switcheroo_control),
                                         "GPUs"),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          global->switcheroo_cancellable,
                          got_switcheroo_control_gpus_property_cb,
                          user_data);
}

static void
shell_global_set_property(GObject         *object,
                          guint            prop_id,
                          const GValue    *value,
                          GParamSpec      *pspec)
{
  ShellGlobal *global = SHELL_GLOBAL (object);

  switch (prop_id)
    {
    case PROP_SESSION_MODE:
      g_clear_pointer (&global->session_mode, g_free);
      global->session_mode = g_ascii_strdown (g_value_get_string (value), -1);
      break;
    case PROP_FRAME_TIMESTAMPS:
      global->frame_timestamps = g_value_get_boolean (value);
      break;
    case PROP_FRAME_FINISH_TIMESTAMP:
      global->frame_finish_timestamp = g_value_get_boolean (value);
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
    case PROP_SESSION_MODE:
      g_value_set_string (value, shell_global_get_session_mode (global));
      break;
    case PROP_BACKEND:
      g_value_set_object (value, global->backend);
      break;
    case PROP_CONTEXT:
      g_value_set_object (value, global->meta_context);
      break;
    case PROP_DISPLAY:
      g_value_set_object (value, global->meta_display);
      break;
    case PROP_WORKSPACE_MANAGER:
      g_value_set_object (value, global->workspace_manager);
      break;
    case PROP_SCREEN_WIDTH:
      {
        int width, height;

        meta_display_get_size (global->meta_display, &width, &height);
        g_value_set_int (value, width);
      }
      break;
    case PROP_SCREEN_HEIGHT:
      {
        int width, height;

        meta_display_get_size (global->meta_display, &width, &height);
        g_value_set_int (value, height);
      }
      break;
    case PROP_STAGE:
      g_value_set_object (value, global->stage);
      break;
    case PROP_WINDOW_GROUP:
      g_value_set_object (value, meta_get_window_group_for_display (global->meta_display));
      break;
    case PROP_TOP_WINDOW_GROUP:
      g_value_set_object (value, meta_get_top_window_group_for_display (global->meta_display));
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
    case PROP_FRAME_TIMESTAMPS:
      g_value_set_boolean (value, global->frame_timestamps);
      break;
    case PROP_FRAME_FINISH_TIMESTAMP:
      g_value_set_boolean (value, global->frame_finish_timestamp);
      break;
    case PROP_SWITCHEROO_CONTROL:
      g_value_set_object (value, global->switcheroo_control);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
switcheroo_appeared_cb (GDBusConnection *connection,
                        const char     *name,
                        const char     *name_owner,
                        gpointer        user_data)
{
  ShellGlobal *global = user_data;

  g_debug ("switcheroo-control appeared");
  shell_net_hadess_switcheroo_control_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                                         G_DBUS_PROXY_FLAGS_NONE,
                                                         "net.hadess.SwitcherooControl",
                                                         "/net/hadess/SwitcherooControl",
                                                         global->switcheroo_cancellable,
                                                         switcheroo_control_ready_cb,
                                                         global);
}

static void
switcheroo_vanished_cb (GDBusConnection *connection,
                        const char      *name,
                        gpointer         user_data)
{
  ShellGlobal *global = user_data;

  g_debug ("switcheroo-control vanished");
  g_clear_object (&global->switcheroo_control);
  g_object_notify (G_OBJECT (global), "switcheroo-control");
}

static void
shell_global_init (ShellGlobal *global)
{
  const char *datadir = g_getenv ("GNOME_SHELL_DATADIR");
  const char *shell_js = g_getenv("GNOME_SHELL_JS");
  char *imagedir, **search_path;
  char *path;
  const char *byteorder_string;

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
  global->userdatadir_path = g_file_new_for_path (global->userdatadir);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  byteorder_string = "LE";
#else
  byteorder_string = "BE";
#endif

  /* And the runtime state */
  path = g_strdup_printf ("%s/gnome-shell/runtime-state-%s.%s",
                          g_get_user_runtime_dir (),
                          byteorder_string,
                          XDisplayName (NULL));
  (void) g_mkdir_with_parents (path, 0700);
  global->runtime_state_path = g_file_new_for_path (path);
  g_free (path);

  global->settings = g_settings_new ("org.gnome.shell");

  if (shell_js)
    {
      int i, j;
      search_path = g_strsplit (shell_js, ":", -1);

      /* The naive g_strsplit above will split 'resource:///foo/bar' into 'resource',
       * '///foo/bar'. Combine these back together by looking for a literal 'resource'
       * in the array. */
      for (i = 0, j = 0; search_path[i];)
        {
          char *out;

          if (strcmp (search_path[i], "resource") == 0 && search_path[i + 1] != NULL)
            {
              out = g_strconcat (search_path[i], ":", search_path[i + 1], NULL);
              g_free (search_path[i]);
              g_free (search_path[i + 1]);
              i += 2;
            }
          else
            {
              out = search_path[i];
              i += 1;
            }

          search_path[j++] = out;
        }

      search_path[j] = NULL; /* NULL-terminate the now possibly shorter array */
    }
  else
    {
      search_path = g_malloc0 (2 * sizeof (char *));
      search_path[0] = g_strdup ("resource:///org/gnome/shell");
    }

  global->js_context = g_object_new (GJS_TYPE_CONTEXT,
                                     "search-path", search_path,
                                     NULL);

  g_strfreev (search_path);

  global->save_ops = g_hash_table_new_full (g_file_hash,
                                            (GEqualFunc) g_file_equal,
                                            g_object_unref, g_object_unref);

  global->switcheroo_cancellable = g_cancellable_new ();
  g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                    "net.hadess.SwitcherooControl",
                    G_BUS_NAME_WATCHER_FLAGS_NONE,
                    switcheroo_appeared_cb,
                    switcheroo_vanished_cb,
                    global,
                    NULL);
}

static void
shell_global_finalize (GObject *object)
{
  ShellGlobal *global = SHELL_GLOBAL (object);

  g_clear_object (&global->js_context);
  g_object_unref (global->settings);

  the_object = NULL;

  g_cancellable_cancel (global->switcheroo_cancellable);
  g_clear_object (&global->switcheroo_cancellable);

  g_clear_object (&global->userdatadir_path);
  g_clear_object (&global->runtime_state_path);

  g_free (global->session_mode);
  g_free (global->imagedir);
  g_free (global->userdatadir);

  g_hash_table_unref (global->save_ops);

  G_OBJECT_CLASS(shell_global_parent_class)->finalize (object);
}

static void
shell_global_class_init (ShellGlobalClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shell_global_get_property;
  gobject_class->set_property = shell_global_set_property;
  gobject_class->finalize = shell_global_finalize;

  shell_global_signals[NOTIFY_ERROR] =
      g_signal_new ("notify-error",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL, NULL,
                    G_TYPE_NONE, 2,
                    G_TYPE_STRING,
                    G_TYPE_STRING);
  shell_global_signals[LOCATE_POINTER] =
      g_signal_new ("locate-pointer",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL, NULL,
                    G_TYPE_NONE, 0);

  g_object_class_install_property (gobject_class,
                                   PROP_SESSION_MODE,
                                   g_param_spec_string ("session-mode",
                                                        "Session Mode",
                                                        "The session mode to use",
                                                        "user",
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SCREEN_WIDTH,
                                   g_param_spec_int ("screen-width",
                                                     "Screen Width",
                                                     "Screen width, in pixels",
                                                     0, G_MAXINT, 1,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SCREEN_HEIGHT,
                                   g_param_spec_int ("screen-height",
                                                     "Screen Height",
                                                     "Screen height, in pixels",
                                                     0, G_MAXINT, 1,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_BACKEND,
                                   g_param_spec_object ("backend",
                                                        "Backend",
                                                        "MetaBackend object",
                                                        META_TYPE_BACKEND,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_CONTEXT,
                                   g_param_spec_object ("context",
                                                        "Context",
                                                        "MetaContext object",
                                                        META_TYPE_CONTEXT,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_DISPLAY,
                                   g_param_spec_object ("display",
                                                        "Display",
                                                        "Metacity display object for the shell",
                                                        META_TYPE_DISPLAY,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_WORKSPACE_MANAGER,
                                   g_param_spec_object ("workspace-manager",
                                                        "Workspace manager",
                                                        "Workspace manager",
                                                        META_TYPE_WORKSPACE_MANAGER,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_STAGE,
                                   g_param_spec_object ("stage",
                                                        "Stage",
                                                        "Stage holding the desktop scene graph",
                                                        CLUTTER_TYPE_ACTOR,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_WINDOW_GROUP,
                                   g_param_spec_object ("window-group",
                                                        "Window Group",
                                                        "Actor holding window actors",
                                                        CLUTTER_TYPE_ACTOR,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                     PROP_TOP_WINDOW_GROUP,
                                     g_param_spec_object ("top-window-group",
                                                          "Top Window Group",
                                                          "Actor holding override-redirect windows",
                                                          CLUTTER_TYPE_ACTOR,
                                                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_WINDOW_MANAGER,
                                   g_param_spec_object ("window-manager",
                                                        "Window Manager",
                                                        "Window management interface",
                                                        SHELL_TYPE_WM,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_SETTINGS,
                                   g_param_spec_object ("settings",
                                                        "Settings",
                                                        "GSettings instance for gnome-shell configuration",
                                                        G_TYPE_SETTINGS,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_DATADIR,
                                   g_param_spec_string ("datadir",
                                                        "Data directory",
                                                        "Directory containing gnome-shell data files",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_IMAGEDIR,
                                   g_param_spec_string ("imagedir",
                                                        "Image directory",
                                                        "Directory containing gnome-shell image files",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_USERDATADIR,
                                   g_param_spec_string ("userdatadir",
                                                        "User data directory",
                                                        "Directory containing gnome-shell user data",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_FOCUS_MANAGER,
                                   g_param_spec_object ("focus-manager",
                                                        "Focus manager",
                                                        "The shell's StFocusManager",
                                                        ST_TYPE_FOCUS_MANAGER,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_FRAME_TIMESTAMPS,
                                   g_param_spec_boolean ("frame-timestamps",
                                                         "Frame Timestamps",
                                                         "Whether to log frame timestamps in the performance log",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_FRAME_FINISH_TIMESTAMP,
                                   g_param_spec_boolean ("frame-finish-timestamp",
                                                         "Frame Finish Timestamps",
                                                         "Whether at the end of a frame to call glFinish and log paintCompletedTimestamp",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_SWITCHEROO_CONTROL,
                                   g_param_spec_object ("switcheroo-control",
                                                        "switcheroo-control",
                                                        "D-Bus Proxy for switcheroo-control daemon",
                                                        G_TYPE_DBUS_PROXY,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

/*
 * _shell_global_init: (skip)
 * @first_property_name: the name of the first property
 * @...: the value of the first property, followed optionally by more
 *  name/value pairs, followed by %NULL
 *
 * Initializes the shell global singleton with the construction-time
 * properties.
 *
 * There are currently no such properties, so @first_property_name should
 * always be %NULL.
 *
 * This call must be called before shell_global_get() and shouldn't be called
 * more than once.
 */
void
_shell_global_init (const char *first_property_name,
                    ...)
{
  va_list argument_list;

  g_return_if_fail (the_object == NULL);

  va_start (argument_list, first_property_name);
  the_object = SHELL_GLOBAL (g_object_new_valist (SHELL_TYPE_GLOBAL,
                                                  first_property_name,
                                                  argument_list));
  va_end (argument_list);

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
  return the_object;
}

/**
 * _shell_global_destroy_gjs_context: (skip)
 * @self: global object
 *
 * Destroys the GjsContext held by ShellGlobal, in order to break reference
 * counting cycles. (The GjsContext holds a reference to ShellGlobal because
 * it's available as window.global inside JS.)
 */
void
_shell_global_destroy_gjs_context (ShellGlobal *self)
{
  g_clear_object (&self->js_context);
}

static guint32
get_current_time_maybe_roundtrip (ShellGlobal *global)
{
  guint32 time;

  time = shell_global_get_current_time (global);
  if (time != CurrentTime)
    return time;

  return meta_display_get_current_time_roundtrip (global->meta_display);
}

static void
focus_window_changed (MetaDisplay *display,
                      GParamSpec  *param,
                      gpointer     user_data)
{
  ShellGlobal *global = user_data;

  /* If the stage window became unfocused, drop the key focus
   * on Clutter's side. */
  if (!meta_stage_is_focused (global->meta_display))
    clutter_stage_set_key_focus (global->stage, NULL);
}

static ClutterActor *
get_key_focused_actor (ShellGlobal *global)
{
  ClutterActor *actor;

  actor = clutter_stage_get_key_focus (global->stage);

  /* If there's no explicit key focus, clutter_stage_get_key_focus()
   * returns the stage. This is a terrible API. */
  if (actor == CLUTTER_ACTOR (global->stage))
    actor = NULL;

  return actor;
}

static void
sync_stage_window_focus (ShellGlobal *global)
{
  ClutterActor *actor;

  actor = get_key_focused_actor (global);

  /* An actor got key focus and the stage needs to be focused. */
  if (actor != NULL && !meta_stage_is_focused (global->meta_display))
    meta_focus_stage_window (global->meta_display,
                             get_current_time_maybe_roundtrip (global));

  /* An actor dropped key focus. Focus the default window. */
  else if (actor == NULL && meta_stage_is_focused (global->meta_display))
    meta_display_focus_default_window (global->meta_display,
                                       get_current_time_maybe_roundtrip (global));
}

static void
focus_actor_changed (ClutterStage *stage,
                     GParamSpec   *param,
                     gpointer      user_data)
{
  ShellGlobal *global = user_data;
  sync_stage_window_focus (global);
}

static void
sync_input_region (ShellGlobal *global)
{
  MetaDisplay *display = global->meta_display;
  MetaX11Display *x11_display = meta_display_get_x11_display (display);

  meta_x11_display_set_stage_input_region (x11_display, global->input_region);
}

/**
 * shell_global_set_stage_input_region:
 * @global: the #ShellGlobal
 * @rectangles: (element-type Meta.Rectangle): a list of #MetaRectangle
 * describing the input region.
 *
 * Sets the area of the stage that is responsive to mouse clicks when
 * we don't have a modal or grab.
 */
void
shell_global_set_stage_input_region (ShellGlobal *global,
                                     GSList      *rectangles)
{
  MetaRectangle *rect;
  XRectangle *rects;
  int nrects, i;
  GSList *r;

  g_return_if_fail (SHELL_IS_GLOBAL (global));

  if (meta_is_wayland_compositor ())
    return;

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
    XFixesDestroyRegion (global->xdisplay, global->input_region);

  global->input_region = XFixesCreateRegion (global->xdisplay, rects, nrects);
  g_free (rects);

  sync_input_region (global);
}

/**
 * shell_global_get_stage:
 *
 * Return value: (transfer none): The default #ClutterStage
 */
ClutterStage *
shell_global_get_stage (ShellGlobal  *global)
{
  return global->stage;
}

/**
 * shell_global_get_display:
 *
 * Return value: (transfer none): The default #MetaDisplay
 */
MetaDisplay *
shell_global_get_display (ShellGlobal  *global)
{
  return global->meta_display;
}

/**
 * shell_global_get_window_actors:
 *
 * Gets the list of #MetaWindowActor for the plugin's screen
 *
 * Return value: (element-type Meta.WindowActor) (transfer container): the list of windows
 */
GList *
shell_global_get_window_actors (ShellGlobal *global)
{
  GList *filtered = NULL;
  GList *l;

  g_return_val_if_fail (SHELL_IS_GLOBAL (global), NULL);

  for (l = meta_get_window_actors (global->meta_display); l; l = l->next)
    if (!meta_window_actor_is_destroyed (l->data))
      filtered = g_list_prepend (filtered, l->data);

  return g_list_reverse (filtered);
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

static gboolean
global_stage_before_paint (gpointer data)
{
  ShellGlobal *global = SHELL_GLOBAL (data);

  if (global->frame_timestamps)
    shell_perf_log_event (shell_perf_log_get_default (),
                          "clutter.stagePaintStart");

  return TRUE;
}

static gboolean
load_gl_symbol (const char  *name,
                void       **func)
{
  *func = cogl_get_proc_address (name);
  if (!*func)
    {
      g_warning ("failed to resolve required GL symbol \"%s\"\n", name);
      return FALSE;
    }
  return TRUE;
}

static void
global_stage_after_paint (ClutterStage     *stage,
                          ClutterStageView *stage_view,
                          ShellGlobal      *global)
{
  /* At this point, we've finished all layout and painting, but haven't
   * actually flushed or swapped */

  if (global->frame_timestamps && global->frame_finish_timestamp)
    {
      /* It's interesting to find out when the paint actually finishes
       * on the GPU. We could wait for this asynchronously with
       * ARB_timer_query (see https://bugzilla.gnome.org/show_bug.cgi?id=732350
       * for an implementation of this), but what we actually would
       * find out then is the latency for drawing a frame, not how much
       * GPU work was needed, since frames can overlap. Calling glFinish()
       * is a fairly reliable way to separate out adjacent frames
       * and measure the amount of GPU work. This is turned on with a
       * separate property from ::frame-timestamps, since it should not
       * be turned on if we're trying to actual measure latency or frame
       * rate.
       */
      static void (*finish) (void);

      if (!finish)
        load_gl_symbol ("glFinish", (void **)&finish);

      cogl_flush ();
      finish ();

      shell_perf_log_event (shell_perf_log_get_default (),
                            "clutter.paintCompletedTimestamp");
    }
}

static gboolean
global_stage_after_swap (gpointer data)
{
  /* Everything is done, we're ready for a new frame */

  ShellGlobal *global = SHELL_GLOBAL (data);

  if (global->frame_timestamps)
    shell_perf_log_event (shell_perf_log_get_default (),
                          "clutter.stagePaintDone");

  return TRUE;
}

static void
update_scaling_factor (ShellGlobal  *global,
                       MetaSettings *settings)
{
  ClutterStage *stage = CLUTTER_STAGE (global->stage);
  StThemeContext *context = st_theme_context_get_for_stage (stage);
  int scaling_factor;

  scaling_factor = meta_settings_get_ui_scaling_factor (settings);
  g_object_set (context, "scale-factor", scaling_factor, NULL);
}

static void
ui_scaling_factor_changed (MetaSettings *settings,
                           ShellGlobal  *global)
{
  update_scaling_factor (global, settings);
}

static void
entry_cursor_func (StEntry  *entry,
                   gboolean  use_ibeam,
                   gpointer  user_data)
{
  ShellGlobal *global = user_data;

  meta_display_set_cursor (global->meta_display,
                           use_ibeam ? META_CURSOR_IBEAM : META_CURSOR_DEFAULT);
}

static void
on_x11_display_closed (MetaDisplay *display,
                       ShellGlobal *global)
{
  g_signal_handlers_disconnect_by_data (global->stage, global);
}

void
_shell_global_set_plugin (ShellGlobal *global,
                          MetaPlugin  *plugin)
{
  MetaDisplay *display;
  MetaBackend *backend;
  MetaSettings *settings;

  g_return_if_fail (SHELL_IS_GLOBAL (global));
  g_return_if_fail (global->plugin == NULL);

  global->backend = meta_get_backend ();
  global->plugin = plugin;
  global->wm = shell_wm_new (plugin);

  display = meta_plugin_get_display (plugin);
  global->meta_display = display;
  global->meta_context = meta_display_get_context (display);
  global->workspace_manager = meta_display_get_workspace_manager (display);

  global->stage = CLUTTER_STAGE (meta_get_stage_for_display (display));

  if (!meta_is_wayland_compositor ())
    {
      MetaX11Display *x11_display = meta_display_get_x11_display (display);
      global->xdisplay = meta_x11_display_get_xdisplay (x11_display);
    }

  st_entry_set_cursor_func (entry_cursor_func, global);
  st_clipboard_set_selection (meta_display_get_selection (display));

  g_signal_connect (global->stage, "notify::width",
                    G_CALLBACK (global_stage_notify_width), global);
  g_signal_connect (global->stage, "notify::height",
                    G_CALLBACK (global_stage_notify_height), global);

  clutter_threads_add_repaint_func_full (CLUTTER_REPAINT_FLAGS_PRE_PAINT,
                                         global_stage_before_paint,
                                         global, NULL);

  g_signal_connect (global->stage, "after-paint",
                    G_CALLBACK (global_stage_after_paint), global);

  clutter_threads_add_repaint_func_full (CLUTTER_REPAINT_FLAGS_POST_PAINT,
                                         global_stage_after_swap,
                                         global, NULL);

  shell_perf_log_define_event (shell_perf_log_get_default(),
                               "clutter.stagePaintStart",
                               "Start of stage page repaint",
                               "");
  shell_perf_log_define_event (shell_perf_log_get_default(),
                               "clutter.paintCompletedTimestamp",
                               "Paint completion on GPU",
                               "");
  shell_perf_log_define_event (shell_perf_log_get_default(),
                               "clutter.stagePaintDone",
                               "End of frame, possibly including swap time",
                               "");

  g_signal_connect (global->stage, "notify::key-focus",
                    G_CALLBACK (focus_actor_changed), global);
  g_signal_connect (global->meta_display, "notify::focus-window",
                    G_CALLBACK (focus_window_changed), global);

  if (global->xdisplay)
    g_signal_connect_object (global->meta_display, "x11-display-closing",
                             G_CALLBACK (on_x11_display_closed), global, 0);

  backend = meta_get_backend ();
  settings = meta_backend_get_settings (backend);
  g_signal_connect (settings, "ui-scaling-factor-changed",
                    G_CALLBACK (ui_scaling_factor_changed), global);

  global->focus_manager = st_focus_manager_get_for_stage (global->stage);

  update_scaling_factor (global, settings);
}

GjsContext *
_shell_global_get_gjs_context (ShellGlobal *global)
{
  return global->js_context;
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

#if defined __linux__ || defined __sun
  char *buf;
  char *buf_p;
  char *buf_end;
  g_autoptr (GError) error = NULL;

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
#elif defined __OpenBSD__
  gchar **args, **args_p;
  gint mib[] = { CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV };

  if (sysctl (mib, G_N_ELEMENTS (mib), NULL, &len, NULL, 0) == -1)
    return;

  args = g_malloc0 (len);

  if (sysctl (mib, G_N_ELEMENTS (mib), args, &len, NULL, 0) == -1) {
    g_warning ("failed to get command line args: %d", errno);
    g_free (args);
    return;
  }

  arr = g_ptr_array_new ();
  for (args_p = args; *args_p != NULL; args_p++) {
    g_ptr_array_add (arr, *args_p);
  }

  g_ptr_array_add (arr, NULL);
#elif defined __FreeBSD__
  char *buf;
  char *buf_p;
  char *buf_end;
  gint mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_ARGS, getpid() };

  if (sysctl (mib, G_N_ELEMENTS (mib), NULL, &len, NULL, 0) == -1)
    return;

  buf = g_malloc0 (len);

  if (sysctl (mib, G_N_ELEMENTS (mib), buf, &len, NULL, 0) == -1) {
    g_warning ("failed to get command line args: %d", errno);
    g_free (buf);
    return;
  }

  buf_end = buf+len;
  arr = g_ptr_array_new ();
  /* The value returned by sysctl is NUL-separated */
  for (buf_p = buf; buf_p < buf_end; buf_p = buf_p + strlen (buf_p) + 1)
    g_ptr_array_add (arr, buf_p);

  g_ptr_array_add (arr, NULL);
#else
  return;
#endif

  /* Close all file descriptors other than stdin/stdout/stderr, otherwise
   * they will leak and stay open after the exec. In particular, this is
   * important for file descriptors that represent mapped graphics buffer
   * objects.
   */
  pre_exec_close_fds ();

  meta_display_close (shell_global_get_display (global),
                      shell_global_get_current_time (global));

  execvp (arr->pdata[0], (char**)arr->pdata);
  g_warning ("failed to reexec: %s", g_strerror (errno));
  g_ptr_array_free (arr, TRUE);
#if defined __linux__ || defined __FreeBSD__
  g_free (buf);
#elif defined __OpenBSD__
  g_free (args);
#endif
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

/**
 * shell_global_get_pointer:
 * @global: the #ShellGlobal
 * @x: (out): the X coordinate of the pointer, in global coordinates
 * @y: (out): the Y coordinate of the pointer, in global coordinates
 * @mods: (out): the current set of modifier keys that are pressed down
 *
 * Gets the pointer coordinates and current modifier key state.
 */
void
shell_global_get_pointer (ShellGlobal         *global,
                          int                 *x,
                          int                 *y,
                          ClutterModifierType *mods)
{
  ClutterModifierType raw_mods;
  MetaCursorTracker *tracker;
  graphene_point_t point;

  tracker = meta_cursor_tracker_get_for_display (global->meta_display);
  meta_cursor_tracker_get_pointer (tracker, &point, &raw_mods);

  if (x)
    *x = point.x;
  if (y)
    *y = point.y;

  *mods = raw_mods & CLUTTER_MODIFIER_MASK;
}

/**
 * shell_global_get_switcheroo_control:
 * @global: A #ShellGlobal
 *
 * Get the global #GDBusProxy instance for the switcheroo-control
 * daemon.
 *
 * Return value: (transfer none): the #GDBusProxy for the daemon,
 *   or %NULL on error.
 */
GDBusProxy *
shell_global_get_switcheroo_control (ShellGlobal  *global)
{
  return global->switcheroo_control;
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

  /* meta_display_get_current_time() will return the correct time
     when handling an X or Gdk event, but will return CurrentTime
     from some Clutter event callbacks.

     clutter_get_current_event_time() will return the correct time
     from a Clutter event callback, but may return CLUTTER_CURRENT_TIME
     timestamp if called at other times.

     So we try meta_display_get_current_time() first, since we
     can recognize a "wrong" answer from that, and then fall back
     to clutter_get_current_event_time().
   */

  time = meta_display_get_current_time (global->meta_display);
  if (time != CLUTTER_CURRENT_TIME)
    return time;

  return clutter_get_current_event_time ();
}

static void
shell_global_app_launched_cb (GAppLaunchContext *context,
                              GAppInfo          *info,
                              GVariant          *platform_data,
                              gpointer           user_data)
{
  gint32 pid;
  const gchar *app_name;

  if (!g_variant_lookup (platform_data, "pid", "i", &pid))
    return;

  app_name = g_app_info_get_id (info);
  if (app_name == NULL)
    app_name = g_app_info_get_executable (info);

  /* Start async request; we don't care about the result */
  gnome_start_systemd_scope (app_name,
                             pid,
                             NULL,
                             NULL,
                             NULL, NULL, NULL);
}

/**
 * shell_global_create_app_launch_context:
 * @global: A #ShellGlobal
 * @timestamp: the timestamp for the launch (or 0 for current time)
 * @workspace: a workspace index, or -1 to indicate the current one
 *
 * Create a #GAppLaunchContext set up with the correct timestamp, and
 * targeted to activate on the current workspace.
 *
 * Return value: (transfer full): A new #GAppLaunchContext
 */
GAppLaunchContext *
shell_global_create_app_launch_context (ShellGlobal *global,
                                        guint32      timestamp,
                                        int          workspace)
{
  MetaWorkspaceManager *workspace_manager = global->workspace_manager;
  MetaStartupNotification *sn;
  MetaLaunchContext *context;
  MetaWorkspace *ws = NULL;

  sn = meta_display_get_startup_notification (global->meta_display);
  context = meta_startup_notification_create_launcher (sn);

  if (timestamp == 0)
    timestamp = shell_global_get_current_time (global);
  meta_launch_context_set_timestamp (context, timestamp);

  if (workspace < 0)
    ws = meta_workspace_manager_get_active_workspace (workspace_manager);
  else
    ws = meta_workspace_manager_get_workspace_by_index (workspace_manager, workspace);

  meta_launch_context_set_workspace (context, ws);

  g_signal_connect (context,
                    "launched",
                    G_CALLBACK (shell_global_app_launched_cb),
                    NULL);

  return (GAppLaunchContext *) context;
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

      g_free (closure);
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
    {
      global->leisure_function_id = g_idle_add_full (G_PRIORITY_LOW,
                                                     run_leisure_functions,
                                                     global, NULL);
      g_source_set_name_by_id (global->leisure_function_id, "[gnome-shell] run_leisure_functions");
    }
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
 * approximation. Animations may be detected as terminating early if they
 * can be drawn fast enough so that the event loop goes idle between frames.
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
  LeisureClosure *closure = g_new (LeisureClosure, 1);
  closure->func = func;
  closure->user_data = user_data;
  closure->notify = notify;

  global->leisure_closures = g_slist_append (global->leisure_closures,
                                             closure);

  if (global->work_count == 0)
    schedule_leisure_functions (global);
}

const char *
shell_global_get_session_mode (ShellGlobal *global)
{
  g_return_val_if_fail (SHELL_IS_GLOBAL (global), "user");

  return global->session_mode;
}

static void
delete_variant_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  ShellGlobal *global = user_data;
  GError *error = NULL;

  if (!g_file_delete_finish (G_FILE (object), result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          g_warning ("Could not delete runtime/persistent state file: %s\n",
                     error->message);
        }

      g_error_free (error);
    }

  g_hash_table_remove (global->save_ops, object);
}

static void
replace_contents_worker (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  GFile *file = source_object;
  GBytes *bytes = task_data;
  GError *error = NULL;
  const gchar *data;
  gsize len;

  data = g_bytes_get_data (bytes, &len);

  if (!g_file_replace_contents (file, data, len, NULL, FALSE,
                                G_FILE_CREATE_REPLACE_DESTINATION,
                                NULL, cancellable, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

static void
replace_contents_async (GFile               *path,
                        GBytes              *bytes,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (G_IS_FILE (path));
  g_assert (bytes != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (path, cancellable, callback, user_data);
  g_task_set_source_tag (task, replace_contents_async);
  g_task_set_task_data (task, g_bytes_ref (bytes), (GDestroyNotify)g_bytes_unref);
  g_task_run_in_thread (task, replace_contents_worker);
}

static gboolean
replace_contents_finish (GFile         *file,
                         GAsyncResult  *result,
                         GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
replace_variant_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  ShellGlobal *global = user_data;
  GError *error = NULL;

  if (!replace_contents_finish (G_FILE (object), result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Could not replace runtime/persistent state file: %s\n",
                     error->message);
        }

      g_error_free (error);
    }

  g_hash_table_remove (global->save_ops, object);
}

static void
save_variant (ShellGlobal *global,
              GFile       *dir,
              const char  *property_name,
              GVariant    *variant)
{
  GFile *path = g_file_get_child (dir, property_name);
  GCancellable *cancellable;

  cancellable = g_hash_table_lookup (global->save_ops, path);
  g_cancellable_cancel (cancellable);

  cancellable = g_cancellable_new ();
  g_hash_table_insert (global->save_ops, g_object_ref (path), cancellable);

  if (variant == NULL || g_variant_get_data (variant) == NULL)
    {
      g_file_delete_async (path, G_PRIORITY_DEFAULT, cancellable,
                           delete_variant_cb, global);
    }
  else
    {
      g_autoptr(GBytes) bytes = NULL;

      bytes = g_bytes_new_with_free_func (g_variant_get_data (variant),
                                          g_variant_get_size (variant),
                                          (GDestroyNotify)g_variant_unref,
                                          g_variant_ref (variant));
      /* g_file_replace_contents_async() can potentially fsync() from the
       * calling thread when completing the asynchronous task. Instead, we
       * want to force that fsync() to a thread to avoid blocking the
       * compositor main loop. Using our own replace_contents_async()
       * simply executes the operation synchronously from a thread.
       */
      replace_contents_async (path, bytes, cancellable, replace_variant_cb, global);
    }

  g_object_unref (path);
}

static GVariant *
load_variant (GFile      *dir,
              const char *property_type,
              const char *property_name)
{
  GVariant *res = NULL;
  GMappedFile *mfile;
  GFile *path = g_file_get_child (dir, property_name);
  char *pathstr;
  GError *local_error = NULL;

  pathstr = g_file_get_path (path);
  mfile = g_mapped_file_new (pathstr, FALSE, &local_error);
  if (!mfile)
    {
      if (!g_error_matches (local_error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
          g_warning ("Failed to open runtime state: %s", local_error->message);
        }
      g_clear_error (&local_error);
    }
  else
    {
      GBytes *bytes = g_mapped_file_get_bytes (mfile);
      res = g_variant_new_from_bytes (G_VARIANT_TYPE (property_type), bytes, FALSE);
      g_bytes_unref (bytes);
      g_mapped_file_unref (mfile);
    }

  g_object_unref (path);
  g_free (pathstr);

  return res;
}

/**
 * shell_global_set_runtime_state:
 * @global: a #ShellGlobal
 * @property_name: Name of the property
 * @variant: (nullable): A #GVariant, or %NULL to unset
 *
 * Change the value of serialized runtime state.
 */
void
shell_global_set_runtime_state (ShellGlobal  *global,
                                const char   *property_name,
                                GVariant     *variant)
{
  save_variant (global, global->runtime_state_path, property_name, variant);
}

/**
 * shell_global_get_runtime_state:
 * @global: a #ShellGlobal
 * @property_type: Expected data type
 * @property_name: Name of the property
 *
 * The shell maintains "runtime" state which does not persist across
 * logout or reboot.
 *
 * Returns: (transfer floating): The value of a serialized property, or %NULL if none stored
 */
GVariant *
shell_global_get_runtime_state (ShellGlobal  *global,
                                const char   *property_type,
                                const char   *property_name)
{
  return load_variant (global->runtime_state_path, property_type, property_name);
}

/**
 * shell_global_set_persistent_state:
 * @global: a #ShellGlobal
 * @property_name: Name of the property
 * @variant: (nullable): A #GVariant, or %NULL to unset
 *
 * Change the value of serialized persistent state.
 */
void
shell_global_set_persistent_state (ShellGlobal *global,
                                   const char  *property_name,
                                   GVariant    *variant)
{
  save_variant (global, global->userdatadir_path, property_name, variant);
}

/**
 * shell_global_get_persistent_state:
 * @global: a #ShellGlobal
 * @property_type: Expected data type
 * @property_name: Name of the property
 *
 * The shell maintains "persistent" state which will persist after
 * logout or reboot.
 *
 * Returns: (transfer none): The value of a serialized property, or %NULL if none stored
 */
GVariant *
shell_global_get_persistent_state (ShellGlobal  *global,
                                   const char   *property_type,
                                   const char   *property_name)
{
  return load_variant (global->userdatadir_path, property_type, property_name);
}

void
_shell_global_locate_pointer (ShellGlobal *global)
{
  g_signal_emit (global, shell_global_signals[LOCATE_POINTER], 0);
}
