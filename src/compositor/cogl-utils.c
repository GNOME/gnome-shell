/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Utilities for use with Cogl
 *
 * Copyright 2010 Red Hat, Inc.
 * Copyright 2010 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <clutter/clutter.h>
#include "cogl-utils.h"

/**
 * meta_create_color_texture_4ub:
 * @red: red component
 * @green: green component
 * @blue: blue component
 * @alpha: alpha component
 * @flags: Optional flags for the texture, or %COGL_TEXTURE_NONE;
 *   %COGL_TEXTURE_NO_SLICING is useful if the texture will be
 *   repeated to create a constant color fill, since hardware
 *   repeat can't be used for a sliced texture.
 *
 * Creates a texture that is a single pixel with the specified
 * unpremultiplied color components.
 *
 * Return value: (transfer full): a newly created Cogl texture
 */
CoglTexture *
meta_create_color_texture_4ub (guint8           red,
                               guint8           green,
                               guint8           blue,
                               guint8           alpha,
                               CoglTextureFlags flags)
{
  CoglColor color;
  guint8 pixel[4];

  cogl_color_init_from_4ub (&color, red, green, blue, alpha);
  cogl_color_premultiply (&color);

  pixel[0] = cogl_color_get_red_byte (&color);
  pixel[1] = cogl_color_get_green_byte (&color);
  pixel[2] = cogl_color_get_blue_byte (&color);
  pixel[3] = cogl_color_get_alpha_byte (&color);

  return cogl_texture_new_from_data (1, 1,
                                     flags,
                                     COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                     COGL_PIXEL_FORMAT_ANY,
                                     4, pixel);
}


/* Based on gnome-shell/src/st/st-private.c:_st_create_texture_material.c */

/**
 * meta_create_texture_pipeline:
 * @src_texture: (allow-none): texture to use initially for the layer
 *
 * Creates a pipeline with a single layer. Using a common template
 * makes it easier for Cogl to share a shader for different uses in
 * Mutter.
 *
 * Return value: (transfer full): a newly created #CoglPipeline
 */
CoglPipeline *
meta_create_texture_pipeline (CoglTexture *src_texture)
{
  static CoglPipeline *texture_pipeline_template = NULL;
  CoglPipeline *pipeline;

  /* The only state used in the pipeline that would affect the shader
     generation is the texture type on the layer. Therefore we create
     a template pipeline which sets this state and all texture
     pipelines are created as a copy of this. That way Cogl can find
     the shader state for the pipeline more quickly by looking at the
     pipeline ancestry instead of resorting to the shader cache. */
  if (G_UNLIKELY (texture_pipeline_template == NULL))
    {
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      texture_pipeline_template = cogl_pipeline_new (ctx);
      cogl_pipeline_set_layer_null_texture (texture_pipeline_template,
                                            0, /* layer */
                                            COGL_TEXTURE_TYPE_2D);
    }

  pipeline = cogl_pipeline_copy (texture_pipeline_template);

  if (src_texture != NULL)
    cogl_pipeline_set_layer_texture (pipeline, 0, src_texture);

  return pipeline;
}
