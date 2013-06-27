/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-util.h"
#include "cogl-blit.h"
#include "cogl-context-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-private.h"
#include "cogl1-context.h"

static const CoglBlitMode *_cogl_blit_default_mode = NULL;

static CoglBool
_cogl_blit_texture_render_begin (CoglBlitData *data)
{
  CoglContext *ctx = data->src_tex->context;
  CoglOffscreen *offscreen;
  CoglFramebuffer *fb;
  CoglPipeline *pipeline;
  unsigned int dst_width, dst_height;
  CoglError *ignore_error = NULL;

  offscreen = _cogl_offscreen_new_with_texture_full
    (data->dst_tex, COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL, 0 /* level */);

  fb = COGL_FRAMEBUFFER (offscreen);
  if (!cogl_framebuffer_allocate (fb, &ignore_error))
    {
      cogl_error_free (ignore_error);
      cogl_object_unref (fb);
      return FALSE;
    }

  data->dest_fb = fb;

  dst_width = cogl_texture_get_width (data->dst_tex);
  dst_height = cogl_texture_get_height (data->dst_tex);

  /* Set up an orthographic projection so we can use pixel
     coordinates to render to the texture */
  cogl_framebuffer_orthographic (fb,
                                 0, 0, dst_width, dst_height,
                                 -1 /* near */, 1 /* far */);

  /* We cache a pipeline used for migrating on to the context so
     that it doesn't have to continuously regenerate a shader
     program */
  if (ctx->blit_texture_pipeline == NULL)
    {
      ctx->blit_texture_pipeline = cogl_pipeline_new (ctx);

      cogl_pipeline_set_layer_filters (ctx->blit_texture_pipeline, 0,
                                       COGL_PIPELINE_FILTER_NEAREST,
                                       COGL_PIPELINE_FILTER_NEAREST);

      /* Disable blending by just directly taking the contents of the
         source texture */
      cogl_pipeline_set_blend (ctx->blit_texture_pipeline,
                               "RGBA = ADD(SRC_COLOR, 0)",
                               NULL);
    }

  pipeline = ctx->blit_texture_pipeline;

  cogl_pipeline_set_layer_texture (pipeline, 0, data->src_tex);

  data->pipeline = pipeline;

  return TRUE;
}

static void
_cogl_blit_texture_render_blit (CoglBlitData *data,
                                int src_x,
                                int src_y,
                                int dst_x,
                                int dst_y,
                                int width,
                                int height)
{
  cogl_framebuffer_draw_textured_rectangle (data->dest_fb,
                                            data->pipeline,
                                            dst_x, dst_y,
                                            dst_x + width,
                                            dst_y + height,
                                            src_x / (float) data->src_width,
                                            src_y / (float) data->src_height,
                                            (src_x + width) /
                                            (float) data->src_width,
                                            (src_y + height) /
                                            (float) data->src_height);
}

static void
_cogl_blit_texture_render_end (CoglBlitData *data)
{
  CoglContext *ctx = data->src_tex->context;

  /* Attach the target texture to the texture render pipeline so that
     we don't keep a reference to the source texture forever. This is
     assuming that the destination texture will live for a long time
     which is currently the case when cogl_blit_* is used from the
     atlas code. It may be better in future to keep around a set of
     dummy 1x1 textures for each texture target that we could bind
     instead. This would also be useful when using a pipeline as a
     hash table key such as for the ARBfp program cache. */
  cogl_pipeline_set_layer_texture (ctx->blit_texture_pipeline, 0,
                                   data->dst_tex);

  cogl_object_unref (data->dest_fb);
}

