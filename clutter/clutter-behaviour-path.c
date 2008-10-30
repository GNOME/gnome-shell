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
 * @short_description: A behaviour interpolating position along a path
 *
 * #ClutterBehaviourPath interpolates actors along a defined path.
 *
 * A path is a set of #ClutterKnots object given when creating a new
 * #ClutterBehaviourPath instance.  Knots can be also added to the path
 * using clutter_behaviour_path_append_knot().  The whole path can be
 * cleared using clutter_behaviour_path_clear().  Each time the behaviour
 * reaches a knot in the path, the "knot-reached" signal is emitted.
 *
 * This first knot in the path is reached with the lower bound value
 * provided by the #ClutterAlpha objectused by the behaviour; the last
 * knot in the path is reached with the upper bound value provided by
 * the #ClutterAlpha object used by the behaviour.
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
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-scriptable.h"
#include "clutter-script-private.h"

#include <math.h>

static void clutter_scriptable_iface_init (ClutterScriptableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterBehaviourPath,
                         clutter_behaviour_path,
	                 CLUTTER_TYPE_BEHAVIOUR,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_SCRIPTABLE,
                                                clutter_scriptable_iface_init));

struct _ClutterBehaviourPathPrivate
{
  GSList      *knots;
  ClutterKnot *last_knot_passed;
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

  PROP_KNOT
};

static void
clutter_behaviour_path_finalize (GObject *object)
{
  ClutterBehaviourPath *self = CLUTTER_BEHAVIOUR_PATH(object);

  g_slist_foreach (self->priv->knots, (GFunc) clutter_knot_free, NULL);
  g_slist_free (self->priv->knots);

  G_OBJECT_CLASS (clutter_behaviour_path_parent_class)->finalize (object);
}

static inline void
interpolate (const ClutterKnot *start,
	     const ClutterKnot *end,
	     ClutterKnot       *out,
	     ClutterFixed       t)
{
  out->x = start->x + COGL_FIXED_TO_INT (t * (end->x - start->x));
  out->y = start->y + COGL_FIXED_TO_INT (t * (end->y - start->y));
}

static gint
node_distance (const ClutterKnot *start,
               const ClutterKnot *end)
{
  gint t;

  g_return_val_if_fail (start != NULL, 0);
  g_return_val_if_fail (end != NULL, 0);

  if (clutter_knot_equal (start, end))
        return 0;

  t = (end->x - start->x) * (end->x - start->x) +
    (end->y - start->y) * (end->y - start->y);

  /*
   * If we are using limited precision sqrti implementation, fallback on
   * clib sqrt if the precission would be less than 10%
   */
#if INT_MAX > CLUTTER_SQRTI_ARG_10_PERCENT
  if (t <= COGL_SQRTI_ARG_10_PERCENT)
    return cogl_sqrti (t);
  else
    return COGL_FLOAT_TO_INT (sqrt(t));
#else
    return cogl_sqrti (t);
#endif
}

static gint
path_total_length (ClutterBehaviourPath *behave)
{
  GSList *l;
  gint    len = 0;

  for (l = behave->priv->knots; l != NULL; l = l->next)
    if (l->next && l->next->data)
      len += node_distance (l->data, l->next->data);

  return len;
}

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
path_alpha_to_position (ClutterBehaviourPath *behave,
                        guint32               alpha)
{
  ClutterBehaviourPathPrivate *priv = behave->priv;
  ClutterBehaviour *behaviour = CLUTTER_BEHAVIOUR (behave);
  GSList  *l;
  gint     total_len, offset, dist = 0;

  /* FIXME: Optimise. Much of the data used here can be pre-generated
   *        ( total_len, dist between knots ) when knots are added/removed.
  */

  /* Calculation as follows:
   *  o Get total length of path
   *  o Find the offset on path where alpha val corresponds to
   *  o Figure out between which knots this offset lies.
   *  o Interpolate new co-ords via dist between these knots
   *  o Apply to actors.
  */

  total_len = path_total_length (behave);
  offset = (alpha * total_len) / CLUTTER_ALPHA_MAX_ALPHA;

  CLUTTER_NOTE (BEHAVIOUR, "alpha %i vs %i, len: %i vs %i",
		alpha, CLUTTER_ALPHA_MAX_ALPHA,
		offset, total_len);

  if (offset == 0)
    {
      /* first knot */
      clutter_behaviour_actors_foreach (behaviour,
					actor_apply_knot_foreach,
					priv->knots->data);

      priv->last_knot_passed = (ClutterKnot*)priv->knots->data;
      g_signal_emit (behave, path_signals[KNOT_REACHED], 0,
                     priv->knots->data);
      return;
    }

  if (offset == total_len)
    {
      /* Special case for last knot */
      ClutterKnot *last_knot = (g_slist_last (priv->knots))->data;

      clutter_behaviour_actors_foreach (behaviour,
					actor_apply_knot_foreach,
					last_knot);

      priv->last_knot_passed = (ClutterKnot*)priv->knots->data;
      g_signal_emit (behave, path_signals[KNOT_REACHED], 0, last_knot);

      return;
    }

  for (l = priv->knots; l != NULL; l = l->next)
    {
      gint dist_to_next = 0;
      ClutterKnot *knot = l->data;

      if (l->next)
        {
          ClutterKnot *next = l->next->data;

	  dist_to_next = node_distance (knot, next);

          if (offset >= dist && offset < (dist + dist_to_next))
            {
	      ClutterKnot new;
	      ClutterFixed t;

	      t = COGL_FIXED_FROM_INT (offset - dist) / dist_to_next;

              interpolate (knot, next, &new, t);

              clutter_behaviour_actors_foreach (behaviour,
					        actor_apply_knot_foreach,
					        &new);

	      if (knot != priv->last_knot_passed)
		{
		  /* We just passed a new Knot */
		  priv->last_knot_passed = knot;
		  g_signal_emit (behave, path_signals[KNOT_REACHED], 0, knot);
		}

	      return;
	    }
        }

      dist += dist_to_next;
    }
}

