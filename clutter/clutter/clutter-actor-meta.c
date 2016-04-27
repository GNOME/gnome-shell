/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-actor-meta
 * @Title: ClutterActorMeta
 * @Short_Description: Base class of actor modifiers
 * @See_Also: #ClutterAction, #ClutterConstraint
 *
 * #ClutterActorMeta is an abstract class providing a common API for
 * modifiers of #ClutterActor behaviour, appearance or layout.
 *
 * A #ClutterActorMeta can only be owned by a single #ClutterActor at
 * any time.
 *
 * Every sub-class of #ClutterActorMeta should check if the
 * #ClutterActorMeta:enabled property is set to %TRUE before applying
 * any kind of modification.
 *
 * #ClutterActorMeta is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-actor-meta-private.h"

#include "clutter-debug.h"
#include "clutter-private.h"

struct _ClutterActorMetaPrivate
{
  ClutterActor *actor;
  guint destroy_id;

  gchar *name;

  guint is_enabled : 1;

  gint priority;
};

enum
{
  PROP_0,

  PROP_ACTOR,
  PROP_NAME,
  PROP_ENABLED,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ClutterActorMeta,
                                     clutter_actor_meta,
                                     G_TYPE_INITIALLY_UNOWNED)

static void
on_actor_destroy (ClutterActor     *actor,
                  ClutterActorMeta *meta)
{
  meta->priv->actor = NULL;
}

static void
clutter_actor_meta_real_set_actor (ClutterActorMeta *meta,
                                   ClutterActor     *actor)
{
  if (meta->priv->actor == actor)
    return;

  if (meta->priv->destroy_id != 0)
    {
      g_signal_handler_disconnect (meta->priv->actor, meta->priv->destroy_id);
      meta->priv->destroy_id = 0;
    }

  meta->priv->actor = actor;

  if (meta->priv->actor != NULL)
    meta->priv->destroy_id = g_signal_connect (meta->priv->actor, "destroy",
                                               G_CALLBACK (on_actor_destroy),
                                               meta);
}

static void
clutter_actor_meta_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ClutterActorMeta *meta = CLUTTER_ACTOR_META (gobject);

  switch (prop_id)
    {
    case PROP_NAME:
      clutter_actor_meta_set_name (meta, g_value_get_string (value));
      break;

    case PROP_ENABLED:
      clutter_actor_meta_set_enabled (meta, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_actor_meta_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ClutterActorMeta *meta = CLUTTER_ACTOR_META (gobject);

  switch (prop_id)
    {
    case PROP_ACTOR:
      g_value_set_object (value, meta->priv->actor);
      break;

    case PROP_NAME:
      g_value_set_string (value, meta->priv->name);
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, meta->priv->is_enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_actor_meta_finalize (GObject *gobject)
{
  ClutterActorMetaPrivate *priv = CLUTTER_ACTOR_META (gobject)->priv;

  if (priv->destroy_id != 0 && priv->actor != NULL)
    g_signal_handler_disconnect (priv->actor, priv->destroy_id);

  g_free (priv->name);

  G_OBJECT_CLASS (clutter_actor_meta_parent_class)->finalize (gobject);
}

void
clutter_actor_meta_class_init (ClutterActorMetaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->set_actor = clutter_actor_meta_real_set_actor;

  /**
   * ClutterActorMeta:actor:
   *
   * The #ClutterActor attached to the #ClutterActorMeta instance
   *
   * Since: 1.4
   */
  obj_props[PROP_ACTOR] =
    g_param_spec_object ("actor",
                         P_("Actor"),
                         P_("The actor attached to the meta"),
                         CLUTTER_TYPE_ACTOR,
                         CLUTTER_PARAM_READABLE);

  /**
   * ClutterActorMeta:name:
   *
   * The unique name to access the #ClutterActorMeta
   *
   * Since: 1.4
   */
  obj_props[PROP_NAME] =
    g_param_spec_string ("name",
                         P_("Name"),
                         P_("The name of the meta"),
                         NULL,
                         CLUTTER_PARAM_READWRITE);

  /**
   * ClutterActorMeta:enabled:
   *
   * Whether or not the #ClutterActorMeta is enabled
   *
   * Since: 1.4
   */
  obj_props[PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          P_("Enabled"),
                          P_("Whether the meta is enabled"),
                          TRUE,
                          CLUTTER_PARAM_READWRITE);

  gobject_class->finalize = clutter_actor_meta_finalize;
  gobject_class->set_property = clutter_actor_meta_set_property;
  gobject_class->get_property = clutter_actor_meta_get_property;
  g_object_class_install_properties (gobject_class,
                                     PROP_LAST,
                                     obj_props);
}

void
clutter_actor_meta_init (ClutterActorMeta *self)
{
  self->priv = clutter_actor_meta_get_instance_private (self);
  self->priv->is_enabled = TRUE;
  self->priv->priority = CLUTTER_ACTOR_META_PRIORITY_DEFAULT;
}

/**
 * clutter_actor_meta_set_name:
 * @meta: a #ClutterActorMeta
 * @name: the name of @meta
 *
 * Sets the name of @meta
 *
 * The name can be used to identify the #ClutterActorMeta instance
 *
 * Since: 1.4
 */
void
clutter_actor_meta_set_name (ClutterActorMeta *meta,
                             const gchar      *name)
{
  g_return_if_fail (CLUTTER_IS_ACTOR_META (meta));

  if (g_strcmp0 (meta->priv->name, name) == 0)
    return;

  g_free (meta->priv->name);
  meta->priv->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (meta), obj_props[PROP_NAME]);
}

/**
 * clutter_actor_meta_get_name:
 * @meta: a #ClutterActorMeta
 *
 * Retrieves the name set using clutter_actor_meta_set_name()
 *
 * Return value: (transfer none): the name of the #ClutterActorMeta
 *   instance, or %NULL if none was set. The returned string is owned
 *   by the #ClutterActorMeta instance and it should not be modified
 *   or freed
 *
 * Since: 1.4
 */
const gchar *
clutter_actor_meta_get_name (ClutterActorMeta *meta)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR_META (meta), NULL);

  return meta->priv->name;
}

