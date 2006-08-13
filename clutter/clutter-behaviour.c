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

#include "clutter-behaviour.h"
#include "clutter-enum-types.h"
#include "clutter-private.h" 	/* for DBG */
#include "clutter-timeline.h"

G_DEFINE_TYPE (ClutterBehaviour, clutter_behaviour, G_TYPE_OBJECT);

struct ClutterBehaviourPrivate
{
  ClutterTimeline *timeline;
  GSList          *actors;
};

enum
{
  PROP_0,
  PROP_TIMELINE
};

#define CLUTTER_BEHAVIOUR_GET_PRIVATE(obj)         \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
               CLUTTER_TYPE_BEHAVIOUR,             \
               ClutterBehaviourPrivate))

static void 
clutter_behaviour_dispose (GObject *object)
{
  ClutterBehaviour *self = CLUTTER_BEHAVIOUR(object); 

  if (self->priv)
    {
      /* FIXME: remove all actors */
    }

  G_OBJECT_CLASS (clutter_behaviour_parent_class)->dispose (object);
}

static void 
clutter_behaviour_finalize (GObject *object)
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
clutter_behaviour_set_property (GObject      *object, 
				guint         prop_id,
				const GValue *value, 
				GParamSpec   *pspec)
{
  ClutterBehaviour        *behaviour;
  ClutterBehaviourPrivate *priv;

  behaviour = CLUTTER_BEHAVIOUR(object);
  priv      = CLUTTER_BEHAVIOUR_GET_PRIVATE(behaviour);

  switch (prop_id) 
    {
    case PROP_TIMELINE:
      clutter_behaviour_set_timelime (behaviour, g_value_get_object (value));
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
    case PROP_TIMELINE:
      g_value_set_object (value, priv->timeline);
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

  object_class->finalize     = clutter_behaviour_finalize;
  object_class->dispose      = clutter_behaviour_dispose;
  object_class->set_property = clutter_behaviour_set_property;
  object_class->get_property = clutter_behaviour_get_property;

  g_object_class_install_property
    (object_class, PROP_TIMELINE,
     g_param_spec_object ("timeline",
			  "Timeline",
			  "Timeline source for behaviour",
			  CLUTTER_TYPE_TIMELINE,
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
clutter_behaviour_new (ClutterTimeline *timeline)
{
  ClutterBehaviour *behave;

  behave = g_object_new (CLUTTER_TYPE_BEHAVIOUR, 
			 "timeline", timeline,
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

ClutterTimeline*
clutter_behaviour_get_timelime (ClutterBehaviour *behave)
{
  return behave->priv->timeline;
}

void
clutter_behaviour_set_timelime (ClutterBehaviour *behave, 
				ClutterTimeline  *timeline)
{
  ClutterBehaviourPrivate *priv;

  priv = CLUTTER_BEHAVIOUR_GET_PRIVATE(behave);

  if (priv->timeline)
    {
      g_object_unref(priv->timeline);
      priv->timeline = NULL;
    }

  if (behave)
    {
      g_object_ref(timeline);
      priv->timeline = timeline;
    }
}
