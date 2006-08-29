#ifndef _HAVE_CLUTTER_BEHAVIOURS_H
#define _HAVE_CLUTTER_BEHAVIOURS_H

#include <glib-object.h>
#include "clutter-alpha.h"
#include "clutter-behaviour.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_BEHAVIOUR_PATH clutter_behaviour_path_get_type()

#define CLUTTER_BEHAVIOUR_PATH(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_PATH, ClutterBehaviourPath))

#define CLUTTER_BEHAVIOUR_PATH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_BEHAVIOUR_PATH, ClutterBehaviourPathClass))

#define CLUTTER_IS_BEHAVIOUR_PATH(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_PATH))

#define CLUTTER_IS_BEHAVIOUR_PATH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_BEHAVIOUR_PATH))

#define CLUTTER_BEHAVIOUR_PATH_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_PATH, ClutterBehaviourPathClass))

typedef struct _ClutterBehaviourPath       ClutterBehaviourPath;
typedef struct ClutterBehaviourPathPrivate ClutterBehaviourPathPrivate;
typedef struct _ClutterBehaviourPathClass  ClutterBehaviourPathClass;
 
struct _ClutterBehaviourPath
{
  ClutterBehaviour             parent;
  ClutterBehaviourPathPrivate *priv;
};

struct _ClutterBehaviourPathClass
{
  ClutterBehaviourClass   parent_class;
};

GType clutter_behaviour_path_get_type (void);

ClutterBehaviour*
clutter_behaviour_path_new (GObject    *object,
                            const char *property,
			    gint        x1,
			    gint        y1,
			    gint        x2,
			    gint        y2);

/* opacity */

#define CLUTTER_TYPE_BEHAVIOUR_OPACITY clutter_behaviour_opacity_get_type()

#define CLUTTER_BEHAVIOUR_OPACITY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_OPACITY, ClutterBehaviourOpacity))

#define CLUTTER_BEHAVIOUR_OPACITY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_BEHAVIOUR_OPACITY, ClutterBehaviourOpacityClass))

#define CLUTTER_IS_BEHAVIOUR_OPACITY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_OPACITY))

#define CLUTTER_IS_BEHAVIOUR_OPACITY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_BEHAVIOUR_OPACITY))

#define CLUTTER_BEHAVIOUR_OPACITY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_OPACITY, ClutterBehaviourOpacityClass))

typedef struct _ClutterBehaviourOpacity       ClutterBehaviourOpacity;
typedef struct ClutterBehaviourOpacityPrivate ClutterBehaviourOpacityPrivate;
typedef struct _ClutterBehaviourOpacityClass  ClutterBehaviourOpacityClass;
 
struct _ClutterBehaviourOpacity
{
  ClutterBehaviour             parent;
  ClutterBehaviourOpacityPrivate *priv;
};

struct _ClutterBehaviourOpacityClass
{
  ClutterBehaviourClass   parent_class;
};

GType clutter_behaviour_opacity_get_type (void);

ClutterBehaviour*
clutter_behaviour_opacity_new (GObject    *object,
                               const char *property,
			       guint8      opacity_start,
			       guint8      opacity_end);

ClutterBehaviour*
clutter_behaviour_opacity_new_from_alpha (ClutterAlpha *alpha,
			                  guint8        opacity_start,
			                  guint8        opacity_end);

G_END_DECLS

#endif
