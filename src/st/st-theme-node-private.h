/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-theme-node-private.h: private structures and functions for StThemeNode
 *
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2011 Quentin "Sardem FF7" Glidic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ST_THEME_NODE_PRIVATE_H__
#define __ST_THEME_NODE_PRIVATE_H__

#include <gdk/gdk.h>

#include "st-theme-node.h"
#include "croco/libcroco.h"
#include "st-types.h"

G_BEGIN_DECLS

/* Keep this in sync with stylish/src/outline.rs:StOutline */
typedef struct {
  ClutterColor color;
  int width;
} StOutline;

/* Keep this in sync with stylish/src/sides.rs:StSides */
/* Note that this matches the order in StSide */
typedef struct {
  double top;
  double right;
  double bottom;
  double left;
} StSides;

/* Note that this matches the order in StSide */
typedef struct {
  StOutline top;
  StOutline right;
  StOutline bottom;
  StOutline left;
} StBorder;

/* Keep this in sync with stylish/src/corners.rs:StCorners */
/* Note that this matches the order in StCorner */
typedef struct {
  int top_left;
  int top_right;
  int bottom_right;
  int bottom_left;
} StCorners;

typedef enum {
  ST_BACKGROUND_SIZE_AUTO,
  ST_BACKGROUND_SIZE_CONTAIN,
  ST_BACKGROUND_SIZE_COVER,
  ST_BACKGROUND_SIZE_FIXED
} StBackgroundSize;

struct _StThemeNode {
  GObject parent;

  StThemeContext *context;
  StThemeNode *parent_node;
  StTheme *theme;

  PangoFontDescription *font_desc;

  ClutterColor background_color;
  /* If gradient is set, then background_color is the gradient start */
  StGradientType background_gradient_type;
  ClutterColor background_gradient_end;

  int background_position_x;
  int background_position_y;

  StBackgroundSize background_size;
  gint background_size_w;
  gint background_size_h;

  ClutterColor foreground_color;

  StOutline outline;
  StBorder border;
  StCorners border_radius;
  StSides padding;
  StSides margin;

  int width;
  int height;
  int min_width;
  int min_height;
  int max_width;
  int max_height;

  int transition_duration;

  GFile *background_image;
  StBorderImage *border_image;
  StShadow *box_shadow;
  StShadow *background_image_shadow;
  StShadow *text_shadow;
  StIconColors *icon_colors;

  GType element_type;
  char *element_id;
  GStrv element_classes;
  GStrv pseudo_classes;
  char *inline_style;

  CRDeclaration **properties;
  int n_properties;

  /* We hold onto these separately so we can destroy them on finalize */
  CRDeclaration *inline_properties;

  guint background_position_set : 1;
  guint background_repeat : 1;

  guint properties_computed : 1;
  guint geometry_computed : 1;
  guint background_computed : 1;
  guint foreground_computed : 1;
  guint border_image_computed : 1;
  guint box_shadow_computed : 1;
  guint background_image_shadow_computed : 1;
  guint text_shadow_computed : 1;
  guint link_type : 2;
  guint rendered_once : 1;
  guint cached_textures : 1;

  int box_shadow_min_width;
  int box_shadow_min_height;

  guint stylesheets_changed_id;

  CoglPipeline *border_slices_texture;
  CoglPipeline *border_slices_pipeline;
  CoglPipeline *background_texture;
  CoglPipeline *background_pipeline;
  CoglPipeline *background_shadow_pipeline;
  CoglPipeline *color_pipeline;

  StThemeNodePaintState cached_state;

  int cached_scale_factor;
};

void _st_theme_node_ensure_background (StThemeNode *node);
void _st_theme_node_ensure_geometry (StThemeNode *node);
void _st_theme_node_apply_margins (StThemeNode *node,
                                   ClutterActor *actor);

G_END_DECLS

#endif /* __ST_THEME_NODE_PRIVATE_H__ */
