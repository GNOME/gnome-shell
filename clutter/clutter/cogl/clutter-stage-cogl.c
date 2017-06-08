/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2007,2008,2009,2010,2011  Intel Corporation.
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
 *  Neil Roberts
 *  Emmanuele Bassi
 */


#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include "clutter-config.h"

#include "clutter-stage-cogl.h"

#include <stdlib.h>
#include <math.h>

#include "clutter-actor-private.h"
#include "clutter-backend-private.h"
#include "clutter-debug.h"
#include "clutter-event.h"
#include "clutter-enum-types.h"
#include "clutter-feature.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

typedef struct _ClutterStageViewCoglPrivate
{
  /*
   * List of previous damaged areas in stage view framebuffer coordinate space.
   */
#define DAMAGE_HISTORY_MAX 16
#define DAMAGE_HISTORY(x) ((x) & (DAMAGE_HISTORY_MAX - 1))
  cairo_rectangle_int_t damage_history[DAMAGE_HISTORY_MAX];
  unsigned int damage_index;
} ClutterStageViewCoglPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ClutterStageViewCogl, clutter_stage_view_cogl,
                            CLUTTER_TYPE_STAGE_VIEW)

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStageCogl,
                         _clutter_stage_cogl,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

enum {
  PROP_0,
  PROP_WRAPPER,
  PROP_BACKEND,
  PROP_LAST
};

static void
clutter_stage_cogl_unrealize (ClutterStageWindow *stage_window)
{
  CLUTTER_NOTE (BACKEND, "Unrealizing Cogl stage [%p]", stage_window);
}

void
_clutter_stage_cogl_presented (ClutterStageCogl *stage_cogl,
                               CoglFrameEvent    frame_event,
                               ClutterFrameInfo *frame_info)
{

  if (frame_event == COGL_FRAME_EVENT_SYNC)
    {
      /* Early versions of the swap_event implementation in Mesa
       * deliver BufferSwapComplete event when not selected for,
       * so if we get a swap event we aren't expecting, just ignore it.
       *
       * https://bugs.freedesktop.org/show_bug.cgi?id=27962
       *
       * FIXME: This issue can be hidden inside Cogl so we shouldn't
       * need to care about this bug here.
       */
      if (stage_cogl->pending_swaps > 0)
        stage_cogl->pending_swaps--;
    }
  else if (frame_event == COGL_FRAME_EVENT_COMPLETE)
    {
      gint64 presentation_time_cogl = frame_info->presentation_time;

      if (presentation_time_cogl != 0)
        {
          ClutterBackend *backend = stage_cogl->backend;
          CoglContext *context = clutter_backend_get_cogl_context (backend);
          gint64 current_time_cogl = cogl_get_clock_time (context);
          gint64 now = g_get_monotonic_time ();

          stage_cogl->last_presentation_time =
            now + (presentation_time_cogl - current_time_cogl) / 1000;
        }

      stage_cogl->refresh_rate = frame_info->refresh_rate;
    }

  _clutter_stage_presented (stage_cogl->wrapper, frame_event, frame_info);
}

static gboolean
clutter_stage_cogl_realize (ClutterStageWindow *stage_window)
{
  ClutterBackend *backend;

  CLUTTER_NOTE (BACKEND, "Realizing stage '%s' [%p]",
                G_OBJECT_TYPE_NAME (stage_window),
                stage_window);

  backend = clutter_get_default_backend ();

  if (backend->cogl_context == NULL)
    {
      g_warning ("Failed to realize stage: missing Cogl context");
      return FALSE;
    }

  return TRUE;
}

