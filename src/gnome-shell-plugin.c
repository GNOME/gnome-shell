/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (c) 2008 Red Hat, Inc.
 * Copyright (c) 2008 Intel Corp.
 *
 * Based on plugin skeleton by:
 * Author: Tomas Frydrych <tf@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#define MUTTER_BUILDING_PLUGIN 1
#include <mutter-plugin.h>

#include <glib/gi18n-lib.h>

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gjs/gjs.h>
#include <girepository.h>
#include <gmodule.h>
#ifdef HAVE_MALLINFO
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>

#include <GL/glx.h>
#include <GL/glxext.h>

#include "display.h"

#include "shell-global-private.h"
#include "shell-perf-log.h"
#include "shell-wm-private.h"
#include "st.h"

static void gnome_shell_plugin_dispose     (GObject *object);
static void gnome_shell_plugin_finalize    (GObject *object);

static void     gnome_shell_plugin_start            (MutterPlugin         *plugin);
static void     gnome_shell_plugin_minimize         (MutterPlugin         *plugin,
                                                     MutterWindow         *actor);
static void     gnome_shell_plugin_maximize         (MutterPlugin         *plugin,
                                                     MutterWindow         *actor,
                                                     gint                  x,
                                                     gint                  y,
                                                     gint                  width,
                                                     gint                  height);
static void     gnome_shell_plugin_unmaximize       (MutterPlugin         *plugin,
                                                     MutterWindow         *actor,
                                                     gint                  x,
                                                     gint                  y,
                                                     gint                  width,
                                                     gint                  height);
static void     gnome_shell_plugin_map              (MutterPlugin         *plugin,
                                                     MutterWindow         *actor);
static void     gnome_shell_plugin_destroy          (MutterPlugin         *plugin,
                                                     MutterWindow         *actor);

static void     gnome_shell_plugin_switch_workspace (MutterPlugin         *plugin,
                                                     gint                  from,
                                                     gint                  to,
                                                     MetaMotionDirection   direction);

static void     gnome_shell_plugin_kill_window_effects   (MutterPlugin         *plugin,
                                                          MutterWindow         *actor);
static void     gnome_shell_plugin_kill_switch_workspace (MutterPlugin         *plugin);

static gboolean                gnome_shell_plugin_xevent_filter (MutterPlugin *plugin,
                                                                 XEvent       *event);
static const MutterPluginInfo *gnome_shell_plugin_plugin_info   (MutterPlugin *plugin);

#define GNOME_TYPE_SHELL_PLUGIN            (gnome_shell_plugin_get_type ())
#define GNOME_SHELL_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_SHELL_PLUGIN, GnomeShellPlugin))
#define GNOME_SHELL_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GNOME_TYPE_SHELL_PLUGIN, GnomeShellPluginClass))
#define GNOME_IS_SHELL_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_SHELL_PLUGIN_TYPE))
#define GNOME_IS_SHELL_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GNOME_TYPE_SHELL_PLUGIN))
#define GNOME_SHELL_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GNOME_TYPE_SHELL_PLUGIN, GnomeShellPluginClass))

typedef struct _GnomeShellPlugin        GnomeShellPlugin;
typedef struct _GnomeShellPluginClass   GnomeShellPluginClass;

struct _GnomeShellPlugin
{
  MutterPlugin parent;

  GjsContext *gjs_context;
  Atom panel_action;
  Atom panel_action_run_dialog;
  Atom panel_action_main_menu;

  int glx_error_base;
  int glx_event_base;
  guint have_swap_event : 1;
};

struct _GnomeShellPluginClass
{
  MutterPluginClass parent_class;
};

/*
 * Create the plugin struct; function pointers initialized in
 * g_module_check_init().
 */
MUTTER_PLUGIN_DECLARE(GnomeShellPlugin, gnome_shell_plugin);

