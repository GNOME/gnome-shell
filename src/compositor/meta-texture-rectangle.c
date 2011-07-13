/*
 * texture rectangle
 *
 * A small utility function to help create a rectangle texture
 *
 * Authored By Neil Roberts  <neil@linux.intel.com>
 *
 * Copyright (C) 2011 Intel Corporation
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

#include <config.h>

#include "meta-texture-rectangle.h"

#ifdef GL_TEXTURE_RECTANGLE_ARB

static void (* pf_glGetIntegerv) (GLenum pname, GLint *params);
static void (* pf_glTexImage2D) (GLenum target, GLint level,
                                 GLint internalFormat,
                                 GLsizei width, GLsizei height,
                                 GLint border, GLenum format, GLenum type,
                                 const GLvoid *pixels);
static void (* pf_glGenTextures) (GLsizei n, GLuint *textures);
static void (* pf_glDeleteTextures) (GLsizei n, const GLuint *texture);
static void (* pf_glBindTexture) (GLenum target, GLuint texture);

static void
rectangle_texture_destroy_cb (void *user_data)
{
  GLuint tex = GPOINTER_TO_UINT (user_data);

  pf_glDeleteTextures (1, &tex);
}

#endif /* GL_TEXTURE_RECTANGLE_ARB */

CoglHandle
meta_texture_rectangle_new (unsigned int width,
                            unsigned int height,
                            CoglTextureFlags flags,
                            CoglPixelFormat format,
                            GLenum internal_gl_format,
                            GLenum internal_format,
                            unsigned int rowstride,
                            const guint8 *data)
{
  CoglHandle cogl_tex = COGL_INVALID_HANDLE;

#ifdef GL_TEXTURE_RECTANGLE_ARB

  static CoglUserDataKey user_data_key;
  GLint old_binding;
  GLuint tex;

  if (pf_glGenTextures == NULL)
    {
      pf_glGetIntegerv = (void *) cogl_get_proc_address ("glGetIntegerv");
      pf_glTexImage2D = (void *) cogl_get_proc_address ("glTexImage2D");
      pf_glGenTextures = (void *) cogl_get_proc_address ("glGenTextures");
      pf_glDeleteTextures = (void *) cogl_get_proc_address ("glDeleteTextures");
      pf_glBindTexture = (void *) cogl_get_proc_address ("glBindTexture");
    }

  pf_glGenTextures (1, &tex);
  pf_glGetIntegerv (GL_TEXTURE_BINDING_RECTANGLE_ARB, &old_binding);
  pf_glBindTexture (GL_TEXTURE_RECTANGLE_ARB, tex);
  pf_glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0,
                   internal_gl_format, width, height,
                   0, internal_gl_format,
                   GL_UNSIGNED_BYTE, NULL);
  pf_glBindTexture (GL_TEXTURE_RECTANGLE_ARB, old_binding);

  cogl_tex = cogl_texture_new_from_foreign (tex,
                                            GL_TEXTURE_RECTANGLE_ARB,
                                            width, height,
                                            0, 0, /* no waste */
                                            internal_format);

  /* Cogl won't destroy the GL texture when a foreign texture is used
     so we need to destroy it manually. We can set a destroy
     notification callback to do this transparently */
  cogl_object_set_user_data (cogl_tex,
                             &user_data_key,
                             GUINT_TO_POINTER (tex),
                             rectangle_texture_destroy_cb);

  /* Use cogl_texture_set_region instead of uploading the data
     directly with GL calls so that we can let Cogl deal with setting
     the pixel store parameters and handling format conversion */
  if (data)
    cogl_texture_set_region (cogl_tex,
                             0, 0, /* src x/y */
                             0, 0, /* dst x/y */
                             width, height, /* dst width/height */
                             width, height, /* src width/height */
                             format,
                             rowstride,
                             data);

#endif /* GL_TEXTURE_RECTANGLE_ARB */

  return cogl_tex;
}
