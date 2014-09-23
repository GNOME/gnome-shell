/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_THEME_PRIVATE_H
#define META_THEME_PRIVATE_H

#include <meta/boxes.h>
#include <meta/theme.h>
#include <meta/common.h>
#include <gtk/gtk.h>

/**
 * MetaStyleInfo: (skip)
 *
 */
typedef struct _MetaStyleInfo MetaStyleInfo;
/**
 * MetaFrameStyle: (skip)
 *
 */
typedef struct _MetaFrameStyle MetaFrameStyle;
/**
 * MetaFrameStyleSet: (skip)
 *
 */
typedef struct _MetaFrameStyleSet MetaFrameStyleSet;
/**
 * MetaFrameLayout: (skip)
 *
 */
typedef struct _MetaFrameLayout MetaFrameLayout;
/**
 * MetaButtonSpace: (skip)
 *
 */
typedef struct _MetaButtonSpace MetaButtonSpace;
/**
 * MetaFrameGeometry: (skip)
 *
 */
typedef struct _MetaFrameGeometry MetaFrameGeometry;

/**
 * Various parameters used to calculate the geometry of a frame.
 * They are used inside a MetaFrameStyle.
 * This corresponds closely to the <frame_geometry> tag in a theme file.
 **/
struct _MetaFrameLayout
{
  /** Reference count. */
  int refcount;

  /** Size of left side */
  int left_width;
  /** Size of right side */
  int right_width;
  /** Size of bottom side */
  int bottom_height;

  /** Border of blue title region
   * \bug (blue?!)
   **/
  GtkBorder title_border;

  /** Extra height for inside of title region, above the font height */
  int title_vertical_pad;

  /** Right indent of buttons from edges of frame */
  int right_titlebar_edge;
  /** Left indent of buttons from edges of frame */
  int left_titlebar_edge;

  /** Width of a button; set even when we are using aspect sizing */
  int button_width;

  /** Height of a button; set even when we are using aspect sizing */
  int button_height;

  /** Space around buttons */
  GtkBorder button_border;

  /** Size of images in buttons */
  guint icon_size;

  /** Space between titlebar elements */
  guint titlebar_spacing;

  /** scale factor for title text */
  double title_scale;

  /** Whether title text will be displayed */
  guint has_title : 1;

  /** Whether we should hide the buttons */
  guint hide_buttons : 1;

  /** Radius of the top left-hand corner; 0 if not rounded */
  guint top_left_corner_rounded_radius;
  /** Radius of the top right-hand corner; 0 if not rounded */
  guint top_right_corner_rounded_radius;
  /** Radius of the bottom left-hand corner; 0 if not rounded */
  guint bottom_left_corner_rounded_radius;
  /** Radius of the bottom right-hand corner; 0 if not rounded */
  guint bottom_right_corner_rounded_radius;
};

/**
 * The computed size of a button (really just a way of tying its
 * visible and clickable areas together).
 * The reason for two different rectangles here is Fitts' law & maximized
 * windows; see bug #97703 for more details.
 */
struct _MetaButtonSpace
{
  /** The screen area where the button's image is drawn */
  GdkRectangle visible;
  /** The screen area where the button can be activated by clicking */
  GdkRectangle clickable;
};

/**
 * Calculated actual geometry of the frame
 */
struct _MetaFrameGeometry
{
  MetaFrameBorders borders;

  int width;
  int height;

  GdkRectangle title_rect;

  int left_titlebar_edge;
  int right_titlebar_edge;
  int top_titlebar_edge;
  int bottom_titlebar_edge;

  /* used for a memset hack */
#define ADDRESS_OF_BUTTON_RECTS(fgeom) (((char*)(fgeom)) + G_STRUCT_OFFSET (MetaFrameGeometry, close_rect))
#define LENGTH_OF_BUTTON_RECTS (G_STRUCT_OFFSET (MetaFrameGeometry, unstick_rect) + sizeof (GdkRectangle) - G_STRUCT_OFFSET (MetaFrameGeometry, close_rect))

