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
typedef struct _MetaAlphaGradientSpec MetaAlphaGradientSpec; 
typedef struct _MetaColorSpec MetaColorSpec;
typedef struct _MetaFrameLayout MetaFrameLayout;
typedef struct _MetaFrameGeometry MetaFrameGeometry;
typedef struct _MetaTheme MetaTheme;
typedef struct _MetaPositionExprEnv MetaPositionExprEnv;
typedef struct _MetaDrawInfo MetaDrawInfo;

#define META_THEME_ERROR (g_quark_from_static_string ("meta-theme-error"))

typedef enum
{
  META_THEME_ERROR_FRAME_GEOMETRY,
  META_THEME_ERROR_BAD_CHARACTER,
  META_THEME_ERROR_BAD_PARENS,
  META_THEME_ERROR_UNKNOWN_VARIABLE,
  META_THEME_ERROR_DIVIDE_BY_ZERO,
  META_THEME_ERROR_MOD_ON_FLOAT,
  META_THEME_ERROR_FAILED
} MetaThemeError;

typedef enum
{
  META_BUTTON_SIZING_ASPECT,
  META_BUTTON_SIZING_FIXED,
  META_BUTTON_SIZING_LAST
} MetaButtonSizing;

/* Parameters used to calculate the geometry of the frame */
struct _MetaFrameLayout
{
  int refcount;
  
  /* Size of left/right/bottom sides */
  int left_width;
  int right_width;
  int bottom_height;
  
  /* Border of blue title region */
  GtkBorder title_border;

  /* Extra height for inside of title region, above the font height */
  int title_vertical_pad;
  
  /* indent of buttons from edges of frame */
  int right_titlebar_edge;
  int left_titlebar_edge;
  
  /* Size of buttons */
  MetaButtonSizing button_sizing;

  double button_aspect; /* height / width */
  
  int button_width;
  int button_height;

  /* Space around buttons */
  GtkBorder button_border;

  /* scale factor for title text */
  double title_scale;
  
  /* Whether title text will be displayed */
  guint has_title : 1;

  /* Round corners */
  guint top_left_corner_rounded : 1;
  guint top_right_corner_rounded : 1;
  guint bottom_left_corner_rounded : 1;
  guint bottom_right_corner_rounded : 1;
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

  GdkRectangle title_rect;

  int left_titlebar_edge;
  int right_titlebar_edge;
  int top_titlebar_edge;
  int bottom_titlebar_edge;

  /* used for a memset hack */
#define ADDRESS_OF_BUTTON_RECTS(fgeom) (((char*)(fgeom)) + G_STRUCT_OFFSET (MetaFrameGeometry, close_rect))
#define LENGTH_OF_BUTTON_RECTS (G_STRUCT_OFFSET (MetaFrameGeometry, right_right_background) + sizeof (GdkRectangle) - G_STRUCT_OFFSET (MetaFrameGeometry, close_rect))
  
  /* The button rects (if changed adjust memset hack) */
  GdkRectangle close_rect;
  GdkRectangle max_rect;
  GdkRectangle min_rect;
  GdkRectangle menu_rect;

#define MAX_MIDDLE_BACKGROUNDS (MAX_BUTTONS_PER_CORNER - 2)
  GdkRectangle left_left_background;
  GdkRectangle left_middle_backgrounds[MAX_MIDDLE_BACKGROUNDS];
  GdkRectangle left_right_background;
  GdkRectangle right_left_background;
  GdkRectangle right_middle_backgrounds[MAX_MIDDLE_BACKGROUNDS];
  GdkRectangle right_right_background;
  /* End of button rects (if changed adjust memset hack) */
  
  /* Round corners */
  guint top_left_corner_rounded : 1;
  guint top_right_corner_rounded : 1;
  guint bottom_left_corner_rounded : 1;
  guint bottom_right_corner_rounded : 1;
};

