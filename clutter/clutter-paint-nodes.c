/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011  Intel Corporation.
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
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-paint-nodes
 * @Title: Paint Nodes
 * @Short_Description: ClutterPaintNode implementations
 *
 * Clutter provides a set of predefined #ClutterPaintNode implementations
 * that cover all the state changes available.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include "clutter-paint-node-private.h"

#include <pango/pango.h>
#include <cogl/cogl.h>

#include "clutter-actor-private.h"
#include "clutter-color.h"
#include "clutter-debug.h"
#include "clutter-private.h"

#include "clutter-paint-nodes.h"

static CoglPipeline *default_color_pipeline   = NULL;
static CoglPipeline *default_texture_pipeline = NULL;

/*< private >
 * _clutter_paint_node_init_types:
 *
 * Initializes the required types for ClutterPaintNode subclasses
 */
void
_clutter_paint_node_init_types (void)
{
  CoglContext *ctx;
  CoglColor cogl_color;
  GType node_type G_GNUC_UNUSED;

  if (G_LIKELY (default_color_pipeline != NULL))
    return;

  ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());

  node_type = clutter_paint_node_get_type ();

  cogl_color_init_from_4f (&cogl_color, 1.0, 1.0, 1.0, 1.0);

  default_color_pipeline = cogl_pipeline_new (ctx);
  cogl_pipeline_set_color (default_color_pipeline, &cogl_color);

  default_texture_pipeline = cogl_pipeline_new (ctx);
  cogl_pipeline_set_layer_null_texture (default_texture_pipeline, 0,
                                        COGL_TEXTURE_TYPE_2D);
  cogl_pipeline_set_color (default_texture_pipeline, &cogl_color);
  cogl_pipeline_set_layer_wrap_mode (default_texture_pipeline, 0,
                                     COGL_PIPELINE_WRAP_MODE_AUTOMATIC);
}

/*
 * Root node, private
 *
 * any frame can only have a since RootNode instance for each
 * top-level actor.
 */

#define clutter_root_node_get_type      _clutter_root_node_get_type

typedef struct _ClutterRootNode         ClutterRootNode;
typedef struct _ClutterPaintNodeClass   ClutterRootNodeClass;

struct _ClutterRootNode
{
  ClutterPaintNode parent_instance;

  CoglFramebuffer *framebuffer;

  CoglBufferBit clear_flags;
  CoglColor clear_color;
};

G_DEFINE_TYPE (ClutterRootNode, clutter_root_node, CLUTTER_TYPE_PAINT_NODE)

static gboolean
clutter_root_node_pre_draw (ClutterPaintNode *node)
{
  ClutterRootNode *rnode = (ClutterRootNode *) node;

  cogl_framebuffer_clear (rnode->framebuffer,
                          rnode->clear_flags,
                          &rnode->clear_color);

  return TRUE;
}

static void
clutter_root_node_post_draw (ClutterPaintNode *node)
{
}

static void
clutter_root_node_finalize (ClutterPaintNode *node)
{
  ClutterRootNode *rnode = (ClutterRootNode *) node;

  cogl_object_unref (rnode->framebuffer);

  CLUTTER_PAINT_NODE_CLASS (clutter_root_node_parent_class)->finalize (node);
}

static void
clutter_root_node_class_init (ClutterRootNodeClass *klass)
{
  ClutterPaintNodeClass *node_class = CLUTTER_PAINT_NODE_CLASS (klass);

  node_class->pre_draw = clutter_root_node_pre_draw;
  node_class->post_draw = clutter_root_node_post_draw;
  node_class->finalize = clutter_root_node_finalize;
}

static void
clutter_root_node_init (ClutterRootNode *self)
{
}

ClutterPaintNode *
_clutter_root_node_new (CoglFramebuffer    *framebuffer,
                        const ClutterColor *clear_color,
                        CoglBufferBit       clear_flags)
{
  ClutterRootNode *res;

  res = _clutter_paint_node_create (_clutter_root_node_get_type ());

  cogl_color_init_from_4ub (&res->clear_color,
                            clear_color->red,
                            clear_color->green,
                            clear_color->blue,
                            clear_color->alpha);
  cogl_color_premultiply (&res->clear_color);

  if (G_LIKELY (framebuffer != NULL))
    res->framebuffer = cogl_object_ref (framebuffer);
  else
    res->framebuffer = cogl_object_ref (cogl_get_draw_framebuffer ());

  res->clear_flags = clear_flags;

  return (ClutterPaintNode *) res;
}

