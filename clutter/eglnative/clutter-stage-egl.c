#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-stage-egl.h"
#include "clutter-egl.h"
#include "clutter-backend-egl.h"

#include "../clutter-main.h"
#include "../clutter-feature.h"
#include "../clutter-color.h"
#include "../clutter-util.h"
#include "../clutter-event.h"
#include "../clutter-enum-types.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-units.h"
#include "../clutter-stage.h"
#include "../clutter-stage-window.h"

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStageEGL,
                         clutter_stage_egl,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static void
clutter_stage_egl_class_init (ClutterStageEGLClass *klass)
{
}

static void
clutter_stage_egl_set_fullscreen (ClutterStageWindow *stage_window,
                                  gboolean            fullscreen)
{
  g_warning ("Stage of type '%s' do not support ClutterStage::set_fullscreen",
             G_OBJECT_TYPE_NAME (stage_window));
}

static void
clutter_stage_egl_set_title (ClutterStageWindow *stage_window,
                             const gchar        *title)
{
  g_warning ("Stage of type '%s' do not support ClutterStage::set_title",
             G_OBJECT_TYPE_NAME (stage_window));
}

static void
clutter_stage_egl_set_cursor_visible (ClutterStageWindow *stage_window,
                                      gboolean            cursor_visible)
{
  g_warning ("Stage of type '%s' do not support ClutterStage::set_cursor_visible",
             G_OBJECT_TYPE_NAME (stage_window));
}

static ClutterActor *
clutter_stage_egl_get_wrapper (ClutterStageWindow *stage_window)
{
  return CLUTTER_ACTOR (CLUTTER_STAGE_EGL (stage_window)->wrapper);
}

static void
clutter_stage_egl_show (ClutterStageWindow *stage_window,
                        gboolean            do_raise)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);

  clutter_actor_map (CLUTTER_ACTOR (stage_egl->wrapper));
}

static void
clutter_stage_egl_hide (ClutterStageWindow *stage_window)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);

  clutter_actor_unmap (CLUTTER_ACTOR (stage_egl->wrapper));
}

static void
clutter_stage_egl_unrealize (ClutterStageWindow *stage_window)
{
}

static gboolean
clutter_stage_egl_realize (ClutterStageWindow *stage_window)
{
  /* the EGL surface is created by the backend */
  return TRUE;
}

static void
clutter_stage_egl_get_geometry (ClutterStageWindow *stage_window,
                                ClutterGeometry    *geometry)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage_window);
  ClutterBackendEGL *backend_egl = stage_egl->backend;

  if (geometry)
    {
      geometry->x = geometry->y = 0;

      geometry->width = backend_egl->surface_width;
      geometry->height = backend_egl->surface_height;
    }
}

static void
clutter_stage_egl_resize (ClutterStageWindow *stage_window,
                          gint                width,
                          gint                height)
{
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->set_fullscreen = clutter_stage_egl_set_fullscreen;
  iface->set_title = clutter_stage_egl_set_title;
  iface->set_cursor_visible = clutter_stage_egl_set_cursor_visible;
  iface->get_wrapper = clutter_stage_egl_get_wrapper;
  iface->realize = clutter_stage_egl_realize;
  iface->unrealize = clutter_stage_egl_unrealize;
  iface->get_geometry = clutter_stage_egl_get_geometry;
  iface->resize = clutter_stage_egl_resize;
  iface->show = clutter_stage_egl_show;
  iface->hide = clutter_stage_egl_hide;
}

static void
clutter_stage_egl_init (ClutterStageEGL *stage)
{
}
