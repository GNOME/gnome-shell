/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 * Authors:
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-texture.h"
#include "cogl-matrix.h"
#include "cogl-spans.h"
#include "cogl-meta-texture.h"
#include "cogl-texture-rectangle-private.h"

#include <string.h>
#include <math.h>

typedef struct _ForeachData
{
  float meta_region_coords[4];
  CoglPipelineWrapMode wrap_s;
  CoglPipelineWrapMode wrap_t;
  CoglMetaTextureCallback callback;
  void *user_data;

  int width;
  int height;

  CoglTexture *padded_textures[9];
  const float *grid_slice_texture_coords;
  float slice_offset_s;
  float slice_offset_t;
  float slice_range_s;
  float slice_range_t;
} ForeachData;

static void
padded_grid_repeat_cb (CoglTexture *slice_texture,
                       const float *slice_texture_coords,
                       const float *meta_coords,
                       void *user_data)
{
  ForeachData *data;
  float mapped_coords[4];

  /* Ignore padding slices for the current grid */
  if (!slice_texture)
    return;

  data = user_data;

  /* NB: the slice_texture_coords[] we get here will always be
   * normalized.
   *
   * We now need to map the normalized slice_texture_coords[] we have
   * here back to the real slice coordinates we saved in the previous
   * stage...
   */
  mapped_coords[0] =
    slice_texture_coords[0] * data->slice_range_s + data->slice_offset_s;
  mapped_coords[1] =
    slice_texture_coords[1] * data->slice_range_t + data->slice_offset_t;
  mapped_coords[2] =
    slice_texture_coords[2] * data->slice_range_s + data->slice_offset_s;
  mapped_coords[3] =
    slice_texture_coords[3] * data->slice_range_t + data->slice_offset_t;

  data->callback (slice_texture,
                  mapped_coords, meta_coords, data->user_data);
}

static int
setup_padded_spans (CoglSpan *spans,
                    float start,
                    float end,
                    float range,
                    int *real_index)
{
  int span_index = 0;

  if (start > 0)
    {
      spans[0].start = 0;
      spans[0].size = start;
      spans[0].waste = 0;
      span_index++;
      spans[1].start = spans[0].size;
    }
  else
    spans[span_index].start = 0;

  spans[span_index].size = end - start;
  spans[span_index].waste = 0;
  *real_index = span_index;
  span_index++;

  if (end < range)
    {
      spans[span_index].start =
        spans[span_index - 1].start + spans[span_index - 1].size;
      spans[span_index].size = range - end;
      spans[span_index].waste = 0;
      span_index++;
    }

  return span_index;
}

/* This handles each sub-texture within the range [0,1] of our
 * original meta texture and repeats each one separately across the
 * users requested virtual texture coordinates.
 *
 * A notable advantage of this approach is that we will batch
 * together callbacks corresponding to the same underlying slice
 * together.
 */
static void
create_grid_and_repeat_cb (CoglTexture *slice_texture,
                           const float *slice_texture_coords,
                           const float *meta_coords,
                           void *user_data)
{
  ForeachData *data = user_data;
  CoglSpan x_spans[3];
  int n_x_spans;
  int x_real_index;
  CoglSpan y_spans[3];
  int n_y_spans;
  int y_real_index;

  /* NB: This callback is called for each slice of the meta-texture
   * in the range [0,1].
   *
   * We define a "padded grid" for each slice of the meta-texture in
   * the range [0,1]. The x axis and y axis grid lines are defined
   * using CoglSpans.
   *
   * The padded grid maps over the meta-texture coordinates in the
   * range [0,1] but only contains one valid cell that corresponds to
   * current slice being iterated and all the surrounding cells just
   * provide padding.
   *
   * Once we've defined our padded grid we then repeat that across
   * the user's original region, calling their callback whenever
   * we see our current slice - ignoring padding.
   *
   * NB: we can assume meta_coords[] are normalized at this point
   * since TextureRectangles aren't iterated with this code-path.
   *
   * NB: spans are always defined using non-normalized coordinates
   */
  n_x_spans = setup_padded_spans (x_spans,
                                  meta_coords[0] * data->width,
                                  meta_coords[2] * data->width,
                                  data->width,
                                  &x_real_index);
  n_y_spans = setup_padded_spans (y_spans,
                                  meta_coords[1] * data->height,
                                  meta_coords[3] * data->height,
                                  data->height,
                                  &y_real_index);

  data->padded_textures[n_x_spans * y_real_index + x_real_index] =
    slice_texture;

  /* Our callback is going to be passed normalized slice texture
   * coordinates, and we will need to map the range [0,1] to the real
   * slice_texture_coords we have here... */
  data->grid_slice_texture_coords = slice_texture_coords;
  data->slice_range_s = fabs (data->grid_slice_texture_coords[2] -
                              data->grid_slice_texture_coords[0]);
  data->slice_range_t = fabs (data->grid_slice_texture_coords[3] -
                              data->grid_slice_texture_coords[1]);
  data->slice_offset_s = MIN (data->grid_slice_texture_coords[0],
                              data->grid_slice_texture_coords[2]);
  data->slice_offset_t = MIN (data->grid_slice_texture_coords[1],
                              data->grid_slice_texture_coords[3]);

  /* Now actually iterate the region the user originally requested
   * using the current padded grid */
  _cogl_texture_spans_foreach_in_region (x_spans,
                                         n_x_spans,
                                         y_spans,
                                         n_y_spans,
                                         data->padded_textures,
                                         data->meta_region_coords,
                                         data->width,
                                         data->height,
                                         data->wrap_s,
                                         data->wrap_t,
                                         padded_grid_repeat_cb,
                                         data);

  /* Clear the padded_textures ready for the next iteration */
  data->padded_textures[n_x_spans * y_real_index + x_real_index] = NULL;
}

