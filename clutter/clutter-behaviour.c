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
 */

#include "config.h"

#include "clutter-actor.h"
#include "clutter-behaviour.h"

G_DEFINE_TYPE (ClutterBehaviour, clutter_behaviour, G_TYPE_OBJECT);

struct ClutterBehaviourPrivate
{
  ClutterAlpha *alpha;
  guint         notify_id;
  GSList       *actors;
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
_clutter_behaviour_dispose (GObject *object)
{
  ClutterBehaviour *self = CLUTTER_BEHAVIOUR(object); 

  if (self->priv)
    {
      /* FIXME: remove all actors */

      clutter_behaviour_set_alpha (self, NULL);
    }

  G_OBJECT_CLASS (clutter_behaviour_parent_class)->dispose (object);
}

static void 
_clutter_behaviour_finalize (GObject *object)
{
  ClutterBehaviour *self = CLUTTER_BEHAVIOUR(object); 

  if (self->priv)
    {
      g_free(self->priv);
      self->priv = NULL;
    }

  G_OBJECT_CLASS (clutter_behaviour_parent_class)->finalize (object);
}

static void
_clutter_behaviour_set_property (GObject      *object, 
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
_clutter_behaviour_get_property (GObject    *object, 
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
clutter_behaviour_class_init (ClutterBehaviourClass *klass)
{
  GObjectClass        *object_class;

  object_class = (GObjectClass*) klass;

  object_class->finalize     = _clutter_behaviour_finalize;
  object_class->dispose      = _clutter_behaviour_dispose;
  object_class->set_property = _clutter_behaviour_set_property;
  object_class->get_property = _clutter_behaviour_get_property;

  g_object_class_install_property
    (object_class, PROP_ALPHA,
     g_param_spec_object ("alpha",
			  "Alpha",
			  "Alpha Object to drive the behaviour",
			  CLUTTER_TYPE_ALPHA,
			  G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (ClutterBehaviourPrivate));
}

static void
clutter_behaviour_init (ClutterBehaviour *self)
{
  ClutterBehaviourPrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_GET_PRIVATE (self);

}

ClutterBehaviour*
clutter_behaviour_new (GObject    *object,
                       const char *property)
{
  ClutterBehaviour *behave;

  behave = g_object_new (CLUTTER_TYPE_BEHAVIOUR, 
                         "object", object,
                         "property", property,
			 NULL);

  return behave;
}

void
clutter_behaviour_apply (ClutterBehaviour *behave, ClutterActor *actor)
{
  g_return_if_fail (actor != NULL);

  if (g_slist_find (behave->priv->actors, (gconstpointer)actor))
    return;

  g_object_ref (actor);
  behave->priv->actors = g_slist_append (behave->priv->actors, actor);
}

void
clutter_behaviour_remove (ClutterBehaviour *behave, ClutterActor *actor)
{
  g_return_if_fail (actor != NULL);

  if (g_slist_find (behave->priv->actors, (gconstpointer)actor))
    {
      g_object_unref (actor);
      behave->priv->actors = g_slist_remove (behave->priv->actors, actor);
    }
}

void
clutter_behaviour_remove_all (ClutterBehaviour *behave)
{
  /* tofix */
}

void
clutter_behaviour_actors_foreach (ClutterBehaviour *behave,
				  GFunc             func,
				  gpointer          userdata)
{
  g_slist_foreach (behave->priv->actors, func, userdata);
}

ClutterAlpha*
clutter_behaviour_get_alpha (ClutterBehaviour *behave)
{
  return behave->priv->alpha;
}

static void
notify_cb (GObject          *object,
           GParamSpec       *param_spec,
           ClutterBehaviour *behave)
{
  ClutterBehaviourClass *class;

  class = CLUTTER_BEHAVIOUR_GET_CLASS(behave);

  if (class->alpha_notify)
    class->alpha_notify (behave);
}

void
clutter_behaviour_set_alpha (ClutterBehaviour *behave,
			     ClutterAlpha     *alpha)
{
  if (behave->priv->notify_id)
    {
      g_signal_handler_disconnect (behave->priv->alpha,
                                   behave->priv->notify_id);
      behave->priv->notify_id = 0;
    }

  if (behave->priv->alpha)
    {
      g_object_unref (behave->priv->alpha);
      behave->priv->alpha = NULL;
    }

  if (alpha)
    {
      behave->priv->alpha = alpha;
      g_object_ref (behave->priv->alpha);

      behave->priv->notify_id = g_signal_connect (behave->priv->alpha, 
						  "notify::alpha",
						  G_CALLBACK(notify_cb),
						  behave);
    }
}
