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

#define MUTTER_BUILDING_PLUGIN 1
#include "mutter-plugin.h"

#include <libintl.h>
#define _(x) dgettext (GETTEXT_PACKAGE, x)
#define N_(x) x

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <gmodule.h>
#include <string.h>

#include "../tidy/tidy-grid.h"

/* For debugging only */
#include "../../../core/window-private.h"
#include "compositor-mutter.h"

#define DESTROY_TIMEOUT     250
#define MINIMIZE_TIMEOUT    250
#define MAXIMIZE_TIMEOUT    250
#define MAP_TIMEOUT         250
#define SWITCH_TIMEOUT      500
#define PANEL_SLIDE_TIMEOUT 250;                \

#define PANEL_SLIDE_THRESHOLD 2
#define PANEL_HEIGHT          40
#define ACTOR_DATA_KEY "MCCP-scratch-actor-data"

#define SWITCHER_CELL_WIDTH  200
#define SWITCHER_CELL_HEIGHT 200

static GQuark actor_data_quark = 0;

typedef struct PluginPrivate PluginPrivate;
typedef struct ActorPrivate  ActorPrivate;

static gboolean do_init  (const char *params);
static void     minimize (MutterWindow *actor);
static void     map      (MutterWindow *actor);
static void     destroy  (MutterWindow *actor);
static void     maximize (MutterWindow *actor,
                          gint x, gint y, gint width, gint height);
static void     unmaximize (MutterWindow *actor,
                            gint x, gint y, gint width, gint height);
static void     switch_workspace (const GList **actors, gint from, gint to,
                                  MetaMotionDirection direction);
static void     kill_effect (MutterWindow *actor, gulong event);
static gboolean xevent_filter (XEvent *xev);
static gboolean reload (const char *params);

/*
 * Create the plugin struct; function pointers initialized in
 * g_module_check_init().
 */
MUTTER_DECLARE_PLUGIN ();

/*
 * Plugin private data that we store in the .plugin_private member.
 */
struct PluginPrivate
{
  ClutterEffectTemplate *destroy_effect;
  ClutterEffectTemplate *minimize_effect;
  ClutterEffectTemplate *maximize_effect;
  ClutterEffectTemplate *map_effect;
  ClutterEffectTemplate *switch_workspace_effect;
  ClutterEffectTemplate *switch_workspace_arrow_effect;
  ClutterEffectTemplate *panel_slide_effect;

  /* Valid only when switch_workspace effect is in progress */
  ClutterTimeline       *tml_switch_workspace1;
  ClutterTimeline       *tml_switch_workspace2;
  GList                **actors;
  ClutterActor          *desktop1;
  ClutterActor          *desktop2;

  ClutterActor          *d_overlay ; /* arrow indicator */
  ClutterActor          *panel;

  ClutterActor          *switcher;

  gboolean               debug_mode : 1;
  gboolean               panel_out  : 1;
  gboolean               panel_out_in_progress : 1;
  gboolean               panel_back_in_progress : 1;
};

/*
 * Per actor private data we attach to each actor.
 */
struct ActorPrivate
{
  ClutterActor *orig_parent;
  gint          orig_x;
  gint          orig_y;

  ClutterTimeline *tml_minimize;
  ClutterTimeline *tml_maximize;
  ClutterTimeline *tml_destroy;
  ClutterTimeline *tml_map;

  gboolean      is_minimized : 1;
  gboolean      is_maximized : 1;
};

/*
 * Actor private data accessor
 */
static void
free_actor_private (gpointer data)
{
  if (G_LIKELY (data != NULL))
    g_slice_free (ActorPrivate, data);
}

static ActorPrivate *
get_actor_private (MutterWindow *actor)
{
  ActorPrivate *priv = g_object_get_qdata (G_OBJECT (actor), actor_data_quark);

  if (G_UNLIKELY (actor_data_quark == 0))
    actor_data_quark = g_quark_from_static_string (ACTOR_DATA_KEY);

  if (G_UNLIKELY (!priv))
    {
      priv = g_slice_new0 (ActorPrivate);

      g_object_set_qdata_full (G_OBJECT (actor),
                               actor_data_quark, priv,
                               free_actor_private);
    }

  return priv;
}