typedef enum
{
  META_IMAGE_FILL_SCALE, /* default, needs to be all-bits-zero for g_new0 */
  META_IMAGE_FILL_TILE
} MetaImageFillType;

typedef enum
{
  META_COLOR_SPEC_BASIC,
  META_COLOR_SPEC_GTK,
  META_COLOR_SPEC_BLEND,
  META_COLOR_SPEC_SHADE
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
  META_GTK_COLOR_TEXT_AA,
  META_GTK_COLOR_LAST
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
    struct {
      MetaColorSpec *base;
      double factor;
    } shade;
  } data;
};

struct _MetaGradientSpec
{
  MetaGradientType type;
  GSList *color_specs;
};

struct _MetaAlphaGradientSpec
{
  MetaGradientType type;
  unsigned char *alphas;
  int n_alphas;
};

struct _MetaDrawInfo
{
  GdkPixbuf   *mini_icon;
  GdkPixbuf   *icon;
  PangoLayout *title_layout;
  int title_layout_width;
  int title_layout_height;
  const MetaFrameGeometry *fgeom;
};

typedef enum
{
  /* Basic drawing */
  META_DRAW_LINE,
  META_DRAW_RECTANGLE,
  META_DRAW_ARC,

  /* Clip to a rectangle */
  META_DRAW_CLIP,
  
  /* Texture thingies */
  META_DRAW_TINT, /* just a filled rectangle with alpha */
  META_DRAW_GRADIENT,
  META_DRAW_IMAGE,
  
  /* GTK theme engine stuff */
  META_DRAW_GTK_ARROW,
  META_DRAW_GTK_BOX,
  META_DRAW_GTK_VLINE,

  /* App's window icon */
  META_DRAW_ICON,
  /* App's window title */
  META_DRAW_TITLE,
  /* a draw op list */
  META_DRAW_OP_LIST,
  /* tiled draw op list */
  META_DRAW_TILE
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
      char *x;
      char *y;
      char *width;
      char *height;
    } clip;
    
    struct {
      MetaColorSpec *color_spec;
      MetaAlphaGradientSpec *alpha_spec;
      char *x;
      char *y;
      char *width;
      char *height;
    } tint;

    struct {
      MetaGradientSpec *gradient_spec;
      MetaAlphaGradientSpec *alpha_spec;
      char *x;
      char *y;
      char *width;
      char *height;
    } gradient;

    struct {
      MetaColorSpec *colorize_spec;
      MetaAlphaGradientSpec *alpha_spec;
      GdkPixbuf *pixbuf;
      char *x;
      char *y;
      char *width;
      char *height;
      guint32 colorize_cache_pixel;
      GdkPixbuf *colorize_cache_pixbuf;
      MetaImageFillType fill_type;
      unsigned int vertical_stripes : 1;
      unsigned int horizontal_stripes : 1;
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

    struct {
      MetaAlphaGradientSpec *alpha_spec;
      char *x;
      char *y;
      char *width;
      char *height;
      MetaImageFillType fill_type;
    } icon;

    struct {
      MetaColorSpec *color_spec;
      char *x;
      char *y;
    } title;

    struct {
      MetaDrawOpList *op_list;
      char *x;
      char *y;
      char *width;
      char *height;
    } op_list;

    struct {
      MetaDrawOpList *op_list;
      char *x;
      char *y;
      char *width;
      char *height;
      char *tile_xoffset;
      char *tile_yoffset;
      char *tile_width;
      char *tile_height;
    } tile;
    
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
  /* Ordered so that background is drawn first */
  META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND,
  META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND,
  META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND,
  META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND,
  META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND,
  META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND,
  META_BUTTON_TYPE_CLOSE,
  META_BUTTON_TYPE_MAXIMIZE,
  META_BUTTON_TYPE_MINIMIZE,
  META_BUTTON_TYPE_MENU,
  META_BUTTON_TYPE_LAST
} MetaButtonType;

