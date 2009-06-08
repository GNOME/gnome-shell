/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "shell-global.h"
#include "shell-wm.h"

#include "display.h"
#include <clutter/glx/clutter-glx.h>
#include <clutter/x11/clutter-x11.h>
#include <gdk/gdkx.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dbus/dbus-glib.h>
#include <libgnomeui/gnome-thumbnail.h>
#include <math.h>
#include <X11/extensions/Xfixes.h>

#define SHELL_DBUS_SERVICE "org.gnome.Shell"

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
  
  MutterPlugin *plugin;
  ShellWM *wm;
  gboolean keyboard_grabbed;
  const char *imagedir;
  const char *configdir;

  /* Displays the root window; see shell_global_create_root_pixmap_actor() */
  ClutterActor *root_pixmap;
};

enum {
  PROP_0,

  PROP_OVERLAY_GROUP,
  PROP_SCREEN,
  PROP_SCREEN_WIDTH,
  PROP_SCREEN_HEIGHT,
  PROP_STAGE,
  PROP_WINDOW_GROUP,
  PROP_WINDOW_MANAGER,
  PROP_IMAGEDIR,
  PROP_CONFIGDIR,
};

/* Signals */
enum
{
  PANEL_RUN_DIALOG,
  PANEL_MAIN_MENU,
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
  switch (prop_id)
    {
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
      g_value_set_object (value, mutter_plugin_get_overlay_group (global->plugin));
      break;
    case PROP_SCREEN:
      g_value_set_object (value, mutter_plugin_get_screen (global->plugin));
      break;
    case PROP_SCREEN_WIDTH:
      {
        int width, height;

        mutter_plugin_query_screen_size (global->plugin, &width, &height);
        g_value_set_int (value, width);
      }
      break;
    case PROP_SCREEN_HEIGHT:
      {
        int width, height;

        mutter_plugin_query_screen_size (global->plugin, &width, &height);
        g_value_set_int (value, height);
      }
      break;
    case PROP_STAGE:
      g_value_set_object (value, mutter_plugin_get_stage (global->plugin));
      break;
    case PROP_WINDOW_GROUP:
      g_value_set_object (value, mutter_plugin_get_window_group (global->plugin));
      break;
    case PROP_WINDOW_MANAGER:
      g_value_set_object (value, global->wm);
      break;
    case PROP_IMAGEDIR:
      g_value_set_string (value, global->imagedir);
      break;
    case PROP_CONFIGDIR:
      g_value_set_string (value, global->configdir);
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
  char *imagedir;
  GFile *conf_dir;

  if (!datadir)
    datadir = GNOME_SHELL_DATADIR;

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
  global->configdir = g_build_filename (g_get_home_dir (), ".gnome2", "shell", NULL);
  conf_dir = g_file_new_for_path (global->configdir);
  g_file_make_directory (conf_dir, NULL, NULL);
  g_object_unref (conf_dir);
  
  global->grab_notifier = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
  g_signal_connect (global->grab_notifier, "grab-notify", G_CALLBACK (grab_notify), global);
  global->gtk_grab_active = FALSE;

  global->root_pixmap = NULL;

  global->input_mode = SHELL_STAGE_INPUT_MODE_NORMAL;
}

static void
shell_global_class_init (ShellGlobalClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shell_global_get_property;
  gobject_class->set_property = shell_global_set_property;

  shell_global_signals[PANEL_RUN_DIALOG] =
    g_signal_new ("panel-run-dialog",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ShellGlobalClass, panel_run_dialog),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__INT,
		  G_TYPE_NONE, 1, G_TYPE_INT);

  shell_global_signals[PANEL_MAIN_MENU] =
    g_signal_new ("panel-main-menu",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ShellGlobalClass, panel_main_menu),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__INT,
		  G_TYPE_NONE, 1, G_TYPE_INT);

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
                                   PROP_WINDOW_GROUP,
                                   g_param_spec_object ("window-group",
                                                        "Window Group",
                                                        "Actor holding window actors",
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
                                   PROP_IMAGEDIR,
                                   g_param_spec_string ("imagedir",
                                                        "Image directory",
                                                        "Directory containing gnome-shell image files",
                                                        NULL,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_CONFIGDIR,
                                   g_param_spec_string ("configdir",
                                                        "Configuration directory",
                                                        "Directory containing gnome-shell configuration files",
                                                        NULL,
                                                        G_PARAM_READABLE));
}