static void
clutter_behaviour_path_alpha_notify (ClutterBehaviour *behave,
                                     guint32           alpha_value)
{
  path_alpha_to_position (CLUTTER_BEHAVIOUR_PATH (behave), alpha_value);
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
    case PROP_KNOT:
      clutter_behaviour_path_append_knot (pathb, g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_path_class_init (ClutterBehaviourPathClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBehaviourClass *behave_class = CLUTTER_BEHAVIOUR_CLASS (klass);

  gobject_class->set_property = clutter_behaviour_path_set_property;
  gobject_class->finalize = clutter_behaviour_path_finalize;

  /**
   * ClutterBehaviourPath:knot:
   *
   * This property can be used to append a new knot to the path.
   *
   * Since: 0.2
   */
  g_object_class_install_property (gobject_class,
                                   PROP_KNOT,
                                   g_param_spec_boxed ("knot",
                                                       "Knot",
                                                       "Can be used to append a knot to the path",
                                                       CLUTTER_TYPE_KNOT,
                                                       CLUTTER_PARAM_WRITABLE));

  /**
   * ClutterBehaviourPath::knot-reached:
   * @pathb: the object which received the signal
   * @knot: the #ClutterKnot reached
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
                  clutter_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_KNOT);

  behave_class->alpha_notify = clutter_behaviour_path_alpha_notify;

  g_type_class_add_private (klass, sizeof (ClutterBehaviourPathPrivate));
}

static void
clutter_behaviour_path_init (ClutterBehaviourPath *self)
{
  ClutterBehaviourPathPrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_PATH_GET_PRIVATE (self);
}

static void
clutter_behaviour_path_set_custom_property (ClutterScriptable *scriptable,
                                            ClutterScript     *script,
                                            const gchar       *name,
                                            const GValue      *value)
{
  if (strcmp (name, "knots") == 0)
    {
      ClutterBehaviourPath *path = CLUTTER_BEHAVIOUR_PATH (scriptable);
      GSList *knots, *l;

      if (!G_VALUE_HOLDS (value, G_TYPE_POINTER))
        return;

      knots = g_value_get_pointer (value);
      for (l = knots; l != NULL; l = l->next)
        {
          ClutterKnot *knot = l->data;

          clutter_behaviour_path_append_knot (path, knot);
          clutter_knot_free (knot);
        }

      g_slist_free (knots);
    }
  else
    g_object_set_property (G_OBJECT (scriptable), name, value);
}

static gboolean
clutter_behaviour_path_parse_custom_node (ClutterScriptable *scriptable,
                                          ClutterScript     *script,
                                          GValue            *value,
                                          const gchar       *name,
                                          JsonNode          *node)
{
  if (strcmp (name, "knots") == 0)
    {
      JsonArray *array;
      guint knots_len, i;
      GSList *knots = NULL;

      array = json_node_get_array (node);
      knots_len = json_array_get_length (array);

      for (i = 0; i < knots_len; i++)
        {
          JsonNode *val = json_array_get_element (array, i);
          ClutterKnot knot = { 0, };

          if (clutter_script_parse_knot (script, val, &knot))
            {
              CLUTTER_NOTE (SCRIPT, "parsed knot [ x:%d, y:%d ]",
                            knot.x, knot.y);

              knots = g_slist_prepend (knots, clutter_knot_copy (&knot));
            }
        }

      g_value_init (value, G_TYPE_POINTER);
      g_value_set_pointer (value, g_slist_reverse (knots));

      return TRUE;
    }

  return FALSE;
}

static void
clutter_scriptable_iface_init (ClutterScriptableIface *iface)
{
  iface->parse_custom_node = clutter_behaviour_path_parse_custom_node;
  iface->set_custom_property = clutter_behaviour_path_set_custom_property;
}

/**
 * clutter_behaviour_path_new:
 * @alpha: a #ClutterAlpha, or %NULL
 * @knots: a list of #ClutterKnots, or %NULL for an empty path
 * @n_knots: the number of nodes in the path
 *
 * Creates a new path behaviour. You can use this behaviour to drive
 * actors along the nodes of a path, described by the @knots.
 *
 * Return value: a #ClutterBehaviour
 *
 * Since: 0.2
 */
ClutterBehaviour *
clutter_behaviour_path_new (ClutterAlpha      *alpha,
			    const ClutterKnot *knots,
                            guint              n_knots)
{
  ClutterBehaviourPath *behave;
  gint i;

  behave = g_object_new (CLUTTER_TYPE_BEHAVIOUR_PATH,
                         "alpha", alpha,
			 NULL);

  for (i = 0; i < n_knots; i++)
    {
      ClutterKnot knot = knots[i];

      clutter_behaviour_path_append_knot (behave, &knot);
    }

  return CLUTTER_BEHAVIOUR (behave);
}

/**
 * clutter_behaviour_path_get_knots:
 * @pathb: a #ClutterBehvaiourPath
 *
 * Returns a copy of the list of knots contained by @pathb
 *
 * Return value: a #GSList of the paths knots.
 *
 * Since: 0.2
 */
GSList *
clutter_behaviour_path_get_knots (ClutterBehaviourPath *pathb)
{
  GSList *retval, *l;

  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR_PATH (pathb), NULL);

  retval = NULL;
  for (l = pathb->priv->knots; l != NULL; l = l->next)
    retval = g_slist_prepend (retval, l->data);

  return g_slist_reverse (retval);
}

/**
 * clutter_behaviour_path_append_knot:
 * @pathb: a #ClutterBehvaiourPath
 * @knot:  a #ClutterKnot to append.
 *
 * Appends a #ClutterKnot to the path
 *
 * Since: 0.2
 */
void
clutter_behaviour_path_append_knot (ClutterBehaviourPath  *pathb,
				    const ClutterKnot     *knot)
{
  ClutterBehaviourPathPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_PATH (pathb));
  g_return_if_fail (knot != NULL);

  priv = pathb->priv;
  priv->knots = g_slist_append (priv->knots, clutter_knot_copy (knot));
}

/**
 * clutter_behaviour_path_insert_knot:
 * @pathb: a #ClutterBehvaiourPath
 * @offset: position in path to insert knot.
 * @knot:  a #ClutterKnot to append.
 *
 * Inserts a #ClutterKnot in the path at specified position. Values greater
 * than total number of knots will append the knot at the end of path.
 *
 * Since: 0.2
 */
void
clutter_behaviour_path_insert_knot (ClutterBehaviourPath  *pathb,
				    guint                  offset,
				    const ClutterKnot     *knot)
{
  ClutterBehaviourPathPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_PATH (pathb));
  g_return_if_fail (knot != NULL);

  priv = pathb->priv;
  priv->knots = g_slist_insert (priv->knots, clutter_knot_copy (knot), offset);
}

