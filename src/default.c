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
#include <gmodule.h>
#include <string.h>

#define DESTROY_TIMEOUT   250
#define MINIMIZE_TIMEOUT  250
#define MAXIMIZE_TIMEOUT  250
#define MAP_TIMEOUT       250
#define SWITCH_TIMEOUT    500

#define ACTOR_DATA_KEY "MCCP-Default-actor-data"
static GQuark actor_data_quark = 0;

static gboolean do_init    (const char *params);
static void     minimize   (MutterWindow *actor);
static void     map        (MutterWindow *actor);
static void     destroy    (MutterWindow *actor);
static void     maximize   (MutterWindow *actor,
                            gint x, gint y, gint width, gint height);
static void     unmaximize (MutterWindow *actor,
                            gint x, gint y, gint width, gint height);

static void switch_workspace (const GList **actors, gint from, gint to,
                              MetaMotionDirection direction);

static void kill_effect (MutterWindow *actor, gulong event);

static gboolean reload (const char *params);


/*
 * Create the plugin struct; function pointers initialized in
 * g_module_check_init().
 */
MUTTER_DECLARE_PLUGIN();

/*
 * Plugin private data that we store in the .plugin_private member.
 */
typedef struct _PluginState
{
  ClutterEffectTemplate *destroy_effect;
  ClutterEffectTemplate *minimize_effect;
  ClutterEffectTemplate *maximize_effect;
  ClutterEffectTemplate *map_effect;
  ClutterEffectTemplate *switch_workspace_effect;

  /* Valid only when switch_workspace effect is in progress */
  ClutterTimeline       *tml_switch_workspace1;
  ClutterTimeline       *tml_switch_workspace2;
  GList                **actors;
  ClutterActor          *desktop1;
  ClutterActor          *desktop2;

  gboolean               debug_mode : 1;
} PluginState;


/*
 * Per actor private data we attach to each actor.
 */
typedef struct _ActorPrivate
{
  ClutterActor *orig_parent;

  ClutterTimeline *tml_minimize;
  ClutterTimeline *tml_maximize;
  ClutterTimeline *tml_destroy;
  ClutterTimeline *tml_map;

  gboolean      is_minimized : 1;
  gboolean      is_maximized : 1;
} ActorPrivate;

static PluginState *plugin_state;

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
  PluginState  *state = plugin_state;
  GList        *l     = *((GList**)data);
  MutterWindow *actor_for_cb = l->data;

  while (l)
    {
      ClutterActor *a = l->data;
      MutterWindow *mc_window = MUTTER_WINDOW (a);
      ActorPrivate *priv = get_actor_private (mc_window);

      if (priv->orig_parent)
        {
          clutter_actor_reparent (a, priv->orig_parent);
          priv->orig_parent = NULL;
        }

      l = l->next;
    }

  clutter_actor_destroy (state->desktop1);
  clutter_actor_destroy (state->desktop2);

  state->actors = NULL;
  state->tml_switch_workspace1 = NULL;
  state->tml_switch_workspace2 = NULL;
  state->desktop1 = NULL;
  state->desktop2 = NULL;

  mutter_plugin_effect_completed (mutter_get_plugin(), actor_for_cb,
                                  MUTTER_PLUGIN_SWITCH_WORKSPACE);
}

