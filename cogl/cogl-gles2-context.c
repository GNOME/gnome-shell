/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Tomeu Vizoso <tomeu.vizoso@collabora.com>
 *  Robert Bragg <robert@linux.intel.com>
 *  Neil Roberts <neil@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-gles2.h"
#include "cogl-gles2-context-private.h"

#include "cogl-context-private.h"
#include "cogl-display-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-framebuffer-gl-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-renderer-private.h"
#include "cogl-swap-chain-private.h"
#include "cogl-texture-2d-gl.h"
#include "cogl-texture-2d-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-error-private.h"

static void _cogl_gles2_context_free (CoglGLES2Context *gles2_context);

COGL_OBJECT_DEFINE (GLES2Context, gles2_context);

static CoglGLES2Context *current_gles2_context;

static CoglUserDataKey offscreen_wrapper_key;

/* The application's main function is renamed to this so that we can
 * provide an alternative main function */
#define MAIN_WRAPPER_REPLACEMENT_NAME "_c31"
/* This uniform is used to flip the rendering or not depending on
 * whether we are rendering to an offscreen buffer or not */
#define MAIN_WRAPPER_FLIP_UNIFORM "_cogl_flip_vector"
/* These comments are used to delimit the added wrapper snippet so
 * that we can remove it again when the shader source is requested via
 * glGetShaderSource */
#define MAIN_WRAPPER_BEGIN "/*_COGL_WRAPPER_BEGIN*/"
#define MAIN_WRAPPER_END "/*_COGL_WRAPPER_END*/"

/* This wrapper function around 'main' is appended to every vertex shader
 * so that we can add some extra code to flip the rendering when
 * rendering to an offscreen buffer */
static const char
main_wrapper_function[] =
  MAIN_WRAPPER_BEGIN "\n"
  "uniform vec4 " MAIN_WRAPPER_FLIP_UNIFORM ";\n"
  "\n"
  "void\n"
  "main ()\n"
  "{\n"
  "  " MAIN_WRAPPER_REPLACEMENT_NAME " ();\n"
  "  gl_Position *= " MAIN_WRAPPER_FLIP_UNIFORM ";\n"
  "}\n"
  MAIN_WRAPPER_END;

enum {
  RESTORE_FB_NONE,
  RESTORE_FB_FROM_OFFSCREEN,
  RESTORE_FB_FROM_ONSCREEN,
};

uint32_t
_cogl_gles2_context_error_quark (void)
{
  return g_quark_from_static_string ("cogl-gles2-context-error-quark");
}

static void
shader_data_unref (CoglGLES2Context *context,
                   CoglGLES2ShaderData *shader_data)
{
  if (--shader_data->ref_count < 1)
    /* Removing the hash table entry should also destroy the data */
    g_hash_table_remove (context->shader_map,
                         GINT_TO_POINTER (shader_data->object_id));
}

static void
program_data_unref (CoglGLES2ProgramData *program_data)
{
  if (--program_data->ref_count < 1)
    /* Removing the hash table entry should also destroy the data */
    g_hash_table_remove (program_data->context->program_map,
                         GINT_TO_POINTER (program_data->object_id));
}

static void
detach_shader (CoglGLES2ProgramData *program_data,
               CoglGLES2ShaderData *shader_data)
{
  GList *l;

  for (l = program_data->attached_shaders; l; l = l->next)
    {
      if (l->data == shader_data)
        {
          shader_data_unref (program_data->context, shader_data);
          program_data->attached_shaders =
            g_list_delete_link (program_data->attached_shaders, l);
          break;
        }
    }
}

static CoglBool
is_symbol_character (char ch)
{
  return g_ascii_isalnum (ch) || ch == '_';
}

static void
replace_token (char *string,
               const char *token,
               const char *replacement,
               int length)
{
  char *token_pos;
  char *last_pos = string;
  char *end = string + length;
  int token_length = strlen (token);

  /* NOTE: this assumes token and replacement are the same length */

  while ((token_pos = _cogl_util_memmem (last_pos,
                                         end - last_pos,
                                         token,
                                         token_length)))
    {
      /* Make sure this isn't in the middle of some longer token */
      if ((token_pos <= string ||
           !is_symbol_character (token_pos[-1])) &&
          (token_pos + token_length == end ||
           !is_symbol_character (token_pos[token_length])))
        memcpy (token_pos, replacement, token_length);

      last_pos = token_pos + token_length;
    }
}

static void
update_current_flip_state (CoglGLES2Context *gles2_ctx)
{
  CoglGLES2FlipState new_flip_state;

  if (gles2_ctx->current_fbo_handle == 0 &&
      cogl_is_offscreen (gles2_ctx->write_buffer))
    new_flip_state = COGL_GLES2_FLIP_STATE_FLIPPED;
  else
    new_flip_state = COGL_GLES2_FLIP_STATE_NORMAL;

  /* If the flip state has changed then we need to reflush all of the
   * dependent state */
  if (new_flip_state != gles2_ctx->current_flip_state)
    {
      gles2_ctx->viewport_dirty = TRUE;
      gles2_ctx->scissor_dirty = TRUE;
      gles2_ctx->front_face_dirty = TRUE;
      gles2_ctx->current_flip_state = new_flip_state;
    }
}

static GLuint
get_current_texture_2d_object (CoglGLES2Context *gles2_ctx)
{
  return g_array_index (gles2_ctx->texture_units,
                        CoglGLES2TextureUnitData,
                        gles2_ctx->current_texture_unit).current_texture_2d;
}

static void
set_texture_object_data (CoglGLES2Context *gles2_ctx,
                         GLenum target,
                         GLint level,
                         GLenum internal_format,
                         GLsizei width,
                         GLsizei height)
{
  GLuint texture_id = get_current_texture_2d_object (gles2_ctx);
  CoglGLES2TextureObjectData *texture_object;

  /* We want to keep track of all texture objects where the data is
   * created by this context so that we can delete them later */
  texture_object = g_hash_table_lookup (gles2_ctx->texture_object_map,
                                        GUINT_TO_POINTER (texture_id));
  if (texture_object == NULL)
    {
      texture_object = g_slice_new0 (CoglGLES2TextureObjectData);
      texture_object->object_id = texture_id;

      g_hash_table_insert (gles2_ctx->texture_object_map,
                           GUINT_TO_POINTER (texture_id),
                           texture_object);
    }

  switch (target)
    {
    case GL_TEXTURE_2D:
      texture_object->target = GL_TEXTURE_2D;

      /* We want to keep track of the dimensions of any texture object
       * setting the GL_TEXTURE_2D target */
      if (level == 0)
        {
          texture_object->width = width;
          texture_object->height = height;
          texture_object->format = internal_format;
        }
      break;

    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      texture_object->target = GL_TEXTURE_CUBE_MAP;
      break;
    }
}

