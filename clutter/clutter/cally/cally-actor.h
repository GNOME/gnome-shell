/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2008 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 *
 * Some parts are based on GailWidget from GAIL
 * GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#ifndef __CALLY_ACTOR_H__
#define __CALLY_ACTOR_H__

#if !defined(__CALLY_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cally/cally.h> can be included directly."
#endif

#include <atk/atk.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define CALLY_TYPE_ACTOR            (cally_actor_get_type ())
#define CALLY_ACTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CALLY_TYPE_ACTOR, CallyActor))
#define CALLY_ACTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CALLY_TYPE_ACTOR, CallyActorClass))
#define CALLY_IS_ACTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CALLY_TYPE_ACTOR))
#define CALLY_IS_ACTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CALLY_TYPE_ACTOR))
#define CALLY_ACTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CALLY_TYPE_ACTOR, CallyActorClass))

typedef struct _CallyActor           CallyActor;
typedef struct _CallyActorClass      CallyActorClass;
typedef struct _CallyActorPrivate    CallyActorPrivate;

/**
 * CallyActionFunc:
 * @cally_actor: a #CallyActor
 *
 * Action function, to be used on #AtkAction implementations as a individual
 * action
 *
 * Since: 1.4
 */
typedef void (* CallyActionFunc) (CallyActor *cally_actor);

/**
 * CallyActionCallback:
 * @cally_actor: a #CallyActor
 * @user_data: user data passed to the function
 *
 * Action function, to be used on #AtkAction implementations as
 * an individual action. Unlike #CallyActionFunc, this function
 * uses the @user_data argument passed to cally_actor_add_action_full().
 *
 * Since: 1.6
 */
typedef void (* CallyActionCallback) (CallyActor *cally_actor,
                                      gpointer    user_data);

/**
 * CallyActor:
 *
 * The <structname>CallyActor</structname> structure contains only private
 * data and should be accessed using the provided API
 *
 * Since: 1.4
 */
struct _CallyActor
{
  /*< private >*/
  AtkGObjectAccessible parent;

  CallyActorPrivate *priv;
};

/**
 * CallyActorClass:
 * @notify_clutter: Signal handler for notify signal on Clutter actor
 * @focus_clutter: Signal handler for key-focus-in and key-focus-out
 *   signal on Clutter actor. This virtual functions is deprecated.
 * @add_actor: Signal handler for actor-added signal on
 *   ClutterContainer interface
 * @remove_actor: Signal handler for actor-added signal on
 *   ClutterContainer interface
 *
 * The <structname>CallyActorClass</structname> structure contains
 * only private data
 *
 * Since: 1.4
 */
struct _CallyActorClass
{
  /*< private >*/
  AtkGObjectAccessibleClass parent_class;

  /*< public >*/
  void     (*notify_clutter) (GObject    *object,
                              GParamSpec *pspec);

  gboolean (*focus_clutter)  (ClutterActor *actor,
                              gpointer      data);

  gint     (*add_actor)      (ClutterActor *container,
                              ClutterActor *actor,
                              gpointer      data);

  gint     (*remove_actor)   (ClutterActor *container,
                              ClutterActor *actor,
                              gpointer      data);

  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[32];
};

CLUTTER_AVAILABLE_IN_1_4
GType      cally_actor_get_type              (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_4
AtkObject* cally_actor_new                   (ClutterActor        *actor);

CLUTTER_AVAILABLE_IN_1_4
guint      cally_actor_add_action            (CallyActor          *cally_actor,
                                              const gchar         *action_name,
                                              const gchar         *action_description,
                                              const gchar         *action_keybinding,
                                              CallyActionFunc      action_func);
CLUTTER_AVAILABLE_IN_1_6
guint      cally_actor_add_action_full       (CallyActor          *cally_actor,
                                              const gchar         *action_name,
                                              const gchar         *action_description,
                                              const gchar         *action_keybinding,
                                              CallyActionCallback  callback,
                                              gpointer             user_data,
                                              GDestroyNotify       notify);

CLUTTER_AVAILABLE_IN_1_4
gboolean   cally_actor_remove_action         (CallyActor          *cally_actor,
                                              gint                 action_id);

CLUTTER_AVAILABLE_IN_1_4
gboolean   cally_actor_remove_action_by_name (CallyActor          *cally_actor,
                                              const gchar         *action_name);

G_END_DECLS

#endif /* __CALLY_ACTOR_H__ */