  /* The button rects (if changed adjust memset hack) */
  MetaButtonSpace close_rect;
  MetaButtonSpace max_rect;
  MetaButtonSpace min_rect;
  MetaButtonSpace menu_rect;
  MetaButtonSpace appmenu_rect;
  MetaButtonSpace shade_rect;
  MetaButtonSpace above_rect;
  MetaButtonSpace stick_rect;
  MetaButtonSpace unshade_rect;
  MetaButtonSpace unabove_rect;
  MetaButtonSpace unstick_rect;
  /* End of button rects (if changed adjust memset hack) */

  /* Saved button layout */
  MetaButtonLayout button_layout;
  int n_left_buttons;
  int n_right_buttons;

  /* Round corners */
  guint top_left_corner_rounded_radius;
  guint top_right_corner_rounded_radius;
  guint bottom_left_corner_rounded_radius;
  guint bottom_right_corner_rounded_radius;
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
  META_BUTTON_TYPE_APPMENU,
  META_BUTTON_TYPE_SHADE,
  META_BUTTON_TYPE_ABOVE,
  META_BUTTON_TYPE_STICK,
  META_BUTTON_TYPE_UNSHADE,
  META_BUTTON_TYPE_UNABOVE,
  META_BUTTON_TYPE_UNSTICK,
  META_BUTTON_TYPE_LAST
} MetaButtonType;

typedef enum
{
  META_STYLE_ELEMENT_FRAME,
  META_STYLE_ELEMENT_TITLEBAR,
  META_STYLE_ELEMENT_TITLE,
  META_STYLE_ELEMENT_BUTTON,
  META_STYLE_ELEMENT_IMAGE,
  META_STYLE_ELEMENT_LAST
} MetaStyleElement;

struct _MetaStyleInfo
{
  int refcount;

  GtkStyleContext *styles[META_STYLE_ELEMENT_LAST];
};

/**
 * How to draw a frame in a particular state (say, a focussed, non-maximised,
 * resizable frame). This corresponds closely to the <frame_style> tag
 * in a theme file.
 */
