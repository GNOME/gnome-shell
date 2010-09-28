/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

 * Authors:
 *  Matthew Allum
 *  Robert Bragg
 *  Kristian Høgsberg
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <wayland-util.h>
#include <wayland-client.h>
#include <xf86drm.h>

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

#include "clutter-stage-wayland.h"
#include "clutter-wayland.h"
#include "clutter-backend-wayland.h"

#include "cogl/cogl-framebuffer-private.h"

static void
wayland_swap_buffers (ClutterStageWayland *stage_wayland);

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStageWayland,
                         _clutter_stage_wayland,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static ClutterStageWaylandWaylandBuffer *
wayland_create_buffer (ClutterStageWayland *stage_wayland,
                       ClutterGeometry *geom)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  EGLDisplay edpy = clutter_egl_display ();
  ClutterStageWaylandWaylandBuffer *buffer;
  EGLint image_attribs[] = {
      EGL_WIDTH, 0,
      EGL_HEIGHT, 0,
      EGL_DRM_BUFFER_FORMAT_MESA, EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
      EGL_DRM_BUFFER_USE_MESA, EGL_DRM_BUFFER_USE_SCANOUT_MESA,
      EGL_NONE
  };
  CoglHandle tex;

  buffer = g_slice_new (ClutterStageWaylandWaylandBuffer);

  image_attribs[1] = geom->width;
  image_attribs[3] = geom->height;
  buffer->drm_image = backend_wayland->create_drm_image (edpy, image_attribs);
  glGenTextures (1, &buffer->texture);
  glBindTexture (GL_TEXTURE_2D, buffer->texture);
  backend_wayland->image_target_texture_2d (GL_TEXTURE_2D, buffer->drm_image);

  tex = cogl_texture_new_from_foreign (buffer->texture,
                                       GL_TEXTURE_2D,
                                       geom->width,
                                       geom->height,
                                       0,
                                       0,
                                       COGL_PIXEL_FORMAT_ARGB_8888);
  buffer->offscreen = cogl_offscreen_new_to_texture (tex);
  cogl_handle_unref (tex);
  buffer->wayland_buffer = NULL;

  return buffer;
}

static void
wayland_free_buffer (ClutterStageWaylandWaylandBuffer *buffer)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  EGLDisplay edpy = clutter_egl_display ();

  if (buffer->wayland_buffer)
    wl_buffer_destroy (buffer->wayland_buffer);

  cogl_handle_unref (buffer->offscreen);
  glDeleteTextures (1, &buffer->texture);
  backend_wayland->destroy_image (edpy, buffer->drm_image);
  g_slice_free (ClutterStageWaylandWaylandBuffer, buffer);
}

static void
clutter_stage_wayland_unrealize (ClutterStageWindow *stage_window)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);

  if (stage_wayland->front_buffer)
    {
      wayland_free_buffer (stage_wayland->front_buffer);
      stage_wayland->front_buffer = NULL;
    }

  if (stage_wayland->back_buffer)
    {
      wayland_free_buffer (stage_wayland->back_buffer);
      stage_wayland->back_buffer = NULL;
    }

  wayland_free_buffer (stage_wayland->pick_buffer);
}

static gboolean
clutter_stage_wayland_realize (ClutterStageWindow *stage_window)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  gfloat width, height;

  clutter_actor_get_size (CLUTTER_ACTOR (stage_wayland->wrapper),
			  &width, &height);
  stage_wayland->pending_allocation.width = (gint)width;
  stage_wayland->pending_allocation.height = (gint)height;
  stage_wayland->allocation = stage_wayland->pending_allocation;

  stage_wayland->wayland_surface =
    wl_compositor_create_surface (backend_wayland->wayland_compositor);
  wl_surface_set_user_data (stage_wayland->wayland_surface, stage_wayland);

  stage_wayland->pick_buffer =
    wayland_create_buffer (stage_wayland, &stage_wayland->allocation);

  return TRUE;
}

static int
clutter_stage_wayland_get_pending_swaps (ClutterStageWindow *stage_window)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);

  return stage_wayland->pending_swaps;
}

static void
clutter_stage_wayland_set_fullscreen (ClutterStageWindow *stage_window,
				      gboolean            fullscreen)
{
  g_warning ("Stage of type '%s' do not support ClutterStage::set_fullscreen",
             G_OBJECT_TYPE_NAME (stage_window));
}

static void
clutter_stage_wayland_set_title (ClutterStageWindow *stage_window,
				 const gchar        *title)
{
  g_warning ("Stage of type '%s' do not support ClutterStage::set_title",
             G_OBJECT_TYPE_NAME (stage_window));
}

static void
clutter_stage_wayland_set_cursor_visible (ClutterStageWindow *stage_window,
					  gboolean            cursor_visible)
{
  g_warning ("Stage of type '%s' do not support ClutterStage::set_cursor_visible",
             G_OBJECT_TYPE_NAME (stage_window));
}

static ClutterActor *
clutter_stage_wayland_get_wrapper (ClutterStageWindow *stage_window)
{
  return CLUTTER_ACTOR (CLUTTER_STAGE_WAYLAND (stage_window)->wrapper);
}

