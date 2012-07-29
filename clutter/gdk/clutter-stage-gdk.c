/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#include "config.h"

#include <math.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <cogl/cogl.h>
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#endif

#include "clutter-backend-gdk.h"
#include "clutter-stage-gdk.h"
#include "clutter-gdk.h"

#include "clutter-actor-private.h"
#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-enum-types.h"
#include "clutter-event-translator.h"
#include "clutter-event-private.h"
#include "clutter-feature.h"
#include "clutter-main.h"
#include "clutter-paint-volume-private.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

static void clutter_stage_window_iface_init     (ClutterStageWindowIface     *iface);

static ClutterStageWindowIface *clutter_stage_window_parent_iface = NULL;

#define clutter_stage_gdk_get_type      _clutter_stage_gdk_get_type

G_DEFINE_TYPE_WITH_CODE (ClutterStageGdk,
                         clutter_stage_gdk,
                         CLUTTER_TYPE_STAGE_COGL,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

#ifdef CLUTTER_WINDOWING_X11
static void
clutter_stage_gdk_update_foreign_event_mask (CoglOnscreen *onscreen,
					     guint32 event_mask,
					     void *user_data)
{
  ClutterStageGdk *stage_gdk = user_data;

  /* we assume that a GDK event mask is bitwise compatible with X11
     event masks */
  gdk_window_set_events (stage_gdk->window, event_mask | CLUTTER_STAGE_GDK_EVENT_MASK);
}
#endif

static void
clutter_stage_gdk_set_gdk_geometry (ClutterStageGdk *stage)
{
  GdkGeometry geometry;
  ClutterStage *wrapper = CLUTTER_STAGE_COGL (stage)->wrapper;
  gboolean resize = clutter_stage_get_user_resizable (wrapper);

  if (!resize)
    {
      geometry.min_width = geometry.max_width = gdk_window_get_width (stage->window);
      geometry.min_height = geometry.max_height = gdk_window_get_height (stage->window);

      gdk_window_set_geometry_hints (stage->window,
				     &geometry,
				     GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);
    }
  else
    {
      clutter_stage_get_minimum_size (wrapper,
				      (guint *)&geometry.min_width,
				      (guint *)&geometry.min_height);

      gdk_window_set_geometry_hints (stage->window,
				     &geometry,
				     GDK_HINT_MIN_SIZE);
    }
}

static void
clutter_stage_gdk_get_geometry (ClutterStageWindow    *stage_window,
                                cairo_rectangle_int_t *geometry)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);

  if (stage_gdk->window != NULL)
    {
      geometry->width = gdk_window_get_width (stage_gdk->window);
      geometry->height = gdk_window_get_height (stage_gdk->window);
    }
  else
    {
      geometry->width = 640;
      geometry->height = 480;
    }
}

static void
clutter_stage_gdk_resize (ClutterStageWindow *stage_window,
                          gint                width,
                          gint                height)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);

  if (width == 0 || height == 0)
    {
      /* Should not happen, if this turns up we need to debug it and
       * determine the cleanest way to fix.
       */
      g_warning ("GDK stage not allowed to have 0 width or height");
      width = 1;
      height = 1;
    }

  CLUTTER_NOTE (BACKEND, "New size received: (%d, %d)", width, height);

  CLUTTER_SET_PRIVATE_FLAGS (CLUTTER_STAGE_COGL (stage_gdk)->wrapper,
			     CLUTTER_IN_RESIZE);

  gdk_window_resize (stage_gdk->window, width, height);
}

static void
clutter_stage_gdk_unrealize (ClutterStageWindow *stage_window)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);

  if (stage_gdk->window != NULL)
    {
      g_object_set_data (G_OBJECT (stage_gdk->window),
			 "clutter-stage-window", NULL);

      if (stage_gdk->foreign_window)
	g_object_unref (stage_gdk->window);
      else
	gdk_window_destroy (stage_gdk->window);

      stage_gdk->window = NULL;
    }

  return clutter_stage_window_parent_iface->unrealize (stage_window);
}

