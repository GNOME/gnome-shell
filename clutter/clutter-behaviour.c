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
 * @short_description: Class for providing behaviours to actors
 *
 * #ClutterBehaviour is the base class for implementing behaviours.  A
 * behaviour is a controller object for #ClutterActor<!-- -->s; you can
 * use a behaviour to control one or more properties of an actor (such
 * as its opacity, or its position).  A #ClutterBehaviour is driven by
 * an "alpha function" stored inside a #ClutterAlpha object; an alpha
 * function is a function depending solely on time.  The alpha function
 * computes a value which is then applied to the properties of the
 * actors driven by a behaviour.
 *
 * Clutter provides some pre-defined behaviours, like #ClutterBehaviourPath,
 * which controls the position of a set of actors making them "walk" along
 * a set of nodes; #ClutterBehaviourOpacity, which controls the opacity
 * of a set of actors; #ClutterBehaviourScale, which controls the width
 * and height of a set of actors.
 *
 * To visualize the effects of different alpha functions on a
 * #ClutterBehaviour implementation it is possible to take the
 * #ClutterBehaviourPath as an example:
 *
 * <figure id="behaviour-path-alpha">
 *   <title>Effects of alpha functions on a path</title>
 *   <graphic fileref="path-alpha-func.png" format="PNG"/>
 * </figure>
 *
 * The actors position between the path's end points directly correlates
 * to the #ClutterAlpha's current alpha value driving the behaviour. With
 * the #ClutterAlpha's function set to %CLUTTER_ALPHA_RAMP_INC the actor
 * will follow the path at a constant velocity, but when changing to
 * %CLUTTER_ALPHA_SINE_INC the actor initially accelerates before quickly
 * decelerating.
 *
 * In order to implement a new behaviour you should subclass #ClutterBehaviour
 * and override the "alpha_notify" virtual function; inside the overridden
 * function you should obtain the alpha value from the #ClutterAlpha
 * instance bound to the behaviour and apply it to the desiderd property
 * (or properties) of every actor controlled by the behaviour.
 *
 * #ClutterBehaviour is available since Clutter 0.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-main.h"
#include "clutter-actor.h"
#include "clutter-behaviour.h"
#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-marshal.h"

/**
 * clutter_knot_copy:
 * @knot: a #ClutterKnot
 *
 * Makes an allocated copy of a knot.
 *
 * Return value: the copied knot.
 *
 * Since: 0.2
 */
ClutterKnot *
clutter_knot_copy (const ClutterKnot *knot)
{
  ClutterKnot *copy;

  copy = g_slice_new0 (ClutterKnot);
  
  *copy = *knot;

  return copy;
}

/**
 * clutter_knot_free:
 * @knot: a #ClutterKnot
 *
 * Frees the memory of an allocated knot.
 *
 * Since: 0.2
 */
void
clutter_knot_free (ClutterKnot *knot)
{
  if (G_LIKELY (knot))
    {
      g_slice_free (ClutterKnot, knot);
    }
}

/**
 * clutter_knot_equal:
 * @knot_a: First knot
 * @knot_b: Second knot
 *
 * Compares to knot and checks if the point to the same location.
 *
 * Return value: %TRUE if the knots point to the same location.
 *
 * Since: 0.2
 */
gboolean
clutter_knot_equal (const ClutterKnot *knot_a,
                    const ClutterKnot *knot_b)
{
  g_return_val_if_fail (knot_a != NULL, FALSE);
  g_return_val_if_fail (knot_b != NULL, FALSE);

  if (knot_a == knot_b)
    return TRUE;

  return knot_a->x == knot_b->x && knot_a->y == knot_b->y;
}

GType
clutter_knot_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (!our_type))
    {
      our_type =
        g_boxed_type_register_static (I_("ClutterKnot"),
                                      (GBoxedCopyFunc) clutter_knot_copy,
                                      (GBoxedFreeFunc) clutter_knot_free);
    }

  return our_type;
}

G_DEFINE_ABSTRACT_TYPE (ClutterBehaviour,
                        clutter_behaviour,
                        G_TYPE_OBJECT);

struct _ClutterBehaviourPrivate
{
  ClutterAlpha *alpha;

  guint notify_id;
  GSList *actors;
};

enum
{
  PROP_0,
  PROP_ALPHA
};

enum {
  APPLIED,
  REMOVED,
  LAST_SIGNAL
};

static guint behave_signals[LAST_SIGNAL] = { 0 };