static void
copy_flipped_texture (CoglGLES2Context *gles2_ctx,
                      int level,
                      int src_x,
                      int src_y,
                      int dst_x,
                      int dst_y,
                      int width,
                      int height)
{
  GLuint tex_id = get_current_texture_2d_object (gles2_ctx);
  CoglGLES2TextureObjectData *tex_object_data;
  CoglContext *ctx;
  const CoglWinsysVtable *winsys;
  CoglTexture2D *dst_texture;
  CoglPixelFormat internal_format;

  tex_object_data = g_hash_table_lookup (gles2_ctx->texture_object_map,
                                         GUINT_TO_POINTER (tex_id));

  /* We can't do anything if the application hasn't set a level 0
   * image on this texture object */
  if (tex_object_data == NULL ||
      tex_object_data->target != GL_TEXTURE_2D ||
      tex_object_data->width <= 0 ||
      tex_object_data->height <= 0)
    return;

  switch (tex_object_data->format)
    {
    case GL_RGB:
      internal_format = COGL_PIXEL_FORMAT_RGB_888;
      break;

    case GL_RGBA:
      internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;
      break;

    case GL_ALPHA:
      internal_format = COGL_PIXEL_FORMAT_A_8;
      break;

    case GL_LUMINANCE:
      internal_format = COGL_PIXEL_FORMAT_G_8;
      break;

    default:
      /* We can't handle this format so just give up */
      return;
    }

  ctx = gles2_ctx->context;
  winsys = ctx->display->renderer->winsys_vtable;

  /* We need to make sure the rendering on the GLES2 context is
   * complete before the blit will be ready in the GLES2 context */
  ctx->glFinish ();
  /* We need to force Cogl to rebind the texture because according to
   * the GL spec a shared texture isn't guaranteed to be updated until
   * is rebound */
  _cogl_get_texture_unit (0)->dirty_gl_texture = TRUE;

  /* Temporarily switch back to the Cogl context */
  winsys->restore_context (ctx);

  dst_texture =
    cogl_gles2_texture_2d_new_from_handle (gles2_ctx->context,
                                           gles2_ctx,
                                           tex_id,
                                           tex_object_data->width,
                                           tex_object_data->height,
                                           internal_format);

  if (dst_texture)
    {
      CoglTexture *src_texture =
        COGL_OFFSCREEN (gles2_ctx->read_buffer)->texture;
      CoglPipeline *pipeline = cogl_pipeline_new (ctx);
      const CoglOffscreenFlags flags = COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL;
      CoglOffscreen *offscreen =
        _cogl_offscreen_new_with_texture_full (COGL_TEXTURE (dst_texture),
                                             flags, level);
      int src_width = cogl_texture_get_width (src_texture);
      int src_height = cogl_texture_get_height (src_texture);
      /* The framebuffer size might be different from the texture size
       * if a level > 0 is used */
      int dst_width =
        cogl_framebuffer_get_width (COGL_FRAMEBUFFER (offscreen));
      int dst_height =
        cogl_framebuffer_get_height (COGL_FRAMEBUFFER (offscreen));
      float x_1, y_1, x_2, y_2, s_1, t_1, s_2, t_2;

      cogl_pipeline_set_layer_texture (pipeline, 0, src_texture);
      cogl_pipeline_set_blend (pipeline,
                               "RGBA = ADD(SRC_COLOR, 0)",
                               NULL);
      cogl_pipeline_set_layer_filters (pipeline,
                                       0, /* layer_num */
                                       COGL_PIPELINE_FILTER_NEAREST,
                                       COGL_PIPELINE_FILTER_NEAREST);

      x_1 = dst_x * 2.0f / dst_width - 1.0f;
      y_1 = dst_y * 2.0f / dst_height - 1.0f;
      x_2 = x_1 + width * 2.0f / dst_width;
      y_2 = y_1 + height * 2.0f / dst_height;

      s_1 = src_x / (float) src_width;
      t_1 = 1.0f - src_y / (float) src_height;
      s_2 = (src_x + width) / (float) src_width;
      t_2 = 1.0f - (src_y + height) / (float) src_height;

      cogl_framebuffer_draw_textured_rectangle (COGL_FRAMEBUFFER (offscreen),
                                                pipeline,
                                                x_1, y_1,
                                                x_2, y_2,
                                                s_1, t_1,
                                                s_2, t_2);

      _cogl_framebuffer_flush_journal (COGL_FRAMEBUFFER (offscreen));

      /* We need to make sure the rendering is complete before the
       * blit will be ready in the GLES2 context */
      ctx->glFinish ();

      cogl_object_unref (pipeline);
      cogl_object_unref (dst_texture);
      cogl_object_unref (offscreen);
    }

  winsys->set_gles2_context (gles2_ctx, NULL);

  /* From what I understand of the GL spec, changes to a shared object
   * are not guaranteed to be propagated to another context until that
   * object is rebound in that context so we can just rebind it
   * here */
  gles2_ctx->vtable->glBindTexture (GL_TEXTURE_2D, tex_id);
}

/* We wrap glBindFramebuffer so that when framebuffer 0 is bound
 * we can instead bind the write_framebuffer passed to
 * cogl_push_gles2_context().
 */
static void
gl_bind_framebuffer_wrapper (GLenum target, GLuint framebuffer)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  gles2_ctx->current_fbo_handle = framebuffer;

  if (framebuffer == 0 && cogl_is_offscreen (gles2_ctx->write_buffer))
    {
      CoglGLES2Offscreen *write = gles2_ctx->gles2_write_buffer;
      framebuffer = write->gl_framebuffer.fbo_handle;
    }

  gles2_ctx->context->glBindFramebuffer (target, framebuffer);

  update_current_flip_state (gles2_ctx);
}

static int
transient_bind_read_buffer (CoglGLES2Context *gles2_ctx)
{
  if (gles2_ctx->current_fbo_handle == 0)
    {
      if (cogl_is_offscreen (gles2_ctx->read_buffer))
        {
          CoglGLES2Offscreen *read = gles2_ctx->gles2_read_buffer;
          GLuint read_fbo_handle = read->gl_framebuffer.fbo_handle;

          gles2_ctx->context->glBindFramebuffer (GL_FRAMEBUFFER,
                                                 read_fbo_handle);

          return RESTORE_FB_FROM_OFFSCREEN;
        }
      else
        {
          _cogl_framebuffer_gl_bind (gles2_ctx->read_buffer,
                                     0 /* target ignored */);

          return RESTORE_FB_FROM_ONSCREEN;
        }
    }
  else
    return RESTORE_FB_NONE;
}

static void
restore_write_buffer (CoglGLES2Context *gles2_ctx,
                      int restore_mode)
{
  switch (restore_mode)
    {
    case RESTORE_FB_FROM_OFFSCREEN:

      gl_bind_framebuffer_wrapper (GL_FRAMEBUFFER, 0);

      break;
    case RESTORE_FB_FROM_ONSCREEN:

      /* Note: we can't restore the original write buffer using
       * _cogl_framebuffer_gl_bind() if it's an offscreen
       * framebuffer because _cogl_framebuffer_gl_bind() doesn't
       * know about the fbo handle owned by the gles2 context.
       */
      if (cogl_is_offscreen (gles2_ctx->write_buffer))
        gl_bind_framebuffer_wrapper (GL_FRAMEBUFFER, 0);
      else
        _cogl_framebuffer_gl_bind (gles2_ctx->write_buffer, GL_FRAMEBUFFER);

      break;
    case RESTORE_FB_NONE:
      break;
    }
}

/* We wrap glReadPixels so when framebuffer 0 is bound then we can
 * read from the read_framebuffer passed to cogl_push_gles2_context().
 */
