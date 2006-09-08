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

#include <string.h>
#include <gobject/gobjectnotifyqueue.c>

#include "clutter-actor.h"
#include "clutter-behaviour.h"
#include "clutter-marshal.h"

struct _ClutterBehaviourPrivate
{
  ClutterAlpha *alpha;

  GSList *actors;
};

enum
{
  PROP_0,

  PROP_ALPHA
};

enum
{
  NOTIFY_BEHAVIOUR,

  SIGNAL_LAST
};

static GObjectClass         *clutter_behaviour_parent_class = NULL;
static GParamSpecPool       *pspec_pool = NULL;
static GObjectNotifyContext  property_notify_context = { 0, };
static guint                 behaviour_signals[SIGNAL_LAST] = { 0, };
static GQuark                quark_property_bridge = 0;

#define CLUTTER_BEHAVIOUR_GET_PRIVATE(obj)         \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
               CLUTTER_TYPE_BEHAVIOUR,             \
               ClutterBehaviourPrivate))

static void
clutter_behaviour_dispatch_property_changed (ClutterBehaviour  *behaviour,
                                             guint              n_pspecs,
                                             GParamSpec       **pspecs)
{
  guint i;

  for (i = 0; i < n_pspecs; i++)
    {
      g_signal_emit (behaviour,
                     behaviour_signals[NOTIFY_BEHAVIOUR],
                     g_quark_from_string (pspecs[i]->name),
                     pspecs[i]);
    }
}

static void
clutter_behaviour_base_finalize (ClutterBehaviourClass *klass)
{
  GList *list, *node;

  list = g_param_spec_pool_list_owned (pspec_pool,
                                       G_OBJECT_CLASS_TYPE (klass));
  for (node = list; node != NULL; node = node->next)
    {
      GParamSpec *pspec = node->data;

      g_param_spec_pool_remove (pspec_pool, pspec);
      g_param_spec_unref (pspec);
    }

  g_list_free (list);
}

static void 
clutter_behaviour_finalize (GObject *object)
{
  ClutterBehaviour *self = CLUTTER_BEHAVIOUR (object);

  if (self->priv->alpha)
    g_object_unref (self->priv->alpha);

  if (self->priv->actors)
    {
      g_slist_foreach (self->priv->actors,
                       (GFunc) g_object_unref,
                       NULL);
      g_slist_free (self->priv->actors);
    }

  G_OBJECT_CLASS (clutter_behaviour_parent_class)->finalize (object);
}

