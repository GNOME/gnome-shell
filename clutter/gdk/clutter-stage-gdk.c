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

#ifdef COGL_HAS_XLIB_SUPPORT
#include <cogl/cogl-xlib.h>
#endif

#define GDK_DISABLE_DEPRECATION_WARNINGS

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
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

#if defined(GDK_WINDOWING_X11) && defined(COGL_HAS_XLIB_SUPPORT)
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

  geometry->x = geometry->y = 0;

  if (stage_gdk->window != NULL)
    {
      geometry->width = gdk_window_get_width (stage_gdk->window);
      geometry->height = gdk_window_get_height (stage_gdk->window);
    }
  else
    {
      geometry->width = 800;
      geometry->height = 600;
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

  /* No need to resize foreign windows, it should be handled by the
   * embedding framework, but on wayland we might need to resize our
   * own subsurface.
   */
  if (!stage_gdk->foreign_window)
    gdk_window_resize (stage_gdk->window, width, height);
#if defined(GDK_WINDOWING_WAYLAND) && defined(COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
  else if (GDK_IS_WAYLAND_WINDOW (stage_gdk->window))
    {
      int scale = gdk_window_get_scale_factor (stage_gdk->window);
      cogl_wayland_onscreen_resize (CLUTTER_STAGE_COGL (stage_gdk)->onscreen,
                                    width * scale, height * scale, 0, 0);
    }
#endif
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
        {
          ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

          g_object_unref (stage_gdk->window);

          /* Clutter still uses part of the deprecated stateful API of
           * Cogl (in particulart cogl_set_framebuffer). It means Cogl
           * can keep an internal reference to the onscreen object we
           * rendered to. In the case of foreign window, we want to
           * avoid this, as we don't know what's going to happen to
           * that window.
           *
           * The following call sets the current Cogl framebuffer to a
           * dummy 1x1 one if we're unrealizing the current one, so
           * Cogl doesn't keep any reference to the foreign window.
           */
          if (cogl_get_draw_framebuffer () == COGL_FRAMEBUFFER (stage_cogl->onscreen))
            _clutter_backend_reset_cogl_framebuffer (stage_cogl->backend);
        }
      else
	gdk_window_destroy (stage_gdk->window);

      stage_gdk->window = NULL;
    }

  clutter_stage_window_parent_iface->unrealize (stage_window);

#if defined(GDK_WINDOWING_WAYLAND)
  g_clear_pointer (&stage_gdk->subsurface, wl_subsurface_destroy);
  g_clear_pointer (&stage_gdk->clutter_surface, wl_surface_destroy);
#endif
}

#if defined(GDK_WINDOWING_WAYLAND)
static struct wl_surface *
clutter_stage_gdk_wayland_surface (ClutterStageGdk *stage_gdk)
{
  GdkDisplay *display;
  struct wl_compositor *compositor;
  struct wl_surface *parent_surface;
  struct wl_region *input_region;
  gint x, y;

  if (!stage_gdk->foreign_window ||
      gdk_window_get_window_type (stage_gdk->window) != GDK_WINDOW_CHILD)
    return gdk_wayland_window_get_wl_surface (stage_gdk->window);

  if (stage_gdk->clutter_surface)
    return stage_gdk->clutter_surface;

  /* On Wayland if we render to a foreign window, we setup our own
   * surface to not render in the same buffers as the embedding
   * framework.
   */
  display = gdk_display_get_default ();
  compositor = gdk_wayland_display_get_wl_compositor (display);
  stage_gdk->clutter_surface = wl_compositor_create_surface (compositor);

  /* Since we run inside GDK, we can let the embedding framework
   * dispatch the events to Clutter. For that to happen we need to
   * disable input on our surface. */
  input_region = wl_compositor_create_region (compositor);
  wl_region_add (input_region, 0, 0, 0, 0);
  wl_surface_set_input_region (stage_gdk->clutter_surface, input_region);
  wl_region_destroy (input_region);

  wl_surface_set_buffer_scale (stage_gdk->clutter_surface,
                               gdk_window_get_scale_factor (stage_gdk->window));

  parent_surface = gdk_wayland_window_get_wl_surface (gdk_window_get_toplevel (stage_gdk->window));
  stage_gdk->subsurface = wl_subcompositor_get_subsurface (stage_gdk->subcompositor,
                                                           stage_gdk->clutter_surface,
                                                           parent_surface);

  gdk_window_get_origin (stage_gdk->window, &x, &y);
  wl_subsurface_set_position (stage_gdk->subsurface, x, y);
  wl_subsurface_set_desync (stage_gdk->subsurface);

  return stage_gdk->clutter_surface;
}
#endif