#define SWAP(A,B) do { float tmp = B; B = A; A = tmp; } while (0)

typedef struct _ClampData
{
  float start;
  float end;
  CoglBool s_flipped;
  CoglBool t_flipped;
  CoglMetaTextureCallback callback;
  void *user_data;
} ClampData;

static void
clamp_s_cb (CoglTexture *sub_texture,
            const float *sub_texture_coords,
            const float *meta_coords,
            void *user_data)
{
  ClampData *clamp_data = user_data;
  float mapped_meta_coords[4] = {
    clamp_data->start,
    meta_coords[1],
    clamp_data->end,
    meta_coords[3]
  };
  if (clamp_data->s_flipped)
    SWAP (mapped_meta_coords[0], mapped_meta_coords[2]);
  /* NB: we never need to flip the t coords when dealing with
   * s-axis clamping so no need to check if ->t_flipped */
  clamp_data->callback (sub_texture,
                        sub_texture_coords, mapped_meta_coords,
                        clamp_data->user_data);
}

static void
clamp_t_cb (CoglTexture *sub_texture,
            const float *sub_texture_coords,
            const float *meta_coords,
            void *user_data)
{
  ClampData *clamp_data = user_data;
  float mapped_meta_coords[4] = {
    meta_coords[0],
    clamp_data->start,
    meta_coords[2],
    clamp_data->end,
  };
  if (clamp_data->s_flipped)
    SWAP (mapped_meta_coords[0], mapped_meta_coords[2]);
  if (clamp_data->t_flipped)
    SWAP (mapped_meta_coords[1], mapped_meta_coords[3]);
  clamp_data->callback (sub_texture,
                        sub_texture_coords, mapped_meta_coords,
                        clamp_data->user_data);
}