#define CLUTTER_BEHAVIOUR_GET_PRIVATE(obj)         \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
               CLUTTER_TYPE_BEHAVIOUR,             \
               ClutterBehaviourPrivate))

static void
clutter_behaviour_dispose (GObject *gobject)
{
  ClutterBehaviour *self = CLUTTER_BEHAVIOUR (gobject);

  clutter_behaviour_set_alpha (self, NULL);
  clutter_behaviour_remove_all (self);

  G_OBJECT_CLASS (clutter_behaviour_parent_class)->dispose (gobject);
}

static void
clutter_behaviour_set_property (GObject      *object, 
				guint         prop_id,
				const GValue *value, 
				GParamSpec   *pspec)
{
  ClutterBehaviour *behaviour = CLUTTER_BEHAVIOUR (object);

  switch (prop_id) 
    {
    case PROP_ALPHA:
      clutter_behaviour_set_alpha (behaviour, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_behaviour_get_property (GObject    *object, 
			        guint       prop_id,
			        GValue     *value, 
			        GParamSpec *pspec)
{
  ClutterBehaviour        *behaviour = CLUTTER_BEHAVIOUR (object);
  ClutterBehaviourPrivate *priv = behaviour->priv;

  switch (prop_id) 
    {
    case PROP_ALPHA:
      g_value_set_object (value, priv->alpha);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    } 
}

static void
clutter_behaviour_alpha_notify_unimplemented (ClutterBehaviour *behaviour,
                                              gdouble           alpha_value)
{
  g_warning ("ClutterBehaviourClass::alpha_notify not implemented for '%s'",
             g_type_name (G_TYPE_FROM_INSTANCE (behaviour)));
}

static void
clutter_behaviour_class_init (ClutterBehaviourClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = clutter_behaviour_dispose;
  object_class->set_property = clutter_behaviour_set_property;
  object_class->get_property = clutter_behaviour_get_property;

  /**
   * ClutterBehaviour:alpha:
   *
   * The #ClutterAlpha object used to drive this behaviour. A #ClutterAlpha
   * object binds a #ClutterTimeline and a function which computes a value
   * (the "alpha") depending on the time. Each time the alpha value changes
   * the alpha-notify virtual function is called.
   *
   * Since: 0.2
   */
  g_object_class_install_property (object_class,
                                   PROP_ALPHA,
                                   g_param_spec_object ("alpha",
                                                        "Alpha",
                                                        "Alpha Object to drive the behaviour",
                                                        CLUTTER_TYPE_ALPHA,
                                                        CLUTTER_PARAM_READWRITE));

  klass->alpha_notify = clutter_behaviour_alpha_notify_unimplemented;

  /**
   * ClutterBehaviour::applied:
   * @behaviour: the #ClutterBehaviour that received the signal
   * @actor: the actor the behaviour was applied to.
   *
   * The ::apply signal is emitted each time the behaviour is applied
   * to an actor.
   *
   * Since: 0.4
   */
  behave_signals[APPLIED] =
    g_signal_new ("applied",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ClutterBehaviourClass, applied),
		  NULL, NULL,
		  clutter_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_ACTOR);
  /**
   * ClutterBehaviour::removed:
   * @behaviour: the #ClutterBehaviour that received the signal
   * @actor: the removed actor
   *
   * The ::removed signal is emitted each time a behaviour is not applied
   * to an actor anymore.
   *
   */
  behave_signals[REMOVED] =
    g_signal_new ("removed",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ClutterBehaviourClass, removed),
		  NULL, NULL,
		  clutter_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_ACTOR);

  g_type_class_add_private (klass, sizeof (ClutterBehaviourPrivate));
}

static void
clutter_behaviour_init (ClutterBehaviour *self)
{
  ClutterBehaviourPrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_GET_PRIVATE (self);

}

static void
remove_actor_on_destroy (ClutterActor     *actor,
                         ClutterBehaviour *behaviour)
{
  clutter_behaviour_remove (behaviour, actor);
}

/**
 * clutter_behaviour_apply:
 * @behave: a #ClutterBehaviour
 * @actor: a #ClutterActor
 *
 * Applies @behave to @actor.  This function adds a reference on
 * the actor.
 *
 * Since: 0.2
 */
void
clutter_behaviour_apply (ClutterBehaviour *behave,
                         ClutterActor     *actor)
{
  ClutterBehaviourPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR (behave));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = behave->priv;

  if (g_slist_find (priv->actors, actor))
    {
      g_warning ("The behaviour of type %s already applies "
                 "to the actor of type %s",
                 g_type_name (G_OBJECT_TYPE (behave)),
                 g_type_name (G_OBJECT_TYPE (actor)));
      return;
    }

  priv->actors = g_slist_append (priv->actors, g_object_ref (actor));
  g_signal_connect (actor, "destroy",
                    G_CALLBACK (remove_actor_on_destroy),
                    behave);

  g_signal_emit (behave, behave_signals[APPLIED], 0, actor);
}