typedef enum
{
  META_MENU_ICON_TYPE_CLOSE,
  META_MENU_ICON_TYPE_MAXIMIZE,
  META_MENU_ICON_TYPE_UNMAXIMIZE,
  META_MENU_ICON_TYPE_MINIMIZE,
  META_MENU_ICON_TYPE_LAST
} MetaMenuIconType;

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
  META_FRAME_PIECE_TITLEBAR,
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
  META_FRAME_PIECE_TITLE,
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

#define N_GTK_STATES 5
struct _MetaFrameStyle
{
  int refcount;
  MetaFrameStyle *parent;
  MetaDrawOpList *buttons[META_BUTTON_TYPE_LAST][META_BUTTON_STATE_LAST];
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
  char *dirname;
  char *filename;
  char *readable_name;
  char *author;
  char *copyright;
  char *date;
  char *description;

  GHashTable *integer_constants;
  GHashTable *float_constants;
  GHashTable *images_by_filename;
  GHashTable *layouts_by_name;
  GHashTable *draw_op_lists_by_name;
  GHashTable *styles_by_name;
  GHashTable *style_sets_by_name;
  MetaFrameStyleSet *style_sets_by_type[META_FRAME_TYPE_LAST];
  MetaDrawOpList *menu_icons[META_MENU_ICON_TYPE_LAST][N_GTK_STATES];
};

struct _MetaPositionExprEnv
{
  int x;
  int y;
  int width;
  int height;
  /* size of an object being drawn, if it has a natural size */
  int object_width;
  int object_height;
  /* global object sizes, always available */
  int left_width;
  int right_width;
  int top_height;
  int bottom_height;
  int title_width;
  int title_height;
  int mini_icon_width;
  int mini_icon_height;
  int icon_width;
  int icon_height;
  /* Theme so we can look up constants */
  MetaTheme *theme;
};

MetaFrameLayout* meta_frame_layout_new           (void);
MetaFrameLayout* meta_frame_layout_copy          (const MetaFrameLayout *src);
void             meta_frame_layout_ref           (MetaFrameLayout       *layout);
void             meta_frame_layout_unref         (MetaFrameLayout       *layout);
void             meta_frame_layout_get_borders   (const MetaFrameLayout *layout,
                                                  int                    text_height,
                                                  MetaFrameFlags         flags,
                                                  int                   *top_height,
                                                  int                   *bottom_height,
                                                  int                   *left_width,
                                                  int                   *right_width);
void             meta_frame_layout_calc_geometry (const MetaFrameLayout  *layout,
                                                  int                     text_height,
                                                  MetaFrameFlags          flags,
                                                  int                     client_width,
                                                  int                     client_height,
                                                  const MetaButtonLayout *button_layout,
                                                  MetaFrameGeometry      *fgeom);

gboolean         meta_frame_layout_validate      (const MetaFrameLayout *layout,
                                                  GError               **error);

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
                                  const MetaDrawInfo  *info,
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
                                         const MetaDrawInfo  *info,
                                         int                   x,
                                         int                   y,
                                         int                   width,
                                         int                   height);
void           meta_draw_op_list_append (MetaDrawOpList       *op_list,
                                         MetaDrawOp           *op);
gboolean       meta_draw_op_list_validate (MetaDrawOpList    *op_list,
                                           GError           **error);
gboolean       meta_draw_op_list_contains (MetaDrawOpList    *op_list,
                                           MetaDrawOpList    *child);

MetaGradientSpec* meta_gradient_spec_new    (MetaGradientType        type);
void              meta_gradient_spec_free   (MetaGradientSpec       *desc);
GdkPixbuf*        meta_gradient_spec_render (const MetaGradientSpec *desc,
                                             GtkWidget              *widget,
                                             int                     width,
                                             int                     height);
gboolean          meta_gradient_spec_validate (MetaGradientSpec     *spec,
                                               GError              **error);

MetaAlphaGradientSpec* meta_alpha_gradient_spec_new  (MetaGradientType       type,
                                                      int                    n_alphas);