/**
 * search_path_init:
 *
 * search_path_init and get_applications_search_path below were copied from glib/gio/gdesktopappinfo.c
 * copyright Red Hat, Inc., written by Alex Larsson, licensed under the LGPL
 *
 * Return value: location of an array with user and system application directories.
 */ 
static gpointer
search_path_init (gpointer data)
{ 	
    char **args = NULL;
    const char * const *data_dirs;
    const char *user_data_dir;
    int i, length, j;
 	 
    data_dirs = g_get_system_data_dirs ();
    length = g_strv_length ((char **)data_dirs);
 	
    args = g_new (char *, length + 2);
 	
    j = 0;
    user_data_dir = g_get_user_data_dir ();
    args[j++] = g_build_filename (user_data_dir, "applications", NULL);
    for (i = 0; i < length; i++)
      args[j++] = g_build_filename (data_dirs[i],
 	                            "applications", NULL);
    args[j++] = NULL;
 	 	
    return args;
}
/**
 * get_applications_search_path:
 *
 * Return value: location of an array with user and system application directories.
 */
static const char * const *
get_applications_search_path (void)
{
    static GOnce once_init = G_ONCE_INIT;
    return g_once (&once_init, search_path_init, NULL);
} 

/**
 * shell_clutter_texture_set_from_pixbuf: 
 * texture: #ClutterTexture to be modified
 * pixbuf: #GdkPixbuf to set as an image for #ClutterTexture
 *
 * Convenience function for setting an image for #ClutterTexture based on #GdkPixbuf.
 * Copied from an example posted by hp in this thread http://mail.gnome.org/archives/gtk-devel-list/2008-September/msg00218.html
 *
 * Return value: %TRUE on success, %FALSE on failure
 */
gboolean
shell_clutter_texture_set_from_pixbuf (ClutterTexture *texture,
                                       GdkPixbuf      *pixbuf)
{
    return clutter_texture_set_from_rgb_data (texture,
                                              gdk_pixbuf_get_pixels (pixbuf),
                                              gdk_pixbuf_get_has_alpha (pixbuf),
                                              gdk_pixbuf_get_width (pixbuf),
                                              gdk_pixbuf_get_height (pixbuf),
                                              gdk_pixbuf_get_rowstride (pixbuf),
                                              gdk_pixbuf_get_has_alpha (pixbuf)
                                              ? 4 : 3,
                                              0, NULL);
}

static GnomeThumbnailFactory *thumbnail_factory;

/**
 * shell_get_thumbnail_for_recent_info:
 *
 * @recent_info: #GtkRecentInfo for which to return a thumbnail
 *
 * Return value: #GdkPixbuf containing a thumbnail for the file described by #GtkRecentInfo 
 *               if the thumbnail exists or can be generated, %NULL otherwise
 */
