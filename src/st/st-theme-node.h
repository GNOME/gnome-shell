/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-theme-node.h: style information for one node in a tree of themed objects
 *
 * Copyright 2008-2010 Red Hat, Inc.
 * Copyright 2009, 2010 Florian Müllner
 * Copyright 2010 Giovanni Campagna
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

#ifndef __ST_THEME_NODE_H__
#define __ST_THEME_NODE_H__

#include <clutter/clutter.h>
#include "st-border-image.h"
#include "st-icon-colors.h"
#include "st-shadow.h"

G_BEGIN_DECLS

/**
 * SECTION:st-theme-node
 * @short_description: style information for one node in a tree of themed objects
 *
 * A #StThemeNode represents the CSS style information (the set of CSS properties) for one
 * node in a tree of themed objects. In typical usage, it represents the style information
 * for a single #ClutterActor. A #StThemeNode is immutable: attributes such as the
 * CSS classes for the node are passed in at construction. If the attributes of the node
 * or any parent node change, the node should be discarded and a new node created.
 * #StThemeNode has generic accessors to look up properties by name and specific
 * accessors for standard CSS properties that add caching and handling of various
 * details of the CSS specification. #StThemeNode also has convenience functions to help
 * in implementing a #ClutterActor with borders and padding.
 *
 * Note that pixel measurements take the #StThemeContext:scale-factor into
 * account so all values are in physical pixels, as opposed to logical pixels.
 * Physical pixels correspond to actor sizes, not necessarily to pixels on
 * display devices (eg. when `scale-monitor-framebuffer` is enabled).
 */

typedef struct _StTheme          StTheme;
typedef struct _StThemeContext   StThemeContext;

#define ST_TYPE_THEME_NODE              (st_theme_node_get_type ())
G_DECLARE_FINAL_TYPE (StThemeNode, st_theme_node, ST, THEME_NODE, GObject)

/**
 * StSide:
 * @ST_SIDE_TOP: The top side.
 * @ST_SIDE_RIGHT: The right side.
 * @ST_SIDE_BOTTOM: The bottom side.
 * @ST_SIDE_LEFT: The left side.
 *
 * Used to target a particular side of a #StThemeNode element.
 */
typedef enum {
    ST_SIDE_TOP,
    ST_SIDE_RIGHT,
    ST_SIDE_BOTTOM,
    ST_SIDE_LEFT
} StSide;

/**
 * StCorner:
 * @ST_CORNER_TOPLEFT: The top-right corner.
 * @ST_CORNER_TOPRIGHT: The top-right corner.
 * @ST_CORNER_BOTTOMRIGHT: The bottom-right corner.
 * @ST_CORNER_BOTTOMLEFT: The bottom-left corner.
 *
 * Used to target a particular corner of a #StThemeNode element.
 */
typedef enum {
    ST_CORNER_TOPLEFT,
    ST_CORNER_TOPRIGHT,
    ST_CORNER_BOTTOMRIGHT,
    ST_CORNER_BOTTOMLEFT
} StCorner;

/* These are the CSS values; that doesn't mean we have to implement blink... */
/**
 * StTextDecoration:
 * @ST_TEXT_DECORATION_: Text is underlined
 * @ST_TEXT_DECORATION_OVERLINE: Text is overlined
 * @ST_TEXT_DECORATION_LINE_THROUGH: Text is striked out
 * @ST_TEXT_DECORATION_BLINK: Text blinks
 *
 * Flags used to determine the decoration of text.
 *
 * Not that neither %ST_TEXT_DECORATION_OVERLINE or %ST_TEXT_DECORATION_BLINK
 * are implemented, currently.
 */
typedef enum {
    ST_TEXT_DECORATION_UNDERLINE    = 1 << 0,
    ST_TEXT_DECORATION_OVERLINE     = 1 << 1,
    ST_TEXT_DECORATION_LINE_THROUGH = 1 << 2,
    ST_TEXT_DECORATION_BLINK        = 1 << 3
} StTextDecoration;

/**
 * StTextAlign:
 * @ST_TEXT_ALIGN_LEFT: Text is aligned at the beginning of the label.
 * @ST_TEXT_ALIGN_CENTER: Text is aligned in the middle of the label.
 * @ST_TEXT_ALIGN_RIGHT: Text is aligned at the end of the label.
 * @ST_GRADIENT_JUSTIFY: Text is justified in the label.
 *
 * Used to align text in a label.
 */
typedef enum {
    ST_TEXT_ALIGN_LEFT = PANGO_ALIGN_LEFT,
    ST_TEXT_ALIGN_CENTER = PANGO_ALIGN_CENTER,
    ST_TEXT_ALIGN_RIGHT = PANGO_ALIGN_RIGHT,
    ST_TEXT_ALIGN_JUSTIFY
} StTextAlign;

