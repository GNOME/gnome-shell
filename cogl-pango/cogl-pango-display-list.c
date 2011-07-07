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
#include <cogl/cogl.h>
#include <string.h>

#include "cogl-pango-display-list.h"
#include "cogl/cogl-context-private.h"

typedef enum
{
  COGL_PANGO_DISPLAY_LIST_TEXTURE,
  COGL_PANGO_DISPLAY_LIST_RECTANGLE,
  COGL_PANGO_DISPLAY_LIST_TRAPEZOID
} CoglPangoDisplayListNodeType;

typedef struct _CoglPangoDisplayListNode CoglPangoDisplayListNode;
typedef struct _CoglPangoDisplayListVertex CoglPangoDisplayListVertex;

struct _CoglPangoDisplayList
{
  gboolean                color_override;
  CoglColor               color;
  GSList                 *nodes;
  GSList                 *last_node;
  CoglPangoPipelineCache *pipeline_cache;
};

struct _CoglPangoDisplayListNode
{
  CoglPangoDisplayListNodeType type;

  gboolean color_override;
  CoglColor color;

  CoglPipeline *pipeline;

  union
  {
    struct
    {
      /* The texture to render these coords from */
      CoglHandle  texture;
      /* Array of vertex data to render out of this texture */
      GArray     *verts;
      /* A VBO representing those vertices */
      CoglHandle  vertex_buffer;
    } texture;

    struct
    {
      float x_1, y_1;
      float x_2, y_2;
    } rectangle;

    struct
    {
      float y_1;
      float x_11;
      float x_21;
      float y_2;
      float x_12;
      float x_22;
    } trapezoid;
  } d;
};

struct _CoglPangoDisplayListVertex
{
  float x, y, t_x, t_y;
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
                                      CoglHandle texture,
                                      float x_1, float y_1,
                                      float x_2, float y_2,
                                      float tx_1, float ty_1,
                                      float tx_2, float ty_2)
{
  CoglPangoDisplayListNode *node;
  CoglPangoDisplayListVertex *verts;

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
      if (node->d.texture.vertex_buffer != COGL_INVALID_HANDLE)
        {
          cogl_handle_unref (node->d.texture.vertex_buffer);
          node->d.texture.vertex_buffer = COGL_INVALID_HANDLE;
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
      node->d.texture.texture = cogl_handle_ref (texture);
      node->d.texture.verts
        = g_array_new (FALSE, FALSE, sizeof (CoglPangoDisplayListVertex));
      node->d.texture.vertex_buffer = COGL_INVALID_HANDLE;

      _cogl_pango_display_list_append_node (dl, node);
    }

  g_array_set_size (node->d.texture.verts,
                    node->d.texture.verts->len + 4);
  verts = &g_array_index (node->d.texture.verts,
                          CoglPangoDisplayListVertex,
                          node->d.texture.verts->len - 4);

  verts->x = x_1;
  verts->y = y_1;
  verts->t_x = tx_1;
  verts->t_y = ty_1;
  verts++;
  verts->x = x_1;
  verts->y = y_2;
  verts->t_x = tx_1;
  verts->t_y = ty_2;
  verts++;
  verts->x = x_2;
  verts->y = y_2;
  verts->t_x = tx_2;
  verts->t_y = ty_2;
  verts++;
  verts->x = x_2;
  verts->y = y_1;
  verts->t_x = tx_2;
  verts->t_y = ty_1;
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
  CoglPangoDisplayListNode *node = g_slice_new (CoglPangoDisplayListNode);

  node->type = COGL_PANGO_DISPLAY_LIST_TRAPEZOID;
  node->color_override = dl->color_override;
  node->color = dl->color;
  node->d.trapezoid.y_1 = y_1;
  node->d.trapezoid.x_11 = x_11;
  node->d.trapezoid.x_21 = x_21;
  node->d.trapezoid.y_2 = y_2;
  node->d.trapezoid.x_12 = x_12;
  node->d.trapezoid.x_22 = x_22;
  node->pipeline = NULL;

  _cogl_pango_display_list_append_node (dl, node);
}

static void
emit_rectangles_through_journal (CoglPangoDisplayListNode *node)
{
  int i;

  for (i = 0; i < node->d.texture.verts->len; i += 4)
    {
      CoglPangoDisplayListVertex *v0 =
        &g_array_index (node->d.texture.verts,
                        CoglPangoDisplayListVertex, i);
      CoglPangoDisplayListVertex *v1 =
        &g_array_index (node->d.texture.verts,
                        CoglPangoDisplayListVertex, i + 2);
      cogl_rectangle_with_texture_coords (v0->x, v0->y, v1->x, v1->y,
                                          v0->t_x, v0->t_y,
                                          v1->t_x, v1->t_y);
    }
}

