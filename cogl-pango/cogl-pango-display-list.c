/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Neil Roberts  <neil@linux.intel.com>
 *
 * Copyright (C) 2009 Intel Corporation.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <string.h>

#include "cogl-pango-display-list.h"
#include "cogl-pango-pipeline-cache.h"
#include "cogl/cogl-context-private.h"

typedef enum
{
  COGL_PANGO_DISPLAY_LIST_TEXTURE,
  COGL_PANGO_DISPLAY_LIST_RECTANGLE,
  COGL_PANGO_DISPLAY_LIST_TRAPEZOID
} CoglPangoDisplayListNodeType;

typedef struct _CoglPangoDisplayListNode CoglPangoDisplayListNode;
typedef struct _CoglPangoDisplayListRectangle CoglPangoDisplayListRectangle;

struct _CoglPangoDisplayList
{
  CoglBool                color_override;
  CoglColor               color;
  GSList                 *nodes;
  GSList                 *last_node;
  CoglPangoPipelineCache *pipeline_cache;
};

/* This matches the format expected by cogl_rectangles_with_texture_coords */
struct _CoglPangoDisplayListRectangle
{
  float x_1, y_1, x_2, y_2;
  float s_1, t_1, s_2, t_2;
};

struct _CoglPangoDisplayListNode
{
  CoglPangoDisplayListNodeType type;

  CoglBool color_override;
  CoglColor color;

  CoglPipeline *pipeline;

  union
  {
    struct
    {
      /* The texture to render these coords from */
      CoglTexture *texture;
      /* Array of rectangles in the format expected by
         cogl_rectangles_with_texture_coords */
      GArray *rectangles;
      /* A primitive representing those vertices */
      CoglPrimitive *primitive;
    } texture;

    struct
    {
      float x_1, y_1;
      float x_2, y_2;
    } rectangle;

    struct
    {
      CoglPrimitive *primitive;
    } trapezoid;
  } d;
};

CoglPangoDisplayList *
_cogl_pango_display_list_new (CoglPangoPipelineCache *pipeline_cache)
{
  CoglPangoDisplayList *dl = g_slice_new0 (CoglPangoDisplayList);

  dl->pipeline_cache = pipeline_cache;

  return dl;
}

static void
_cogl_pango_display_list_append_node (CoglPangoDisplayList *dl,
                                      CoglPangoDisplayListNode *node)
{
  if (dl->last_node)
    dl->last_node = dl->last_node->next = g_slist_prepend (NULL, node);
  else
    dl->last_node = dl->nodes = g_slist_prepend (NULL, node);
}

void
_cogl_pango_display_list_set_color_override (CoglPangoDisplayList *dl,
                                             const CoglColor *color)
{
  dl->color_override = TRUE;
  dl->color = *color;
}

void
_cogl_pango_display_list_remove_color_override (CoglPangoDisplayList *dl)
{
  dl->color_override = FALSE;
}