/*
 * Transform node
 *
 * A private PaintNode, that changes the modelview of its child
 * nodes.
 */

#define clutter_transform_node_get_type _clutter_transform_node_get_type

typedef struct _ClutterTransformNode {
  ClutterPaintNode parent_instance;

  CoglMatrix modelview;
} ClutterTransformNode;

typedef struct _ClutterPaintNodeClass   ClutterTransformNodeClass;

G_DEFINE_TYPE (ClutterTransformNode, clutter_transform_node, CLUTTER_TYPE_PAINT_NODE)

static gboolean
clutter_transform_node_pre_draw (ClutterPaintNode *node)
{
  ClutterTransformNode *tnode = (ClutterTransformNode *) node;
  CoglMatrix matrix;

  cogl_push_matrix ();

  cogl_get_modelview_matrix (&matrix);
  cogl_matrix_multiply (&matrix, &matrix, &tnode->modelview);
  cogl_set_modelview_matrix (&matrix);

  return TRUE;
}

static void
clutter_transform_node_post_draw (ClutterPaintNode *node)
{
  cogl_pop_matrix ();
}

static void
clutter_transform_node_class_init (ClutterTransformNodeClass *klass)
{
  ClutterPaintNodeClass *node_class;

  node_class = CLUTTER_PAINT_NODE_CLASS (klass);
  node_class->pre_draw = clutter_transform_node_pre_draw;
  node_class->post_draw = clutter_transform_node_post_draw;
}

static void
clutter_transform_node_init (ClutterTransformNode *self)
{
  cogl_matrix_init_identity (&self->modelview);
}

ClutterPaintNode *
_clutter_transform_node_new (const CoglMatrix *modelview)
{
  ClutterTransformNode *res;

  res = _clutter_paint_node_create (_clutter_transform_node_get_type ());

  if (modelview != NULL)
    res->modelview = *modelview;

  return (ClutterPaintNode *) res;
}

/*
 * Dummy node, private
 *
 * an empty node, used temporarily until we can drop API compatibility,
 * and we'll be able to build a full render tree for each frame.
 */

#define clutter_dummy_node_get_type      _clutter_dummy_node_get_type

typedef struct _ClutterDummyNode        ClutterDummyNode;
typedef struct _ClutterPaintNodeClass   ClutterDummyNodeClass;

struct _ClutterDummyNode
{
  ClutterPaintNode parent_instance;

  ClutterActor *actor;
};

G_DEFINE_TYPE (ClutterDummyNode, clutter_dummy_node, CLUTTER_TYPE_PAINT_NODE)

static gboolean
clutter_dummy_node_pre_draw (ClutterPaintNode *node)
{
  return TRUE;
}

static JsonNode *
clutter_dummy_node_serialize (ClutterPaintNode *node)
{
  ClutterDummyNode *dnode = (ClutterDummyNode *) node;
  JsonBuilder *builder;
  JsonNode *res;

  if (dnode->actor == NULL)
    return json_node_new (JSON_NODE_NULL);

  builder = json_builder_new ();
  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "actor");
  json_builder_add_string_value (builder, _clutter_actor_get_debug_name (dnode->actor));

  json_builder_end_object (builder);

  res = json_builder_get_root (builder);
  g_object_unref (builder);

  return res;
}

static void
clutter_dummy_node_class_init (ClutterDummyNodeClass *klass)
{
  ClutterPaintNodeClass *node_class = CLUTTER_PAINT_NODE_CLASS (klass);

  node_class->pre_draw = clutter_dummy_node_pre_draw;
  node_class->serialize = clutter_dummy_node_serialize;
}

static void
clutter_dummy_node_init (ClutterDummyNode *self)
{
}

ClutterPaintNode *
_clutter_dummy_node_new (ClutterActor *actor)
{
  ClutterPaintNode *res;

  res = _clutter_paint_node_create (_clutter_dummy_node_get_type ());

  ((ClutterDummyNode *) res)->actor = actor;

  return res;
}

/*
 * Pipeline node
 */

struct _ClutterPipelineNode
{
  ClutterPaintNode parent_instance;

  CoglPipeline *pipeline;
};

/**
 * ClutterPipelineNodeClass:
 *
 * The <structname>ClutterPipelineNodeClass</structname> structure is an opaque
 * type whose members cannot be directly accessed.
 *
 * Since: 1.10
 */