static void
clutter_stage_cogl_schedule_update (ClutterStageWindow *stage_window,
                                    gint                sync_delay)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);
  gint64 now;
  float refresh_rate;
  gint64 refresh_interval;

  if (stage_cogl->update_time != -1)
    return;

  now = g_get_monotonic_time ();

  if (sync_delay < 0)
    {
      stage_cogl->update_time = now;
      return;
    }

  /* We only extrapolate presentation times for 150ms  - this is somewhat
   * arbitrary. The reasons it might not be accurate for larger times are
   * that the refresh interval might be wrong or the vertical refresh
   * might be downclocked if nothing is going on onscreen.
   */
  if (stage_cogl->last_presentation_time == 0||
      stage_cogl->last_presentation_time < now - 150000)
    {
      stage_cogl->update_time = now;
      return;
    }

  refresh_rate = stage_cogl->refresh_rate;
  if (refresh_rate == 0.0)
    refresh_rate = 60.0;

  refresh_interval = (gint64) (0.5 + 1000000 / refresh_rate);
  if (refresh_interval == 0)
    refresh_interval = 16667; /* 1/60th second */

  stage_cogl->update_time = stage_cogl->last_presentation_time + 1000 * sync_delay;

  while (stage_cogl->update_time < now)
    stage_cogl->update_time += refresh_interval;
}

static gint64
clutter_stage_cogl_get_update_time (ClutterStageWindow *stage_window)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  if (stage_cogl->pending_swaps)
    return -1; /* in the future, indefinite */

  return stage_cogl->update_time;
}

static void
clutter_stage_cogl_clear_update_time (ClutterStageWindow *stage_window)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  stage_cogl->update_time = -1;
}

static ClutterActor *
clutter_stage_cogl_get_wrapper (ClutterStageWindow *stage_window)
{
  return CLUTTER_ACTOR (CLUTTER_STAGE_COGL (stage_window)->wrapper);
}

static void
clutter_stage_cogl_show (ClutterStageWindow *stage_window,
			 gboolean            do_raise)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  clutter_actor_map (CLUTTER_ACTOR (stage_cogl->wrapper));
}

static void
clutter_stage_cogl_hide (ClutterStageWindow *stage_window)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  clutter_actor_unmap (CLUTTER_ACTOR (stage_cogl->wrapper));
}

static void
clutter_stage_cogl_resize (ClutterStageWindow *stage_window,
                           gint                width,
                           gint                height)
{
}

static gboolean
clutter_stage_cogl_has_redraw_clips (ClutterStageWindow *stage_window)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  /* NB: at the start of each new frame there is an implied clip that
   * clips everything (i.e. nothing would be drawn) so we need to make
   * sure we return True in the un-initialized case here.
   *
   * NB: a clip width of 0 means a full stage redraw has been queued
   * so we effectively don't have any redraw clips in that case.
   */
  if (!stage_cogl->initialized_redraw_clip ||
      (stage_cogl->initialized_redraw_clip &&
       stage_cogl->bounding_redraw_clip.width != 0))
    return TRUE;
  else
    return FALSE;
}

static gboolean
clutter_stage_cogl_ignoring_redraw_clips (ClutterStageWindow *stage_window)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  /* NB: a clip width of 0 means a full stage redraw is required */
  if (stage_cogl->initialized_redraw_clip &&
      stage_cogl->bounding_redraw_clip.width == 0)
    return TRUE;
  else
    return FALSE;
}

/* A redraw clip represents (in stage coordinates) the bounding box of
 * something that needs to be redraw. Typically they are added to the
 * StageWindow as a result of clutter_actor_queue_clipped_redraw() by
 * actors such as ClutterGLXTexturePixmap. All redraw clips are
 * discarded after the next paint.
 *
 * A NULL stage_clip means the whole stage needs to be redrawn.
 *
 * What we do with this information:
 * - we keep track of the bounding box for all redraw clips
 * - when we come to redraw; we scissor the redraw to that box and use
 *   glBlitFramebuffer to present the redraw to the front
 *   buffer.
 */
