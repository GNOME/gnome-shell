/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010,2011 Intel Corporation.
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

#include "cogl-context-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-layer-private.h"
#include "cogl-node-private.h"

#include <glib.h>

typedef struct
{
  int parent_id;
  int *node_id_ptr;
  GString *graph;
  int indent;
} PrintDebugState;

static CoglBool
dump_layer_cb (CoglNode *node, void *user_data)
{
  CoglPipelineLayer *layer = COGL_PIPELINE_LAYER (node);
  PrintDebugState *state = user_data;
  int layer_id = *state->node_id_ptr;
  PrintDebugState state_out;
  GString *changes_label;
  CoglBool changes = FALSE;

  if (state->parent_id >= 0)
    g_string_append_printf (state->graph, "%*slayer%p -> layer%p;\n",
                            state->indent, "",
                            layer->_parent.parent,
                            layer);

  g_string_append_printf (state->graph,
                          "%*slayer%p [label=\"layer=0x%p\\n"
                          "ref count=%d\" "
                          "color=\"blue\"];\n",
                          state->indent, "",
                          layer,
                          layer,
                          COGL_OBJECT (layer)->ref_count);

  changes_label = g_string_new ("");
  g_string_append_printf (changes_label,
                          "%*slayer%p -> layer_state%d [weight=100];\n"
                          "%*slayer_state%d [shape=box label=\"",
                          state->indent, "",
                          layer,
                          layer_id,
                          state->indent, "",
                          layer_id);

  if (layer->differences & COGL_PIPELINE_LAYER_STATE_UNIT)
    {
      changes = TRUE;
      g_string_append_printf (changes_label,
                              "\\lunit=%u\\n",
                              layer->unit_index);
    }

  if (layer->differences & COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA)
    {
      changes = TRUE;
      g_string_append_printf (changes_label,
                              "\\ltexture=%p\\n",
                              layer->texture);
    }

  if (changes)
    {
      g_string_append_printf (changes_label, "\"];\n");
      g_string_append (state->graph, changes_label->str);
      g_string_free (changes_label, TRUE);
    }

  state_out.parent_id = layer_id;

  state_out.node_id_ptr = state->node_id_ptr;
  (*state_out.node_id_ptr)++;

  state_out.graph = state->graph;
  state_out.indent = state->indent + 2;

  _cogl_pipeline_node_foreach_child (COGL_NODE (layer),
                                     dump_layer_cb,
                                     &state_out);

  return TRUE;
}

static CoglBool
dump_layer_ref_cb (CoglPipelineLayer *layer, void *data)
{
  PrintDebugState *state = data;
  int pipeline_id = *state->node_id_ptr;

  g_string_append_printf (state->graph,
                          "%*spipeline_state%d -> layer%p;\n",
                          state->indent, "",
                          pipeline_id,
                          layer);

  return TRUE;
}

