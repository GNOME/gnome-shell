/* Metacity Theme Rendering */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_THEME_H
#define META_THEME_H

#include "gradient.h"
#include "common.h"
#include <gtk/gtkrc.h>

typedef struct _MetaFrameStyle MetaFrameStyle;
typedef struct _MetaFrameStyleSet MetaFrameStyleSet;
typedef struct _MetaDrawOp MetaDrawOp;
typedef struct _MetaDrawOpList MetaDrawOpList;
typedef struct _MetaGradientSpec MetaGradientSpec;
typedef struct _MetaColorSpec MetaColorSpec;
typedef struct _MetaFrameLayout MetaFrameLayout;
typedef struct _MetaFrameGeometry MetaFrameGeometry;
typedef struct _MetaTheme MetaTheme;
typedef struct _MetaPositionExprEnv MetaPositionExprEnv;

typedef enum
{
  META_SCALE_NONE,
  META_SCALE_VERTICALLY,
  META_SCALE_HORIZONTALLY,
  META_SCALE_BOTH
} MetaScaleMode;

/* Parameters used to calculate the geometry of the frame */
struct _MetaFrameLayout
{
  /* Size of left/right/bottom sides */
  int left_width;
  int right_width;
  int bottom_height;
  
  /* Border of blue title region */
  GtkBorder title_border;

  /* Border inside title region, around title */
  GtkBorder text_border;  
 
  /* padding on either side of spacer */
  int spacer_padding;

  /* Size of spacer */
  int spacer_width;
  int spacer_height;

  /* indent of buttons from edges of frame */
  int right_inset;
  int left_inset;
  
  /* Size of buttons */
  int button_width;
  int button_height;

  /* Space around buttons */
  GtkBorder button_border;

  /* Space inside button which is clickable but doesn't draw the
   * button icon
   */
  GtkBorder inner_button_border;
};


/* Calculated actual geometry of the frame */
struct _MetaFrameGeometry
{
  int left_width;
  int right_width;
  int top_height;
  int bottom_height;

  int width;
  int height;
  
  GdkRectangle close_rect;
  GdkRectangle max_rect;
  GdkRectangle min_rect;
  GdkRectangle spacer_rect;
  GdkRectangle menu_rect;
  GdkRectangle title_rect;

  int left_titlebar_edge;
  int right_titlebar_edge;
  int top_titlebar_edge;
  int bottom_titlebar_edge;
};


typedef enum
{
  META_COLOR_SPEC_BASIC,
  META_COLOR_SPEC_GTK,
  META_COLOR_SPEC_BLEND
} MetaColorSpecType;

typedef enum
{
  META_GTK_COLOR_FG,
  META_GTK_COLOR_BG,
  META_GTK_COLOR_LIGHT,
  META_GTK_COLOR_DARK,
  META_GTK_COLOR_MID,
  META_GTK_COLOR_TEXT,
  META_GTK_COLOR_BASE,
  META_GTK_COLOR_TEXT_AA
} MetaGtkColorComponent;

struct _MetaColorSpec
{
  MetaColorSpecType type;
  union
  {
    struct {
      GdkColor color;
    } basic;
    struct {
      MetaGtkColorComponent component;
      GtkStateType state;
    } gtk;
    struct {
      MetaColorSpec *foreground;
      MetaColorSpec *background;
      double alpha;
    } blend;
  } data;
};

struct _MetaGradientSpec
{
  MetaGradientType type;
  GSList *color_specs;
};

typedef enum
{
  /* Basic drawing */
  META_DRAW_LINE,
  META_DRAW_RECTANGLE,
  META_DRAW_ARC,

  /* Texture thingies */
  META_DRAW_TINT, /* just a filled rectangle with alpha */
  META_DRAW_GRADIENT,
  META_DRAW_IMAGE,
  
  /* GTK theme engine stuff */
  META_DRAW_GTK_ARROW,
  META_DRAW_GTK_BOX,
  META_DRAW_GTK_VLINE
} MetaDrawType;