static void
on_switch_workspace_effect_complete (ClutterActor *group, gpointer data)
{
  MutterPlugin   *plugin = mutter_get_plugin ();
  PluginPrivate  *ppriv  = plugin->plugin_private;
  GList          *l      = *((GList**)data);
  MutterWindow   *actor_for_cb = l->data;

  while (l)
    {
      ClutterActor *a    = l->data;
      MutterWindow *mcw  = MUTTER_WINDOW (a);
      ActorPrivate *priv = get_actor_private (mcw);

      if (priv->orig_parent)
        {
          clutter_actor_reparent (a, priv->orig_parent);
          priv->orig_parent = NULL;
        }

      l = l->next;
    }

  clutter_actor_destroy (ppriv->desktop1);
  clutter_actor_destroy (ppriv->desktop2);
  clutter_actor_destroy (ppriv->d_overlay);

  ppriv->actors = NULL;
  ppriv->tml_switch_workspace1 = NULL;
  ppriv->tml_switch_workspace2 = NULL;
  ppriv->desktop1 = NULL;
  ppriv->desktop2 = NULL;

  mutter_plugin_effect_completed (plugin, actor_for_cb,
                                  MUTTER_PLUGIN_SWITCH_WORKSPACE);
}

static void
switch_workspace (const GList **actors, gint from, gint to,
                  MetaMotionDirection direction)
{
  MutterPlugin  *plugin = mutter_get_plugin ();
  PluginPrivate *ppriv  = plugin->plugin_private;
  GList         *l;
  gint           n_workspaces;
  ClutterActor  *group1  = clutter_group_new ();
  ClutterActor  *group2  = clutter_group_new ();
  ClutterActor  *group3  = clutter_group_new ();
  ClutterActor  *stage, *label, *rect, *window_layer, *overlay_layer;
  gint           to_x, to_y, from_x = 0, from_y = 0;
  ClutterColor   white = { 0xff, 0xff, 0xff, 0xff };
  ClutterColor   black = { 0x33, 0x33, 0x33, 0xff };
  gint           screen_width;
  gint           screen_height;

  stage = mutter_plugin_get_stage (plugin);

  mutter_plugin_query_screen_size (plugin, &screen_width, &screen_height);

  window_layer = mutter_plugin_get_window_group (plugin);
  overlay_layer = mutter_plugin_get_overlay_group (plugin);

  clutter_container_add_actor (CLUTTER_CONTAINER (window_layer), group1);
  clutter_container_add_actor (CLUTTER_CONTAINER (window_layer), group2);
  clutter_container_add_actor (CLUTTER_CONTAINER (overlay_layer), group3);

  if (from == to)
    {
      clutter_actor_destroy (group3);
      clutter_actor_destroy (group2);
      clutter_actor_destroy (group1);

      mutter_plugin_effect_completed (plugin, NULL,
                                      MUTTER_PLUGIN_SWITCH_WORKSPACE);
      return;
    }

  n_workspaces = g_list_length (plugin->work_areas);

  l = g_list_last (*((GList**) actors));

  while (l)
    {
      MutterWindow *mcw  = l->data;
      ActorPrivate *priv = get_actor_private (mcw);
      ClutterActor *a    = CLUTTER_ACTOR (mcw);
      gint          workspace;

      workspace = mutter_window_get_workspace (mcw);

      if (workspace == to || workspace == from)
        {
          gint x, y;
          guint w, h;

          clutter_actor_get_position (a, &x, &y);
          clutter_actor_get_size (a, &w, &h);

          priv->orig_parent = clutter_actor_get_parent (a);

          clutter_actor_reparent (a, workspace == to ? group2 : group1);
          clutter_actor_show_all (a);
          clutter_actor_raise_top (a);
        }
      else if (workspace < 0)
        {
          /* Sticky window */
          priv->orig_parent = NULL;
        }
      else
        {
          /* Window on some other desktop */
          clutter_actor_hide (a);
          priv->orig_parent = NULL;
        }

      l = l->prev;
    }

  /* Make arrow indicator */
  rect = clutter_rectangle_new ();
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect), &white);
  clutter_container_add_actor (CLUTTER_CONTAINER (group3), rect);

  label = clutter_label_new ();
  clutter_label_set_font_name (CLUTTER_LABEL (label), "Sans Bold 148");
  clutter_label_set_color (CLUTTER_LABEL (label), &black);
  clutter_container_add_actor (CLUTTER_CONTAINER (group3), label);

  clutter_actor_set_size (rect,
                          clutter_actor_get_width (label),
                          clutter_actor_get_height (label));

  ppriv->actors  = (GList **)actors;
  ppriv->desktop1 = group1;
  ppriv->desktop2 = group2;
  ppriv->d_overlay = group3;

  switch (direction)
    {
    case META_MOTION_UP:
      clutter_label_set_text (CLUTTER_LABEL (label), "\342\206\221");

      to_x = 0;
      to_y = -screen_height;
      break;

    case META_MOTION_DOWN:
      clutter_label_set_text (CLUTTER_LABEL (label), "\342\206\223");

      to_x = 0;
      to_y = -screen_height;
      break;

    case META_MOTION_LEFT:
      clutter_label_set_text (CLUTTER_LABEL (label), "\342\206\220");

      to_x = -screen_width * -1;
      to_y = 0;
      break;

    case META_MOTION_RIGHT:
      clutter_label_set_text (CLUTTER_LABEL (label), "\342\206\222");

      to_x = -screen_width;
      to_y = 0;
      break;

    default:
      break;
    }

  /* dest group offscreen and on top */
  clutter_actor_set_position (group2, to_x, to_y); /* *-1 for simpler */
  clutter_actor_raise_top (group2);

  /* center arrow */
  clutter_actor_set_position 
                    (group3,
                     (screen_width - clutter_actor_get_width (group3)) / 2,
                     (screen_height - clutter_actor_get_height (group3)) / 2);


  /* workspace were going too */
  ppriv->tml_switch_workspace2 =
    clutter_effect_move (ppriv->switch_workspace_effect, group2,
                         0, 0,
                         on_switch_workspace_effect_complete,

                         actors);
  /* coming from */
  ppriv->tml_switch_workspace1 =
    clutter_effect_move (ppriv->switch_workspace_effect, group1,
                         to_x, to_y,
                         NULL, NULL);

  /* arrow */
  clutter_effect_fade (ppriv->switch_workspace_arrow_effect, group3,
                       0,
                       NULL, NULL);


}

