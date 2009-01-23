/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-behaviour-path
 * @short_description: A behaviour for moving actors along a #ClutterPath
 *
 * #ClutterBehaviourPath interpolates actors along a defined path.
 *
 * A path is described by a #ClutterPath object. The path can contain
 * straight line parts and bezier curves. If the path contains
 * %CLUTTER_PATH_MOVE_TO parts then the actors will jump to those
 * coordinates. This can be used make disjoint paths.
 *
 * When creating a path behaviour in a #ClutterScript, you can specify
 * the path property directly as a string. For example:
 *
 * |[
 * {
 *   "id"     : "spline-path",
 *   "type"   : "ClutterBehaviourPath",
 *   "path"   : "M 50 50 L 100 100",
 *   "alpha"  : {
 *      "timeline" : "main-timeline",
 *      "function" : "ramp
 *    }
 * }
 * ]|
 *
 * <note>If the alpha function is a periodic function, i.e. it returns to
 * 0 after reaching %CLUTTER_ALPHA_MAX_ALPHA, then the actors will walk
 * the path back to the starting #ClutterKnot.</note>
 *
 * #ClutterBehaviourPath is available since Clutter 0.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-actor.h"
#include "clutter-behaviour.h"
#include "clutter-behaviour-path.h"
#include "clutter-bezier.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-script-private.h"
#include "clutter-scriptable.h"

#include <math.h>

static void clutter_scriptable_iface_init (ClutterScriptableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterBehaviourPath,
                         clutter_behaviour_path,
                         CLUTTER_TYPE_BEHAVIOUR,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_SCRIPTABLE,
                                                clutter_scriptable_iface_init));

struct _ClutterBehaviourPathPrivate
{
  ClutterPath *path;
  guint        last_knot_passed;
};

#define CLUTTER_BEHAVIOUR_PATH_GET_PRIVATE(obj)    \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
               CLUTTER_TYPE_BEHAVIOUR_PATH,        \
               ClutterBehaviourPathPrivate))

enum
{
  KNOT_REACHED,

  LAST_SIGNAL
};

static guint path_signals[LAST_SIGNAL] = { 0, };

enum
{
  PROP_0,

  PROP_PATH
};

static void
actor_apply_knot_foreach (ClutterBehaviour *behaviour,
                          ClutterActor     *actor,
                          gpointer          data)
{
  ClutterKnot *knot = data;

  CLUTTER_NOTE (BEHAVIOUR, "Setting actor to %ix%i", knot->x, knot->y);

  clutter_actor_set_position (actor, knot->x, knot->y);
}

static void
clutter_behaviour_path_alpha_notify (ClutterBehaviour *behave,
                                     gdouble           alpha_value)
{
  ClutterBehaviourPath *pathb = CLUTTER_BEHAVIOUR_PATH (behave);
  ClutterBehaviourPathPrivate *priv = pathb->priv;
  ClutterKnot position;
  guint knot_num;

  if (priv->path)
    knot_num = clutter_path_get_position (priv->path, alpha_value, &position);
  else
    {
      memset (&position, 0, sizeof (position));
      knot_num = 0;
    }

  clutter_behaviour_actors_foreach (behave,
                                    actor_apply_knot_foreach,
                                    &position);

  if (knot_num != priv->last_knot_passed)
    {
      g_signal_emit (behave, path_signals[KNOT_REACHED], 0, knot_num);
      priv->last_knot_passed = knot_num;
    }
}