struct _MetaDrawOp
{
  MetaDrawType type;

  /* Positions are strings because they can be expressions */
  union
  {
    struct {
      MetaColorSpec *color_spec;
      int dash_on_length;
      int dash_off_length;
      int width;
      char *x1;
      char *y1;
      char *x2;
      char *y2;
    } line;

    struct {
      MetaColorSpec *color_spec;
      gboolean filled;
      char *x;
      char *y;
      char *width;
      char *height;
    } rectangle;

    struct {
      MetaColorSpec *color_spec;
      gboolean filled;
      char *x;
      char *y;
      char *width;
      char *height;
      double start_angle;
      double extent_angle;
    } arc;
    
    struct {
      MetaColorSpec *color_spec;
      double alpha;
      char *x;
      char *y;
      char *width;
      char *height;
    } tint;

    struct {
      MetaGradientSpec *gradient_spec;
      double alpha;
      char *x;
      char *y;
      char *width;
      char *height;
    } gradient;

    struct {
      GdkPixbuf *pixbuf;
      double alpha;
      MetaScaleMode scale_mode;
      char *x;
      char *y;
      char *width;
      char *height;
    } image;
    
    struct {
      GtkStateType state;
      GtkShadowType shadow;
      GtkArrowType arrow;
      gboolean filled;
      char *x;
      char *y;
      char *width;
      char *height;
    } gtk_arrow;

    struct {
      GtkStateType state;
      GtkShadowType shadow;
      char *x;
      char *y;
      char *width;
      char *height;
    } gtk_box;

    struct {
      GtkStateType state;
      char *x;
      char *y1;
      char *y2;  
    } gtk_vline;
    
  } data;
};

struct _MetaDrawOpList
{
  int refcount;
  MetaDrawOp **ops;
  int n_ops;
  int n_allocated;
};

typedef enum
{
  META_BUTTON_STATE_NORMAL,
  META_BUTTON_STATE_PRESSED,
  META_BUTTON_STATE_PRELIGHT,
  META_BUTTON_STATE_LAST
} MetaButtonState;

typedef enum
{
  META_BUTTON_TYPE_CLOSE,
  META_BUTTON_TYPE_MAXIMIZE,
  META_BUTTON_TYPE_MINIMIZE,
  META_BUTTON_TYPE_MENU,
  META_BUTTON_TYPE_LAST
} MetaButtonType;

typedef enum
{
  /* Listed in the order in which the textures are drawn.
   * (though this only matters for overlaps of course.)
   * Buttons are drawn after the frame textures.
   *
   * On the corners, horizontal pieces are arbitrarily given the
   * corner area:
   *
   *   =====                 |====
   *   |                     |
   *   |       rather than   |
   *
   */
  
  /* entire frame */
  META_FRAME_PIECE_ENTIRE_BACKGROUND,
  /* entire titlebar background */
  META_FRAME_PIECE_TITLEBAR_BACKGROUND,
  /* portion of the titlebar background inside the titlebar
   * background edges
   */
  META_FRAME_PIECE_TITLEBAR_MIDDLE,
  /* left end of titlebar */
  META_FRAME_PIECE_LEFT_TITLEBAR_EDGE,
  /* right end of titlebar */
  META_FRAME_PIECE_RIGHT_TITLEBAR_EDGE,
  /* top edge of titlebar */
  META_FRAME_PIECE_TOP_TITLEBAR_EDGE,
  /* bottom edge of titlebar */
  META_FRAME_PIECE_BOTTOM_TITLEBAR_EDGE,
  /* render over title background (text area) */
  META_FRAME_PIECE_TITLE_BACKGROUND,
  /* left edge of the frame */
  META_FRAME_PIECE_LEFT_EDGE,
  /* right edge of the frame */
  META_FRAME_PIECE_RIGHT_EDGE,
  /* bottom edge of the frame */
  META_FRAME_PIECE_BOTTOM_EDGE,
  /* place over entire frame, after drawing everything else */
  META_FRAME_PIECE_OVERLAY,
  /* Used to get size of the enum */
  META_FRAME_PIECE_LAST
} MetaFramePiece;