/*
 * Minimize effect completion callback; this function restores actor state, and
 * calls the manager callback function.
 */
static void
on_minimize_effect_complete (ClutterActor *actor, gpointer data)
{
  /*
   * Must reverse the effect of the effect; must hide it first to ensure
   * that the restoration will not be visible.
   */
  MutterPlugin *plugin = mutter_get_plugin ();
  ActorPrivate *apriv;
  MutterWindow *mcw = MUTTER_WINDOW (actor);

  apriv = get_actor_private (MUTTER_WINDOW (actor));
  apriv->tml_minimize = NULL;

  clutter_actor_hide (actor);

  clutter_actor_set_scale (actor, 1.0, 1.0);
  clutter_actor_move_anchor_point_from_gravity (actor,
                                                CLUTTER_GRAVITY_NORTH_WEST);

  /* Now notify the manager that we are done with this effect */
  mutter_plugin_effect_completed (plugin, mcw,
                                      MUTTER_PLUGIN_MINIMIZE);
}

/*
 * Simple minimize handler: it applies a scale effect (which must be reversed on
 * completion).
 */
static void
minimize (MutterWindow *mcw)

{
  MutterPlugin      *plugin = mutter_get_plugin ();
  PluginPrivate     *priv   = plugin->plugin_private;
  MetaCompWindowType type;
  ClutterActor      *actor  = CLUTTER_ACTOR (mcw);

  type = mutter_window_get_window_type (mcw);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv = get_actor_private (mcw);

      apriv->is_minimized = TRUE;

      clutter_actor_move_anchor_point_from_gravity (actor,
                                                    CLUTTER_GRAVITY_CENTER);

      apriv->tml_minimize = clutter_effect_scale (priv->minimize_effect,
                                                  actor,
                                                  0.0,
                                                  0.0,
                                                  (ClutterEffectCompleteFunc)
                                                  on_minimize_effect_complete,
                                                  NULL);
    }
  else
    mutter_plugin_effect_completed (plugin, mcw, MUTTER_PLUGIN_MINIMIZE);
}

/*
 * Minimize effect completion callback; this function restores actor state, and
 * calls the manager callback function.
 */
static void
on_maximize_effect_complete (ClutterActor *actor, gpointer data)
{
  /*
   * Must reverse the effect of the effect.
   */
  MutterPlugin *plugin = mutter_get_plugin ();
  MutterWindow *mcw    = MUTTER_WINDOW (actor);
  ActorPrivate *apriv  = get_actor_private (mcw);

  apriv->tml_maximize = NULL;

  clutter_actor_set_scale (actor, 1.0, 1.0);
  clutter_actor_move_anchor_point_from_gravity (actor,
                                                CLUTTER_GRAVITY_NORTH_WEST);

  /* Now notify the manager that we are done with this effect */
  mutter_plugin_effect_completed (plugin, mcw, MUTTER_PLUGIN_MAXIMIZE);
}