void
_clutter_stage_gdk_notify_configure (ClutterStageGdk *stage_gdk,
                                     gint x,
                                     gint y,
                                     gint width,
                                     gint height)
{
  if (x < 0 || y < 0 || width < 1 || height < 1)
    return;

  if (stage_gdk->foreign_window)
    {
      ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_gdk);
      int scale = gdk_window_get_scale_factor (stage_gdk->window);

#if defined(GDK_WINDOWING_WAYLAND) && defined(COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
      if (GDK_IS_WAYLAND_WINDOW (stage_gdk->window) &&
          gdk_window_get_window_type (stage_gdk->window) == GDK_WINDOW_CHILD &&
          stage_gdk->subsurface)
        {
          gint rx, ry;
          gdk_window_get_origin (stage_gdk->window, &rx, &ry);
          wl_subsurface_set_position (stage_gdk->subsurface, rx, ry);

          wl_surface_set_buffer_scale (stage_gdk->clutter_surface, scale);
          cogl_wayland_onscreen_resize (stage_cogl->onscreen,
                                        width * scale, height * scale, 0, 0);
        }
      else
#endif
#if defined(GDK_WINDOWING_X11) && defined(COGL_HAS_XLIB_SUPPORT)
      if (GDK_IS_X11_WINDOW (stage_gdk->window))
        {
          ClutterBackend *backend = CLUTTER_BACKEND (stage_cogl->backend);
          XConfigureEvent xevent = { ConfigureNotify };
          xevent.window = GDK_WINDOW_XID (stage_gdk->window);
          xevent.width = width * scale;
          xevent.height = height * scale;

          /* Ensure cogl knows about the new size immediately, as we will
           * draw before we get the ConfigureNotify response. */
          cogl_xlib_renderer_handle_event (backend->cogl_renderer, (XEvent *)&xevent);
        }
      else
#endif
        {
          /* Currently we only support X11 and Wayland. */
          g_assert_not_reached();
        }
    }
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
  gfloat width, height;
  int scale;

  if (backend->cogl_context == NULL)
    {
      g_warning ("Missing Cogl context: was Clutter correctly initialized?");
      return FALSE;
    }

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
                stage_gdk->blank_cursor = gdk_cursor_new_for_display (backend_gdk->display, GDK_BLANK_CURSOR);

              attributes.cursor = stage_gdk->blank_cursor;
            }

          /* If the ClutterStage:use-alpha is set, but GDK does not have an
           * RGBA visual, then we unset the property on the Stage
           */
          if (use_alpha)
            {
              if (gdk_screen_get_rgba_visual (backend_gdk->screen) == NULL)
                {
                  clutter_stage_set_use_alpha (stage_cogl->wrapper, FALSE);
                  use_alpha = FALSE;
                }
            }

#if defined(GDK_WINDOWING_X11) && defined(COGL_HAS_XLIB_SUPPORT)
          if (GDK_IS_X11_DISPLAY (backend_gdk->display))
            {
              XVisualInfo *xvisinfo = cogl_clutter_winsys_xlib_get_visual_info ();
              if (xvisinfo != NULL)
                {
                  attributes.visual = gdk_x11_screen_lookup_visual (backend_gdk->screen,
                                                                    xvisinfo->visualid);
                }
            }
          else
