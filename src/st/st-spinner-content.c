/*
 * Copyright 2024 Red Hat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "st-spinner-content.h"

#include <cairo.h>

#include <st-widget.h>

#define MIN_RADIUS 8
#define NAT_RADIUS 48
#define SMALL_WIDTH 2.5
#define LARGE_WIDTH 12
#define SPIN_DURATION_MS 1200
#define START_ANGLE (G_PI * 0.35)
#define CIRCLE_OPACITY 0.15
#define MIN_ARC_LENGTH (G_PI * 0.015)
#define MAX_ARC_LENGTH (G_PI * 0.9)
#define IDLE_DISTANCE (G_PI * 0.9)
#define OVERLAP_DISTANCE (G_PI * 0.7)
#define EXTEND_DISTANCE (G_PI * 1.1)
#define CONTRACT_DISTANCE (G_PI * 1.35)
/* How many full cycles it takes for the spinner to loop. Should be:
 * (IDLE_DISTANCE + EXTEND_DISTANCE + CONTRACT_DISTANCE - OVERLAP_DISTANCE) * k,
 * where k is an integer */
#define N_CYCLES 53

/**
 * StSpinnerContent:
 *
 * A [iface@Clutter.Content] showing a loading spinner.
 *
 * `StSpinnerContent` size varies depending on the available space, but is
 * capped at 96×96 pixels.
 *
 * It will be animated whenever it is attached to a mapped actor.
 *
 * If the attached actor is a [class@Widget], its style information will
 * be used, similar to symbolic icons.
 */

struct _StSpinnerContent
{
  GObject parent_instance;

  ClutterTimeline *timeline;
  ClutterActor *actor;

  CoglTexture *texture;
  gboolean dirty;

  CoglBitmap *buffer;
};

static void st_spinner_content_iface_init (ClutterContentInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (StSpinnerContent, st_spinner_content, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTENT,
                                                      st_spinner_content_iface_init))

static void
st_spinner_content_init (StSpinnerContent *spinner)
{
}

static void
st_spinner_content_class_init (StSpinnerContentClass *klass)
{
}

static void
actor_map_cb (GObject    *object,
              GParamSpec *pspec,
              gpointer    user_data)
{
  ClutterActor *actor = CLUTTER_ACTOR (object);
  StSpinnerContent *spinner = user_data;

  if (!spinner->timeline)
    return;

  if (clutter_actor_is_mapped (actor))
    clutter_timeline_start (spinner->timeline);
  else
    clutter_timeline_stop (spinner->timeline);
}

static void
new_frame_cb (ClutterTimeline *timeline,
              int              elapsed,
              gpointer         user_data)
{
  ClutterContent *content = user_data;

  clutter_content_invalidate (content);
}

static void
st_spinner_content_set_actor (StSpinnerContent *spinner,
                              ClutterActor     *actor)
{
  if (actor == spinner->actor)
    return;

  if (spinner->actor)
    {
      g_clear_object (&spinner->timeline);
      g_signal_handlers_disconnect_by_func (spinner->actor, actor_map_cb, spinner);
    }

  g_set_object (&spinner->actor, actor);

  if (spinner->actor)
    {
      spinner->timeline = clutter_timeline_new_for_actor (actor,
                                                          SPIN_DURATION_MS * N_CYCLES);
      clutter_timeline_set_repeat_count (spinner->timeline, -1);
      clutter_timeline_set_progress_mode (spinner->timeline, CLUTTER_LINEAR);

      g_signal_connect_object (spinner->timeline, "new-frame", G_CALLBACK (new_frame_cb), spinner, 0);

      if (clutter_actor_is_mapped (actor))
        clutter_timeline_start (spinner->timeline);

      g_signal_connect (actor, "notify::mapped", G_CALLBACK (actor_map_cb), spinner);
    }

  clutter_content_invalidate (CLUTTER_CONTENT (spinner));
}

static void
st_spinner_content_attached (ClutterContent *content,
                             ClutterActor   *actor)
{
  st_spinner_content_set_actor (ST_SPINNER_CONTENT (content), actor);
}

static void
st_spinner_content_detached (ClutterContent *content,
                             ClutterActor   *actor)
{
  st_spinner_content_set_actor (ST_SPINNER_CONTENT (content), NULL);
}