/*
 * The Nature of Maximize operation is such that it is difficult to do a visual
 * effect that would work well. Scaling, the obvious effect, does not work that
 * well, because at the end of the effect we end up with window content bigger
 * and differently laid out than in the real window; this is a proof concept.
 *
 * (Something like a sound would be more appropriate.)
 */
static void
maximize (MutterWindow *mcw,
          gint end_x, gint end_y, gint end_width, gint end_height)
{
  MutterPlugin       *plugin = mutter_get_plugin ();
  PluginPrivate      *priv   = plugin->plugin_private;
  MetaCompWindowType  type;
  ClutterActor       *actor  = CLUTTER_ACTOR (mcw);

  gdouble  scale_x  = 1.0;
  gdouble  scale_y  = 1.0;
  gint     anchor_x = 0;
  gint     anchor_y = 0;

  type = mutter_window_get_window_type (mcw);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv = get_actor_private (mcw);
      guint width, height;
      gint  x, y;

      apriv->is_maximized = TRUE;

      clutter_actor_get_size (actor, &width, &height);
      clutter_actor_get_position (actor, &x, &y);

      /*
       * Work out the scale and anchor point so that the window is expanding
       * smoothly into the target size.
       */
      scale_x = (gdouble)end_width / (gdouble) width;
      scale_y = (gdouble)end_height / (gdouble) height;

      anchor_x = (gdouble)(x - end_x)*(gdouble)width /
        ((gdouble)(end_width - width));
      anchor_y = (gdouble)(y - end_y)*(gdouble)height /
        ((gdouble)(end_height - height));

      clutter_actor_move_anchor_point (actor, anchor_x, anchor_y);

      apriv->tml_maximize = clutter_effect_scale (priv->maximize_effect,
                                                  actor,
                                                  scale_x,
                                                  scale_y,
                                                  (ClutterEffectCompleteFunc)
                                                  on_maximize_effect_complete,
                                                  NULL);

      return;
    }

  mutter_plugin_effect_completed (plugin, mcw, MUTTER_PLUGIN_MAXIMIZE);
}

/*
 * See comments on the maximize() function.
 *
 * (Just a skeleton code.)
 */
static void
unmaximize (MutterWindow *mcw,
            gint end_x, gint end_y, gint end_width, gint end_height)
{
  MutterPlugin       *plugin = mutter_get_plugin ();
  MetaCompWindowType  type;

  type = mutter_window_get_window_type (mcw);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv = get_actor_private (mcw);

      apriv->is_maximized = FALSE;
    }

  /* Do this conditionally, if the effect requires completion callback. */
  mutter_plugin_effect_completed (plugin, mcw, MUTTER_PLUGIN_UNMAXIMIZE);
}

static void
on_map_effect_complete (ClutterActor *actor, gpointer data)
{
  /*
   * Must reverse the effect of the effect.
   */
  MutterPlugin *plugin = mutter_get_plugin ();
  MutterWindow *mcw    = MUTTER_WINDOW (actor);
  ActorPrivate *apriv  = get_actor_private (mcw);

  apriv->tml_map = NULL;

  clutter_actor_move_anchor_point_from_gravity (actor,
                                                CLUTTER_GRAVITY_NORTH_WEST);

  /* Now notify the manager that we are done with this effect */
  mutter_plugin_effect_completed (plugin, mcw, MUTTER_PLUGIN_MAP);
}

/*
 * Simple map handler: it applies a scale effect which must be reversed on
 * completion).
 */
static void
map (MutterWindow *mcw)
{
  MutterPlugin       *plugin = mutter_get_plugin ();
  PluginPrivate      *priv   = plugin->plugin_private;
  MetaCompWindowType  type;
  ClutterActor       *actor  = CLUTTER_ACTOR (mcw);

  type = mutter_window_get_window_type (mcw);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv  = get_actor_private (mcw);

      clutter_actor_move_anchor_point_from_gravity (actor,
                                                    CLUTTER_GRAVITY_CENTER);

      clutter_actor_set_scale (actor, 0.0, 0.0);
      clutter_actor_show (actor);

      apriv->tml_map = clutter_effect_scale (priv->map_effect,
                                             actor,
                                             1.0,
                                             1.0,
                                             (ClutterEffectCompleteFunc)
                                             on_map_effect_complete,
                                             NULL);

      apriv->is_minimized = FALSE;

    }
  else
    mutter_plugin_effect_completed (plugin, mcw, MUTTER_PLUGIN_MAP);
}

/*
 * Destroy effect completion callback; this is a simple effect that requires no
 * further action than notifying the manager that the effect is completed.
 */
