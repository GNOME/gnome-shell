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
#include <canberra.h>
#include <canberra-gtk.h>
#include <clutter/x11/clutter-x11.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <girepository.h>
#include <meta/meta-backend.h>
#include <meta/display.h>
#include <meta/util.h>
#include <meta/meta-shaped-texture.h>
#include <meta/meta-cursor-tracker.h>
#include <meta/meta-settings.h>
#include <meta/meta-workspace-manager.h>
#include <meta/meta-x11-display.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-journal.h>
#include <errno.h>
#include <unistd.h>
#endif

/* Memory report bits */
#ifdef HAVE_MALLINFO
#include <malloc.h>
#endif

#if defined __OpenBSD__ || defined __FreeBSD__
#include <sys/sysctl.h>
#endif

#include "shell-enum-types.h"
#include "shell-global-private.h"
#include "shell-perf-log.h"
#include "shell-window-tracker.h"
#include "shell-wm.h"
#include "st.h"

static ShellGlobal *the_object = NULL;

struct _ShellGlobal {
  GObject parent;

  ClutterStage *stage;
  Window stage_xwindow;

  MetaDisplay *meta_display;
  MetaWorkspaceManager *workspace_manager;
  GdkDisplay *gdk_display;
  MetaX11Display *x11_display;
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

  /* For sound notifications */
  ca_context *sound_context;

  gboolean has_modal;
  gboolean frame_timestamps;
  gboolean frame_finish_timestamp;
};

enum {
  PROP_0,

  PROP_SESSION_MODE,
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
};

/* Signals */
enum
{
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

  global->sound_context = ca_gtk_context_get ();
  ca_context_change_props (global->sound_context,
                           CA_PROP_APPLICATION_NAME, "GNOME Shell",
                           CA_PROP_APPLICATION_ID, "org.gnome.Shell",
                           CA_PROP_APPLICATION_ICON_NAME, "start-here",
                           CA_PROP_APPLICATION_LANGUAGE, setlocale (LC_MESSAGES, NULL),
                           NULL);
  ca_context_open (global->sound_context);

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
}