/**
 * clutter_actor_meta_set_enabled:
 * @meta: a #ClutterActorMeta
 * @is_enabled: whether @meta is enabled
 *
 * Sets whether @meta should be enabled or not
 *
 * Since: 1.4
 */
void
clutter_actor_meta_set_enabled (ClutterActorMeta *meta,
                                gboolean          is_enabled)
{
  g_return_if_fail (CLUTTER_IS_ACTOR_META (meta));

  is_enabled = !!is_enabled;

  if (meta->priv->is_enabled == is_enabled)
    return;

  meta->priv->is_enabled = is_enabled;

  g_object_notify_by_pspec (G_OBJECT (meta), obj_props[PROP_ENABLED]);
}

/**
 * clutter_actor_meta_get_enabled:
 * @meta: a #ClutterActorMeta
 *
 * Retrieves whether @meta is enabled
 *
 * Return value: %TRUE if the #ClutterActorMeta instance is enabled
 *
 * Since: 1.4
 */
gboolean
clutter_actor_meta_get_enabled (ClutterActorMeta *meta)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR_META (meta), FALSE);

  return meta->priv->is_enabled;
}

/*
 * _clutter_actor_meta_set_actor
 * @meta: a #ClutterActorMeta
 * @actor: a #ClutterActor or %NULL
 *
 * Sets or unsets a back pointer to the #ClutterActor that owns
 * the @meta
 *
 * Since: 1.4
 */
void
_clutter_actor_meta_set_actor (ClutterActorMeta *meta,
                               ClutterActor     *actor)
{
  g_return_if_fail (CLUTTER_IS_ACTOR_META (meta));
  g_return_if_fail (actor == NULL || CLUTTER_IS_ACTOR (actor));

  CLUTTER_ACTOR_META_GET_CLASS (meta)->set_actor (meta, actor);
}

/**
 * clutter_actor_meta_get_actor:
 * @meta: a #ClutterActorMeta
 *
 * Retrieves a pointer to the #ClutterActor that owns @meta
 *
 * Return value: (transfer none): a pointer to a #ClutterActor or %NULL
 *
 * Since: 1.4
 */
ClutterActor *
clutter_actor_meta_get_actor (ClutterActorMeta *meta)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR_META (meta), NULL);

  return meta->priv->actor;
}