GdkPixbuf *
shell_get_thumbnail_for_recent_info(GtkRecentInfo  *recent_info)
{
    char *existing_thumbnail;
    GdkPixbuf *pixbuf = NULL;
    const gchar *uri = gtk_recent_info_get_uri (recent_info);
    time_t mtime = gtk_recent_info_get_modified (recent_info);
    const gchar *mime_type = gtk_recent_info_get_mime_type (recent_info);
    GError *error = NULL;
    
    if (thumbnail_factory == NULL)
      thumbnail_factory = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);

    existing_thumbnail = gnome_thumbnail_factory_lookup (thumbnail_factory, uri, mtime);

    if (existing_thumbnail != NULL)
      {
        pixbuf = gdk_pixbuf_new_from_file(existing_thumbnail, &error);
        if (error != NULL) 
          {
            g_warning("Could not generate a pixbuf from file %s: %s", existing_thumbnail, error->message);
            g_clear_error (&error);
          }
      }
    else if (gnome_thumbnail_factory_has_valid_failed_thumbnail (thumbnail_factory, uri, mtime))
      return NULL;
    else if (gnome_thumbnail_factory_can_thumbnail (thumbnail_factory, uri, mime_type, mtime)) 
      {
        pixbuf = gnome_thumbnail_factory_generate_thumbnail (thumbnail_factory, uri, mime_type);
        if (pixbuf)
          {
            // we need to save the thumbnail so that we don't need to generate it again in the future
            gnome_thumbnail_factory_save_thumbnail (thumbnail_factory, pixbuf, uri, mtime);
          }          
        else 
          {
            g_warning ("Could not generate thumbnail for %s", uri);
            gnome_thumbnail_factory_create_failed_thumbnail (thumbnail_factory, uri, mtime);
          }
      }

    return pixbuf;   
}


/**
 * shell_get_categories_for_desktop_file:
 *
 * @desktop_file_name: name of the desktop file for which to retrieve categories
 *
 * Return value: (element-type char*) (transfer full): List of categories
 *
 */
GSList *
shell_get_categories_for_desktop_file(const char *desktop_file_name)
{
    GKeyFile *key_file;
    const char * const *search_dirs;
    char **categories = NULL;
    GSList *categories_list = NULL; 
    char *full_path = NULL;   
    GError *error = NULL;
    gsize len;  
    int i; 

    key_file = g_key_file_new (); 
    search_dirs = get_applications_search_path();
   
    g_key_file_load_from_dirs (key_file, desktop_file_name, (const char **)search_dirs, &full_path, 0, &error);

    if (error != NULL) 
      {
        g_warning ("Error when loading a key file for %s: %s", desktop_file_name, error->message);
        g_clear_error (&error);
      }
    else 
      {
        categories = g_key_file_get_string_list (key_file,
                                                 "Desktop Entry",
                                                 "Categories",
                                                 &len,
                                                 &error);
        if (error != NULL)
          {
            // "Categories" is not a required key in the desktop files, so it's ok if we didn't find it
            g_clear_error (&error);
          } 
      }

    g_key_file_free (key_file);
 
    if (categories == NULL)
      return NULL;

    // gjs currently does not support returning arrays (other than a NULL value for an array), so we need 
    // to convert the array we are returning to GSList, returning which gjs supports. 
    // See http://bugzilla.gnome.org/show_bug.cgi?id=560567 for more info on gjs array support.
    for (i = 0; categories[i]; i++)
      {
        categories_list = g_slist_prepend (categories_list, g_strdup (categories[i])); 
      }

    g_strfreev (categories);

    return categories_list;
}

/**
 * shell_get_event_key_symbol:
 *
 * Return value: Clutter key value for the key press and release events, 
 *               as specified in clutter-keysyms.h  
 */
guint16
shell_get_event_key_symbol(ClutterEvent *event)
{
  g_return_val_if_fail(event->type == CLUTTER_KEY_PRESS ||
                       event->type == CLUTTER_KEY_RELEASE, 0);

  return event->key.keyval;
}

/**
 * shell_get_button_event_click_count:
 *
 * Return value: click count for button press and release events
 */
guint16
shell_get_button_event_click_count(ClutterEvent *event)
{
  g_return_val_if_fail(event->type == CLUTTER_BUTTON_PRESS ||
                       event->type == CLUTTER_BUTTON_RELEASE, 0);
  return event->button.click_count;
}

/**
 * shell_get_event_related:
 *
 * Return value: (transfer none): related actor
 */