struct _ClutterPipelineNodeClass
{
  ClutterPaintNodeClass parent_class;
};

G_DEFINE_TYPE (ClutterPipelineNode, clutter_pipeline_node, CLUTTER_TYPE_PAINT_NODE)

static void
clutter_pipeline_node_finalize (ClutterPaintNode *node)
{
  ClutterPipelineNode *pnode = CLUTTER_PIPELINE_NODE (node);

  if (pnode->pipeline != NULL)
    cogl_object_unref (pnode->pipeline);

  CLUTTER_PAINT_NODE_CLASS (clutter_pipeline_node_parent_class)->finalize (node);
}

static gboolean
clutter_pipeline_node_pre_draw (ClutterPaintNode *node)
{
  ClutterPipelineNode *pnode = CLUTTER_PIPELINE_NODE (node);

  if (node->operations != NULL &&
      pnode->pipeline != NULL)
    {
      cogl_push_source (pnode->pipeline);
      return TRUE;
    }

  return FALSE;
}

static void
clutter_pipeline_node_draw (ClutterPaintNode *node)
{
  ClutterPipelineNode *pnode = CLUTTER_PIPELINE_NODE (node);
  guint i;

  if (pnode->pipeline == NULL)
    return;

  if (node->operations == NULL)
    return;

  for (i = 0; i < node->operations->len; i++)
    {
      const ClutterPaintOperation *op;

      op = &g_array_index (node->operations, ClutterPaintOperation, i);

      switch (op->opcode)
        {
        case PAINT_OP_INVALID:
          break;

        case PAINT_OP_TEX_RECT:
          cogl_rectangle_with_texture_coords (op->op.texrect[0],
                                              op->op.texrect[1],
                                              op->op.texrect[2],
                                              op->op.texrect[3],
                                              op->op.texrect[4],
                                              op->op.texrect[5],
                                              op->op.texrect[6],
                                              op->op.texrect[7]);
          break;

        case PAINT_OP_PATH:
          cogl_path_fill (op->op.path);
          break;

        case PAINT_OP_PRIMITIVE:
          {
            CoglFramebuffer *fb = cogl_get_draw_framebuffer ();

            cogl_framebuffer_draw_primitive (fb, pnode->pipeline,
                                             op->op.primitive);
          }
          break;
        }
    }
}

static void
clutter_pipeline_node_post_draw (ClutterPaintNode *node)
{
  cogl_pop_source ();
}

static JsonNode *
clutter_pipeline_node_serialize (ClutterPaintNode *node)
{
  ClutterPipelineNode *pnode = CLUTTER_PIPELINE_NODE (node);
  JsonBuilder *builder;
  CoglColor color;
  JsonNode *res;

  if (pnode->pipeline == NULL)
    return json_node_new (JSON_NODE_NULL);

  builder = json_builder_new ();
  json_builder_begin_object (builder);

  cogl_pipeline_get_color (pnode->pipeline, &color);
  json_builder_set_member_name (builder, "color");
  json_builder_begin_array (builder);
  json_builder_add_double_value (builder, cogl_color_get_red (&color));
  json_builder_add_double_value (builder, cogl_color_get_green (&color));
  json_builder_add_double_value (builder, cogl_color_get_blue (&color));
  json_builder_add_double_value (builder, cogl_color_get_alpha (&color));
  json_builder_end_array (builder);

#if 0
  json_builder_set_member_name (builder, "layers");
  json_builder_begin_array (builder);
  cogl_pipeline_foreach_layer (pnode->pipeline,
                               clutter_pipeline_node_serialize_layer,
                               builder);
  json_builder_end_array (builder);
#endif

  json_builder_end_object (builder);

  res = json_builder_get_root (builder);
  g_object_unref (builder);

  return res;
}

static void
clutter_pipeline_node_class_init (ClutterPipelineNodeClass *klass)
{
  ClutterPaintNodeClass *node_class;

  node_class = CLUTTER_PAINT_NODE_CLASS (klass);
  node_class->pre_draw = clutter_pipeline_node_pre_draw;
  node_class->draw = clutter_pipeline_node_draw;
  node_class->post_draw = clutter_pipeline_node_post_draw;
  node_class->finalize = clutter_pipeline_node_finalize;
  node_class->serialize = clutter_pipeline_node_serialize;
}

static void
clutter_pipeline_node_init (ClutterPipelineNode *self)
{
}

