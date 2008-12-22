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

#define MUTTER_TYPE_DEFAULT_PLUGIN            (mutter_default_plugin_get_type ())
#define MUTTER_DEFAULT_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MUTTER_TYPE_DEFAULT_PLUGIN, MutterDefaultPlugin))
#define MUTTER_DEFAULT_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MUTTER_TYPE_DEFAULT_PLUGIN, MutterDefaultPluginClass))
#define MUTTER_IS_DEFAULT_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MUTTER_DEFAULT_PLUGIN_TYPE))
#define MUTTER_IS_DEFAULT_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MUTTER_TYPE_DEFAULT_PLUGIN))
#define MUTTER_DEFAULT_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MUTTER_TYPE_DEFAULT_PLUGIN, MutterDefaultPluginClass))

#define MUTTER_DEFAULT_PLUGIN_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), MUTTER_TYPE_DEFAULT_PLUGIN, MutterDefaultPluginPrivate))

typedef struct _MutterDefaultPlugin        MutterDefaultPlugin;
typedef struct _MutterDefaultPluginClass   MutterDefaultPluginClass;
typedef struct _MutterDefaultPluginPrivate MutterDefaultPluginPrivate;

struct _MutterDefaultPlugin
{
  MutterPlugin parent;

  MutterDefaultPluginPrivate *priv;
};

struct _MutterDefaultPluginClass
{
  MutterPluginClass parent_class;
};

static GQuark actor_data_quark = 0;

static void     minimize   (MutterPlugin *plugin,
                            MutterWindow *actor);
static void     map        (MutterPlugin *plugin,
                            MutterWindow *actor);
static void     destroy    (MutterPlugin *plugin,
                            MutterWindow *actor);
static void     maximize   (MutterPlugin *plugin,
                            MutterWindow *actor,
                            gint x, gint y, gint width, gint height);
static void     unmaximize (MutterPlugin *plugin,
                            MutterWindow *actor,
                            gint x, gint y, gint width, gint height);

static void switch_workspace (MutterPlugin *plugin,
                              const GList **actors, gint from, gint to,
                              MetaMotionDirection direction);

static void kill_effect (MutterPlugin *plugin,
                         MutterWindow *actor, gulong event);

static const MutterPluginInfo * plugin_info (MutterPlugin *plugin);

MUTTER_PLUGIN_DECLARE(MutterDefaultPlugin, mutter_default_plugin);

/*
 * Plugin private data that we store in the .plugin_private member.
 */
struct _MutterDefaultPluginPrivate
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

  MutterPluginInfo       info;

  gboolean               debug_mode : 1;
};

static void
mutter_default_plugin_dispose (GObject *object)
{
  MutterDefaultPluginPrivate *priv = MUTTER_DEFAULT_PLUGIN (object)->priv;

  g_object_unref (priv->destroy_effect);
  g_object_unref (priv->minimize_effect);
  g_object_unref (priv->maximize_effect);
  g_object_unref (priv->switch_workspace_effect);

  G_OBJECT_CLASS (mutter_default_plugin_parent_class)->dispose (object);
}

static void
mutter_default_plugin_finalize (GObject *object)
{
  G_OBJECT_CLASS (mutter_default_plugin_parent_class)->finalize (object);
}

