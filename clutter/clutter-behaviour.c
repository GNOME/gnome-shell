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
#include "clutter-marshal.h"

G_DEFINE_TYPE (ClutterBehaviour, clutter_behaviour, G_TYPE_OBJECT);

struct ClutterBehaviourPrivate
{
  GObject    *object;
  GParamSpec *param_spec;
  guint       notify_id;
  GSList     *actors;
};

enum
{
  PROP_0,
  PROP_OBJECT,
  PROP_PROPERTY
};

enum {
  SIGNAL_PROPERTY_CHANGE,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

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

      clutter_behaviour_set_object (self, NULL);
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
    case PROP_OBJECT:
      clutter_behaviour_set_object (behaviour, g_value_get_object (value));
      break;
    case PROP_PROPERTY:
      clutter_behaviour_set_property (behaviour, g_value_get_string (value));
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
    case PROP_OBJECT:
      g_value_set_object (value, priv->object);
      break;
    case PROP_PROPERTY:
      g_value_set_string (value, priv->param_spec->name);
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
    (object_class, PROP_OBJECT,
     g_param_spec_object ("object",
			  "Object",
			  "Object whose property to monitor",
			  G_TYPE_OBJECT,
			  G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property
    (object_class, PROP_PROPERTY,
     g_param_spec_string ("property",
			  "Property",
			  "Property to monitor",
                          NULL,
			  G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  signals[SIGNAL_PROPERTY_CHANGE] =
     g_signal_new ("property-change",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterBehaviourClass, property_change),
		  NULL, NULL,
		  clutter_marshal_VOID__OBJECT_POINTER,
		  G_TYPE_NONE, 
		  2, G_TYPE_OBJECT, G_TYPE_POINTER);

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

GObject*
clutter_behaviour_get_object (ClutterBehaviour *behave)
{
  return behave->priv->object;
}

void
clutter_behaviour_set_object (ClutterBehaviour *behave, 
                              GObject          *object)
{
  ClutterBehaviourPrivate *priv;
  const char *property;

  priv = CLUTTER_BEHAVIOUR_GET_PRIVATE(behave);

  if (priv->object)
    {
      property = clutter_behaviour_get_property (behave);
      clutter_behaviour_set_property (behave, NULL);

      g_object_unref(priv->object);
      priv->object = NULL;
    }
  else
    property = NULL;

  if (object)
    {
      priv->object = g_object_ref(object);

      if (property)
        clutter_behaviour_set_property (behave, property);
    }
}

const char *
clutter_behaviour_get_property (ClutterBehaviour *behave)
{
  if (behave->priv->param_spec)
    return behave->priv->param_spec->name;
  else
    return NULL;
}

GParamSpec *
clutter_behaviour_get_param_spec (ClutterBehaviour *behave)
{
  return behave->priv->param_spec;
}

static void
notify_cb (GObject          *object,
           GParamSpec       *param_spec,
           ClutterBehaviour *behave)
{
        g_signal_emit (behave,
                       signals[SIGNAL_PROPERTY_CHANGE],
                       0,
                       object,
                       param_spec);
}

void
clutter_behaviour_set_property (ClutterBehaviour *behave,
                                const char       *property)
{
  g_return_if_fail (behave->priv->object);

  if (behave->priv->notify_id)
    {
      g_signal_handler_disconnect (behave->priv->object,
                                   behave->priv->notify_id);
      behave->priv->notify_id = 0;
    }

  behave->priv->param_spec = NULL;

  if (property)
    {
      guint signal_id;
      GClosure *closure;

      behave->priv->param_spec =
        g_object_class_find_property (G_OBJECT_GET_CLASS (behave->priv->object),
                                      property);
      g_return_if_fail (behave->priv->param_spec);

      signal_id = g_signal_lookup ("notify",
                                   G_OBJECT_TYPE (behave->priv->object));
      closure = g_cclosure_new ((GCallback) notify_cb, behave, NULL);

      behave->priv->notify_id =
        g_signal_connect_closure_by_id (behave->priv->object,
                                        signal_id,
                                        g_quark_from_string (property),
                                        closure,
                                        FALSE);
    }
}