/**
 * clutter_behaviour_path_remove_knot:
 * @pathb: a #ClutterBehvaiourPath
 * @offset: position in path to remove knot.
 *
 * Removes a #ClutterKnot in the path at specified offset.
 *
 * Since: 0.2
 */
void
clutter_behaviour_path_remove_knot (ClutterBehaviourPath  *pathb,
				    guint                  offset)
{
  ClutterBehaviourPathPrivate *priv;
  GSList                      *togo;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_PATH (pathb));

  priv = pathb->priv;

  togo = g_slist_nth (priv->knots, offset);

  if (togo)
    {
      clutter_knot_free ((ClutterKnot*)togo->data);
      priv->knots = g_slist_delete_link (priv->knots, togo);
    }
}

static void
clutter_behaviour_path_append_knots_valist (ClutterBehaviourPath *pathb,
					    const ClutterKnot    *first_knot,
					    va_list               args)
{
  const ClutterKnot * knot;

  knot = first_knot;
  while (knot)
    {
      clutter_behaviour_path_append_knot (pathb, knot);
      knot = va_arg (args, ClutterKnot*);
    }
}

/**
 * clutter_behaviour_path_append_knots:
 * @pathb: a #ClutterBehvaiourPath
 * @first_knot: the #ClutterKnot knot to add to the path
 * @Varargs: additional knots to add to the path
 *
 * Adds a NULL-terminated list of knots to a path.  This function is
 * equivalent to calling clutter_behaviour_path_append_knot() for each
 * member of the list.
 *
 * Since: 0.2
 */
void
clutter_behaviour_path_append_knots (ClutterBehaviourPath *pathb,
				     const ClutterKnot    *first_knot,
				     ...)
{
  va_list args;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_PATH (pathb));
  g_return_if_fail (first_knot != NULL);

  va_start (args, first_knot);
  clutter_behaviour_path_append_knots_valist (pathb, first_knot, args);
  va_end (args);
}

/**
 * clutter_behaviour_path_clear:
 * @pathb: a #ClutterBehvaiourPath
 *
 * Removes all knots from a path
 *
 * Since: 0.2
 */
void
clutter_behaviour_path_clear (ClutterBehaviourPath *pathb)
{
  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_PATH (pathb));

  g_slist_foreach (pathb->priv->knots, (GFunc) clutter_knot_free, NULL);
  g_slist_free (pathb->priv->knots);

  pathb->priv->knots = NULL;
}