static void
clutter_stage_cogl_add_redraw_clip (ClutterStageWindow    *stage_window,
                                    cairo_rectangle_int_t *stage_clip)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  /* If we are already forced to do a full stage redraw then bail early */
  if (clutter_stage_cogl_ignoring_redraw_clips (stage_window))
    return;

  /* A NULL stage clip means a full stage redraw has been queued and
   * we keep track of this by setting a zero width
   * stage_cogl->bounding_redraw_clip */
  if (stage_clip == NULL)
    {
      stage_cogl->bounding_redraw_clip.width = 0;
      stage_cogl->initialized_redraw_clip = TRUE;
      return;
    }

  /* Ignore requests to add degenerate/empty clip rectangles */
  if (stage_clip->width == 0 || stage_clip->height == 0)
    return;

  if (!stage_cogl->initialized_redraw_clip)
    {
      stage_cogl->bounding_redraw_clip = *stage_clip;
    }
  else if (stage_cogl->bounding_redraw_clip.width > 0)
    {
      _clutter_util_rectangle_union (&stage_cogl->bounding_redraw_clip,
                                     stage_clip,
                                     &stage_cogl->bounding_redraw_clip);
    }

  stage_cogl->initialized_redraw_clip = TRUE;
}

static gboolean
clutter_stage_cogl_get_redraw_clip_bounds (ClutterStageWindow    *stage_window,
                                           cairo_rectangle_int_t *stage_clip)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);

  if (stage_cogl->using_clipped_redraw)
    {
      *stage_clip = stage_cogl->bounding_redraw_clip;

      return TRUE;
    }

  return FALSE;
}

static inline gboolean
valid_buffer_age (ClutterStageViewCogl *view_cogl,
                  int                   age)
{
  ClutterStageViewCoglPrivate *view_priv =
    clutter_stage_view_cogl_get_instance_private (view_cogl);

  if (age <= 0)
    return FALSE;

  return age < MIN (view_priv->damage_index, DAMAGE_HISTORY_MAX);
}

static gboolean
swap_framebuffer (ClutterStageWindow    *stage_window,
                  ClutterStageView      *view,
                  cairo_rectangle_int_t *swap_region,
                  gboolean               swap_with_damage)
{
  CoglFramebuffer *framebuffer = clutter_stage_view_get_onscreen (view);
  int damage[4], ndamage;

  damage[0] = swap_region->x;
  damage[1] = swap_region->y;
  damage[2] = swap_region->width;
  damage[3] = swap_region->height;

  if (swap_region->width != 0)
    ndamage = 1;
  else
    ndamage = 0;

  if (cogl_is_onscreen (framebuffer))
    {
      CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);

      /* push on the screen */
      if (ndamage == 1 && !swap_with_damage)
        {
          CLUTTER_NOTE (BACKEND,
                        "cogl_onscreen_swap_region (onscreen: %p, "
                        "x: %d, y: %d, "
                        "width: %d, height: %d)",
                        onscreen,
                        damage[0], damage[1], damage[2], damage[3]);

          cogl_onscreen_swap_region (onscreen,
                                     damage, ndamage);

          return FALSE;
        }
      else
        {
          CLUTTER_NOTE (BACKEND, "cogl_onscreen_swap_buffers (onscreen: %p)",
                        onscreen);

          cogl_onscreen_swap_buffers_with_damage (onscreen,
                                                  damage, ndamage);

          return TRUE;
        }
    }
  else
    {
      CLUTTER_NOTE (BACKEND, "cogl_framebuffer_finish (framebuffer: %p)",
                    framebuffer);
      cogl_framebuffer_finish (framebuffer);

      return FALSE;
    }
}

static void
paint_stage (ClutterStageCogl            *stage_cogl,
             ClutterStageView            *view,
             const cairo_rectangle_int_t *clip)
{
  ClutterStage *stage = stage_cogl->wrapper;

  _clutter_stage_maybe_setup_viewport (stage, view);
  _clutter_stage_paint_view (stage, view, clip);

  if (clutter_stage_view_get_onscreen (view) !=
      clutter_stage_view_get_framebuffer (view))
    {
      clutter_stage_view_blit_offscreen (view, clip);
    }
}