static void
on_destroy_effect_complete (ClutterActor *actor, gpointer data)
{
  MutterPlugin *plugin = mutter_get_plugin ();
  MutterWindow *mcw    = MUTTER_WINDOW (actor);
  ActorPrivate *apriv  = get_actor_private (mcw);

  apriv->tml_destroy = NULL;

  mutter_plugin_effect_completed (plugin, mcw, MUTTER_PLUGIN_DESTROY);
}

/*
 * Simple TV-out like effect.
 */
static void
destroy (MutterWindow *mcw)
{
  MutterPlugin       *plugin = mutter_get_plugin ();
  PluginPrivate      *priv   = plugin->plugin_private;
  MetaCompWindowType  type;
  ClutterActor       *actor  = CLUTTER_ACTOR (mcw);

  type = mutter_window_get_window_type (mcw);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv  = get_actor_private (mcw);

      clutter_actor_move_anchor_point_from_gravity (actor,
                                                    CLUTTER_GRAVITY_CENTER);

      apriv->tml_destroy = clutter_effect_scale (priv->destroy_effect,
                                                 actor,
                                                 1.0,
                                                 0.0,
                                                 (ClutterEffectCompleteFunc)
                                                 on_destroy_effect_complete,
                                                 NULL);
    }
  else
    mutter_plugin_effect_completed (plugin, mcw, MUTTER_PLUGIN_DESTROY);
}

/*
 * Use this function to disable stage input
 *
 * Used by the completion callback for the panel in/out effects
 */
static void
disable_stage (MutterPlugin *plugin)
{
  gint screen_width, screen_height;

  mutter_plugin_query_screen_size (plugin, &screen_width, &screen_height);
  mutter_plugin_set_stage_input_area (plugin, 0, 0, screen_width, 1);
}

static void
on_panel_effect_complete (ClutterActor *panel, gpointer data)
{
  gboolean       reactive = GPOINTER_TO_INT (data);
  MutterPlugin  *plugin   = mutter_get_plugin ();
  PluginPrivate *priv     = plugin->plugin_private;

  if (reactive)
    {
      priv->panel_out_in_progress = FALSE;
      mutter_plugin_set_stage_reactive (plugin, reactive);
    }
  else
    {
      priv->panel_back_in_progress = FALSE;
      disable_stage (plugin);
    }
}

static gboolean
xevent_filter (XEvent *xev)
{
  MutterPlugin *plugin = mutter_get_plugin ();
  ClutterActor                *stage;

  stage = mutter_plugin_get_stage (plugin);

  clutter_x11_handle_event (xev);

  return FALSE;
}

static void
kill_effect (MutterWindow *mcw, gulong event)
{
  MutterPlugin *plugin = mutter_get_plugin ();
  ActorPrivate *apriv;
  ClutterActor *actor = CLUTTER_ACTOR (mcw);

  if (event & MUTTER_PLUGIN_SWITCH_WORKSPACE)
    {
      PluginPrivate *ppriv  = plugin->plugin_private;

      if (ppriv->tml_switch_workspace1)
        {
          clutter_timeline_stop (ppriv->tml_switch_workspace1);
          clutter_timeline_stop (ppriv->tml_switch_workspace2);
          on_switch_workspace_effect_complete (ppriv->desktop1, ppriv->actors);
        }

      if (!(event & ~MUTTER_PLUGIN_SWITCH_WORKSPACE))
        {
          /* Workspace switch only, nothing more to do */
          return;
        }
    }

  apriv = get_actor_private (mcw);

  if ((event & MUTTER_PLUGIN_MINIMIZE) && apriv->tml_minimize)
    {
      clutter_timeline_stop (apriv->tml_minimize);
      on_minimize_effect_complete (actor, NULL);
    }

  if ((event & MUTTER_PLUGIN_MAXIMIZE) && apriv->tml_maximize)
    {
      clutter_timeline_stop (apriv->tml_maximize);
      on_maximize_effect_complete (actor, NULL);
    }

  if ((event & MUTTER_PLUGIN_MAP) && apriv->tml_map)
    {
      clutter_timeline_stop (apriv->tml_map);
      on_map_effect_complete (actor, NULL);
    }

  if ((event & MUTTER_PLUGIN_DESTROY) && apriv->tml_destroy)
    {
      clutter_timeline_stop (apriv->tml_destroy);
      on_destroy_effect_complete (actor, NULL);
    }
}