#endif
            {
              attributes.visual = use_alpha
                                ? gdk_screen_get_rgba_visual (backend_gdk->screen)
                                : gdk_screen_get_system_visual (backend_gdk->screen);
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
      gdk_window_ensure_native (stage_gdk->window);
    }
  else
    {
      width = gdk_window_get_width (stage_gdk->window);
      height = gdk_window_get_height (stage_gdk->window);
    }

  g_object_set_data (G_OBJECT (stage_gdk->window), "clutter-stage-window", stage_gdk);

  scale = gdk_window_get_scale_factor (stage_gdk->window);
  stage_cogl->onscreen = cogl_onscreen_new (backend->cogl_context,
                                            width * scale, height * scale);

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
#if defined(GDK_WINDOWING_WAYLAND) && defined(COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
  if (GDK_IS_WAYLAND_WINDOW (stage_gdk->window))
    {
      cogl_wayland_onscreen_set_foreign_surface (stage_cogl->onscreen,
                                                 clutter_stage_gdk_wayland_surface (stage_gdk));
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
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);
  ClutterStage *stage = CLUTTER_STAGE_COGL (stage_window)->wrapper;
  gboolean swap_throttle;

  if (stage == NULL || CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return;

  if (stage_gdk->window == NULL || stage_gdk->foreign_window)
    return;

  CLUTTER_NOTE (BACKEND, "%ssetting fullscreen", is_fullscreen ? "" : "un");

  if (is_fullscreen)
    gdk_window_fullscreen (stage_gdk->window);
  else
    gdk_window_unfullscreen (stage_gdk->window);

  /* Full-screen stages are usually unredirected to improve performance
   * by avoiding a copy; when that happens, we need to turn back swap
   * throttling because we won't be managed by the compositor any more,
   */
  swap_throttle = is_fullscreen;

#ifdef GDK_WINDOWING_WAYLAND
  {
    /* Except on Wayland, where there's a deadlock due to both Cogl
     * and GDK attempting to consume the throttling event; see bug
     * https://bugzilla.gnome.org/show_bug.cgi?id=754671#c1
     */
    GdkDisplay *display = clutter_gdk_get_default_display ();
    if (GDK_IS_WAYLAND_DISPLAY (display))
      swap_throttle = FALSE;
  }
#endif

  cogl_onscreen_set_swap_throttled (stage_cogl->onscreen, swap_throttle);
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
        {
          GdkDisplay *display = clutter_gdk_get_default_display ();

	  stage_gdk->blank_cursor = gdk_cursor_new_for_display (display, GDK_BLANK_CURSOR);
        }

      gdk_window_set_cursor (stage_gdk->window, stage_gdk->blank_cursor);
    }
}

static void
clutter_stage_gdk_set_title (ClutterStageWindow *stage_window,
                             const gchar        *title)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);

  if (stage_gdk->window == NULL || stage_gdk->foreign_window)
    return;

  gdk_window_set_title (stage_gdk->window, title);
}

static void
clutter_stage_gdk_set_user_resizable (ClutterStageWindow *stage_window,
                                      gboolean            is_resizable)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);
  GdkWMFunction function;

  if (stage_gdk->window == NULL || stage_gdk->foreign_window)
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

  if (stage_gdk->window == NULL || stage_gdk->foreign_window)
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

  /* Foreign window should be shown by the embedding framework. */
  if (!stage_gdk->foreign_window)
    {
      if (do_raise)
        gdk_window_show (stage_gdk->window);
      else
        gdk_window_show_unraised (stage_gdk->window);
    }
}

static void
clutter_stage_gdk_hide (ClutterStageWindow *stage_window)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);

  g_return_if_fail (stage_gdk->window != NULL);

  clutter_actor_unmap (CLUTTER_ACTOR (CLUTTER_STAGE_COGL (stage_gdk)->wrapper));

  /* Foreign window should be hidden by the embedding framework. */
  if (!stage_gdk->foreign_window)
    gdk_window_hide (stage_gdk->window);
}

