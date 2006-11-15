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
 * SECTION:clutter-behaviour
 * @short_description: Class for providing common behaviours to actors
 *
 */

/* TODO:
 *  o Document 
 *  o Add props
 *  o Optimise
 */

#include "config.h"

#include "clutter-actor.h"
#include "clutter-behaviour.h"
#include "clutter-behaviours.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"

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
clutter_behaviour_path_alpha_notify (ClutterBehaviour *behave);

static void 
clutter_behaviour_path_dispose (GObject *object)
{
  ClutterBehaviourPath *self = CLUTTER_BEHAVIOUR_PATH(object); 

  if (self->priv)
    {
      /* FIXME: unref knots */
    }

  G_OBJECT_CLASS (clutter_behaviour_path_parent_class)->dispose (object);
}

static void 
clutter_behaviour_path_finalize (GObject *object)
{
  ClutterBehaviourPath *self = CLUTTER_BEHAVIOUR_PATH(object); 

  if (self->priv)
    {
      g_free(self->priv);
      self->priv = NULL;
    }

  G_OBJECT_CLASS (clutter_behaviour_path_parent_class)->finalize (object);
}


static void
clutter_behaviour_path_class_init (ClutterBehaviourPathClass *klass)
{
  GObjectClass          *object_class;
  ClutterBehaviourClass *behave_class;

  object_class = (GObjectClass*) klass;
  behave_class = (ClutterBehaviourClass*) klass;

  object_class->finalize     = clutter_behaviour_path_finalize;
  object_class->dispose      = clutter_behaviour_path_dispose;

  behave_class->alpha_notify = clutter_behaviour_path_alpha_notify;

  g_type_class_add_private (object_class, sizeof (ClutterBehaviourPathPrivate));
}

static void
clutter_behaviour_path_init (ClutterBehaviourPath *self)
{
  ClutterBehaviourPathPrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_PATH_GET_PRIVATE (self);
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

  alpha = clutter_alpha_get_alpha 
                   (clutter_behaviour_get_alpha (CLUTTER_BEHAVIOUR(behave)));

  total_len = path_total_length (behave);

  offset = (alpha * total_len) / CLUTTER_ALPHA_MAX_ALPHA;

  if (offset == 0)
    {
      clutter_behaviour_actors_foreach (CLUTTER_BEHAVIOUR(behave), 
					(GFunc)actor_apply_knot_foreach,
					behave->priv->knots->data);
      return;
    }

  for (l = behave->priv->knots; l != NULL; l = l->next)
    if (l->next)
      {
	dist_to_next = node_distance (l->data, l->next->data);
	
	if (offset >= dist && offset < (dist + dist_to_next))
	  {
	    ClutterKnot new;
	    double t;
	    
	    /* FIXME: Use fixed */
	    t = (double)(offset - dist) / dist_to_next ;
	    
	    interpolate (l->data, l->next->data, &new, t);
	    
	    clutter_behaviour_actors_foreach (CLUTTER_BEHAVIOUR(behave), 
					      (GFunc)actor_apply_knot_foreach,
					      &new);
	    return;
	  }

	dist += dist_to_next;
      }
}

static void
clutter_behaviour_path_alpha_notify (ClutterBehaviour *behave)
{
  path_alpha_to_position (CLUTTER_BEHAVIOUR_PATH(behave));
}

ClutterBehaviour*
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
      clutter_path_behaviour_append_knot (behave, &knot);
    }

  return CLUTTER_BEHAVIOUR(behave);
}

GSList*
clutter_path_behaviour_get_knots (ClutterBehaviourPath *behave)
{
  GSList *retval, *l;

  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR_PATH (behave), NULL);

  retval = NULL;
  for (l = behave->priv->knots; l != NULL; l = l->next)
    retval = g_slist_prepend (retval, l->data);
  
  return g_slist_reverse (retval);
}

void
clutter_path_behaviour_append_knot (ClutterBehaviourPath  *pathb,
				    const ClutterKnot     *knot)
{
  ClutterBehaviourPathPrivate *priv;

  g_return_if_fail (knot != NULL);

  priv = pathb->priv;

  priv->knots = g_slist_append (priv->knots,
                                clutter_knot_copy (knot));
}

void
clutter_path_behaviour_append_knots_valist (ClutterBehaviourPath  *pathb,
					    const ClutterKnot     *first_knot,
					    va_list                args)
{
  const ClutterKnot * knot;

  knot = first_knot;
  while (knot)
    {
      clutter_path_behaviour_append_knot (pathb, knot);
      knot = va_arg (args, ClutterKnot*);
    }
}

void
clutter_path_behavior_append_knots (ClutterBehaviourPath  *pathb,
				    const ClutterKnot     *first_knot,
				    ...)
{
  va_list args;

  g_return_if_fail (first_knot != NULL);

  va_start (args, first_knot);
  clutter_path_behaviour_append_knots_valist (pathb, first_knot, args);
  va_end (args);
}

