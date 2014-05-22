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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_PLUGIN_H_
#define META_PLUGIN_H_

#include <meta/types.h>
#include <meta/compositor.h>
#include <meta/compositor-mutter.h>

#include <clutter/clutter.h>
#include <X11/extensions/Xfixes.h>
#include <gmodule.h>

#define META_TYPE_PLUGIN            (meta_plugin_get_type ())
#define META_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_PLUGIN, MetaPlugin))
#define META_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_PLUGIN, MetaPluginClass))
#define META_IS_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_PLUGIN))
#define META_IS_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_PLUGIN))
#define META_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_PLUGIN, MetaPluginClass))

typedef struct _MetaPlugin        MetaPlugin;
typedef struct _MetaPluginClass   MetaPluginClass;
typedef struct _MetaPluginVersion MetaPluginVersion;
typedef struct _MetaPluginInfo    MetaPluginInfo;
typedef struct _MetaPluginPrivate MetaPluginPrivate;

struct _MetaPlugin
{
  GObject parent;

  MetaPluginPrivate *priv;
};

/**
 * MetaPluginClass:
 * @start: virtual function called when the compositor starts managing a screen
 * @minimize: virtual function called when a window is minimized
 * @maximize: virtual function called when a window is maximized
 * @unmaximize: virtual function called when a window is unmaximized
 * @map: virtual function called when a window is mapped
 * @destroy: virtual function called when a window is destroyed
 * @switch_workspace: virtual function called when the user switches to another
 * workspace
 * @kill_window_effects: virtual function called when the effects on a window
 * need to be killed prematurely; the plugin must call the completed() callback
 * as if the effect terminated naturally
 * @kill_switch_workspace: virtual function called when the workspace-switching
 * effect needs to be killed prematurely
 * @xevent_filter: virtual function called when handling each event
 * @keybinding_filter: virtual function called when handling each keybinding
 * @plugin_info: virtual function that returns information about the
 * #MetaPlugin
 */