/**
 * clutter_pipeline_node_new:
 * @pipeline: (allow-none): a Cogl pipeline state object, or %NULL
 *
 * Creates a new #ClutterPaintNode that will use the @pipeline to
 * paint its contents.
 *
 * This function will acquire a reference on the passed @pipeline,
 * so it is safe to call cogl_object_unref() when it returns.
 *
 * Return value: (transfer full): the newly created #ClutterPaintNode.
 *   Use clutter_paint_node_unref() when done.
 *
 * Since: 1.10
 */
ClutterPaintNode *
clutter_pipeline_node_new (CoglPipeline *pipeline)
{
  ClutterPipelineNode *res;

  g_return_val_if_fail (pipeline == NULL || cogl_is_pipeline (pipeline), NULL);

  res = _clutter_paint_node_create (CLUTTER_TYPE_PIPELINE_NODE);

  if (pipeline != NULL)
    res->pipeline = cogl_object_ref (pipeline);

  return (ClutterPaintNode *) res;
}

/*
 * Color node
 */

struct _ClutterColorNode
{
  ClutterPipelineNode parent_instance;
};

/**
 * ClutterColorNodeClass:
 *
 * The <structname>ClutterColorNodeClass</structname> structure is an
 * opaque type whose members cannot be directly accessed.
 *
 * Since: 1.10
 */
struct _ClutterColorNodeClass
{
  ClutterPipelineNodeClass parent_class;
};

G_DEFINE_TYPE (ClutterColorNode, clutter_color_node, CLUTTER_TYPE_PIPELINE_NODE)

static void
clutter_color_node_class_init (ClutterColorNodeClass *klass)
{

}

static void
clutter_color_node_init (ClutterColorNode *cnode)
{
  ClutterPipelineNode *pnode = CLUTTER_PIPELINE_NODE (cnode);

  g_assert (default_color_pipeline != NULL);
  pnode->pipeline = cogl_pipeline_copy (default_color_pipeline);
}

/**
 * clutter_color_node_new:
 * @color: (allow-none): the color to paint, or %NULL
 *
 * Creates a new #ClutterPaintNode that will paint a solid color
 * fill using @color.
 *
 * Return value: (transfer full): the newly created #ClutterPaintNode. Use
 *   clutter_paint_node_unref() when done
 *
 * Since: 1.10
 */
ClutterPaintNode *
clutter_color_node_new (const ClutterColor *color)
{
  ClutterPipelineNode *cnode;

  cnode = _clutter_paint_node_create (CLUTTER_TYPE_COLOR_NODE);

  if (color != NULL)
    {
      CoglColor cogl_color;

      cogl_color_init_from_4ub (&cogl_color,
                                color->red,
                                color->green,
                                color->blue,
                                color->alpha);
      cogl_color_premultiply (&cogl_color);

      cogl_pipeline_set_color (cnode->pipeline, &cogl_color);
    }

  return (ClutterPaintNode *) cnode;
}

/*
 * Texture node
 */

struct _ClutterTextureNode
{
  ClutterPipelineNode parent_instance;
};

/**
 * ClutterTextureNodeClass:
 *
 * The <structname>ClutterTextureNodeClass</structname> structure is an
 * opaque type whose members cannot be directly accessed.
 *
 * Since: 1.10
 */
struct _ClutterTextureNodeClass
{
  ClutterPipelineNodeClass parent_class;
};

G_DEFINE_TYPE (ClutterTextureNode, clutter_texture_node, CLUTTER_TYPE_PIPELINE_NODE)

static void
clutter_texture_node_class_init (ClutterTextureNodeClass *klass)
{
}

static void
clutter_texture_node_init (ClutterTextureNode *self)
{
  ClutterPipelineNode *pnode = CLUTTER_PIPELINE_NODE (self);

  g_assert (default_texture_pipeline != NULL);
  pnode->pipeline = cogl_pipeline_copy (default_texture_pipeline);
}

static CoglPipelineFilter
clutter_scaling_filter_to_cogl_pipeline_filter (ClutterScalingFilter filter)
{
  switch (filter)
    {
    case CLUTTER_SCALING_FILTER_NEAREST:
      return COGL_PIPELINE_FILTER_NEAREST;

    case CLUTTER_SCALING_FILTER_LINEAR:
      return COGL_PIPELINE_FILTER_LINEAR;

    case CLUTTER_SCALING_FILTER_TRILINEAR:
      return COGL_PIPELINE_FILTER_LINEAR_MIPMAP_LINEAR;
    }

  return COGL_PIPELINE_FILTER_LINEAR;
}

