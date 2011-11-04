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

#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-util.h>
#include <wayland-client.h>
#include <xf86drm.h>

#include "clutter-stage-wayland.h"
#include "clutter-wayland.h"
#include "clutter-backend-wayland.h"

#include "clutter-actor-private.h"
#include "clutter-debug.h"
#include "clutter-event.h"
#include "clutter-enum-types.h"
#include "clutter-feature.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include "cogl/cogl-framebuffer-private.h"

static void
wayland_swap_buffers (ClutterStageWayland *stage_wayland);

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

enum
{
  PROP_0,

  PROP_BACKEND,
  PROP_WRAPPER
};

G_DEFINE_TYPE_WITH_CODE (ClutterStageWayland,
                         _clutter_stage_wayland,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define VISUAL_ARGB_PRE COGL_PIXEL_FORMAT_ARGB_8888_PRE
#define VISUAL_ARGB     COGL_PIXEL_FORMAT_ARGB_8888
#define VISUAL_RGB      COGL_PIXEL_FORMAT_RGB_888
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
#define VISUAL_ARGB_PRE COGL_PIXEL_FORMAT_BGRA_8888_PRE
#define VISUAL_ARGB     COGL_PIXEL_FORMAT_BGRA_8888
#define VISUAL_RGB      COGL_PIXEL_FORMAT_BGR_888
#endif

static struct wl_visual *
get_visual (struct wl_display *display, CoglPixelFormat format)
{
  switch (format)
    {
    case VISUAL_ARGB_PRE:
      return wl_display_get_premultiplied_argb_visual (display);
    case VISUAL_ARGB:
      return wl_display_get_argb_visual (display);
    case VISUAL_RGB:
      return wl_display_get_rgb_visual (display);
    default:
      return NULL;
    }
}
static ClutterStageWaylandWaylandBuffer *
wayland_create_shm_buffer (ClutterBackendWayland *backend_wayland,
                           cairo_rectangle_int_t *geom)
{
  ClutterStageWaylandWaylandBufferSHM *buffer;
  struct wl_visual *visual;
  CoglHandle tex;
  CoglTextureFlags flags = COGL_TEXTURE_NONE; /* XXX: tweak flags? */
  CoglPixelFormat format = VISUAL_ARGB_PRE;
  int fd;
  gchar tmp[] = "/tmp/clutter-wayland-shm-XXXXXX";

  buffer = g_slice_new (ClutterStageWaylandWaylandBufferSHM);

  buffer->buffer.type = BUFFER_TYPE_SHM;

  tex = cogl_texture_new_with_size ((unsigned int) geom->width,
			            (unsigned int) geom->height,
			            flags, format);
  buffer->format = format;
  buffer->stride = cogl_texture_get_rowstride(tex);
  buffer->size = cogl_texture_get_data(tex, format, buffer->stride, NULL);
  buffer->buffer.tex = tex;

  fd = g_mkstemp_full(tmp, O_RDWR, 0600);
  ftruncate(fd, buffer->size);
  buffer->data = mmap(NULL, buffer->size, PROT_READ | PROT_WRITE,
	              MAP_SHARED, fd, 0);

  g_unlink(tmp);

  visual = get_visual (backend_wayland->wayland_display, format);

  buffer->buffer.wayland_buffer =
    wl_shm_create_buffer (backend_wayland->wayland_shm,
                          fd,
                          geom->width,
                          geom->height,
                          buffer->stride, visual);
  close(fd);
  return &buffer->buffer;
}

static ClutterStageWaylandWaylandBuffer *
wayland_create_drm_buffer (ClutterBackendWayland *backend_wayland,
                           cairo_rectangle_int_t *geom)
{
  EGLDisplay edpy = clutter_wayland_get_egl_display ();
  struct wl_visual *visual;
  EGLint name, stride;
  ClutterStageWaylandWaylandBufferDRM *buffer;
  EGLint image_attribs[] = {
      EGL_WIDTH, 0,
      EGL_HEIGHT, 0,
      EGL_DRM_BUFFER_FORMAT_MESA, EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
      EGL_DRM_BUFFER_USE_MESA, EGL_DRM_BUFFER_USE_SCANOUT_MESA,
      EGL_NONE
  };

  buffer = g_slice_new (ClutterStageWaylandWaylandBufferDRM);

  buffer->buffer.type = BUFFER_TYPE_DRM;

  image_attribs[1] = geom->width;
  image_attribs[3] = geom->height;
  buffer->drm_image = backend_wayland->create_drm_image (edpy, image_attribs);
  glGenTextures (1, &buffer->texture);
  glBindTexture (GL_TEXTURE_2D, buffer->texture);
  backend_wayland->image_target_texture_2d (GL_TEXTURE_2D, buffer->drm_image);

  buffer->buffer.tex = cogl_texture_new_from_foreign (buffer->texture,
					       GL_TEXTURE_2D,
					       geom->width,
					       geom->height,
					       0,
					       0,
					       VISUAL_ARGB_PRE);

  backend_wayland->export_drm_image (edpy, buffer->drm_image,
				     &name, NULL, &stride);
  visual = get_visual (backend_wayland->wayland_display, VISUAL_ARGB_PRE);
  buffer->buffer.wayland_buffer =
    wl_drm_create_buffer (backend_wayland->wayland_drm,
                          name,
                          geom->width,
                          geom->height,
                          stride, visual);

  return &buffer->buffer;
}

static ClutterStageWaylandWaylandBuffer *
wayland_create_buffer (cairo_rectangle_int_t *geom)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  ClutterStageWaylandWaylandBuffer *buffer;
  cairo_rectangle_int_t rect;

  if (backend_wayland->drm_enabled &&
      backend_wayland->wayland_drm != NULL)
    buffer = wayland_create_drm_buffer (backend_wayland, geom);
  else if (backend_wayland->wayland_shm != NULL)
    buffer = wayland_create_shm_buffer (backend_wayland, geom);
  else
    return NULL;

  buffer->offscreen = cogl_offscreen_new_to_texture (buffer->tex);

  rect.x = geom->x;
  rect.y = geom->y;
  rect.width = geom->width;
  rect.height = geom->height;
  buffer->dirty_region = cairo_region_create_rectangle (&rect);

  return buffer;
}

