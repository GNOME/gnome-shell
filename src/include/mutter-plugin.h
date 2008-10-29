/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (c) 2008 Intel Corp.
 *
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

#ifndef MUTTER_PLUGIN_H_
#define MUTTER_PLUGIN_H_

#include "types.h"
#include "compositor.h"
#include "compositor-mutter.h"

#include <clutter/clutter.h>
#include <X11/extensions/Xfixes.h>

/*
 * This file defines the plugin API.
 *
 * Effects plugin is shared library loaded via g_module_open(); it is
 * recommended that the GModule API is used (otherwise you are on your own to
 * do proper plugin clean up when the module is unloaded).
 *
 * The plugin interface is exported via the MutterPlugin struct.
 */

typedef struct MutterPlugin MutterPlugin;

/*
 * Effect flags: identify events that the plugin can handle, used by kill_effect
 * function.
 */
#define MUTTER_PLUGIN_MINIMIZE         (1<<0)
#define MUTTER_PLUGIN_MAXIMIZE         (1<<1)
#define MUTTER_PLUGIN_UNMAXIMIZE       (1<<2)
#define MUTTER_PLUGIN_MAP              (1<<3)
#define MUTTER_PLUGIN_DESTROY          (1<<4)
#define MUTTER_PLUGIN_SWITCH_WORKSPACE (1<<5)

#define MUTTER_PLUGIN_ALL_EFFECTS      (~0)

#define MUTTER_DECLARE_PLUGIN() G_MODULE_EXPORT MutterPlugin mutter_plugin = \
    {                                                                   \
      METACITY_MAJOR_VERSION,                                           \
      METACITY_MINOR_VERSION,                                           \
      METACITY_MICRO_VERSION,                                           \
      METACITY_CLUTTER_PLUGIN_API_VERSION                               \
    };                                                                  \
  static inline MutterPlugin * mutter_get_plugin ()                     \
  {                                                                     \
    return &mutter_plugin;                                              \
  }

struct MutterPlugin
{
  /*
   * Version information; the first three numbers match the Metacity version
   * with which the plugin was compiled (see clutter-plugins/simple.c for sample
   * code).
   */
  guint version_major;
  guint version_minor;
  guint version_micro;

  /*
   * Version of the plugin API; this is unrelated to the matacity version
   * per se. The API version is checked by the plugin manager and must match
   * the one used by it (see clutter-plugins/simple.c for sample code).
   */
  guint version_api;

#ifndef MUTTER_BUILDING_PLUGIN
  const
#endif
  gchar   *name;     /* Human-readable name for UI */

  /*
   * This function is called once the plugin has been loaded.
   *
   * @params is a string containing additional parameters for the plugin and is
   * specified after the plugin name in the gconf database, separated by a
   * colon.
   *
   * The following parameter tokens need to be handled by all
   * plugins:
   *
   *   'debug'
   *             Indicates running in debug mode; the plugin
   *             might want to print useful debug info, or
   *             extend effect duration, etc.
   *
   *   'disable: ...;'
   *
   *             The disable token indicates that the effects
   *             listed after the colon should be disabled.
   *
   *             The list is comma-separated, terminated by a
   *             semicolon and consisting of the following
   *             tokens:
   *
   *                minimize
   *                maximize
   *                unmaximize
   *                map
   *                destroy
   *                switch-workspace
   *
   *   FIXME: ^^^ Instead of configuring in terms of what should be
   *   disabled, and needing a mechanism for coping with the user
   *   mistakenly not disabling the right things, it might be neater
   *   if plugins were enabled on a per effect basis in the first
   *   place. I.e. in gconf we could have effect:plugin key value
   *   pairs.
   */

  gboolean (*do_init) (const char *params);

  /*
   * Event handlers
   *
   * Plugins must not make any special assumptions about the nature of
   * ClutterActor, as the implementation details can change.
   *
   * Plugins must restore actor properties on completion (i.e., fade effects
   * must restore opacity back to the original value, scale effects scale,
   * etc.).
   *
   * On completion, each event handler must call the manager completed()
   * callback function.
   */
  void (*minimize)         (MutterWindow     *actor);

  void (*maximize)         (MutterWindow     *actor,
                            gint                x,
                            gint                y,
                            gint                width,
                            gint                height);

  void (*unmaximize)       (MutterWindow     *actor,
                            gint                x,
                            gint                y,
                            gint                width,
                            gint                height);

  void (*map)              (MutterWindow     *actor);

  void (*destroy)          (MutterWindow     *actor);

  /*
   * Each actor in the list has a workspace number attached to it using
   * g_object_set_data() with key MUTTER_PLUGIN_WORKSPACE_KEY;
   * workspace < 0 indicates the window is sticky (i.e., on all desktops).
   * TODO: Add accessor for sticky bit in new MutterWindow structure
   */
  void (*switch_workspace) (const GList       **actors,
                            gint                from,
                            gint                to,
                            MetaMotionDirection direction);

  /*
   * Called if an effect should be killed prematurely; the plugin must
   * call the completed() callback as if the effect terminated naturally.
   * The events parameter is a bitmask indicating which effects are to be
   * killed.
   */
  void (*kill_effect)      (MutterWindow     *actor,
                            gulong            events);

  /*
   * The plugin manager will call this function when module should be reloaded.
   * This happens, for example, when the parameters for the plugin changed.
   */
  gboolean (*reload) (const char *params);

  /* General XEvent filter. This is fired *before* metacity itself handles
   * an event. Return TRUE to block any further processing.
   */
  gboolean (*xevent_filter) (XEvent *event);

  /* List of PluginWorkspaceRectangles defining the geometry of individual
   * workspaces. */
  GList *work_areas;

  void  *plugin_private; /* Plugin private data go here; use the plugin init
                          * function to allocate and initialize any private
                          * data.
                          */

  /* Private; manager private data. */
  void *manager_private;
};

#ifndef MUTTER_PLUGIN_FROM_MANAGER_
static inline MutterPlugin *mutter_get_plugin ();
#endif

void
mutter_plugin_effect_completed (MutterPlugin  *plugin,
                                MutterWindow  *actor,
                                unsigned long  event);

ClutterActor *
mutter_plugin_get_overlay_group (MutterPlugin *plugin);

ClutterActor *
mutter_plugin_get_window_group (MutterPlugin *plugin);

ClutterActor *
mutter_plugin_get_stage (MutterPlugin *plugin);

void
mutter_plugin_query_screen_size (MutterPlugin *plugin,
                                 int          *width,
                                 int          *height);

ClutterActor *
mutter_plugin_get_overlay_group (MutterPlugin *plugin);

ClutterActor *
mutter_plugin_get_stage (MutterPlugin *plugin);

void
mutter_plugin_set_stage_reactive (MutterPlugin *plugin,
                                  gboolean      reactive);

void
mutter_plugin_set_stage_input_area (MutterPlugin *plugin,
                                    gint x, gint y, gint width, gint height);

void
mutter_plugin_set_stage_input_region (MutterPlugin *plugin,
                                      XserverRegion region);

GList *
mutter_plugin_get_windows (MutterPlugin *plugin);

#endif /* MUTTER_PLUGIN_H_ */