void                   meta_alpha_gradient_spec_free (MetaAlphaGradientSpec *spec);


MetaFrameStyle* meta_frame_style_new   (MetaFrameStyle *parent);
void            meta_frame_style_ref   (MetaFrameStyle *style);
void            meta_frame_style_unref (MetaFrameStyle *style);

void meta_frame_style_draw (MetaFrameStyle          *style,
                            GtkWidget               *widget,
                            GdkDrawable             *drawable,
                            int                      x_offset,
                            int                      y_offset,
                            const GdkRectangle      *clip,
                            const MetaFrameGeometry *fgeom,
                            int                      client_width,
                            int                      client_height,
                            PangoLayout             *title_layout,
                            int                      text_height,
                            MetaButtonState          button_states[META_BUTTON_TYPE_LAST],
                            GdkPixbuf               *mini_icon,
                            GdkPixbuf               *icon);


gboolean       meta_frame_style_validate (MetaFrameStyle    *style,
                                          GError           **error);

MetaFrameStyleSet* meta_frame_style_set_new   (MetaFrameStyleSet *parent);
void               meta_frame_style_set_ref   (MetaFrameStyleSet *style_set);
void               meta_frame_style_set_unref (MetaFrameStyleSet *style_set);

gboolean       meta_frame_style_set_validate  (MetaFrameStyleSet *style_set,
                                               GError           **error);

MetaTheme* meta_theme_get_current (void);
void       meta_theme_set_current (const char *name,
                                   gboolean    force_reload);

MetaTheme* meta_theme_new      (void);
void       meta_theme_free     (MetaTheme *theme);
gboolean   meta_theme_validate (MetaTheme *theme,
                                GError   **error);
GdkPixbuf* meta_theme_load_image (MetaTheme  *theme,
                                  const char *filename,
                                  GError    **error);

MetaFrameStyle* meta_theme_get_frame_style (MetaTheme     *theme,
                                            MetaFrameType  type,
                                            MetaFrameFlags flags);

double meta_theme_get_title_scale (MetaTheme     *theme,
                                   MetaFrameType  type,
                                   MetaFrameFlags flags);

void meta_theme_draw_frame (MetaTheme              *theme,
                            GtkWidget              *widget,
                            GdkDrawable            *drawable,
                            const GdkRectangle     *clip,
                            int                     x_offset,
                            int                     y_offset,
                            MetaFrameType           type,
                            MetaFrameFlags          flags,
                            int                     client_width,
                            int                     client_height,
                            PangoLayout            *title_layout,
                            int                     text_height,
                            const MetaButtonLayout *button_layout,
                            MetaButtonState         button_states[META_BUTTON_TYPE_LAST],
                            GdkPixbuf              *mini_icon,
                            GdkPixbuf              *icon);

void meta_theme_draw_menu_icon (MetaTheme          *theme,
                                GtkWidget          *widget,
                                GdkDrawable        *drawable,
                                const GdkRectangle *clip,
                                int                 x_offset,
                                int                 y_offset,
                                int                 width,
                                int                 height,
                                MetaMenuIconType    type);
     
void meta_theme_get_frame_borders (MetaTheme         *theme,
                                   MetaFrameType      type,
                                   int                text_height,
                                   MetaFrameFlags     flags,
                                   int               *top_height,
                                   int               *bottom_height,
                                   int               *left_width,
                                   int               *right_width);
void meta_theme_calc_geometry (MetaTheme              *theme,
                               MetaFrameType           type,
                               int                     text_height,
                               MetaFrameFlags          flags,
                               int                     client_width,
                               int                     client_height,
                               const MetaButtonLayout *button_layout,
                               MetaFrameGeometry      *fgeom);
                                   
MetaFrameLayout*   meta_theme_lookup_layout       (MetaTheme         *theme,
                                                   const char        *name);
void               meta_theme_insert_layout       (MetaTheme         *theme,
                                                   const char        *name,
                                                   MetaFrameLayout   *layout);