void
clutter_path_behavior_remove_knot (ClutterBehaviourPath  *behave,
				   guint                  index)
{
  /* FIXME: implement */
}

ClutterKnot*
clutter_path_behavior_get_knot (ClutterBehaviourPath  *behave,
				guint                  index)
{
  /* FIXME: implement */
  return NULL;
}

void
clutter_path_behavior_insert_knot (ClutterBehaviourPath  *behave,
				   ClutterKnot           *knot,
				   guint                  index)
{
  /* FIXME: implement */
}


/*
 * ====================== Opacity ============================
 */


G_DEFINE_TYPE (ClutterBehaviourOpacity,   \
               clutter_behaviour_opacity, \
	       CLUTTER_TYPE_BEHAVIOUR);

struct ClutterBehaviourOpacityPrivate
{
  guint8 opacity_start;
  guint8 opacity_end;
};

#define CLUTTER_BEHAVIOUR_OPACITY_GET_PRIVATE(obj)    \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj),    \
               CLUTTER_TYPE_BEHAVIOUR_OPACITY,        \
               ClutterBehaviourOpacityPrivate))

static void
clutter_behaviour_opacity_frame_foreach (ClutterActor            *actor,
					 ClutterBehaviourOpacity *behave)
{
  guint32                         alpha;
  guint8                          opacity;
  ClutterBehaviourOpacityPrivate *priv;
  ClutterBehaviour               *_behave;

  priv = CLUTTER_BEHAVIOUR_OPACITY_GET_PRIVATE (behave);
  _behave = CLUTTER_BEHAVIOUR (behave);

  alpha = clutter_alpha_get_alpha (clutter_behaviour_get_alpha (_behave));

  opacity = (alpha * (priv->opacity_end - priv->opacity_start)) 
                            / CLUTTER_ALPHA_MAX_ALPHA;

  opacity += priv->opacity_start;

  CLUTTER_DBG("alpha %i opacity %i\n", alpha, opacity);

  clutter_actor_set_opacity (actor, opacity);
}

static void
clutter_behaviour_opacity_alpha_notify (ClutterBehaviour *behave)
{
  clutter_behaviour_actors_foreach 
                     (behave,
		      (GFunc)clutter_behaviour_opacity_frame_foreach,
		      CLUTTER_BEHAVIOUR_OPACITY(behave));
}

static void 
clutter_behaviour_opacity_dispose (GObject *object)
{
  G_OBJECT_CLASS (clutter_behaviour_opacity_parent_class)->dispose (object);
}

static void 
clutter_behaviour_opacity_finalize (GObject *object)
{
  ClutterBehaviourOpacity *self = CLUTTER_BEHAVIOUR_OPACITY(object); 

  if (self->priv)
    {
      g_free(self->priv);
      self->priv = NULL;
    }

  G_OBJECT_CLASS (clutter_behaviour_opacity_parent_class)->finalize (object);
}


static void
clutter_behaviour_opacity_class_init (ClutterBehaviourOpacityClass *klass)
{
  GObjectClass          *object_class;
  ClutterBehaviourClass *behave_class;

  object_class = (GObjectClass*) klass;

  object_class->finalize     = clutter_behaviour_opacity_finalize;
  object_class->dispose      = clutter_behaviour_opacity_dispose;

  behave_class = (ClutterBehaviourClass*) klass;

  behave_class->alpha_notify = clutter_behaviour_opacity_alpha_notify;

  g_type_class_add_private (object_class, sizeof (ClutterBehaviourOpacityPrivate));
}

static void
clutter_behaviour_opacity_init (ClutterBehaviourOpacity *self)
{
  ClutterBehaviourOpacityPrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_OPACITY_GET_PRIVATE (self);
}

ClutterBehaviour*
clutter_behaviour_opacity_new (ClutterAlpha *alpha,
			       guint8        opacity_start,
			       guint8        opacity_end)
{
  ClutterBehaviourOpacity *behave;

  behave = g_object_new (CLUTTER_TYPE_BEHAVIOUR_OPACITY, 
                         "alpha", alpha,
			 NULL);

  behave->priv->opacity_start = opacity_start;
  behave->priv->opacity_end   = opacity_end;

  return CLUTTER_BEHAVIOUR(behave);
}

/*
 * ====================== Scale ============================
 */

G_DEFINE_TYPE (ClutterBehaviourScale,   \
               clutter_behaviour_scale, \
	       CLUTTER_TYPE_BEHAVIOUR);

struct ClutterBehaviourScalePrivate
{
  ClutterFixed   scale_begin;
  ClutterFixed   scale_end;
  ClutterGravity gravity;
};