/**
 * clutter_texture_node_new:
 * @texture: a #CoglTexture
 * @color: a #ClutterColor
 * @min_filter: the minification filter for the texture
 * @mag_filter: the magnification filter for the texture
 *
 * Creates a new #ClutterPaintNode that will paint the passed @texture.
 *
 * This function will take a reference on @texture, so it is safe to
 * call cogl_object_unref() on @texture when it returns.
 *
 * Return value: (transfer full): the newly created #ClutterPaintNode.
 *   Use clutter_paint_node_unref() when done
 *
 * Since: 1.10
 */
ClutterPaintNode *
clutter_texture_node_new (CoglTexture          *texture,
                          const ClutterColor   *color,
                          ClutterScalingFilter  min_filter,
                          ClutterScalingFilter  mag_filter)
{
  ClutterPipelineNode *tnode;
  CoglColor cogl_color;
  CoglPipelineFilter min_f, mag_f;

  g_return_val_if_fail (cogl_is_texture (texture), NULL);

  tnode = _clutter_paint_node_create (CLUTTER_TYPE_TEXTURE_NODE);

  cogl_pipeline_set_layer_texture (tnode->pipeline, 0, texture);

  min_f = clutter_scaling_filter_to_cogl_pipeline_filter (min_filter);
  mag_f = clutter_scaling_filter_to_cogl_pipeline_filter (mag_filter);
  cogl_pipeline_set_layer_filters (tnode->pipeline, 0, min_f, mag_f);

  cogl_color_init_from_4ub (&cogl_color,
                            color->red,
                            color->green,
                            color->blue,
                            color->alpha);
  cogl_color_premultiply (&cogl_color);
  cogl_pipeline_set_color (tnode->pipeline, &cogl_color);

  return (ClutterPaintNode *) tnode;
}

/*
 * Text node
 */

struct _ClutterTextNode
{
  ClutterPaintNode parent_instance;

  PangoLayout *layout;
  CoglColor color;
};

/**
 * ClutterTextNodeClass:
 *
 * The <structname>ClutterTextNodeClass</structname> structure is an opaque
 * type whose contents cannot be directly accessed.
 *
 * Since: 1.10
 */
struct _ClutterTextNodeClass
{
  ClutterPaintNodeClass parent_class;
};

G_DEFINE_TYPE (ClutterTextNode, clutter_text_node, CLUTTER_TYPE_PAINT_NODE)

static void
clutter_text_node_finalize (ClutterPaintNode *node)
{
  ClutterTextNode *tnode = CLUTTER_TEXT_NODE (node);

  if (tnode->layout != NULL)
    g_object_unref (tnode->layout);

  CLUTTER_PAINT_NODE_CLASS (clutter_text_node_parent_class)->finalize (node);
}

static gboolean
clutter_text_node_pre_draw (ClutterPaintNode *node)
{
  ClutterTextNode *tnode = CLUTTER_TEXT_NODE (node);

  return tnode->layout != NULL;
}

static void
clutter_text_node_draw (ClutterPaintNode *node)
{
  ClutterTextNode *tnode = CLUTTER_TEXT_NODE (node);
  PangoRectangle extents;
  guint i;

  if (node->operations == NULL)
    return;

  pango_layout_get_pixel_extents (tnode->layout, NULL, &extents);

  for (i = 0; i < node->operations->len; i++)
    {
      const ClutterPaintOperation *op;
      float op_width, op_height;
      gboolean clipped = FALSE;

      op = &g_array_index (node->operations, ClutterPaintOperation, i);

      switch (op->opcode)
        {
        case PAINT_OP_TEX_RECT:
          op_width = op->op.texrect[2] - op->op.texrect[0];
          op_height = op->op.texrect[3] - op->op.texrect[1];

          /* if the primitive size was smaller than the layout,
           * we clip the layout when drawin, to avoid spilling
           * it out
           */
          if (extents.width > op_width ||
              extents.height > op_height)
            {
              cogl_clip_push_rectangle (op->op.texrect[0],
                                        op->op.texrect[1],
                                        op->op.texrect[2],
                                        op->op.texrect[3]);
              clipped = TRUE;
            }

          cogl_pango_render_layout (tnode->layout,
                                    op->op.texrect[0],
                                    op->op.texrect[1],
                                    &tnode->color,
                                    0);

          if (clipped)
            cogl_clip_pop ();
          break;

        case PAINT_OP_PATH:
        case PAINT_OP_PRIMITIVE:
        case PAINT_OP_INVALID:
          break;
        }
    }
}

