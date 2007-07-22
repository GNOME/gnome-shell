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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _HAVE_CLUTTER_BEHAVIOUR_H
#define _HAVE_CLUTTER_BEHAVIOUR_H

#include <glib-object.h>
#include <clutter/clutter-alpha.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_KNOT       (clutter_knot_get_type ())

typedef struct _ClutterKnot  ClutterKnot;

/**
 * ClutterKnot:
 * @x: X coordinate of the knot
 * @y: Y coordinate of the knot
 *
 * Point in a path behaviour.
 *
 * Since: 0.2
 */
struct _ClutterKnot
{
  gint x;
  gint y;
};

GType        clutter_knot_get_type (void) G_GNUC_CONST;
ClutterKnot *clutter_knot_copy     (const ClutterKnot *knot);
void         clutter_knot_free     (ClutterKnot       *knot);
gboolean     clutter_knot_equal    (const ClutterKnot *knot_a,
                                    const ClutterKnot *knot_b);

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

typedef struct _ClutterBehaviour        ClutterBehaviour;
typedef struct _ClutterBehaviourPrivate ClutterBehaviourPrivate;
typedef struct _ClutterBehaviourClass   ClutterBehaviourClass;

/**
 * ClutterBehaviourFunction:
 * @behaviour: the #ClutterBehaviour
 * @actor: an actor driven by @behaviour
 * @data: optional data passed to the function
 *
 * This function is passed to clutter_behaviour_foreach_actor() and
 * will be called for each actor driven by @behaviour.
 */
typedef void (*ClutterBehaviourForeachFunc) (ClutterBehaviour *behaviour,
                                             ClutterActor     *actor,
                                             gpointer          data);

struct _ClutterBehaviour
{
  /*< private >*/
  GObject parent;
  ClutterBehaviourPrivate *priv;
};

struct _ClutterBehaviourClass
{
  GObjectClass parent_class;

  /* vfunc, not signal */
  void (*alpha_notify) (ClutterBehaviour *behave,
                        guint32           alpha_value);

  void (*applied)  (ClutterBehaviour *behave,
		    ClutterActor     *actor);
  void (*removed)  (ClutterBehaviour *behave,
		    ClutterActor     *actor);

  /* padding, for future expansion */
  void (*_clutter_behaviour1) (void);
  void (*_clutter_behaviour2) (void);
  void (*_clutter_behaviour3) (void);
  void (*_clutter_behaviour4) (void);
  void (*_clutter_behaviour5) (void);
  void (*_clutter_behaviour6) (void);
};

GType clutter_behaviour_get_type (void) G_GNUC_CONST;

void          clutter_behaviour_apply          (ClutterBehaviour            *behave,
                                                ClutterActor                *actor);
void          clutter_behaviour_remove         (ClutterBehaviour            *behave,
                                                ClutterActor                *actor);
void          clutter_behaviour_actors_foreach (ClutterBehaviour            *behave,
                                                ClutterBehaviourForeachFunc  func,
                                                gpointer                     data);
gint          clutter_behaviour_get_n_actors   (ClutterBehaviour            *behave);
ClutterActor *clutter_behaviour_get_nth_actor  (ClutterBehaviour            *behave,
						gint                         index);
GSList *      clutter_behaviour_get_actors     (ClutterBehaviour            *behave);
ClutterAlpha *clutter_behaviour_get_alpha      (ClutterBehaviour            *behave);
void          clutter_behaviour_set_alpha      (ClutterBehaviour            *behave,
                                                ClutterAlpha                *alpha);
gboolean      clutter_behaviour_is_applied     (ClutterBehaviour            *behave,
						ClutterActor                *actor);
void          clutter_behaviour_clear          (ClutterBehaviour            *behave);

G_END_DECLS

#endif