struct _MetaPluginClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/

  /**
   * MetaPluginClass::start:
   *
   * Virtual function called when the compositor starts managing a screen
   */
  void (*start)            (MetaPlugin         *plugin);

  /**
   * MetaPluginClass::minimize:
   * @actor: a #MetaWindowActor
   *
   * Virtual function called when the window represented by @actor is minimized.
   */
  void (*minimize)         (MetaPlugin         *plugin,
                            MetaWindowActor    *actor);

  /**
   * MetaPluginClass::maximize:
   * @actor: a #MetaWindowActor
   * @x: target X coordinate
   * @y: target Y coordinate
   * @width: target width
   * @height: target height
   *
   * Virtual function called when the window represented by @actor is maximized.
   */
  void (*maximize)         (MetaPlugin         *plugin,
                            MetaWindowActor    *actor,
                            gint                x,
                            gint                y,
                            gint                width,
                            gint                height);

  /**
   * MetaPluginClass::unmaximize:
   * @actor: a #MetaWindowActor
   * @x: target X coordinate
   * @y: target Y coordinate
   * @width: target width
   * @height: target height
   *
   * Virtual function called when the window represented by @actor is unmaximized.
   */
  void (*unmaximize)       (MetaPlugin         *plugin,
                            MetaWindowActor    *actor,
                            gint                x,
                            gint                y,
                            gint                width,
                            gint                height);

  /**
   * MetaPluginClass::map:
   * @actor: a #MetaWindowActor
   *
   * Virtual function called when the window represented by @actor is mapped.
   */
  void (*map)              (MetaPlugin         *plugin,
                            MetaWindowActor    *actor);

  /**
   * MetaPluginClass::destroy:
   * @actor: a #MetaWindowActor
   *
   * Virtual function called when the window represented by @actor is destroyed.
   */
  void (*destroy)          (MetaPlugin         *plugin,
                            MetaWindowActor    *actor);

  /**
   * MetaPluginClass::switch_workspace:
   * @from: origin workspace
   * @to: destination workspace
   * @direction: a #MetaMotionDirection
   *
   * Virtual function called when the window represented by @actor is destroyed.
   */
  void (*switch_workspace) (MetaPlugin         *plugin,
                            gint                from,
                            gint                to,
                            MetaMotionDirection direction);

  void (*show_tile_preview) (MetaPlugin      *plugin,
                             MetaWindow      *window,
                             MetaRectangle   *tile_rect,
                             int              tile_monitor_number);
  void (*hide_tile_preview) (MetaPlugin      *plugin);

  void (*show_window_menu)  (MetaPlugin      *plugin,
                             MetaWindow      *window,
                             int              x,
                             int              y);

  /**
   * MetaPluginClass::kill_window_effects:
   * @actor: a #MetaWindowActor
   *
   * Virtual function called when the effects on @actor need to be killed
   * prematurely; the plugin must call the completed() callback as if the effect
   * terminated naturally.
   */
  void (*kill_window_effects)      (MetaPlugin      *plugin,
                                    MetaWindowActor *actor);

  /**
   * MetaPluginClass::kill_switch_workspace:
   *
   * Virtual function called when the workspace-switching effect needs to be
   * killed prematurely.
   */
  void (*kill_switch_workspace)    (MetaPlugin     *plugin);

  /**
   * MetaPluginClass::xevent_filter:
   * @event: (type xlib.XEvent):
   *
   * Virtual function called when handling each event.
   *
   * Returns: %TRUE if the plugin handled the event type (i.e., if the return
   * value is %FALSE, there will be no subsequent call to the manager
   * completed() callback, and the compositor must ensure that any appropriate
   * post-effect cleanup is carried out.
   */
  gboolean (*xevent_filter) (MetaPlugin       *plugin,
                             XEvent           *event);

  /**
   * MetaPluginClass::keybinding_filter:
   * @binding: a #MetaKeyBinding
   *
   * Virtual function called when handling each keybinding.
   *
   * Returns: %TRUE if the plugin handled the keybinding.
   */
  gboolean (*keybinding_filter) (MetaPlugin     *plugin,
                                 MetaKeyBinding *binding);

  /**
   * MetaPluginClass::confirm_display_config:
   * @plugin: a #MetaPlugin
   *
   * Virtual function called when the display configuration changes.
   * The common way to implement this function is to show some form
   * of modal dialog that should ask the user if everything was ok.
   *
   * When confirmed by the user, the plugin must call meta_plugin_complete_display_change()
   * to make the configuration permanent. If that function is not
   * called within the timeout, the previous configuration will be
   * reapplied.
   */
  void (*confirm_display_change) (MetaPlugin *plugin);

  /**
   * MetaPluginClass::plugin_info:
   * @plugin: a #MetaPlugin
   *
   * Virtual function that returns information about the #MetaPlugin.
   *
   * Returns: a #MetaPluginInfo.
   */
  const MetaPluginInfo * (*plugin_info) (MetaPlugin *plugin);

};

/**
 * MetaPluginInfo:
 * @name: name of the plugin
 * @version: version of the plugin
 * @author: author of the plugin
 * @license: license of the plugin
 * @description: description of the plugin
 */
struct _MetaPluginInfo
{
  const gchar *name;
  const gchar *version;
  const gchar *author;
  const gchar *license;
  const gchar *description;
};

GType meta_plugin_get_type (void);

const MetaPluginInfo * meta_plugin_get_info (MetaPlugin *plugin);

/**
 * MetaPluginVersion:
 * @version_major: major component of the version number of Meta with which the plugin was compiled
 * @version_minor: minor component of the version number of Meta with which the plugin was compiled
 * @version_micro: micro component of the version number of Meta with which the plugin was compiled
 * @version_api: version of the plugin API
 */
struct _MetaPluginVersion
{
  /*
   * Version information; the first three numbers match the Meta version
   * with which the plugin was compiled (see clutter-plugins/simple.c for sample
   * code).
   */
  guint version_major;
  guint version_minor;
  guint version_micro;

  /*
   * Version of the plugin API; this is unrelated to the matacity version
   * per se. The API version is checked by the plugin manager and must match
   * the one used by it (see clutter-plugins/default.c for sample code).
   */
  guint version_api;
};

/*
 * Convenience macro to set up the plugin type. Based on GEdit.
 */