struct _MetaFrameStyle
{
  int refcount;
  MetaFrameStyle *parent;
  MetaDrawOpList *button_icons[META_BUTTON_TYPE_LAST][META_BUTTON_STATE_LAST];
  MetaDrawOpList *button_backgrounds[META_BUTTON_TYPE_LAST][META_BUTTON_STATE_LAST];
  MetaDrawOpList *pieces[META_FRAME_PIECE_LAST];
  MetaFrameLayout *layout;
};

/* Kinds of frame...
 * 
 *  normal ->   noresize / vert only / horz only / both
 *              focused / unfocused
 *  max    ->   focused / unfocused
 *  shaded ->   focused / unfocused
 *  max/shaded -> focused / unfocused
 *
 *  so 4 states with 8 sub-states in one, 2 sub-states in the other 3,
 *  meaning 14 total
 *
 * 14 window states times 7 or 8 window types. Except some
 * window types never get a frame so that narrows it down a bit.
 * 
 */
typedef enum
{
  META_FRAME_STATE_NORMAL,
  META_FRAME_STATE_MAXIMIZED,
  META_FRAME_STATE_SHADED,
  META_FRAME_STATE_MAXIMIZED_AND_SHADED,
  META_FRAME_STATE_LAST
} MetaFrameState;

typedef enum
{
  META_FRAME_RESIZE_NONE,
  META_FRAME_RESIZE_VERTICAL,
  META_FRAME_RESIZE_HORIZONTAL,
  META_FRAME_RESIZE_BOTH,
  META_FRAME_RESIZE_LAST
} MetaFrameResize;

typedef enum
{
  META_FRAME_FOCUS_NO,
  META_FRAME_FOCUS_YES,
  META_FRAME_FOCUS_LAST
} MetaFrameFocus;

/* One StyleSet per window type (for window types that get a frame) */
struct _MetaFrameStyleSet
{
  int refcount;
  MetaFrameStyleSet *parent;
  MetaFrameStyle *normal_styles[META_FRAME_RESIZE_LAST][META_FRAME_FOCUS_LAST];
  MetaFrameStyle *maximized_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *shaded_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *maximized_and_shaded_styles[META_FRAME_FOCUS_LAST];
};

struct _MetaTheme
{
  char *name;
  char *filename;

  GHashTable *styles_by_name;
  GHashTable *style_sets_by_name;
  MetaFrameStyleSet *style_sets_by_type[META_FRAME_TYPE_LAST];
};

#define META_POSITION_EXPR_ERROR (g_quark_from_static_string ("meta-position-expr-error"))
typedef enum
{
  META_POSITION_EXPR_ERROR_BAD_CHARACTER,
  META_POSITION_EXPR_ERROR_BAD_PARENS,
  META_POSITION_EXPR_ERROR_UNKNOWN_VARIABLE,
  META_POSITION_EXPR_ERROR_DIVIDE_BY_ZERO,
  META_POSITION_EXPR_ERROR_MOD_ON_FLOAT,
  META_POSITION_EXPR_ERROR_FAILED
} MetaPositionExprError;

struct _MetaPositionExprEnv
{
  int x;
  int y;
  int width;
  int height;
  /* size of an image or whatever */
  int object_width;
  int object_height;
};

MetaFrameLayout* meta_frame_layout_new           (void);
void             meta_frame_layout_free          (MetaFrameLayout       *layout);
void             meta_frame_layout_get_borders   (const MetaFrameLayout *layout,
                                                  GtkWidget             *widget,
                                                  int                    text_height,
                                                  MetaFrameFlags         flags,
                                                  int                   *top_height,
                                                  int                   *bottom_height,
                                                  int                   *left_width,
                                                  int                   *right_width);
