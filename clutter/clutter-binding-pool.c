/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2008  Intel Corporation.
 *
 * Authored By: Emmanuele Bassi <ebassi@linux.intel.com>
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
 */

/**
 * SECTION:clutter-binding-pool
 * @short_description: Pool for key bindings
 *
 * #ClutterBindingPool is a data structure holding a set of key bindings.
 * Each key binding associates a key symbol (eventually with modifiers)
 * to an action. A callback function is associated to each action.
 *
 * For a given key symbol and modifier mask combination there can be only one
 * action; for each action there can be only one callback. There can be
 * multiple actions with the same name, and the same callback can be used
 * to handle multiple key bindings.
 *
 * Actors requiring key bindings should create a new #ClutterBindingPool
 * inside their class initialization function and then install actions
 * like this:
 *
 * |[
 * static void
 * foo_class_init (FooClass *klass)
 * {
 *   ClutterBindingPool *binding_pool;
 *
 *   binding_pool = clutter_binding_pool_get_for_class (klass);
 *
 *   clutter_binding_pool_install_action (binding_pool, "move-up",
 *                                        CLUTTER_Up, 0,
 *                                        G_CALLBACK (foo_action_move_up),
 *                                        NULL, NULL);
 *   clutter_binding_pool_install_action (binding_pool, "move-up",
 *                                        CLUTTER_KP_Up, 0,
 *                                        G_CALLBACK (foo_action_move_up),
 *                                        NULL, NULL);
 * }
 * ]|
 *
 * The callback has a signature of:
 *
 * |[
 *    gboolean (* callback) (GObject             *instance,
 *                           const gchar         *action_name,
 *                           guint                key_val,
 *                           ClutterModifierType  modifiers,
 *                           gpointer             user_data);
 * ]|
 *
 * The actor should then override the #ClutterActor::key-press-event and
 * use clutter_binding_pool_activate() to match a #ClutterKeyEvent structure
 * to one of the actions:
 *
 * |[
 *   ClutterBindingPool *pool;
 *
 *   /&ast; retrieve the binding pool for the type of the actor &ast;/
 *   pool = clutter_binding_pool_find (G_OBJECT_TYPE_NAME (actor));
 *
 *   /&ast; activate any callback matching the key symbol and modifiers
 *    &ast; mask of the key event. the returned value can be directly
 *    &ast; used to signal that the actor has handled the event.
 *    &ast;/
 *   return clutter_binding_pool_activate (pool, G_OBJECT (actor),
 *                                         key_event-&gt;keyval,
 *                                         key_event-&gt;modifier_state);
 * ]|
 *
 * The clutter_binding_pool_activate() function will return %FALSE if
 * no action for the given key binding was found, if the action was
 * blocked (using clutter_binding_pool_block_action()) or if the
 * key binding handler returned %FALSE.
 *
 * #ClutterBindingPool is available since Clutter 1.0
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-binding-pool.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

#define CLUTTER_BINDING_POOL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), CLUTTER_TYPE_BINDING_POOL, ClutterBindingPoolClass))
#define CLUTTER_IS_BINDING_POOL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CLUTTER_TYPE_BINDING_POOL))
#define CLUTTER_BINDING_POOL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CLUTTER_TYPE_BINDING_POOL, ClutterBindingPoolClass))

#define BINDING_MOD_MASK        ((CLUTTER_SHIFT_MASK   | \
                                  CLUTTER_CONTROL_MASK | \
                                  CLUTTER_MOD1_MASK    | \
                                  CLUTTER_SUPER_MASK   | \
                                  CLUTTER_HYPER_MASK   | \
                                  CLUTTER_META_MASK)   | CLUTTER_RELEASE_MASK)

typedef struct _ClutterBindingPoolClass ClutterBindingPoolClass;
typedef struct _ClutterBindingEntry     ClutterBindingEntry;

static GSList *clutter_binding_pools = NULL;
static GQuark  key_class_bindings = 0;

struct _ClutterBindingPool
{
  GObject parent_instance;

  gchar *name; /* interned string, do not free */

  GSList *entries;
  GHashTable *entries_hash;
};

struct _ClutterBindingPoolClass
{
  GObjectClass parent_class;
};

struct _ClutterBindingEntry
{
  gchar *name; /* interned string, do not free */

  guint key_val;
  ClutterModifierType modifiers;

  GClosure *closure;

  guint is_blocked  : 1;
};

enum
{
  PROP_0,

  PROP_NAME
};

