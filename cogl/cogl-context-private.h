/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2013 Intel Corporation.
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

#ifndef __COGL_CONTEXT_PRIVATE_H
#define __COGL_CONTEXT_PRIVATE_H

#include "cogl-context.h"
#include "cogl-winsys-private.h"
#include "cogl-flags.h"

#ifdef COGL_HAS_XLIB_SUPPORT
#include "cogl-xlib-private.h"
#endif

#include "cogl-display-private.h"
#include "cogl-primitives.h"
#include "cogl-clip-stack.h"
#include "cogl-matrix-stack.h"
#include "cogl-pipeline-private.h"
#include "cogl-buffer-private.h"
#include "cogl-bitmask.h"
#include "cogl-atlas.h"
#include "cogl-driver.h"
#include "cogl-texture-driver.h"
#include "cogl-pipeline-cache.h"
#include "cogl-texture-2d.h"
#include "cogl-texture-3d.h"
#include "cogl-texture-rectangle.h"
#include "cogl-sampler-cache-private.h"
#include "cogl-gpu-info-private.h"
#include "cogl-gl-header.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-fence-private.h"
#include "cogl-poll-private.h"

typedef struct
{
  GLfloat v[3];
  GLfloat t[2];
  GLubyte c[4];
} CoglTextureGLVertex;

struct _CoglContext
{
  CoglObject _parent;

  CoglDisplay *display;

  CoglDriver driver;

  /* Information about the GPU and driver which we can use to
     determine certain workarounds */
  CoglGpuInfo gpu;

  /* vtables for the driver functions */
  const CoglDriverVtable *driver_vtable;
  const CoglTextureDriver *texture_driver;

  int glsl_major;
  int glsl_minor;

  /* Features cache */
  unsigned long features[COGL_FLAGS_N_LONGS_FOR_SIZE (_COGL_N_FEATURE_IDS)];
  CoglFeatureFlags feature_flags; /* legacy/deprecated feature flags */
  CoglPrivateFeatureFlags private_feature_flags;

  CoglBool needs_viewport_scissor_workaround;
  CoglFramebuffer *viewport_scissor_workaround_framebuffer;

  CoglPipeline *default_pipeline;
  CoglPipelineLayer *default_layer_0;
  CoglPipelineLayer *default_layer_n;
  CoglPipelineLayer *dummy_layer_dependant;

  GHashTable *attribute_name_states_hash;
  GArray *attribute_name_index_map;
  int n_attribute_names;

  CoglBitmask       enabled_builtin_attributes;
  CoglBitmask       enabled_texcoord_attributes;
  CoglBitmask       enabled_custom_attributes;

  /* These are temporary bitmasks that are used when disabling
   * builtin,texcoord and custom attribute arrays. They are here just
   * to avoid allocating new ones each time */
  CoglBitmask       enable_builtin_attributes_tmp;
  CoglBitmask       enable_texcoord_attributes_tmp;
  CoglBitmask       enable_custom_attributes_tmp;
  CoglBitmask       changed_bits_tmp;

  CoglBool          legacy_backface_culling_enabled;

  /* A few handy matrix constants */
  CoglMatrix        identity_matrix;
  CoglMatrix        y_flip_matrix;

  /* Value that was last used when calling glMatrixMode to avoid
     calling it multiple times */
  CoglMatrixMode    flushed_matrix_mode;

  /* The matrix stack entries that should be flushed during the next
   * pipeline state flush */
  CoglMatrixEntry *current_projection_entry;
  CoglMatrixEntry *current_modelview_entry;

  CoglMatrixEntry identity_entry;

  /* A cache of the last (immutable) matrix stack entries that were
   * flushed to the GL matrix builtins */
  CoglMatrixEntryCache builtin_flushed_projection;
  CoglMatrixEntryCache builtin_flushed_modelview;

  GArray           *texture_units;
  int               active_texture_unit;

  CoglPipelineFogState legacy_fog_state;

  /* Pipelines */
  CoglPipeline     *opaque_color_pipeline; /* used for set_source_color */
  CoglPipeline     *blended_color_pipeline; /* used for set_source_color */
  CoglPipeline     *texture_pipeline; /* used for set_source_texture */
  GString          *codegen_header_buffer;
  GString          *codegen_source_buffer;
  GString          *codegen_boilerplate_buffer;
  GList            *source_stack;