#define CLUTTER_BEHAVIOUR_SCALE_GET_PRIVATE(obj)    \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj),    \
               CLUTTER_TYPE_BEHAVIOUR_SCALE,        \
               ClutterBehaviourScalePrivate))

static void
clutter_behaviour_scale_frame_foreach (ClutterActor            *actor,
					 ClutterBehaviourScale *behave)
{
  ClutterFixed                    scale, factor;
  guint32                         alpha;
  ClutterBehaviourScalePrivate   *priv;
  ClutterBehaviour               *_behave;
  gint                            sw, sh, w, h;

  priv = CLUTTER_BEHAVIOUR_SCALE_GET_PRIVATE (behave);
  _behave = CLUTTER_BEHAVIOUR (behave);

  alpha = clutter_alpha_get_alpha (clutter_behaviour_get_alpha (_behave));

  /* FIXME: use all fixed if possible
  factor = CLUTTER_FIXED_DIV(CLUTTER_INT_TO_FIXED(alpha/2),
			     CLUTTER_INT_TO_FIXED(CLUTTER_ALPHA_MAX_ALPHA/2));
  */

  factor = CLUTTER_FLOAT_TO_FIXED((double) alpha / CLUTTER_ALPHA_MAX_ALPHA);

  scale = CLUTTER_FIXED_MUL(factor, (priv->scale_end - priv->scale_begin));

  scale += priv->scale_begin;

  clutter_actor_set_scalex (actor, scale, scale);

  if (priv->gravity == CLUTTER_GRAVITY_NONE 
      || priv->gravity == CLUTTER_GRAVITY_NORTH_WEST)
    return;

  clutter_actor_get_abs_size (actor, (guint*)&sw, (guint*)&sh);
  clutter_actor_get_size (actor, (guint*)&w, (guint*)&h);

  switch (priv->gravity)
    {
    case CLUTTER_GRAVITY_NORTH:
      break;
    case CLUTTER_GRAVITY_NORTH_EAST:
      break;
    case CLUTTER_GRAVITY_EAST:
      break;
    case CLUTTER_GRAVITY_SOUTH_EAST:
      break;
    case CLUTTER_GRAVITY_SOUTH:
      break;
    case CLUTTER_GRAVITY_SOUTH_WEST:
      break;
    case CLUTTER_GRAVITY_WEST:
      break;
    case CLUTTER_GRAVITY_CENTER:
      printf("%i vs %i\n", sw, w);
      clutter_actor_move_by (actor, sw - w, sh - h);
    default:
      break;
    }
}

static void
clutter_behaviour_scale_alpha_notify (ClutterBehaviour *behave)
{
  clutter_behaviour_actors_foreach 
                     (behave,
		      (GFunc)clutter_behaviour_scale_frame_foreach,
		      CLUTTER_BEHAVIOUR_SCALE(behave));
}

static void 
clutter_behaviour_scale_dispose (GObject *object)
{
  G_OBJECT_CLASS (clutter_behaviour_scale_parent_class)->dispose (object);
}

static void 
clutter_behaviour_scale_finalize (GObject *object)
{
  ClutterBehaviourScale *self = CLUTTER_BEHAVIOUR_SCALE(object); 

  if (self->priv)
    {
      g_free(self->priv);
      self->priv = NULL;
    }

  G_OBJECT_CLASS (clutter_behaviour_scale_parent_class)->finalize (object);
}


static void
clutter_behaviour_scale_class_init (ClutterBehaviourScaleClass *klass)
{
  GObjectClass          *object_class;
  ClutterBehaviourClass *behave_class;

  object_class = (GObjectClass*) klass;

  object_class->finalize     = clutter_behaviour_scale_finalize;
  object_class->dispose      = clutter_behaviour_scale_dispose;

  behave_class = (ClutterBehaviourClass*) klass;

  behave_class->alpha_notify = clutter_behaviour_scale_alpha_notify;

  g_type_class_add_private (object_class, sizeof (ClutterBehaviourScalePrivate));
}

static void
clutter_behaviour_scale_init (ClutterBehaviourScale *self)
{
  ClutterBehaviourScalePrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_SCALE_GET_PRIVATE (self);
}

ClutterBehaviour*
clutter_behaviour_scale_new (ClutterAlpha  *alpha,
			     double         scale_begin,
			     double         scale_end,
			     ClutterGravity gravity)
{
  ClutterBehaviourScale *behave;

  behave = g_object_new (CLUTTER_TYPE_BEHAVIOUR_SCALE, 
                         "alpha", alpha,
			 NULL);

  behave->priv->scale_begin = CLUTTER_FLOAT_TO_FIXED (scale_begin);
  behave->priv->scale_end   = CLUTTER_FLOAT_TO_FIXED (scale_end);
  behave->priv->gravity     = gravity;

  return CLUTTER_BEHAVIOUR(behave);
}