static JsonNode *
clutter_text_node_serialize (ClutterPaintNode *node)
{
  ClutterTextNode *tnode = CLUTTER_TEXT_NODE (node);
  JsonBuilder *builder;
  JsonNode *res;

  builder = json_builder_new ();

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "layout");

  if (pango_layout_get_character_count (tnode->layout) > 12)
    {
      const char *text = pango_layout_get_text (tnode->layout);
      char *str;

      str = g_strndup (text, 12);
      json_builder_add_string_value (builder, str);
      g_free (str);
    }
  else
    json_builder_add_string_value (builder, pango_layout_get_text (tnode->layout));

  json_builder_set_member_name (builder, "color");
  json_builder_begin_array (builder);
  json_builder_add_double_value (builder, cogl_color_get_red (&tnode->color));
  json_builder_add_double_value (builder, cogl_color_get_green (&tnode->color));
  json_builder_add_double_value (builder, cogl_color_get_blue (&tnode->color));
  json_builder_add_double_value (builder, cogl_color_get_alpha (&tnode->color));
  json_builder_end_array (builder);

  json_builder_end_object (builder);

  res = json_builder_get_root (builder);
  g_object_unref (builder);

  return res;
}

static void
clutter_text_node_class_init (ClutterTextNodeClass *klass)
{
  ClutterPaintNodeClass *node_class = CLUTTER_PAINT_NODE_CLASS (klass);

  node_class->pre_draw = clutter_text_node_pre_draw;
  node_class->draw = clutter_text_node_draw;
  node_class->finalize = clutter_text_node_finalize;
  node_class->serialize = clutter_text_node_serialize;
}

static void
clutter_text_node_init (ClutterTextNode *self)
{
  cogl_color_init_from_4f (&self->color, 0.0, 0.0, 0.0, 1.0);
}

/**
 * clutter_text_node_new:
 * @layout: (allow-none): a #PangoLayout, or %NULL
 * @color: (allow-none): the color used to paint the layout,
 *   or %NULL
 *
 * Creates a new #ClutterPaintNode that will paint a #PangoLayout
 * with the given color.
 *
 * This function takes a reference on the passed @layout, so it
 * is safe to call g_object_unref() after it returns.
 *
 * Return value: (transfer full): the newly created #ClutterPaintNode.
 *   Use clutter_paint_node_unref() when done
 *
 * Since: 1.10
 */
ClutterPaintNode *
clutter_text_node_new (PangoLayout        *layout,
                       const ClutterColor *color)
{
  ClutterTextNode *res;

  g_return_val_if_fail (layout == NULL || PANGO_IS_LAYOUT (layout), NULL);

  res = _clutter_paint_node_create (CLUTTER_TYPE_TEXT_NODE);

  if (layout != NULL)
    res->layout = g_object_ref (layout);

  if (color != NULL)
    {
      cogl_color_init_from_4ub (&res->color,
                                color->red,
                                color->green,
                                color->blue,
                                color->alpha);
    }

  return (ClutterPaintNode *) res;
}

/*
 * Clip node
 */
struct _ClutterClipNode
{
  ClutterPaintNode parent_instance;
};

/**
 * ClutterClipNodeClass:
 *
 * The <structname>ClutterClipNodeClass</structname> structure is an opaque
 * type whose members cannot be directly accessed.
 *
 * Since: 1.10
 */
struct _ClutterClipNodeClass
{
  ClutterPaintNodeClass parent_class;
};

G_DEFINE_TYPE (ClutterClipNode, clutter_clip_node, CLUTTER_TYPE_PAINT_NODE)

static gboolean
clutter_clip_node_pre_draw (ClutterPaintNode *node)
{
  gboolean retval = FALSE;
  CoglFramebuffer *fb;
  guint i;

  if (node->operations == NULL)
    return FALSE;

  fb = cogl_get_draw_framebuffer ();

  for (i = 0; i < node->operations->len; i++)
    {
      const ClutterPaintOperation *op;

      op = &g_array_index (node->operations, ClutterPaintOperation, i);

      switch (op->opcode)
        {
        case PAINT_OP_TEX_RECT:
          cogl_framebuffer_push_rectangle_clip (fb,
                                                op->op.texrect[0],
                                                op->op.texrect[1],
                                                op->op.texrect[2],
                                                op->op.texrect[3]);
          retval = TRUE;
          break;

        case PAINT_OP_PATH:
          cogl_framebuffer_push_path_clip (fb, op->op.path);
          retval = TRUE;
          break;

        case PAINT_OP_PRIMITIVE:
        case PAINT_OP_INVALID:
          break;
        }
    }

  return retval;
}