static gboolean
clutter_stage_gdk_realize (ClutterStageWindow *stage_window)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);
  ClutterBackend *backend = CLUTTER_BACKEND (stage_cogl->backend);
  ClutterBackendGdk *backend_gdk = CLUTTER_BACKEND_GDK (backend);
  GdkWindowAttr attributes;
  gboolean cursor_visible;
  gboolean use_alpha;
  gfloat   width, height;

  if (!stage_gdk->foreign_window)
    {
      if (stage_gdk->window != NULL)
        {
          /* complete realizing the stage */
          cairo_rectangle_int_t geometry;

          clutter_stage_gdk_get_geometry (stage_window, &geometry);
          clutter_actor_set_size (CLUTTER_ACTOR (stage_cogl->wrapper),
                                  geometry.width,
                                  geometry.height);

          gdk_window_ensure_native (stage_gdk->window);
          gdk_window_set_events (stage_gdk->window, CLUTTER_STAGE_GDK_EVENT_MASK);

          return TRUE;
        }
      else
        {
          attributes.title = NULL;
          g_object_get (stage_cogl->wrapper,
                        "cursor-visible", &cursor_visible,
                        "title", &attributes.title,
                        "width", &width,
                        "height", &height,
                        "use-alpha", &use_alpha,
                        NULL);

          attributes.width = width;
          attributes.height = height;
          attributes.wclass = GDK_INPUT_OUTPUT;
          attributes.window_type = GDK_WINDOW_TOPLEVEL;
          attributes.event_mask = CLUTTER_STAGE_GDK_EVENT_MASK;

          attributes.cursor = NULL;
          if (!cursor_visible)
            {
              if (stage_gdk->blank_cursor == NULL)
                stage_gdk->blank_cursor = gdk_cursor_new (GDK_BLANK_CURSOR);

              attributes.cursor = stage_gdk->blank_cursor;
            }

          attributes.visual = NULL;
          if (use_alpha)
            {
              attributes.visual = gdk_screen_get_rgba_visual (backend_gdk->screen);

              if (attributes.visual == NULL)
                clutter_stage_set_use_alpha (stage_cogl->wrapper, FALSE);
            }

          if (attributes.visual == NULL)
            {
             /* This could still be an RGBA visual, although normally it's not */
             attributes.visual = gdk_screen_get_system_visual (backend_gdk->screen);
            }

          stage_gdk->foreign_window = FALSE;
          stage_gdk->window = gdk_window_new (NULL, &attributes,
                                              GDK_WA_TITLE | GDK_WA_CURSOR | GDK_WA_VISUAL);

          g_free (attributes.title);
        }

      clutter_stage_gdk_set_gdk_geometry (stage_gdk);
    }

  gdk_window_ensure_native (stage_gdk->window);

  g_object_set_data (G_OBJECT (stage_gdk->window),
                     "clutter-stage-window", stage_gdk);

  stage_cogl->onscreen = cogl_onscreen_new (backend->cogl_context,
					    width, height);

#if defined(GDK_WINDOWING_X11) && defined(COGL_HAS_XLIB_SUPPORT)
  if (GDK_IS_X11_WINDOW (stage_gdk->window))
    {
      cogl_x11_onscreen_set_foreign_window_xid (stage_cogl->onscreen,
                                                GDK_WINDOW_XID (stage_gdk->window),
                                                clutter_stage_gdk_update_foreign_event_mask,
                                                stage_gdk);
    }
  else
#endif
#if defined(GDK_WINDOWING_WIN32) && defined(COGL_HAS_WIN32_SUPPORT)
  if (GDK_IS_WIN32_WINDOW (stage_gdk->window))
    {
      cogl_win32_onscreen_set_foreign_window (stage_cogl->onscreen,
					      gdk_win32_window_get_handle (stage_gdk->window));
    }
  else
#endif
    {
      g_warning ("Cannot find an appropriate CoglWinsys for a "
		 "GdkWindow of type %s", G_OBJECT_TYPE_NAME (stage_gdk->window));

      cogl_object_unref (stage_cogl->onscreen);
      stage_cogl->onscreen = NULL;

      if (!stage_gdk->foreign_window)
        gdk_window_destroy (stage_gdk->window);

      stage_gdk->window = NULL;

      return FALSE;
    }

  return clutter_stage_window_parent_iface->realize (stage_window);
}

