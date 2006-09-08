/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
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

#ifndef __CLUTTER_BEHAVIOUR_H__
#define __CLUTTER_BEHAVIOUR_H__

#include <glib-object.h>
#include <clutter/clutter-actor.h>
#include <clutter/clutter-alpha.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BEHAVIOUR            (clutter_behaviour_get_type ())
#define CLUTTER_BEHAVIOUR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BEHAVIOUR, ClutterBehaviour))
#define CLUTTER_BEHAVIOUR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BEHAVIOUR, ClutterBehaviourClass))
#define CLUTTER_IS_BEHAVIOUR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BEHAVIOUR))
#define CLUTTER_IS_BEHAVIOUR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BEHAVIOUR))
#define CLUTTER_BEHAVIOUR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BEHAVIOUR, ClutterBehaviourClass))

typedef struct _ClutterBehaviour        ClutterBehaviour;
typedef struct _ClutterBehaviourPrivate ClutterBehaviourPrivate;
typedef struct _ClutterBehaviourClass   ClutterBehaviourClass;

typedef gboolean (*ClutterAlphaTransform) (ClutterBehaviour *behaviour,
                                           GParamSpec       *pspec,
                                           const GValue     *src,
                                           GValue           *dest);

struct _ClutterBehaviour
{
  GObject parent_instance;

  /*< private >*/
  ClutterBehaviourPrivate *priv;
};

struct _ClutterBehaviourClass
{
  GObjectClass parent_class;

  /* override this to change the way behaviour properties
   * are dispatched
   */
  void (*dispatch_property_changed) (ClutterBehaviour  *behaviour,
                                     guint              n_pspecs,
                                     GParamSpec       **pspecs);

  /* signals */
  void (*notify_behaviour) (ClutterBehaviour *behaviour,
                            GParamSpec       *pspec);

  /* padding, for future expansion */
  void (*_clutter_behaviour_1) (void);
  void (*_clutter_behaviour_2) (void);
  void (*_clutter_behaviour_3) (void);
  void (*_clutter_behaviour_4) (void);
};

GType         clutter_behaviour_get_type        (void) G_GNUC_CONST;

void          clutter_behaviour_apply           (ClutterBehaviour *behaviour,
                                                 ClutterActor     *actor);
void          clutter_behaviour_remove          (ClutterBehaviour *behaviour,
                                                 ClutterActor     *actor);
void          clutter_behaviour_actors_foreach  (ClutterBehaviour *behaviour,
                                                 GFunc             func,
                                                 gpointer          userdata);
GSList *      clutter_behaviour_get_actors      (ClutterBehaviour *behaviour);
void          clutter_behaviour_set_alpha       (ClutterBehaviour *behaviour,
                                                 ClutterAlpha     *alpha);
ClutterAlpha *clutter_behaviour_get_alpha       (ClutterBehaviour *behaviour);



#define CLUTTER_BEHAVIOUR_WARN_INVALID_TRANSFORM(behaviour,pspec,value) \
G_STMT_START { \
  ClutterBehaviour *_behaviour = (ClutterBehaviour *) behaviour; \
  GParamSpec *_pspec = (GParamSpec *) pspec; \
  GValue *_value = (GValue *) value; \
  g_warning ("%s: behaviours of type `%s' are unable to transform values " \
             "of type `%s' into values of type `%s' for property `%s'", \
             G_STRLOC, \
             g_type_name (G_OBJECT_TYPE (_behaviour)), \
             g_type_name (G_VALUE_TYPE (_value)), \
             g_type_name (G_PARAM_SPEC_TYPE (_pspec)), \
             _pspec->name); \
} G_STMT_END

GParamSpec **clutter_behaviour_class_list_properties (ClutterBehaviourClass *klass,
                                                      guint                 *n_pspecs);
GParamSpec * clutter_behaviour_class_find_property   (ClutterBehaviourClass *klass,
                                                      const gchar           *property_name);
void         clutter_behaviour_class_bind_property   (ClutterBehaviourClass *klass,
                                                      GType                  actor_type,
                                                      const gchar           *actor_property,
                                                      ClutterAlphaTransform  func);


G_END_DECLS

#endif /* __CLUTTER_BEHAVIOUR_H__ */