/**
 * StGradientType:
 * @ST_GRADIENT_NONE: No gradient.
 * @ST_GRADIENT_VERTICAL: A vertical gradient.
 * @ST_GRADIENT_HORIZONTAL: A horizontal gradient.
 * @ST_GRADIENT_RADIAL: Lookup the style requested in the icon name.
 *
 * Used to specify options when rendering gradients.
 */
typedef enum {
  ST_GRADIENT_NONE,
  ST_GRADIENT_VERTICAL,
  ST_GRADIENT_HORIZONTAL,
  ST_GRADIENT_RADIAL
} StGradientType;

/**
 * StIconStyle:
 * @ST_ICON_STYLE_REQUESTED: Lookup the style requested in the icon name.
 * @ST_ICON_STYLE_REGULAR: Try to always load regular icons, even when symbolic
 *   icon names are given.
 * @ST_ICON_STYLE_SYMBOLIC: Try to always load symbolic icons, even when regular
 *   icon names are given.
 *
 * Used to specify options when looking up icons.
 */
typedef enum {
  ST_ICON_STYLE_REQUESTED,
  ST_ICON_STYLE_REGULAR,
  ST_ICON_STYLE_SYMBOLIC
} StIconStyle;

typedef struct _StThemeNodePaintState StThemeNodePaintState;

struct _StThemeNodePaintState {
  StThemeNode *node;

  float alloc_width;
  float alloc_height;

  float box_shadow_width;
  float box_shadow_height;

  float resource_scale;

  CoglPipeline *box_shadow_pipeline;
  CoglPipeline *prerendered_texture;
  CoglPipeline *prerendered_pipeline;
  CoglPipeline *corner_material[4];
};

StThemeNode *st_theme_node_new (StThemeContext *context,
                                StThemeNode    *parent_node,   /* can be null */
                                StTheme        *theme,         /* can be null */
                                GType           element_type,
                                const char     *element_id,
                                const char     *element_class,
                                const char     *pseudo_class,
                                const char     *inline_style);

StThemeNode *st_theme_node_get_parent (StThemeNode *node);

StTheme *st_theme_node_get_theme (StThemeNode *node);

gboolean    st_theme_node_equal (StThemeNode *node_a, StThemeNode *node_b);
guint       st_theme_node_hash  (StThemeNode *node);

GType       st_theme_node_get_element_type  (StThemeNode *node);
const char *st_theme_node_get_element_id    (StThemeNode *node);
GStrv       st_theme_node_get_element_classes (StThemeNode *node);
GStrv       st_theme_node_get_pseudo_classes (StThemeNode *node);

/* Generic getters ... these are not cached so are less efficient. The other
 * reason for adding the more specific version is that we can handle the
 * details of the actual CSS rules, which can be complicated, especially
 * for fonts
 */
gboolean st_theme_node_lookup_color  (StThemeNode  *node,
                                      const char   *property_name,
                                      gboolean      inherit,
                                      ClutterColor *color);
gboolean st_theme_node_lookup_double (StThemeNode  *node,
                                      const char   *property_name,
                                      gboolean      inherit,
                                      double       *value);
gboolean st_theme_node_lookup_length (StThemeNode *node,
                                      const char  *property_name,
                                      gboolean     inherit,
                                      gdouble     *length);
gboolean st_theme_node_lookup_time   (StThemeNode *node,
                                      const char  *property_name,
                                      gboolean     inherit,
                                      gdouble     *value);
gboolean st_theme_node_lookup_shadow (StThemeNode  *node,
                                      const char   *property_name,
                                      gboolean      inherit,
                                      StShadow    **shadow);
gboolean st_theme_node_lookup_url    (StThemeNode  *node,
                                      const char   *property_name,
                                      gboolean      inherit,
                                      GFile       **file);

/* Easier-to-use variants of the above, for application-level use */
void          st_theme_node_get_color  (StThemeNode  *node,
                                        const char   *property_name,
                                        ClutterColor *color);
gdouble       st_theme_node_get_double (StThemeNode  *node,
                                        const char   *property_name);
gdouble       st_theme_node_get_length (StThemeNode  *node,
                                        const char   *property_name);
StShadow     *st_theme_node_get_shadow (StThemeNode  *node,
                                        const char   *property_name);
GFile        *st_theme_node_get_url    (StThemeNode  *node,
                                        const char   *property_name);

/* Specific getters for particular properties: cached
 */
void st_theme_node_get_background_color (StThemeNode  *node,
                                         ClutterColor *color);
void st_theme_node_get_foreground_color (StThemeNode  *node,
                                         ClutterColor *color);
void st_theme_node_get_background_gradient (StThemeNode   *node,
                                            StGradientType *type,
                                            ClutterColor   *start,
                                            ClutterColor   *end);

GFile *st_theme_node_get_background_image (StThemeNode *node);

int    st_theme_node_get_border_width  (StThemeNode  *node,
                                        StSide        side);