void
_clutter_actor_meta_set_priority (ClutterActorMeta *meta,
                                  gint priority)
{
  g_return_if_fail (CLUTTER_IS_ACTOR_META (meta));

  /* This property shouldn't be modified after the actor meta is in
     use because ClutterMetaGroup doesn't resort the list when it
     changes. If we made the priority public then we could either make
     the priority a construct-only property or listen for
     notifications on the property from the ClutterMetaGroup and
     resort. */
  g_return_if_fail (meta->priv->actor == NULL);

  meta->priv->priority = priority;
}

gint
_clutter_actor_meta_get_priority (ClutterActorMeta *meta)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR_META (meta), 0);

  return meta->priv->priority;
}

gboolean
_clutter_actor_meta_is_internal (ClutterActorMeta *meta)
{
  gint priority = meta->priv->priority;

  return (priority <= CLUTTER_ACTOR_META_PRIORITY_INTERNAL_LOW ||
          priority >= CLUTTER_ACTOR_META_PRIORITY_INTERNAL_HIGH);
}

/*
 * ClutterMetaGroup: a collection of ClutterActorMeta instances
 */

G_DEFINE_TYPE (ClutterMetaGroup, _clutter_meta_group, G_TYPE_OBJECT);

static void
_clutter_meta_group_dispose (GObject *gobject)
{
  _clutter_meta_group_clear_metas (CLUTTER_META_GROUP (gobject));

  G_OBJECT_CLASS (_clutter_meta_group_parent_class)->dispose (gobject);
}

static void
_clutter_meta_group_class_init (ClutterMetaGroupClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = _clutter_meta_group_dispose;
}

static void
_clutter_meta_group_init (ClutterMetaGroup *self)
{
}

/*
 * _clutter_meta_group_add_meta:
 * @group: a #ClutterMetaGroup
 * @meta: a #ClutterActorMeta to add
 *
 * Adds @meta to @group
 *
 * This function will remove the floating reference of @meta or, if the
 * floating reference has already been sunk, add a reference to it
 */
void
_clutter_meta_group_add_meta (ClutterMetaGroup *group,
                              ClutterActorMeta *meta)
{
  GList *prev = NULL, *l;

  if (meta->priv->actor != NULL)
    {
      g_warning ("The meta of type '%s' with name '%s' is "
                 "already attached to actor '%s'",
                 G_OBJECT_TYPE_NAME (meta),
                 meta->priv->name != NULL
                   ? meta->priv->name
                   : "<unknown>",
                 clutter_actor_get_name (meta->priv->actor) != NULL
                   ? clutter_actor_get_name (meta->priv->actor)
                   : G_OBJECT_TYPE_NAME (meta->priv->actor));
      return;
    }

  /* Find a meta that has lower priority and insert before that */
  for (l = group->meta; l; l = l->next)
    if (_clutter_actor_meta_get_priority (l->data) <
        _clutter_actor_meta_get_priority (meta))
      break;
    else
      prev = l;

  if (prev == NULL)
    group->meta = g_list_prepend (group->meta, meta);
  else
    {
      prev->next = g_list_prepend (prev->next, meta);
      prev->next->prev = prev;
    }

  g_object_ref_sink (meta);

  _clutter_actor_meta_set_actor (meta, group->actor);
}

/*
 * _clutter_meta_group_remove_meta:
 * @group: a #ClutterMetaGroup
 * @meta: a #ClutterActorMeta to remove
 *
 * Removes @meta from @group and releases the reference being held on it
 */
void
_clutter_meta_group_remove_meta (ClutterMetaGroup *group,
                                 ClutterActorMeta *meta)
{
  if (meta->priv->actor != group->actor)
    {
      g_warning ("The meta of type '%s' with name '%s' is not "
                 "attached to the actor '%s'",
                 G_OBJECT_TYPE_NAME (meta),
                 meta->priv->name != NULL
                   ? meta->priv->name
                   : "<unknown>",
                 clutter_actor_get_name (group->actor) != NULL
                   ? clutter_actor_get_name (group->actor)
                   : G_OBJECT_TYPE_NAME (group->actor));
      return;
    }

  _clutter_actor_meta_set_actor (meta, NULL);

  group->meta = g_list_remove (group->meta, meta);
  g_object_unref (meta);
}