static void
gl_read_pixels_wrapper (GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLenum format,
                        GLenum type,
                        GLvoid *pixels)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  int restore_mode = transient_bind_read_buffer (gles2_ctx);

  gles2_ctx->context->glReadPixels (x, y, width, height, format, type, pixels);

  restore_write_buffer (gles2_ctx, restore_mode);

  /* If the read buffer is a CoglOffscreen then the data will be
   * upside down compared to what GL expects so we need to flip it */
  if (gles2_ctx->current_fbo_handle == 0 &&
      cogl_is_offscreen (gles2_ctx->read_buffer))
    {
      int bpp, bytes_per_row, stride, y;
      uint8_t *bytes = pixels;
      uint8_t *temprow;

      /* Try to determine the bytes per pixel for the given
       * format/type combination. If there's a format which doesn't
       * make sense then we'll just give up because GL will probably
       * have just thrown an error */
      switch (format)
        {
        case GL_RGB:
          switch (type)
            {
            case GL_UNSIGNED_BYTE:
              bpp = 3;
              break;

            case GL_UNSIGNED_SHORT_5_6_5:
              bpp = 2;
              break;

            default:
              return;
            }
          break;

        case GL_RGBA:
          switch (type)
            {
            case GL_UNSIGNED_BYTE:
              bpp = 4;
              break;

            case GL_UNSIGNED_SHORT_4_4_4_4:
            case GL_UNSIGNED_SHORT_5_5_5_1:
              bpp = 2;
              break;

            default:
              return;
            }
          break;

        case GL_ALPHA:
          switch (type)
            {
            case GL_UNSIGNED_BYTE:
              bpp = 1;
              break;

            default:
              return;
            }
          break;

        default:
          return;
        }

      bytes_per_row = bpp * width;
      stride = ((bytes_per_row + gles2_ctx->pack_alignment - 1) &
                ~(gles2_ctx->pack_alignment - 1));
      temprow = g_alloca (bytes_per_row);

      /* vertically flip the buffer in-place */
      for (y = 0; y < height / 2; y++)
        {
          if (y != height - y - 1) /* skip center row */
            {
              memcpy (temprow,
                      bytes + y * stride,
                      bytes_per_row);
              memcpy (bytes + y * stride,
                      bytes + (height - y - 1) * stride,
                      bytes_per_row);
              memcpy (bytes + (height - y - 1) * stride,
                      temprow,
                      bytes_per_row);
            }
        }
    }
}

static void
gl_copy_tex_image_2d_wrapper (GLenum target,
                              GLint level,
                              GLenum internal_format,
                              GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height,
                              GLint border)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  /* If we are reading from a CoglOffscreen buffer then the image will
   * be upside down with respect to what GL expects so we can't use
   * glCopyTexImage2D. Instead we we'll try to use the Cogl API to
   * flip it */
  if (gles2_ctx->current_fbo_handle == 0 &&
      cogl_is_offscreen (gles2_ctx->read_buffer))
    {
      /* This will only work with the GL_TEXTURE_2D target. FIXME:
       * GLES2 also supports setting cube map textures with
       * glTexImage2D so we need to handle that too */
      if (target != GL_TEXTURE_2D)
        return;

      /* Create an empty texture to hold the data */
      gles2_ctx->vtable->glTexImage2D (target,
                                       level,
                                       internal_format,
                                       width, height,
                                       border,
                                       internal_format, /* format */
                                       GL_UNSIGNED_BYTE, /* type */
                                       NULL /* data */);

      copy_flipped_texture (gles2_ctx,
                            level,
                            x, y, /* src_x/src_y */
                            0, 0, /* dst_x/dst_y */
                            width, height);
    }
  else
    {
      int restore_mode = transient_bind_read_buffer (gles2_ctx);

      gles2_ctx->context->glCopyTexImage2D (target, level, internal_format,
                                            x, y, width, height, border);

      restore_write_buffer (gles2_ctx, restore_mode);

      set_texture_object_data (gles2_ctx,
                               target,
                               level,
                               internal_format,
                               width, height);
    }
}

static void
gl_copy_tex_sub_image_2d_wrapper (GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  /* If we are reading from a CoglOffscreen buffer then the image will
   * be upside down with respect to what GL expects so we can't use
   * glCopyTexSubImage2D. Instead we we'll try to use the Cogl API to
   * flip it */
  if (gles2_ctx->current_fbo_handle == 0 &&
      cogl_is_offscreen (gles2_ctx->read_buffer))
    {
      /* This will only work with the GL_TEXTURE_2D target. FIXME:
       * GLES2 also supports setting cube map textures with
       * glTexImage2D so we need to handle that too */
      if (target != GL_TEXTURE_2D)
        return;

      copy_flipped_texture (gles2_ctx,
                            level,
                            x, y, /* src_x/src_y */
                            xoffset, yoffset, /* dst_x/dst_y */
                            width, height);
    }
  else
    {
      int restore_mode = transient_bind_read_buffer (gles2_ctx);

      gles2_ctx->context->glCopyTexSubImage2D (target, level,
                                               xoffset, yoffset,
                                               x, y, width, height);

      restore_write_buffer (gles2_ctx, restore_mode);
    }
}

static GLuint
gl_create_shader_wrapper (GLenum type)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  GLuint id;

  id = gles2_ctx->context->glCreateShader (type);

  if (id != 0)
    {
      CoglGLES2ShaderData *data = g_slice_new (CoglGLES2ShaderData);

      data->object_id = id;
      data->type = type;
      data->ref_count = 1;
      data->deleted = FALSE;

      g_hash_table_insert (gles2_ctx->shader_map,
                           GINT_TO_POINTER (id),
                           data);
    }

  return id;
}

static void
gl_delete_shader_wrapper (GLuint shader)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  CoglGLES2ShaderData *shader_data;

  if ((shader_data = g_hash_table_lookup (gles2_ctx->shader_map,
                                          GINT_TO_POINTER (shader))) &&
      !shader_data->deleted)
    {
      shader_data->deleted = TRUE;
      shader_data_unref (gles2_ctx, shader_data);
    }

  gles2_ctx->context->glDeleteShader (shader);
}

static GLuint
gl_create_program_wrapper (void)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  GLuint id;

  id = gles2_ctx->context->glCreateProgram ();

  if (id != 0)
    {
      CoglGLES2ProgramData *data = g_slice_new (CoglGLES2ProgramData);

      data->object_id = id;
      data->attached_shaders = NULL;
      data->ref_count = 1;
      data->deleted = FALSE;
      data->context = gles2_ctx;
      data->flip_vector_location = 0;
      data->flip_vector_state = COGL_GLES2_FLIP_STATE_UNKNOWN;

      g_hash_table_insert (gles2_ctx->program_map,
                           GINT_TO_POINTER (id),
                           data);
    }

  return id;
}

static void
gl_delete_program_wrapper (GLuint program)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  CoglGLES2ProgramData *program_data;

  if ((program_data = g_hash_table_lookup (gles2_ctx->program_map,
                                           GINT_TO_POINTER (program))) &&
      !program_data->deleted)
    {
      program_data->deleted = TRUE;
      program_data_unref (program_data);
    }

  gles2_ctx->context->glDeleteProgram (program);
}