static void
clutter_stage_wayland_show (ClutterStageWindow *stage_window,
			    gboolean            do_raise)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);

  clutter_actor_map (CLUTTER_ACTOR (stage_wayland->wrapper));
}

static void
clutter_stage_wayland_hide (ClutterStageWindow *stage_window)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);

  clutter_actor_unmap (CLUTTER_ACTOR (stage_wayland->wrapper));
}

static void
clutter_stage_wayland_get_geometry (ClutterStageWindow *stage_window,
				    ClutterGeometry    *geometry)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);

  if (geometry)
    {
      *geometry = stage_wayland->allocation;
    }
}

static void
clutter_stage_wayland_resize (ClutterStageWindow *stage_window,
			      gint                width,
			      gint                height)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);

  stage_wayland->pending_allocation.width = width;
  stage_wayland->pending_allocation.height = height;
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->realize = clutter_stage_wayland_realize;
  iface->unrealize = clutter_stage_wayland_unrealize;
  iface->get_pending_swaps = clutter_stage_wayland_get_pending_swaps;
  iface->set_fullscreen = clutter_stage_wayland_set_fullscreen;
  iface->set_title = clutter_stage_wayland_set_title;
  iface->set_cursor_visible = clutter_stage_wayland_set_cursor_visible;
  iface->get_wrapper = clutter_stage_wayland_get_wrapper;
  iface->get_geometry = clutter_stage_wayland_get_geometry;
  iface->resize = clutter_stage_wayland_resize;
  iface->show = clutter_stage_wayland_show;
  iface->hide = clutter_stage_wayland_hide;
}

static void
_clutter_stage_wayland_class_init (ClutterStageWaylandClass *klass)
{
}

static void
_clutter_stage_wayland_init (ClutterStageWayland *stage_wayland)
{
  stage_wayland->allocation.x = 0;
  stage_wayland->allocation.y = 0;
  stage_wayland->allocation.width = 640;
  stage_wayland->allocation.height = 480;
  stage_wayland->save_allocation = stage_wayland->allocation;
}

static void
wayland_free_front_buffer (void *data)
{
  ClutterStageWayland *stage_wayland = data;

  if (stage_wayland->front_buffer)
    wayland_free_buffer (stage_wayland->front_buffer);
  stage_wayland->front_buffer = stage_wayland->pending_buffer;
  stage_wayland->pending_buffer = NULL;

  if (stage_wayland->back_buffer)
    wayland_swap_buffers (stage_wayland);
}

static void
wayland_frame_callback (void *data, uint32_t _time)
{
  ClutterStageWayland *stage_wayland = data;

  stage_wayland->pending_swaps--;
}

static void
wayland_swap_buffers (ClutterStageWayland *stage_wayland)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  EGLDisplay edpy = clutter_egl_display ();
  EGLint name;
  EGLint stride;
  struct wl_visual *visual;

  if (stage_wayland->pending_buffer)
    return;

  stage_wayland->pending_buffer = stage_wayland->back_buffer;
  stage_wayland->back_buffer = NULL;

  backend_wayland->export_drm_image (edpy,
				     stage_wayland->pending_buffer->drm_image,
				     &name, NULL, &stride);
  visual =
    wl_display_get_premultiplied_argb_visual (backend_wayland->wayland_display);
  stage_wayland->pending_buffer->wayland_buffer =
    wl_drm_create_buffer (backend_wayland->wayland_drm,
                          name,
                          stage_wayland->allocation.width,
                          stage_wayland->allocation.height,
                          stride, visual);
  wl_surface_attach (stage_wayland->wayland_surface,
                     stage_wayland->pending_buffer->wayland_buffer);
  wl_surface_map (stage_wayland->wayland_surface,
                  stage_wayland->allocation.x,
                  stage_wayland->allocation.y,
                  stage_wayland->allocation.width,
                  stage_wayland->allocation.height);
  wl_display_sync_callback (backend_wayland->wayland_display,
                            wayland_free_front_buffer,
                            stage_wayland);

  stage_wayland->pending_swaps++;
  wl_display_frame_callback (backend_wayland->wayland_display,
			     wayland_frame_callback,
			     stage_wayland);
}

void
_clutter_stage_wayland_redraw (ClutterStageWayland *stage_wayland,
			       ClutterStage    *stage)
{
  ClutterActor *wrapper = CLUTTER_ACTOR (stage_wayland->wrapper);

  stage_wayland->allocation = stage_wayland->pending_allocation;

  if (stage_wayland->back_buffer)
    {
      wayland_free_buffer (stage_wayland->back_buffer);
      stage_wayland->back_buffer = NULL;
    }

  stage_wayland->back_buffer = wayland_create_buffer (stage_wayland,
                                                  &stage_wayland->allocation);

  cogl_set_framebuffer (stage_wayland->back_buffer->offscreen);
  _clutter_stage_maybe_setup_viewport (stage_wayland->wrapper);

  clutter_actor_paint (wrapper);
  cogl_flush ();
  glFlush ();

  cogl_set_framebuffer (stage_wayland->pick_buffer->offscreen);
  _clutter_stage_maybe_setup_viewport (stage_wayland->wrapper);

  wayland_swap_buffers (stage_wayland);
}