const gchar * g_module_check_init (GModule *module);
const gchar *
g_module_check_init (GModule *module)
{
  MutterPlugin *plugin = mutter_get_plugin ();

  /* Human readable name (for use in UI) */
  plugin->name = "Experimental effects",

  /* Plugin load time initialiser */
  plugin->do_init = do_init;

  /* Effect handlers */
  plugin->minimize         = minimize;
  plugin->destroy          = destroy;
  plugin->map              = map;
  plugin->maximize         = maximize;
  plugin->unmaximize       = unmaximize;
  plugin->switch_workspace = switch_workspace;
  plugin->kill_effect      = kill_effect;
  plugin->xevent_filter    = xevent_filter;

  /* The reload handler */
  plugin->reload           = reload;

  return NULL;
}

static void switcher_clone_weak_notify (gpointer data, GObject *object);

static void
switcher_origin_weak_notify (gpointer data, GObject *object)
{
  ClutterActor *clone = data;

  /*
   * The original MutterWindow destroyed; remove the weak reference the
   * we added to the clone referencing the original window, then
   * destroy the clone.
   */
  g_object_weak_unref (G_OBJECT (clone), switcher_clone_weak_notify, object);
  clutter_actor_destroy (clone);
}

static void
switcher_clone_weak_notify (gpointer data, GObject *object)
{
  ClutterActor *origin = data;

  /*
   * Clone destroyed -- this function gets only called whent the clone
   * is destroyed while the original MutterWindow still exists, so remove
   * the weak reference we added on the origin for sake of the clone.
   */
  g_object_weak_unref (G_OBJECT (origin), switcher_origin_weak_notify, object);
}

static gboolean
switcher_clone_input_cb (ClutterActor *clone,
                         ClutterEvent *event,
                         gpointer      data)
{
  MutterWindow  *mw = data;
  MetaWindow    *window;
  MetaWorkspace *workspace;

  printf ("Actor %p (%s) clicked\n",
          clone, clutter_actor_get_name (clone));

  window    = mutter_window_get_meta_window (mw);
  workspace = meta_window_get_workspace (window);

  meta_workspace_activate_with_focus (workspace, window, event->any.time);

  return FALSE;
}

/*
 * This is a simple example of how a switcher might access the windows.
 *
 * Note that we use ClutterCloneTexture hooked up to the texture *inside*
 * MutterWindow (with FBO support, we could clone the entire MutterWindow,
 * although for the switcher purposes that is probably not what is wanted
 * anyway).
 */
static void
hide_switcher (void)
{
  MutterPlugin  *plugin = mutter_get_plugin ();
  PluginPrivate *priv   = plugin->plugin_private;

  if (!priv->switcher)
    return;

  clutter_actor_destroy (priv->switcher);
  priv->switcher = NULL;
}

static void
show_switcher (void)
{
  MutterPlugin  *plugin   = mutter_get_plugin ();
  PluginPrivate *priv     = plugin->plugin_private;
  ClutterActor  *overlay;
  GList         *l;
  ClutterActor  *switcher;
  TidyGrid      *grid;
  guint          panel_height;
  gint           panel_y;
  gint           screen_width, screen_height;

  mutter_plugin_query_screen_size (plugin, &screen_width, &screen_height);

  switcher = tidy_grid_new ();

  grid = TIDY_GRID (switcher);

  tidy_grid_set_homogenous_rows (grid, TRUE);
  tidy_grid_set_homogenous_columns (grid, TRUE);
  tidy_grid_set_column_major (grid, FALSE);
  tidy_grid_set_row_gap (grid, CLUTTER_UNITS_FROM_INT (10));
  tidy_grid_set_column_gap (grid, CLUTTER_UNITS_FROM_INT (10));

  l = mutter_plugin_get_windows (plugin);
  while (l)
    {
      MutterWindow       *mw   = l->data;
      MetaCompWindowType  type = mutter_window_get_window_type (mw);
      ClutterActor       *a    = CLUTTER_ACTOR (mw);
      ClutterActor       *texture;
      ClutterActor       *clone;
      guint               w, h;
      gdouble             s_x, s_y, s;

      /*
       * Only show regular windows.
       */
      if (mutter_window_is_override_redirect (mw) ||
          type != META_COMP_WINDOW_NORMAL)
        {
          l = l->next;
          continue;
        }

#if 0
      printf ("Adding %p:%s\n",
              mw,
              mutter_window_get_meta_window (mw) ?
              mutter_window_get_meta_window (mw)->desc : "unknown");
#endif

      texture = mutter_window_get_texture (mw);
      clone   = clutter_clone_texture_new (CLUTTER_TEXTURE (texture));

      clutter_actor_set_name (clone, mutter_window_get_meta_window (mw)->desc);
      g_signal_connect (clone,
                        "button-press-event",
                        G_CALLBACK (switcher_clone_input_cb), mw);

      g_object_weak_ref (G_OBJECT (mw), switcher_origin_weak_notify, clone);
      g_object_weak_ref (G_OBJECT (clone), switcher_clone_weak_notify, mw);

      /*
       * Scale clone to fit the predefined size of the grid cell
       */
      clutter_actor_get_size (a, &w, &h);
      s_x = (gdouble) SWITCHER_CELL_WIDTH  / (gdouble) w;
      s_y = (gdouble) SWITCHER_CELL_HEIGHT / (gdouble) h;

      s = s_x < s_y ? s_x : s_y;

      if (s_x < s_y)
        clutter_actor_set_size (clone,
                                (guint)((gdouble)w * s_x),
                                (guint)((gdouble)h * s_x));
      else
        clutter_actor_set_size (clone,
                                (guint)((gdouble)w * s_y),
                                (guint)((gdouble)h * s_y));

      clutter_actor_set_reactive (clone, TRUE);

      clutter_container_add_actor (CLUTTER_CONTAINER (grid), clone);
      l = l->next;
    }

  if (priv->switcher)
    hide_switcher ();

  priv->switcher = switcher;

  panel_height = clutter_actor_get_height (priv->panel);
  panel_y      = clutter_actor_get_y (priv->panel);

  clutter_actor_set_position (switcher, 10, panel_height + panel_y);

  overlay = mutter_plugin_get_overlay_group (plugin); 
  clutter_container_add_actor (CLUTTER_CONTAINER (overlay), switcher);

  clutter_actor_set_width (grid, screen_width);
}