static CoglBool
foreach_clamped_region (CoglMetaTexture *meta_texture,
                        float *tx_1,
                        float *ty_1,
                        float *tx_2,
                        float *ty_2,
                        CoglPipelineWrapMode wrap_s,
                        CoglPipelineWrapMode wrap_t,
                        CoglMetaTextureCallback callback,
                        void *user_data)
{
  float width = cogl_texture_get_width (COGL_TEXTURE (meta_texture));
  ClampData clamp_data;

  /* Consider that *tx_1 may be > *tx_2 and to simplify things
   * we just flip them around if that's the case and keep a note
   * of the fact that they are flipped. */
  if (*tx_1 > *tx_2)
    {
      SWAP (*tx_1, *tx_2);
      clamp_data.s_flipped = TRUE;
    }
  else
    clamp_data.s_flipped = FALSE;

  /* The same goes for ty_1 and ty_2... */
  if (*ty_1 > *ty_2)
    {
      SWAP (*ty_1, *ty_2);
      clamp_data.t_flipped = TRUE;
    }
  else
    clamp_data.t_flipped = FALSE;

  clamp_data.callback = callback;
  clamp_data.user_data = user_data;

  if (wrap_s == COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE)
    {
      float max_s_coord;
      float half_texel_width;

      /* Consider that rectangle textures have non-normalized
       * coordinates... */
      if (cogl_is_texture_rectangle (meta_texture))
        max_s_coord = width;
      else
        max_s_coord = 1.0;

      half_texel_width = max_s_coord / (width * 2);

      /* Handle any left clamped region */
      if (*tx_1 < 0)
        {
          clamp_data.start = *tx_1;
          clamp_data.end = MIN (0, *tx_2);;
          cogl_meta_texture_foreach_in_region (meta_texture,
                                               half_texel_width, *ty_1,
                                               half_texel_width, *ty_2,
                                               COGL_PIPELINE_WRAP_MODE_REPEAT,
                                               wrap_t,
                                               clamp_s_cb,
                                               &clamp_data);
          /* Have we handled everything? */
          if (*tx_2 <= 0)
            return TRUE;

          /* clamp tx_1 since we've handled everything with x < 0 */
          *tx_1 = 0;
        }

      /* Handle any right clamped region - including the corners */
      if (*tx_2 > max_s_coord)
        {
          clamp_data.start = MAX (max_s_coord, *tx_1);
          clamp_data.end = *tx_2;
          cogl_meta_texture_foreach_in_region (meta_texture,
                                               max_s_coord - half_texel_width,
                                               *ty_1,
                                               max_s_coord - half_texel_width,
                                               *ty_2,
                                               COGL_PIPELINE_WRAP_MODE_REPEAT,
                                               wrap_t,
                                               clamp_s_cb,
                                               &clamp_data);
          /* Have we handled everything? */
          if (*tx_1 >= max_s_coord)
            return TRUE;

          /* clamp tx_2 since we've handled everything with x >
           * max_s_coord */
          *tx_2 = max_s_coord;
        }
    }

  if (wrap_t == COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE)
    {
      float height = cogl_texture_get_height (COGL_TEXTURE (meta_texture));
      float max_t_coord;
      float half_texel_height;

      /* Consider that rectangle textures have non-normalized
       * coordinates... */
      if (cogl_is_texture_rectangle (meta_texture))
        max_t_coord = height;
      else
        max_t_coord = 1.0;

      half_texel_height = max_t_coord / (height * 2);

      /* Handle any top clamped region */
      if (*ty_1 < 0)
        {
          clamp_data.start = *ty_1;
          clamp_data.end = MIN (0, *ty_2);;
          cogl_meta_texture_foreach_in_region (meta_texture,
                                               *tx_1, half_texel_height,
                                               *tx_2, half_texel_height,
                                               wrap_s,
                                               COGL_PIPELINE_WRAP_MODE_REPEAT,
                                               clamp_t_cb,
                                               &clamp_data);
          /* Have we handled everything? */
          if (*tx_2 <= 0)
            return TRUE;

          /* clamp ty_1 since we've handled everything with y < 0 */
          *ty_1 = 0;
        }

      /* Handle any bottom clamped region */
      if (*ty_2 > max_t_coord)
        {
          clamp_data.start = MAX (max_t_coord, *ty_1);;
          clamp_data.end = *ty_2;
          cogl_meta_texture_foreach_in_region (meta_texture,
                                               *tx_1,
                                               max_t_coord - half_texel_height,
                                               *tx_2,
                                               max_t_coord - half_texel_height,
                                               wrap_s,
                                               COGL_PIPELINE_WRAP_MODE_REPEAT,
                                               clamp_t_cb,
                                               &clamp_data);
          /* Have we handled everything? */
          if (*ty_1 >= max_t_coord)
            return TRUE;

          /* clamp ty_2 since we've handled everything with y >
           * max_t_coord */
          *ty_2 = max_t_coord;
        }
    }

  if (clamp_data.s_flipped)
    SWAP (*tx_1, *tx_2);
  if (clamp_data.t_flipped)
    SWAP (*ty_1, *ty_2);

  return FALSE;
}

typedef struct _NormalizeData
{
  CoglMetaTextureCallback callback;
  void *user_data;
  float s_normalize_factor;
  float t_normalize_factor;
} NormalizeData;

static void
normalize_meta_coords_cb (CoglTexture *slice_texture,
                          const float *slice_coords,
                          const float *meta_coords,
                          void *user_data)
{
  NormalizeData *data = user_data;
  float normalized_meta_coords[4] = {
      meta_coords[0] * data->s_normalize_factor,
      meta_coords[1] * data->t_normalize_factor,
      meta_coords[2] * data->s_normalize_factor,
      meta_coords[3] * data->t_normalize_factor
  };

  data->callback (slice_texture,
                  slice_coords, normalized_meta_coords,
                  data->user_data);
}

typedef struct _UnNormalizeData
{
  CoglMetaTextureCallback callback;
  void *user_data;
  float width;
  float height;
} UnNormalizeData;

static void
un_normalize_slice_coords_cb (CoglTexture *slice_texture,
                              const float *slice_coords,
                              const float *meta_coords,
                              void *user_data)
{
  UnNormalizeData *data = user_data;
  float un_normalized_slice_coords[4] = {
    slice_coords[0] * data->width,
    slice_coords[1] * data->height,
    slice_coords[2] * data->width,
    slice_coords[3] * data->height
  };

  data->callback (slice_texture,
                  un_normalized_slice_coords, meta_coords,
                  data->user_data);
}

