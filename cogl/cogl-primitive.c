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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-object-private.h"
#include "cogl-primitive.h"
#include "cogl-primitive-private.h"

#include <stdarg.h>

static void _cogl_primitive_free (CoglPrimitive *primitive);

COGL_OBJECT_DEFINE (Primitive, primitive);

/* XXX: should we have an n_attributes arg instead of NULL terminating? */
CoglPrimitive *
cogl_primitive_new_with_attributes_array (CoglVerticesMode mode,
                                          int n_vertices,
                                          CoglVertexAttribute **attributes)
{
  CoglPrimitive *primitive = g_slice_new (CoglPrimitive);
  int i;

  primitive->mode = mode;
  primitive->first_vertex = 0;
  primitive->n_vertices = n_vertices;
  primitive->indices = NULL;
  primitive->attributes =
    g_array_new (TRUE, FALSE, sizeof (CoglVertexAttribute *));

  for (i = 0; attributes[i]; i++)
    {
      CoglVertexAttribute *attribute = attributes[i];
      cogl_object_ref (attribute);

      g_return_val_if_fail (cogl_is_vertex_attribute (attributes), NULL);

      g_array_append_val (primitive->attributes, attribute);
    }

  return _cogl_primitive_object_new (primitive);
}

CoglPrimitive *
cogl_primitive_new (CoglVerticesMode mode,
                    int n_vertices,
                    ...)
{
  va_list ap;
  int n_attributes;
  CoglVertexAttribute **attributes;
  int i;
  CoglVertexAttribute *attribute;

  va_start (ap, n_vertices);
  for (n_attributes = 0; va_arg (ap, CoglVertexAttribute *); n_attributes++)
    ;
  va_end (ap);

  attributes = g_alloca (sizeof (CoglVertexAttribute *) * (n_attributes + 1));
  attributes[n_attributes] = NULL;

  va_start (ap, n_vertices);
  for (i = 0; (attribute = va_arg (ap, CoglVertexAttribute *)); i++)
    attributes[i] = attribute;
  va_end (ap);

  return cogl_primitive_new_with_attributes_array (mode, n_vertices,
                                                   attributes);
}

CoglPrimitive *
cogl_primitive_new_p3 (CoglVerticesMode mode,
                       int n_vertices,
                       const CoglP3Vertex *data)
{
  CoglVertexArray *array =
    cogl_vertex_array_new (n_vertices * sizeof (CoglP3Vertex));
  CoglBuffer *buffer = COGL_BUFFER (array);
  CoglVertexAttribute *attributes[2];
  CoglPrimitive *prim;

  cogl_buffer_set_data (buffer, 0, (guint8 *)data,
                        n_vertices * sizeof (CoglP3Vertex));
  attributes[0] =
    cogl_vertex_attribute_new (array,
                               "cogl_position_in",
                               sizeof (CoglP3Vertex),
                               offsetof (CoglP3Vertex, x),
                               3,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] = NULL;
  prim = cogl_primitive_new_with_attributes_array (mode, n_vertices,
                                                   attributes);
  cogl_object_unref (prim);
  return prim;
}