static void
fill_current_damage_history_and_step (ClutterStageView *view)
{
  ClutterStageViewCogl *view_cogl = CLUTTER_STAGE_VIEW_COGL (view);
  ClutterStageViewCoglPrivate *view_priv =
    clutter_stage_view_cogl_get_instance_private (view_cogl);
  cairo_rectangle_int_t view_rect;
  float fb_scale;
  cairo_rectangle_int_t *current_fb_damage;

  current_fb_damage =
    &view_priv->damage_history[DAMAGE_HISTORY (view_priv->damage_index)];
  clutter_stage_view_get_layout (view, &view_rect);
  fb_scale = clutter_stage_view_get_scale (view);

  *current_fb_damage = (cairo_rectangle_int_t) {
    .x = 0,
    .y = 0,
    .width = view_rect.width * fb_scale,
    .height = view_rect.height * fb_scale
  };
  view_priv->damage_index++;
}

static void
transform_swap_region_to_onscreen (ClutterStageView      *view,
                                   cairo_rectangle_int_t *swap_region)
{
  CoglFramebuffer *framebuffer;
  cairo_rectangle_int_t layout;
  gfloat x1, y1, x2, y2;
  gint width, height;

  framebuffer = clutter_stage_view_get_onscreen (view);
  clutter_stage_view_get_layout (view, &layout);

  x1 = (float) swap_region->x / layout.width;
  y1 = (float) swap_region->y / layout.height;
  x2 = (float) (swap_region->x + swap_region->width) / layout.width;
  y2 = (float) (swap_region->y + swap_region->height) / layout.height;

  clutter_stage_view_transform_to_onscreen (view, &x1, &y1);
  clutter_stage_view_transform_to_onscreen (view, &x2, &y2);

  width = cogl_framebuffer_get_width (framebuffer);
  height = cogl_framebuffer_get_height (framebuffer);

  x1 = floor (x1 * width);
  y1 = floor (height - (y1 * height));
  x2 = ceil (x2 * width);
  y2 = ceil (height - (y2 * height));

  *swap_region = (cairo_rectangle_int_t) {
    .x = x1,
    .y = y1,
    .width = x2 - x1,
    .height = y2 - y1
  };
}

static void
calculate_scissor_region (cairo_rectangle_int_t *fb_clip_region,
                          int                    subpixel_compensation,
                          int                    fb_width,
                          int                    fb_height,
                          cairo_rectangle_int_t *out_scissor_rect)
{
  int scissor_x;
  int scissor_y;
  int scissor_width;
  int scissor_height;

  scissor_x = fb_clip_region->x;
  scissor_y = fb_clip_region->y;
  scissor_width = fb_clip_region->width;
  scissor_height = fb_clip_region->height;

  if (fb_clip_region->x > 0)
    scissor_x += subpixel_compensation;
  if (fb_clip_region->y > 0)
    scissor_y += subpixel_compensation;
  if (fb_clip_region->x + fb_clip_region->width < fb_width)
    scissor_width -= 2 * subpixel_compensation;
  if (fb_clip_region->y + fb_clip_region->height < fb_height)
    scissor_height -= 2 * subpixel_compensation;

  *out_scissor_rect = (cairo_rectangle_int_t) {
    .x = scissor_x,
    .y = scissor_y,
    .width = scissor_width,
    .height = scissor_height
  };
}