  int               legacy_state_set;

  CoglPipelineCache *pipeline_cache;

  /* Textures */
  CoglTexture2D *default_gl_texture_2d_tex;
  CoglTexture3D *default_gl_texture_3d_tex;
  CoglTextureRectangle *default_gl_texture_rect_tex;

  /* Central list of all framebuffers so all journals can be flushed
   * at any time. */
  GList            *framebuffers;

  /* Global journal buffers */
  GArray           *journal_flush_attributes_array;
  GArray           *journal_clip_bounds;

  GArray           *polygon_vertices;

  /* Some simple caching, to minimize state changes... */
  CoglPipeline     *current_pipeline;
  unsigned long     current_pipeline_changes_since_flush;
  CoglBool          current_pipeline_with_color_attrib;
  CoglBool          current_pipeline_unknown_color_alpha;
  unsigned long     current_pipeline_age;

  CoglBool          gl_blend_enable_cache;

  CoglBool              depth_test_enabled_cache;
  CoglDepthTestFunction depth_test_function_cache;
  CoglBool              depth_writing_enabled_cache;
  float                 depth_range_near_cache;
  float                 depth_range_far_cache;

  CoglBool              legacy_depth_test_enabled;

  CoglBuffer       *current_buffer[COGL_BUFFER_BIND_TARGET_COUNT];

  /* Framebuffers */
  GSList           *framebuffer_stack;
  CoglFramebuffer  *window_buffer;
  unsigned long     current_draw_buffer_state_flushed;
  unsigned long     current_draw_buffer_changes;
  CoglFramebuffer  *current_draw_buffer;
  CoglFramebuffer  *current_read_buffer;

  gboolean have_last_offscreen_allocate_flags;
  CoglOffscreenAllocateFlags last_offscreen_allocate_flags;

  GHashTable *swap_callback_closures;
  int next_swap_callback_id;

  CoglList onscreen_events_queue;
  CoglList onscreen_dirty_queue;
  CoglClosure *onscreen_dispatch_idle;

  CoglGLES2Context *current_gles2_context;
  GQueue gles2_context_stack;

  /* Primitives */
  CoglPath         *current_path;
  CoglPipeline     *stencil_pipeline;

  /* Pre-generated VBOs containing indices to generate GL_TRIANGLES
     out of a vertex array of quads */
  CoglIndices      *quad_buffer_indices_byte;
  unsigned int      quad_buffer_indices_len;
  CoglIndices      *quad_buffer_indices;

  CoglIndices      *rectangle_byte_indices;
  CoglIndices      *rectangle_short_indices;
  int               rectangle_short_indices_len;

  CoglBool          in_begin_gl_block;

  CoglPipeline     *texture_download_pipeline;
  CoglPipeline     *blit_texture_pipeline;

  GSList           *atlases;
  GHookList         atlas_reorganize_callbacks;

  /* This debugging variable is used to pick a colour for visually
     displaying the quad batches. It needs to be global so that it can
     be reset by cogl_clear. It needs to be reset to increase the
     chances of getting the same colour during an animation */
  uint8_t            journal_rectangles_color;

  /* Cached values for GL_MAX_TEXTURE_[IMAGE_]UNITS to avoid calling
     glGetInteger too often */
  GLint             max_texture_units;
  GLint             max_texture_image_units;
  GLint             max_activateable_texture_units;

  /* Fragment processing programs */
  CoglHandle              current_program;

  CoglPipelineProgramType current_fragment_program_type;
  CoglPipelineProgramType current_vertex_program_type;
  GLuint                  current_gl_program;

  CoglBool current_gl_dither_enabled;
  CoglColorMask current_gl_color_mask;

  /* Clipping */
  /* TRUE if we have a valid clipping stack flushed. In that case
     current_clip_stack will describe what the current state is. If
     this is FALSE then the current clip stack is completely unknown
     so it will need to be reflushed. In that case current_clip_stack
     doesn't need to be a valid pointer. We can't just use NULL in
     current_clip_stack to mark a dirty state because NULL is a valid
     stack (meaning no clipping) */
  CoglBool          current_clip_stack_valid;
  /* The clip state that was flushed. This isn't intended to be used
     as a stack to push and pop new entries. Instead the current stack
     that the user wants is part of the framebuffer state. This is
     just used to record the flush state so we can avoid flushing the
     same state multiple times. When the clip state is flushed this
     will hold a reference */
  CoglClipStack    *current_clip_stack;
  /* Whether the stencil buffer was used as part of the current clip
     state. If TRUE then any further use of the stencil buffer (such
     as for drawing paths) would need to be merged with the existing
     stencil buffer */
  CoglBool          current_clip_stack_uses_stencil;