static void
wayland_free_shm_buffer (ClutterStageWaylandWaylandBuffer *generic_buffer)
{
  ClutterStageWaylandWaylandBufferSHM *buffer;

  buffer = (ClutterStageWaylandWaylandBufferSHM *)generic_buffer;

  munmap(buffer->data, buffer->size);
  g_slice_free (ClutterStageWaylandWaylandBufferSHM, buffer);
}

static void
wayland_free_drm_buffer (ClutterStageWaylandWaylandBuffer *generic_buffer)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  EGLDisplay edpy = clutter_wayland_get_egl_display ();
  ClutterStageWaylandWaylandBufferDRM *buffer;

  buffer = (ClutterStageWaylandWaylandBufferDRM *)generic_buffer;

  glDeleteTextures (1, &buffer->texture);
  backend_wayland->destroy_image (edpy, buffer->drm_image);
  g_slice_free (ClutterStageWaylandWaylandBufferDRM, buffer);
}

static void
wayland_free_buffer (ClutterStageWaylandWaylandBuffer *buffer)
{
  cogl_handle_unref (buffer->tex);
  wl_buffer_destroy (buffer->wayland_buffer);
  cogl_handle_unref (buffer->offscreen);

  if (buffer->type == BUFFER_TYPE_DRM)
    wayland_free_drm_buffer(buffer);
  else if (buffer->type == BUFFER_TYPE_SHM)
    wayland_free_shm_buffer(buffer);
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
    wayland_create_buffer (&stage_wayland->allocation);

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
clutter_stage_wayland_get_geometry (ClutterStageWindow    *stage_window,
				    cairo_rectangle_int_t *geometry)
{
  if (geometry != NULL)
    {
      ClutterStageWayland *stage_wayland =
        CLUTTER_STAGE_WAYLAND (stage_window);

      *geometry = stage_wayland->allocation;
    }
}

static void
clutter_stage_wayland_resize (ClutterStageWindow *stage_window,
			      gint                width,
			      gint                height)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);

  fprintf (stderr, "resize %dx%d\n", width, height);

  stage_wayland->pending_allocation.width = width;
  stage_wayland->pending_allocation.height = height;

  /* FIXME: Shouldn't the stage repaint everything when it gets resized? */
  cairo_region_union_rectangle (stage_wayland->repaint_region,
                                &stage_wayland->pending_allocation);
}

#define CAIRO_REGION_FULL ((cairo_region_t *) 1)

static gboolean
clutter_stage_wayland_has_redraw_clips (ClutterStageWindow *stage_window)
{
  return TRUE;
}