static gboolean
clutter_stage_cogl_redraw_view (ClutterStageWindow *stage_window,
                                ClutterStageView   *view)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);
  ClutterStageViewCogl *view_cogl = CLUTTER_STAGE_VIEW_COGL (view);
  ClutterStageViewCoglPrivate *view_priv =
    clutter_stage_view_cogl_get_instance_private (view_cogl);
  CoglFramebuffer *fb = clutter_stage_view_get_framebuffer (view);
  cairo_rectangle_int_t view_rect;
  gboolean have_clip;
  gboolean may_use_clipped_redraw;
  gboolean use_clipped_redraw;
  gboolean can_blit_sub_buffer;
  gboolean has_buffer_age;
  gboolean do_swap_buffer;
  gboolean swap_with_damage;
  ClutterActor *wrapper;
  cairo_rectangle_int_t redraw_clip;
  cairo_rectangle_int_t swap_region;
  cairo_rectangle_int_t fb_clip_region;
  gboolean clip_region_empty;
  float fb_scale;
  int subpixel_compensation = 0;
  int fb_width, fb_height;

  wrapper = CLUTTER_ACTOR (stage_cogl->wrapper);

  clutter_stage_view_get_layout (view, &view_rect);
  fb_scale = clutter_stage_view_get_scale (view);
  fb_width = cogl_framebuffer_get_width (fb);
  fb_height = cogl_framebuffer_get_height (fb);

  can_blit_sub_buffer =
    cogl_is_onscreen (fb) &&
    cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_REGION);

  has_buffer_age =
    cogl_is_onscreen (fb) &&
    cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_BUFFER_AGE);

  /* NB: a zero width redraw clip == full stage redraw */
  if (stage_cogl->bounding_redraw_clip.width == 0)
    have_clip = FALSE;
  else
    {
      redraw_clip = stage_cogl->bounding_redraw_clip;
      _clutter_util_rectangle_intersection (&redraw_clip,
                                            &view_rect,
                                            &redraw_clip);

      have_clip = !(redraw_clip.x == view_rect.x &&
                    redraw_clip.y == view_rect.y &&
                    redraw_clip.width == view_rect.width &&
                    redraw_clip.height == view_rect.height);
    }

  may_use_clipped_redraw = FALSE;
  if (_clutter_stage_window_can_clip_redraws (stage_window) &&
      (can_blit_sub_buffer || has_buffer_age) &&
      have_clip &&
      /* some drivers struggle to get going and produce some junk
       * frames when starting up... */
      cogl_onscreen_get_frame_counter (COGL_ONSCREEN (fb)) > 3)
    {
      may_use_clipped_redraw = TRUE;

      if (fb_scale != floorf (fb_scale))
        subpixel_compensation = ceilf (fb_scale);

      fb_clip_region = (cairo_rectangle_int_t) {
        .x = (floorf ((redraw_clip.x - view_rect.x) * fb_scale) -
              subpixel_compensation),
        .y = (floorf ((redraw_clip.y - view_rect.y) * fb_scale) -
              subpixel_compensation),
        .width = (ceilf (redraw_clip.width * fb_scale) +
                  (2 * subpixel_compensation)),
        .height = (ceilf (redraw_clip.height * fb_scale) +
                   (2 * subpixel_compensation))
      };
    }
  else
    {
      fb_clip_region = (cairo_rectangle_int_t) { 0 };
    }

  if (may_use_clipped_redraw &&
      G_LIKELY (!(clutter_paint_debug_flags & CLUTTER_DEBUG_DISABLE_CLIPPED_REDRAWS)))
    use_clipped_redraw = TRUE;
  else
    use_clipped_redraw = FALSE;

  clip_region_empty = may_use_clipped_redraw && fb_clip_region.width == 0;

  swap_with_damage = FALSE;
  if (has_buffer_age)
    {
      if (use_clipped_redraw && !clip_region_empty)
        {
          int age, i;
          cairo_rectangle_int_t *current_fb_damage =
            &view_priv->damage_history[DAMAGE_HISTORY (view_priv->damage_index++)];

          age = cogl_onscreen_get_buffer_age (COGL_ONSCREEN (fb));

          if (valid_buffer_age (view_cogl, age))
            {
              cairo_rectangle_int_t damage_region;

              *current_fb_damage = fb_clip_region;

              for (i = 1; i <= age; i++)
                {
                  cairo_rectangle_int_t *fb_damage =
                    &view_priv->damage_history[DAMAGE_HISTORY (view_priv->damage_index - i - 1)];

                  _clutter_util_rectangle_union (&fb_clip_region,
                                                 fb_damage,
                                                 &fb_clip_region);
                }

              /* Update the bounding redraw clip state with the extra damage. */
              damage_region = (cairo_rectangle_int_t) {
                .x = view_rect.x + floorf (fb_clip_region.x / fb_scale),
                .y = view_rect.y + floorf (fb_clip_region.y / fb_scale),
                .width = ceilf (fb_clip_region.width / fb_scale),
                .height = ceilf (fb_clip_region.height / fb_scale)
              };
              _clutter_util_rectangle_union (&stage_cogl->bounding_redraw_clip,
                                             &damage_region,
                                             &stage_cogl->bounding_redraw_clip);

              CLUTTER_NOTE (CLIPPING, "Reusing back buffer(age=%d) - repairing region: x=%d, y=%d, width=%d, height=%d\n",
                            age,
                            fb_clip_region.x,
                            fb_clip_region.y,
                            fb_clip_region.width,
                            fb_clip_region.height);

              swap_with_damage = TRUE;
            }
          else
            {
              CLUTTER_NOTE (CLIPPING, "Invalid back buffer(age=%d): forcing full redraw\n", age);
              use_clipped_redraw = FALSE;
              *current_fb_damage = (cairo_rectangle_int_t) {
                .x = 0,
                .y = 0,
                .width = view_rect.width * fb_scale,
                .height = view_rect.height * fb_scale
              };
            }
        }
      else if (!use_clipped_redraw)
        {
          fill_current_damage_history_and_step (view);
        }
    }

  cogl_push_framebuffer (fb);
  if (use_clipped_redraw && clip_region_empty)
    {
      CLUTTER_NOTE (CLIPPING, "Empty stage output paint\n");
    }
  else if (use_clipped_redraw)
    {
      cairo_rectangle_int_t scissor_rect;

      calculate_scissor_region (&fb_clip_region,
                                subpixel_compensation,
                                fb_width, fb_height,
                                &scissor_rect);

      CLUTTER_NOTE (CLIPPING,
                    "Stage clip pushed: x=%d, y=%d, width=%d, height=%d\n",
                    scissor_rect.x,
                    scissor_rect.y,
                    scissor_rect.width,
                    scissor_rect.height);

      stage_cogl->using_clipped_redraw = TRUE;

      cogl_framebuffer_push_scissor_clip (fb,
                                          scissor_rect.x,
                                          scissor_rect.y,
                                          scissor_rect.width,
                                          scissor_rect.height);
      paint_stage (stage_cogl, view,
                   &(cairo_rectangle_int_t) {
                     .x = view_rect.x + floorf ((fb_clip_region.x - 0) / fb_scale),
                     .y = view_rect.y + floorf ((fb_clip_region.y - 0) / fb_scale),
                     .width = ceilf ((fb_clip_region.width + 0) / fb_scale),
                     .height = ceilf ((fb_clip_region.height + 0) / fb_scale)
                   });
      cogl_framebuffer_pop_clip (fb);

      stage_cogl->using_clipped_redraw = FALSE;
    }
  else
    {
      CLUTTER_NOTE (CLIPPING, "Unclipped stage paint\n");

      /* If we are trying to debug redraw issues then we want to pass
       * the bounding_redraw_clip so it can be visualized */
      if (G_UNLIKELY (clutter_paint_debug_flags & CLUTTER_DEBUG_DISABLE_CLIPPED_REDRAWS) &&
          may_use_clipped_redraw &&
          !clip_region_empty)
        {
          cairo_rectangle_int_t scissor_rect;

          calculate_scissor_region (&fb_clip_region,
                                    subpixel_compensation,
                                    fb_width, fb_height,
                                    &scissor_rect);

          cogl_framebuffer_push_scissor_clip (fb,
                                              scissor_rect.x,
                                              scissor_rect.y,
                                              scissor_rect.width,
                                              scissor_rect.height);
          paint_stage (stage_cogl, view,
                       &(cairo_rectangle_int_t) {
                         .x = view_rect.x + floorf (fb_clip_region.x / fb_scale),
                         .y = view_rect.y + floorf (fb_clip_region.y / fb_scale),
                         .width = ceilf (fb_clip_region.width / fb_scale),
                         .height = ceilf (fb_clip_region.height / fb_scale)
                       });
          cogl_framebuffer_pop_clip (fb);
        }
      else
        paint_stage (stage_cogl, view, &view_rect);
    }
  cogl_pop_framebuffer ();

  if (may_use_clipped_redraw &&
      G_UNLIKELY ((clutter_paint_debug_flags & CLUTTER_DEBUG_REDRAWS)))
    {
      CoglContext *ctx = cogl_framebuffer_get_context (fb);
      static CoglPipeline *outline = NULL;
      ClutterActor *actor = CLUTTER_ACTOR (wrapper);
      float x_1 = redraw_clip.x;
      float x_2 = redraw_clip.x + redraw_clip.width;
      float y_1 = redraw_clip.y;
      float y_2 = redraw_clip.y + redraw_clip.height;
      CoglVertexP2 quad[4] = {
        { x_1, y_1 },
        { x_2, y_1 },
        { x_2, y_2 },
        { x_1, y_2 }
      };
      CoglPrimitive *prim;
      CoglMatrix modelview;

      if (outline == NULL)
        {
          outline = cogl_pipeline_new (ctx);
          cogl_pipeline_set_color4ub (outline, 0xff, 0x00, 0x00, 0xff);
        }

      prim = cogl_primitive_new_p2 (ctx,
                                    COGL_VERTICES_MODE_LINE_LOOP,
                                    4, /* n_vertices */
                                    quad);

      cogl_framebuffer_push_matrix (fb);
      cogl_matrix_init_identity (&modelview);
      _clutter_actor_apply_modelview_transform (actor, &modelview);
      cogl_framebuffer_set_modelview_matrix (fb, &modelview);
      cogl_framebuffer_draw_primitive (fb, outline, prim);
      cogl_framebuffer_pop_matrix (fb);
      cogl_object_unref (prim);
    }

  /* XXX: It seems there will be a race here in that the stage
   * window may be resized before the cogl_onscreen_swap_region
   * is handled and so we may copy the wrong region. I can't
   * really see how we can handle this with the current state of X
   * but at least in this case a full redraw should be queued by
   * the resize anyway so it should only exhibit temporary
   * artefacts.
   */
  if (use_clipped_redraw)
    {
      if (use_clipped_redraw && clip_region_empty)
        {
          do_swap_buffer = FALSE;
        }
      else if (use_clipped_redraw)
        {
          swap_region = fb_clip_region;
          g_assert (swap_region.width > 0);
          do_swap_buffer = TRUE;
        }
      else
        {
          swap_region = (cairo_rectangle_int_t) {
            .x = 0,
            .y = 0,
            .width = view_rect.width * fb_scale,
            .height = view_rect.height * fb_scale,
          };
          do_swap_buffer = TRUE;
        }
    }
  else
    {
      swap_region = (cairo_rectangle_int_t) { 0 };
      do_swap_buffer = TRUE;
    }

  if (do_swap_buffer)
    {
      if (clutter_stage_view_get_onscreen (view) !=
          clutter_stage_view_get_framebuffer (view))
        {
          transform_swap_region_to_onscreen (view, &swap_region);
        }

      return swap_framebuffer (stage_window,
                               view,
                               &swap_region,
                               swap_with_damage);
    }
  else
    {
      return FALSE;
    }
}