  /* This is used as a temporary buffer to fill a CoglBuffer when
     cogl_buffer_map fails and we only want to map to fill it with new
     data */
  GByteArray       *buffer_map_fallback_array;
  CoglBool          buffer_map_fallback_in_use;
  size_t            buffer_map_fallback_offset;

  CoglWinsysRectangleState rectangle_state;

  CoglSamplerCache *sampler_cache;

  /* FIXME: remove these when we remove the last xlib based clutter
   * backend. they should be tracked as part of the renderer but e.g.
   * the eglx backend doesn't yet have a corresponding Cogl winsys
   * and so we wont have a renderer in that case. */
#ifdef COGL_HAS_XLIB_SUPPORT
  int damage_base;
  /* List of callback functions that will be given every Xlib event */
  GSList *event_filters;
  /* Current top of the XError trap state stack. The actual memory for
     these is expected to be allocated on the stack by the caller */
  CoglXlibTrapState *trap_state;
#endif

  unsigned long winsys_features
    [COGL_FLAGS_N_LONGS_FOR_SIZE (COGL_WINSYS_FEATURE_N_FEATURES)];
  void *winsys;

  /* Array of names of uniforms. These are used like quarks to give a
     unique number to each uniform name except that we ensure that
     they increase sequentially so that we can use the id as an index
     into a bitfield representing the uniforms that a pipeline
     overrides from its parent. */
  GPtrArray *uniform_names;
  /* A hash table to quickly get an index given an existing name. The
     name strings are owned by the uniform_names array. The values are
     the uniform location cast to a pointer. */
  GHashTable *uniform_name_hash;
  int n_uniform_names;

  CoglPollSource *fences_poll_source;
  CoglList fences;

  /* This defines a list of function pointers that Cogl uses from
     either GL or GLES. All functions are accessed indirectly through
     these pointers rather than linking to them directly */
#ifndef APIENTRY
#define APIENTRY
#endif

#define COGL_EXT_BEGIN(name, \
                       min_gl_major, min_gl_minor, \
                       gles_availability, \
                       extension_suffixes, extension_names)
#define COGL_EXT_FUNCTION(ret, name, args) \
  ret (APIENTRY * name) args;
#define COGL_EXT_END()

#include "gl-prototypes/cogl-all-functions.h"

#undef COGL_EXT_BEGIN
#undef COGL_EXT_FUNCTION
#undef COGL_EXT_END
};

CoglContext *
_cogl_context_get_default ();

const CoglWinsysVtable *
_cogl_context_get_winsys (CoglContext *context);

/* Query the GL extensions and lookup the corresponding function
 * pointers. Theoretically the list of extensions can change for
 * different GL contexts so it is the winsys backend's responsiblity
 * to know when to re-query the GL extensions. The backend should also
 * check whether the GL context is supported by Cogl. If not it should
 * return FALSE and set @error */
CoglBool
_cogl_context_update_features (CoglContext *context,
                               CoglError **error);

/* Obtains the context and returns retval if NULL */
#define _COGL_GET_CONTEXT(ctxvar, retval) \
CoglContext *ctxvar = _cogl_context_get_default (); \
if (ctxvar == NULL) return retval;

#define NO_RETVAL

void
_cogl_context_set_current_projection_entry (CoglContext *context,
                                            CoglMatrixEntry *entry);

void
_cogl_context_set_current_modelview_entry (CoglContext *context,
                                           CoglMatrixEntry *entry);

/*
 * _cogl_context_get_gl_extensions:
 * @context: A CoglContext
 *
 * Return value: a NULL-terminated array of strings representing the
 *   supported extensions by the current driver. This array is owned
 *   by the caller and should be freed with g_strfreev().
 */
char **
_cogl_context_get_gl_extensions (CoglContext *context);

const char *
_cogl_context_get_gl_version (CoglContext *context);

#endif /* __COGL_CONTEXT_PRIVATE_H */
