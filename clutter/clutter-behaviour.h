#ifndef _HAVE_CLUTTER_BEHAVIOUR_H
#define _HAVE_CLUTTER_BEHAVIOUR_H

#include <glib-object.h>

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

typedef struct _ClutterBehaviour       ClutterBehaviour;
typedef struct ClutterBehaviourPrivate ClutterBehaviourPrivate;
typedef struct _ClutterBehaviourClass  ClutterBehaviourClass;
 
struct _ClutterBehaviour
{
  GObject                 parent;
  ClutterBehaviourPrivate *priv;
};

struct _ClutterBehaviourClass
{
  GObjectClass parent_class;

  void (* property_change) (ClutterBehaviour *behave,
                            GObject          *object,
                            GParamSpec       *param_spec);
};

GType clutter_behaviour_get_type (void);

ClutterBehaviour*
clutter_behaviour_new (GObject    *object,
                       const char *property);

void
clutter_behaviour_apply (ClutterBehaviour *behave, ClutterActor *actor);

void
clutter_behaviour_remove (ClutterBehaviour *behave, ClutterActor *actor);

void
clutter_behaviour_remove_all (ClutterBehaviour *behave);

void
clutter_behaviour_actors_foreach (ClutterBehaviour *behave,
				  GFunc             func,
				  gpointer          userdata);

void
clutter_behaviour_set_object (ClutterBehaviour *behave, 
                              GObject          *object);

GObject*
clutter_behaviour_get_object (ClutterBehaviour *behave);

void
clutter_behaviour_set_property (ClutterBehaviour *behave,
                                const char       *property);

const char*
clutter_behaviour_get_property (ClutterBehaviour *behave);

GParamSpec*
clutter_behaviour_get_param_spec (ClutterBehaviour *behave);

G_END_DECLS

#endif