static void
emit_vertex_buffer_geometry (CoglPangoDisplayListNode *node)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* It's expensive to go through the Cogl journal for large runs
   * of text in part because the journal transforms the quads in software
   * to avoid changing the modelview matrix. So for larger runs of text
   * we load the vertices into a VBO, and this has the added advantage
   * that if the text doesn't change from frame to frame the VBO can
   * be re-used avoiding the repeated cost of validating the data and
   * mapping it into the GPU... */

  if (node->d.texture.vertex_buffer == COGL_INVALID_HANDLE)
    {
      CoglHandle vb = cogl_vertex_buffer_new (node->d.texture.verts->len);

      cogl_vertex_buffer_add (vb, "gl_Vertex", 2,
                              COGL_ATTRIBUTE_TYPE_FLOAT, FALSE,
                              sizeof (CoglPangoDisplayListVertex),
                              &g_array_index (node->d.texture.verts,
                                              CoglPangoDisplayListVertex, 0).x);
      cogl_vertex_buffer_add (vb, "gl_MultiTexCoord0", 2,
                              COGL_ATTRIBUTE_TYPE_FLOAT, FALSE,
                              sizeof (CoglPangoDisplayListVertex),
                              &g_array_index (node->d.texture.verts,
                                              CoglPangoDisplayListVertex,
                                              0).t_x);
      cogl_vertex_buffer_submit (vb);

      node->d.texture.vertex_buffer = vb;
    }

#ifdef CLUTTER_COGL_HAS_GL
  if (ctx->driver == COGL_DRIVER_GL)
    cogl_vertex_buffer_draw (node->d.texture.vertex_buffer,
                             GL_QUADS,
                             0, node->d.texture.verts->len);
  else
#endif
    {
      /* GLES doesn't support GL_QUADS so instead we use a VBO with
         indexed vertices to generate GL_TRIANGLES from the quads */

      int n_indices = node->d.texture.verts->len / 4 * 6;
      CoglHandle indices_vbo
        = cogl_vertex_buffer_indices_get_for_quads (n_indices);

      cogl_vertex_buffer_draw_elements (node->d.texture.vertex_buffer,
                                        COGL_VERTICES_MODE_TRIANGLES,
                                        indices_vbo,
                                        0, node->d.texture.verts->len - 1,
                                        0, n_indices);
    }
}

static void
_cogl_pango_display_list_render_texture (CoglPangoDisplayListNode *node)
{
  /* For small runs of text like icon labels, we can get better performance
   * going through the Cogl journal since text may then be batched together
   * with other geometry. */
  /* FIXME: 100 is a number I plucked out of thin air; it would be good
   * to determine this empirically! */
  if (node->d.texture.verts->len < 100)
    emit_rectangles_through_journal (node);
  else
    emit_vertex_buffer_geometry (node);
}

void
_cogl_pango_display_list_render (CoglPangoDisplayList *dl,
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
      cogl_push_source (node->pipeline);

      switch (node->type)
        {
        case COGL_PANGO_DISPLAY_LIST_TEXTURE:
          _cogl_pango_display_list_render_texture (node);
          break;

        case COGL_PANGO_DISPLAY_LIST_RECTANGLE:
          cogl_rectangle (node->d.rectangle.x_1,
                          node->d.rectangle.y_1,
                          node->d.rectangle.x_2,
                          node->d.rectangle.y_2);
          break;

        case COGL_PANGO_DISPLAY_LIST_TRAPEZOID:
          {
            float points[8];
            CoglPath *path;

            points[0] =  node->d.trapezoid.x_11;
            points[1] =  node->d.trapezoid.y_1;
            points[2] =  node->d.trapezoid.x_12;
            points[3] =  node->d.trapezoid.y_2;
            points[4] =  node->d.trapezoid.x_22;
            points[5] =  node->d.trapezoid.y_2;
            points[6] =  node->d.trapezoid.x_21;
            points[7] =  node->d.trapezoid.y_1;

            path = cogl_path_new ();
            cogl_path_polygon (path, points, 4);
            cogl_path_fill (path);
            cogl_object_unref (path);
          }
          break;
        }

      cogl_pop_source ();
    }
}

static void
_cogl_pango_display_list_node_free (CoglPangoDisplayListNode *node)
{
  if (node->type == COGL_PANGO_DISPLAY_LIST_TEXTURE)
    {
      g_array_free (node->d.texture.verts, TRUE);
      if (node->d.texture.texture != COGL_INVALID_HANDLE)
        cogl_handle_unref (node->d.texture.texture);
      if (node->d.texture.vertex_buffer != COGL_INVALID_HANDLE)
        cogl_handle_unref (node->d.texture.vertex_buffer);
    }

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
