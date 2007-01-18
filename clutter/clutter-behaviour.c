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
 * In order to implement a new behaviour you should subclass #ClutterBehaviour
 * and override the "alpha_notify" virtual function; inside the overridden
 * function you should obtain the alpha value from the #ClutterAlpha
 * instance bound to the behaviour and apply it to the desiderd property
 * (or properties) of every actor controlled by the behaviour. 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-main.h"
#include "clutter-actor.h"
#include "clutter-behaviour.h"
#include "clutter-debug.h"
#include "clutter-private.h"

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
  SIGNAL_LAST
};


#define CLUTTER_BEHAVIOUR_GET_PRIVATE(obj)         \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
               CLUTTER_TYPE_BEHAVIOUR,             \
               ClutterBehaviourPrivate))

static void 
clutter_behaviour_finalize (GObject *object)
{
  ClutterBehaviour *self = CLUTTER_BEHAVIOUR (object);

  clutter_behaviour_set_alpha (self, NULL);
  
  g_slist_foreach (self->priv->actors, (GFunc) g_object_unref, NULL);
  g_slist_free (self->priv->actors);

  G_OBJECT_CLASS (clutter_behaviour_parent_class)->finalize (object);
}

static void
clutter_behaviour_set_property (GObject      *object, 
				guint         prop_id,
				const GValue *value, 
				GParamSpec   *pspec)
{
  ClutterBehaviour *behaviour;

  behaviour = CLUTTER_BEHAVIOUR(object);

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
  ClutterBehaviour        *behaviour;
  ClutterBehaviourPrivate *priv;

  behaviour = CLUTTER_BEHAVIOUR(object);
  priv      = CLUTTER_BEHAVIOUR_GET_PRIVATE(behaviour);

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
                                              guint32           alpha_value)
{
  g_warning ("ClutterBehaviourClass::alpha_notify not implemented for `%s'",
             g_type_name (G_TYPE_FROM_INSTANCE (behaviour)));
}

static void
clutter_behaviour_class_init (ClutterBehaviourClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize     = clutter_behaviour_finalize;
  object_class->set_property = clutter_behaviour_set_property;
  object_class->get_property = clutter_behaviour_get_property;

  g_object_class_install_property (object_class,
                                   PROP_ALPHA,
                                   g_param_spec_object ("alpha",
                                                        "Alpha",
                                                        "Alpha Object to drive the behaviour",
                                                        CLUTTER_TYPE_ALPHA,
                                                        G_PARAM_CONSTRUCT |
                                                        CLUTTER_PARAM_READWRITE));

  klass->alpha_notify = clutter_behaviour_alpha_notify_unimplemented;

  g_type_class_add_private (klass, sizeof (ClutterBehaviourPrivate));
}

static void
clutter_behaviour_init (ClutterBehaviour *self)
{
  ClutterBehaviourPrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_GET_PRIVATE (self);

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
  g_return_if_fail (CLUTTER_IS_BEHAVIOUR (behave));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  if (g_slist_find (behave->priv->actors, actor))
    {
      g_warning ("The behaviour of type %s already applies "
                 "to the actor of type %s",
                 g_type_name (G_OBJECT_TYPE (behave)),
                 g_type_name (G_OBJECT_TYPE (actor)));
      return;
    }

  g_object_ref (actor);
  behave->priv->actors = g_slist_prepend (behave->priv->actors, actor);
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
  g_return_if_fail (CLUTTER_IS_BEHAVIOUR (behave));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  if (!g_slist_find (behave->priv->actors, actor))
    {
      g_warning ("The behaviour of type %s is not applied "
                 "to the actor of type %s",
                 g_type_name (G_OBJECT_TYPE (behave)),
                 g_type_name (G_OBJECT_TYPE (actor)));
      return;
    }
  
  g_object_unref (actor);
  behave->priv->actors = g_slist_remove (behave->priv->actors, actor);
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
 * @index:  the index of an actor this behaviour is applied too. 
 *
 * Gets an actor the behaviour was applied to referenced by index num.
 *
 * Return value: A Clutter actor or NULL if index is invalid.
 *
 * Since: 0.2
 */
ClutterActor*
clutter_behaviour_get_nth_actor (ClutterBehaviour *behave,
				 gint              index)
{
  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR (behave), NULL);

  return g_slist_nth_data (behave->priv->actors, index);
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
 * Return value: a #ClutterAlpha object, or %NULL if no alpha
 *   object has been bound to this behaviour.
 * 
 * Since: 0.2
 */
ClutterAlpha*
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

  if (klass->alpha_notify)
    {
      guint32 alpha_value;

      alpha_value = clutter_alpha_get_alpha (behave->priv->alpha);

      CLUTTER_NOTE (BEHAVIOUR, "calling %s::alpha_notify (%p, %d)",
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
 * Binds @alpha to a #ClutterBehaviour.  The #ClutterAlpha object
 * is what makes a behaviour work: for each tick of the timeline
 * used by #ClutterAlpha a new value of the alpha parameter is
 * computed by the alpha function; the value should be used by
 * the #ClutterBehaviour to update one or more properties of the
 * actors to which the behaviour applies.
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
      g_object_ref_sink (priv->alpha);

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
 * derived classes use this in there alpha notify method but use 
 * #clutter_behaviour_actors_foreach as it avoids alot of needless allocations.
 *
 * Return value: a list of actors. You should free the returned list
 *   with g_slist_free() when finished using it.
 * 
 * Since: 0.2
 */
GSList *
clutter_behaviour_get_actors (ClutterBehaviour *behave)
{
  GSList *retval, *l;

  g_return_val_if_fail (CLUTTER_BEHAVIOUR (behave), NULL);

  retval = NULL;
  for (l = behave->priv->actors; l != NULL; l = l->next)
    retval = g_slist_prepend (retval, l->data);

  return g_slist_reverse (retval);
}
