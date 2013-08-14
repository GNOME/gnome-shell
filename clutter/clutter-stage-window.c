#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>

#include "clutter-actor.h"
#include "clutter-stage-window.h"
#include "clutter-private.h"

#define clutter_stage_window_get_type   _clutter_stage_window_get_type

typedef ClutterStageWindowIface ClutterStageWindowInterface;

G_DEFINE_INTERFACE (ClutterStageWindow, clutter_stage_window, G_TYPE_OBJECT);

static void
clutter_stage_window_default_init (ClutterStageWindowInterface *iface)
{
  GParamSpec *pspec;

  pspec = g_param_spec_object ("backend",
                               "Backend",
                               "Back pointer to the Backend instance",
                               CLUTTER_TYPE_BACKEND,
                               G_PARAM_WRITABLE |
                               G_PARAM_CONSTRUCT_ONLY |
                               G_PARAM_STATIC_STRINGS);
  g_object_interface_install_property (iface, pspec);

  pspec = g_param_spec_object ("wrapper",
                               "Wrapper",
                               "Back pointer to the Stage actor",
                               CLUTTER_TYPE_STAGE,
                               G_PARAM_WRITABLE |
                               G_PARAM_CONSTRUCT_ONLY |
                               G_PARAM_STATIC_STRINGS);
  g_object_interface_install_property (iface, pspec);
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
  ClutterStageWindowIface *iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);

  if (iface->set_title)
    iface->set_title (window, title);
}

void
_clutter_stage_window_set_fullscreen (ClutterStageWindow *window,
                                      gboolean            is_fullscreen)
{
  ClutterStageWindowIface *iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);

  if (iface->set_fullscreen)
    iface->set_fullscreen (window, is_fullscreen);
}

void
_clutter_stage_window_set_cursor_visible (ClutterStageWindow *window,
                                          gboolean            is_visible)
{
  ClutterStageWindowIface *iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);

  if (iface->set_cursor_visible)
    iface->set_cursor_visible (window, is_visible);
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
_clutter_stage_window_get_geometry (ClutterStageWindow    *window,
                                    cairo_rectangle_int_t *geometry)
{
  CLUTTER_STAGE_WINDOW_GET_IFACE (window)->get_geometry (window, geometry);
}

void
_clutter_stage_window_schedule_update  (ClutterStageWindow *window,
                                        int                 sync_delay)
{
  ClutterStageWindowIface *iface;

  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (window));

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->schedule_update == NULL)
    {
      g_assert (!clutter_feature_available (CLUTTER_FEATURE_SWAP_EVENTS));
      return;
    }

  iface->schedule_update (window, sync_delay);
}

gint64
_clutter_stage_window_get_update_time (ClutterStageWindow *window)
{
  ClutterStageWindowIface *iface;

  g_return_val_if_fail (CLUTTER_IS_STAGE_WINDOW (window), 0);

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->get_update_time == NULL)
    {
      g_assert (!clutter_feature_available (CLUTTER_FEATURE_SWAP_EVENTS));
      return 0;
    }

  return iface->get_update_time (window);
}

void
_clutter_stage_window_clear_update_time (ClutterStageWindow *window)
{
  ClutterStageWindowIface *iface;

  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (window));

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->clear_update_time == NULL)
    {
      g_assert (!clutter_feature_available (CLUTTER_FEATURE_SWAP_EVENTS));
      return;
    }

  iface->clear_update_time (window);
}

void
_clutter_stage_window_add_redraw_clip (ClutterStageWindow    *window,
                                       cairo_rectangle_int_t *stage_clip)
{
  ClutterStageWindowIface *iface;

  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (window));

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->add_redraw_clip != NULL)
    iface->add_redraw_clip (window, stage_clip);
}

/* Determines if the backend will clip the rendering of the next
 * frame.
 *
 * Note: at the start of each new frame there is an implied clip that
 * clips everything (i.e. nothing would be drawn) so this function
 * will return True at the start of a new frame if the backend
 * supports clipped redraws.
 */
