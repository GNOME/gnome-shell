#ifndef __CLUTTER_STAGE_COGL_H__
#define __CLUTTER_STAGE_COGL_H__

#include <cairo.h>
#include <clutter/clutter-backend.h>
#include <clutter/clutter-stage.h>

#ifdef COGL_HAS_X11_SUPPORT
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#endif

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
  GObject parent_instance;

 /* the stage wrapper */
  ClutterStage *wrapper;

  /* back pointer to the backend */
  ClutterBackend *backend;

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

  guint dirty_backbuffer     : 1;
};

struct _ClutterStageCoglClass
{
  GObjectClass parent_class;
};

GType _clutter_stage_cogl_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CLUTTER_STAGE_COGL_H__ */