G_DEFINE_TYPE (ClutterBindingPool, clutter_binding_pool, G_TYPE_OBJECT);

static guint
binding_entry_hash (gconstpointer v)
{
  const ClutterBindingEntry *e = v;
  guint h;

  h = e->key_val;
  h ^= e->modifiers;

  return h;
}

static gint
binding_entry_compare (gconstpointer v1,
                       gconstpointer v2)
{
  const ClutterBindingEntry *e1 = v1;
  const ClutterBindingEntry *e2 = v2;

  return (e1->key_val == e2->key_val && e1->modifiers == e2->modifiers);
}

static ClutterBindingEntry *
binding_entry_new (const gchar         *name,
                   guint                key_val,
                   ClutterModifierType  modifiers)
{
  ClutterBindingEntry *entry;

  modifiers = modifiers & BINDING_MOD_MASK;

  entry = g_slice_new (ClutterBindingEntry);
  entry->key_val = key_val;
  entry->modifiers = modifiers;
  entry->name = (gchar *) g_intern_string (name);
  entry->closure = NULL;
  entry->is_blocked = FALSE;

  return entry;
}

static ClutterBindingEntry *
binding_pool_lookup_entry (ClutterBindingPool  *pool,
                           guint                key_val,
                           ClutterModifierType  modifiers)
{
  ClutterBindingEntry lookup_entry = { 0, };

  lookup_entry.key_val = key_val;
  lookup_entry.modifiers = modifiers;

  return g_hash_table_lookup (pool->entries_hash, &lookup_entry);
}

static void
binding_entry_free (gpointer data)
{
  if (G_LIKELY (data))
    {
      ClutterBindingEntry *entry = data;

      g_closure_unref (entry->closure);

      g_slice_free (ClutterBindingEntry, entry);
    }
}

static void
clutter_binding_pool_finalize (GObject *gobject)
{
  ClutterBindingPool *pool = CLUTTER_BINDING_POOL (gobject);

  /* remove from the pools */
  clutter_binding_pools = g_slist_remove (clutter_binding_pools, pool);

  g_hash_table_destroy (pool->entries_hash);

  g_slist_foreach (pool->entries, (GFunc) binding_entry_free, NULL);
  g_slist_free (pool->entries);

  G_OBJECT_CLASS (clutter_binding_pool_parent_class)->finalize (gobject);
}