static void
gnome_shell_plugin_class_init (GnomeShellPluginClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  MutterPluginClass *plugin_class  = MUTTER_PLUGIN_CLASS (klass);

  gobject_class->dispose         = gnome_shell_plugin_dispose;
  gobject_class->finalize        = gnome_shell_plugin_finalize;

  plugin_class->start            = gnome_shell_plugin_start;
  plugin_class->map              = gnome_shell_plugin_map;
  plugin_class->minimize         = gnome_shell_plugin_minimize;
  plugin_class->maximize         = gnome_shell_plugin_maximize;
  plugin_class->unmaximize       = gnome_shell_plugin_unmaximize;
  plugin_class->destroy          = gnome_shell_plugin_destroy;

  plugin_class->switch_workspace = gnome_shell_plugin_switch_workspace;

  plugin_class->kill_window_effects   = gnome_shell_plugin_kill_window_effects;
  plugin_class->kill_switch_workspace = gnome_shell_plugin_kill_switch_workspace;

  plugin_class->xevent_filter    = gnome_shell_plugin_xevent_filter;
  plugin_class->plugin_info      = gnome_shell_plugin_plugin_info;
}

static void
gnome_shell_plugin_init (GnomeShellPlugin *shell_plugin)
{
  g_setenv ("XDG_MENU_PREFIX", "gs-", TRUE);
  meta_prefs_override_preference_location ("/apps/mutter/general/attach_modal_dialogs",
                                           "/desktop/gnome/shell/windows/attach_modal_dialogs");
  meta_prefs_override_preference_location ("/apps/metacity/general/button_layout",
                                           "/desktop/gnome/shell/windows/button_layout");
  meta_prefs_override_preference_location ("/apps/metacity/general/side_by_side_tiling",
                                           "/desktop/gnome/shell/windows/side_by_side_tiling");
}

static void
update_font_options (GtkSettings *settings)
{
  StThemeContext *context;
  ClutterStage *stage;
  ClutterBackend *backend;
  gint dpi;
  gint hinting;
  gchar *hint_style_str;
  cairo_hint_style_t hint_style = CAIRO_HINT_STYLE_NONE;
  gint antialias;
  cairo_antialias_t antialias_mode = CAIRO_ANTIALIAS_NONE;
  cairo_font_options_t *options;

  /* Disable text mipmapping; it causes problems on pre-GEM Intel
   * drivers and we should just be rendering text at the right
   * size rather than scaling it. If we do effects where we dynamically
   * zoom labels, then we might want to reconsider.
   */
  clutter_set_font_flags (clutter_get_font_flags () & ~CLUTTER_FONT_MIPMAPPING);

  g_object_get (settings,
                "gtk-xft-dpi", &dpi,
                "gtk-xft-antialias", &antialias,
                "gtk-xft-hinting", &hinting,
                "gtk-xft-hintstyle", &hint_style_str,
                NULL);

  stage = CLUTTER_STAGE (clutter_stage_get_default ());
  context = st_theme_context_get_for_stage (stage);

  if (dpi != -1)
    /* GTK stores resolution as 1024 * dots/inch */
    st_theme_context_set_resolution (context, dpi / 1024);
  else
    st_theme_context_set_default_resolution (context);

  /* Clutter (as of 0.9) passes comprehensively wrong font options
   * override whatever set_font_flags() did above.
   *
   * http://bugzilla.openedhand.com/show_bug.cgi?id=1456
   */
  backend = clutter_get_default_backend ();
  options = cairo_font_options_create ();

  cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_ON);

  if (hinting >= 0 && !hinting)
    {
      hint_style = CAIRO_HINT_STYLE_NONE;
    }
  else if (hint_style_str)
    {
      if (strcmp (hint_style_str, "hintnone") == 0)
        hint_style = CAIRO_HINT_STYLE_NONE;
      else if (strcmp (hint_style_str, "hintslight") == 0)
        hint_style = CAIRO_HINT_STYLE_SLIGHT;
      else if (strcmp (hint_style_str, "hintmedium") == 0)
        hint_style = CAIRO_HINT_STYLE_MEDIUM;
      else if (strcmp (hint_style_str, "hintfull") == 0)
        hint_style = CAIRO_HINT_STYLE_FULL;
    }

  g_free (hint_style_str);

  cairo_font_options_set_hint_style (options, hint_style);

  /* We don't want to turn on subpixel anti-aliasing; since Clutter
   * doesn't currently have the code to support ARGB masks,
   * generating them then squashing them back to A8 is pointless.
   */
  antialias_mode = (antialias < 0 || antialias) ? CAIRO_ANTIALIAS_GRAY
                                                : CAIRO_ANTIALIAS_NONE;

  cairo_font_options_set_antialias (options, antialias_mode);

  clutter_backend_set_font_options (backend, options);
  cairo_font_options_destroy (options);
}