static void
clutter_behaviour_set_property (GObject      *object, 
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
clutter_behaviour_get_property (GObject    *object, 
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
property_notify_dispatcher (GObject     *object,
                            guint        n_pspecs,
                            GParamSpec **pspecs)
{
  CLUTTER_BEHAVIOUR_GET_CLASS (object)->dispatch_property_changed (CLUTTER_BEHAVIOUR (object), n_pspecs, pspecs);
}

static void
clutter_behaviour_class_init (ClutterBehaviourClass *klass)
{
  GObjectClass        *object_class;

  object_class = G_OBJECT_CLASS (klass);

  clutter_behaviour_parent_class = g_type_class_peek_parent (klass);
  pspec_pool = g_param_spec_pool_new (FALSE);
  property_notify_context.quark_notify_queue =
    g_quark_from_static_string ("ClutterBehaviour-notify-queue");
  property_notify_context.dispatcher =
    property_notify_dispatcher;
  quark_property_bridge = g_quark_from_static_string ("clutter-property-bridge");

  object_class->finalize     = clutter_behaviour_finalize;
  object_class->set_property = clutter_behaviour_set_property;
  object_class->get_property = clutter_behaviour_get_property;

  klass->dispatch_property_changed = clutter_behaviour_dispatch_property_changed;

  g_type_class_add_private (object_class, sizeof (ClutterBehaviourPrivate));
}

static void
clutter_behaviour_init (ClutterBehaviour *self)
{
  ClutterBehaviourPrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_GET_PRIVATE (self);
  
  priv->actors = NULL;
}

GType
clutter_behaviour_get_type (void)
{
  static GType behaviour_type = 0;

  if (G_UNLIKELY (behaviour_type == 0))
    {
      const GTypeInfo behaviour_info =
      {
        sizeof (ClutterBehaviourClass),
        NULL, /* base init */
        (GBaseFinalizeFunc) clutter_behaviour_base_finalize,
        (GClassInitFunc) clutter_behaviour_class_init,
        NULL,
        NULL,
        sizeof (ClutterBehaviour),
        0,
        (GInstanceInitFunc) clutter_behaviour_init,
        NULL,
      };

      behaviour_type =
        g_type_register_static (G_TYPE_OBJECT, "ClutterBehaviour",
                                &behaviour_info,
                                G_TYPE_FLAG_ABSTRACT);
    }

  return behaviour_type;
}

/**
 * clutter_behaviour_apply:
 * @behaviour: a #ClutterBehaviour
 * @actor: a #ClutterActor
 *
 * Applies @behaviour to @actor.
 *
 * This function holds a reference on @actor.
 *
 * Since: 0.2
 */
void
clutter_behaviour_apply (ClutterBehaviour *behaviour,
                         ClutterActor     *actor)
{
  g_return_if_fail (CLUTTER_IS_BEHAVIOUR (behaviour));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  if (g_slist_find (behaviour->priv->actors, actor))
    {
      g_warning ("The behaviour of type `%s' has already been "
                 "applied to the actor of type `%s'",
                 G_OBJECT_TYPE_NAME (behaviour),
                 G_OBJECT_TYPE_NAME (actor));
      return;
    }

  g_object_ref (behaviour);
  g_object_freeze_notify (G_OBJECT (behaviour));

  g_object_ref (actor);
  behaviour->priv->actors = g_slist_append (behaviour->priv->actors, actor);

  g_object_thaw_notify (G_OBJECT (behaviour));
  g_object_unref (behaviour);
}

/**
 * clutter_behaviour_remove:
 * @behaviour: a #ClutterBehaviour
 * @actor: a #ClutterActor
 *
 * Removes @behaviour from the list of behaviours
 * applied to @actor.
 *
 * This function removes a reference on @actor.
 *
 * Since: 0.2
 */
void
clutter_behaviour_remove (ClutterBehaviour *behaviour,
                          ClutterActor     *actor)
{
  g_return_if_fail (CLUTTER_IS_BEHAVIOUR (behaviour));
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  g_object_ref (behaviour);
  g_object_freeze_notify (G_OBJECT (behaviour));

  if (g_slist_find (behaviour->priv->actors, (gconstpointer) actor))
    {
      behaviour->priv->actors = g_slist_remove (behaviour->priv->actors, actor);
      
      g_object_unref (actor);
    }
  else
    {
      g_warning ("The behaviour of type `%s' was not applied "
                 "to the actor of type `%s'",
                 G_OBJECT_TYPE_NAME (behaviour),
                 G_OBJECT_TYPE_NAME (actor));
    }

  g_object_thaw_notify (G_OBJECT (behaviour));
  g_object_unref (behaviour);
}

/**
 * clutter_behaviour_actors_foreach:
 * @behaviour: a #ClutterBehaviour
 * @func: the function you want to apply to all #ClutterActor objects
 * @data: data to be passed to @func
 *
 * Applies @func to all the actors bound to @behaviour.
 *
 * Since: 0.2
 */
void
clutter_behaviour_actors_foreach (ClutterBehaviour *behaviour,
				  GFunc             func,
				  gpointer          data)
{
  g_return_if_fail (CLUTTER_IS_BEHAVIOUR (behaviour));
  g_return_if_fail (func != NULL);

  g_slist_foreach (behaviour->priv->actors, func, data);
}

/**
 * clutter_behaviour_get_actors:
 * @behaviour: a #ClutterBehaviour
 *
 * Gets the list of all #ClutterActor objects bound to @behaviour.
 *
 * Return value: a GSList with all the actors.  The elements of 
 *   the list are owned by the #ClutterBehaviour and should not
 *   be freed or modified.  You should free the list when done.
 *
 * Since: 0.2
 */
GSList *
clutter_behaviour_get_actors (ClutterBehaviour *behaviour)
{
  GSList *retval, *l;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (behaviour), NULL);

  retval = NULL;

  for (l = behaviour->priv->actors; l != NULL; l = l->next)
    retval = g_slist_prepend (retval, l->data);

  return g_slist_reverse (retval);
}

typedef struct
{
  GType actor_type;
  gchar *actor_property;
  ClutterAlphaTransform transform_func;
} BridgeData;

static void
bridge_data_free (BridgeData *data)
{
  g_free (data->actor_property);
  g_slice_free (BridgeData, data);
}

static void
clutter_behaviour_apply_value (ClutterBehaviour *behaviour,
                               GType             actor_type,
                               const gchar      *actor_property,
                               const GValue     *value)
{
  GSList *l;

  for (l = behaviour->priv->actors; l != NULL; l = l->next)
    {
      ClutterActor *actor = l->data;

      if (G_OBJECT_TYPE (actor) == actor_type)
        {
          g_object_set_property (G_OBJECT (actor),
                                 actor_property,
                                 value);
        }
    }
}

static void
alpha_notify_func (ClutterAlpha     *alpha,
                   GParamSpec       *alpha_pspec,
                   ClutterBehaviour *behaviour)
{
  ClutterBehaviourClass *klass;
  GParamSpec **pspecs;
  guint n_pspecs, i;
  GValue alpha_value = { 0, };

  klass = CLUTTER_BEHAVIOUR_GET_CLASS (behaviour);

  g_value_init (&alpha_value, G_TYPE_UINT);
  g_value_set_uint (&alpha_value, clutter_alpha_get_value (alpha));

  pspecs = clutter_behaviour_class_list_properties (klass, &n_pspecs);
  for (i = 0; i < n_pspecs; i++)
    {
      BridgeData *data;
      GParamSpec *redirect;
      GValue transform_value = { 0, };
      gboolean res;

      redirect = g_param_spec_get_redirect_target (pspecs[i]);
      if (!redirect)
        redirect = pspecs[i];

      data = g_param_spec_get_qdata (redirect, quark_property_bridge);
      g_assert (data != NULL);

      res = data->transform_func (behaviour, redirect,
                                  &alpha_value,
                                  &transform_value);
      if (!res)
        continue;

      clutter_behaviour_apply_value (behaviour,
                                     data->actor_type,
                                     data->actor_property,
                                     &transform_value);
    }

  g_free (pspecs);
}

void
clutter_behaviour_set_alpha (ClutterBehaviour *behaviour,
                             ClutterAlpha     *alpha)
{
  ClutterBehaviourPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR (behaviour));
  g_return_if_fail (alpha == NULL || CLUTTER_IS_ALPHA (alpha));

  priv = behaviour->priv;

  if (priv->alpha == alpha)
    return;

  g_object_ref (behaviour);

  if (priv->alpha)
    {
      g_signal_handlers_disconnect_by_func (priv->alpha,
                                            alpha_notify_func,
                                            behaviour);
      g_object_unref (priv->alpha);
      priv->alpha = NULL;
    }

  if (alpha)
    {
      priv->alpha = alpha;
      g_object_ref_sink (priv->alpha);

      g_signal_connect (priv->alpha, "notify::alpha",
                        G_CALLBACK (alpha_notify_func),
                        behaviour);
    }

  g_object_notify (G_OBJECT (behaviour), "alpha");
  g_object_unref (behaviour);
}

