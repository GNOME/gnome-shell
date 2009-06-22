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
#include <gmodule.h>

/*
 * FIXME -- move these to a private include
 * Required by plugin manager.
 */
#define MUTTER_PLUGIN_MINIMIZE         (1<<0)
#define MUTTER_PLUGIN_MAXIMIZE         (1<<1)
#define MUTTER_PLUGIN_UNMAXIMIZE       (1<<2)
#define MUTTER_PLUGIN_MAP              (1<<3)
#define MUTTER_PLUGIN_DESTROY          (1<<4)
#define MUTTER_PLUGIN_SWITCH_WORKSPACE (1<<5)

#define MUTTER_PLUGIN_ALL_EFFECTS      (~0)

#define MUTTER_TYPE_PLUGIN            (mutter_plugin_get_type ())
#define MUTTER_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MUTTER_TYPE_PLUGIN, MutterPlugin))
#define MUTTER_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MUTTER_TYPE_PLUGIN, MutterPluginClass))
#define MUTTER_IS_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MUTTER_TYPE_PLUGIN))
#define MUTTER_IS_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MUTTER_TYPE_PLUGIN))
#define MUTTER_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MUTTER_TYPE_PLUGIN, MutterPluginClass))

typedef struct _MutterPlugin        MutterPlugin;
typedef struct _MutterPluginClass   MutterPluginClass;
typedef struct _MutterPluginVersion MutterPluginVersion;
typedef struct _MutterPluginInfo    MutterPluginInfo;
typedef struct _MutterPluginPrivate MutterPluginPrivate;

struct _MutterPlugin
{
  GObject parent;

  MutterPluginPrivate *priv;
};

struct _MutterPluginClass
{
  GObjectClass parent_class;

  void (*minimize)         (MutterPlugin       *plugin,
                            MutterWindow       *actor);

  void (*maximize)         (MutterPlugin       *plugin,
                            MutterWindow       *actor,
                            gint                x,
                            gint                y,
                            gint                width,
                            gint                height);

  void (*unmaximize)       (MutterPlugin       *plugin,
                            MutterWindow       *actor,
                            gint                x,
                            gint                y,
                            gint                width,
                            gint                height);

  void (*map)              (MutterPlugin       *plugin,
                            MutterWindow       *actor);

  void (*destroy)          (MutterPlugin       *plugin,
                            MutterWindow       *actor);

  void (*switch_workspace) (MutterPlugin       *plugin,
                            const GList       **actors,
                            gint                from,
                            gint                to,
                            MetaMotionDirection direction);

  /*
   * Called if an effect should be killed prematurely; the plugin must
   * call the completed() callback as if the effect terminated naturally.
   * The events parameter is a bitmask indicating which effects are to be
   * killed.
   */
  void (*kill_effect)      (MutterPlugin     *plugin,
                            MutterWindow     *actor,
                            gulong            events);

  /* General XEvent filter. This is fired *before* mutter itself handles
   * an event. Return TRUE to block any further processing.
   */
  gboolean (*xevent_filter) (MutterPlugin       *plugin,
                             XEvent             *event);

  const MutterPluginInfo * (*plugin_info) (MutterPlugin *plugin);
};

struct _MutterPluginInfo
{
  const gchar *name;
  const gchar *version;
  const gchar *author;
  const gchar *license;
  const gchar *description;
};

GType mutter_plugin_get_type (void);

gulong        mutter_plugin_features            (MutterPlugin *plugin);
gboolean      mutter_plugin_disabled            (MutterPlugin *plugin);
gboolean      mutter_plugin_running             (MutterPlugin *plugin);
gboolean      mutter_plugin_debug_mode          (MutterPlugin *plugin);
const MutterPluginInfo * mutter_plugin_get_info (MutterPlugin *plugin);

struct _MutterPluginVersion
{
  /*
   * Version information; the first three numbers match the Mutter version
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
#define MUTTER_PLUGIN_DECLARE(ObjectName, object_name)                  \
  G_MODULE_EXPORT MutterPluginVersion mutter_plugin_version =           \
    {                                                                   \
      MUTTER_MAJOR_VERSION,                                           \
      MUTTER_MINOR_VERSION,                                           \
      MUTTER_MICRO_VERSION,                                           \
      MUTTER_PLUGIN_API_VERSION                               \
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
  GType mutter_plugin_register_type (GTypeModule *type_module);         \
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
                                                    MUTTER_TYPE_PLUGIN, \
                                                    #ObjectName,        \
                                                    &our_info,          \
                                                    0);                 \
                                                                        \
                                                                        \
    return g_define_type_id;                                            \
  }                                                                     \
                                                                        \
  G_MODULE_EXPORT GType                                                 \
  mutter_plugin_register_type (GTypeModule *type_module)                \
  {                                                                     \
    return object_name##_register_type (type_module);                   \
  }                                                                     \

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

Display *
mutter_plugin_get_xdisplay (MutterPlugin *plugin);

MetaScreen *
mutter_plugin_get_screen (MutterPlugin *plugin);

void
_mutter_plugin_effect_started (MutterPlugin *plugin);

#endif /* MUTTER_PLUGIN_H_ */