static void
gl_use_program_wrapper (GLuint program)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  CoglGLES2ProgramData *program_data;

  program_data = g_hash_table_lookup (gles2_ctx->program_map,
                                      GINT_TO_POINTER (program));

  if (program_data)
    program_data->ref_count++;
  if (gles2_ctx->current_program)
    program_data_unref (gles2_ctx->current_program);

  gles2_ctx->current_program = program_data;

  gles2_ctx->context->glUseProgram (program);
}

static void
gl_attach_shader_wrapper (GLuint program,
                          GLuint shader)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  CoglGLES2ProgramData *program_data;
  CoglGLES2ShaderData *shader_data;

  if ((program_data = g_hash_table_lookup (gles2_ctx->program_map,
                                           GINT_TO_POINTER (program))) &&
      (shader_data = g_hash_table_lookup (gles2_ctx->shader_map,
                                          GINT_TO_POINTER (shader))) &&
      /* Ignore attempts to attach a shader that is already attached */
      g_list_find (program_data->attached_shaders, shader_data) == NULL)
    {
      shader_data->ref_count++;
      program_data->attached_shaders =
        g_list_prepend (program_data->attached_shaders, shader_data);
    }

  gles2_ctx->context->glAttachShader (program, shader);
}

static void
gl_detach_shader_wrapper (GLuint program,
                          GLuint shader)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  CoglGLES2ProgramData *program_data;
  CoglGLES2ShaderData *shader_data;

  if ((program_data = g_hash_table_lookup (gles2_ctx->program_map,
                                           GINT_TO_POINTER (program))) &&
      (shader_data = g_hash_table_lookup (gles2_ctx->shader_map,
                                          GINT_TO_POINTER (shader))))
    detach_shader (program_data, shader_data);

  gles2_ctx->context->glDetachShader (program, shader);
}

static void
gl_shader_source_wrapper (GLuint shader,
                          GLsizei count,
                          const char *const *string,
                          const GLint *length)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  CoglGLES2ShaderData *shader_data;

  if ((shader_data = g_hash_table_lookup (gles2_ctx->shader_map,
                                          GINT_TO_POINTER (shader))) &&
      shader_data->type == GL_VERTEX_SHADER)
    {
      char **string_copy = g_alloca ((count + 1) * sizeof (char *));
      int *length_copy = g_alloca ((count + 1) * sizeof (int));
      int i;

      /* Replace any occurences of the symbol 'main' with a different
       * symbol so that we can provide our own wrapper main
       * function */

      for (i = 0; i < count; i++)
        {
          int string_length;

          if (length == NULL || length[i] < 0)
            string_length = strlen (string[i]);
          else
            string_length = length[i];

          string_copy[i] = g_memdup (string[i], string_length);

          replace_token (string_copy[i],
                         "main",
                         MAIN_WRAPPER_REPLACEMENT_NAME,
                         string_length);

          length_copy[i] = string_length;
        }

      string_copy[count] = (char *) main_wrapper_function;
      length_copy[count] = sizeof (main_wrapper_function) - 1;

      gles2_ctx->context->glShaderSource (shader,
                                          count + 1,
                                          (const char *const *) string_copy,
                                          length_copy);

      /* Note: we don't need to free the last entry in string_copy[]
       * because it is our static wrapper string... */
      for (i = 0; i < count; i++)
        g_free (string_copy[i]);
    }
  else
    gles2_ctx->context->glShaderSource (shader, count, string, length);
}

static void
gl_get_shader_source_wrapper (GLuint shader,
                              GLsizei buf_size,
                              GLsizei *length_out,
                              GLchar *source)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  CoglGLES2ShaderData *shader_data;
  GLsizei length;

  gles2_ctx->context->glGetShaderSource (shader,
                                         buf_size,
                                         &length,
                                         source);

  if ((shader_data = g_hash_table_lookup (gles2_ctx->shader_map,
                                          GINT_TO_POINTER (shader))) &&
      shader_data->type == GL_VERTEX_SHADER)
    {
      GLsizei copy_length = MIN (length, buf_size - 1);
      static const char wrapper_marker[] = MAIN_WRAPPER_BEGIN;
      char *wrapper_start;

      /* Strip out the wrapper snippet we added when the source was
       * specified */
      wrapper_start = _cogl_util_memmem (source,
                                         copy_length,
                                         wrapper_marker,
                                         sizeof (wrapper_marker) - 1);
      if (wrapper_start)
        {
          length = wrapper_start - source;
          copy_length = length;
          *wrapper_start = '\0';
        }

      /* Correct the name of the main function back to its original */
      replace_token (source,
                     MAIN_WRAPPER_REPLACEMENT_NAME,
                     "main",
                     copy_length);
    }

  if (length_out)
    *length_out = length;
}

static void
gl_link_program_wrapper (GLuint program)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  CoglGLES2ProgramData *program_data;

  gles2_ctx->context->glLinkProgram (program);

  program_data = g_hash_table_lookup (gles2_ctx->program_map,
                                      GINT_TO_POINTER (program));

  if (program_data)
    {
      GLint status;

      gles2_ctx->context->glGetProgramiv (program, GL_LINK_STATUS, &status);

      if (status)
        program_data->flip_vector_location =
          gles2_ctx->context->glGetUniformLocation (program,
                                                    MAIN_WRAPPER_FLIP_UNIFORM);
    }
}

static void
gl_get_program_iv_wrapper (GLuint program,
                           GLenum pname,
                           GLint *params)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  gles2_ctx->context->glGetProgramiv (program, pname, params);

  switch (pname)
    {
    case GL_ATTACHED_SHADERS:
      /* Decrease the number of shaders to try and hide the shader
       * wrapper we added */
      if (*params > 1)
        (*params)--;
      break;
    }
}

static void
flush_viewport_state (CoglGLES2Context *gles2_ctx)
{
  if (gles2_ctx->viewport_dirty)
    {
      int y;

      if (gles2_ctx->current_flip_state == COGL_GLES2_FLIP_STATE_FLIPPED)
        {
          /* We need to know the height of the current framebuffer in
           * order to flip the viewport. Fortunately we don't need to
           * track the height of the FBOs created within the GLES2
           * context because we would never be flipping if they are
           * bound so we can just assume Cogl's framebuffer is bound
           * when we are flipping */
          int fb_height = cogl_framebuffer_get_height (gles2_ctx->write_buffer);
          y = fb_height - (gles2_ctx->viewport[1] + gles2_ctx->viewport[3]);
        }
      else
        y = gles2_ctx->viewport[1];

      gles2_ctx->context->glViewport (gles2_ctx->viewport[0],
                                      y,
                                      gles2_ctx->viewport[2],
                                      gles2_ctx->viewport[3]);

      gles2_ctx->viewport_dirty = FALSE;
    }
}

static void
flush_scissor_state (CoglGLES2Context *gles2_ctx)
{
  if (gles2_ctx->scissor_dirty)
    {
      int y;

      if (gles2_ctx->current_flip_state == COGL_GLES2_FLIP_STATE_FLIPPED)
        {
          /* See comment above about the viewport flipping */
          int fb_height = cogl_framebuffer_get_height (gles2_ctx->write_buffer);
          y = fb_height - (gles2_ctx->scissor[1] + gles2_ctx->scissor[3]);
        }
      else
        y = gles2_ctx->scissor[1];

      gles2_ctx->context->glScissor (gles2_ctx->scissor[0],
                                     y,
                                     gles2_ctx->scissor[2],
                                     gles2_ctx->scissor[3]);

      gles2_ctx->scissor_dirty = FALSE;
    }
}