void
cogl_meta_texture_foreach_in_region (CoglMetaTexture *meta_texture,
                                     float tx_1,
                                     float ty_1,
                                     float tx_2,
                                     float ty_2,
                                     CoglPipelineWrapMode wrap_s,
                                     CoglPipelineWrapMode wrap_t,
                                     CoglMetaTextureCallback callback,
                                     void *user_data)
{
  CoglTexture *texture = COGL_TEXTURE (meta_texture);
  float width = cogl_texture_get_width (texture);
  float height = cogl_texture_get_height (texture);
  NormalizeData normalize_data;

  if (wrap_s == COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
    wrap_s = COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;
  if (wrap_t == COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
    wrap_t = COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;

  if (wrap_s == COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE ||
      wrap_t == COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE)
    {
      CoglBool finished = foreach_clamped_region (meta_texture,
                                                  &tx_1, &ty_1, &tx_2, &ty_2,
                                                  wrap_s, wrap_t,
                                                  callback,
                                                  user_data);
      if (finished)
        return;

      /* Since clamping has been handled we now want to normalize our
       * wrap modes we se can assume from this point on we don't
       * need to consider CLAMP_TO_EDGE. (NB: The spans code will
       * assert that CLAMP_TO_EDGE isn't requested) */
      if (wrap_s == COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE)
        wrap_s = COGL_PIPELINE_WRAP_MODE_REPEAT;
      if (wrap_t == COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE)
        wrap_t = COGL_PIPELINE_WRAP_MODE_REPEAT;
    }

  /* It makes things simpler to deal with non-normalized region
   * coordinates beyond this point and only re-normalize just before
   * calling the user's callback... */

  if (!cogl_is_texture_rectangle (COGL_TEXTURE (meta_texture)))
    {
      normalize_data.callback = callback;
      normalize_data.user_data = user_data;
      normalize_data.s_normalize_factor = 1.0f / width;
      normalize_data.t_normalize_factor = 1.0f / height;
      callback = normalize_meta_coords_cb;
      user_data = &normalize_data;
      tx_1 *= width;
      ty_1 *= height;
      tx_2 *= width;
      ty_2 *= height;
    }

  /* XXX: at some point this wont be routed through the CoglTexture
   * vtable, instead there will be a separate CoglMetaTexture
   * interface vtable. */

  if (texture->vtable->foreach_sub_texture_in_region)
    {
      ForeachData data;

      data.meta_region_coords[0] = tx_1;
      data.meta_region_coords[1] = ty_1;
      data.meta_region_coords[2] = tx_2;
      data.meta_region_coords[3] = ty_2;
      data.wrap_s = wrap_s;
      data.wrap_t = wrap_t;
      data.callback = callback;
      data.user_data = user_data;

      data.width = width;
      data.height = height;

      memset (data.padded_textures, 0, sizeof (data.padded_textures));

      /*
       * 1) We iterate all the slices of the meta-texture only within
       *    the range [0,1].
       *
       * 2) We define a "padded grid" for each slice of the
       *    meta-texture in the range [0,1].
       *
       *    The padded grid maps over the meta-texture coordinates in
       *    the range [0,1] but only contains one valid cell that
       *    corresponds to current slice being iterated and all the
       *    surrounding cells just provide padding.
       *
       * 3) Once we've defined our padded grid we then repeat that
       *    across the user's original region, calling their callback
       *    whenever we see our current slice - ignoring padding.
       *
       * A notable benefit of this design is that repeating a texture
       * made of multiple slices will result in us repeating each
       * slice in-turn so the user gets repeat callbacks for the same
       * texture batched together. For manual emulation of texture
       * repeats done by drawing geometry this makes it more likely
       * that we can batch geometry.
       */

      texture->vtable->foreach_sub_texture_in_region (texture,
                                                      0, 0, 1, 1,
                                                      create_grid_and_repeat_cb,
                                                      &data);
    }
  else
    {
      CoglSpan x_span = { 0, width, 0 };
      CoglSpan y_span = { 0, height, 0 };
      float meta_region_coords[4] = { tx_1, ty_1, tx_2, ty_2 };
      UnNormalizeData un_normalize_data;

      /* If we are dealing with a CoglTextureRectangle then we need a shim
       * callback that un_normalizes the slice coordinates we get from
       * _cogl_texture_spans_foreach_in_region before passing them to
       * the user's callback. */
      if (cogl_is_texture_rectangle (meta_texture))
        {
          un_normalize_data.callback = callback;
          un_normalize_data.user_data = user_data;
          un_normalize_data.width = width;
          un_normalize_data.height = height;
          callback = un_normalize_slice_coords_cb;
          user_data = &un_normalize_data;
        }

      _cogl_texture_spans_foreach_in_region (&x_span, 1,
                                             &y_span, 1,
                                             &texture,
                                             meta_region_coords,
                                             width,
                                             height,
                                             wrap_s,
                                             wrap_t,
                                             callback,
                                             user_data);
    }
}
#undef SWAP
