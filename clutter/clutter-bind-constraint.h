#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_BIND_CONSTRAINT_H__
#define __CLUTTER_BIND_CONSTRAINT_H__

#include <clutter/clutter-constraint.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BIND_CONSTRAINT    (clutter_bind_constraint_get_type ())
#define CLUTTER_BIND_CONSTRAINT(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BIND_CONSTRAINT, ClutterBindConstraint))
#define CLUTTER_IS_BIND_CONSTRAINT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BIND_CONSTRAINT))

typedef struct _ClutterBindConstraint   ClutterBindConstraint;

typedef enum { /*< prefix=CLUTTER_BIND >*/
  CLUTTER_BIND_X_AXIS,
  CLUTTER_BIND_Y_AXIS,
  CLUTTER_BIND_Z_AXIS
} ClutterBindAxis;

GType clutter_bind_constraint_get_type (void) G_GNUC_CONST;

ClutterConstraint *clutter_bind_constraint_new (ClutterActor    *source,
                                                ClutterBindAxis  axis,
                                                gfloat           offset);

G_END_DECLS

#endif /* __CLUTTER_BIND_CONSTRAINT_H__ */