static void
switch_workspace (const GList **actors, gint from, gint to,
                  MetaMotionDirection direction)
{
  MutterPlugin *plugin = mutter_get_plugin();
  PluginState  *state  = plugin_state;
  GList        *l;
  gint          n_workspaces;
  ClutterActor *workspace0  = clutter_group_new ();
  ClutterActor *workspace1  = clutter_group_new ();
  ClutterActor *stage;
  int           screen_width, screen_height;

  stage = mutter_plugin_get_stage (plugin);

  mutter_plugin_query_screen_size (plugin,
					      &screen_width,
					      &screen_height);
  clutter_actor_set_anchor_point (workspace1,
                                  screen_width,
                                  screen_height);
  clutter_actor_set_position (workspace1,
                              screen_width,
                              screen_height);

  clutter_actor_set_scale (workspace1, 0.0, 0.0);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), workspace1);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), workspace0);

  if (from == to)
    {
      mutter_plugin_effect_completed (mutter_get_plugin(), NULL,
                           MUTTER_PLUGIN_SWITCH_WORKSPACE);
      return;
    }

  n_workspaces = g_list_length (plugin->work_areas);

  l = g_list_last (*((GList**) actors));

  while (l)
    {
      MutterWindow *mc_window	= l->data;
      ActorPrivate *priv	= get_actor_private (mc_window);
      ClutterActor *window	= CLUTTER_ACTOR (mc_window);
      gint          win_workspace;

      win_workspace = mutter_window_get_workspace (mc_window);

      if (win_workspace == to || win_workspace == from)
        {
          gint x, y;
          guint w, h;

          clutter_actor_get_position (window, &x, &y);
          clutter_actor_get_size (window, &w, &h);

          priv->orig_parent = clutter_actor_get_parent (window);

          clutter_actor_reparent (window,
				  win_workspace == to ? workspace1 : workspace0);
          clutter_actor_show_all (window);
          clutter_actor_raise_top (window);
        }
      else if (win_workspace < 0)
        {
          /* Sticky window */
          priv->orig_parent = NULL;
        }
      else
        {
          /* Window on some other desktop */
          clutter_actor_hide (window);
          priv->orig_parent = NULL;
        }

      l = l->prev;
    }

  state->actors   = (GList **)actors;
  state->desktop1 = workspace0;
  state->desktop2 = workspace1;

  state->tml_switch_workspace2 =
    clutter_effect_scale (state->switch_workspace_effect,
                          workspace1, 1.0, 1.0,
                          on_switch_workspace_effect_complete,
                          (gpointer)actors);

  state->tml_switch_workspace1 =
    clutter_effect_scale (state->switch_workspace_effect,
                          workspace0, 0.0, 0.0,
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
  ActorPrivate *apriv;
  MutterWindow *mc_window = MUTTER_WINDOW (actor);

  apriv = get_actor_private (MUTTER_WINDOW (actor));
  apriv->tml_minimize = NULL;

  clutter_actor_hide (actor);

  /* FIXME - we shouldn't assume the original scale, it should be saved
   * at the start of the effect */
  clutter_actor_set_scale (actor, 1.0, 1.0);
  clutter_actor_move_anchor_point_from_gravity (actor,
                                                CLUTTER_GRAVITY_NORTH_WEST);

  /* Now notify the manager that we are done with this effect */
  mutter_plugin_effect_completed (mutter_get_plugin(), mc_window,
                                  MUTTER_PLUGIN_MINIMIZE);
}

/*
 * Simple minimize handler: it applies a scale effect (which must be reversed on
 * completion).
 */
static void
minimize (MutterWindow *mc_window)
{
  PluginState        *state  = plugin_state;
  MetaCompWindowType  type;
  ClutterActor       *actor  = CLUTTER_ACTOR (mc_window);

  type = mutter_window_get_window_type (mc_window);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv = get_actor_private (mc_window);

      apriv->is_minimized = TRUE;

      clutter_actor_move_anchor_point_from_gravity (actor,
                                                    CLUTTER_GRAVITY_CENTER);

      apriv->tml_minimize = clutter_effect_scale (state->minimize_effect,
                                                  actor,
                                                  0.0,
                                                  0.0,
                                                  (ClutterEffectCompleteFunc)
                                                  on_minimize_effect_complete,
                                                  NULL);
    }
  else
    mutter_plugin_effect_completed (mutter_get_plugin(), mc_window,
                                    MUTTER_PLUGIN_MINIMIZE);
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
  MutterWindow  *mc_window = MUTTER_WINDOW (actor);
  ActorPrivate  *apriv     = get_actor_private (mc_window);

  apriv->tml_maximize = NULL;

  /* FIXME - don't assume the original scale was 1.0 */
  clutter_actor_set_scale (actor, 1.0, 1.0);
  clutter_actor_move_anchor_point_from_gravity (actor,
                                                CLUTTER_GRAVITY_NORTH_WEST);

  /* Now notify the manager that we are done with this effect */
  mutter_plugin_effect_completed (mutter_get_plugin(), mc_window,
                                  MUTTER_PLUGIN_MAXIMIZE);
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
maximize (MutterWindow *mc_window,
          gint end_x, gint end_y, gint end_width, gint end_height)
{
  MetaCompWindowType  type;
  ClutterActor	     *actor = CLUTTER_ACTOR (mc_window);

  gdouble  scale_x    = 1.0;
  gdouble  scale_y    = 1.0;
  gint     anchor_x   = 0;
  gint     anchor_y   = 0;

  type = mutter_window_get_window_type (mc_window);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv = get_actor_private (mc_window);
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

      apriv->tml_maximize =
	clutter_effect_scale (plugin_state->maximize_effect,
			      actor,
			      scale_x,
			      scale_y,
			      (ClutterEffectCompleteFunc)
                              on_maximize_effect_complete,
                              NULL);

      return;
    }

  mutter_plugin_effect_completed (mutter_get_plugin(), mc_window,
                                  MUTTER_PLUGIN_MAXIMIZE);
}