struct _MetaFrameStyle
{
  /** Reference count. */
  int refcount;
  /**
   * Parent style.
   * Settings which are unspecified here will be taken from there.
   */
  MetaFrameStyle *parent;
  /**
   * Details such as the height and width of each edge, the corner rounding,
   * and the aspect ratio of the buttons.
   */
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
  META_FRAME_STATE_TILED_LEFT,
  META_FRAME_STATE_TILED_RIGHT,
  META_FRAME_STATE_SHADED,
  META_FRAME_STATE_MAXIMIZED_AND_SHADED,
  META_FRAME_STATE_TILED_LEFT_AND_SHADED,
  META_FRAME_STATE_TILED_RIGHT_AND_SHADED,
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

/**
 * How to draw frames at different times: when it's maximised or not, shaded
 * or not, when it's focussed or not, and (for non-maximised windows), when
 * it can be horizontally or vertically resized, both, or neither.
 * Not all window types actually get a frame.
 *
 * A theme contains one of these objects for each type of window (each
 * MetaFrameType), that is, normal, dialogue (modal and non-modal), etc.
 *
 * This corresponds closely to the <frame_style_set> tag in a theme file.
 */
struct _MetaFrameStyleSet
{
  int refcount;
  MetaFrameStyleSet *parent;
  MetaFrameStyle *normal_styles[META_FRAME_RESIZE_LAST][META_FRAME_FOCUS_LAST];
  MetaFrameStyle *maximized_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *tiled_left_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *tiled_right_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *shaded_styles[META_FRAME_RESIZE_LAST][META_FRAME_FOCUS_LAST];
  MetaFrameStyle *maximized_and_shaded_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *tiled_left_and_shaded_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *tiled_right_and_shaded_styles[META_FRAME_FOCUS_LAST];
};

/**
 * A theme. This is a singleton class which groups all settings from a theme
 * together.
 */
struct _MetaTheme
{
  MetaFrameStyleSet *style_sets_by_type[META_FRAME_TYPE_LAST];
};

MetaFrameLayout* meta_frame_layout_new           (void);
MetaFrameLayout* meta_frame_layout_copy          (const MetaFrameLayout *src);
void             meta_frame_layout_ref           (MetaFrameLayout       *layout);
void             meta_frame_layout_unref         (MetaFrameLayout       *layout);
void             meta_frame_layout_get_borders   (const MetaFrameLayout *layout,
                                                  int                    text_height,
                                                  MetaFrameFlags         flags,
                                                  MetaFrameType          type,
                                                  MetaFrameBorders      *borders);

MetaFrameStyle* meta_frame_style_new   (MetaFrameStyle *parent);
void            meta_frame_style_ref   (MetaFrameStyle *style);
void            meta_frame_style_unref (MetaFrameStyle *style);

void            meta_frame_style_apply_scale (const MetaFrameStyle *style,
                                              PangoFontDescription *font_desc);

MetaFrameStyleSet* meta_frame_style_set_new   (MetaFrameStyleSet *parent);
void               meta_frame_style_set_ref   (MetaFrameStyleSet *style_set);
void               meta_frame_style_set_unref (MetaFrameStyleSet *style_set);

MetaFrameStyle* meta_theme_get_frame_style (MetaTheme     *theme,
                                            MetaFrameType  type,
                                            MetaFrameFlags flags);

MetaStyleInfo * meta_theme_create_style_info (GdkScreen   *screen,
                                              const gchar *variant);
MetaStyleInfo * meta_style_info_ref          (MetaStyleInfo *style);
void            meta_style_info_unref        (MetaStyleInfo  *style_info);

void            meta_style_info_set_flags    (MetaStyleInfo  *style_info,
                                              MetaFrameFlags  flags);

PangoFontDescription * meta_style_info_create_font_desc (MetaStyleInfo *style_info);

void meta_theme_draw_frame (MetaTheme              *theme,
                            MetaStyleInfo          *style_info,
                            cairo_t                *cr,
                            MetaFrameType           type,
                            MetaFrameFlags          flags,
                            int                     client_width,
                            int                     client_height,
                            PangoLayout            *title_layout,
                            int                     text_height,
                            const MetaButtonLayout *button_layout,
                            MetaButtonState         button_states[META_BUTTON_TYPE_LAST],
                            GdkPixbuf              *mini_icon);

void meta_theme_get_frame_borders (MetaTheme         *theme,
                                   MetaStyleInfo     *style_info,
                                   MetaFrameType      type,
                                   int                text_height,
                                   MetaFrameFlags     flags,
                                   MetaFrameBorders  *borders);

void meta_theme_calc_geometry (MetaTheme              *theme,
                               MetaStyleInfo          *style_info,
                               MetaFrameType           type,
                               int                     text_height,
                               MetaFrameFlags          flags,
                               int                     client_width,
                               int                     client_height,
                               const MetaButtonLayout *button_layout,
                               MetaFrameGeometry      *fgeom);

/* random stuff */

int                   meta_pango_font_desc_get_text_height (const PangoFontDescription *font_desc,
                                                            PangoContext         *context);

#define META_THEME_ALLOWS(theme, feature) (theme->format_version >= feature)

/* What version of the theme file format were various features introduced in? */
#define META_THEME_SHADE_STICK_ABOVE_BUTTONS 2
#define META_THEME_UBIQUITOUS_CONSTANTS 2
#define META_THEME_VARIED_ROUND_CORNERS 2
#define META_THEME_IMAGES_FROM_ICON_THEMES 2
#define META_THEME_UNRESIZABLE_SHADED_STYLES 2
#define META_THEME_DEGREES_IN_ARCS 2
#define META_THEME_HIDDEN_BUTTONS 2
#define META_THEME_COLOR_CONSTANTS 2
#define META_THEME_FRAME_BACKGROUNDS 2

#endif /* META_THEME_PRIVATE_H */