static void
clutter_clip_node_post_draw (ClutterPaintNode *node)
{
  CoglFramebuffer *fb;
  guint i;

  if (node->operations == NULL)
    return;

  fb = cogl_get_draw_framebuffer ();

  for (i = 0; i < node->operations->len; i++)
    {
      const ClutterPaintOperation *op;

      op = &g_array_index (node->operations, ClutterPaintOperation, i);

      switch (op->opcode)
        {
        case PAINT_OP_PATH:
        case PAINT_OP_TEX_RECT:
          cogl_framebuffer_pop_clip (fb);
          break;

        case PAINT_OP_PRIMITIVE:
        case PAINT_OP_INVALID:
          break;
        }
    }
}

static void
clutter_clip_node_class_init (ClutterClipNodeClass *klass)
{
  ClutterPaintNodeClass *node_class;

  node_class = CLUTTER_PAINT_NODE_CLASS (klass);
  node_class->pre_draw = clutter_clip_node_pre_draw;
  node_class->post_draw = clutter_clip_node_post_draw;
}

static void
clutter_clip_node_init (ClutterClipNode *self)
{
}

/**
 * clutter_clip_node_new:
 *
 * Creates a new #ClutterPaintNode that will clip its child
 * nodes to the 2D regions added to it.
 *
 * Return value: (transfer full): the newly created #ClutterPaintNode.
 *   Use clutter_paint_node_unref() when done.
 *
 * Since: 1.10
 */
ClutterPaintNode *
clutter_clip_node_new (void)
{
  return _clutter_paint_node_create (CLUTTER_TYPE_CLIP_NODE);
}

/*
 * ClutterLayerNode (private)
 */

#define clutter_layer_node_get_type     _clutter_layer_node_get_type

struct _ClutterLayerNode
{
  ClutterPaintNode parent_instance;

  cairo_rectangle_t viewport;

  CoglMatrix projection;

  float fbo_width;
  float fbo_height;

  CoglPipeline *state;
  CoglFramebuffer *offscreen;
  CoglTexture *texture;

  guint8 opacity;
};

struct _ClutterLayerNodeClass
{
  ClutterPaintNodeClass parent_class;
};

G_DEFINE_TYPE (ClutterLayerNode, clutter_layer_node, CLUTTER_TYPE_PAINT_NODE)

static gboolean
clutter_layer_node_pre_draw (ClutterPaintNode *node)
{
  ClutterLayerNode *lnode = (ClutterLayerNode *) node;
  CoglMatrix matrix;

  /* if we were unable to create an offscreen buffer for this node, then
   * we simply ignore it
   */
  if (lnode->offscreen == NULL)
    return FALSE;

  /* if no geometry was submitted for this node then we simply ignore it */
  if (node->operations == NULL)
    return FALSE;

  /* copy the same modelview from the current framebuffer to the one we
   * are going to use
   */
  cogl_get_modelview_matrix (&matrix);

  cogl_push_framebuffer (lnode->offscreen);

  cogl_framebuffer_set_modelview_matrix (lnode->offscreen, &matrix);

  cogl_framebuffer_set_viewport (lnode->offscreen,
                                 lnode->viewport.x,
                                 lnode->viewport.y,
                                 lnode->viewport.width,
                                 lnode->viewport.height);

  cogl_framebuffer_set_projection_matrix (lnode->offscreen,
                                          &lnode->projection);

  /* clear out the target framebuffer */
  cogl_framebuffer_clear4f (lnode->offscreen,
                            COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH,
                            0.f, 0.f, 0.f, 0.f);

  cogl_push_matrix ();

  /* every draw operation after this point will happen an offscreen
   * framebuffer
   */

  return TRUE;
}