static gboolean
clutter_stage_wayland_ignoring_redraw_clips (ClutterStageWindow *stage_window)
{
  return FALSE;
}

static void
clutter_stage_wayland_add_redraw_clip (ClutterStageWindow    *stage_window,
				       cairo_rectangle_int_t *stage_clip)
{
  ClutterStageWayland *stage_wayland = CLUTTER_STAGE_WAYLAND (stage_window);
  cairo_rectangle_int_t rect;

  if (stage_clip == NULL)
    rect = stage_wayland->allocation;
  else
    rect = stage_clip;

  if (stage_wayland->repaint_region == NULL)
    stage_wayland->repaint_region = cairo_region_create_rectangle (&rect);
  else
    cairo_region_union_rectangle (stage_wayland->repaint_region, &rect);
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

  iface->add_redraw_clip = clutter_stage_wayland_add_redraw_clip;
  iface->has_redraw_clips = clutter_stage_wayland_has_redraw_clips;
  iface->ignoring_redraw_clips = clutter_stage_wayland_ignoring_redraw_clips;
}

static void
clutter_stage_wayland_set_property (GObject      *gobject,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ClutterStageWayland *self = CLUTTER_STAGE_WAYLAND (gobject);

  switch (prop_id)
    {
    case PROP_BACKEND:
      self->backend = g_value_get_object (value);
      break;

    case PROP_WRAPPER:
      self->wrapper = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
_clutter_stage_wayland_class_init (ClutterStageWaylandClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = clutter_stage_wayland_set_property;

  g_object_class_override_property (gobject_class, PROP_BACKEND, "backend");
  g_object_class_override_property (gobject_class, PROP_WRAPPER, "wrapper");
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
wayland_frame_callback (void *data, uint32_t _time)
{
  ClutterStageWayland *stage_wayland = data;

  stage_wayland->pending_swaps--;
}

static void
wayland_damage_buffer(ClutterStageWaylandWaylandBuffer *generic_buffer)
{
  ClutterStageWaylandWaylandBufferSHM *buffer;
  int size;

  if (generic_buffer->type != BUFFER_TYPE_SHM)
    return;

  buffer = (ClutterStageWaylandWaylandBufferSHM *)generic_buffer;

  size = cogl_texture_get_data(buffer->buffer.tex, buffer->format,
                               buffer->stride, NULL);
  g_assert(size == (int)buffer->size);

  (void) cogl_texture_get_data(buffer->buffer.tex, buffer->format,
                               buffer->stride, buffer->data);

}

static void
wayland_swap_buffers (ClutterStageWayland *stage_wayland)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendWayland *backend_wayland = CLUTTER_BACKEND_WAYLAND (backend);
  ClutterStageWaylandWaylandBuffer *buffer;

  buffer = stage_wayland->front_buffer;
  stage_wayland->front_buffer = stage_wayland->back_buffer;
  stage_wayland->back_buffer = buffer;

  wayland_damage_buffer(stage_wayland->front_buffer);

  wl_surface_attach (stage_wayland->wayland_surface,
                     stage_wayland->front_buffer->wayland_buffer,
  /* 0,0 here is "relative to the old buffer," not absolute */
                     0, 0);
  wl_surface_map_toplevel (stage_wayland->wayland_surface);

  stage_wayland->pending_swaps++;
  wl_display_frame_callback (backend_wayland->wayland_display,
			     wayland_frame_callback,
			     stage_wayland);
}

static void
_clutter_stage_wayland_repair_dirty(ClutterStageWayland *stage_wayland,
				       ClutterStage     *stage)
{
  CoglMaterial *outline = NULL;
  CoglHandle vbo;
  float vertices[8], texcoords[8];
  CoglMatrix modelview;
  cairo_region_t *dirty;
  cairo_rectangle_int_t rect;
  int i, count;
  float width, height;

  dirty = stage_wayland->back_buffer->dirty_region;
  stage_wayland->back_buffer->dirty_region = NULL;
  cairo_region_subtract (dirty, stage_wayland->repaint_region);
  width = stage_wayland->allocation.width;
  height = stage_wayland->allocation.height;
  
  /* If this is the first time we render, there is no front buffer to
   * copy back from, but then the dirty region not covered by the
   * repaint should be empty, because we repaint the entire stage.
   *
   * assert(stage_wayland->front_buffer != NULL) ||
   *   cairo_region_is_empty(dirty);
   *
   * FIXME: in test-rotate, the stage never queues a full repaint
   * initially, it's restricted to the paint box of it's rotating
   * children.
   */

  if (!stage_wayland->front_buffer)
    return;

  outline = cogl_material_new ();
  cogl_material_set_layer (outline, 0, stage_wayland->front_buffer->tex);
  count = cairo_region_num_rectangles (dirty);

  for (i = 0; i < count; i++)
    {
      cairo_region_get_rectangle (dirty, i, &rect);
      vbo = cogl_vertex_buffer_new (4);

      vertices[0] = rect.x - 1;
      vertices[1] = rect.y - 1;
      vertices[2] = rect.x + rect.width + 1;
      vertices[3] = rect.y - 1;
      vertices[4] = rect.x + rect.width + 1;
      vertices[5] = rect.y + rect.height + 1;
      vertices[6] = rect.x - 1;
      vertices[7] = rect.y + rect.height + 1;

      cogl_vertex_buffer_add (vbo,
			      "gl_Vertex",
			      2, /* n_components */
			      COGL_ATTRIBUTE_TYPE_FLOAT,
			      FALSE, /* normalized */
			      0, /* stride */
			      vertices);

      texcoords[0] = vertices[0] / width;
      texcoords[1] = vertices[1] / height;
      texcoords[2] = vertices[2] / width;
      texcoords[3] = vertices[3] / height;
      texcoords[4] = vertices[4] / width;
      texcoords[5] = vertices[5] / height;
      texcoords[6] = vertices[6] / width;
      texcoords[7] = vertices[7] / height;

      cogl_vertex_buffer_add (vbo,
			      "gl_MultiTexCoord0",
			      2, /* n_components */
			      COGL_ATTRIBUTE_TYPE_FLOAT,
			      FALSE, /* normalized */
			      0, /* stride */
			      texcoords);

      cogl_vertex_buffer_submit (vbo);

      cogl_push_matrix ();
      cogl_matrix_init_identity (&modelview);
      _clutter_actor_apply_modelview_transform (CLUTTER_ACTOR (stage),
						&modelview);
      cogl_set_modelview_matrix (&modelview);
      cogl_set_source (outline);
      cogl_vertex_buffer_draw (vbo, COGL_VERTICES_MODE_TRIANGLE_FAN,
			       0 , 4);
      cogl_pop_matrix ();
      cogl_object_unref (vbo);
    }

  cairo_region_destroy (dirty);
}

void
_clutter_stage_wayland_repaint_region (ClutterStageWayland *stage_wayland,
				       ClutterStage        *stage)
{
  cairo_rectangle_int_t rect;
  int i, count;

  count = cairo_region_num_rectangles (stage_wayland->repaint_region);
  for (i = 0; i < count; i++)
    {
      cairo_region_get_rectangle (stage_wayland->repaint_region, i, &rect);

      cogl_clip_push_window_rectangle (rect.x - 1,
                                       rect.y - 1,
                                       rect.width + 2,
                                       rect.height + 2);

      /* FIXME: We should pass geom in as second arg, but some actors
       * cull themselves a little to much.  Disable for now.*/
      _clutter_stage_do_paint (stage, NULL);

      cogl_clip_pop ();
    }
}

void
_clutter_stage_wayland_redraw (ClutterStageWayland *stage_wayland,
			       ClutterStage    *stage)
{
  stage_wayland->allocation = stage_wayland->pending_allocation;

  if (!stage_wayland->back_buffer)
    {
      stage_wayland->back_buffer =
        wayland_create_buffer (&stage_wayland->allocation);
    }

  cogl_set_framebuffer (stage_wayland->back_buffer->offscreen);
  _clutter_stage_maybe_setup_viewport (stage_wayland->wrapper);

  _clutter_stage_wayland_repair_dirty (stage_wayland, stage);

  _clutter_stage_wayland_repaint_region (stage_wayland, stage);

  cogl_flush ();
  glFlush ();

  wayland_swap_buffers (stage_wayland);

  if (stage_wayland->back_buffer)
    stage_wayland->back_buffer->dirty_region = stage_wayland->repaint_region;
  else
    cairo_region_destroy (stage_wayland->repaint_region);

  stage_wayland->repaint_region = NULL;

  cogl_set_framebuffer (stage_wayland->pick_buffer->offscreen);
  _clutter_stage_maybe_setup_viewport (stage_wayland->wrapper);
}
