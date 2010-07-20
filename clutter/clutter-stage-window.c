#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>

#include "clutter-actor.h"
#include "clutter-stage-window.h"
#include "clutter-private.h"

GType
clutter_stage_window_get_type (void)
{
  static GType stage_window_type = 0;

  if (G_UNLIKELY (stage_window_type == 0))
    {
      const GTypeInfo stage_window_info = {
        sizeof (ClutterStageWindowIface),
        NULL,
        NULL,
      };

      stage_window_type =
        g_type_register_static (G_TYPE_INTERFACE, I_("ClutterStageWindow"),
                                &stage_window_info, 0);

      g_type_interface_add_prerequisite (stage_window_type,
                                         G_TYPE_OBJECT);
    }

  return stage_window_type;
}

ClutterActor *
_clutter_stage_window_get_wrapper (ClutterStageWindow *window)
{
  return CLUTTER_STAGE_WINDOW_GET_IFACE (window)->get_wrapper (window);
}

void
_clutter_stage_window_set_title (ClutterStageWindow *window,
                                 const gchar        *title)
{
  CLUTTER_STAGE_WINDOW_GET_IFACE (window)->set_title (window, title);
}

void
_clutter_stage_window_set_fullscreen (ClutterStageWindow *window,
                                      gboolean            is_fullscreen)
{
  CLUTTER_STAGE_WINDOW_GET_IFACE (window)->set_fullscreen (window,
                                                           is_fullscreen);
}

void
_clutter_stage_window_set_cursor_visible (ClutterStageWindow *window,
                                          gboolean            is_visible)
{
  CLUTTER_STAGE_WINDOW_GET_IFACE (window)->set_cursor_visible (window,
                                                               is_visible);
}

void
_clutter_stage_window_set_user_resizable (ClutterStageWindow *window,
                                          gboolean            is_resizable)
{
  CLUTTER_STAGE_WINDOW_GET_IFACE (window)->set_user_resizable (window,
                                                               is_resizable);
}

gboolean
_clutter_stage_window_realize (ClutterStageWindow *window)
{
  return CLUTTER_STAGE_WINDOW_GET_IFACE (window)->realize (window);
}

void
_clutter_stage_window_unrealize (ClutterStageWindow *window)
{
  CLUTTER_STAGE_WINDOW_GET_IFACE (window)->unrealize (window);
}

void
_clutter_stage_window_show (ClutterStageWindow *window,
                            gboolean            do_raise)
{
  CLUTTER_STAGE_WINDOW_GET_IFACE (window)->show (window, do_raise);
}

void
_clutter_stage_window_hide (ClutterStageWindow *window)
{
  CLUTTER_STAGE_WINDOW_GET_IFACE (window)->hide (window);
}

void
_clutter_stage_window_resize (ClutterStageWindow *window,
                              gint                width,
                              gint                height)
{
  CLUTTER_STAGE_WINDOW_GET_IFACE (window)->resize (window, width, height);
}

void
_clutter_stage_window_get_geometry (ClutterStageWindow *window,
                                    ClutterGeometry    *geometry)
{
  CLUTTER_STAGE_WINDOW_GET_IFACE (window)->get_geometry (window, geometry);
}

int
_clutter_stage_window_get_pending_swaps (ClutterStageWindow *window)
{
  ClutterStageWindowIface *iface;

  g_return_val_if_fail (CLUTTER_IS_STAGE_WINDOW (window), 0);

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->get_pending_swaps == NULL)
    {
      g_assert (!clutter_feature_available (CLUTTER_FEATURE_SWAP_EVENTS));
      return 0;
    }

  return iface->get_pending_swaps (window);
}

void
_clutter_stage_window_add_redraw_clip (ClutterStageWindow *window,
                                       ClutterGeometry    *stage_clip)
{
  ClutterStageWindowIface *iface;

  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (window));

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->add_redraw_clip)
    iface->add_redraw_clip (window, stage_clip);
}

gboolean
_clutter_stage_window_has_redraw_clips (ClutterStageWindow *window)
{
  ClutterStageWindowIface *iface;

  g_return_val_if_fail (CLUTTER_IS_STAGE_WINDOW (window), FALSE);

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->has_redraw_clips)
    return iface->has_redraw_clips (window);

  return FALSE;
}

gboolean
_clutter_stage_window_ignoring_redraw_clips (ClutterStageWindow *window)
{
  ClutterStageWindowIface *iface;

  g_return_val_if_fail (CLUTTER_IS_STAGE_WINDOW (window), FALSE);

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->ignoring_redraw_clips)
    return iface->ignoring_redraw_clips (window);

  return TRUE;
}