void             meta_frame_layout_calc_geometry (const MetaFrameLayout *layout,
                                                  GtkWidget             *widget,
                                                  int                    text_height,
                                                  MetaFrameFlags         flags,
                                                  int                    client_width,
                                                  int                    client_height,
                                                  MetaFrameGeometry     *fgeom);

gboolean meta_parse_position_expression (const char                 *expr,
                                         const MetaPositionExprEnv  *env,
                                         int                        *x_return,
                                         int                        *y_return,
                                         GError                    **err);
gboolean meta_parse_size_expression     (const char                 *expr,
                                         const MetaPositionExprEnv  *env,
                                         int                        *val_return,
                                         GError                    **err);

MetaColorSpec* meta_color_spec_new             (MetaColorSpecType  type);
MetaColorSpec* meta_color_spec_new_from_string (const char        *str,
                                                GError           **err);
MetaColorSpec* meta_color_spec_new_gtk         (MetaGtkColorComponent component,
                                                GtkStateType          state);
void           meta_color_spec_free            (MetaColorSpec     *spec);
void           meta_color_spec_render          (MetaColorSpec     *spec,
                                                GtkWidget         *widget,
                                                GdkColor          *color);


MetaDrawOp*    meta_draw_op_new  (MetaDrawType        type);
void           meta_draw_op_free (MetaDrawOp          *op);
void           meta_draw_op_draw (const MetaDrawOp    *op,
                                  GtkWidget           *widget,
                                  GdkDrawable         *drawable,
                                  const GdkRectangle  *clip,
                                  /* logical region being drawn */
                                  int                  x,
                                  int                  y,
                                  int                  width,
                                  int                  height);


MetaDrawOpList* meta_draw_op_list_new   (int                   n_preallocs);
void            meta_draw_op_list_ref   (MetaDrawOpList       *op_list);
void            meta_draw_op_list_unref (MetaDrawOpList       *op_list);
void            meta_draw_op_list_draw  (const MetaDrawOpList *op_list,
                                         GtkWidget            *widget,
                                         GdkDrawable          *drawable,
                                         const GdkRectangle   *clip,
                                         int                   x,
                                         int                   y,
                                         int                   width,
                                         int                   height);
void           meta_draw_op_list_append (MetaDrawOpList       *op_list,
                                         MetaDrawOp           *op);

MetaGradientSpec* meta_gradient_spec_new    (MetaGradientType        type);
void              meta_gradient_spec_free   (MetaGradientSpec       *desc);
GdkPixbuf*        meta_gradient_spec_render (const MetaGradientSpec *desc,
                                             GtkWidget              *widget,
                                             int                     width,
                                             int                     height);


MetaFrameStyle* meta_frame_style_new   (MetaFrameStyle *parent);
void            meta_frame_style_ref   (MetaFrameStyle *style);
void            meta_frame_style_unref (MetaFrameStyle *style);

void meta_frame_style_draw (MetaFrameStyle     *style,
                            GtkWidget          *widget,
                            GdkDrawable        *drawable,
                            int                 x_offset,
                            int                 y_offset,
                            const GdkRectangle *clip,
                            MetaFrameFlags      flags,
                            int                 client_width,
                            int                 client_height,
                            PangoLayout        *title_layout,
                            int                 text_height,
                            MetaButtonState     button_states[META_BUTTON_TYPE_LAST]);

MetaFrameStyleSet* meta_frame_style_set_new   (MetaFrameStyleSet *parent);
void               meta_frame_style_set_ref   (MetaFrameStyleSet *style_set);
void               meta_frame_style_set_unref (MetaFrameStyleSet *style_set);

MetaTheme* meta_theme_new  (void);
void       meta_theme_free (MetaTheme *theme);

MetaFrameStyle* meta_frame_style_get_test (void);

#endif