static CoglBool
_cogl_blit_framebuffer_begin (CoglBlitData *data)
{
  CoglContext *ctx = data->src_tex->context;
  CoglOffscreen *dst_offscreen = NULL, *src_offscreen = NULL;
  CoglFramebuffer *dst_fb, *src_fb;
  CoglError *ignore_error = NULL;

  /* We can only blit between FBOs if both textures are the same
     format and the blit framebuffer extension is supported */
  if ((_cogl_texture_get_format (data->src_tex) & ~COGL_A_BIT) !=
      (_cogl_texture_get_format (data->dst_tex) & ~COGL_A_BIT) ||
      !_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT))
    return FALSE;

  dst_offscreen = _cogl_offscreen_new_with_texture_full
    (data->dst_tex, COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL, 0 /* level */);

  dst_fb = COGL_FRAMEBUFFER (dst_offscreen);
  if (!cogl_framebuffer_allocate (dst_fb, &ignore_error))
    {
      cogl_error_free (ignore_error);
      goto error;
    }

  src_offscreen= _cogl_offscreen_new_with_texture_full
    (data->src_tex,
     COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL,
     0 /* level */);

  src_fb = COGL_FRAMEBUFFER (src_offscreen);
  if (!cogl_framebuffer_allocate (src_fb, &ignore_error))
    {
      cogl_error_free (ignore_error);
      goto error;
    }

  data->src_fb = src_fb;
  data->dest_fb = dst_fb;

  return TRUE;

error:

  if (dst_offscreen)
    cogl_object_unref (dst_offscreen);
  if (src_offscreen)
    cogl_object_unref (src_offscreen);

  return FALSE;
}

static void
_cogl_blit_framebuffer_blit (CoglBlitData *data,
                             int src_x,
                             int src_y,
                             int dst_x,
                             int dst_y,
                             int width,
                             int height)
{
  _cogl_blit_framebuffer (data->src_fb,
                          data->dest_fb,
                          src_x, src_y,
                          dst_x, dst_y,
                          width, height);
}

static void
_cogl_blit_framebuffer_end (CoglBlitData *data)
{
  cogl_object_unref (data->src_fb);
  cogl_object_unref (data->dest_fb);
}

static CoglBool
_cogl_blit_copy_tex_sub_image_begin (CoglBlitData *data)
{
  CoglOffscreen *offscreen;
  CoglFramebuffer *fb;
  CoglError *ignore_error = NULL;

  /* This will only work if the target texture is a CoglTexture2D */
  if (!cogl_is_texture_2d (data->dst_tex))
    return FALSE;

  offscreen = _cogl_offscreen_new_with_texture_full
    (data->src_tex, COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL, 0 /* level */);

  fb = COGL_FRAMEBUFFER (offscreen);
  if (!cogl_framebuffer_allocate (fb, &ignore_error))
    {
      cogl_error_free (ignore_error);
      cogl_object_unref (fb);
      return FALSE;
    }

  data->src_fb = fb;

  return TRUE;
}

static void
_cogl_blit_copy_tex_sub_image_blit (CoglBlitData *data,
                                    int src_x,
                                    int src_y,
                                    int dst_x,
                                    int dst_y,
                                    int width,
                                    int height)
{
  _cogl_texture_2d_copy_from_framebuffer (COGL_TEXTURE_2D (data->dst_tex),
                                          src_x, src_y,
                                          width, height,
                                          data->src_fb,
                                          dst_x, dst_y,
                                          0); /* level */
}

static void
_cogl_blit_copy_tex_sub_image_end (CoglBlitData *data)
{
  cogl_object_unref (data->src_fb);
}

static CoglBool
_cogl_blit_get_tex_data_begin (CoglBlitData *data)
{
  data->format = _cogl_texture_get_format (data->src_tex);
  data->bpp = _cogl_pixel_format_get_bytes_per_pixel (data->format);

  data->image_data = g_malloc (data->bpp * data->src_width *
                               data->src_height);
  cogl_texture_get_data (data->src_tex, data->format,
                         data->src_width * data->bpp, data->image_data);

  return TRUE;
}

static void
_cogl_blit_get_tex_data_blit (CoglBlitData *data,
                              int src_x,
                              int src_y,
                              int dst_x,
                              int dst_y,
                              int width,
                              int height)
{
  CoglError *ignore = NULL;
  int rowstride = data->src_width * data->bpp;
  int offset = rowstride * src_y + src_x * data->bpp;

  _cogl_texture_set_region (data->dst_tex,
                            width, height,
                            data->format,
                            rowstride,
                            data->image_data + offset,
                            dst_x, dst_y,
                            0, /* level */
                            &ignore);
  /* TODO: support chaining up errors during the blit */
}