static void
clutter_layer_node_post_draw (ClutterPaintNode *node)
{
  ClutterLayerNode *lnode = CLUTTER_LAYER_NODE (node);
  CoglFramebuffer *fb;
  guint i;

  /* switch to the previous framebuffer */
  cogl_pop_matrix ();
  cogl_pop_framebuffer ();

  fb = cogl_get_draw_framebuffer ();

  for (i = 0; i < node->operations->len; i++)
    {
      const ClutterPaintOperation *op;

      op = &g_array_index (node->operations, ClutterPaintOperation, i);
      switch (op->opcode)
        {
        case PAINT_OP_INVALID:
          break;

        case PAINT_OP_TEX_RECT:
          /* now we need to paint the texture */
          cogl_push_source (lnode->state);
          cogl_rectangle_with_texture_coords (op->op.texrect[0],
                                              op->op.texrect[1],
                                              op->op.texrect[2],
                                              op->op.texrect[3],
                                              op->op.texrect[4],
                                              op->op.texrect[5],
                                              op->op.texrect[6],
                                              op->op.texrect[7]);
          cogl_pop_source ();
          break;

        case PAINT_OP_PATH:
          cogl_push_source (lnode->state);
          cogl_path_fill (op->op.path);
          cogl_pop_source ();
          break;

        case PAINT_OP_PRIMITIVE:
          cogl_framebuffer_draw_primitive (fb, lnode->state, op->op.primitive);
          break;
        }
    }
}

static void
clutter_layer_node_finalize (ClutterPaintNode *node)
{
  ClutterLayerNode *lnode = CLUTTER_LAYER_NODE (node);

  if (lnode->state != NULL)
    cogl_object_unref (lnode->state);

  if (lnode->offscreen != NULL)
    cogl_object_unref (lnode->offscreen);

  CLUTTER_PAINT_NODE_CLASS (clutter_layer_node_parent_class)->finalize (node);
}

static void
clutter_layer_node_class_init (ClutterLayerNodeClass *klass)
{
  ClutterPaintNodeClass *node_class;

  node_class = CLUTTER_PAINT_NODE_CLASS (klass);
  node_class->pre_draw = clutter_layer_node_pre_draw;
  node_class->post_draw = clutter_layer_node_post_draw;
  node_class->finalize = clutter_layer_node_finalize;
}

static void
clutter_layer_node_init (ClutterLayerNode *self)
{
  cogl_matrix_init_identity (&self->projection);
}

/*
 * clutter_layer_node_new:
 * @projection: the projection matrix to use to set up the layer
 * @viewport: (type cairo.Rectangle): the viewport to use to set up the layer
 * @width: the width of the layer
 * @height: the height of the layer
 * @opacity: the opacity to be used when drawing the layer
 *
 * Creates a new #ClutterLayerNode.
 *
 * All children of this node will be painted inside a separate
 * framebuffer; the framebuffer will then be painted using the
 * given @opacity.
 *
 * Return value: (transfer full): the newly created #ClutterLayerNode.
 *   Use clutter_paint_node_unref() when done.
 *
 * Since: 1.10
 */
ClutterPaintNode *
_clutter_layer_node_new (const CoglMatrix        *projection,
                         const cairo_rectangle_t *viewport,
                         float                    width,
                         float                    height,
                         guint8                   opacity)
{
  ClutterLayerNode *res;
  CoglColor color;

  res = _clutter_paint_node_create (CLUTTER_TYPE_LAYER_NODE);

  res->projection = *projection;
  res->viewport = *viewport;
  res->fbo_width = width;
  res->fbo_height = height;
  res->opacity = opacity;

  /* the texture backing the FBO */
  res->texture = cogl_texture_new_with_size (MAX (res->fbo_width, 1),
                                             MAX (res->fbo_height, 1),
                                             COGL_TEXTURE_NO_SLICING,
                                             COGL_PIXEL_FORMAT_RGBA_8888_PRE);

  res->offscreen = COGL_FRAMEBUFFER (cogl_offscreen_new_to_texture (res->texture));
  if (res->offscreen == NULL)
    {
      g_critical ("%s: Unable to create an offscreen buffer", G_STRLOC);

      cogl_object_unref (res->texture);
      res->texture = NULL;

      goto out;
    }

  cogl_color_init_from_4ub (&color, opacity, opacity, opacity, opacity);

  /* the pipeline used to paint the texture; we use nearest
   * interpolation filters because the texture is always
   * going to be painted at a 1:1 texel:pixel ratio
   */
  res->state = cogl_pipeline_copy (default_texture_pipeline);
  cogl_pipeline_set_layer_filters (res->state, 0,
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);
  cogl_pipeline_set_layer_texture (res->state, 0, res->texture);
  cogl_pipeline_set_color (res->state, &color);
  cogl_object_unref (res->texture);

out:
  return (ClutterPaintNode *) res;
}