gboolean
_clutter_stage_window_has_redraw_clips (ClutterStageWindow *window)
{
  ClutterStageWindowIface *iface;

  g_return_val_if_fail (CLUTTER_IS_STAGE_WINDOW (window), FALSE);

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->has_redraw_clips != NULL)
    return iface->has_redraw_clips (window);

  return FALSE;
}

/* Determines if the backend will discard any additional redraw clips
 * and instead promote them to a full stage redraw.
 *
 * The ideas is that backend may have some heuristics that cause it to
 * give up tracking redraw clips so this can be used to avoid the cost
 * of calculating a redraw clip when we know it's going to be ignored
 * anyway.
 */
gboolean
_clutter_stage_window_ignoring_redraw_clips (ClutterStageWindow *window)
{
  ClutterStageWindowIface *iface;

  g_return_val_if_fail (CLUTTER_IS_STAGE_WINDOW (window), FALSE);

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->ignoring_redraw_clips != NULL)
    return iface->ignoring_redraw_clips (window);

  return TRUE;
}

gboolean
_clutter_stage_window_get_redraw_clip_bounds (ClutterStageWindow    *window,
                                              cairo_rectangle_int_t *stage_clip)
{
  ClutterStageWindowIface *iface;

  g_return_val_if_fail (CLUTTER_IS_STAGE_WINDOW (window), FALSE);

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->get_redraw_clip_bounds != NULL)
    return iface->get_redraw_clip_bounds (window, stage_clip);

  return FALSE;
}

void
_clutter_stage_window_set_accept_focus (ClutterStageWindow *window,
                                        gboolean            accept_focus)
{
  ClutterStageWindowIface *iface;

  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (window));

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->set_accept_focus)
    iface->set_accept_focus (window, accept_focus);
}

void
_clutter_stage_window_redraw (ClutterStageWindow *window)
{
  ClutterStageWindowIface *iface;

  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (window));

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->redraw)
    iface->redraw (window);
}


void
_clutter_stage_window_get_dirty_pixel (ClutterStageWindow *window,
                                       int *x, int *y)
{
  ClutterStageWindowIface *iface;

  *x = 0;
  *y = 0;

  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (window));

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->get_dirty_pixel)
    iface->get_dirty_pixel (window, x, y);
}

void
_clutter_stage_window_dirty_back_buffer (ClutterStageWindow *window)
{
  ClutterStageWindowIface *iface;

  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (window));

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->dirty_back_buffer)
    iface->dirty_back_buffer (window);
}

/* NB: The presumption shouldn't be that a stage can't be comprised of
 * multiple internal framebuffers, so instead of simply naming this
 * function _clutter_stage_window_get_framebuffer(), the "active"
 * infix is intended to clarify that it gets the framebuffer that is
 * currently in use/being painted.
 */
CoglFramebuffer *
_clutter_stage_window_get_active_framebuffer (ClutterStageWindow *window)
{
  ClutterStageWindowIface *iface;

  g_return_val_if_fail (CLUTTER_IS_STAGE_WINDOW (window), NULL);

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->get_active_framebuffer)
    return iface->get_active_framebuffer (window);
  else
    return NULL;
}

gboolean
_clutter_stage_window_can_clip_redraws (ClutterStageWindow *window)
{
  ClutterStageWindowIface *iface;

  g_return_val_if_fail (CLUTTER_IS_STAGE_WINDOW (window), FALSE);

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->can_clip_redraws != NULL)
    return iface->can_clip_redraws (window);

  return FALSE;
}

void
_clutter_stage_window_set_scale_factor (ClutterStageWindow *window,
                                        int                 factor)
{
  ClutterStageWindowIface *iface;

  g_return_if_fail (CLUTTER_IS_STAGE_WINDOW (window));

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->set_scale_factor != NULL)
    iface->set_scale_factor (window, factor);
}

int
_clutter_stage_window_get_scale_factor (ClutterStageWindow *window)
{
  ClutterStageWindowIface *iface;

  g_return_val_if_fail (CLUTTER_IS_STAGE_WINDOW (window), 1);

  iface = CLUTTER_STAGE_WINDOW_GET_IFACE (window);
  if (iface->get_scale_factor != NULL)
    return iface->get_scale_factor (window);

  return 1;
}
