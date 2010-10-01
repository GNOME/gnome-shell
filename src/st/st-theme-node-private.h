/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __ST_THEME_NODE_PRIVATE_H__
#define __ST_THEME_NODE_PRIVATE_H__

#include <gdk/gdk.h>

#include "st-theme-node.h"

G_BEGIN_DECLS

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
  gboolean background_position_set : 1;

  ClutterColor foreground_color;
  ClutterColor border_color[4];
  ClutterColor outline_color;

  int border_width[4];
  int border_radius[4];
  int outline_width;
  guint padding[4];

  int width;
  int height;
  int min_width;
  int min_height;
  int max_width;
  int max_height;

  int transition_duration;

  char *background_image;
  StBorderImage *border_image;
  StShadow *shadow;
  StShadow *text_shadow;

  GType element_type;
  char *element_id;
  char *element_class;
  char *pseudo_class;
  char *inline_style;

  CRDeclaration **properties;
  int n_properties;

  /* We hold onto these separately so we can destroy them on finalize */
  CRDeclaration *inline_properties;

  guint properties_computed : 1;
  guint geometry_computed : 1;
  guint background_computed : 1;
  guint foreground_computed : 1;
  guint border_image_computed : 1;
  guint shadow_computed : 1;
  guint text_shadow_computed : 1;
  guint link_type : 2;

  /* Graphics state */
  float alloc_width;
  float alloc_height;

  CoglHandle background_shadow_material;
  CoglHandle border_shadow_material;
  CoglHandle background_texture;
  CoglHandle border_texture;
  CoglHandle corner_texture[4];
};

struct _StThemeNodeClass {
  GObjectClass parent_class;

};

void _st_theme_node_ensure_background (StThemeNode *node);
void _st_theme_node_ensure_geometry (StThemeNode *node);

void _st_theme_node_init_drawing_state (StThemeNode *node);
void _st_theme_node_free_drawing_state (StThemeNode *node);

G_END_DECLS

#endif /* __ST_THEME_NODE_PRIVATE_H__ */
