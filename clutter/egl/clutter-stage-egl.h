#ifndef __CLUTTER_STAGE_EGL_H__
#define __CLUTTER_STAGE_EGL_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <clutter/clutter-stage.h>

#ifdef COGL_HAS_X11_SUPPORT
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "../x11/clutter-stage-x11.h"
#endif

#include "clutter-egl-headers.h"
#include "clutter-backend-egl.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_STAGE_EGL                  (_clutter_stage_egl_get_type ())
#define CLUTTER_STAGE_EGL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_EGL, ClutterStageEGL))
#define CLUTTER_IS_STAGE_EGL(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_EGL))
#define CLUTTER_STAGE_EGL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_STAGE_EGL, ClutterStageEGLClass))
#define CLUTTER_IS_STAGE_EGL_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_STAGE_EGL))
#define CLUTTER_STAGE_EGL_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_STAGE_EGL, ClutterStageEGLClass))

typedef struct _ClutterStageEGL         ClutterStageEGL;
typedef struct _ClutterStageEGLClass    ClutterStageEGLClass;

struct _ClutterStageEGL
{
#ifdef COGL_HAS_X11_SUPPORT

  ClutterStageX11 parent_instance;

#else

  GObject parent_instance;

 /* the stage wrapper */
  ClutterStage      *wrapper;

  /* back pointer to the backend */
  ClutterBackendEGL *backend;

#endif

  CoglOnscreen *onscreen;

  /* We only enable clipped redraws after 2 frames, since we've seen
   * a lot of drivers can struggle to get going and may output some
   * junk frames to start with. */
  unsigned long frame_count;

  gboolean initialized_redraw_clip;
  ClutterGeometry bounding_redraw_clip;
};

struct _ClutterStageEGLClass
{
#ifdef COGL_HAS_X11_SUPPORT
  ClutterStageX11Class parent_class;
#else
  GObjectClass parent_class;
#endif
};

GType _clutter_stage_egl_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_STAGE_EGL_H__ */