static CoglBool
dump_pipeline_cb (CoglNode *node, void *user_data)
{
  CoglPipeline *pipeline = COGL_PIPELINE (node);
  PrintDebugState *state = user_data;
  int pipeline_id = *state->node_id_ptr;
  PrintDebugState state_out;
  GString *changes_label;
  CoglBool changes = FALSE;
  CoglBool layers = FALSE;

  if (state->parent_id >= 0)
    g_string_append_printf (state->graph, "%*spipeline%d -> pipeline%d;\n",
                            state->indent, "",
                            state->parent_id,
                            pipeline_id);

  g_string_append_printf (state->graph,
                          "%*spipeline%d [label=\"pipeline=0x%p\\n"
                          "ref count=%d\\n"
                          "breadcrumb=\\\"%s\\\"\" color=\"red\"];\n",
                          state->indent, "",
                          pipeline_id,
                          pipeline,
                          COGL_OBJECT (pipeline)->ref_count,
                          pipeline->has_static_breadcrumb ?
#ifdef COGL_DEBUG_ENABLED
                          pipeline->static_breadcrumb : "NULL"
#else
                          "NULL"
#endif
                          );

  changes_label = g_string_new ("");
  g_string_append_printf (changes_label,
                          "%*spipeline%d -> pipeline_state%d [weight=100];\n"
                          "%*spipeline_state%d [shape=box label=\"",
                          state->indent, "",
                          pipeline_id,
                          pipeline_id,
                          state->indent, "",
                          pipeline_id);


  if (pipeline->differences & COGL_PIPELINE_STATE_COLOR)
    {
      changes = TRUE;
      g_string_append_printf (changes_label,
                              "\\lcolor=0x%02X%02X%02X%02X\\n",
                              cogl_color_get_red_byte (&pipeline->color),
                              cogl_color_get_green_byte (&pipeline->color),
                              cogl_color_get_blue_byte (&pipeline->color),
                              cogl_color_get_alpha_byte (&pipeline->color));
    }

  if (pipeline->differences & COGL_PIPELINE_STATE_BLEND)
    {
      const char *blend_enable_name;

      changes = TRUE;

      switch (pipeline->blend_enable)
        {
        case COGL_PIPELINE_BLEND_ENABLE_AUTOMATIC:
          blend_enable_name = "AUTO";
          break;
        case COGL_PIPELINE_BLEND_ENABLE_ENABLED:
          blend_enable_name = "ENABLED";
          break;
        case COGL_PIPELINE_BLEND_ENABLE_DISABLED:
          blend_enable_name = "DISABLED";
          break;
        default:
          blend_enable_name = "UNKNOWN";
        }
      g_string_append_printf (changes_label,
                              "\\lblend=%s\\n",
                              blend_enable_name);
    }

  if (pipeline->differences & COGL_PIPELINE_STATE_LAYERS)
    {
      changes = TRUE;
      layers = TRUE;
      g_string_append_printf (changes_label, "\\ln_layers=%d\\n",
                              pipeline->n_layers);
    }

  if (changes)
    {
      g_string_append_printf (changes_label, "\"];\n");
      g_string_append (state->graph, changes_label->str);
      g_string_free (changes_label, TRUE);
    }

  if (layers)
    {
      g_list_foreach (pipeline->layer_differences,
                      (GFunc)dump_layer_ref_cb,
                      state);
    }

  state_out.parent_id = pipeline_id;

  state_out.node_id_ptr = state->node_id_ptr;
  (*state_out.node_id_ptr)++;

  state_out.graph = state->graph;
  state_out.indent = state->indent + 2;

  _cogl_pipeline_node_foreach_child (COGL_NODE (pipeline),
                                     dump_pipeline_cb,
                                     &state_out);

  return TRUE;
}

/* This function is just here to be called from GDB so we don't really
   want to put a declaration in a header and we just add it here to
   avoid a warning */
void
_cogl_debug_dump_pipelines_dot_file (const char *filename);

void
_cogl_debug_dump_pipelines_dot_file (const char *filename)
{
  GString *graph;
  PrintDebugState layer_state;
  PrintDebugState pipeline_state;
  int layer_id = 0;
  int pipeline_id = 0;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!ctx->default_pipeline)
    return;

  graph = g_string_new ("");
  g_string_append_printf (graph, "digraph {\n");

  layer_state.graph = graph;
  layer_state.parent_id = -1;
  layer_state.node_id_ptr = &layer_id;
  layer_state.indent = 0;
  dump_layer_cb ((CoglNode *)ctx->default_layer_0, &layer_state);

  pipeline_state.graph = graph;
  pipeline_state.parent_id = -1;
  pipeline_state.node_id_ptr = &pipeline_id;
  pipeline_state.indent = 0;
  dump_pipeline_cb ((CoglNode *)ctx->default_pipeline, &pipeline_state);

  g_string_append_printf (graph, "}\n");

  if (filename)
    g_file_set_contents (filename, graph->str, -1, NULL);
  else
    g_print ("%s", graph->str);

  g_string_free (graph, TRUE);
}