/*
 * See comments on the maximize() function.
 *
 * (Just a skeleton code.)
 */
static void
unmaximize (MutterWindow *mc_window,
            gint end_x, gint end_y, gint end_width, gint end_height)
{
  MetaCompWindowType type = mutter_window_get_window_type (mc_window);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv = get_actor_private (mc_window);

      apriv->is_maximized = FALSE;
    }

  /* Do this conditionally, if the effect requires completion callback. */
  mutter_plugin_effect_completed (mutter_get_plugin(), mc_window,
                                  MUTTER_PLUGIN_UNMAXIMIZE);
}

static void
on_map_effect_complete (ClutterActor *actor, gpointer data)
{
  /*
   * Must reverse the effect of the effect.
   */
  MutterWindow  *mc_window = MUTTER_WINDOW (actor);
  ActorPrivate  *apriv     = get_actor_private (mc_window);

  apriv->tml_map = NULL;

  clutter_actor_move_anchor_point_from_gravity (actor,
                                                CLUTTER_GRAVITY_NORTH_WEST);

  /* Now notify the manager that we are done with this effect */
  mutter_plugin_effect_completed (mutter_get_plugin(), mc_window, MUTTER_PLUGIN_MAP);
}

/*
 * Simple map handler: it applies a scale effect which must be reversed on
 * completion).
 */
static void
map (MutterWindow *mc_window)
{
  MetaCompWindowType  type;
  ClutterActor       *actor = CLUTTER_ACTOR (mc_window);

  type = mutter_window_get_window_type (mc_window);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv = get_actor_private (mc_window);

      clutter_actor_move_anchor_point_from_gravity (actor,
                                                    CLUTTER_GRAVITY_CENTER);

      clutter_actor_set_scale (actor, 0.0, 0.0);
      clutter_actor_show (actor);

      apriv->tml_map = clutter_effect_scale (plugin_state->map_effect,
                                             actor,
                                             1.0,
                                             1.0,
                                             (ClutterEffectCompleteFunc)
                                             on_map_effect_complete,
                                             NULL);

      apriv->is_minimized = FALSE;

    }
  else
    mutter_plugin_effect_completed (mutter_get_plugin(), mc_window,
                                    MUTTER_PLUGIN_MAP);
}

/*
 * Destroy effect completion callback; this is a simple effect that requires no
 * further action than notifying the manager that the effect is completed.
 */
static void
on_destroy_effect_complete (ClutterActor *actor, gpointer data)
{
  MutterPlugin *plugin = mutter_get_plugin();
  MutterWindow *mc_window = MUTTER_WINDOW (actor);
  ActorPrivate *apriv = get_actor_private (mc_window);

  apriv->tml_destroy = NULL;

  mutter_plugin_effect_completed (plugin, mc_window,
                                  MUTTER_PLUGIN_DESTROY);
}

/*
 * Simple TV-out like effect.
 */
