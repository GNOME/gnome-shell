#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_ALIGN_CONSTRAINT_H__
#define __CLUTTER_ALIGN_CONSTRAINT_H__

#include <clutter/clutter-constraint.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ALIGN_CONSTRAINT           (clutter_align_constraint_get_type ())
#define CLUTTER_ALIGN_CONSTRAINT(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ALIGN_CONSTRAINT, ClutterAlignConstraint))
#define CLUTTER_IS_ALIGN_CONSTRAINT(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ALIGN_CONSTRAINT))

typedef struct _ClutterAlignConstraint  ClutterAlignConstraint;

typedef enum { /*< prefix=CLUTTER_ALIGN >*/
  CLUTTER_ALIGN_X_AXIS,
  CLUTTER_ALIGN_Y_AXIS,
} ClutterAlignAxis;

GType clutter_align_constraint_get_type (void) G_GNUC_CONST;

ClutterConstraint *clutter_align_constraint_new (ClutterActor     *source,
                                                 ClutterAlignAxis  axis,
                                                 gfloat            factor);

G_END_DECLS

#endif /* __CLUTTER_ALIGN_CONSTRAINT_H__ */