static void
flush_front_face_state (CoglGLES2Context *gles2_ctx)
{
  if (gles2_ctx->front_face_dirty)
    {
      GLenum front_face;

      if (gles2_ctx->current_flip_state == COGL_GLES2_FLIP_STATE_FLIPPED)
        {
          if (gles2_ctx->front_face == GL_CW)
            front_face = GL_CCW;
          else
            front_face = GL_CW;
        }
      else
        front_face = gles2_ctx->front_face;

      gles2_ctx->context->glFrontFace (front_face);

      gles2_ctx->front_face_dirty = FALSE;
    }
}

static void
pre_draw_wrapper (CoglGLES2Context *gles2_ctx)
{
  /* If there's no current program then we'll just let GL report an
   * error */
  if (gles2_ctx->current_program == NULL)
    return;

  flush_viewport_state (gles2_ctx);
  flush_scissor_state (gles2_ctx);
  flush_front_face_state (gles2_ctx);

  /* We want to flip rendering when the application is rendering to a
   * Cogl offscreen buffer in order to maintain the flipped texture
   * coordinate origin */
  if (gles2_ctx->current_flip_state !=
      gles2_ctx->current_program->flip_vector_state)
    {
      GLuint location =
        gles2_ctx->current_program->flip_vector_location;
      float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

      if (gles2_ctx->current_flip_state == COGL_GLES2_FLIP_STATE_FLIPPED)
        value[1] = -1.0f;

      gles2_ctx->context->glUniform4fv (location, 1, value);

      gles2_ctx->current_program->flip_vector_state =
        gles2_ctx->current_flip_state;
    }
}

static void
gl_clear_wrapper (GLbitfield mask)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  /* Clearing is affected by the scissor state so we need to ensure
   * that's flushed */
  flush_scissor_state (gles2_ctx);

  gles2_ctx->context->glClear (mask);
}

static void
gl_draw_elements_wrapper (GLenum mode,
                          GLsizei count,
                          GLenum type,
                          const GLvoid *indices)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  pre_draw_wrapper (gles2_ctx);

  gles2_ctx->context->glDrawElements (mode, count, type, indices);
}

static void
gl_draw_arrays_wrapper (GLenum mode,
                        GLint first,
                        GLsizei count)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  pre_draw_wrapper (gles2_ctx);

  gles2_ctx->context->glDrawArrays (mode, first, count);
}

static void
gl_get_program_info_log_wrapper (GLuint program,
                                 GLsizei buf_size,
                                 GLsizei *length_out,
                                 GLchar *info_log)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  GLsizei length;

  gles2_ctx->context->glGetProgramInfoLog (program,
                                           buf_size,
                                           &length,
                                           info_log);

  replace_token (info_log,
                 MAIN_WRAPPER_REPLACEMENT_NAME,
                 "main",
                 MIN (length, buf_size));

  if (length_out)
    *length_out = length;
}

static void
gl_get_shader_info_log_wrapper (GLuint shader,
                                GLsizei buf_size,
                                GLsizei *length_out,
                                GLchar *info_log)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  GLsizei length;

  gles2_ctx->context->glGetShaderInfoLog (shader,
                                          buf_size,
                                          &length,
                                          info_log);

  replace_token (info_log,
                 MAIN_WRAPPER_REPLACEMENT_NAME,
                 "main",
                 MIN (length, buf_size));

  if (length_out)
    *length_out = length;
}

static void
gl_front_face_wrapper (GLenum mode)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  /* If the mode doesn't make any sense then we'll just let the
   * context deal with it directly so that it will throw an error */
  if (mode != GL_CW && mode != GL_CCW)
    gles2_ctx->context->glFrontFace (mode);
  else
    {
      gles2_ctx->front_face = mode;
      gles2_ctx->front_face_dirty = TRUE;
    }
}

static void
gl_viewport_wrapper (GLint x,
                     GLint y,
                     GLsizei width,
                     GLsizei height)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  /* If the viewport is invalid then we'll just let the context deal
   * with it directly so that it will throw an error */
  if (width < 0 || height < 0)
    gles2_ctx->context->glViewport (x, y, width, height);
  else
    {
      gles2_ctx->viewport[0] = x;
      gles2_ctx->viewport[1] = y;
      gles2_ctx->viewport[2] = width;
      gles2_ctx->viewport[3] = height;
      gles2_ctx->viewport_dirty = TRUE;
    }
}

static void
gl_scissor_wrapper (GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  /* If the scissor is invalid then we'll just let the context deal
   * with it directly so that it will throw an error */
  if (width < 0 || height < 0)
    gles2_ctx->context->glScissor (x, y, width, height);
  else
    {
      gles2_ctx->scissor[0] = x;
      gles2_ctx->scissor[1] = y;
      gles2_ctx->scissor[2] = width;
      gles2_ctx->scissor[3] = height;
      gles2_ctx->scissor_dirty = TRUE;
    }
}

static void
gl_get_boolean_v_wrapper (GLenum pname,
                          GLboolean *params)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  switch (pname)
    {
    case GL_VIEWPORT:
      {
        int i;

        for (i = 0; i < 4; i++)
          params[i] = !!gles2_ctx->viewport[i];
      }
      break;

    case GL_SCISSOR_BOX:
      {
        int i;

        for (i = 0; i < 4; i++)
          params[i] = !!gles2_ctx->scissor[i];
      }
      break;

    default:
      gles2_ctx->context->glGetBooleanv (pname, params);
    }
}

static void
gl_get_integer_v_wrapper (GLenum pname,
                          GLint *params)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  switch (pname)
    {
    case GL_VIEWPORT:
      {
        int i;

        for (i = 0; i < 4; i++)
          params[i] = gles2_ctx->viewport[i];
      }
      break;

    case GL_SCISSOR_BOX:
      {
        int i;

        for (i = 0; i < 4; i++)
          params[i] = gles2_ctx->scissor[i];
      }
      break;

    case GL_FRONT_FACE:
      params[0] = gles2_ctx->front_face;
      break;

    default:
      gles2_ctx->context->glGetIntegerv (pname, params);
    }
}

static void
gl_get_float_v_wrapper (GLenum pname,
                        GLfloat *params)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  switch (pname)
    {
    case GL_VIEWPORT:
      {
        int i;

        for (i = 0; i < 4; i++)
          params[i] = gles2_ctx->viewport[i];
      }
      break;

    case GL_SCISSOR_BOX:
      {
        int i;

        for (i = 0; i < 4; i++)
          params[i] = gles2_ctx->scissor[i];
      }
      break;

    case GL_FRONT_FACE:
      params[0] = gles2_ctx->front_face;
      break;

    default:
      gles2_ctx->context->glGetFloatv (pname, params);
    }
}

static void
gl_pixel_store_i_wrapper (GLenum pname, GLint param)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  gles2_ctx->context->glPixelStorei (pname, param);

  if (pname == GL_PACK_ALIGNMENT &&
      (param == 1 || param == 2 || param == 4 || param == 8))
    gles2_ctx->pack_alignment = param;
}

