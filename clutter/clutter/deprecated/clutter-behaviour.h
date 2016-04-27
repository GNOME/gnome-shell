/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen  <jorn@openedhand.com>
 *             Emmanuele Bassi  <ebassi@openedhand.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_BEHAVIOUR_H__
#define __CLUTTER_BEHAVIOUR_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BEHAVIOUR clutter_behaviour_get_type()

#define CLUTTER_BEHAVIOUR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_BEHAVIOUR, ClutterBehaviour))

#define CLUTTER_BEHAVIOUR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_BEHAVIOUR, ClutterBehaviourClass))

#define CLUTTER_IS_BEHAVIOUR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_BEHAVIOUR))

#define CLUTTER_IS_BEHAVIOUR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_BEHAVIOUR))

#define CLUTTER_BEHAVIOUR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_BEHAVIOUR, ClutterBehaviourClass))

typedef struct _ClutterBehaviourPrivate ClutterBehaviourPrivate;
typedef struct _ClutterBehaviourClass   ClutterBehaviourClass;

/**
 * ClutterBehaviourForeachFunc:
 * @behaviour: the #ClutterBehaviour
 * @actor: an actor driven by @behaviour
 * @data: (closure): optional data passed to the function
 *
 * This function is passed to clutter_behaviour_actors_foreach() and
 * will be called for each actor driven by @behaviour.
 *
 * Since: 0.2
 *
 * Deprecated: 1.6
 */
typedef void (*ClutterBehaviourForeachFunc) (ClutterBehaviour *behaviour,
                                             ClutterActor     *actor,
                                             gpointer          data);

/**
 * ClutterBehaviour:
 *
 * #ClutterBehaviour-struct contains only private data and should
 * be accessed with the functions below.
 *
 * Since: 0.2
 *
 * Deprecated: 1.6
 */
struct _ClutterBehaviour
{
  /*< private >*/
  GObject parent;
  ClutterBehaviourPrivate *priv;
};

/**
 * ClutterBehaviourClass:
 * @alpha_notify: virtual function, called each time the #ClutterAlpha
 *   computes a new alpha value; the actors to which the behaviour applies
 *   should be changed in this function. Every subclass of #ClutterBehaviour
 *   must implement this virtual function
 * @applied: signal class handler for the ClutterBehaviour::applied signal
 * @removed: signal class handler for the ClutterBehaviour::removed signal
 *
 * Base class for behaviours.
 *
 * Since: 0.2
 *
 * Deprecated: 1.6
 */
struct _ClutterBehaviourClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  /* vfunc, not signal */
  void (*alpha_notify) (ClutterBehaviour *behave,
                        gdouble           alpha_value);

  /* signals */
  void (*applied)  (ClutterBehaviour *behave,
		    ClutterActor     *actor);
  void (*removed)  (ClutterBehaviour *behave,
		    ClutterActor     *actor);

  /*< private >*/
  /* padding, for future expansion */
  void (*_clutter_behaviour1) (void);
  void (*_clutter_behaviour2) (void);
  void (*_clutter_behaviour3) (void);
  void (*_clutter_behaviour4) (void);
  void (*_clutter_behaviour5) (void);
  void (*_clutter_behaviour6) (void);
};

CLUTTER_DEPRECATED_IN_1_6
GType clutter_behaviour_get_type (void) G_GNUC_CONST;

CLUTTER_DEPRECATED_IN_1_6
void          clutter_behaviour_apply          (ClutterBehaviour            *behave,
                                                ClutterActor                *actor);
CLUTTER_DEPRECATED_IN_1_6
void          clutter_behaviour_remove         (ClutterBehaviour            *behave,
                                                ClutterActor                *actor);
CLUTTER_DEPRECATED_IN_1_6
void          clutter_behaviour_remove_all     (ClutterBehaviour            *behave);
CLUTTER_DEPRECATED_IN_1_6
void          clutter_behaviour_actors_foreach (ClutterBehaviour            *behave,
                                                ClutterBehaviourForeachFunc  func,
                                                gpointer                     data);
CLUTTER_DEPRECATED_IN_1_6
gint          clutter_behaviour_get_n_actors   (ClutterBehaviour            *behave);
CLUTTER_DEPRECATED_IN_1_6
ClutterActor *clutter_behaviour_get_nth_actor  (ClutterBehaviour            *behave,
						gint                         index_);
CLUTTER_DEPRECATED_IN_1_6
GSList *      clutter_behaviour_get_actors     (ClutterBehaviour            *behave);
CLUTTER_DEPRECATED_IN_1_6
ClutterAlpha *clutter_behaviour_get_alpha      (ClutterBehaviour            *behave);
CLUTTER_DEPRECATED_IN_1_6
void          clutter_behaviour_set_alpha      (ClutterBehaviour            *behave,
                                                ClutterAlpha                *alpha);
CLUTTER_DEPRECATED_IN_1_6
gboolean      clutter_behaviour_is_applied     (ClutterBehaviour            *behave,
						ClutterActor                *actor);

G_END_DECLS

#endif /* __CLUTTER_BEHAVIOUR_H__ */