static void
st_spinner_content_paint_content (ClutterContent      *content,
                                  ClutterActor        *actor,
                                  ClutterPaintNode    *root,
                                  ClutterPaintContext *paint_context)
{
  StSpinnerContent *spinner = ST_SPINNER_CONTENT (content);
  ClutterPaintNode *node;

  if (spinner->buffer == NULL)
    return;

  if (spinner->dirty)
    g_clear_object (&spinner->texture);

  if (spinner->texture == NULL)
    spinner->texture = COGL_TEXTURE (cogl_texture_2d_new_from_bitmap (spinner->buffer));

  if (spinner->texture == NULL)
    return;

  node = clutter_actor_create_texture_paint_node (actor, spinner->texture);
  clutter_paint_node_set_static_name (node, "Spinner Content");
  clutter_paint_node_add_child (root, node);
  clutter_paint_node_unref (node);

  spinner->dirty = FALSE;
}

#define LERP(a, b, t) (a + (b - a) * t)
#define INVERSE_LERP(a, b, t) ((t - a) / (b - a))

static double
normalize_angle (double angle)
{
  while (angle < 0)
    angle += G_PI * 2;

  while (angle > G_PI * 2)
    angle -= G_PI * 2;

  return angle;
}

static inline double
ease_in_out_sine (double t)
{
  return -0.5 * (cos (M_PI * t) - 1);
}

static double
get_arc_start (double angle)
{
  double l = IDLE_DISTANCE + EXTEND_DISTANCE + CONTRACT_DISTANCE - OVERLAP_DISTANCE;
  double t;

  angle = fmod (angle, l);

  if (angle > EXTEND_DISTANCE)
    t = 1;
  else
    t = ease_in_out_sine (angle / EXTEND_DISTANCE);

  return LERP (MIN_ARC_LENGTH, MAX_ARC_LENGTH, t) - angle * MAX_ARC_LENGTH / l;
}

static double
get_arc_end (double angle)
{
  double l = IDLE_DISTANCE + EXTEND_DISTANCE + CONTRACT_DISTANCE - OVERLAP_DISTANCE;
  double t;

  angle = fmod (angle, l);

  if (angle < EXTEND_DISTANCE - OVERLAP_DISTANCE)
    t = 0;
  else if (angle > l - IDLE_DISTANCE)
    t = 1;
  else
    t = ease_in_out_sine ((angle - EXTEND_DISTANCE + OVERLAP_DISTANCE) / CONTRACT_DISTANCE);

  return LERP (0, MAX_ARC_LENGTH - MIN_ARC_LENGTH, t) - angle * MAX_ARC_LENGTH / l;
}

static void
st_spinner_content_draw_spinner (StSpinnerContent *spinner,
                                 cairo_t          *cr,
                                 int               width,
                                 int               height)
{
  CoglColor color;
  double radius, line_width;
  double progress, start_angle, end_angle;

  g_assert (spinner->actor);

  if (ST_IS_WIDGET (spinner->actor))
    {
      StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (spinner->actor));
      st_theme_node_get_foreground_color (theme_node, &color);
    }
  else
    {
      cogl_color_init_from_4f (&color, 0., 0., 0., 1.);
    }

  radius = MIN (floorf (MIN (width, height) / 2), NAT_RADIUS);
  line_width = LERP (SMALL_WIDTH, LARGE_WIDTH,
                     INVERSE_LERP (MIN_RADIUS, NAT_RADIUS, radius));
  radius -= roundf (line_width / 2.);

  if (radius < 0)
    return;

  cairo_translate (cr, roundf (width / 2), roundf(height / 2));
  cairo_set_line_width (cr, line_width);

  /* Circle */

  cairo_save (cr);

  cairo_set_source_rgba (cr,
                         color.red / 255.,
                         color.green / 255.,
                         color.blue / 255.,
                         color.alpha / 255. * CIRCLE_OPACITY);
  cairo_arc (cr, 0, 0, radius, 0, 2 * M_PI);
  cairo_stroke (cr);

  cairo_restore (cr);

  /* Moving part */

  cairo_save (cr);

  if (spinner->timeline)
    progress = clutter_timeline_get_progress (spinner->timeline) * N_CYCLES * M_PI * 2;
  else
    progress = EXTEND_DISTANCE - OVERLAP_DISTANCE / 2;

  start_angle = progress + get_arc_start (progress) + START_ANGLE;
  end_angle = progress + get_arc_end (progress) + START_ANGLE;

  start_angle = normalize_angle (start_angle);
  end_angle = normalize_angle (end_angle);

  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_source_rgba (cr,
                         color.red / 255.,
                         color.green / 255.,
                         color.blue / 255.,
                         color.alpha / 255.);

  cairo_arc (cr, 0, 0, radius, end_angle, start_angle);
  cairo_stroke (cr);

  cairo_restore (cr);
}