static void
gl_active_texture_wrapper (GLenum texture)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  int texture_unit;

  gles2_ctx->context->glActiveTexture (texture);

  texture_unit = texture - GL_TEXTURE0;

  /* If the application is binding some odd looking texture unit
   * numbers then we'll just ignore it and hope that GL has generated
   * an error */
  if (texture_unit >= 0 && texture_unit < 512)
    {
      gles2_ctx->current_texture_unit = texture_unit;
      g_array_set_size (gles2_ctx->texture_units,
                        MAX (texture_unit, gles2_ctx->texture_units->len));
    }
}

static void
gl_delete_textures_wrapper (GLsizei n,
                            const GLuint *textures)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;
  int texture_index;
  int texture_unit;

  gles2_ctx->context->glDeleteTextures (n, textures);

  for (texture_index = 0; texture_index < n; texture_index++)
    {
      /* Reset any texture units that have any of these textures bound */
      for (texture_unit = 0;
           texture_unit < gles2_ctx->texture_units->len;
           texture_unit++)
        {
          CoglGLES2TextureUnitData *unit =
            &g_array_index (gles2_ctx->texture_units,
                            CoglGLES2TextureUnitData,
                            texture_unit);

          if (unit->current_texture_2d == textures[texture_index])
            unit->current_texture_2d = 0;
        }

      /* Remove the binding. We can do this immediately because unlike
       * shader objects the deletion isn't delayed until the object is
       * unbound */
      g_hash_table_remove (gles2_ctx->texture_object_map,
                           GUINT_TO_POINTER (textures[texture_index]));
    }
}

static void
gl_bind_texture_wrapper (GLenum target,
                         GLuint texture)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  gles2_ctx->context->glBindTexture (target, texture);

  if (target == GL_TEXTURE_2D)
    {
      CoglGLES2TextureUnitData *unit =
        &g_array_index (gles2_ctx->texture_units,
                        CoglGLES2TextureUnitData,
                        gles2_ctx->current_texture_unit);
      unit->current_texture_2d = texture;
    }
}

static void
gl_tex_image_2d_wrapper (GLenum target,
                         GLint level,
                         GLint internal_format,
                         GLsizei width,
                         GLsizei height,
                         GLint border,
                         GLenum format,
                         GLenum type,
                         const GLvoid *pixels)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  gles2_ctx->context->glTexImage2D (target,
                                    level,
                                    internal_format,
                                    width, height,
                                    border,
                                    format,
                                    type,
                                    pixels);

  set_texture_object_data (gles2_ctx,
                           target,
                           level,
                           internal_format,
                           width, height);
}

static void
_cogl_gles2_offscreen_free (CoglGLES2Offscreen *gles2_offscreen)
{
  _cogl_list_remove (&gles2_offscreen->link);
  g_slice_free (CoglGLES2Offscreen, gles2_offscreen);
}

static void
force_delete_program_object (CoglGLES2Context *context,
                             CoglGLES2ProgramData *program_data)
{
  if (!program_data->deleted)
    {
      context->context->glDeleteProgram (program_data->object_id);
      program_data->deleted = TRUE;
      program_data_unref (program_data);
    }
}

static void
force_delete_shader_object (CoglGLES2Context *context,
                            CoglGLES2ShaderData *shader_data)
{
  if (!shader_data->deleted)
    {
      context->context->glDeleteShader (shader_data->object_id);
      shader_data->deleted = TRUE;
      shader_data_unref (context, shader_data);
    }
}

static void
force_delete_texture_object (CoglGLES2Context *context,
                             CoglGLES2TextureObjectData *texture_data)
{
  context->context->glDeleteTextures (1, &texture_data->object_id);
}

static void
_cogl_gles2_context_free (CoglGLES2Context *gles2_context)
{
  CoglContext *ctx = gles2_context->context;
  const CoglWinsysVtable *winsys;
  GList *objects, *l;

  if (gles2_context->current_program)
    program_data_unref (gles2_context->current_program);

  /* Try to forcibly delete any shaders, programs and textures so that
   * they won't get leaked. Because all GLES2 contexts are in the same
   * share list as Cogl's context these won't get deleted by default.
   * FIXME: we should do this for all of the other resources too, like
   * textures */
  objects = g_hash_table_get_values (gles2_context->program_map);
  for (l = objects; l; l = l->next)
    force_delete_program_object (gles2_context, l->data);
  g_list_free (objects);
  objects = g_hash_table_get_values (gles2_context->shader_map);
  for (l = objects; l; l = l->next)
    force_delete_shader_object (gles2_context, l->data);
  g_list_free (objects);
  objects = g_hash_table_get_values (gles2_context->texture_object_map);
  for (l = objects; l; l = l->next)
    force_delete_texture_object (gles2_context, l->data);
  g_list_free (objects);

  /* All of the program and shader objects should now be destroyed */
  if (g_hash_table_size (gles2_context->program_map) > 0)
    g_warning ("Program objects have been leaked from a CoglGLES2Context");
  if (g_hash_table_size (gles2_context->shader_map) > 0)
    g_warning ("Shader objects have been leaked from a CoglGLES2Context");

  g_hash_table_destroy (gles2_context->program_map);
  g_hash_table_destroy (gles2_context->shader_map);

  g_hash_table_destroy (gles2_context->texture_object_map);
  g_array_free (gles2_context->texture_units, TRUE);

  winsys = ctx->display->renderer->winsys_vtable;
  winsys->destroy_gles2_context (gles2_context);

  while (!_cogl_list_empty (&gles2_context->foreign_offscreens))
    {
      CoglGLES2Offscreen *gles2_offscreen =
        _cogl_container_of (gles2_context->foreign_offscreens.next,
                            gles2_offscreen,
                            link);

      /* Note: this will also indirectly free the gles2_offscreen by
       * calling the destroy notify for the _user_data */
      cogl_object_set_user_data (COGL_OBJECT (gles2_offscreen->original_offscreen),
                                 &offscreen_wrapper_key,
                                 NULL,
                                 NULL);
    }

  g_free (gles2_context->vtable);

  g_free (gles2_context);
}

static void
free_shader_data (CoglGLES2ShaderData *data)
{
  g_slice_free (CoglGLES2ShaderData, data);
}

static void
free_program_data (CoglGLES2ProgramData *data)
{
  while (data->attached_shaders)
    detach_shader (data,
                   data->attached_shaders->data);

  g_slice_free (CoglGLES2ProgramData, data);
}

static void
free_texture_object_data (CoglGLES2TextureObjectData *data)
{
  g_slice_free (CoglGLES2TextureObjectData, data);
}

CoglGLES2Context *
cogl_gles2_context_new (CoglContext *ctx, CoglError **error)
{
  CoglGLES2Context *gles2_ctx;
  const CoglWinsysVtable *winsys;

  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_GLES2_CONTEXT))
    {
      _cogl_set_error (error, COGL_GLES2_CONTEXT_ERROR,
                   COGL_GLES2_CONTEXT_ERROR_UNSUPPORTED,
                   "Backend doesn't support creating GLES2 contexts");

      return NULL;
    }

  gles2_ctx = g_malloc0 (sizeof (CoglGLES2Context));

  gles2_ctx->context = ctx;

  _cogl_list_init (&gles2_ctx->foreign_offscreens);

  winsys = ctx->display->renderer->winsys_vtable;
  gles2_ctx->winsys = winsys->context_create_gles2_context (ctx, error);
  if (gles2_ctx->winsys == NULL)
    {
      g_free (gles2_ctx);
      return NULL;
    }

  gles2_ctx->current_flip_state = COGL_GLES2_FLIP_STATE_UNKNOWN;
  gles2_ctx->viewport_dirty = TRUE;
  gles2_ctx->scissor_dirty = TRUE;
  gles2_ctx->front_face_dirty = TRUE;
  gles2_ctx->front_face = GL_CCW;
  gles2_ctx->pack_alignment = 4;

  gles2_ctx->vtable = g_malloc0 (sizeof (CoglGLES2Vtable));
