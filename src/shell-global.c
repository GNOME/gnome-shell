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
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
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
      g_value_set_object (value, shell_global_get_screen (global));
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
 * shell_global_get_screen:
 *
 * Return value: (transfer none): The default #MetaScreen
 */
MetaScreen *
shell_global_get_screen (ShellGlobal  *global)
{
  return mutter_plugin_get_screen (global->plugin);
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
  ClutterStage *stage = CLUTTER_STAGE (mutter_plugin_get_stage (global->plugin));
  Window stagewin = clutter_x11_get_stage_window (stage);

  return mutter_plugin_begin_modal (global->plugin, stagewin, None, 0, timestamp);
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
  mutter_plugin_end_modal (global->plugin, timestamp);
}

/**
 * shell_global_display_is_grabbed
 * @global: a #ShellGlobal
 *
 * Determines whether Mutter currently has a grab (keyboard or mouse or
 * both) on the display. This could be the result of a current window
 * management operation like a window move, or could be from
 * shell_global_begin_modal().
 *
 * This function is useful to for ad-hoc checks to avoid over-grabbing
 * the Mutter grab a grab from GTK+. Longer-term we might instead want a
 * mechanism to make Mutter use GDK grabs instead of raw XGrabPointer().
 *
 * Return value: %TRUE if Mutter has a grab on the display
 */
gboolean
shell_global_display_is_grabbed (ShellGlobal *global)
{
  MetaScreen *screen = mutter_plugin_get_screen (global->plugin);
  MetaDisplay *display = meta_screen_get_display (screen);

  return meta_display_get_grab_op (display) != META_GRAB_OP_NONE;
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

  /* Also grab org.gnome.Panel to replace any existing panel process,
   * unless a special environment variable is passed.  The environment
   * variable is used by the gnome-shell (no --replace) launcher in
   * Xephyr */
  if (!g_getenv ("GNOME_SHELL_NO_REPLACE_PANEL"))
    {
      if (!dbus_g_proxy_call (bus, "RequestName", &error, G_TYPE_STRING,
                              "org.gnome.Panel", G_TYPE_UINT,
                              DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE,
                              G_TYPE_INVALID, G_TYPE_UINT,
                              &request_name_result, G_TYPE_INVALID))
        {
          g_print ("failed to acquire org.gnome.Panel: %s\n", error->message);
          exit (1);
        }
    }
  g_object_unref (bus);
}

static void 
grab_notify (GtkWidget *widget, gboolean was_grabbed, gpointer user_data)
{
  ShellGlobal *global = SHELL_GLOBAL (user_data);
  
  global->gtk_grab_active = !was_grabbed;

  /* Update for the new setting of gtk_grab_active */
  shell_global_set_stage_input_mode (global, global->input_mode);
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
  Pixmap root_pixmap_id = None;

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
         root_pixmap_id = *(Pixmap *)data;
       }
     else
       {
         g_warning ("Could not get the root window pixmap");
       }

     XFree(data);
  }

  clutter_x11_texture_pixmap_set_pixmap (CLUTTER_X11_TEXTURE_PIXMAP (global->root_pixmap),
                                         root_pixmap_id);
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

/* Workaround for a clutter bug where if ClutterGLXTexturePixmap
 * is painted without the pixmap being set, a crash will occur inside
 * Cogl.
 *
 * http://bugzilla.openedhand.com/show_bug.cgi?id=1644
 */
static void
root_pixmap_paint (ClutterActor *actor, gpointer data)
{
  Pixmap pixmap;

  g_object_get (G_OBJECT (actor), "pixmap", &pixmap, NULL);
  if (!pixmap)
    g_signal_stop_emission_by_name (actor, "paint");
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
 * shell_global_format_time_relative_pretty:
 * @global:
 * @delta: Time in seconds since the current time
 * @text: (out): Relative human-consumption-only time string
 * @next_update: (out): Time in seconds until we should redisplay the time
 *
 * Format a time value for human consumption only.  The passed time
 * value is a delta in terms of seconds from the current time.
 * This function needs to be in C because of its use of ngettext() which
 * is not accessible from JavaScript.
 */
void
shell_global_format_time_relative_pretty (ShellGlobal *global,
                                          guint        delta,
                                          char       **text,
                                          guint       *next_update)
{
#define MINUTE (60)
#define HOUR (MINUTE*60)
#define DAY (HOUR*24)
#define WEEK (DAY*7)
  if (delta < MINUTE) {
    *text = g_strdup (_("Less than a minute ago"));
    *next_update = MINUTE - delta;
   } else if (delta < HOUR) {
     *text = g_strdup_printf (ngettext ("%d minute ago", "%d minutes ago", delta / MINUTE), delta / MINUTE);
     *next_update = MINUTE - (delta % MINUTE);
   } else if (delta < DAY) {
     *text = g_strdup_printf (ngettext ("%d hour ago", "%d hours ago", delta / HOUR), delta / HOUR);
     *next_update = HOUR - (delta % HOUR);
   } else if (delta < WEEK) {
     *text = g_strdup_printf (ngettext ("%d day ago", "%d days ago", delta / DAY), delta / DAY);
     *next_update = DAY - (delta % DAY);
   } else {
     *text = g_strdup_printf (ngettext ("%d week ago", "%d weeks ago", delta / WEEK), delta / WEEK);
     *next_update = WEEK - (delta % WEEK);
   }
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

      g_signal_connect (global->root_pixmap, "paint",
                        G_CALLBACK (root_pixmap_paint), NULL);

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