static void
settings_notify_cb (GtkSettings *settings,
                    GParamSpec  *pspec,
                    gpointer     data)
{
  update_font_options (settings);
}

static void
malloc_statistics_callback (ShellPerfLog *perf_log,
                            gpointer      data)
{
#ifdef HAVE_MALLINFO
  struct mallinfo info = mallinfo ();

  shell_perf_log_update_statistic_i (perf_log,
                                     "malloc.arenaSize",
                                     info.arena);
  shell_perf_log_update_statistic_i (perf_log,
                                     "malloc.mmapSize",
                                     info.hblkhd);
  shell_perf_log_update_statistic_i (perf_log,
                                     "malloc.usedSize",
                                     info.uordblks);
#endif
}

static void
add_statistics (GnomeShellPlugin *shell_plugin)
{
  ShellPerfLog *perf_log = shell_perf_log_get_default ();

  /* For probably historical reasons, mallinfo() defines the returned values,
   * even those in bytes as int, not size_t. We're determined not to use
   * more than 2G of malloc'ed memory, so are OK with that.
   */
  shell_perf_log_define_statistic (perf_log,
                                   "malloc.arenaSize",
                                   "Amount of memory allocated by malloc() with brk(), in bytes",
                                   "i");
  shell_perf_log_define_statistic (perf_log,
                                   "malloc.mmapSize",
                                   "Amount of memory allocated by malloc() with mmap(), in bytes",
                                   "i");
  shell_perf_log_define_statistic (perf_log,
                                   "malloc.usedSize",
                                   "Amount of malloc'ed memory currently in use",
                                   "i");

  shell_perf_log_add_statistics_callback (perf_log,
                                          malloc_statistics_callback,
                                          NULL, NULL);
}

static void
gnome_shell_plugin_start (MutterPlugin *plugin)
{
  GnomeShellPlugin *shell_plugin = GNOME_SHELL_PLUGIN (plugin);
  MetaScreen *screen;
  MetaDisplay *display;
  Display *xdisplay;
  GtkSettings *settings;
  GError *error = NULL;
  int status;
  const char *shell_js;
  char **search_path;
  ShellGlobal *global;
  const char *glx_extensions;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  settings = gtk_settings_get_default ();
  g_object_connect (settings,
                    "signal::notify::gtk-xft-dpi",
                    G_CALLBACK (settings_notify_cb), NULL,
                    "signal::notify::gtk-xft-antialias",
                    G_CALLBACK (settings_notify_cb), NULL,
                    "signal::notify::gtk-xft-hinting",
                    G_CALLBACK (settings_notify_cb), NULL,
                    "signal::notify::gtk-xft-hintstyle",
                    G_CALLBACK (settings_notify_cb), NULL,
                    NULL);
  update_font_options (settings);

  screen = mutter_plugin_get_screen (plugin);
  display = meta_screen_get_display (screen);

  xdisplay = meta_display_get_xdisplay (display);

  glXQueryExtension (xdisplay,
                     &shell_plugin->glx_error_base,
                     &shell_plugin->glx_event_base);

  glx_extensions = glXQueryExtensionsString (xdisplay,
                                             meta_screen_get_screen_number (screen));
  shell_plugin->have_swap_event = strstr (glx_extensions, "GLX_INTEL_swap_event") != NULL;

  shell_perf_log_define_event (shell_perf_log_get_default (),
                               "glx.swapComplete",
                               "GL buffer swap complete event received (with timestamp of completion)",
                               "x");

  g_irepository_prepend_search_path (GNOME_SHELL_PKGLIBDIR);

  shell_js = g_getenv("GNOME_SHELL_JS");
  if (!shell_js)
    shell_js = JSDIR;

  search_path = g_strsplit(shell_js, ":", -1);
  shell_plugin->gjs_context = gjs_context_new_with_search_path(search_path);
  g_strfreev(search_path);

  /* Initialize the global object here. */
  global = shell_global_get ();

  _shell_global_set_plugin (global, MUTTER_PLUGIN(shell_plugin));
  _shell_global_set_gjs_context (global, shell_plugin->gjs_context);

  add_statistics (shell_plugin);

  if (!gjs_context_eval (shell_plugin->gjs_context,
                         "const Main = imports.ui.main; Main.start();",
                         -1,
                         "<main>",
                         &status,
                         &error))
    {
      g_message ("Execution of main.js threw exception: %s", error->message);
      g_error_free (error);
      /* We just exit() here, since in a development environment you'll get the
       * error in your shell output, and it's way better than a busted WM,
       * which typically manifests as a white screen.
       *
       * In production, we shouldn't crash =)  But if we do, we should get
       * restarted by the session infrastructure, which is likely going
       * to be better than some undefined state.
       *
       * If there was a generic "hook into bug-buddy for non-C crashes"
       * infrastructure, here would be the place to put it.
       */
      exit (1);
    }
}