#define COGL_EXT_BEGIN(name, \
                       min_gl_major, min_gl_minor, \
                       gles_availability, \
                       extension_suffixes, extension_names)

#define COGL_EXT_FUNCTION(ret, name, args) \
  gles2_ctx->vtable->name = (void *) ctx->name;

#define COGL_EXT_END()

#include "gl-prototypes/cogl-gles2-functions.h"

#undef COGL_EXT_BEGIN
#undef COGL_EXT_FUNCTION
#undef COGL_EXT_END

  gles2_ctx->vtable->glBindFramebuffer =
    (void *) gl_bind_framebuffer_wrapper;
  gles2_ctx->vtable->glReadPixels =
    (void *) gl_read_pixels_wrapper;
  gles2_ctx->vtable->glCopyTexImage2D =
    (void *) gl_copy_tex_image_2d_wrapper;
  gles2_ctx->vtable->glCopyTexSubImage2D =
    (void *) gl_copy_tex_sub_image_2d_wrapper;

  gles2_ctx->vtable->glCreateShader = gl_create_shader_wrapper;
  gles2_ctx->vtable->glDeleteShader = gl_delete_shader_wrapper;
  gles2_ctx->vtable->glCreateProgram = gl_create_program_wrapper;
  gles2_ctx->vtable->glDeleteProgram = gl_delete_program_wrapper;
  gles2_ctx->vtable->glUseProgram = gl_use_program_wrapper;
  gles2_ctx->vtable->glAttachShader = gl_attach_shader_wrapper;
  gles2_ctx->vtable->glDetachShader = gl_detach_shader_wrapper;
  gles2_ctx->vtable->glShaderSource = gl_shader_source_wrapper;
  gles2_ctx->vtable->glGetShaderSource = gl_get_shader_source_wrapper;
  gles2_ctx->vtable->glLinkProgram = gl_link_program_wrapper;
  gles2_ctx->vtable->glGetProgramiv = gl_get_program_iv_wrapper;
  gles2_ctx->vtable->glGetProgramInfoLog = gl_get_program_info_log_wrapper;
  gles2_ctx->vtable->glGetShaderInfoLog = gl_get_shader_info_log_wrapper;
  gles2_ctx->vtable->glClear = gl_clear_wrapper;
  gles2_ctx->vtable->glDrawElements = gl_draw_elements_wrapper;
  gles2_ctx->vtable->glDrawArrays = gl_draw_arrays_wrapper;
  gles2_ctx->vtable->glFrontFace = gl_front_face_wrapper;
  gles2_ctx->vtable->glViewport = gl_viewport_wrapper;
  gles2_ctx->vtable->glScissor = gl_scissor_wrapper;
  gles2_ctx->vtable->glGetBooleanv = gl_get_boolean_v_wrapper;
  gles2_ctx->vtable->glGetIntegerv = gl_get_integer_v_wrapper;
  gles2_ctx->vtable->glGetFloatv = gl_get_float_v_wrapper;
  gles2_ctx->vtable->glPixelStorei = gl_pixel_store_i_wrapper;
  gles2_ctx->vtable->glActiveTexture = gl_active_texture_wrapper;
  gles2_ctx->vtable->glDeleteTextures = gl_delete_textures_wrapper;
  gles2_ctx->vtable->glBindTexture = gl_bind_texture_wrapper;
  gles2_ctx->vtable->glTexImage2D = gl_tex_image_2d_wrapper;

  gles2_ctx->shader_map =
    g_hash_table_new_full (g_direct_hash,
                           g_direct_equal,
                           NULL, /* key_destroy */
                           (GDestroyNotify) free_shader_data);
  gles2_ctx->program_map =
    g_hash_table_new_full (g_direct_hash,
                           g_direct_equal,
                           NULL, /* key_destroy */
                           (GDestroyNotify) free_program_data);

  gles2_ctx->texture_object_map =
    g_hash_table_new_full (g_direct_hash,
                           g_direct_equal,
                           NULL, /* key_destroy */
                           (GDestroyNotify) free_texture_object_data);

  gles2_ctx->texture_units = g_array_new (FALSE, /* not zero terminated */
                                          TRUE, /* clear */
                                          sizeof (CoglGLES2TextureUnitData));
  gles2_ctx->current_texture_unit = 0;
  g_array_set_size (gles2_ctx->texture_units, 1);

  return _cogl_gles2_context_object_new (gles2_ctx);
}

const CoglGLES2Vtable *
cogl_gles2_context_get_vtable (CoglGLES2Context *gles2_ctx)
{
  return gles2_ctx->vtable;
}

/* When drawing to a CoglFramebuffer from a separate context we have
 * to be able to allocate ancillary buffers for that context...
 */
static CoglGLES2Offscreen *
_cogl_gles2_offscreen_allocate (CoglOffscreen *offscreen,
                                CoglGLES2Context *gles2_context,
                                CoglError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (offscreen);
  const CoglWinsysVtable *winsys;
  CoglError *internal_error = NULL;
  CoglGLES2Offscreen *gles2_offscreen;
  int level_width;
  int level_height;

  if (!framebuffer->allocated &&
      !cogl_framebuffer_allocate (framebuffer, error))
    {
      return NULL;
    }

  _cogl_list_for_each (gles2_offscreen,
                       &gles2_context->foreign_offscreens,
                       link)
    {
      if (gles2_offscreen->original_offscreen == offscreen)
        return gles2_offscreen;
    }

  winsys = _cogl_framebuffer_get_winsys (framebuffer);
  winsys->save_context (framebuffer->context);
  if (!winsys->set_gles2_context (gles2_context, &internal_error))
    {
      winsys->restore_context (framebuffer->context);

      cogl_error_free (internal_error);
      _cogl_set_error (error, COGL_FRAMEBUFFER_ERROR,
                   COGL_FRAMEBUFFER_ERROR_ALLOCATE,
                   "Failed to bind gles2 context to create framebuffer");
      return NULL;
    }

  gles2_offscreen = g_slice_new0 (CoglGLES2Offscreen);

  _cogl_texture_get_level_size (offscreen->texture,
                                offscreen->texture_level,
                                &level_width,
                                &level_height,
                                NULL);

  if (!_cogl_framebuffer_try_creating_gl_fbo (gles2_context->context,
                                              offscreen->texture,
                                              offscreen->texture_level,
                                              level_width,
                                              level_height,
                                              offscreen->depth_texture,
                                              &COGL_FRAMEBUFFER (offscreen)->config,
                                              offscreen->allocation_flags,
                                              &gles2_offscreen->gl_framebuffer))
    {
      winsys->restore_context (framebuffer->context);

      g_slice_free (CoglGLES2Offscreen, gles2_offscreen);

      _cogl_set_error (error, COGL_FRAMEBUFFER_ERROR,
                   COGL_FRAMEBUFFER_ERROR_ALLOCATE,
                   "Failed to create an OpenGL framebuffer object");
      return NULL;
    }

  winsys->restore_context (framebuffer->context);

  gles2_offscreen->original_offscreen = offscreen;

  _cogl_list_insert (&gles2_context->foreign_offscreens,
                     &gles2_offscreen->link);

  /* So we avoid building up an ever growing collection of ancillary
   * buffers for wrapped framebuffers, we make sure that the wrappers
   * get freed when the original offscreen framebuffer is freed. */
  cogl_object_set_user_data (COGL_OBJECT (framebuffer),
                             &offscreen_wrapper_key,
                             gles2_offscreen,
                             (CoglUserDataDestroyCallback)
                                _cogl_gles2_offscreen_free);

  return gles2_offscreen;
}

