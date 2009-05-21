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

typedef enum
{
  COGL_PANGO_DISPLAY_LIST_TEXTURE,
  COGL_PANGO_DISPLAY_LIST_RECTANGLE,
  COGL_PANGO_DISPLAY_LIST_TRAPEZOID
} CoglPangoDisplayListNodeType;

typedef struct _CoglPangoDisplayListNode CoglPangoDisplayListNode;
typedef struct _CoglPangoDisplayListVertex CoglPangoDisplayListVertex;

#ifdef HAVE_CLUTTER_GLX
#define COGL_PANGO_DISPLAY_LIST_DRAW_MODE GL_QUADS
#else
/* GLES doesn't support GL_QUADS so we use GL_TRIANGLES instead */
#define COGL_PANGO_DISPLAY_LIST_DRAW_MODE GL_TRIANGLES
#endif

struct _CoglPangoDisplayList
{
  CoglColor  color;
  GSList    *nodes;
  GSList    *last_node;
};

struct _CoglPangoDisplayListNode
{
  CoglPangoDisplayListNodeType type;

  CoglColor color;

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
_cogl_pango_display_list_new (void)
{
  return g_slice_new0 (CoglPangoDisplayList);
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
_cogl_pango_display_list_set_color (CoglPangoDisplayList *dl,
                                    const CoglColor *color)
{
  dl->color = *color;
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
      && !memcmp (&dl->color, &node->color, sizeof (CoglColor)))
    {
      /* Get rid of the vertex buffer so that it will be recreated */
      if (node->d.texture.vertex_buffer != COGL_INVALID_HANDLE)
        {
          cogl_vertex_buffer_unref (node->d.texture.vertex_buffer);
          node->d.texture.vertex_buffer = COGL_INVALID_HANDLE;
        }
    }
  else
    {
      /* Otherwise create a new node */
      node = g_slice_new (CoglPangoDisplayListNode);

      node->type = COGL_PANGO_DISPLAY_LIST_TEXTURE;
      node->color = dl->color;
      node->d.texture.texture = cogl_texture_ref (texture);
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

#ifndef HAVE_CLUTTER_GLX

  /* GLES doesn't support GL_QUADS so we use GL_TRIANGLES instead with
     two extra vertices per quad. FIXME: It might be better to use
     indexed elements here but cogl vertex buffers don't currently
     support storing the indices */

  g_array_set_size (node->d.texture.verts,
                    node->d.texture.verts->len + 2);
  verts = &g_array_index (node->d.texture.verts,
                          CoglPangoDisplayListVertex,
                          node->d.texture.verts->len - 6);

  verts[4] = verts[0];
  verts[5] = verts[2];

#endif /* HAVE_CLUTTER_GLX */
}

void
_cogl_pango_display_list_add_rectangle (CoglPangoDisplayList *dl,
                                        float x_1, float y_1,
                                        float x_2, float y_2)
{
  CoglPangoDisplayListNode *node = g_slice_new (CoglPangoDisplayListNode);

  node->type = COGL_PANGO_DISPLAY_LIST_RECTANGLE;
  node->color = dl->color;
  node->d.rectangle.x_1 = x_1;
  node->d.rectangle.y_1 = y_1;
  node->d.rectangle.x_2 = x_2;
  node->d.rectangle.y_2 = y_2;

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
  node->color = dl->color;
  node->d.trapezoid.y_1 = y_1;
  node->d.trapezoid.x_11 = x_11;
  node->d.trapezoid.x_21 = x_21;
  node->d.trapezoid.y_2 = y_2;
  node->d.trapezoid.x_12 = x_12;
  node->d.trapezoid.x_22 = x_22;

  _cogl_pango_display_list_append_node (dl, node);
}

static void
_cogl_pango_display_list_render_texture (CoglHandle material,
                                         CoglPangoDisplayListNode *node)
{
  cogl_material_set_layer (material, 0, node->d.texture.texture);
  cogl_material_set_color (material, &node->color);
  cogl_set_source (material);

  if (node->d.texture.vertex_buffer == COGL_INVALID_HANDLE)
    {
      CoglHandle vb = cogl_vertex_buffer_new (node->d.texture.verts->len);

      cogl_vertex_buffer_add (vb, "gl_Vertex", 2, GL_FLOAT, FALSE,
                              sizeof (CoglPangoDisplayListVertex),
                              &g_array_index (node->d.texture.verts,
                                              CoglPangoDisplayListVertex, 0).x);
      cogl_vertex_buffer_add (vb, "gl_MultiTexCoord0", 2, GL_FLOAT, FALSE,
                              sizeof (CoglPangoDisplayListVertex),
                              &g_array_index (node->d.texture.verts,
                                              CoglPangoDisplayListVertex,
                                              0).t_x);
      cogl_vertex_buffer_submit (vb);

      node->d.texture.vertex_buffer = vb;
    }

  cogl_vertex_buffer_draw (node->d.texture.vertex_buffer,
                           COGL_PANGO_DISPLAY_LIST_DRAW_MODE,
                           0, node->d.texture.verts->len);
}

void
_cogl_pango_display_list_render (CoglPangoDisplayList *dl,
                                 CoglHandle glyph_material,
                                 CoglHandle solid_material)
{
  GSList *l;

  for (l = dl->nodes; l; l = l->next)
    {
      CoglPangoDisplayListNode *node = l->data;

      switch (node->type)
        {
        case COGL_PANGO_DISPLAY_LIST_TEXTURE:
          _cogl_pango_display_list_render_texture (glyph_material, node);
          break;

        case COGL_PANGO_DISPLAY_LIST_RECTANGLE:
          cogl_material_set_color (solid_material, &node->color);
          cogl_set_source (solid_material);
          cogl_rectangle (node->d.rectangle.x_1,
                          node->d.rectangle.y_1,
                          node->d.rectangle.x_2,
                          node->d.rectangle.y_2);
          break;

        case COGL_PANGO_DISPLAY_LIST_TRAPEZOID:
          {
            float points[8];

            points[0] =  node->d.trapezoid.x_11;
            points[1] =  node->d.trapezoid.y_1;
            points[2] =  node->d.trapezoid.x_12;
            points[3] =  node->d.trapezoid.y_2;
            points[4] =  node->d.trapezoid.x_22;
            points[5] =  node->d.trapezoid.y_2;
            points[6] =  node->d.trapezoid.x_21;
            points[7] =  node->d.trapezoid.y_1;

            cogl_material_set_color (solid_material, &node->color);
            cogl_set_source (solid_material);
            cogl_path_polygon (points, 4);
            cogl_path_fill ();
          }
          break;
        }
    }
}

static void
_cogl_pango_display_list_node_free (CoglPangoDisplayListNode *node)
{
  if (node->type == COGL_PANGO_DISPLAY_LIST_TEXTURE)
    {
      g_array_free (node->d.texture.verts, TRUE);
      if (node->d.texture.texture != COGL_INVALID_HANDLE)
        cogl_texture_unref (node->d.texture.texture);
      if (node->d.texture.vertex_buffer != COGL_INVALID_HANDLE)
        cogl_vertex_buffer_unref (node->d.texture.vertex_buffer);
    }

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