static void
gnome_shell_plugin_dispose (GObject *object)
{
  G_OBJECT_CLASS(gnome_shell_plugin_parent_class)->dispose (object);
}

static void
gnome_shell_plugin_finalize (GObject *object)
{
  G_OBJECT_CLASS(gnome_shell_plugin_parent_class)->finalize (object);
}

static ShellWM *
get_shell_wm (void)
{
  ShellWM *wm;

  g_object_get (shell_global_get (),
                "window-manager", &wm,
                NULL);
  /* drop extra ref added by g_object_get */
  g_object_unref (wm);

  return wm;
}

static void
gnome_shell_plugin_minimize (MutterPlugin         *plugin,
			     MutterWindow         *actor)
{
  _shell_wm_minimize (get_shell_wm (),
                      actor);

}

static void
gnome_shell_plugin_maximize (MutterPlugin         *plugin,
                             MutterWindow         *actor,
                             gint                  x,
                             gint                  y,
                             gint                  width,
                             gint                  height)
{
  _shell_wm_maximize (get_shell_wm (),
                      actor, x, y, width, height);
}

static void
gnome_shell_plugin_unmaximize (MutterPlugin         *plugin,
                               MutterWindow         *actor,
                               gint                  x,
                               gint                  y,
                               gint                  width,
                               gint                  height)
{
  _shell_wm_unmaximize (get_shell_wm (),
                        actor, x, y, width, height);
}

static void
gnome_shell_plugin_map (MutterPlugin         *plugin,
                        MutterWindow         *actor)
{
  _shell_wm_map (get_shell_wm (),
                 actor);
}

static void
gnome_shell_plugin_destroy (MutterPlugin         *plugin,
                            MutterWindow         *actor)
{
  _shell_wm_destroy (get_shell_wm (),
                     actor);
}

static void
gnome_shell_plugin_switch_workspace (MutterPlugin         *plugin,
                                     gint                  from,
                                     gint                  to,
                                     MetaMotionDirection   direction)
{
  _shell_wm_switch_workspace (get_shell_wm(), from, to, direction);
}

static void
gnome_shell_plugin_kill_window_effects (MutterPlugin         *plugin,
                                        MutterWindow         *actor)
{
  _shell_wm_kill_window_effects (get_shell_wm(), actor);
}

static void
gnome_shell_plugin_kill_switch_workspace (MutterPlugin         *plugin)
{
  _shell_wm_kill_switch_workspace (get_shell_wm());
}

static gboolean
gnome_shell_plugin_xevent_filter (MutterPlugin *plugin,
                                  XEvent       *xev)
{
#ifdef GLX_INTEL_swap_event
  GnomeShellPlugin *shell_plugin = GNOME_SHELL_PLUGIN (plugin);

  if (shell_plugin->have_swap_event &&
      xev->type == (shell_plugin->glx_event_base + GLX_BufferSwapComplete))
    {
      GLXBufferSwapComplete *swap_complete_event;
      swap_complete_event = (GLXBufferSwapComplete *)xev;

      /* Buggy early versions of the INTEL_swap_event implementation in Mesa
       * can send this with a ust of 0. Simplify life for consumers
       * by ignoring such events */
      if (swap_complete_event->ust != 0)
        shell_perf_log_event_x (shell_perf_log_get_default (),
                                "glx.swapComplete",
                                swap_complete_event->ust);
    }
#endif

  return clutter_x11_handle_event (xev) != CLUTTER_X11_FILTER_CONTINUE;
}

static const
MutterPluginInfo *gnome_shell_plugin_plugin_info (MutterPlugin *plugin)
{
  static const MutterPluginInfo info = {
    .name = "GNOME Shell",
    .version = "0.1",
    .author = "Various",
    .license = "GPLv2+",
    .description = "Provides GNOME Shell core functionality"
  };

  return &info;
}