void
_cogl_pango_display_list_add_texture (CoglPangoDisplayList *dl,
                                      CoglTexture *texture,
                                      float x_1, float y_1,
                                      float x_2, float y_2,
                                      float tx_1, float ty_1,
                                      float tx_2, float ty_2)
{
  CoglPangoDisplayListNode *node;
  CoglPangoDisplayListRectangle *rectangle;

  /* Add to the last node if it is a texture node with the same
     target texture */
  if (dl->last_node
      && (node = dl->last_node->data)->type == COGL_PANGO_DISPLAY_LIST_TEXTURE
      && node->d.texture.texture == texture
      && (dl->color_override
          ? (node->color_override && cogl_color_equal (&dl->color, &node->color))
          : !node->color_override))
    {
      /* Get rid of the vertex buffer so that it will be recreated */
      if (node->d.texture.primitive != NULL)
        {
          cogl_object_unref (node->d.texture.primitive);
          node->d.texture.primitive = NULL;
        }
    }
  else
    {
      /* Otherwise create a new node */
      node = g_slice_new (CoglPangoDisplayListNode);

      node->type = COGL_PANGO_DISPLAY_LIST_TEXTURE;
      node->color_override = dl->color_override;
      node->color = dl->color;
      node->pipeline = NULL;
      node->d.texture.texture = cogl_object_ref (texture);
      node->d.texture.rectangles
        = g_array_new (FALSE, FALSE, sizeof (CoglPangoDisplayListRectangle));
      node->d.texture.primitive = NULL;

      _cogl_pango_display_list_append_node (dl, node);
    }

  g_array_set_size (node->d.texture.rectangles,
                    node->d.texture.rectangles->len + 1);
  rectangle = &g_array_index (node->d.texture.rectangles,
                              CoglPangoDisplayListRectangle,
                              node->d.texture.rectangles->len - 1);
  rectangle->x_1 = x_1;
  rectangle->y_1 = y_1;
  rectangle->x_2 = x_2;
  rectangle->y_2 = y_2;
  rectangle->s_1 = tx_1;
  rectangle->t_1 = ty_1;
  rectangle->s_2 = tx_2;
  rectangle->t_2 = ty_2;
}

void
_cogl_pango_display_list_add_rectangle (CoglPangoDisplayList *dl,
                                        float x_1, float y_1,
                                        float x_2, float y_2)
{
  CoglPangoDisplayListNode *node = g_slice_new (CoglPangoDisplayListNode);

  node->type = COGL_PANGO_DISPLAY_LIST_RECTANGLE;
  node->color_override = dl->color_override;
  node->color = dl->color;
  node->d.rectangle.x_1 = x_1;
  node->d.rectangle.y_1 = y_1;
  node->d.rectangle.x_2 = x_2;
  node->d.rectangle.y_2 = y_2;
  node->pipeline = NULL;

  _cogl_pango_display_list_append_node (dl, node);
}

void
_cogl_pango_display_list_add_trapezoid (CoglPangoDisplayList *dl,
                                        float y_1,
                                        float x_11,
                                        float x_21,
                                        float y_2,
                                        float x_12,
                                        float x_22)
{
  CoglContext *ctx = dl->pipeline_cache->ctx;
  CoglPangoDisplayListNode *node = g_slice_new (CoglPangoDisplayListNode);
  CoglVertexP2 vertices[4] = {
        { x_11, y_1 },
        { x_12, y_2 },
        { x_22, y_2 },
        { x_21, y_1 }
  };

  node->type = COGL_PANGO_DISPLAY_LIST_TRAPEZOID;
  node->color_override = dl->color_override;
  node->color = dl->color;
  node->pipeline = NULL;

  node->d.trapezoid.primitive =
    cogl_primitive_new_p2 (ctx,
                           COGL_VERTICES_MODE_TRIANGLE_FAN,
                           4,
                           vertices);

  _cogl_pango_display_list_append_node (dl, node);
}

static void
emit_rectangles_through_journal (CoglFramebuffer *fb,
                                 CoglPipeline *pipeline,
                                 CoglPangoDisplayListNode *node)
{
  const float *rectangles = (const float *)node->d.texture.rectangles->data;

  cogl_framebuffer_draw_textured_rectangles (fb,
                                             pipeline,
                                             rectangles,
                                             node->d.texture.rectangles->len);
}