static void
mutter_default_plugin_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mutter_default_plugin_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
mutter_default_plugin_constructed (GObject *object)
{
  MutterPlugin               *plugin = MUTTER_PLUGIN (object);
  MutterDefaultPluginPrivate *priv   = MUTTER_DEFAULT_PLUGIN (object)->priv;

  guint destroy_timeout  = DESTROY_TIMEOUT;
  guint minimize_timeout = MINIMIZE_TIMEOUT;
  guint maximize_timeout = MAXIMIZE_TIMEOUT;
  guint map_timeout      = MAP_TIMEOUT;
  guint switch_timeout   = SWITCH_TIMEOUT;

  if (mutter_plugin_debug_mode (plugin))
    {
      g_debug ("Plugin %s: Entering debug mode.", priv->info.name);

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
}

static void
mutter_default_plugin_class_init (MutterDefaultPluginClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  MutterPluginClass *plugin_class  = MUTTER_PLUGIN_CLASS (klass);

  gobject_class->finalize        = mutter_default_plugin_finalize;
  gobject_class->dispose         = mutter_default_plugin_dispose;
  gobject_class->constructed     = mutter_default_plugin_constructed;
  gobject_class->set_property    = mutter_default_plugin_set_property;
  gobject_class->get_property    = mutter_default_plugin_get_property;

  plugin_class->map              = map;
  plugin_class->minimize         = minimize;
  plugin_class->maximize         = maximize;
  plugin_class->unmaximize       = unmaximize;
  plugin_class->destroy          = destroy;
  plugin_class->switch_workspace = switch_workspace;
  plugin_class->kill_effect      = kill_effect;
  plugin_class->plugin_info      = plugin_info;

  g_type_class_add_private (gobject_class, sizeof (MutterDefaultPluginPrivate));
}

static void
mutter_default_plugin_init (MutterDefaultPlugin *self)
{
  MutterDefaultPluginPrivate *priv;

  self->priv = priv = MUTTER_DEFAULT_PLUGIN_GET_PRIVATE (self);

  priv->info.name        = "Default Effects";
  priv->info.version     = "0.1";
  priv->info.author      = "Intel Corp.";
  priv->info.license     = "GPL";
  priv->info.description = "This is an example of a plugin implementation.";
}


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

typedef struct SwitchWorkspaceData
{
  MutterPlugin  *plugin;
  const GList  **actors;
} SwitchWorkspaceData;

static void
on_switch_workspace_effect_complete (ClutterActor *group, gpointer data)
{
  SwitchWorkspaceData        *sw_data = data;
  MutterPlugin               *plugin  = sw_data->plugin;
  MutterDefaultPluginPrivate *priv = MUTTER_DEFAULT_PLUGIN (plugin)->priv;
  GList        *l     = *((GList**)sw_data->actors);
  MutterWindow *actor_for_cb = l->data;

  while (l)
    {
      ClutterActor *a = l->data;
      MutterWindow *mc_window = MUTTER_WINDOW (a);
      ActorPrivate *apriv = get_actor_private (mc_window);

      if (apriv->orig_parent)
        {
          clutter_actor_reparent (a, apriv->orig_parent);
          apriv->orig_parent = NULL;
        }

      l = l->next;
    }

  clutter_actor_destroy (priv->desktop1);
  clutter_actor_destroy (priv->desktop2);

  priv->actors = NULL;
  priv->tml_switch_workspace1 = NULL;
  priv->tml_switch_workspace2 = NULL;
  priv->desktop1 = NULL;
  priv->desktop2 = NULL;

  g_free (data);

  mutter_plugin_effect_completed (plugin, actor_for_cb,
                                  MUTTER_PLUGIN_SWITCH_WORKSPACE);
}

static void
switch_workspace (MutterPlugin *plugin,
                  const GList **actors, gint from, gint to,
                  MetaMotionDirection direction)
{
  MutterDefaultPluginPrivate *priv = MUTTER_DEFAULT_PLUGIN (plugin)->priv;
  GList        *l;
  gint          n_workspaces;
  ClutterActor *workspace0  = clutter_group_new ();
  ClutterActor *workspace1  = clutter_group_new ();
  ClutterActor *stage;
  int           screen_width, screen_height;
  MetaScreen   *screen = mutter_plugin_get_screen (plugin);
  SwitchWorkspaceData *sw_data = g_new (SwitchWorkspaceData, 1);

  sw_data->plugin = plugin;
  sw_data->actors = actors;

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
      mutter_plugin_effect_completed (plugin, NULL,
                           MUTTER_PLUGIN_SWITCH_WORKSPACE);
      return;
    }

  n_workspaces = meta_screen_get_n_workspaces (screen);

  l = g_list_last (*((GList**) actors));

  while (l)
    {
      MutterWindow *mc_window	= l->data;
      ActorPrivate *apriv	= get_actor_private (mc_window);
      ClutterActor *window	= CLUTTER_ACTOR (mc_window);
      gint          win_workspace;

      win_workspace = mutter_window_get_workspace (mc_window);

      if (win_workspace == to || win_workspace == from)
        {
          gint x, y;
          guint w, h;

          clutter_actor_get_position (window, &x, &y);
          clutter_actor_get_size (window, &w, &h);

          apriv->orig_parent = clutter_actor_get_parent (window);

          clutter_actor_reparent (window,
				  win_workspace == to ? workspace1 : workspace0);
          clutter_actor_show_all (window);
          clutter_actor_raise_top (window);
        }
      else if (win_workspace < 0)
        {
          /* Sticky window */
          apriv->orig_parent = NULL;
        }
      else
        {
          /* Window on some other desktop */
          clutter_actor_hide (window);
          apriv->orig_parent = NULL;
        }

      l = l->prev;
    }

  priv->actors   = (GList **)actors;
  priv->desktop1 = workspace0;
  priv->desktop2 = workspace1;

  priv->tml_switch_workspace2 =
    clutter_effect_scale (priv->switch_workspace_effect,
                          workspace1, 1.0, 1.0,
                          on_switch_workspace_effect_complete,
                          (gpointer)sw_data);

  priv->tml_switch_workspace1 =
    clutter_effect_scale (priv->switch_workspace_effect,
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
  MutterPlugin *plugin = data;
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
  mutter_plugin_effect_completed (plugin, mc_window,
                                  MUTTER_PLUGIN_MINIMIZE);
}

/*
 * Simple minimize handler: it applies a scale effect (which must be reversed on
 * completion).
 */
static void
minimize (MutterPlugin *plugin, MutterWindow *mc_window)
{
  MutterDefaultPluginPrivate *priv = MUTTER_DEFAULT_PLUGIN (plugin)->priv;
  MetaCompWindowType          type;
  ClutterActor               *actor  = CLUTTER_ACTOR (mc_window);

  type = mutter_window_get_window_type (mc_window);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv = get_actor_private (mc_window);

      apriv->is_minimized = TRUE;

      clutter_actor_move_anchor_point_from_gravity (actor,
                                                    CLUTTER_GRAVITY_CENTER);

      apriv->tml_minimize = clutter_effect_scale (priv->minimize_effect,
                                                  actor,
                                                  0.0,
                                                  0.0,
                                                  (ClutterEffectCompleteFunc)
                                                  on_minimize_effect_complete,
                                                  plugin);
    }
  else
    mutter_plugin_effect_completed (plugin, mc_window,
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
  MutterPlugin * plugin = data;
  MutterWindow  *mc_window = MUTTER_WINDOW (actor);
  ActorPrivate  *apriv     = get_actor_private (mc_window);

  apriv->tml_maximize = NULL;

  /* FIXME - don't assume the original scale was 1.0 */
  clutter_actor_set_scale (actor, 1.0, 1.0);
  clutter_actor_move_anchor_point_from_gravity (actor,
                                                CLUTTER_GRAVITY_NORTH_WEST);

  /* Now notify the manager that we are done with this effect */
  mutter_plugin_effect_completed (plugin, mc_window,
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
maximize (MutterPlugin *plugin,
          MutterWindow *mc_window,
          gint end_x, gint end_y, gint end_width, gint end_height)
{
  MutterDefaultPluginPrivate *priv = MUTTER_DEFAULT_PLUGIN (plugin)->priv;
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
	clutter_effect_scale (priv->maximize_effect,
			      actor,
			      scale_x,
			      scale_y,
			      (ClutterEffectCompleteFunc)
                              on_maximize_effect_complete,
                              plugin);

      return;
    }

  mutter_plugin_effect_completed (plugin, mc_window,
                                  MUTTER_PLUGIN_MAXIMIZE);
}

/*
 * See comments on the maximize() function.
 *
 * (Just a skeleton code.)
 */
static void
unmaximize (MutterPlugin *plugin,
            MutterWindow *mc_window,
            gint end_x, gint end_y, gint end_width, gint end_height)
{
  MetaCompWindowType type = mutter_window_get_window_type (mc_window);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv = get_actor_private (mc_window);

      apriv->is_maximized = FALSE;
    }

  /* Do this conditionally, if the effect requires completion callback. */
  mutter_plugin_effect_completed (plugin, mc_window,
                                  MUTTER_PLUGIN_UNMAXIMIZE);
}

static void
on_map_effect_complete (ClutterActor *actor, gpointer data)
{
  /*
   * Must reverse the effect of the effect.
   */
  MutterPlugin  *plugin = data;
  MutterWindow  *mc_window = MUTTER_WINDOW (actor);
  ActorPrivate  *apriv     = get_actor_private (mc_window);

  apriv->tml_map = NULL;

  clutter_actor_move_anchor_point_from_gravity (actor,
                                                CLUTTER_GRAVITY_NORTH_WEST);

  /* Now notify the manager that we are done with this effect */
  mutter_plugin_effect_completed (plugin, mc_window, MUTTER_PLUGIN_MAP);
}

/*
 * Simple map handler: it applies a scale effect which must be reversed on
 * completion).
 */
static void
map (MutterPlugin *plugin, MutterWindow *mc_window)
{
  MutterDefaultPluginPrivate *priv = MUTTER_DEFAULT_PLUGIN (plugin)->priv;
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

      apriv->tml_map = clutter_effect_scale (priv->map_effect,
                                             actor,
                                             1.0,
                                             1.0,
                                             (ClutterEffectCompleteFunc)
                                             on_map_effect_complete,
                                             plugin);

      apriv->is_minimized = FALSE;

    }
  else
    mutter_plugin_effect_completed (plugin, mc_window,
                                    MUTTER_PLUGIN_MAP);
}