static void
st_spinner_content_redraw (StSpinnerContent *spinner)
{
  ClutterActorBox allocation;
  int width, height;
  int real_width, real_height;
  float scale_factor;
  cairo_surface_t *surface;
  gboolean mapped_buffer;
  unsigned char *data;
  CoglBuffer *buffer;
  cairo_t *cr;

  g_assert (spinner->actor && clutter_actor_is_mapped (spinner->actor));

  spinner->dirty = TRUE;

  clutter_actor_get_allocation_box (spinner->actor, &allocation);

  width = (int)(0.5 + allocation.x2 - allocation.x1);
  height = (int)(0.5 + allocation.y2 - allocation.y1);

  scale_factor = clutter_actor_get_resource_scale (spinner->actor);

  real_width = ceilf (width * scale_factor);
  real_height = ceilf (height * scale_factor);

  if (width == 0 || height == 0)
    return;

  if (spinner->buffer == NULL)
    {
      ClutterBackend *backend;
      CoglContext *ctx;

      backend = clutter_context_get_backend (clutter_actor_get_context (spinner->actor));
      ctx = clutter_backend_get_cogl_context (backend);
      spinner->buffer = cogl_bitmap_new_with_size (ctx,
                                                   real_width,
                                                   real_height,
                                                   COGL_PIXEL_FORMAT_CAIRO_ARGB32_COMPAT);
    }

  buffer = COGL_BUFFER (cogl_bitmap_get_buffer (spinner->buffer));
  if (buffer == NULL)
    return;
  cogl_buffer_set_update_hint (buffer, COGL_BUFFER_UPDATE_HINT_DYNAMIC);

  data = cogl_buffer_map (buffer,
                          COGL_BUFFER_ACCESS_READ_WRITE,
                          COGL_BUFFER_MAP_HINT_DISCARD);

  if (data != NULL)
    {
      int bitmap_stride = cogl_bitmap_get_rowstride (spinner->buffer);

      surface = cairo_image_surface_create_for_data (data,
                                                     CAIRO_FORMAT_ARGB32,
                                                     real_width,
                                                     real_height,
                                                     bitmap_stride);
      mapped_buffer = TRUE;
    }
  else
    {
      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            real_width,
                                            real_height);

      mapped_buffer = FALSE;
    }

  cairo_surface_set_device_scale (surface,
                                  scale_factor,
                                  scale_factor);

  cr = cairo_create (surface);

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_restore (cr);

  st_spinner_content_draw_spinner (spinner, cr, width, height);

  cairo_destroy (cr);

  if (mapped_buffer)
    cogl_buffer_unmap (buffer);
  else
    {
      int size = cairo_image_surface_get_stride (surface) * height;
      cogl_buffer_set_data (buffer,
                            0,
                            cairo_image_surface_get_data (surface),
                            size);
    }

  cairo_surface_destroy (surface);
}

static void
st_spinner_content_invalidate (ClutterContent *content)
{
  StSpinnerContent *spinner = ST_SPINNER_CONTENT (content);

  g_clear_object (&spinner->buffer);

  if (spinner->actor && clutter_actor_is_mapped (spinner->actor))
    st_spinner_content_redraw (spinner);
}

static void
st_spinner_content_iface_init (ClutterContentInterface *iface)
{
  iface->paint_content = st_spinner_content_paint_content;
  iface->attached = st_spinner_content_attached;
  iface->detached = st_spinner_content_detached;
  iface->invalidate = st_spinner_content_invalidate;
}

/**
 * st_spinner_content_new:
 *
 * Returns: (transfer full): the newly created #StSpinnerContent content
 */
ClutterContent *
st_spinner_content_new (void)
{
  return g_object_new (ST_TYPE_SPINNER_CONTENT, NULL);
}