static void
destroy (MutterWindow *mc_window)
{
  MetaCompWindowType   type;
  ClutterActor	      *actor = CLUTTER_ACTOR (mc_window);

  type = mutter_window_get_window_type (mc_window);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv = get_actor_private (mc_window);

      clutter_actor_move_anchor_point_from_gravity (actor,
                                                    CLUTTER_GRAVITY_CENTER);

      apriv->tml_destroy = clutter_effect_scale (plugin_state->destroy_effect,
                                                 actor,
                                                 1.0,
                                                 0.0,
                                                 (ClutterEffectCompleteFunc)
                                                 on_destroy_effect_complete,
                                                 NULL);
    }
  else
    mutter_plugin_effect_completed (mutter_get_plugin(), mc_window,
                                    MUTTER_PLUGIN_DESTROY);
}

static void
kill_effect (MutterWindow *mc_window, gulong event)
{
  ActorPrivate *apriv;
  ClutterActor *actor  = CLUTTER_ACTOR (mc_window);

  if (event & MUTTER_PLUGIN_SWITCH_WORKSPACE)
    {
      PluginState *state = plugin_state;

      if (state->tml_switch_workspace1)
        {
          clutter_timeline_stop (state->tml_switch_workspace1);
          clutter_timeline_stop (state->tml_switch_workspace2);
          on_switch_workspace_effect_complete (state->desktop1, state->actors);
        }

      if (!(event & ~MUTTER_PLUGIN_SWITCH_WORKSPACE))
        {
          /* Workspace switch only, nothing more to do */
          return;
        }
    }

  apriv = get_actor_private (mc_window);

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
  plugin->name = "Default Effects";

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

  /* The reload handler */
  plugin->reload           = reload;

  return NULL;
}

/*
 * Core of the plugin init function, called for initial initialization and
 * by the reload() function. Returns TRUE on success.
 */
static gboolean
do_init (const char *params)
{
  guint destroy_timeout  = DESTROY_TIMEOUT;
  guint minimize_timeout = MINIMIZE_TIMEOUT;
  guint maximize_timeout = MAXIMIZE_TIMEOUT;
  guint map_timeout      = MAP_TIMEOUT;
  guint switch_timeout   = SWITCH_TIMEOUT;

  plugin_state = g_new0 (PluginState, 1);

  if (params)
    {
      if (strstr (params, "debug"))
        {
          g_debug ("%s: Entering debug mode.", mutter_get_plugin()->name);

          plugin_state->debug_mode = TRUE;

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

  plugin_state->destroy_effect
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (
							destroy_timeout),
                                    CLUTTER_ALPHA_SINE_INC);


  plugin_state->minimize_effect
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (
							minimize_timeout),
                                    CLUTTER_ALPHA_SINE_INC);

  plugin_state->maximize_effect
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (
							maximize_timeout),
                                    CLUTTER_ALPHA_SINE_INC);

  plugin_state->map_effect
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (
							map_timeout),
                                    CLUTTER_ALPHA_SINE_INC);

  plugin_state->switch_workspace_effect
    =  clutter_effect_template_new (clutter_timeline_new_for_duration (
							switch_timeout),
                                    CLUTTER_ALPHA_SINE_INC);

  return TRUE;
}

static void
free_plugin_private (PluginState *state)
{
  if (!state)
    return;

  g_object_unref (state->destroy_effect);
  g_object_unref (state->minimize_effect);
  g_object_unref (state->maximize_effect);
  g_object_unref (state->switch_workspace_effect);

  g_free (state);
}

/*
 * Called by the plugin manager when we stuff like the command line parameters
 * changed.
 */
static gboolean
reload (const char *params)
{
  PluginState *state;

  state = plugin_state;

  if (do_init (params))
    {
      /* Success; free the old state */
      free_plugin_private (plugin_state);
      return TRUE;
    }
  else
    {
      /* Fail -- fall back to the old state. */
      plugin_state = state;
    }

  return FALSE;
}

/*
 * GModule unload function -- do any cleanup required.
 */
G_MODULE_EXPORT void g_module_unload (GModule *module);
G_MODULE_EXPORT void
g_module_unload (GModule *module)
{
  free_plugin_private (plugin_state);
}