static gboolean
clutter_stage_gdk_can_clip_redraws (ClutterStageWindow *stage_window)
{
  return TRUE;
}

static int
clutter_stage_gdk_get_scale_factor (ClutterStageWindow *stage_window)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);

  if (stage_gdk->window == NULL)
    return 1;

  return gdk_window_get_scale_factor (stage_gdk->window);
}

static void
clutter_stage_gdk_redraw (ClutterStageWindow *stage_window)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);
  GdkFrameClock *clock;

  if (stage_gdk->window == NULL ||
      (clock = gdk_window_get_frame_clock (stage_gdk->window)) == NULL)
    {
      clutter_stage_window_parent_iface->redraw (stage_window);
      return;
    }

  gdk_frame_clock_begin_updating (clock);

  clutter_stage_window_parent_iface->redraw (stage_window);

  gdk_frame_clock_end_updating (clock);
}

static void
clutter_stage_gdk_schedule_update (ClutterStageWindow *stage_window,
                                    gint                sync_delay)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);
  GdkFrameClock *clock;

  if (stage_gdk->window == NULL ||
      (clock = gdk_window_get_frame_clock (stage_gdk->window)) == NULL)
    {
      clutter_stage_window_parent_iface->schedule_update (stage_window, sync_delay);
      return;
    }

  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_PAINT);

  clutter_stage_window_parent_iface->schedule_update (stage_window, sync_delay);
}

static gint64
clutter_stage_gdk_get_update_time (ClutterStageWindow *stage_window)
{
  ClutterStageGdk *stage_gdk = CLUTTER_STAGE_GDK (stage_window);
  GdkFrameClock *frame_clock;
  GdkFrameTimings *frame_timings;

  if (stage_gdk->window == NULL ||
      (frame_clock = gdk_window_get_frame_clock (stage_gdk->window)) == NULL ||
      (frame_timings = gdk_frame_clock_get_current_timings (frame_clock)) == NULL ||
      !gdk_frame_timings_get_complete (frame_timings))
    return -1; /* No data, indefinite */

  return (gdk_frame_timings_get_presentation_time (frame_timings) +
          gdk_frame_timings_get_refresh_interval (frame_timings));
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

#if defined(GDK_WINDOWING_WAYLAND)
static void
registry_handle_global (void *data,
                        struct wl_registry *registry,
                        uint32_t name,
                        const char *interface,
                        uint32_t version)
{
  ClutterStageGdk *stage_gdk = data;

  if (strcmp (interface, "wl_subcompositor") == 0)
    {
      stage_gdk->subcompositor = wl_registry_bind (registry,
                                                   name,
                                                   &wl_subcompositor_interface,
                                                   1);
    }
}

static void
registry_handle_global_remove (void *data,
                               struct wl_registry *registry,
                               uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
  registry_handle_global,
  registry_handle_global_remove
};
#endif

static void
clutter_stage_gdk_init (ClutterStageGdk *stage)
{
#if defined(GDK_WINDOWING_WAYLAND)
  {
    GdkDisplay *gdk_display = gdk_display_get_default ();
    if (GDK_IS_WAYLAND_DISPLAY (gdk_display))
      {
        struct wl_display *display;
        struct wl_registry *registry;

        display = gdk_wayland_display_get_wl_display (gdk_display);
        registry = wl_display_get_registry (display);
        wl_registry_add_listener (registry, &registry_listener, stage);

        wl_display_roundtrip (display);
      }
  }
#endif
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
  iface->get_scale_factor = clutter_stage_gdk_get_scale_factor;

  iface->redraw = clutter_stage_gdk_redraw;
  iface->schedule_update = clutter_stage_gdk_schedule_update;
  iface->get_update_time = clutter_stage_gdk_get_update_time;
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
