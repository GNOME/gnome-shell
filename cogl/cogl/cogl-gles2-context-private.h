/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Intel Corporation.
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
 * Authors:
 *  Tomeu Vizoso <tomeu.vizoso@collabora.com>
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#ifndef __COGL_GLES2_CONTEXT_PRIVATE_H
#define __COGL_GLES2_CONTEXT_PRIVATE_H

#include <glib.h>

#include "cogl-object-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-list.h"

typedef struct _CoglGLES2Offscreen
{
  CoglList link;
  CoglOffscreen *original_offscreen;
  CoglGLFramebuffer gl_framebuffer;
} CoglGLES2Offscreen;

typedef struct
{
  /* GL's ID for the shader */
  GLuint object_id;
  /* Shader type */
  GLenum type;

  /* Number of references to this shader. The shader will have one
   * reference when it is created. This reference will be removed when
   * glDeleteShader is called. An additional reference will be taken
   * whenever the shader is attached to a program. This is necessary
   * to correctly detect when a shader is destroyed because
   * glDeleteShader doesn't actually delete the object if it is
   * attached to a program */
  int ref_count;

  /* Set once this object has had glDeleteShader called on it. We need
   * to keep track of this so we don't deref the data twice if the
   * application calls glDeleteShader multiple times */
  CoglBool deleted;
} CoglGLES2ShaderData;

typedef enum
{
  COGL_GLES2_FLIP_STATE_UNKNOWN,
  COGL_GLES2_FLIP_STATE_NORMAL,
  COGL_GLES2_FLIP_STATE_FLIPPED
} CoglGLES2FlipState;

typedef struct
{
  /* GL's ID for the program */
  GLuint object_id;

  /* List of shaders attached to this program */
  GList *attached_shaders;

  /* Reference count. There can be up to two references. One of these
   * will exist between glCreateProgram and glDeleteShader, the other
   * will exist while the program is made current. This is necessary
   * to correctly detect when the program is deleted because
   * glDeleteShader will delay the deletion if the program is
   * current */
  int ref_count;

  /* Set once this object has had glDeleteProgram called on it. We need
   * to keep track of this so we don't deref the data twice if the
   * application calls glDeleteProgram multiple times */
  CoglBool deleted;

  GLuint flip_vector_location;

  /* A cache of what value we've put in the flip vector uniform so
   * that we don't flush unless it's changed */
  CoglGLES2FlipState flip_vector_state;

  CoglGLES2Context *context;
} CoglGLES2ProgramData;

/* State tracked for each texture unit */
typedef struct
{
  /* The currently bound texture for the GL_TEXTURE_2D */
  GLuint current_texture_2d;
} CoglGLES2TextureUnitData;

/* State tracked for each texture object */
typedef struct
{
  /* GL's ID for this object */
  GLuint object_id;

  GLenum target;

  /* The details for texture when it has a 2D target */
  int width, height;
  GLenum format;
} CoglGLES2TextureObjectData;

struct _CoglGLES2Context
{
  CoglObject _parent;

  CoglContext *context;

  /* This is set to FALSE until the first time the GLES2 context is
   * bound to something. We need to keep track of this so we can set
   * the viewport and scissor the first time it is bound. */
  CoglBool has_been_bound;

  CoglFramebuffer *read_buffer;
  CoglGLES2Offscreen *gles2_read_buffer;
  CoglFramebuffer *write_buffer;
  CoglGLES2Offscreen *gles2_write_buffer;

  GLuint current_fbo_handle;

  CoglList foreign_offscreens;

  CoglGLES2Vtable *vtable;

  /* Hash table mapping GL's IDs for shaders and objects to ShaderData
   * and ProgramData so that we can maintain extra data for these
   * objects. Although technically the IDs will end up global across
   * all GLES2 contexts because they will all be in the same share
   * list, we don't really want to expose this outside of the Cogl API
   * so we will assume it is undefined behaviour if an application
   * relies on this. */
  GHashTable *shader_map;
  GHashTable *program_map;

  /* Currently in use program. We need to keep track of this so that
   * we can keep a reference to the data for the program while it is
   * current */
  CoglGLES2ProgramData *current_program;

  /* Whether the currently bound framebuffer needs flipping. This is
   * used to check for changes so that we can dirty the following
   * state flags */
  CoglGLES2FlipState current_flip_state;

  /* The following state is tracked separately from the GL context
   * because we need to modify it depending on whether we are flipping
   * the geometry. */
  CoglBool viewport_dirty;
  int viewport[4];
  CoglBool scissor_dirty;
  int scissor[4];
  CoglBool front_face_dirty;
  GLenum front_face;

  /* We need to keep track of the pack alignment so we can flip the
   * results of glReadPixels read from a CoglOffscreen */
  int pack_alignment;

  /* A hash table of CoglGLES2TextureObjects indexed by the texture
   * object ID so that we can track some state */
  GHashTable *texture_object_map;

  /* Array of CoglGLES2TextureUnits to keep track of state for each
   * texture unit */
  GArray *texture_units;

  /* The currently active texture unit indexed from 0 (not from
   * GL_TEXTURE0) */
  int current_texture_unit;

  void *winsys;
};

#endif /* __COGL_GLES2_CONTEXT_PRIVATE_H */