static void
toggle_switcher ()
{
  MutterPlugin  *plugin   = mutter_get_plugin ();
  PluginPrivate *priv     = plugin->plugin_private;

  if (priv->switcher)
    hide_switcher ();
  else
    show_switcher ();
}

static gboolean
stage_input_cb (ClutterActor *stage, ClutterEvent *event, gpointer data)
{
  gboolean capture = GPOINTER_TO_INT (data);

  if ((capture && event->type == CLUTTER_MOTION) ||
      (!capture && event->type == CLUTTER_BUTTON_PRESS))
    {
      gint event_y;
      MutterPlugin       *plugin = mutter_get_plugin ();
      PluginPrivate      *priv   = plugin->plugin_private;

      if (event->type == CLUTTER_MOTION)
        event_y = ((ClutterMotionEvent*)event)->y;
      else
        event_y = ((ClutterButtonEvent*)event)->y;

      if (priv->panel_out_in_progress || priv->panel_back_in_progress)
        return FALSE;

      if (priv->panel_out &&
          (event->type == CLUTTER_BUTTON_PRESS || !priv->switcher))
        {
          guint height = clutter_actor_get_height (priv->panel);
          gint  x      = clutter_actor_get_x (priv->panel);

          if (event_y > (gint)height)
            {
              priv->panel_back_in_progress  = TRUE;

              clutter_effect_move (priv->panel_slide_effect,
                                   priv->panel, x, -height,
                                   on_panel_effect_complete,
                                   GINT_TO_POINTER (FALSE));
              priv->panel_out = FALSE;
            }
        }
      else if (event_y < PANEL_SLIDE_THRESHOLD)
        {
          gint  x = clutter_actor_get_x (priv->panel);

          priv->panel_out_in_progress  = TRUE;
          clutter_effect_move (priv->panel_slide_effect,
                               priv->panel, x, 0,
                               on_panel_effect_complete,
                               GINT_TO_POINTER (TRUE));

          priv->panel_out = TRUE;
        }
    }
  else if (event->type == CLUTTER_KEY_RELEASE)
    {
      ClutterKeyEvent *kev = (ClutterKeyEvent *) event;

      g_print ("*** key press event (key:%c) ***\n",
	       clutter_key_event_symbol (kev));

    }

  if (!capture && (event->type == CLUTTER_BUTTON_PRESS))
    {
      toggle_switcher ();
    }

  return FALSE;
}

static ClutterActor *
make_panel (gint width)
{
  ClutterActor *panel;
  ClutterActor *background;
  ClutterColor  clr = {0x44, 0x44, 0x44, 0x7f};

  panel = clutter_group_new ();

  /* FIME -- size and color */
  background = clutter_rectangle_new_with_color (&clr);
  clutter_container_add_actor (CLUTTER_CONTAINER (panel), background);
  clutter_actor_set_size (background, width, PANEL_HEIGHT);

  return panel;
}

/*
 * Core of the plugin init function, called for initial initialization and
 * by the reload() function. Returns TRUE on success.
 */
