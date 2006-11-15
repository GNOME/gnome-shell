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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-actor.h"
#include "clutter-behaviour.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-behaviour-path.h"

#include <math.h>

static ClutterKnot *
clutter_knot_copy (const ClutterKnot *knot)
{
  ClutterKnot *copy;

  copy = g_slice_new0 (ClutterKnot);
  
  *copy = *knot;

  return copy;
}

static void
clutter_knot_free (ClutterKnot *knot)
{
  if (G_LIKELY (knot))
    {
      g_slice_free (ClutterKnot, knot);
    }
}

GType
clutter_knot_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (!our_type))
    our_type = g_boxed_type_register_static 
                            ("ClutterKnot",
			     (GBoxedCopyFunc) clutter_knot_copy,
			     (GBoxedFreeFunc) clutter_knot_free);
  return our_type;
}


G_DEFINE_TYPE (ClutterBehaviourPath,   \
               clutter_behaviour_path, \
	       CLUTTER_TYPE_BEHAVIOUR);

struct _ClutterBehaviourPathPrivate
{
  GSList *knots;
};

#define CLUTTER_BEHAVIOUR_PATH_GET_PRIVATE(obj)    \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
               CLUTTER_TYPE_BEHAVIOUR_PATH,        \
               ClutterBehaviourPathPrivate))

static void 
clutter_behaviour_path_finalize (GObject *object)
{
  ClutterBehaviourPath *self = CLUTTER_BEHAVIOUR_PATH(object);

  g_slist_foreach (self->priv->knots, (GFunc) clutter_knot_free, NULL);
  g_slist_free (self->priv->knots);

  G_OBJECT_CLASS (clutter_behaviour_path_parent_class)->finalize (object);
}

static void
interpolate (const ClutterKnot *begin, 
	     const ClutterKnot *end, 
	     ClutterKnot       *out,
	     double             t)
{
  /* FIXME: fixed point */
  out->x = begin->x + t * (end->x - begin->x);
  out->y = begin->y + t * (end->y - begin->y);
}

static gint
node_distance (const ClutterKnot *begin, const ClutterKnot *end)
{
  g_return_val_if_fail (begin != NULL, 0);
  g_return_val_if_fail (end != NULL, 0);

  /* FIXME: need fixed point here */
  return sqrt ((end->x - begin->x) * (end->x - begin->x) +
               (end->y - begin->y) * (end->y - begin->y));
}

static gint
path_total_length (ClutterBehaviourPath *behave)
{
  GSList *l;
  gint    len = 0;

  for (l = behave->priv->knots; l != NULL; l = l->next)
    if (l->next)
      len += node_distance (l->data, l->next->data);

  return len;
}

static void
actor_apply_knot_foreach (ClutterActor            *actor,
			  ClutterKnot             *knot)
{
  clutter_actor_set_position (actor, knot->x, knot->y);
}

static void
path_alpha_to_position (ClutterBehaviourPath *behave)
{
  ClutterBehaviourPathPrivate *priv = behave->priv;
  ClutterBehaviour *behaviour = CLUTTER_BEHAVIOUR (behave);
  guint32  alpha;
  GSList  *l;
  gint     total_len, offset, dist_to_next, dist = 0;

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

  alpha = clutter_alpha_get_alpha (clutter_behaviour_get_alpha (behaviour));

  total_len = path_total_length (behave);
  offset = (alpha * total_len) / CLUTTER_ALPHA_MAX_ALPHA;

  if (offset == 0)
    {
      clutter_behaviour_actors_foreach (behaviour, 
					(GFunc) actor_apply_knot_foreach,
					priv->knots->data);
      return;
    }

  for (l = priv->knots; l != NULL; l = l->next)
    {
      ClutterKnot *knot = l->data;
      
      if (l->next)
        {
          ClutterKnot *next = l->next->data;

	  dist_to_next = node_distance (knot, next);
          if (offset >= dist && offset < (dist + dist_to_next))
            {
	      ClutterKnot new;
	      double t;
	    
	      /* FIXME: Use fixed */
	      t = (double) (offset - dist) / dist_to_next;
              interpolate (knot, next, &new, t);

              clutter_behaviour_actors_foreach (behaviour, 
					        (GFunc)actor_apply_knot_foreach,
					        &new);
	      return;
	    }
        }

      dist += dist_to_next;
    }
}

static void
clutter_behaviour_path_alpha_notify (ClutterBehaviour *behave)
{
  path_alpha_to_position (CLUTTER_BEHAVIOUR_PATH(behave));
}

static void
clutter_behaviour_path_class_init (ClutterBehaviourPathClass *klass)
{
  GObjectClass          *object_class;
  ClutterBehaviourClass *behave_class;

  object_class = (GObjectClass*) klass;
  behave_class = (ClutterBehaviourClass*) klass;

  object_class->finalize = clutter_behaviour_path_finalize;

  behave_class->alpha_notify = clutter_behaviour_path_alpha_notify;

  g_type_class_add_private (object_class, sizeof (ClutterBehaviourPathPrivate));
}

static void
clutter_behaviour_path_init (ClutterBehaviourPath *self)
{
  ClutterBehaviourPathPrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_PATH_GET_PRIVATE (self);
}

/**
 * clutter_behaviour_path_new:
 * @alpha: a #ClutterAlpha
 * @knots: a list of #ClutterKnots
 * @n_knots: the number of nodes in the path
 *
 * FIXME
 *
 * Return value: a #ClutterBehaviour
 */
ClutterBehaviour *
clutter_behaviour_path_new (ClutterAlpha          *alpha,
			    const ClutterKnot     *knots,
                            guint                  n_knots)
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

GSList *
clutter_behaviour_path_get_knots (ClutterBehaviourPath *behave)
{
  GSList *retval, *l;

  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR_PATH (behave), NULL);

  retval = NULL;
  for (l = behave->priv->knots; l != NULL; l = l->next)
    retval = g_slist_prepend (retval, l->data);
  
  return g_slist_reverse (retval);
}

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

void
clutter_behaviour_path_clear (ClutterBehaviourPath *pathb)
{
  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_PATH (pathb));

  g_slist_foreach (pathb->priv->knots, (GFunc) clutter_knot_free, NULL);
  g_slist_free (pathb->priv->knots);

  pathb->priv->knots = NULL;
}
