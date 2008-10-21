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

#define DESTROY_TIMEOUT     250
#define MINIMIZE_TIMEOUT    250
#define MAXIMIZE_TIMEOUT    250
#define MAP_TIMEOUT         250
#define SWITCH_TIMEOUT      500
#define PANEL_SLIDE_TIMEOUT 250;                \

#define PANEL_SLIDE_THRESHOLD 2
#define PANEL_HEIGHT          40
#define ACTOR_DATA_KEY "MCCP-scratch-actor-data"
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
 * First we create the header struct and initialize its static members.
 * Any dynamically allocated data should be initialized in the
 * init () function below.
 */
G_MODULE_EXPORT MutterPlugin mutter_plugin =
  {
    /*
     * These are predefined values; do not modify.
     */
    .version_major = METACITY_MAJOR_VERSION,
    .version_minor = METACITY_MINOR_VERSION,
    .version_micro = METACITY_MICRO_VERSION,
    .version_api   = METACITY_CLUTTER_PLUGIN_API_VERSION,

    /* Human readable name (for use in UI) */
    .name = "Experimental effects",

    /* Plugin load time initialiser */
    .do_init = do_init,

    /* Effect handlers */
    .minimize         = minimize,
    .destroy          = destroy,
    .map              = map,
    .maximize         = maximize,
    .unmaximize       = unmaximize,
    .switch_workspace = switch_workspace,
    .kill_effect      = kill_effect,
    .xevent_filter    = xevent_filter,

    /* The reload handler */
    .reload           = reload
  };

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
  ClutterEffectTemplate *panel_slide_effect;

  /* Valid only when switch_workspace effect is in progress */
  ClutterTimeline       *tml_switch_workspace1;
  ClutterTimeline       *tml_switch_workspace2;
  GList                **actors;
  ClutterActor          *desktop1;
  ClutterActor          *desktop2;

  ClutterActor          *panel;

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

static inline
MutterPlugin *
get_plugin ()
{
  return &mutter_plugin;
}

static void
on_switch_workspace_effect_complete (ClutterActor *group, gpointer data)
{
  MutterPlugin   *plugin = get_plugin ();
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
  MutterPlugin  *plugin = get_plugin ();
  PluginPrivate *ppriv  = plugin->plugin_private;
  GList         *l;
  gint           n_workspaces;
  ClutterActor  *group1  = clutter_group_new ();
  ClutterActor  *group2  = clutter_group_new ();
  ClutterActor  *stage;
  gint           screen_width;
  gint           screen_height;

  stage = mutter_plugin_get_stage (plugin);

  mutter_plugin_query_screen_size (plugin, &screen_width, &screen_height);

#if 1
  clutter_actor_set_anchor_point (group2, screen_width, screen_height);

  clutter_actor_set_position (group2, screen_width, screen_height);
#endif

  clutter_actor_set_scale (group2, 0.0, 0.0);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group2);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group1);

  if (from == to)
    {
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

  ppriv->actors   = (GList **)actors;
  ppriv->desktop1 = group1;
  ppriv->desktop2 = group2;

  ppriv->tml_switch_workspace2 = clutter_effect_scale (
                                           ppriv->switch_workspace_effect,
                                           group2, 1.0, 1.0,
                                           on_switch_workspace_effect_complete,
                                           (gpointer)actors);

  ppriv->tml_switch_workspace1 = clutter_effect_scale (
                                           ppriv->switch_workspace_effect,
                                           group1, 0.0, 0.0,
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
  MutterPlugin *plugin = get_plugin ();
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
  MutterPlugin      *plugin = get_plugin ();
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
  MutterPlugin *plugin = get_plugin ();
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
  MutterPlugin       *plugin = get_plugin ();
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
  MutterPlugin       *plugin = get_plugin ();
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
  MutterPlugin *plugin = get_plugin ();
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
  MutterPlugin       *plugin = get_plugin ();
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
  MutterPlugin *plugin = get_plugin ();
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
  MutterPlugin       *plugin = get_plugin ();
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
  MutterPlugin  *plugin   = get_plugin ();
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
  MutterPlugin *plugin = get_plugin ();
  ClutterActor                *stage;

  stage = mutter_plugin_get_stage (plugin);

  clutter_x11_handle_event (xev);

  return FALSE;
}

static void
kill_effect (MutterWindow *mcw, gulong event)
{
  MutterPlugin *plugin = get_plugin ();
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


#if 0
const gchar * g_module_check_init (GModule *module);
const gchar *
g_module_check_init (GModule *module)
{
  /*
   * Unused; left here for documentation purposes.
   *
   * NB: this function is called *before* the plugin manager does its own
   *     initialization of the plugin struct, so you cannot process fields
   *     like .params in here; use the init function below instead.
   */
  return NULL;
}
#endif

static gboolean
stage_input_cb (ClutterActor *stage, ClutterEvent *event, gpointer data)
{
  if (event->type == CLUTTER_MOTION)
    {
      ClutterMotionEvent *mev = (ClutterMotionEvent *) event;
      MutterPlugin       *plugin = get_plugin ();
      PluginPrivate      *priv   = plugin->plugin_private;

      if (priv->panel_out_in_progress || priv->panel_back_in_progress)
        return FALSE;

      if (priv->panel_out)
        {
          guint height = clutter_actor_get_height (priv->panel);
          gint  x      = clutter_actor_get_x (priv->panel);

          if (mev->y > (gint)height)
            {
              priv->panel_back_in_progress  = TRUE;
              clutter_effect_move (priv->panel_slide_effect,
                                   priv->panel, x, -height,
                                   on_panel_effect_complete,
                                   GINT_TO_POINTER (FALSE));
              priv->panel_out = FALSE;
            }

          return FALSE;
        }
      else if (mev->y < PANEL_SLIDE_THRESHOLD)
        {
          gint  x = clutter_actor_get_x (priv->panel);

          priv->panel_out_in_progress  = TRUE;
          clutter_effect_move (priv->panel_slide_effect,
                               priv->panel, x, 0,
                               on_panel_effect_complete,
                               GINT_TO_POINTER (TRUE));

          priv->panel_out = TRUE;

          return FALSE;
        }

      return FALSE;
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
  MutterPlugin *plugin = get_plugin ();

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
                    "captured-event", G_CALLBACK (stage_input_cb), NULL);

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

  g_free (priv);

  get_plugin()->plugin_private = NULL;
}

/*
 * Called by the plugin manager when we stuff like the command line parameters
 * changed.
 */
static gboolean
reload (const char *params)
{
  MutterPlugin  *plugin = get_plugin ();
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
  PluginPrivate *priv = get_plugin()->plugin_private;

  free_plugin_private (priv);
}