static void
clutter_binding_pool_set_property (GObject      *gobject,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ClutterBindingPool *pool = CLUTTER_BINDING_POOL (gobject);

  switch (prop_id)
    {
    case PROP_NAME:
      pool->name = (gchar *) g_intern_string (g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_binding_pool_get_property (GObject    *gobject,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ClutterBindingPool *pool = CLUTTER_BINDING_POOL (gobject);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, pool->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_binding_pool_constructed (GObject *gobject)
{
  ClutterBindingPool *pool = CLUTTER_BINDING_POOL (gobject);

  /* bad monkey! bad, bad monkey! */
  if (G_UNLIKELY (pool->name == NULL))
    g_critical ("No name set for ClutterBindingPool %p", pool);

  if (G_OBJECT_CLASS (clutter_binding_pool_parent_class)->constructed)
    G_OBJECT_CLASS (clutter_binding_pool_parent_class)->constructed (gobject);
}

static void
clutter_binding_pool_class_init (ClutterBindingPoolClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec = NULL;

  gobject_class->constructed = clutter_binding_pool_constructed;
  gobject_class->set_property = clutter_binding_pool_set_property;
  gobject_class->get_property = clutter_binding_pool_get_property;
  gobject_class->finalize = clutter_binding_pool_finalize;

  /**
   * ClutterBindingPool:name:
   *
   * The unique name of the #ClutterBindingPool.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_string ("name",
                               "Name",
                               "The unique name of the binding pool",
                               NULL,
                               CLUTTER_PARAM_READWRITE |
                               G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_NAME, pspec);
}

static void
clutter_binding_pool_init (ClutterBindingPool *pool)
{
  pool->name = NULL;
  pool->entries = NULL;
  pool->entries_hash = g_hash_table_new (binding_entry_hash,
                                         binding_entry_compare);

  clutter_binding_pools = g_slist_prepend (clutter_binding_pools, pool);
}

/**
 * clutter_binding_pool_new:
 * @name: the name of the binding pool
 *
 * Creates a new #ClutterBindingPool that can be used to store
 * key bindings for an actor. The @name must be a unique identifier
 * for the binding pool, so that clutter_binding_pool_find() will
 * be able to return the correct binding pool.
 *
 * Return value: the newly created binding pool with the given
 *   name. Use g_object_unref() when done.
 *
 * Since: 1.0
 */
ClutterBindingPool *
clutter_binding_pool_new (const gchar *name)
{
  ClutterBindingPool *pool;

  g_return_val_if_fail (name != NULL, NULL);

  pool = clutter_binding_pool_find (name);
  if (G_UNLIKELY (pool))
    {
      g_warning ("A binding pool named '%s' is already present "
                 "in the binding pools list",
                 pool->name);
      return NULL;
    }

  return g_object_new (CLUTTER_TYPE_BINDING_POOL, "name", name, NULL);
}

/**
 * clutter_binding_pool_get_for_class:
 * @klass: a #GObjectClass pointer
 *
 * Retrieves the #ClutterBindingPool for the given #GObject class
 * and, eventually, creates it. This function is a wrapper around
 * clutter_binding_pool_new() and uses the class type name as the
 * unique name for the binding pool.
 *
 * Calling this function multiple times will return the same
 * #ClutterBindingPool.
 *
 * A binding pool for a class can also be retrieved using
 * clutter_binding_pool_find() with the class type name:
 *
 * |[
 *   pool = clutter_binding_pool_find (G_OBJECT_TYPE_NAME (instance));
 * ]|
 *
 * Return value: (transfer none): the binding pool for the given class.
 *   The returned #ClutterBindingPool is owned by Clutter and should not
 *   be freed directly
 *
 * Since: 1.0
 */
ClutterBindingPool *
clutter_binding_pool_get_for_class (gpointer klass)
{
  ClutterBindingPool *pool;

  g_return_val_if_fail (G_IS_OBJECT_CLASS (klass), NULL);

  if (G_UNLIKELY (key_class_bindings == 0))
    key_class_bindings = g_quark_from_static_string ("clutter-bindings-set");

  pool = g_dataset_id_get_data (klass, key_class_bindings);
  if (pool)
    return pool;

  pool = clutter_binding_pool_new (G_OBJECT_CLASS_NAME (klass));
  g_dataset_id_set_data_full (klass, key_class_bindings,
                              pool,
                              g_object_unref);

  return pool;
}

/**
 * clutter_binding_pool_find:
 * @name: the name of the binding pool to find
 *
 * Finds the #ClutterBindingPool with @name.
 *
 * Return value: (transfer none): a pointer to the #ClutterBindingPool, or %NULL
 *
 * Since: 1.0
 */
ClutterBindingPool *
clutter_binding_pool_find (const gchar *name)
{
  GSList *l;

  g_return_val_if_fail (name != NULL, NULL);

  for (l = clutter_binding_pools; l != NULL; l = l->next)
    {
      ClutterBindingPool *pool = l->data;

      if (g_str_equal (pool->name, (gpointer) name))
        return pool;
    }

  return NULL;
}

/**
 * clutter_binding_pool_install_action:
 * @pool: a #ClutterBindingPool
 * @action_name: the name of the action
 * @key_val: key symbol
 * @modifiers: bitmask of modifiers
 * @callback: function to be called when the action is activated
 * @data: data to be passed to @callback
 * @notify: function to be called when the action is removed
 *   from the pool
 *
 * Installs a new action inside a #ClutterBindingPool. The action
 * is bound to @key_val and @modifiers.
 *
 * The same action name can be used for multiple @key_val, @modifiers
 * pairs.
 *
 * When an action has been activated using clutter_binding_pool_activate()
 * the passed @callback will be invoked (with @data).
 *
 * Actions can be blocked with clutter_binding_pool_block_action()
 * and then unblocked using clutter_binding_pool_unblock_action().
 *
 * Since: 1.0
 */
void
clutter_binding_pool_install_action (ClutterBindingPool  *pool,
                                     const gchar         *action_name,
                                     guint                key_val,
                                     ClutterModifierType  modifiers,
                                     GCallback            callback,
                                     gpointer             data,
                                     GDestroyNotify       notify)
{
  ClutterBindingEntry *entry;
  GClosure *closure;

  g_return_if_fail (pool != NULL);
  g_return_if_fail (action_name != NULL);
  g_return_if_fail (key_val != 0);
  g_return_if_fail (callback != NULL);

  entry = binding_pool_lookup_entry (pool, key_val, modifiers);
  if (G_UNLIKELY (entry))
    {
      g_warning ("There already is an action '%s' for the given "
                 "key symbol of %d (modifiers: %d) installed inside "
                 "the binding pool.",
                 entry->name,
                 entry->key_val, entry->modifiers);
      return;
    }
  else
    entry = binding_entry_new (action_name, key_val, modifiers);

  closure = g_cclosure_new (callback, data, (GClosureNotify) notify);
  entry->closure = g_closure_ref (closure);
  g_closure_sink (closure);

  if (G_CLOSURE_NEEDS_MARSHAL (closure))
    {
      GClosureMarshal marshal;

      marshal = clutter_marshal_BOOLEAN__STRING_UINT_ENUM;
      g_closure_set_marshal (closure, marshal);
    }

  pool->entries = g_slist_prepend (pool->entries, entry);
  g_hash_table_insert (pool->entries_hash, entry, entry);
}

/**
 * clutter_binding_pool_install_closure:
 * @pool: a #ClutterBindingPool
 * @action_name: the name of the action
 * @key_val: key symbol
 * @modifiers: bitmask of modifiers
 * @closure: a #GClosure
 *
 * A #GClosure variant of clutter_binding_pool_install_action().
 *
 * Installs a new action inside a #ClutterBindingPool. The action
 * is bound to @key_val and @modifiers.
 *
 * The same action name can be used for multiple @key_val, @modifiers
 * pairs.
 *
 * When an action has been activated using clutter_binding_pool_activate()
 * the passed @closure will be invoked.
 *
 * Actions can be blocked with clutter_binding_pool_block_action()
 * and then unblocked using clutter_binding_pool_unblock_action().
 *
 * Since: 1.0
 */
void
clutter_binding_pool_install_closure (ClutterBindingPool  *pool,
                                      const gchar         *action_name,
                                      guint                key_val,
                                      ClutterModifierType  modifiers,
                                      GClosure            *closure)
{
  ClutterBindingEntry *entry;

  g_return_if_fail (pool != NULL);
  g_return_if_fail (action_name != NULL);
  g_return_if_fail (key_val != 0);
  g_return_if_fail (closure != NULL);

  entry = binding_pool_lookup_entry (pool, key_val, modifiers);
  if (G_UNLIKELY (entry))
    {
      g_warning ("There already is an action '%s' for the given "
                 "key symbol of %d (modifiers: %d) installed inside "
                 "the binding pool.",
                 entry->name,
                 entry->key_val, entry->modifiers);
      return;
    }
  else
    entry = binding_entry_new (action_name, key_val, modifiers);

  entry->closure = g_closure_ref (closure);
  g_closure_sink (closure);

  if (G_CLOSURE_NEEDS_MARSHAL (closure))
    {
      GClosureMarshal marshal;

      marshal = clutter_marshal_BOOLEAN__STRING_UINT_ENUM;
      g_closure_set_marshal (closure, marshal);
    }

  pool->entries = g_slist_prepend (pool->entries, entry);
  g_hash_table_insert (pool->entries_hash, entry, entry);
}

/**
 * clutter_binding_pool_override_action:
 * @pool: a #ClutterBindingPool
 * @key_val: key symbol
 * @modifiers: bitmask of modifiers
 * @callback: function to be called when the action is activated
 * @data: data to be passed to @callback
 * @notify: function to be called when the action is removed
 *   from the pool
 *
 * Allows overriding the action for @key_val and @modifiers inside a
 * #ClutterBindingPool. See clutter_binding_pool_install_action().
 *
 * When an action has been activated using clutter_binding_pool_activate()
 * the passed @callback will be invoked (with @data).
 *
 * Actions can be blocked with clutter_binding_pool_block_action()
 * and then unblocked using clutter_binding_pool_unblock_action().
 *
 * Since: 1.0
 */
void
clutter_binding_pool_override_action (ClutterBindingPool  *pool,
                                      guint                key_val,
                                      ClutterModifierType  modifiers,
                                      GCallback            callback,
                                      gpointer             data,
                                      GDestroyNotify       notify)
{
  ClutterBindingEntry *entry;
  GClosure *closure;

  g_return_if_fail (pool != NULL);
  g_return_if_fail (key_val != 0);
  g_return_if_fail (callback != NULL);

  entry = binding_pool_lookup_entry (pool, key_val, modifiers);
  if (G_UNLIKELY (entry == NULL))
    {
      g_warning ("There is no action for the given key symbol "
                 "of %d (modifiers: %d) installed inside the "
                 "binding pool.",
                 key_val, modifiers);
      return;
    }

  if (entry->closure)
    {
      g_closure_unref (entry->closure);
      entry->closure = NULL;
    }

  closure = g_cclosure_new (callback, data, (GClosureNotify) notify);
  entry->closure = g_closure_ref (closure);
  g_closure_sink (closure);

  if (G_CLOSURE_NEEDS_MARSHAL (closure))
    {
      GClosureMarshal marshal;

      marshal = clutter_marshal_BOOLEAN__STRING_UINT_ENUM;
      g_closure_set_marshal (closure, marshal);
    }
}

/**
 * clutter_binding_pool_override_closure:
 * @pool: a #ClutterBindingPool
 * @key_val: key symbol
 * @modifiers: bitmask of modifiers
 * @closure: a #GClosure
 *
 * A #GClosure variant of clutter_binding_pool_override_action().
 *
 * Allows overriding the action for @key_val and @modifiers inside a
 * #ClutterBindingPool. See clutter_binding_pool_install_closure().
 *
 * When an action has been activated using clutter_binding_pool_activate()
 * the passed @callback will be invoked (with @data).
 *
 * Actions can be blocked with clutter_binding_pool_block_action()
 * and then unblocked using clutter_binding_pool_unblock_action().
 *
 * Since: 1.0
 */
void
clutter_binding_pool_override_closure (ClutterBindingPool  *pool,
                                       guint                key_val,
                                       ClutterModifierType  modifiers,
                                       GClosure            *closure)
{
  ClutterBindingEntry *entry;

  g_return_if_fail (pool != NULL);
  g_return_if_fail (key_val != 0);
  g_return_if_fail (closure != NULL);

  entry = binding_pool_lookup_entry (pool, key_val, modifiers);
  if (G_UNLIKELY (entry == NULL))
    {
      g_warning ("There is no action for the given key symbol "
                 "of %d (modifiers: %d) installed inside the "
                 "binding pool.",
                 key_val, modifiers);
      return;
    }

  if (entry->closure)
    {
      g_closure_unref (entry->closure);
      entry->closure = NULL;
    }

  entry->closure = g_closure_ref (closure);
  g_closure_sink (closure);

  if (G_CLOSURE_NEEDS_MARSHAL (closure))
    {
      GClosureMarshal marshal;

      marshal = clutter_marshal_BOOLEAN__STRING_UINT_ENUM;
      g_closure_set_marshal (closure, marshal);
    }
}

/**
 * clutter_binding_pool_find_action:
 * @pool: a #ClutterBindingPool
 * @key_val: a key symbol
 * @modifiers: a bitmask for the modifiers
 *
 * Retrieves the name of the action matching the given key symbol
 * and modifiers bitmask.
 *
 * Return value: the name of the action, if found, or %NULL. The
 *   returned string is owned by the binding pool and should never
 *   be modified or freed
 *
 * Since: 1.0
 */
G_CONST_RETURN gchar *
clutter_binding_pool_find_action (ClutterBindingPool  *pool,
                                  guint                key_val,
                                  ClutterModifierType  modifiers)
{
  ClutterBindingEntry *entry;

  g_return_val_if_fail (pool != NULL, NULL);
  g_return_val_if_fail (key_val != 0, NULL);

  entry = binding_pool_lookup_entry (pool, key_val, modifiers);
  if (!entry)
    return NULL;

  return entry->name;
}

/**
 * clutter_binding_pool_remove_action:
 * @pool: a #ClutterBindingPool
 * @key_val: a key symbol
 * @modifiers: a bitmask for the modifiers
 *
 * Removes the action matching the given @key_val, @modifiers pair,
 * if any exists.
 *
 * Since: 1.0
 */
void
clutter_binding_pool_remove_action (ClutterBindingPool  *pool,
                                    guint                key_val,
                                    ClutterModifierType  modifiers)
{
  ClutterBindingEntry remove_entry = { 0, };
  GSList *l;

  g_return_if_fail (pool != NULL);
  g_return_if_fail (key_val != 0);

  modifiers = modifiers & BINDING_MOD_MASK;

  remove_entry.key_val = key_val;
  remove_entry.modifiers = modifiers;

  for (l = pool->entries; l != NULL; l = l->data)
    {
      ClutterBindingEntry *e = l->data;

      if (e->key_val == remove_entry.key_val &&
          e->modifiers == remove_entry.modifiers)
        {
          pool->entries = g_slist_remove_link (pool->entries, l);
          break;
        }
    }

  g_hash_table_remove (pool->entries_hash, &remove_entry);
}

static gboolean
clutter_binding_entry_invoke (ClutterBindingEntry *entry,
                              GObject             *gobject)
{
  GValue params[4] = { { 0, }, { 0, }, { 0, }, { 0, } };
  GValue result = { 0, };
  gboolean retval = TRUE;

  g_value_init (&params[0], G_TYPE_OBJECT);
  g_value_set_object (&params[0], gobject);

  g_value_init (&params[1], G_TYPE_STRING);
  g_value_set_string (&params[1], entry->name);

  g_value_init (&params[2], G_TYPE_UINT);
  g_value_set_uint (&params[2], entry->key_val);

  g_value_init (&params[3], CLUTTER_TYPE_MODIFIER_TYPE);
  g_value_set_flags (&params[3], entry->modifiers);

  g_value_init (&result, G_TYPE_BOOLEAN);

  g_closure_invoke (entry->closure, &result, 4, params, NULL);

  retval = g_value_get_boolean (&result);

  g_value_unset (&result);

  g_value_unset (&params[0]);
  g_value_unset (&params[1]);
  g_value_unset (&params[2]);
  g_value_unset (&params[3]);

  return retval;
}

/**
 * clutter_binding_pool_activate:
 * @pool: a #ClutterBindingPool
 * @key_val: the key symbol
 * @modifiers: bitmask for the modifiers
 * @gobject: a #GObject
 *
 * Activates the callback associated to the action that is
 * bound to the @key_val and @modifiers pair.
 *
 * The callback has the following signature:
 *
 * |[
 *   void (* callback) (GObject             *gobject,
 *                      const gchar         *action_name,
 *                      guint                key_val,
 *                      ClutterModifierType  modifiers,
 *                      gpointer             user_data);
 * ]|
 *
 * Where the #GObject instance is @gobject and the user data
 * is the one passed when installing the action with
 * clutter_binding_pool_install_action().
 *
 * If the action bound to the @key_val, @modifiers pair has been
 * blocked using clutter_binding_pool_block_action(), the callback
 * will not be invoked, and this function will return %FALSE.
 *
 * Return value: %TRUE if an action was found and was activated
 *
 * Since: 1.0
 */
gboolean
clutter_binding_pool_activate (ClutterBindingPool  *pool,
                               guint                key_val,
                               ClutterModifierType  modifiers,
                               GObject             *gobject)
{
  ClutterBindingEntry *entry = NULL;

  g_return_val_if_fail (pool != NULL, FALSE);
  g_return_val_if_fail (key_val != 0, FALSE);
  g_return_val_if_fail (G_IS_OBJECT (gobject), FALSE);

  modifiers = (modifiers & BINDING_MOD_MASK);

  entry = binding_pool_lookup_entry (pool, key_val, modifiers);
  if (!entry)
    return FALSE;

  if (!entry->is_blocked)
    return clutter_binding_entry_invoke (entry, gobject);

  return FALSE;
}

/**
 * clutter_binding_pool_block_action:
 * @pool: a #ClutterBindingPool
 * @action_name: an action name
 *
 * Blocks all the actions with name @action_name inside @pool.
 *
 * Since: 1.0
 */
void
clutter_binding_pool_block_action (ClutterBindingPool *pool,
                                   const gchar        *action_name)
{
  GSList *l;

  g_return_if_fail (pool != NULL);
  g_return_if_fail (action_name != NULL);

  for (l = pool->entries; l != NULL; l = l->next)
    {
      ClutterBindingEntry *entry = l->data;

      if (g_str_equal (entry->name, (gpointer) action_name))
        entry->is_blocked = TRUE;
    }
}

/**
 * clutter_binding_pool_unblock_action:
 * @pool: a #ClutterBindingPool
 * @action_name: an action name
 *
 * Unblockes all the actions with name @action_name inside @pool.
 *
 * Unblocking an action does not cause the callback bound to it to
 * be invoked in case clutter_binding_pool_activate() was called on
 * an action previously blocked with clutter_binding_pool_block_action().
 *
 * Since: 1.0
 */
void
clutter_binding_pool_unblock_action (ClutterBindingPool *pool,
                                     const gchar        *action_name)
{
  GSList *l;

  g_return_if_fail (pool != NULL);
  g_return_if_fail (action_name != NULL);

  for (l = pool->entries; l != NULL; l = l->next)
    {
      ClutterBindingEntry *entry = l->data;

      if (g_str_equal (entry->name, (gpointer) action_name))
        entry->is_blocked = FALSE;
    }
}
