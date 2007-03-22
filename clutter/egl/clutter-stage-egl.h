#ifndef __CLUTTER_STAGE_EGL_H__
#define __CLUTTER_STAGE_EGL_H__

#include <clutter/clutter-stage.h>

#define CLUTTER_TYPE_STAGE_EGL                  (clutter_stage_egl_get_type ())
#define CLUTTER_STAGE_EGL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_EGL, ClutterStageEgl))
#define CLUTTER_IS_STAGE_EGL(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_EGL))
#define CLUTTER_STAGE_EGL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_STAGE_EGL, ClutterStageEglClass))
#define CLUTTER_IS_STAGE_EGL_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_STAGE_EGL))
#define CLUTTER_STAGE_EGL_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_STAGE_EGL, ClutterStageEglClass))

typedef struct _ClutterStageEgl         ClutterStageEgl;
typedef struct _ClutterStageEglClass    ClutterStageEglClass;

struct _ClutterStageEgl
{
  ClutterStage parent_instance;
};

struct _ClutterStageEglClass
{
  ClutterStageClass parent_class;
};

GType clutter_stage_egl_get_type (void) G_GNUC_CONST;

#endif /* __CLUTTER_STAGE_EGL_H__ */
