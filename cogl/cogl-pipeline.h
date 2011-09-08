/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 *
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_PIPELINE_H__
#define __COGL_PIPELINE_H__

G_BEGIN_DECLS

#include <cogl/cogl-types.h>

#ifdef COGL_ENABLE_EXPERIMENTAL_API

/**
 * SECTION:cogl-pipeline
 * @short_description: Functions for creating and manipulating the GPU
 *                     pipeline
 *
 * Cogl allows creating and manipulating objects representing the full
 * configuration of the GPU pipeline. In simplified terms the GPU
 * pipeline takes primitive geometry as the input, it first performs
 * vertex processing, allowing you to deform your geometry, then
 * rasterizes that (turning it from pure geometry into fragments) then
 * performs fragment processing including depth testing and texture
 * mapping. Finally it blends the result with the framebuffer.
 */

typedef struct _CoglPipeline	      CoglPipeline;

#define COGL_PIPELINE(OBJECT) ((CoglPipeline *)OBJECT)

#define cogl_pipeline_new cogl_pipeline_new_EXP
/**
 * cogl_pipeline_new:
 *
 * Allocates and initializes a default simple pipeline that will color
 * a primitive white.
 *
 * Return value: a pointer to a new #CoglPipeline
 *
 * Since: 2.0
 * Stability: Unstable
 */
CoglPipeline *
cogl_pipeline_new (void);

#define cogl_pipeline_copy cogl_pipeline_copy_EXP
/**
 * cogl_pipeline_copy:
 * @source: a #CoglPipeline object to copy
 *
 * Creates a new pipeline with the configuration copied from the
 * source pipeline.
 *
 * We would strongly advise developers to always aim to use
 * cogl_pipeline_copy() instead of cogl_pipeline_new() whenever there will
 * be any similarity between two pipelines. Copying a pipeline helps Cogl
 * keep track of a pipelines ancestry which we may use to help minimize GPU
 * state changes.
 *
 * Returns: a pointer to the newly allocated #CoglPipeline
 *
 * Since: 2.0
 * Stability: Unstable
 */
CoglPipeline *
cogl_pipeline_copy (CoglPipeline *source);

#define cogl_is_pipeline cogl_is_pipeline_EXP
/**
 * cogl_is_pipeline:
 * @handle: A CoglHandle
 *
 * Gets whether the given handle references an existing pipeline object.
 *
 * Return value: %TRUE if the handle references a #CoglPipeline,
 *   %FALSE otherwise
 *
 * Since: 2.0
 * Stability: Unstable
 */
gboolean
cogl_is_pipeline (CoglHandle handle);

/**
 * CoglPipelineLayerCallback:
 * @pipeline: The #CoglPipeline whos layers are being iterated
 * @layer_index: The current layer index
 * @user_data: The private data passed to cogl_pipeline_foreach_layer()
 *
 * The callback prototype used with cogl_pipeline_foreach_layer() for
 * iterating all the layers of a @pipeline.
 *
 * Since: 2.0
 * Stability: Unstable
 */
typedef gboolean (*CoglPipelineLayerCallback) (CoglPipeline *pipeline,
                                               int layer_index,
                                               void *user_data);

#define cogl_pipeline_foreach_layer cogl_pipeline_foreach_layer_EXP
/**
 * cogl_pipeline_foreach_layer:
 * @pipeline: A #CoglPipeline object
 * @callback: A #CoglPipelineLayerCallback to be called for each layer
 *            index
 * @user_data: Private data that will be passed to the callback
 *
 * Iterates all the layer indices of the given @pipeline.
 *
 * Since: 2.0
 * Stability: Unstable
 */
void
cogl_pipeline_foreach_layer (CoglPipeline *pipeline,
                             CoglPipelineLayerCallback callback,
                             void *user_data);

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

G_END_DECLS

#endif /* __COGL_PIPELINE_H__ */