CoglBool
cogl_push_gles2_context (CoglContext *ctx,
                         CoglGLES2Context *gles2_ctx,
                         CoglFramebuffer *read_buffer,
                         CoglFramebuffer *write_buffer,
                         CoglError **error)
{
  const CoglWinsysVtable *winsys = ctx->display->renderer->winsys_vtable;
  CoglError *internal_error = NULL;

  _COGL_RETURN_VAL_IF_FAIL (gles2_ctx != NULL, FALSE);

  /* The read/write buffers are properties of the gles2 context and we
   * don't currently track the read/write buffers as part of the stack
   * entries so we explicitly don't allow the same context to be
   * pushed multiple times. */
  if (g_queue_find (&ctx->gles2_context_stack, gles2_ctx))
    {
      g_critical ("Pushing the same GLES2 context multiple times isn't "
                  "supported");
      return FALSE;
    }

  if (ctx->gles2_context_stack.length == 0)
    {
      _cogl_journal_flush (read_buffer->journal);
      if (write_buffer != read_buffer)
        _cogl_journal_flush (write_buffer->journal);
      winsys->save_context (ctx);
    }
  else
    gles2_ctx->vtable->glFlush ();

  if (gles2_ctx->read_buffer != read_buffer)
    {
      if (cogl_is_offscreen (read_buffer))
        {
          gles2_ctx->gles2_read_buffer =
            _cogl_gles2_offscreen_allocate (COGL_OFFSCREEN (read_buffer),
                                            gles2_ctx,
                                            error);
          /* XXX: what consistency guarantees should this api have?
           *
           * It should be safe to return at this point but we provide
           * no guarantee to the caller whether their given buffers
           * may be referenced and old buffers unreferenced even
           * if the _push fails. */
          if (!gles2_ctx->gles2_read_buffer)
            return FALSE;
        }
      else
        gles2_ctx->gles2_read_buffer = NULL;
      if (gles2_ctx->read_buffer)
        cogl_object_unref (gles2_ctx->read_buffer);
      gles2_ctx->read_buffer = cogl_object_ref (read_buffer);
    }

  if (gles2_ctx->write_buffer != write_buffer)
    {
      if (cogl_is_offscreen (write_buffer))
        {
          gles2_ctx->gles2_write_buffer =
            _cogl_gles2_offscreen_allocate (COGL_OFFSCREEN (write_buffer),
                                            gles2_ctx,
                                            error);
          /* XXX: what consistency guarantees should this api have?
           *
           * It should be safe to return at this point but we provide
           * no guarantee to the caller whether their given buffers
           * may be referenced and old buffers unreferenced even
           * if the _push fails. */
          if (!gles2_ctx->gles2_write_buffer)
            return FALSE;
        }
      else
        gles2_ctx->gles2_write_buffer = NULL;
      if (gles2_ctx->write_buffer)
        cogl_object_unref (gles2_ctx->write_buffer);
      gles2_ctx->write_buffer = cogl_object_ref (write_buffer);

      update_current_flip_state (gles2_ctx);
    }

  if (!winsys->set_gles2_context (gles2_ctx, &internal_error))
    {
      winsys->restore_context (ctx);

      cogl_error_free (internal_error);
      _cogl_set_error (error, COGL_GLES2_CONTEXT_ERROR,
                   COGL_GLES2_CONTEXT_ERROR_DRIVER,
                   "Driver failed to make GLES2 context current");
      return FALSE;
    }

  g_queue_push_tail (&ctx->gles2_context_stack, gles2_ctx);

  /* The last time this context was pushed may have been with a
   * different offscreen draw framebuffer and so if GL framebuffer 0
   * is bound for this GLES2 context we may need to bind a new,
   * corresponding, window system framebuffer... */
  if (gles2_ctx->current_fbo_handle == 0)
    {
      if (cogl_is_offscreen (gles2_ctx->write_buffer))
        {
          CoglGLES2Offscreen *write = gles2_ctx->gles2_write_buffer;
          GLuint handle = write->gl_framebuffer.fbo_handle;
          gles2_ctx->context->glBindFramebuffer (GL_FRAMEBUFFER, handle);
        }
    }

  current_gles2_context = gles2_ctx;

  /* If this is the first time this gles2 context has been used then
   * we'll force the viewport and scissor to the right size. GL has
   * the semantics that the viewport and scissor default to the size
   * of the first surface the context is used with. If the first
   * CoglFramebuffer that this context is used with is an offscreen,
   * then the surface from GL's point of view will be the 1x1 dummy
   * surface so the viewport will be wrong. Therefore we just override
   * the default viewport and scissor here */
  if (!gles2_ctx->has_been_bound)
    {
      int fb_width = cogl_framebuffer_get_width (write_buffer);
      int fb_height = cogl_framebuffer_get_height (write_buffer);

      gles2_ctx->vtable->glViewport (0, 0, /* x/y */
                                     fb_width, fb_height);
      gles2_ctx->vtable->glScissor (0, 0, /* x/y */
                                    fb_width, fb_height);
      gles2_ctx->has_been_bound = TRUE;
    }

  return TRUE;
}

CoglGLES2Vtable *
cogl_gles2_get_current_vtable (void)
{
  return current_gles2_context ? current_gles2_context->vtable : NULL;
}

void
cogl_pop_gles2_context (CoglContext *ctx)
{
  CoglGLES2Context *gles2_ctx;
  const CoglWinsysVtable *winsys = ctx->display->renderer->winsys_vtable;

  _COGL_RETURN_IF_FAIL (ctx->gles2_context_stack.length > 0);

  g_queue_pop_tail (&ctx->gles2_context_stack);

  gles2_ctx = g_queue_peek_tail (&ctx->gles2_context_stack);

  if (gles2_ctx)
    {
      winsys->set_gles2_context (gles2_ctx, NULL);
      current_gles2_context = gles2_ctx;
    }
  else
    {
      winsys->restore_context (ctx);
      current_gles2_context = NULL;
    }
}

CoglTexture2D *
cogl_gles2_texture_2d_new_from_handle (CoglContext *ctx,
                                       CoglGLES2Context *gles2_ctx,
                                       unsigned int handle,
                                       int width,
                                       int height,
                                       CoglPixelFormat format)
{
  return cogl_texture_2d_gl_new_from_foreign (ctx,
                                              handle,
                                              width,
                                              height,
                                              format);
}

CoglBool
cogl_gles2_texture_get_handle (CoglTexture *texture,
                               unsigned int *handle,
                               unsigned int *target)
{
  return cogl_texture_get_gl_texture (texture, handle, target);
}