static void
emit_vertex_buffer_geometry (CoglFramebuffer *fb,
                             CoglPipeline *pipeline,
                             CoglPangoDisplayListNode *node)
{
  CoglContext *ctx = fb->context;

  /* It's expensive to go through the Cogl journal for large runs
   * of text in part because the journal transforms the quads in software
   * to avoid changing the modelview matrix. So for larger runs of text
   * we load the vertices into a VBO, and this has the added advantage
   * that if the text doesn't change from frame to frame the VBO can
   * be re-used avoiding the repeated cost of validating the data and
   * mapping it into the GPU... */

  if (node->d.texture.primitive == NULL)
    {
      CoglAttributeBuffer *buffer;
      CoglVertexP2T2 *verts, *v;
      int n_verts;
      CoglBool allocated = FALSE;
      CoglAttribute *attributes[2];
      CoglPrimitive *prim;
      int i;

      n_verts = node->d.texture.rectangles->len * 4;

      buffer
        = cogl_attribute_buffer_new_with_size (ctx,
                                               n_verts *
                                               sizeof (CoglVertexP2T2));

      if ((verts = cogl_buffer_map (COGL_BUFFER (buffer),
                                    COGL_BUFFER_ACCESS_WRITE,
                                    COGL_BUFFER_MAP_HINT_DISCARD)) == NULL)
        {
          verts = g_new (CoglVertexP2T2, n_verts);
          allocated = TRUE;
        }

      v = verts;

      /* Copy the rectangles into the buffer and expand into four
         vertices instead of just two */
      for (i = 0; i < node->d.texture.rectangles->len; i++)
        {
          const CoglPangoDisplayListRectangle *rectangle
            = &g_array_index (node->d.texture.rectangles,
                              CoglPangoDisplayListRectangle, i);

          v->x = rectangle->x_1;
          v->y = rectangle->y_1;
          v->s = rectangle->s_1;
          v->t = rectangle->t_1;
          v++;
          v->x = rectangle->x_1;
          v->y = rectangle->y_2;
          v->s = rectangle->s_1;
          v->t = rectangle->t_2;
          v++;
          v->x = rectangle->x_2;
          v->y = rectangle->y_2;
          v->s = rectangle->s_2;
          v->t = rectangle->t_2;
          v++;
          v->x = rectangle->x_2;
          v->y = rectangle->y_1;
          v->s = rectangle->s_2;
          v->t = rectangle->t_1;
          v++;
        }

      if (allocated)
        {
          cogl_buffer_set_data (COGL_BUFFER (buffer),
                                0, /* offset */
                                verts,
                                sizeof (CoglVertexP2T2) * n_verts);
          g_free (verts);
        }
      else
        cogl_buffer_unmap (COGL_BUFFER (buffer));

      attributes[0] = cogl_attribute_new (buffer,
                                          "cogl_position_in",
                                          sizeof (CoglVertexP2T2),
                                          G_STRUCT_OFFSET (CoglVertexP2T2, x),
                                          2, /* n_components */
                                          COGL_ATTRIBUTE_TYPE_FLOAT);
      attributes[1] = cogl_attribute_new (buffer,
                                          "cogl_tex_coord0_in",
                                          sizeof (CoglVertexP2T2),
                                          G_STRUCT_OFFSET (CoglVertexP2T2, s),
                                          2, /* n_components */
                                          COGL_ATTRIBUTE_TYPE_FLOAT);

      prim = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLES,
                                                 n_verts,
                                                 attributes,
                                                 2 /* n_attributes */);

#ifdef CLUTTER_COGL_HAS_GL
      if ((ctx->private_feature_flags & COGL_PRIVATE_FEATURE_QUADS))
        cogl_primitive_set_mode (prim, GL_QUADS);
      else
#endif
        {
          /* GLES doesn't support GL_QUADS so instead we use a VBO
             with indexed vertices to generate GL_TRIANGLES from the
             quads */

          CoglIndices *indices =
            cogl_get_rectangle_indices (ctx, node->d.texture.rectangles->len);

          cogl_primitive_set_indices (prim, indices,
                                      node->d.texture.rectangles->len * 6);
        }

      node->d.texture.primitive = prim;

      cogl_object_unref (buffer);
      cogl_object_unref (attributes[0]);
      cogl_object_unref (attributes[1]);
    }

  cogl_primitive_draw (node->d.texture.primitive,
                       fb,
                       pipeline);
}