/*
 * Destroy effect completion callback; this is a simple effect that requires no
 * further action than notifying the manager that the effect is completed.
 */
static void
on_destroy_effect_complete (ClutterActor *actor, gpointer data)
{
  MutterPlugin *plugin = data;
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
destroy (MutterPlugin *plugin, MutterWindow *mc_window)
{
  MutterDefaultPluginPrivate *priv = MUTTER_DEFAULT_PLUGIN (plugin)->priv;
  MetaCompWindowType   type;
  ClutterActor	      *actor = CLUTTER_ACTOR (mc_window);

  type = mutter_window_get_window_type (mc_window);

  if (type == META_COMP_WINDOW_NORMAL)
    {
      ActorPrivate *apriv = get_actor_private (mc_window);

      clutter_actor_move_anchor_point_from_gravity (actor,
                                                    CLUTTER_GRAVITY_CENTER);

      apriv->tml_destroy = clutter_effect_scale (priv->destroy_effect,
                                                 actor,
                                                 1.0,
                                                 0.0,
                                                 (ClutterEffectCompleteFunc)
                                                 on_destroy_effect_complete,
                                                 plugin);
    }
  else
    mutter_plugin_effect_completed (plugin, mc_window,
                                    MUTTER_PLUGIN_DESTROY);
}

static void
kill_effect (MutterPlugin *plugin, MutterWindow *mc_window, gulong event)
{
  ActorPrivate *apriv;
  ClutterActor *actor  = CLUTTER_ACTOR (mc_window);

  if (event & MUTTER_PLUGIN_SWITCH_WORKSPACE)
    {
      MutterDefaultPluginPrivate *priv = MUTTER_DEFAULT_PLUGIN (plugin)->priv;

      if (priv->tml_switch_workspace1)
        {
          clutter_timeline_stop (priv->tml_switch_workspace1);
          clutter_timeline_stop (priv->tml_switch_workspace2);
          on_switch_workspace_effect_complete (priv->desktop1, priv->actors);
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

static const MutterPluginInfo *
plugin_info (MutterPlugin *plugin)
{
  MutterDefaultPluginPrivate *priv = MUTTER_DEFAULT_PLUGIN (plugin)->priv;

  return &priv->info;
}