static void
_cogl_blit_get_tex_data_end (CoglBlitData *data)
{
  g_free (data->image_data);
}

/* These should be specified in order of preference */
static const CoglBlitMode
_cogl_blit_modes[] =
  {
    {
      "texture-render",
      _cogl_blit_texture_render_begin,
      _cogl_blit_texture_render_blit,
      _cogl_blit_texture_render_end
    },
    {
      "framebuffer",
      _cogl_blit_framebuffer_begin,
      _cogl_blit_framebuffer_blit,
      _cogl_blit_framebuffer_end
    },
    {
      "copy-tex-sub-image",
      _cogl_blit_copy_tex_sub_image_begin,
      _cogl_blit_copy_tex_sub_image_blit,
      _cogl_blit_copy_tex_sub_image_end
    },
    {
      "get-tex-data",
      _cogl_blit_get_tex_data_begin,
      _cogl_blit_get_tex_data_blit,
      _cogl_blit_get_tex_data_end
    }
  };

void
_cogl_blit_begin (CoglBlitData *data,
                  CoglTexture *dst_tex,
                  CoglTexture *src_tex)
{
  int i;

  if (_cogl_blit_default_mode == NULL)
    {
      const char *default_mode_string;

      /* Allow the default to be specified with an environment
         variable. For the time being these functions are only used
         when blitting between atlas textures so the environment
         variable is named to be specific to the atlas code. If we
         want to use the code in other places we should create another
         environment variable for each specific use case */
      if ((default_mode_string = g_getenv ("COGL_ATLAS_DEFAULT_BLIT_MODE")))
        {
          for (i = 0; i < G_N_ELEMENTS (_cogl_blit_modes); i++)
            if (!strcmp (_cogl_blit_modes[i].name, default_mode_string))
              {
                _cogl_blit_default_mode = _cogl_blit_modes + i;
                break;
              }

          if (i >= G_N_ELEMENTS (_cogl_blit_modes))
            {
              g_warning ("Unknown blit mode %s", default_mode_string);
              _cogl_blit_default_mode = _cogl_blit_modes;
            }
        }
      else
        /* Default to the first blit mode */
        _cogl_blit_default_mode = _cogl_blit_modes;
    }

  memset (data, 0, sizeof (CoglBlitData));

  data->dst_tex = dst_tex;
  data->src_tex = src_tex;

  data->src_width = cogl_texture_get_width (src_tex);
  data->src_height = cogl_texture_get_height (src_tex);

  /* Try the default blit mode first */
  if (!_cogl_blit_default_mode->begin_func (data))
    {
      COGL_NOTE (ATLAS, "Failed to set up blit mode %s",
                 _cogl_blit_default_mode->name);

      /* Try all of the other modes in order */
      for (i = 0; i < G_N_ELEMENTS (_cogl_blit_modes); i++)
        if (_cogl_blit_modes + i != _cogl_blit_default_mode &&
            _cogl_blit_modes[i].begin_func (data))
          {
            /* Use this mode as the default from now on */
            _cogl_blit_default_mode = _cogl_blit_modes + i;
            break;
          }
        else
          COGL_NOTE (ATLAS,
                     "Failed to set up blit mode %s",
                     _cogl_blit_modes[i].name);

      /* The last blit mode can't fail so this should never happen */
      _COGL_RETURN_IF_FAIL (i < G_N_ELEMENTS (_cogl_blit_modes));
    }

  data->blit_mode = _cogl_blit_default_mode;

  COGL_NOTE (ATLAS, "Setup blit using %s", _cogl_blit_default_mode->name);
}

void
_cogl_blit (CoglBlitData *data,
            int src_x,
            int src_y,
            int dst_x,
            int dst_y,
            int width,
            int height)
{
  data->blit_mode->blit_func (data, src_x, src_y, dst_x, dst_y, width, height);
}

void
_cogl_blit_end (CoglBlitData *data)
{
  data->blit_mode->end_func (data);
}