static void
_cogl_framebuffer_draw_display_list_texture (CoglFramebuffer *fb,
                                             CoglPipeline *pipeline,
                                             CoglPangoDisplayListNode *node)
{
  /* For small runs of text like icon labels, we can get better performance
   * going through the Cogl journal since text may then be batched together
   * with other geometry. */
  /* FIXME: 25 is a number I plucked out of thin air; it would be good
   * to determine this empirically! */
  if (node->d.texture.rectangles->len < 25)
    emit_rectangles_through_journal (fb, pipeline, node);
  else
    emit_vertex_buffer_geometry (fb, pipeline, node);
}

void
_cogl_pango_display_list_render (CoglFramebuffer *fb,
                                 CoglPangoDisplayList *dl,
                                 const CoglColor *color)
{
  GSList *l;

  for (l = dl->nodes; l; l = l->next)
    {
      CoglPangoDisplayListNode *node = l->data;
      CoglColor draw_color;

      if (node->pipeline == NULL)
        {
          if (node->type == COGL_PANGO_DISPLAY_LIST_TEXTURE)
            node->pipeline =
              _cogl_pango_pipeline_cache_get (dl->pipeline_cache,
                                              node->d.texture.texture);
          else
            node->pipeline =
              _cogl_pango_pipeline_cache_get (dl->pipeline_cache,
                                              NULL);
        }

      if (node->color_override)
        /* Use the override color but preserve the alpha from the
           draw color */
        cogl_color_init_from_4ub (&draw_color,
                                  cogl_color_get_red_byte (&node->color),
                                  cogl_color_get_green_byte (&node->color),
                                  cogl_color_get_blue_byte (&node->color),
                                  cogl_color_get_alpha_byte (color));
      else
        draw_color = *color;
      cogl_color_premultiply (&draw_color);

      cogl_pipeline_set_color (node->pipeline, &draw_color);

      switch (node->type)
        {
        case COGL_PANGO_DISPLAY_LIST_TEXTURE:
          _cogl_framebuffer_draw_display_list_texture (fb, node->pipeline, node);
          break;

        case COGL_PANGO_DISPLAY_LIST_RECTANGLE:
          cogl_framebuffer_draw_rectangle (fb,
                                           node->pipeline,
                                           node->d.rectangle.x_1,
                                           node->d.rectangle.y_1,
                                           node->d.rectangle.x_2,
                                           node->d.rectangle.y_2);
          break;

        case COGL_PANGO_DISPLAY_LIST_TRAPEZOID:
          cogl_framebuffer_draw_primitive (fb, node->pipeline,
                                           node->d.trapezoid.primitive);
          break;
        }
    }
}

static void
_cogl_pango_display_list_node_free (CoglPangoDisplayListNode *node)
{
  if (node->type == COGL_PANGO_DISPLAY_LIST_TEXTURE)
    {
      g_array_free (node->d.texture.rectangles, TRUE);
      if (node->d.texture.texture != NULL)
        cogl_object_unref (node->d.texture.texture);
      if (node->d.texture.primitive != NULL)
        cogl_object_unref (node->d.texture.primitive);
    }
  else if (node->type == COGL_PANGO_DISPLAY_LIST_TRAPEZOID)
    cogl_object_unref (node->d.trapezoid.primitive);

  if (node->pipeline)
    cogl_object_unref (node->pipeline);

  g_slice_free (CoglPangoDisplayListNode, node);
}

void
_cogl_pango_display_list_clear (CoglPangoDisplayList *dl)
{
  g_slist_foreach (dl->nodes, (GFunc) _cogl_pango_display_list_node_free, NULL);
  g_slist_free (dl->nodes);
  dl->nodes = NULL;
  dl->last_node = NULL;
}

void
_cogl_pango_display_list_free (CoglPangoDisplayList *dl)
{
  _cogl_pango_display_list_clear (dl);
  g_slice_free (CoglPangoDisplayList, dl);
}
