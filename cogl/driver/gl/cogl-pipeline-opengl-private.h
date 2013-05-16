/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_PIPELINE_OPENGL_PRIVATE_H
#define __COGL_PIPELINE_OPENGL_PRIVATE_H

#include "cogl-pipeline-private.h"
#include "cogl-matrix-stack.h"

/*
 * cogl-pipeline.c owns the GPU's texture unit state so we have some
 * private structures for describing the current state of a texture
 * unit that we track in a per context array (ctx->texture_units) that
 * grows according to the largest texture unit used so far...
 *
 * Roughly speaking the members in this structure are of two kinds:
 * either they are a low level reflection of the state we send to
 * OpenGL or they are for high level meta data assoicated with the
 * texture unit when flushing CoglPipelineLayers that is typically
 * used to optimize subsequent re-flushing of the same layer.
 *
 * The low level members are at the top, and the high level members
 * start with the .layer member.
 */
typedef struct _CoglTextureUnit
{
  /* The base 0 texture unit index which can be used with
   * glActiveTexture () */
  int                index;

  /* The GL target currently glEnabled or 0 if nothing is
   * enabled. This is only used by the fixed pipeline fragend */
  GLenum             enabled_gl_target;

  /* The raw GL texture object name for which we called glBindTexture when
   * we flushed the last layer. (NB: The CoglTexture associated
   * with a layer may represent more than one GL texture) */
  GLuint             gl_texture;
  /* The target of the GL texture object. This is just used so that we
   * can quickly determine the intended target to flush when
   * dirty_gl_texture == TRUE */
  GLenum             gl_target;

  /* Foreign textures are those not created or deleted by Cogl. If we ever
   * call glBindTexture for a foreign texture then the next time we are
   * asked to glBindTexture we can't try and optimize a redundant state
   * change because we don't know if the original texture name was deleted
   * and now we are being asked to bind a recycled name. */
  CoglBool           is_foreign;

  /* We have many components in Cogl that need to temporarily bind arbitrary
   * textures e.g. to query texture object parameters and since we don't
   * want that to result in too much redundant reflushing of layer state
   * when all that's needed is to re-bind the layer's gl_texture we use this
   * to track when the unit->gl_texture state is out of sync with the GL
   * texture object really bound too (GL_TEXTURE0+unit->index).
   *
   * XXX: as a further optimization cogl-pipeline.c uses a convention
   * of always using texture unit 1 for these transient bindings so we
   * can assume this is only ever TRUE for unit 1.
   */
  CoglBool           dirty_gl_texture;

  /* A matrix stack giving us the means to associate a texture
   * transform matrix with the texture unit. */
  CoglMatrixStack   *matrix_stack;

  /*
   * Higher level layer state associated with the unit...
   */

  /* The CoglPipelineLayer whos state was flushed to update this
   * texture unit last.
   *
   * This will be set to NULL if the layer is modified or freed which
   * means when we come to flush a layer; if this pointer is still
   * valid and == to the layer being flushed we don't need to update
   * any texture unit state. */
  CoglPipelineLayer *layer;

  /* To help minimize the state changes required we track the
   * difference flags associated with the layer whos state was last
   * flushed to update this texture unit.
   *
   * Note: we track this explicitly because .layer may get invalidated
   * if that layer is modified or deleted. Even if the layer is
   * invalidated though these flags can be used to optimize the state
   * flush of the next layer
   */
  unsigned long      layer_changes_since_flush;

  /* Whenever a CoglTexture's internal GL texture storage changes
   * cogl-pipeline.c is notified with a call to
   * _cogl_pipeline_texture_storage_change_notify which inturn sets
   * this to TRUE for each texture unit that it is currently bound
   * too. When we later come to flush some pipeline state then we will
   * always check this to potentially force an update of the texture
   * state even if the pipeline hasn't changed. */
  CoglBool           texture_storage_changed;

} CoglTextureUnit;

CoglTextureUnit *
_cogl_get_texture_unit (int index_);

void
_cogl_destroy_texture_units (void);

void
_cogl_set_active_texture_unit (int unit_index);

void
_cogl_bind_gl_texture_transient (GLenum gl_target,
                                 GLuint gl_texture,
                                 CoglBool is_foreign);

void
_cogl_delete_gl_texture (GLuint gl_texture);

void
_cogl_pipeline_flush_gl_state (CoglContext *context,
                               CoglPipeline *pipeline,
                               CoglFramebuffer *framebuffer,
                               CoglBool skip_gl_state,
                               CoglBool unknown_color_alpha);

#endif /* __COGL_PIPELINE_OPENGL_PRIVATE_H */