int    st_theme_node_get_border_radius (StThemeNode  *node,
                                        StCorner      corner);
void   st_theme_node_get_border_color  (StThemeNode  *node,
                                        StSide        side,
                                        ClutterColor *color);

int    st_theme_node_get_outline_width (StThemeNode  *node);
void   st_theme_node_get_outline_color (StThemeNode  *node,
                                        ClutterColor *color);

double st_theme_node_get_padding       (StThemeNode  *node,
                                        StSide        side);

double st_theme_node_get_horizontal_padding (StThemeNode *node);
double st_theme_node_get_vertical_padding   (StThemeNode *node);

double st_theme_node_get_margin       (StThemeNode  *node,
                                       StSide        side);

int    st_theme_node_get_width         (StThemeNode  *node);
int    st_theme_node_get_height        (StThemeNode  *node);
int    st_theme_node_get_min_width     (StThemeNode  *node);
int    st_theme_node_get_min_height    (StThemeNode  *node);
int    st_theme_node_get_max_width     (StThemeNode  *node);
int    st_theme_node_get_max_height    (StThemeNode  *node);

int    st_theme_node_get_transition_duration (StThemeNode *node);

StIconStyle st_theme_node_get_icon_style (StThemeNode *node);

StTextDecoration st_theme_node_get_text_decoration (StThemeNode *node);

StTextAlign st_theme_node_get_text_align (StThemeNode *node);

double st_theme_node_get_letter_spacing (StThemeNode *node);

/* Font rule processing is pretty complicated, so we just hardcode it
 * under the standard font/font-family/font-size/etc names. This means
 * you can't have multiple separate styled fonts for a single item,
 * but that should be OK.
 */
const PangoFontDescription *st_theme_node_get_font (StThemeNode *node);

gchar *st_theme_node_get_font_features (StThemeNode *node);

StBorderImage *st_theme_node_get_border_image (StThemeNode *node);
StShadow      *st_theme_node_get_box_shadow   (StThemeNode *node);
StShadow      *st_theme_node_get_text_shadow  (StThemeNode *node);

StShadow      *st_theme_node_get_background_image_shadow (StThemeNode *node);

StIconColors  *st_theme_node_get_icon_colors  (StThemeNode *node);

/* Helpers for get_preferred_width()/get_preferred_height() ClutterActor vfuncs */
void st_theme_node_adjust_for_height       (StThemeNode  *node,
                                            float        *for_height);
void st_theme_node_adjust_preferred_width  (StThemeNode  *node,
                                            float        *min_width_p,
                                            float        *natural_width_p);
void st_theme_node_adjust_for_width        (StThemeNode  *node,
                                            float        *for_width);
void st_theme_node_adjust_preferred_height (StThemeNode  *node,
                                            float        *min_height_p,
                                            float        *natural_height_p);

/* Helper for allocate() ClutterActor vfunc */
void st_theme_node_get_content_box         (StThemeNode        *node,
                                            const ClutterActorBox *allocation,
                                            ClutterActorBox       *content_box);
/* Helper for StThemeNodeTransition */
void st_theme_node_get_paint_box           (StThemeNode           *node,
                                            const ClutterActorBox *allocation,
                                            ClutterActorBox       *paint_box);
/* Helper for background prerendering */
void st_theme_node_get_background_paint_box (StThemeNode           *node,
                                             const ClutterActorBox *allocation,
                                             ClutterActorBox       *paint_box);

gboolean st_theme_node_geometry_equal (StThemeNode *node,
                                       StThemeNode *other);
gboolean st_theme_node_paint_equal    (StThemeNode *node,
                                       StThemeNode *other);

/**
 * st_theme_node_paint: (skip)
 */
void st_theme_node_paint (StThemeNode            *node,
                          StThemeNodePaintState  *state,
                          CoglFramebuffer        *framebuffer,
                          const ClutterActorBox  *box,
                          guint8                  paint_opacity,
                          float                   resource_scale);

void st_theme_node_invalidate_background_image (StThemeNode *node);
void st_theme_node_invalidate_border_image (StThemeNode *node);

gchar * st_theme_node_to_string (StThemeNode *node);

void st_theme_node_paint_state_init (StThemeNodePaintState *state);
void st_theme_node_paint_state_free (StThemeNodePaintState *state);
void st_theme_node_paint_state_copy (StThemeNodePaintState *state,
                                     StThemeNodePaintState *other);
void st_theme_node_paint_state_invalidate (StThemeNodePaintState *state);
gboolean st_theme_node_paint_state_invalidate_for_file (StThemeNodePaintState *state,
                                                        GFile                 *file);

void st_theme_node_paint_state_set_node (StThemeNodePaintState *state,
                                         StThemeNode           *node);

G_END_DECLS

#endif /* __ST_THEME_NODE_H__ */