static void
clutter_behaviour_path_get_property (GObject      *gobject,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  ClutterBehaviourPath *pathb = CLUTTER_BEHAVIOUR_PATH (gobject);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_object (value, clutter_behaviour_path_get_path (pathb));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_path_set_property (GObject      *gobject,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ClutterBehaviourPath *pathb = CLUTTER_BEHAVIOUR_PATH (gobject);

  switch (prop_id)
    {
    case PROP_PATH:
      clutter_behaviour_path_set_path (pathb, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_path_dispose (GObject *gobject)
{
  ClutterBehaviourPath *pathb = CLUTTER_BEHAVIOUR_PATH (gobject);

  clutter_behaviour_path_set_path (pathb, NULL);

  G_OBJECT_CLASS (clutter_behaviour_path_parent_class)->dispose (gobject);
}

static void
clutter_behaviour_path_class_init (ClutterBehaviourPathClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBehaviourClass *behave_class = CLUTTER_BEHAVIOUR_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->get_property = clutter_behaviour_path_get_property;
  gobject_class->set_property = clutter_behaviour_path_set_property;
  gobject_class->dispose = clutter_behaviour_path_dispose;

  pspec = g_param_spec_object ("path",
                               "Path",
                               "The ClutterPath object representing the path "
                               "to animate along",
                               CLUTTER_TYPE_PATH,
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_PATH, pspec);

  /**
   * ClutterBehaviourPath::knot-reached:
   * @pathb: the object which received the signal
   * @knot_num: the index of the #ClutterPathKnot reached
   *
   * This signal is emitted each time a node defined inside the path
   * is reached.
   *
   * Since: 0.2
   */
  path_signals[KNOT_REACHED] =
    g_signal_new ("knot-reached",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterBehaviourPathClass, knot_reached),
                  NULL, NULL,
                  clutter_marshal_VOID__UINT,
                  G_TYPE_NONE, 1,
                  G_TYPE_UINT);

  behave_class->alpha_notify = clutter_behaviour_path_alpha_notify;

  g_type_class_add_private (klass, sizeof (ClutterBehaviourPathPrivate));
}

static ClutterScriptableIface *parent_scriptable_iface = NULL;

static gboolean
clutter_behaviour_path_parse_custom_node (ClutterScriptable *scriptable,
                                          ClutterScript     *script,
                                          GValue            *value,
                                          const gchar       *name,
                                          JsonNode          *node)
{
  if (strcmp ("path", name) == 0)
    {
      ClutterPath *path;
      GValue node_value = { 0 };

      path = g_object_ref_sink (clutter_path_new ());

      json_node_get_value (node, &node_value);

      if (!G_VALUE_HOLDS (&node_value, G_TYPE_STRING)
          || !clutter_path_set_description (path,
                                            g_value_get_string (&node_value)))
        g_warning ("Invalid path description");

      g_value_unset (&node_value);

      g_value_init (value, G_TYPE_OBJECT);
      g_value_take_object (value, path);

      return TRUE;
    }
  /* chain up */
  else if (parent_scriptable_iface->parse_custom_node)
    return parent_scriptable_iface->parse_custom_node (scriptable, script,
                                                       value, name, node);
  else
    return FALSE;
}

static void
clutter_scriptable_iface_init (ClutterScriptableIface *iface)
{
  parent_scriptable_iface = g_type_interface_peek_parent (iface);

  if (!parent_scriptable_iface)
    parent_scriptable_iface
      = g_type_default_interface_peek (CLUTTER_TYPE_SCRIPTABLE);

  iface->parse_custom_node = clutter_behaviour_path_parse_custom_node;
}

static void
clutter_behaviour_path_init (ClutterBehaviourPath *self)
{
  ClutterBehaviourPathPrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_PATH_GET_PRIVATE (self);

  priv->path = NULL;
  priv->last_knot_passed = G_MAXUINT;
}

/**
 * clutter_behaviour_path_new:
 * @alpha: a #ClutterAlpha, or %NULL
 * @path: a #ClutterPath or %NULL for an empty path
 *
 * Creates a new path behaviour. You can use this behaviour to drive
 * actors along the nodes of a path, described by @path.
 *
 * This will claim the floating reference on the #ClutterPath so you
 * do not need to unref if it.
 *
 * Return value: a #ClutterBehaviour
 *
 * Since: 0.2
 */
ClutterBehaviour *
clutter_behaviour_path_new (ClutterAlpha *alpha,
                            ClutterPath  *path)
{
  return g_object_new (CLUTTER_TYPE_BEHAVIOUR_PATH,
                       "alpha", alpha,
                       "path", path,
                       NULL);
}

/**
 * clutter_behaviour_path_new_with_description:
 * @alpha: a #ClutterAlpha
 * @desc: a string description of the path
 *
 * Creates a new path behaviour using the path described by @desc. See
 * clutter_path_add_string() for a description of the format.
 *
 * Return value: a #ClutterBehaviour
 *
 * Since: 1.0
 */
ClutterBehaviour *
clutter_behaviour_path_new_with_description (ClutterAlpha *alpha,
                                             const gchar  *desc)
{
  return g_object_new (CLUTTER_TYPE_BEHAVIOUR_PATH,
                       "alpha", alpha,
                       "path", clutter_path_new_with_description (desc),
                       NULL);
}

/**
 * clutter_behaviour_path_new_with_knots:
 * @alpha: a #ClutterAlpha
 * @knots: an array of #ClutterKnot<!-- -->s
 * @n_knots: number of entries in @knots
 *
 * Creates a new path behaviour that will make the actors visit all of
 * the given knots in order with straight lines in between.
 *
 * A path will be created where the first knot is used in a
 * %CLUTTER_PATH_MOVE_TO and the subsequent knots are used in
 * %CLUTTER_PATH_LINE_TO<!-- -->s.
 *
 * Return value: a #ClutterBehaviour
 *
 * Since: 1.0
 */
ClutterBehaviour *
clutter_behaviour_path_new_with_knots (ClutterAlpha      *alpha,
                                       const ClutterKnot *knots,
                                       guint              n_knots)
{
  ClutterPath *path = clutter_path_new ();
  guint i;

  if (n_knots > 0)
    {
      clutter_path_add_move_to (path, knots[0].x, knots[0].y);

      for (i = 1; i < n_knots; i++)
        clutter_path_add_line_to (path, knots[i].x, knots[i].y);
    }

  return g_object_new (CLUTTER_TYPE_BEHAVIOUR_PATH,
                       "alpha", alpha,
                       "path", path,
                       NULL);
}

/**
 * clutter_behaviour_path_set_path:
 * @pathb: the path behaviour
 * @path: the new path to follow
 *
 * Change the path that the actors will follow. This will take the
 * floating reference on the #ClutterPath so you do not need to unref
 * it.
 *
 * Since: 1.0
 */
void
clutter_behaviour_path_set_path (ClutterBehaviourPath *pathb,
                                 ClutterPath          *path)
{
  ClutterBehaviourPathPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_PATH (pathb));

  priv = pathb->priv;

  if (path)
    g_object_ref_sink (path);

  if (priv->path)
    g_object_unref (priv->path);

  priv->path = path;

  g_object_notify (G_OBJECT (pathb), "path");
}

/**
 * clutter_behaviour_path_get_path:
 * @pathb: a #ClutterBehaviourPath instance
 *
 * Get the current path of the behaviour
 *
 * Return value: the path
 *
 * Since: 1.0
 */
ClutterPath *
clutter_behaviour_path_get_path (ClutterBehaviourPath *pathb)
{
  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR_PATH (pathb), NULL);

  return pathb->priv->path;
}