ClutterAlpha *
clutter_behaviour_get_alpha (ClutterBehaviour *behaviour)
{
  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR (behaviour), NULL);

  if (behaviour->priv->alpha)
    return g_object_ref (behaviour->priv->alpha);

  return NULL;
}

/**
 * clutter_behaviour_class_bind_property:
 * @klass: a #ClutterBehaviourClass
 * @actor_type: the type of the actor's class
 * @actor_property: the name of the actor's class property
 * @pspec: a #GParamSpec
 *
 * FIXME
 *
 * Since: 0.2
 */
void
clutter_behaviour_class_bind_property (ClutterBehaviourClass *klass,
                                       GType                  actor_type,
                                       const gchar           *actor_property,
                                       ClutterAlphaTransform  func)
{
  GParamSpec *overridden, *pspec;
  GObjectClass *actor_class;
  BridgeData *bridge_data;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_CLASS (klass));
  g_return_if_fail (actor_type != G_TYPE_INVALID);
  g_return_if_fail (actor_property != NULL);
  g_return_if_fail (func != NULL);

  actor_class = g_type_class_ref (actor_type);
  if (!actor_class)
    {
      g_warning (G_STRLOC ": unable to find class `%s'",
                 g_type_name (actor_type));
      return;
    }

  pspec = g_object_class_find_property (actor_class, actor_property);
  if (!pspec)
    {
      g_warning (G_STRLOC ": objects of class `%s' do not have a "
                 "property called `%s'",
                 g_type_name (actor_type),
                 actor_property);
    }
  g_type_class_unref (actor_class);

  overridden = g_param_spec_override (pspec->name, pspec);
  
  bridge_data = g_slice_new (BridgeData);
  bridge_data->actor_type = actor_type;
  bridge_data->actor_property = g_strdup (actor_property);
  bridge_data->transform_func = func;

  g_param_spec_ref (overridden);
  g_param_spec_set_qdata_full (overridden, quark_property_bridge,
                               bridge_data,
                               (GDestroyNotify) bridge_data_free);
  g_param_spec_pool_insert (pspec_pool, overridden, G_OBJECT_CLASS_TYPE (klass));
}

/**
 * clutter_behaviour_class_list_properties:
 * @klass: a #ClutterBehaviourClass
 * @n_pspecs: return location for the number of properties found
 *
 * FIXME
 *
 * Return value: an allocated list of #GParamSpec objects.  Use
 *   g_free() on the returned value when done.
 * 
 * Since: 0.2
 */
GParamSpec **
clutter_behaviour_class_list_properties (ClutterBehaviourClass *klass,
                                         guint                 *n_pspecs)
{
  GParamSpec **pspecs;
  guint n;

  pspecs = g_param_spec_pool_list (pspec_pool,
                                   G_OBJECT_CLASS_TYPE (klass),
                                   &n);

  if (n_pspecs)
    *n_pspecs = n;

  return pspecs;
}

/**
 * clutter_behaviour_class_find_property:
 * @klass: a #ClutterBehaviourClass
 * @property_name: the name of the property
 *
 * FIXME
 *
 * Return value: a #GParamSpec or %NULL
 *
 * Since: 0.2
 */
GParamSpec *
clutter_behaviour_class_find_property (ClutterBehaviourClass *klass,
                                       const gchar           *property_name)
{
  GParamSpec *pspec;
  GParamSpec *redirect;

  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR_CLASS (klass), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  pspec = g_param_spec_pool_lookup (pspec_pool,
                                    property_name,
                                    G_OBJECT_CLASS_TYPE (klass),
                                    TRUE);
  if (pspec)
    {
      redirect = g_param_spec_get_redirect_target (pspec);
      if (redirect)
        return redirect;
      else
        return pspec;
    }
  else
    return NULL;
}