/**
 * clutter_behaviour_is_applied:
 * @behave: a #ClutterBehaviour
 * @actor: a #ClutterActor
 *
 * Check if @behave applied to  @actor.
 *
 * Return value: TRUE if actor has behaviour. FALSE otherwise.
 *
 * Since: 0.4
 */
gboolean
clutter_behaviour_is_applied (ClutterBehaviour *behave,
			      ClutterActor     *actor)
{
  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR (behave), FALSE);
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), FALSE);

  return (g_slist_find (behave->priv->actors, actor) != NULL);
}

/**
 * clutter_behaviour_remove:
 * @behave: a #ClutterBehaviour
 * @actor: a #ClutterActor
 *
 * Removes @actor from the list of #ClutterActor<!-- -->s to which
 * @behave applies.  This function removes a reference on the actor.
 *
 * Since: 0.2
 */
void
clutter_behaviour_remove (ClutterBehaviour *behave,
                          ClutterActor     *actor)
{
  ClutterBehaviourPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR (behave));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = behave->priv;

  if (!g_slist_find (priv->actors, actor))
    {
      g_warning ("The behaviour of type %s is not applied "
                 "to the actor of type %s",
                 g_type_name (G_OBJECT_TYPE (behave)),
                 g_type_name (G_OBJECT_TYPE (actor)));
      return;
    }

  g_signal_handlers_disconnect_by_func (actor,
                                        G_CALLBACK (remove_actor_on_destroy),
                                        behave);

  priv->actors = g_slist_remove (priv->actors, actor);
  
  g_signal_emit (behave, behave_signals[REMOVED], 0, actor);

  g_object_unref (actor);
}

/**
 * clutter_behaviour_get_n_actors:
 * @behave: a #ClutterBehaviour
 *
 * Gets the number of actors this behaviour is applied too.
 *
 * Return value: The number of applied actors 
 *
 * Since: 0.2
 */
gint
clutter_behaviour_get_n_actors (ClutterBehaviour *behave)
{
  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR (behave), 0);

  return g_slist_length (behave->priv->actors);
}

/**
 * clutter_behaviour_get_nth_actor:
 * @behave: a #ClutterBehaviour
 * @index_: the index of an actor this behaviour is applied too. 
 *
 * Gets an actor the behaviour was applied to referenced by index num.
 *
 * Return value: (transfer none): A Clutter actor or NULL if @index_ is invalid.
 *
 * Since: 0.2
 */
ClutterActor*
clutter_behaviour_get_nth_actor (ClutterBehaviour *behave,
				 gint              index_)
{
  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR (behave), NULL);

  return g_slist_nth_data (behave->priv->actors, index_);
}


/**
 * clutter_behaviour_actors_foreach:
 * @behave: a #ClutterBehaviour
 * @func: a function called for each actor
 * @data: optional data to be passed to the function, or %NULL
 *
 * Calls @func for every actor driven by @behave.
 *
 * Since: 0.2
 */
void
clutter_behaviour_actors_foreach (ClutterBehaviour            *behave,
				  ClutterBehaviourForeachFunc  func,
				  gpointer                     data)
{
  GSList *l;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR (behave));
  g_return_if_fail (func != NULL);

  for (l = behave->priv->actors; l != NULL; l = l->next)
    {
      ClutterActor *actor = l->data;

      g_assert (CLUTTER_IS_ACTOR (actor));

      func (behave, actor, data);
    }
}

/**
 * clutter_behaviour_get_alpha:
 * @behave: a #ClutterBehaviour
 *
 * Retrieves the #ClutterAlpha object bound to @behave.
 *
 * Return value: (transfer none): a #ClutterAlpha object, or %NULL if no alpha
 *   object has been bound to this behaviour.
 * 
 * Since: 0.2
 */
ClutterAlpha *
clutter_behaviour_get_alpha (ClutterBehaviour *behave)
{
  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR (behave), NULL);

  return behave->priv->alpha;
}