CoglPrimitive *
cogl_primitive_new_p2c4 (CoglVerticesMode mode,
                         int n_vertices,
                         const CoglP2C4Vertex *data)
{
  CoglVertexArray *array =
    cogl_vertex_array_new (n_vertices * sizeof (CoglP2C4Vertex));
  CoglBuffer *buffer = COGL_BUFFER (array);
  CoglVertexAttribute *attributes[3];
  CoglPrimitive *prim;

  cogl_buffer_set_data (buffer, 0, (guint8 *)data,
                        n_vertices * sizeof (CoglP2C4Vertex));
  attributes[0] =
    cogl_vertex_attribute_new (array,
                               "cogl_position_in",
                               sizeof (CoglP2C4Vertex),
                               offsetof (CoglP2C4Vertex, x),
                               2,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] =
    cogl_vertex_attribute_new (array,
                               "cogl_color_in",
                               sizeof (CoglP2C4Vertex),
                               offsetof (CoglP2C4Vertex, r),
                               4,
                               COGL_VERTEX_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
  attributes[2] = NULL;
  prim = cogl_primitive_new_with_attributes_array (mode, n_vertices,
                                                   attributes);
  cogl_object_unref (prim);
  return prim;
}

CoglPrimitive *
cogl_primitive_new_p3c4 (CoglVerticesMode mode,
                         int n_vertices,
                         const CoglP3C4Vertex *data)
{
  CoglVertexArray *array =
    cogl_vertex_array_new (n_vertices * sizeof (CoglP3C4Vertex));
  CoglBuffer *buffer = COGL_BUFFER (array);
  CoglVertexAttribute *attributes[3];
  CoglPrimitive *prim;

  cogl_buffer_set_data (buffer, 0, (guint8 *)data,
                        n_vertices * sizeof (CoglP3C4Vertex));
  attributes[0] =
    cogl_vertex_attribute_new (array,
                               "cogl_position_in",
                               sizeof (CoglP3C4Vertex),
                               offsetof (CoglP3C4Vertex, x),
                               3,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] =
    cogl_vertex_attribute_new (array,
                               "cogl_color_in",
                               sizeof (CoglP3C4Vertex),
                               offsetof (CoglP3C4Vertex, r),
                               4,
                               COGL_VERTEX_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
  attributes[2] = NULL;
  prim = cogl_primitive_new_with_attributes_array (mode, n_vertices,
                                                   attributes);
  cogl_object_unref (prim);
  return prim;
}

CoglPrimitive *
cogl_primitive_new_p2t2 (CoglVerticesMode mode,
                         int n_vertices,
                         const CoglP2T2Vertex *data)
{
  CoglVertexArray *array =
    cogl_vertex_array_new (n_vertices * sizeof (CoglP2T2Vertex));
  CoglBuffer *buffer = COGL_BUFFER (array);
  CoglVertexAttribute *attributes[3];
  CoglPrimitive *prim;

  cogl_buffer_set_data (buffer, 0, (guint8 *)data,
                        n_vertices * sizeof (CoglP2T2Vertex));
  attributes[0] =
    cogl_vertex_attribute_new (array,
                               "cogl_position_in",
                               sizeof (CoglP2T2Vertex),
                               offsetof (CoglP2T2Vertex, x),
                               2,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] =
    cogl_vertex_attribute_new (array,
                               "cogl_tex_coord0_in",
                               sizeof (CoglP2T2Vertex),
                               offsetof (CoglP2T2Vertex, x),
                               2,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);
  attributes[2] = NULL;
  prim = cogl_primitive_new_with_attributes_array (mode, n_vertices,
                                                   attributes);
  cogl_object_unref (array);
  return prim;
}

CoglPrimitive *
cogl_primitive_new_p3t2 (CoglVerticesMode mode,
                         int n_vertices,
                         const CoglP3T2Vertex *data)
{
  CoglVertexArray *array =
    cogl_vertex_array_new (n_vertices * sizeof (CoglP3T2Vertex));
  CoglBuffer *buffer = COGL_BUFFER (array);
  CoglVertexAttribute *attributes[3];
  CoglPrimitive *prim;

  cogl_buffer_set_data (buffer, 0, (guint8 *)data,
                        n_vertices * sizeof (CoglP3T2Vertex));
  attributes[0] =
    cogl_vertex_attribute_new (array,
                               "cogl_position_in",
                               sizeof (CoglP3T2Vertex),
                               offsetof (CoglP3T2Vertex, x),
                               3,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] =
    cogl_vertex_attribute_new (array,
                               "cogl_tex_coord0_in",
                               sizeof (CoglP3T2Vertex),
                               offsetof (CoglP3T2Vertex, x),
                               2,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);
  attributes[2] = NULL;
  prim = cogl_primitive_new_with_attributes_array (mode, n_vertices,
                                                   attributes);
  cogl_object_unref (prim);
  return prim;

}

CoglPrimitive *
cogl_primitive_new_p2t2c4 (CoglVerticesMode mode,
                           int n_vertices,
                           const CoglP2T2C4Vertex *data)
{
  CoglVertexArray *array =
    cogl_vertex_array_new (n_vertices * sizeof (CoglP2T2C4Vertex));
  CoglBuffer *buffer = COGL_BUFFER (array);
  CoglVertexAttribute *attributes[4];
  CoglPrimitive *prim;

  cogl_buffer_set_data (buffer, 0, (guint8 *)data,
                        n_vertices * sizeof (CoglP2T2C4Vertex));
  attributes[0] =
    cogl_vertex_attribute_new (array,
                               "cogl_position_in",
                               sizeof (CoglP2T2C4Vertex),
                               offsetof (CoglP2T2C4Vertex, x),
                               2,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] =
    cogl_vertex_attribute_new (array,
                               "cogl_tex_coord0_in",
                               sizeof (CoglP2T2C4Vertex),
                               offsetof (CoglP2T2C4Vertex, x),
                               2,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);
  attributes[2] =
    cogl_vertex_attribute_new (array,
                               "cogl_color_in",
                               sizeof (CoglP2T2C4Vertex),
                               offsetof (CoglP2T2C4Vertex, r),
                               4,
                               COGL_VERTEX_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
  attributes[3] = NULL;
  prim = cogl_primitive_new_with_attributes_array (mode, n_vertices,
                                                   attributes);
  cogl_object_unref (prim);
  return prim;
}

CoglPrimitive *
cogl_primitive_new_p3t2c4 (CoglVerticesMode mode,
                           int n_vertices,
                           const CoglP3T2C4Vertex *data)
{
  CoglVertexArray *array =
    cogl_vertex_array_new (n_vertices * sizeof (CoglP3T2C4Vertex));
  CoglBuffer *buffer = COGL_BUFFER (array);
  CoglVertexAttribute *attributes[4];
  CoglPrimitive *prim;

  cogl_buffer_set_data (buffer, 0, (guint8 *)data,
                        n_vertices * sizeof (CoglP3T2C4Vertex));
  attributes[0] =
    cogl_vertex_attribute_new (array,
                               "cogl_position_in",
                               sizeof (CoglP3T2C4Vertex),
                               offsetof (CoglP3T2C4Vertex, x),
                               3,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);
  attributes[1] =
    cogl_vertex_attribute_new (array,
                               "cogl_tex_coord0_in",
                               sizeof (CoglP3T2C4Vertex),
                               offsetof (CoglP3T2C4Vertex, x),
                               2,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);
  attributes[2] =
    cogl_vertex_attribute_new (array,
                               "cogl_color_in",
                               sizeof (CoglP3T2C4Vertex),
                               offsetof (CoglP3T2C4Vertex, r),
                               4,
                               COGL_VERTEX_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
  attributes[3] = NULL;
  prim = cogl_primitive_new_with_attributes_array (mode, n_vertices,
                                                   attributes);
  cogl_object_unref (prim);
  return prim;
}

static void
free_attributes_list (CoglPrimitive *primitive)
{
  int i;

  for (i = 0; i < primitive->attributes->len; i++)
    {
      CoglVertexAttribute *attribute =
        g_array_index (primitive->attributes, CoglVertexAttribute *, i);
      cogl_object_unref (attribute);
    }
  g_array_set_size (primitive->attributes, 0);
}

static void
_cogl_primitive_free (CoglPrimitive *primitive)
{
  free_attributes_list (primitive);

  g_array_free (primitive->attributes, TRUE);

  g_slice_free (CoglPrimitive, primitive);
}

void
cogl_primitive_set_attributes (CoglPrimitive *primitive,
                               CoglVertexAttribute **attributes)
{
  int i;

  free_attributes_list (primitive);

  g_array_set_size (primitive->attributes, 0);
  for (i = 0; attributes[i]; i++)
    {
      cogl_object_ref (attributes[i]);
      g_return_if_fail (cogl_is_vertex_attribute (attributes[i]));
      g_array_append_val (primitive->attributes, attributes[i]);
    }
}

int
cogl_primitive_get_first_vertex (CoglPrimitive *primitive)
{
  g_return_val_if_fail (cogl_is_primitive (primitive), 0);

  return primitive->first_vertex;
}

void
cogl_primitive_set_first_vertex (CoglPrimitive *primitive,
                                 int first_vertex)
{
  g_return_if_fail (cogl_is_primitive (primitive));

  primitive->first_vertex = first_vertex;
}

int
cogl_primitive_get_n_vertices (CoglPrimitive *primitive)
{
  g_return_val_if_fail (cogl_is_primitive (primitive), 0);

  return primitive->n_vertices;
}

void
cogl_primitive_set_n_vertices (CoglPrimitive *primitive,
                               int n_vertices)
{
  g_return_if_fail (cogl_is_primitive (primitive));

  primitive->n_vertices = n_vertices;
}

CoglVerticesMode
cogl_primitive_get_mode (CoglPrimitive *primitive)
{
  g_return_val_if_fail (cogl_is_primitive (primitive), 0);

  return primitive->mode;
}

void
cogl_primitive_set_mode (CoglPrimitive *primitive,
                         CoglVerticesMode mode)
{
  g_return_if_fail (cogl_is_primitive (primitive));

  primitive->mode = mode;
}

void
cogl_primitive_set_indices (CoglPrimitive *primitive,
                            CoglIndices *indices)
{
  g_return_if_fail (cogl_is_primitive (primitive));

  if (indices)
    cogl_object_ref (indices);
  if (primitive->indices)
    cogl_object_unref (primitive->indices);
  primitive->indices = indices;
}

/* XXX: cogl_draw_primitive() ? */
void
cogl_primitive_draw (CoglPrimitive *primitive)
{
  CoglVertexAttribute **attributes =
    (CoglVertexAttribute **)primitive->attributes->data;

  if (primitive->indices)
    cogl_draw_indexed_vertex_attributes_array (primitive->mode,
                                               primitive->first_vertex,
                                               primitive->n_vertices,
                                               primitive->indices,
                                               attributes);
  else
    cogl_draw_vertex_attributes_array (primitive->mode,
                                       primitive->first_vertex,
                                       primitive->n_vertices,
                                       attributes);
}