MetaDrawOpList*    meta_theme_lookup_draw_op_list (MetaTheme         *theme,
                                                   const char        *name);
void               meta_theme_insert_draw_op_list (MetaTheme         *theme,
                                                   const char        *name,
                                                   MetaDrawOpList    *op_list);
MetaFrameStyle*    meta_theme_lookup_style        (MetaTheme         *theme,
                                                   const char        *name);
void               meta_theme_insert_style        (MetaTheme         *theme,
                                                   const char        *name,
                                                   MetaFrameStyle    *style);
MetaFrameStyleSet* meta_theme_lookup_style_set    (MetaTheme         *theme,
                                                   const char        *name);
void               meta_theme_insert_style_set    (MetaTheme         *theme,
                                                   const char        *name,
                                                   MetaFrameStyleSet *style_set);
gboolean meta_theme_define_int_constant   (MetaTheme   *theme,
                                           const char  *name,
                                           int          value,
                                           GError     **error);
gboolean meta_theme_lookup_int_constant   (MetaTheme   *theme,
                                           const char  *name,
                                           int         *value);
gboolean meta_theme_define_float_constant (MetaTheme   *theme,
                                           const char  *name,
                                           double       value,
                                           GError     **error);
gboolean meta_theme_lookup_float_constant (MetaTheme   *theme,
                                           const char  *name,
                                           double      *value);

char*    meta_theme_replace_constants     (MetaTheme   *theme,
                                           const char  *expr,
                                           GError     **err);

/* random stuff */

PangoFontDescription* meta_gtk_widget_get_font_desc        (GtkWidget            *widget,
                                                            double                scale,
							    const PangoFontDescription *override);
int                   meta_pango_font_desc_get_text_height (PangoFontDescription *font_desc,
                                                            PangoContext         *context);


/* Enum converters */
MetaGtkColorComponent meta_color_component_from_string (const char            *str);
const char*           meta_color_component_to_string   (MetaGtkColorComponent  component);
MetaButtonState       meta_button_state_from_string    (const char            *str);
const char*           meta_button_state_to_string      (MetaButtonState        state);
MetaButtonType        meta_button_type_from_string     (const char            *str);
const char*           meta_button_type_to_string       (MetaButtonType         type);
MetaMenuIconType      meta_menu_icon_type_from_string  (const char            *str);
const char*           meta_menu_icon_type_to_string    (MetaMenuIconType       type);
MetaFramePiece        meta_frame_piece_from_string     (const char            *str);
const char*           meta_frame_piece_to_string       (MetaFramePiece         piece);
MetaFrameState        meta_frame_state_from_string     (const char            *str);
const char*           meta_frame_state_to_string       (MetaFrameState         state);
MetaFrameResize       meta_frame_resize_from_string    (const char            *str);
const char*           meta_frame_resize_to_string      (MetaFrameResize        resize);
MetaFrameFocus        meta_frame_focus_from_string     (const char            *str);
const char*           meta_frame_focus_to_string       (MetaFrameFocus         focus);
MetaFrameType         meta_frame_type_from_string      (const char            *str);
const char*           meta_frame_type_to_string        (MetaFrameType          type);
MetaGradientType      meta_gradient_type_from_string   (const char            *str);
const char*           meta_gradient_type_to_string     (MetaGradientType       type);
GtkStateType          meta_gtk_state_from_string       (const char            *str);
const char*           meta_gtk_state_to_string         (GtkStateType           state);
GtkShadowType         meta_gtk_shadow_from_string      (const char            *str);
const char*           meta_gtk_shadow_to_string        (GtkShadowType          shadow);
GtkArrowType          meta_gtk_arrow_from_string       (const char            *str);
const char*           meta_gtk_arrow_to_string         (GtkArrowType           arrow);
MetaImageFillType     meta_image_fill_type_from_string (const char            *str);
const char*           meta_image_fill_type_to_string   (MetaImageFillType      fill_type);


#endif