static void
notify_cb (GObject          *object,
           GParamSpec       *param_spec,
           ClutterBehaviour *behave)
{
  ClutterBehaviourClass *klass;

  klass = CLUTTER_BEHAVIOUR_GET_CLASS (behave);

  CLUTTER_NOTE (BEHAVIOUR, "notify::alpha");

  /* no actors, we can stop right here */
  if (behave->priv->actors == NULL)
    return;

  if (klass->alpha_notify)
    {
      gdouble alpha_value = clutter_alpha_get_alpha (behave->priv->alpha);

      CLUTTER_NOTE (BEHAVIOUR, "calling %s::alpha_notify (%p, %.4f)",
                    g_type_name (G_TYPE_FROM_CLASS (klass)),
                    behave, alpha_value);

      klass->alpha_notify (behave, alpha_value);
    }
}

/**
 * clutter_behaviour_set_alpha:
 * @behave: a #ClutterBehaviour
 * @alpha: a #ClutterAlpha or %NULL to unset a previously set alpha
 *
 * Binds @alpha to a #ClutterBehaviour. The #ClutterAlpha object
 * is what makes a behaviour work: for each tick of the timeline
 * used by #ClutterAlpha a new value of the alpha parameter is
 * computed by the alpha function; the value should be used by
 * the #ClutterBehaviour to update one or more properties of the
 * actors to which the behaviour applies.
 *
 * If @alpha is not %NULL, the #ClutterBehaviour will take ownership
 * of the #ClutterAlpha instance.
 *
 * Since: 0.2
 */
void
clutter_behaviour_set_alpha (ClutterBehaviour *behave,
			     ClutterAlpha     *alpha)
{
  ClutterBehaviourPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR (behave));
  g_return_if_fail (alpha == NULL || CLUTTER_IS_ALPHA (alpha));

  priv = behave->priv;

  if (alpha)
    g_object_ref_sink (alpha);

  if (priv->notify_id)
    {
      CLUTTER_NOTE (BEHAVIOUR, "removing previous notify-id (%d)",
                    priv->notify_id);

      g_signal_handler_disconnect (priv->alpha, priv->notify_id);
      priv->notify_id = 0;
    }

  if (priv->alpha)
    {
      CLUTTER_NOTE (BEHAVIOUR, "removing previous alpha object");

      g_object_unref (priv->alpha);
      priv->alpha = NULL;
    }

  if (alpha)
    {
      priv->alpha = alpha;

      priv->notify_id = g_signal_connect (priv->alpha, "notify::alpha",
                                          G_CALLBACK(notify_cb),
                                          behave);
      
      CLUTTER_NOTE (BEHAVIOUR, "setting new alpha object (%p, notify:%d)",
                    priv->alpha, priv->notify_id);
    }
}

/**
 * clutter_behaviour_get_actors:
 * @behave: a #ClutterBehaviour
 *
 * Retrieves all the actors to which @behave applies. It is not recommended
 * for derived classes to use this in there alpha notify method but use 
 * #clutter_behaviour_actors_foreach as it avoids alot of needless allocations.
 *
 * Return value: (transfer container) (element-type ClutterActor): a list of actors.
 *   You should free the returned list with g_slist_free() when finished using it.
 * 
 * Since: 0.2
 */
GSList *
clutter_behaviour_get_actors (ClutterBehaviour *behave)
{
  ClutterBehaviourPrivate *priv;
  GSList *retval, *l;

  g_return_val_if_fail (CLUTTER_BEHAVIOUR (behave), NULL);

  priv = behave->priv;
  retval = NULL;
  for (l = priv->actors; l != NULL; l = l->next)
    retval = g_slist_prepend (retval, l->data);

  return g_slist_reverse (retval);
}

/**
 * clutter_behaviour_remove_all:
 * @behave: a #ClutterBehaviour
 *
 * Removes every actor from the list that @behave holds.
 *
 * Since: 0.4
 */
void
clutter_behaviour_remove_all (ClutterBehaviour *behave)
{
  ClutterBehaviourPrivate *priv;
  GSList *l;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR (behave));

  priv = behave->priv;
  for (l = priv->actors; l != NULL; l = l->next)
    {
      ClutterActor *actor = l->data;

      g_signal_emit (behave, behave_signals[REMOVED], 0, actor);
      g_signal_handlers_disconnect_by_func (actor,
                                            G_CALLBACK (remove_actor_on_destroy),
                                            behave);
      g_object_unref (actor);
    }

  g_slist_free (priv->actors);
  priv->actors = NULL;
}