static void
clutter_stage_gdk_set_fullscreen (ClutterStageWindow *stage_window,
                                  gboolean            is_fullscreen)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);
  ClutterStage *stage = CLUTTER_STAGE_COGL (stage_window)->wrapper;

  if (stage == NULL || CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return;

  if (stage_gdk->window == NULL)
    return;

  CLUTTER_NOTE (BACKEND, "%ssetting fullscreen", is_fullscreen ? "" : "un");

  if (is_fullscreen)
    gdk_window_fullscreen (stage_gdk->window);
  else
    gdk_window_unfullscreen (stage_gdk->window);
}

static void
clutter_stage_gdk_set_cursor_visible (ClutterStageWindow *stage_window,
                                      gboolean            cursor_visible)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);

  if (stage_gdk->window == NULL)
    return;

  if (cursor_visible)
    {
      gdk_window_set_cursor (stage_gdk->window, NULL);
    }
  else
    {
      if (stage_gdk->blank_cursor == NULL)
	stage_gdk->blank_cursor = gdk_cursor_new (GDK_BLANK_CURSOR);

      gdk_window_set_cursor (stage_gdk->window, stage_gdk->blank_cursor);
    }
}

static void
clutter_stage_gdk_set_title (ClutterStageWindow *stage_window,
                             const gchar        *title)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);

  if (stage_gdk->window == NULL)
    return;

  gdk_window_set_title (stage_gdk->window, title);
}

static void
clutter_stage_gdk_set_user_resizable (ClutterStageWindow *stage_window,
                                      gboolean            is_resizable)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);
  GdkWMFunction function;

  if (stage_gdk->window == NULL)
    return;

  function = GDK_FUNC_MOVE | GDK_FUNC_MINIMIZE | GDK_FUNC_CLOSE;
  if (is_resizable)
    function |= GDK_FUNC_RESIZE | GDK_FUNC_MAXIMIZE;

  gdk_window_set_functions (stage_gdk->window, function);

  clutter_stage_gdk_set_gdk_geometry (stage_gdk);
}

static void
clutter_stage_gdk_set_accept_focus (ClutterStageWindow *stage_window,
                                    gboolean            accept_focus)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);

  if (stage_gdk->window == NULL)
    return;

  gdk_window_set_accept_focus (stage_gdk->window, accept_focus);
}

static void
clutter_stage_gdk_show (ClutterStageWindow *stage_window,
                        gboolean            do_raise)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);

  g_return_if_fail (stage_gdk->window != NULL);

  clutter_actor_map (CLUTTER_ACTOR (CLUTTER_STAGE_COGL (stage_gdk)->wrapper));

  if (do_raise)
    gdk_window_show (stage_gdk->window);
  else
    gdk_window_show_unraised (stage_gdk->window);
}

static void
clutter_stage_gdk_hide (ClutterStageWindow *stage_window)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);

  g_return_if_fail (stage_gdk->window != NULL);

  clutter_actor_unmap (CLUTTER_ACTOR (CLUTTER_STAGE_COGL (stage_gdk)->wrapper));
  gdk_window_hide (stage_gdk->window);
}

static gboolean
clutter_stage_gdk_can_clip_redraws (ClutterStageWindow *stage_window)
{
  return TRUE;
}

static void
clutter_stage_gdk_dispose (GObject *gobject)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (gobject);

  if (stage_gdk->window != NULL)
    {
      g_object_set_data (G_OBJECT (stage_gdk->window),
			 "clutter-stage-window", NULL);
      if (stage_gdk->foreign_window)
	g_object_unref (stage_gdk->window);
      else
	gdk_window_destroy (stage_gdk->window);
      stage_gdk->window = NULL;
    }

  if (stage_gdk->blank_cursor != NULL)
    {
      g_object_unref (stage_gdk->blank_cursor);
      stage_gdk->blank_cursor = NULL;
    }

  G_OBJECT_CLASS (clutter_stage_gdk_parent_class)->dispose (gobject);
}

static void
clutter_stage_gdk_class_init (ClutterStageGdkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_stage_gdk_dispose;
}

static void
clutter_stage_gdk_init (ClutterStageGdk *stage)
{
  /* nothing to do here */
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  clutter_stage_window_parent_iface = g_type_interface_peek_parent (iface);

  iface->set_title = clutter_stage_gdk_set_title;
  iface->set_fullscreen = clutter_stage_gdk_set_fullscreen;
  iface->set_cursor_visible = clutter_stage_gdk_set_cursor_visible;
  iface->set_user_resizable = clutter_stage_gdk_set_user_resizable;
  iface->set_accept_focus = clutter_stage_gdk_set_accept_focus;
  iface->show = clutter_stage_gdk_show;
  iface->hide = clutter_stage_gdk_hide;
  iface->resize = clutter_stage_gdk_resize;
  iface->get_geometry = clutter_stage_gdk_get_geometry;
  iface->realize = clutter_stage_gdk_realize;
  iface->unrealize = clutter_stage_gdk_unrealize;
  iface->can_clip_redraws = clutter_stage_gdk_can_clip_redraws;
}