ClutterActor *
shell_get_event_related (ClutterEvent *event)
{
  g_return_val_if_fail (event->type == CLUTTER_ENTER ||
                        event->type == CLUTTER_LEAVE, NULL);
  return event->crossing.related;
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
  static ShellGlobal *the_object = NULL;

  if (!the_object)
    the_object = g_object_new (SHELL_TYPE_GLOBAL, 0);

  return the_object;
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
    mutter_plugin_set_stage_reactive (global->plugin, FALSE);
  else if (mode == SHELL_STAGE_INPUT_MODE_FULLSCREEN || !global->input_region)
    mutter_plugin_set_stage_reactive (global->plugin, TRUE);
  else
    mutter_plugin_set_stage_input_region (global->plugin, global->input_region);

  global->input_mode = mode;
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
  MetaScreen *screen = mutter_plugin_get_screen (global->plugin);
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
 * shell_global_get_windows:
 *
 * Gets the list of MutterWindows for the plugin's screen
 *
 * Return value: (element-type MutterWindow) (transfer none): the list of windows
 */
GList *
shell_global_get_windows (ShellGlobal *global)
{
  g_return_val_if_fail (SHELL_IS_GLOBAL (global), NULL);

  return mutter_plugin_get_windows (global->plugin);
}

void
_shell_global_set_plugin (ShellGlobal  *global,
                          MutterPlugin *plugin)
{
  g_return_if_fail (SHELL_IS_GLOBAL (global));
  g_return_if_fail (global->plugin == NULL);

  global->plugin = plugin;
  global->wm = shell_wm_new (plugin);
}

/**
 * shell_global_grab_keyboard:
 * @global: a #ShellGlobal
 *
 * Grab the keyboard to the stage window. The stage will receive
 * all keyboard events until shell_global_ungrab_keyboard() is called.
 * This is appropriate to do when the desktop goes into a special
 * mode where no normal global key shortcuts or application keyboard
 * processing should happen.
 */
gboolean
shell_global_grab_keyboard (ShellGlobal *global)
{
  MetaScreen *screen = mutter_plugin_get_screen (global->plugin);
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  ClutterStage *stage = CLUTTER_STAGE (mutter_plugin_get_stage (global->plugin));
  Window stagewin = clutter_x11_get_stage_window (stage);

  /* FIXME: we need to coordinate with the rest of Metacity or we
   * may grab the keyboard away from other portions of Metacity
   * and leave Metacity in a confused state. An X client is allowed
   * to overgrab itself, though not allowed to grab they keyboard
   * away from another applications.
   */
  if (global->keyboard_grabbed)
    return FALSE;

  if (XGrabKeyboard (xdisplay, stagewin,
                     False, /* owner_events - steal events from the rest of metacity */
                     GrabModeAsync, GrabModeAsync,
                     CurrentTime) != Success)
    return FALSE; /* probably AlreadyGrabbed, some other app has a keyboard grab */

  global->keyboard_grabbed = TRUE;

  return TRUE;
}

/**
 * shell_global_ungrab_keyboard:
 * @global: a #ShellGlobal
 *
 * Undoes the effect of shell_global_grab_keyboard
 */
void
shell_global_ungrab_keyboard (ShellGlobal *global)
{
  MetaScreen *screen;
  MetaDisplay *display;
  Display *xdisplay;

  g_return_if_fail (global->keyboard_grabbed);

  screen = mutter_plugin_get_screen (global->plugin);
  display = meta_screen_get_display (screen);
  xdisplay = meta_display_get_xdisplay (display);

  XUngrabKeyboard (xdisplay, CurrentTime);

  global->keyboard_grabbed = FALSE;
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

void 
shell_global_grab_dbus_service (ShellGlobal *global)
{
  GError *error = NULL;
  DBusGConnection *session;
  DBusGProxy *bus;
  guint32 request_name_result;
  
  session = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
  
  bus = dbus_g_proxy_new_for_name (session,
                                   DBUS_SERVICE_DBUS,
                                   DBUS_PATH_DBUS,
                                   DBUS_INTERFACE_DBUS);
  
  if (!dbus_g_proxy_call (bus, "RequestName", &error,
                          G_TYPE_STRING, SHELL_DBUS_SERVICE,
                          G_TYPE_UINT, 0,
                          G_TYPE_INVALID,
                          G_TYPE_UINT, &request_name_result,
                          G_TYPE_INVALID)) 
    {
      g_print ("failed to acquire org.gnome.Shell: %s\n", error->message);
      /* If we somehow got started again, it's not an error to be running
       * already.  So just exit 0.
       */
      exit (0);  
    }
  
  g_object_unref (bus);
}

void 
shell_global_start_task_panel (ShellGlobal *global)
{
  const char* panel_args[] = {"gnomeshell-taskpanel", SHELL_DBUS_SERVICE, NULL};
  GError *error = NULL;

  if (!g_spawn_async (NULL, (char**)panel_args, NULL, G_SPAWN_SEARCH_PATH, NULL,
                      NULL, NULL, &error)) 
    {
      g_critical ("failed to execute %s: %s", panel_args[0], error->message);
      g_clear_error (&error);
    }
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
 * shell_global_create_vertical_gradient:
 * @top: the color at the top
 * @bottom: the color at the bottom
 *
 * Creates a vertical gradient actor.
 *
 * Return value: (transfer none): a #ClutterCairoTexture actor with the
 *               gradient. The texture actor is floating, hence (transfer none).
 */
ClutterCairoTexture *
shell_global_create_vertical_gradient (ClutterColor *top,
                                       ClutterColor *bottom)
{
  ClutterCairoTexture *texture;
  cairo_t *cr;
  cairo_pattern_t *pattern;

  /* Draw the gradient on an 8x8 pixel texture. Because the gradient is drawn
   * from the uppermost to the lowermost row, after stretching 1/16 of the
   * texture height has the top color and 1/16 has the bottom color. The 8
   * pixel width is chosen for reasons related to graphics hardware internals.
   */
  texture = CLUTTER_CAIRO_TEXTURE (clutter_cairo_texture_new (8, 8));
  cr = clutter_cairo_texture_create (texture);

  pattern = cairo_pattern_create_linear (0, 0, 0, 8);
  cairo_pattern_add_color_stop_rgba (pattern, 0,
                                     top->red / 255.,
                                     top->green / 255.,
                                     top->blue / 255.,
                                     top->alpha / 255.);
  cairo_pattern_add_color_stop_rgba (pattern, 1,
                                     bottom->red / 255.,
                                     bottom->green / 255.,
                                     bottom->blue / 255.,
                                     bottom->alpha / 255.);

  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  cairo_pattern_destroy (pattern);
  cairo_destroy (cr);

  return texture;
}

/*
 * Updates the global->root_pixmap actor with the root window's pixmap or fails
 * with a warning.
 */
static void
update_root_window_pixmap (ShellGlobal *global)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;

  if (!XGetWindowProperty (gdk_x11_get_default_xdisplay (),
                           gdk_x11_get_default_root_xwindow (),
                           gdk_x11_get_xatom_by_name ("_XROOTPMAP_ID"),
                           0, LONG_MAX,
                           False,
                           AnyPropertyType,
                           &type, &format, &nitems, &bytes_after, &data) &&
      type != None)
  {
     /* Got a property. */
     if (type == XA_PIXMAP && format == 32 && nitems == 1)
       {
         /* Was what we expected. */
         clutter_x11_texture_pixmap_set_pixmap (CLUTTER_X11_TEXTURE_PIXMAP (global->root_pixmap),
                                                *(Pixmap *)data);
       }
     else
       {
         g_warning ("Could not get the root window pixmap");
       }

     XFree(data);
  }
}

/*
 * Called when the X server emits a root window change event. If the event is
 * about a new pixmap, update the global->root_pixmap actor.
 */
static GdkFilterReturn
root_window_filter (GdkXEvent *native, GdkEvent *event, gpointer data)
{
  XEvent *xevent = (XEvent *)native;

  if ((xevent->type == PropertyNotify) &&
      (xevent->xproperty.window == gdk_x11_get_default_root_xwindow ()) &&
      (xevent->xproperty.atom == gdk_x11_get_xatom_by_name ("_XROOTPMAP_ID")))
    update_root_window_pixmap (SHELL_GLOBAL (data));

  return GDK_FILTER_CONTINUE;
}

/*
 * Called when the root window pixmap actor is destroyed.
 */
static void
root_pixmap_destroy (GObject *sender, gpointer data)
{
  ShellGlobal *global = SHELL_GLOBAL (data);

  gdk_window_remove_filter (gdk_get_default_root_window (),
                            root_window_filter, global);
  global->root_pixmap = NULL;
}

/**
 * shell_global_create_root_pixmap_actor:
 * @global: a #ShellGlobal
 *
 * Creates an actor showing the root window pixmap.
 *
 * Return value: (transfer none): a #ClutterActor with the root window pixmap.
 *               The actor is floating, hence (transfer none).
 */
ClutterActor *
shell_global_create_root_pixmap_actor (ShellGlobal *global)
{
  GdkWindow *window;
  ClutterActor *stage;

  /* The actor created is actually a ClutterClone of global->root_pixmap. */

  if (global->root_pixmap == NULL)
    {
      global->root_pixmap = clutter_glx_texture_pixmap_new ();

      /* The low and medium quality filters give nearest-neighbor resizing. */
      clutter_texture_set_filter_quality (CLUTTER_TEXTURE (global->root_pixmap),
                                          CLUTTER_TEXTURE_QUALITY_HIGH);

      /* We can only clone an actor within a stage, so we hide the source
       * texture then add it to the stage */
      clutter_actor_hide (global->root_pixmap);
      stage = mutter_plugin_get_stage (global->plugin);
      clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                                   global->root_pixmap);

      /* This really should never happen; but just in case... */
      g_signal_connect (global->root_pixmap, "destroy",
                        G_CALLBACK (root_pixmap_destroy), global);

      /* Metacity handles changes to some root window properties in its global
       * event filter, though not _XROOTPMAP_ID. For all root window property
       * changes, the global filter returns GDK_FILTER_CONTINUE, so our
       * window specific filter will be called after the global one.
       *
       * Because Metacity is already handling root window property updates,
       * we don't have to worry about adding the PropertyChange mask to the
       * root window to get PropertyNotify events.
       */
      window = gdk_get_default_root_window ();
      gdk_window_add_filter (window, root_window_filter, global);

      update_root_window_pixmap (global);
    }

  return clutter_clone_new (global->root_pixmap);
}

void
shell_global_clutter_cairo_texture_draw_clock (ClutterCairoTexture *texture,
                                               int                  hour,
                                               int                  minute)
{
  cairo_t *cr;
  guint width, height;
  double xc, yc, radius, hour_radius, minute_radius;
  double angle;

  clutter_cairo_texture_get_surface_size (texture, &width, &height);
  xc = (double)width / 2;
  yc = (double)height / 2;
  radius = (double)(MIN(width, height)) / 2 - 2;
  minute_radius = radius - 3;
  hour_radius = radius / 2;

  clutter_cairo_texture_clear (texture);
  cr = clutter_cairo_texture_create (texture);
  cairo_set_line_width (cr, 1.0);

  /* Outline */
  cairo_arc (cr, xc, yc, radius, 0.0, 2.0 * M_PI);
  cairo_stroke (cr);

  /* Hour hand. (We add a fraction to @hour for the minutes, then
   * convert to radians, and then subtract pi/2 because cairo's origin
   * is at 3:00, not 12:00.)
   */
  angle = ((hour + minute / 60.0) / 12.0) * 2.0 * M_PI - M_PI / 2.0;
  cairo_move_to (cr, xc, yc);
  cairo_line_to (cr,
                 xc + hour_radius * cos (angle),
                 yc + hour_radius * sin (angle));
  cairo_stroke (cr);

  /* Minute hand */
  angle = (minute / 60.0) * 2.0 * M_PI - M_PI / 2.0;
  cairo_move_to (cr, xc, yc);
  cairo_line_to (cr,
                 xc + minute_radius * cos (angle),
                 yc + minute_radius * sin (angle));
  cairo_stroke (cr);

  cairo_destroy (cr);
}