static void
clutter_stage_cogl_redraw (ClutterStageWindow *stage_window)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_window);
  gboolean swap_event = FALSE;
  GList *l;

  for (l = _clutter_stage_window_get_views (stage_window); l; l = l->next)
    {
      ClutterStageView *view = l->data;

      swap_event =
        clutter_stage_cogl_redraw_view (stage_window, view) || swap_event;
    }

  _clutter_stage_window_finish_frame (stage_window);

  if (swap_event)
    {
      /* If we have swap buffer events then cogl_onscreen_swap_buffers
       * will return immediately and we need to track that there is a
       * swap in progress... */
      if (clutter_feature_available (CLUTTER_FEATURE_SWAP_EVENTS))
        stage_cogl->pending_swaps++;
    }

  /* reset the redraw clipping for the next paint... */
  stage_cogl->initialized_redraw_clip = FALSE;

  stage_cogl->frame_count++;
}

static void
clutter_stage_cogl_get_dirty_pixel (ClutterStageWindow *stage_window,
                                    ClutterStageView   *view,
                                    int                *x,
                                    int                *y)
{
  CoglFramebuffer *framebuffer = clutter_stage_view_get_framebuffer (view);
  gboolean has_buffer_age =
    cogl_is_onscreen (framebuffer) &&
    cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_BUFFER_AGE);
  float fb_scale;
  gboolean scale_is_fractional;

  fb_scale = clutter_stage_view_get_scale (view);
  if (fb_scale != floorf (fb_scale))
    scale_is_fractional = TRUE;
  else
    scale_is_fractional = FALSE;

  /*
   * Buffer damage is tracked in the framebuffer coordinate space
   * using the damage history. When fractional scaling is used, a
   * coordinate on the stage might not correspond to the exact position of any
   * physical pixel, which causes issues when painting using the pick mode.
   *
   * For now, always use the (0, 0) pixel for picking when using fractional
   * framebuffer scaling.
   */
  if (!has_buffer_age || scale_is_fractional)
    {
      *x = 0;
      *y = 0;
    }
  else
    {
      ClutterStageViewCogl *view_cogl = CLUTTER_STAGE_VIEW_COGL (view);
      ClutterStageViewCoglPrivate *view_priv =
        clutter_stage_view_cogl_get_instance_private (view_cogl);
      cairo_rectangle_int_t view_layout;
      cairo_rectangle_int_t *fb_damage;

      clutter_stage_view_get_layout (view, &view_layout);

      fb_damage = &view_priv->damage_history[DAMAGE_HISTORY (view_priv->damage_index - 1)];
      *x = fb_damage->x / fb_scale;
      *y = fb_damage->y / fb_scale;
    }
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->realize = clutter_stage_cogl_realize;
  iface->unrealize = clutter_stage_cogl_unrealize;
  iface->get_wrapper = clutter_stage_cogl_get_wrapper;
  iface->resize = clutter_stage_cogl_resize;
  iface->show = clutter_stage_cogl_show;
  iface->hide = clutter_stage_cogl_hide;
  iface->schedule_update = clutter_stage_cogl_schedule_update;
  iface->get_update_time = clutter_stage_cogl_get_update_time;
  iface->clear_update_time = clutter_stage_cogl_clear_update_time;
  iface->add_redraw_clip = clutter_stage_cogl_add_redraw_clip;
  iface->has_redraw_clips = clutter_stage_cogl_has_redraw_clips;
  iface->ignoring_redraw_clips = clutter_stage_cogl_ignoring_redraw_clips;
  iface->get_redraw_clip_bounds = clutter_stage_cogl_get_redraw_clip_bounds;
  iface->redraw = clutter_stage_cogl_redraw;
  iface->get_dirty_pixel = clutter_stage_cogl_get_dirty_pixel;
}

static void
clutter_stage_cogl_set_property (GObject      *gobject,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
  ClutterStageCogl *self = CLUTTER_STAGE_COGL (gobject);

  switch (prop_id)
    {
    case PROP_WRAPPER:
      self->wrapper = g_value_get_object (value);
      break;

    case PROP_BACKEND:
      self->backend = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
_clutter_stage_cogl_class_init (ClutterStageCoglClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = clutter_stage_cogl_set_property;

  g_object_class_override_property (gobject_class, PROP_WRAPPER, "wrapper");
  g_object_class_override_property (gobject_class, PROP_BACKEND, "backend");
}

static void
_clutter_stage_cogl_init (ClutterStageCogl *stage)
{
  stage->last_presentation_time = 0;
  stage->refresh_rate = 0.0;

  stage->update_time = -1;
}

static void
clutter_stage_view_cogl_init (ClutterStageViewCogl *view_cogl)
{
}

static void
clutter_stage_view_cogl_class_init (ClutterStageViewCoglClass *klass)
{
}