/*
 * _clutter_meta_group_peek_metas:
 * @group: a #ClutterMetaGroup
 *
 * Returns a pointer to the #ClutterActorMeta list
 *
 * Return value: a const pointer to the #GList of #ClutterActorMeta
 */
const GList *
_clutter_meta_group_peek_metas (ClutterMetaGroup *group)
{
  return group->meta;
}

/*
 * _clutter_meta_group_get_metas_no_internal:
 * @group: a #ClutterMetaGroup
 *
 * Returns a new allocated list containing all of the metas that don't
 * have an internal priority.
 *
 * Return value: A GList containing non-internal metas. Free with
 * g_list_free.
 */
GList *
_clutter_meta_group_get_metas_no_internal (ClutterMetaGroup *group)
{
  GList *ret = NULL;
  GList *l;

  /* Build a new list filtering out the internal metas */
  for (l = group->meta; l; l = l->next)
    if (!_clutter_actor_meta_is_internal (l->data))
      ret = g_list_prepend (ret, l->data);

  return g_list_reverse (ret);
}

/*
 * _clutter_meta_group_has_metas_no_internal:
 * @group: a #ClutterMetaGroup
 *
 * Returns whether the group has any metas that don't have an internal priority.
 *
 * Return value: %TRUE if metas without internal priority exist
 *   %FALSE otherwise
 */
gboolean
_clutter_meta_group_has_metas_no_internal (ClutterMetaGroup *group)
{
  GList *l;

  for (l = group->meta; l; l = l->next)
    if (!_clutter_actor_meta_is_internal (l->data))
      return TRUE;

  return FALSE;
}

/*
 * _clutter_meta_group_clear_metas:
 * @group: a #ClutterMetaGroup
 *
 * Clears @group of all #ClutterActorMeta instances and releases
 * the reference on them
 */
void
_clutter_meta_group_clear_metas (ClutterMetaGroup *group)
{
  g_list_foreach (group->meta, (GFunc) _clutter_actor_meta_set_actor, NULL);

  g_list_foreach (group->meta, (GFunc) g_object_unref, NULL);
  g_list_free (group->meta);
  group->meta = NULL;
}

/*
 * _clutter_meta_group_clear_metas_no_internal:
 * @group: a #ClutterMetaGroup
 *
 * Clears @group of all #ClutterActorMeta instances that don't have an
 * internal priority and releases the reference on them
 */
void
_clutter_meta_group_clear_metas_no_internal (ClutterMetaGroup *group)
{
  GList *internal_list = NULL;
  GList *l, *next;

  for (l = group->meta; l; l = next)
    {
      next = l->next;

      if (_clutter_actor_meta_is_internal (l->data))
        {
          if (internal_list)
            internal_list->prev = l;
          l->next = internal_list;
          l->prev = NULL;
          internal_list = l;
        }
      else
        {
          _clutter_actor_meta_set_actor (l->data, NULL);
          g_object_unref (l->data);
          g_list_free_1 (l);
        }
    }

  group->meta = g_list_reverse (internal_list);
}

/*
 * _clutter_meta_group_get_meta:
 * @group: a #ClutterMetaGroup
 * @name: the name of the #ClutterActorMeta to retrieve
 *
 * Retrieves a named #ClutterActorMeta from @group
 *
 * Return value: a #ClutterActorMeta for the given name, or %NULL
 */
ClutterActorMeta *
_clutter_meta_group_get_meta (ClutterMetaGroup *group,
                              const gchar      *name)
{
  GList *l;

  for (l = group->meta; l != NULL; l = l->next)
    {
      ClutterActorMeta *meta = l->data;

      if (g_strcmp0 (meta->priv->name, name) == 0)
        return meta;
    }

  return NULL;
}

/*< private >
 * clutter_actor_meta_get_debug_name:
 * @meta: a #ClutterActorMeta
 *
 * Retrieves the name of the @meta for debugging purposes.
 *
 * Return value: (transfer none): the name of the @meta. The returned
 *   string is owned by the @meta instance and it should not be
 *   modified or freed
 */
const gchar *
_clutter_actor_meta_get_debug_name (ClutterActorMeta *meta)
{
  return meta->priv->name != NULL ? meta->priv->name
                                  : G_OBJECT_TYPE_NAME (meta);
}