#define META_PLUGIN_DECLARE(ObjectName, object_name)                    \
  G_MODULE_EXPORT MetaPluginVersion meta_plugin_version =               \
    {                                                                   \
      MUTTER_MAJOR_VERSION,                                             \
      MUTTER_MINOR_VERSION,                                             \
      MUTTER_MICRO_VERSION,                                             \
      MUTTER_PLUGIN_API_VERSION                                         \
    };                                                                  \
                                                                        \
  static GType g_define_type_id = 0;                                    \
                                                                        \
  /* Prototypes */                                                      \
  G_MODULE_EXPORT                                                       \
  GType object_name##_get_type (void);                                  \
                                                                        \
  G_MODULE_EXPORT                                                       \
  GType object_name##_register_type (GTypeModule *type_module);         \
                                                                        \
  G_MODULE_EXPORT                                                       \
  GType meta_plugin_register_type (GTypeModule *type_module);           \
                                                                        \
  GType                                                                 \
  object_name##_get_type ()                                             \
  {                                                                     \
    return g_define_type_id;                                            \
  }                                                                     \
                                                                        \
  static void object_name##_init (ObjectName *self);                    \
  static void object_name##_class_init (ObjectName##Class *klass);      \
  static gpointer object_name##_parent_class = NULL;                    \
  static void object_name##_class_intern_init (gpointer klass)          \
  {                                                                     \
    object_name##_parent_class = g_type_class_peek_parent (klass);      \
    object_name##_class_init ((ObjectName##Class *) klass);             \
  }                                                                     \
                                                                        \
  GType                                                                 \
  object_name##_register_type (GTypeModule *type_module)                \
  {                                                                     \
    static const GTypeInfo our_info =                                   \
      {                                                                 \
        sizeof (ObjectName##Class),                                     \
        NULL, /* base_init */                                           \
        NULL, /* base_finalize */                                       \
        (GClassInitFunc) object_name##_class_intern_init,               \
        NULL,                                                           \
        NULL, /* class_data */                                          \
        sizeof (ObjectName),                                            \
        0, /* n_preallocs */                                            \
        (GInstanceInitFunc) object_name##_init                          \
      };                                                                \
                                                                        \
    g_define_type_id = g_type_module_register_type (type_module,        \
                                                    META_TYPE_PLUGIN,   \
                                                    #ObjectName,        \
                                                    &our_info,          \
                                                    0);                 \
                                                                        \
                                                                        \
    return g_define_type_id;                                            \
  }                                                                     \
                                                                        \
  G_MODULE_EXPORT GType                                                 \
  meta_plugin_register_type (GTypeModule *type_module)                  \
  {                                                                     \
    return object_name##_register_type (type_module);                   \
  }                                                                     \

void
meta_plugin_switch_workspace_completed (MetaPlugin *plugin);

void
meta_plugin_minimize_completed (MetaPlugin      *plugin,
                                MetaWindowActor *actor);

void
meta_plugin_maximize_completed (MetaPlugin      *plugin,
                                MetaWindowActor *actor);

void
meta_plugin_unmaximize_completed (MetaPlugin      *plugin,
                                  MetaWindowActor *actor);

void
meta_plugin_map_completed (MetaPlugin      *plugin,
                           MetaWindowActor *actor);

void
meta_plugin_destroy_completed (MetaPlugin      *plugin,
                               MetaWindowActor *actor);

void
meta_plugin_complete_display_change (MetaPlugin *plugin,
                                     gboolean    ok);

/**
 * MetaModalOptions:
 * @META_MODAL_POINTER_ALREADY_GRABBED: if set the pointer is already
 *   grabbed by the plugin and should not be grabbed again.
 * @META_MODAL_KEYBOARD_ALREADY_GRABBED: if set the keyboard is already
 *   grabbed by the plugin and should not be grabbed again.
 *
 * Options that can be provided when calling meta_plugin_begin_modal().
 */
typedef enum {
  META_MODAL_POINTER_ALREADY_GRABBED = 1 << 0,
  META_MODAL_KEYBOARD_ALREADY_GRABBED = 1 << 1
} MetaModalOptions;

gboolean
meta_plugin_begin_modal (MetaPlugin      *plugin,
                         MetaModalOptions options,
                         guint32          timestamp);

void
meta_plugin_end_modal (MetaPlugin *plugin,
                       guint32     timestamp);

MetaScreen *meta_plugin_get_screen        (MetaPlugin *plugin);

void _meta_plugin_set_compositor (MetaPlugin *plugin, MetaCompositor *compositor);

/* XXX: Putting this in here so it's in the public header. */
void     meta_plugin_manager_set_plugin_type (GType gtype);

#endif /* META_PLUGIN_H_ */