/**
 * clutter_gdk_get_stage_window:
 * @stage: a #ClutterStage
 *
 * Gets the stages GdkWindow.
 *
 * Return value: (transfer none): A GdkWindow* for the stage window.
 *
 * Since: 1.10
 */
GdkWindow *
clutter_gdk_get_stage_window (ClutterStage *stage)
{
  ClutterStageWindow *impl;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  impl = _clutter_stage_get_window (stage);
  if (!CLUTTER_IS_STAGE_GDK (impl))
    {
      g_critical ("The Clutter backend is not a GDK backend");
      return NULL;
    }

  return CLUTTER_STAGE_GDK (impl)->window;
}

/**
 * clutter_gdk_get_stage_from_window:
 * @window: a #GtkWindow
 *
 * Gets the stage for a particular X window.  
 *
 * Return value: (transfer none): A #ClutterStage, or% NULL if a stage
 *   does not exist for the window
 *
 * Since: 1.10
 */
ClutterStage *
clutter_gdk_get_stage_from_window (GdkWindow *window)
{
  ClutterStageGdk *stage_gdk = g_object_get_data (G_OBJECT (window), "clutter-stage-window");

  if (stage_gdk != NULL && CLUTTER_IS_STAGE_GDK (stage_gdk))
    return CLUTTER_STAGE_COGL (stage_gdk)->wrapper;

  return NULL;
}

typedef struct 
{
  ClutterStageGdk *stage_gdk;
  GdkWindow *window;
} ForeignWindowClosure;

static void
set_foreign_window_callback (ClutterActor *actor,
                             void         *data)
{
  ForeignWindowClosure *closure = data;
  ClutterStageGdk *stage_gdk = closure->stage_gdk;

  stage_gdk->window = closure->window;
  stage_gdk->foreign_window = TRUE;

  /* calling this with the stage unrealized will unset the stage
   * from the GL context; once the stage is realized the GL context
   * will be set again
   */
  clutter_stage_ensure_current (CLUTTER_STAGE (actor));
}

/**
 * clutter_gdk_set_stage_foreign:
 * @stage: a #ClutterStage
 * @window: an existing #GdkWindow
 *
 * Target the #ClutterStage to use an existing external #GdkWindow
 *
 * Return value: %TRUE if foreign window is valid
 *
 * Since: 1.10
 */
gboolean
clutter_gdk_set_stage_foreign (ClutterStage *stage,
                               GdkWindow    *window)
{
  ForeignWindowClosure closure;
  ClutterStageGdk *stage_gdk;
  ClutterStageWindow *impl;
  ClutterActor *actor;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);
  g_return_val_if_fail (!CLUTTER_ACTOR_IN_DESTRUCTION (stage), FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  impl = _clutter_stage_get_window (stage);
  if (!CLUTTER_IS_STAGE_GDK (impl))
    {
      g_critical ("The Clutter backend is not a GDK backend");
      return FALSE;
    }

  stage_gdk = CLUTTER_STAGE_GDK (impl);

  if (g_object_get_data (G_OBJECT (window), "clutter-stage-window") != NULL)
    {
      g_critical ("The provided GdkWindow is already in use by another ClutterStage");
      return FALSE;
    }

  closure.stage_gdk = stage_gdk;
  closure.window = g_object_ref (window);

  actor = CLUTTER_ACTOR (stage);

  _clutter_actor_rerealize (actor,
                            set_foreign_window_callback,
                            &closure);

  /* Queue a relayout - so the stage will be allocated the new
   * window size.
   *
   * Note also that when the stage gets allocated the new
   * window size that will result in the stage's
   * priv->viewport being changed, which will in turn result
   * in the Cogl viewport changing when _clutter_do_redraw
   * calls _clutter_stage_maybe_setup_viewport().
   */
  clutter_actor_queue_relayout (actor);

  return TRUE;
}
