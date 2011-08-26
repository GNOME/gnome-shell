#ifndef __CLUTTER_STAGE_COGL_H__
#define __CLUTTER_STAGE_COGL_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <cairo.h>
#include <clutter/clutter-stage.h>

#ifdef COGL_HAS_X11_SUPPORT
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#endif

#ifdef CLUTTER_WINDOWING_X11
#include "../x11/clutter-stage-x11.h"
#endif
#ifdef CLUTTER_WINDOWING_GDK
#include "../gdk/clutter-stage-gdk.h"
#endif

#include "clutter-backend-cogl.h"

G_BEGIN_DECLS

#define CLUTTER_TYPE_STAGE_COGL                  (_clutter_stage_cogl_get_type ())
#define CLUTTER_STAGE_COGL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_COGL, ClutterStageCogl))
#define CLUTTER_IS_STAGE_COGL(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_COGL))
#define CLUTTER_STAGE_COGL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_STAGE_COGL, ClutterStageCoglClass))
#define CLUTTER_IS_STAGE_COGL_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_STAGE_COGL))
#define CLUTTER_STAGE_COGL_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_STAGE_COGL, ClutterStageCoglClass))

typedef struct _ClutterStageCogl         ClutterStageCogl;
typedef struct _ClutterStageCoglClass    ClutterStageCoglClass;

struct _ClutterStageCogl
{
#ifdef CLUTTER_WINDOWING_X11
  ClutterStageX11 parent_instance;

#elif defined(CLUTTER_WINDOWING_GDK)
  ClutterStageGdk parent_instance;

#else
  GObject parent_instance;

 /* the stage wrapper */
  ClutterStage      *wrapper;

  /* back pointer to the backend */
  ClutterBackendCogl *backend;

#endif

  CoglOnscreen *onscreen;

  gint pending_swaps;
  unsigned int swap_callback_id;

  /* We only enable clipped redraws after 2 frames, since we've seen
   * a lot of drivers can struggle to get going and may output some
   * junk frames to start with. */
  unsigned long frame_count;

  cairo_rectangle_int_t bounding_redraw_clip;

  guint initialized_redraw_clip : 1;

  /* TRUE if the current paint cycle has a clipped redraw. In that
     case bounding_redraw_clip specifies the the bounds. */
  guint using_clipped_redraw : 1;
};

struct _ClutterStageCoglClass
{
#ifdef CLUTTER_WINDOWING_X11
  ClutterStageX11Class parent_class;
#elif defined(CLUTTER_WINDOWING_GDK)
  GObjectClass parent_class;
#endif
};

GType _clutter_stage_cogl_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_STAGE_COGL_H__ */
