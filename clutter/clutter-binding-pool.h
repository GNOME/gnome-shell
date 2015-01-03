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

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_BINDING_POOL_H__
#define __CLUTTER_BINDING_POOL_H__

#include <glib-object.h>
#include <clutter/clutter-event.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BINDING_POOL       (clutter_binding_pool_get_type ())
#define CLUTTER_BINDING_POOL(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BINDING_POOL, ClutterBindingPool))
#define CLUTTER_IS_BINDING_POOL(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BINDING_POOL))

/**
 * ClutterBindingPool:
 *
 * Container of key bindings. The #ClutterBindingPool struct is
 * private.
 *
 * Since: 1.0
 */
typedef struct _ClutterBindingPool      ClutterBindingPool;
typedef struct _ClutterBindingPoolClass ClutterBindingPoolClass;

/**
 * ClutterBindingActionFunc:
 * @gobject: a #GObject
 * @action_name: the name of the action
 * @key_val: the key symbol
 * @modifiers: bitmask of the modifier flags
 * @user_data: data passed to the function
 *
 * The prototype for the callback function registered with
 * clutter_binding_pool_install_action() and invoked by
 * clutter_binding_pool_activate().
 *
 * Return value: the function should return %TRUE if the key
 *   binding has been handled, and return %FALSE otherwise
 *
 * Since: 1.0
 */
typedef gboolean (* ClutterBindingActionFunc) (GObject             *gobject,
                                               const gchar         *action_name,
                                               guint                key_val,
                                               ClutterModifierType  modifiers,
                                               gpointer             user_data);

CLUTTER_AVAILABLE_IN_1_0
GType clutter_binding_pool_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_0
ClutterBindingPool *  clutter_binding_pool_new              (const gchar         *name);
CLUTTER_AVAILABLE_IN_1_0
ClutterBindingPool *  clutter_binding_pool_get_for_class    (gpointer             klass);
CLUTTER_AVAILABLE_IN_1_0
ClutterBindingPool *  clutter_binding_pool_find             (const gchar         *name);

CLUTTER_AVAILABLE_IN_1_0
void                  clutter_binding_pool_install_action   (ClutterBindingPool  *pool,
                                                             const gchar         *action_name,
                                                             guint                key_val,
                                                             ClutterModifierType  modifiers,
                                                             GCallback            callback,
                                                             gpointer             data,
                                                             GDestroyNotify       notify);
CLUTTER_AVAILABLE_IN_1_0
void                  clutter_binding_pool_install_closure  (ClutterBindingPool  *pool,
                                                             const gchar         *action_name,
                                                             guint                key_val,
                                                             ClutterModifierType  modifiers,
                                                             GClosure            *closure);
CLUTTER_AVAILABLE_IN_1_0
void                  clutter_binding_pool_override_action  (ClutterBindingPool  *pool,
                                                             guint                key_val,
                                                             ClutterModifierType  modifiers,
                                                             GCallback            callback,
                                                             gpointer             data,
                                                             GDestroyNotify       notify);
CLUTTER_AVAILABLE_IN_1_0
void                  clutter_binding_pool_override_closure (ClutterBindingPool  *pool,
                                                             guint                key_val,
                                                             ClutterModifierType  modifiers,
                                                             GClosure            *closure);

CLUTTER_AVAILABLE_IN_1_0
const gchar *         clutter_binding_pool_find_action      (ClutterBindingPool  *pool,
                                                             guint                key_val,
                                                             ClutterModifierType  modifiers);
CLUTTER_AVAILABLE_IN_1_0
void                  clutter_binding_pool_remove_action    (ClutterBindingPool  *pool,
                                                             guint                key_val,
                                                             ClutterModifierType  modifiers);

CLUTTER_AVAILABLE_IN_1_0
gboolean              clutter_binding_pool_activate         (ClutterBindingPool  *pool,
                                                             guint                key_val,
                                                             ClutterModifierType  modifiers,
                                                             GObject             *gobject);

CLUTTER_AVAILABLE_IN_1_0
void                  clutter_binding_pool_block_action     (ClutterBindingPool  *pool,
                                                             const gchar         *action_name);
CLUTTER_AVAILABLE_IN_1_0
void                  clutter_binding_pool_unblock_action   (ClutterBindingPool  *pool,
                                                             const gchar         *action_name);

G_END_DECLS

#endif /* __CLUTTER_BINDING_POOL_H__ */