static gboolean
do_init (const char *params)
{
  MutterPlugin *plugin = mutter_get_plugin ();

  PluginPrivate *priv = g_new0 (PluginPrivate, 1);
  guint          destroy_timeout     = DESTROY_TIMEOUT;
  guint          minimize_timeout    = MINIMIZE_TIMEOUT;
  guint          maximize_timeout    = MAXIMIZE_TIMEOUT;
  guint          map_timeout         = MAP_TIMEOUT;
  guint          switch_timeout      = SWITCH_TIMEOUT;
  guint          panel_slide_timeout = PANEL_SLIDE_TIMEOUT;
  const gchar   *name;
  ClutterActor  *overlay;
  ClutterActor  *panel;
  gint           screen_width, screen_height;

  plugin->plugin_private = priv;

  name = plugin->name;
  plugin->name = _(name);

  mutter_plugin_query_screen_size (plugin, &screen_width, &screen_height);

  if (params)
    {
      if (strstr (params, "debug"))
        {
          g_debug ("%s: Entering debug mode.",
                   plugin->name);

          priv->debug_mode = TRUE;

          /*
           * Double the effect duration to make them easier to observe.
           */
          destroy_timeout  *= 2;
          minimize_timeout *= 2;
          maximize_timeout *= 2;
          map_timeout      *= 2;
          switch_timeout   *= 2;
        }
    }

  priv->destroy_effect
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (
							destroy_timeout),
                                    CLUTTER_ALPHA_SINE_INC);


  priv->minimize_effect
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (
							minimize_timeout),
                                    CLUTTER_ALPHA_SINE_INC);

  priv->maximize_effect
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (
							maximize_timeout),
                                    CLUTTER_ALPHA_SINE_INC);

  priv->map_effect
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (
							map_timeout),
                                    CLUTTER_ALPHA_SINE_INC);

  priv->switch_workspace_effect
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (
							switch_timeout),
                                    CLUTTER_ALPHA_SINE_INC);

  /* better syncing as multiple groups run off this */
  clutter_effect_template_set_timeline_clone (priv->switch_workspace_effect,
                                              TRUE);

  priv->switch_workspace_arrow_effect
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (
							switch_timeout*4),
                                    CLUTTER_ALPHA_SINE_INC);

  overlay = mutter_plugin_get_overlay_group (plugin);

  panel = priv->panel = make_panel (screen_width);
  clutter_container_add_actor (CLUTTER_CONTAINER (overlay), panel);

  priv->panel_slide_effect
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (
							panel_slide_timeout),
                                    CLUTTER_ALPHA_SINE_INC);

  clutter_actor_set_position (panel, 0,
                              -clutter_actor_get_height (panel));

  /*
   * Set up the stage even processing
   */
  disable_stage (plugin);

  /*
   * Hook to the captured signal, so we get to see all events before our
   * children and do not interfere with their event processing.
   */
  g_signal_connect (mutter_plugin_get_stage (plugin),
                    "captured-event", G_CALLBACK (stage_input_cb),
                    GINT_TO_POINTER (TRUE));

  g_signal_connect (mutter_plugin_get_stage (plugin),
                    "button-press-event", G_CALLBACK (stage_input_cb),
                    GINT_TO_POINTER (FALSE));

  mutter_plugin_set_stage_input_area (plugin, 0, 0, screen_width, 1);

  clutter_set_motion_events_enabled (TRUE);

  return TRUE;
}

static void
free_plugin_private (PluginPrivate *priv)
{
  if (!priv)
    return;

  g_object_unref (priv->destroy_effect);
  g_object_unref (priv->minimize_effect);
  g_object_unref (priv->maximize_effect);
  g_object_unref (priv->switch_workspace_effect);
  g_object_unref (priv->switch_workspace_arrow_effect);

  g_free (priv);

  mutter_get_plugin()->plugin_private = NULL;
}

/*
 * Called by the plugin manager when we stuff like the command line parameters
 * changed.
 */
static gboolean
reload (const char *params)
{
  MutterPlugin  *plugin = mutter_get_plugin ();
  PluginPrivate *priv   = plugin->plugin_private;

  if (do_init (params))
    {
      /* Success; free the old private struct */
      free_plugin_private (priv);
      return TRUE;
    }
  else
    {
      /* Fail -- fall back to the old private. */
      plugin->plugin_private = priv;
    }

  return FALSE;
}

/*
 * GModule unload function -- do any cleanup required.
 */
G_MODULE_EXPORT void g_module_unload (GModule *module);
G_MODULE_EXPORT void g_module_unload (GModule *module)
{
  PluginPrivate *priv = mutter_get_plugin()->plugin_private;

  free_plugin_private (priv);
}
