/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* These functions are just here temporarily for the 1.10.x releases
   to maintain ABI compatibility. They will be removed again
   immediately once the branch for 1.12.x is created */

#include "cogl2-compatibility.h"
#include "cogl-framebuffer.h"
#include "cogl-framebuffer-private.h"
#include "cogl-index-buffer.h"
#include "cogl-pipeline.h"

void
cogl_clip_push_from_path (CoglPath *path)
{
  cogl_framebuffer_push_path_clip (cogl_get_draw_framebuffer (), path);
}

/* These were never declared in a public header so we might as well
   keep it that way. The declarations here are just to avoid a
   warning */
GQuark
cogl_display_error_quark (void);

GQuark
cogl_onscreen_template_error_quark (void);

GQuark
cogl_swap_chain_error_quark (void);

GQuark
cogl_texture_3d_error_quark (void);

CoglBool
cogl_index_buffer_allocate (CoglIndexBuffer *indices,
                            CoglError *error);

CoglBool
cogl_is_journal (void *object);

void
cogl_vdraw_indexed_attributes (CoglFramebuffer *framebuffer,
                               CoglPipeline *pipeline,
                               CoglVerticesMode mode,
                               int first_vertex,
                               int n_vertices,
                               CoglIndices *indices,
                               ...);

GQuark
cogl_display_error_quark (void)
{
  return g_quark_from_static_string ("cogl-display-error-quark");
}

GQuark
cogl_onscreen_template_error_quark (void)
{
  return g_quark_from_static_string ("cogl-onscreen-template-error-quark");
}

GQuark
cogl_swap_chain_error_quark (void)
{
  return g_quark_from_static_string ("cogl-swap-chain-error-quark");
}

GQuark
cogl_texture_3d_error_quark (void)
{
  return g_quark_from_static_string ("cogl-texture-3d-error-quark");
}

CoglBool
cogl_index_buffer_allocate (CoglIndexBuffer *indices,
                            CoglError *error)
{
  return TRUE;
}

CoglBool
cogl_is_journal (void *object)
{
  /* There's no way to get a pointer to a journal so this will never
     return TRUE from an application's perspective */
  return FALSE;
}

void
cogl_vdraw_indexed_attributes (CoglFramebuffer *framebuffer,
                               CoglPipeline *pipeline,
                               CoglVerticesMode mode,
                               int first_vertex,
                               int n_vertices,
                               CoglIndices *indices,
                               ...)
{
  va_list ap;
  int n_attributes;
  CoglAttribute **attributes;
  int i;
  CoglAttribute *attribute;

  va_start (ap, indices);
  for (n_attributes = 0; va_arg (ap, CoglAttribute *); n_attributes++)
    ;
  va_end (ap);

  attributes = g_alloca (sizeof (CoglAttribute *) * n_attributes);

  va_start (ap, indices);
  for (i = 0; (attribute = va_arg (ap, CoglAttribute *)); i++)
    attributes[i] = attribute;
  va_end (ap);

  _cogl_framebuffer_draw_indexed_attributes (framebuffer,
                                             pipeline,
                                             mode,
                                             first_vertex,
                                             n_vertices,
                                             indices,
                                             attributes,
                                             n_attributes,
                                             COGL_DRAW_SKIP_LEGACY_STATE);
}