static void
shell_global_finalize (GObject *object)
{
  ShellGlobal *global = SHELL_GLOBAL (object);

  g_clear_object (&global->js_context);
  g_object_unref (global->settings);

  the_object = NULL;

  g_clear_object (&global->userdatadir_path);
  g_clear_object (&global->runtime_state_path);

  g_free (global->session_mode);
  g_free (global->imagedir);
  g_free (global->userdatadir);

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

  g_object_class_install_property (gobject_class,
                                   PROP_SESSION_MODE,
                                   g_param_spec_string ("session-mode",
                                                        "Session Mode",
                                                        "The session mode to use",
                                                        "user",
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

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
                                   PROP_DISPLAY,
                                   g_param_spec_object ("display",
                                                        "Display",
                                                        "Metacity display object for the shell",
                                                        META_TYPE_DISPLAY,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_WORKSPACE_MANAGER,
                                   g_param_spec_object ("workspace-manager",
                                                        "Workspace manager",
                                                        "Workspace manager",
                                                        META_TYPE_WORKSPACE_MANAGER,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_STAGE,
                                   g_param_spec_object ("stage",
                                                        "Stage",
                                                        "Stage holding the desktop scene graph",
                                                        CLUTTER_TYPE_ACTOR,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_WINDOW_GROUP,
                                   g_param_spec_object ("window-group",
                                                        "Window Group",
                                                        "Actor holding window actors",
                                                        CLUTTER_TYPE_ACTOR,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
                                     PROP_TOP_WINDOW_GROUP,
                                     g_param_spec_object ("top-window-group",
                                                          "Top Window Group",
                                                          "Actor holding override-redirect windows",
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
  g_object_class_install_property (gobject_class,
                                   PROP_FRAME_TIMESTAMPS,
                                   g_param_spec_boolean ("frame-timestamps",
                                                         "Frame Timestamps",
                                                         "Whether to log frame timestamps in the performance log",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_FRAME_FINISH_TIMESTAMP,
                                   g_param_spec_boolean ("frame-finish-timestamp",
                                                         "Frame Finish Timestamps",
                                                         "Whether at the end of a frame to call glFinish and log paintCompletedTimestamp",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
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

  if (global->has_modal)
    return;

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

  if (global->has_modal)
    return;

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

  if (global->has_modal)
    meta_set_stage_input_region (display, None);
  else
    meta_set_stage_input_region (display, global->input_region);
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
global_stage_after_paint (ClutterStage *stage,
                          ShellGlobal  *global)
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

void
_shell_global_set_plugin (ShellGlobal *global,
                          MetaPlugin  *plugin)
{
  MetaDisplay *display;
  MetaBackend *backend;
  MetaSettings *settings;

  g_return_if_fail (SHELL_IS_GLOBAL (global));
  g_return_if_fail (global->plugin == NULL);

  global->plugin = plugin;
  global->wm = shell_wm_new (plugin);

  display = meta_plugin_get_display (plugin);
  global->meta_display = display;
  global->workspace_manager = meta_display_get_workspace_manager (display);
  global->x11_display = meta_display_get_x11_display (display);
  global->xdisplay = meta_x11_display_get_xdisplay (global->x11_display);

  global->gdk_display = gdk_x11_lookup_xdisplay (global->xdisplay);

  global->stage = CLUTTER_STAGE (meta_get_stage_for_display (display));

  if (meta_is_wayland_compositor ())
    {
      global->stage_xwindow = None;
    }
  else
    {
      global->stage_xwindow = clutter_x11_get_stage_window (global->stage);
    }

  st_entry_set_cursor_func (entry_cursor_func, global);

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
 * Returns: %TRUE if we succesfully entered the mode. %FALSE if we couldn't
 *  enter the mode. Failure may occur because an application has the pointer
 *  or keyboard grabbed, because Mutter is in a mode itself like moving a
 *  window or alt-Tab window selection, or because shell_global_begin_modal()
 *  was previouly called.
 */
gboolean
shell_global_begin_modal (ShellGlobal       *global,
                          guint32           timestamp,
                          MetaModalOptions  options)
{
  /* Make it an error to call begin_modal while we already
   * have a modal active. */
  if (global->has_modal)
    return FALSE;

  global->has_modal = meta_plugin_begin_modal (global->plugin, options, timestamp);
  sync_input_region (global);
  return global->has_modal;
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
  if (!global->has_modal)
    return;

  meta_plugin_end_modal (global->plugin, timestamp);
  global->has_modal = FALSE;

  /* If the stage window is unfocused, ensure that there's no
   * actor focused on Clutter's side. */
  if (!meta_stage_is_focused (global->meta_display))
    clutter_stage_set_key_focus (global->stage, NULL);

  /* An actor dropped key focus. Focus the default window. */
  else if (get_key_focused_actor (global) && meta_stage_is_focused (global->meta_display))
    meta_display_focus_default_window (global->meta_display,
                                       get_current_time_maybe_roundtrip (global));

  sync_input_region (global);
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
  GError *error = NULL;

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
 * shell_global_log_structured:
 * @message: A message to print
 * @keys: (allow-none) (array zero-terminated=1) (element-type utf8): Optional structured data
 *
 * Log structured data in an operating-system specific fashion.  The
 * parameter @opts should be an array of UTF-8 KEY=VALUE strings.
 * This function does not support binary data.  See
 * http://www.freedesktop.org/software/systemd/man/systemd.journal-fields.html
 * or more information about fields that can be used on a systemd
 * system.
 *
 */
void
shell_global_log_structured (const char *message,
                             const char *const *keys)
{
#ifdef HAVE_SYSTEMD
    const char *const*iter;
    char *msgkey;
    guint i, n_opts;
    struct iovec *iovs;

    for (n_opts = 0, iter = keys; *iter; iter++, n_opts++)
        ;

    n_opts++; /* Add one for MESSAGE= */
    iovs = g_alloca (sizeof (struct iovec) * n_opts);

    for (i = 0, iter = keys; *iter; iter++, i++) {
        iovs[i].iov_base = (char*)keys[i];
        iovs[i].iov_len = strlen (keys[i]);
    }
    g_assert(i == n_opts-1);
    msgkey = g_strconcat ("MESSAGE=", message, NULL);
    iovs[i].iov_base = msgkey;
    iovs[i].iov_len = strlen (msgkey);

    // The code location isn't useful since we're wrapping
    sd_journal_sendv (iovs, n_opts);
    g_free (msgkey);
#else
    g_print ("%s\n", message);
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
 * shell_global_init_xdnd:
 * @global: the #ShellGlobal
 *
 * Enables tracking of Xdnd events
 */
void shell_global_init_xdnd (ShellGlobal *global)
{
  Window output_window = meta_get_overlay_window (global->meta_display);
  long xdnd_version = 5;

  XChangeProperty (global->xdisplay, global->stage_xwindow,
                   gdk_x11_get_xatom_by_name ("XdndAware"), XA_ATOM,
                   32, PropModeReplace, (const unsigned char *)&xdnd_version, 1);

  XChangeProperty (global->xdisplay, output_window,
                   gdk_x11_get_xatom_by_name ("XdndProxy"), XA_WINDOW,
                   32, PropModeReplace, (const unsigned char *)&global->stage_xwindow, 1);

  /*
   * XdndProxy is additionally set on the proxy window as verification that the
   * XdndProxy property on the target window isn't a left-over
   */
  XChangeProperty (global->xdisplay, global->stage_xwindow,
                   gdk_x11_get_xatom_by_name ("XdndProxy"), XA_WINDOW,
                   32, PropModeReplace, (const unsigned char *)&global->stage_xwindow, 1);
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

  tracker = meta_cursor_tracker_get_for_display (global->meta_display);
  meta_cursor_tracker_get_pointer (tracker, x, y, &raw_mods);

  *mods = raw_mods & CLUTTER_MODIFIER_MASK;
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
  ClutterModifierType mods;
  ClutterMotionEvent event;

  shell_global_get_pointer (global, &x, &y, &mods);

  event.type = CLUTTER_MOTION;
  event.time = shell_global_get_current_time (global);
  event.flags = CLUTTER_EVENT_FLAG_SYNTHETIC;
  event.stage = global->stage;
  event.x = x;
  event.y = y;
  event.modifier_state = mods;
  event.axes = NULL;
  event.device = clutter_device_manager_get_device (clutter_device_manager_get_default (),
                                                    META_VIRTUAL_CORE_POINTER_ID);

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
  GdkAppLaunchContext *context;

  context = gdk_display_get_app_launch_context (global->gdk_display);

  if (timestamp == 0)
    timestamp = shell_global_get_current_time (global);
  gdk_app_launch_context_set_timestamp (context, timestamp);

  if (workspace < 0)
    {
      MetaWorkspaceManager *workspace_manager = global->workspace_manager;

      workspace =
        meta_workspace_manager_get_active_workspace_index (workspace_manager);
    }
  gdk_app_launch_context_set_desktop (context, workspace);

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

static void
build_ca_proplist_for_event (ca_proplist  *props,
                             const char   *event_property,
                             const char   *event_id,
                             const char   *event_description,
                             ClutterEvent *for_event)
{
  ca_proplist_sets (props, event_property, event_id);
  ca_proplist_sets (props, CA_PROP_EVENT_DESCRIPTION, event_description);
  ca_proplist_sets (props, CA_PROP_CANBERRA_CACHE_CONTROL, "volatile");

  if (for_event)
    {
      if (clutter_event_type (for_event) != CLUTTER_KEY_PRESS &&
          clutter_event_type (for_event) != CLUTTER_KEY_RELEASE)
        {
          ClutterPoint point;

          clutter_event_get_position (for_event, &point);

          ca_proplist_setf (props, CA_PROP_EVENT_MOUSE_X, "%d", (int)point.x);
          ca_proplist_setf (props, CA_PROP_EVENT_MOUSE_Y, "%d", (int)point.y);
        }

      if (clutter_event_type (for_event) == CLUTTER_BUTTON_PRESS ||
          clutter_event_type (for_event) == CLUTTER_BUTTON_RELEASE)
        {
          gint button;

          button = clutter_event_get_button (for_event);
          ca_proplist_setf (props, CA_PROP_EVENT_MOUSE_BUTTON, "%d", button);
        }
    }
}

/**
 * shell_global_play_theme_sound:
 * @global: the #ShellGlobal
 * @id: an id, used to cancel later (0 if not needed)
 * @name: the sound name
 * @for_event: (nullable): a #ClutterEvent in response to which the sound is played
 *
 * Plays a simple sound picked according to Freedesktop sound theme.
 * Really just a workaround for libcanberra not being introspected.
 */
void
shell_global_play_theme_sound (ShellGlobal  *global,
                               guint         id,
                               const char   *name,
                               const char   *description,
                               ClutterEvent *for_event)
{
  ca_proplist *props;

  ca_proplist_create (&props);
  build_ca_proplist_for_event (props, CA_PROP_EVENT_ID, name, description, for_event);

  ca_context_play_full (global->sound_context, id, props, NULL, NULL);

  ca_proplist_destroy (props);
}

/**
 * shell_global_play_theme_sound_full:
 * @global: the #ShellGlobal
 * @id: an id, used to cancel later (0 if not needed)
 * @name: the sound name
 * @description: the localized description of the event that triggered this alert
 * @for_event: (nullable): a #ClutterEvent in response to which the sound is played
 * @application_id: application on behalf of which the sound is played
 * @application_name:
 *
 * Plays a simple sound picked according to Freedesktop sound theme.
 * Really just a workaround for libcanberra not being introspected.
 */
void
shell_global_play_theme_sound_full (ShellGlobal  *global,
                                    guint         id,
                                    const char   *name,
                                    const char   *description,
                                    ClutterEvent *for_event,
                                    const char   *application_id,
                                    const char   *application_name)
{
  ca_proplist *props;

  ca_proplist_create (&props);
  build_ca_proplist_for_event (props, CA_PROP_EVENT_ID, name, description, for_event);
  ca_proplist_sets (props, CA_PROP_APPLICATION_ID, application_id);
  ca_proplist_sets (props, CA_PROP_APPLICATION_NAME, application_name);

  ca_context_play_full (global->sound_context, id, props, NULL, NULL);

  ca_proplist_destroy (props);
}

/**
 * shell_global_play_sound_file_full:
 * @global: the #ShellGlobal
 * @id: an id, used to cancel later (0 if not needed)
 * @file_name: the file name to play
 * @description: the localized description of the event that triggered this alert
 * @for_event: (nullable): a #ClutterEvent in response to which the sound is played
 * @application_id: application on behalf of which the sound is played
 * @application_name:
 *
 * Like shell_global_play_theme_sound_full(), but with an explicit path
 * instead of a themed sound.
 */
void
shell_global_play_sound_file_full  (ShellGlobal  *global,
                                    guint         id,
                                    const char   *file_name,
                                    const char   *description,
                                    ClutterEvent *for_event,
                                    const char   *application_id,
                                    const char   *application_name)
{
  ca_proplist *props;

  ca_proplist_create (&props);
  build_ca_proplist_for_event (props, CA_PROP_MEDIA_FILENAME, file_name, description, for_event);
  ca_proplist_sets (props, CA_PROP_APPLICATION_ID, application_id);
  ca_proplist_sets (props, CA_PROP_APPLICATION_NAME, application_name);

  ca_context_play_full (global->sound_context, id, props, NULL, NULL);

  ca_proplist_destroy (props);
}

/**
 * shell_global_play_sound_file:
 * @global: the #ShellGlobal
 * @id: an id, used to cancel later (0 if not needed)
 * @file_name: the file name to play
 * @description: the localized description of the event that triggered this alert
 * @for_event: (nullable): a #ClutterEvent in response to which the sound is played
 *
 * Like shell_global_play_theme_sound(), but with an explicit path
 * instead of a themed sound.
 */
void
shell_global_play_sound_file (ShellGlobal  *global,
                              guint         id,
                              const char   *file_name,
                              const char   *description,
                              ClutterEvent *for_event)
{
  ca_proplist *props;

  ca_proplist_create (&props);
  build_ca_proplist_for_event (props, CA_PROP_MEDIA_FILENAME, file_name, description, for_event);

  ca_context_play_full (global->sound_context, id, props, NULL, NULL);

  ca_proplist_destroy (props);
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

const char *
shell_global_get_session_mode (ShellGlobal *global)
{
  g_return_val_if_fail (SHELL_IS_GLOBAL (global), "user");

  return global->session_mode;
}

static void
save_variant (GFile      *dir,
              const char *property_name,
              GVariant   *variant)
{
  GFile *path = g_file_get_child (dir, property_name);

  if (variant == NULL || g_variant_get_data (variant) == NULL)
    (void) g_file_delete (path, NULL, NULL);
  else
    {
      gsize size = g_variant_get_size (variant);
      g_file_replace_contents (path, g_variant_get_data (variant), size,
                               NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION,
                               NULL, NULL, NULL);
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
      res = g_variant_new_from_bytes (G_VARIANT_TYPE (property_type), bytes, TRUE);
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
  save_variant (global->runtime_state_path, property_name, variant);
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
  save_variant (global->userdatadir_path, property_name, variant);
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
