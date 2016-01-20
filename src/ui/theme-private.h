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
 **/
struct _MetaFrameLayout
{
  /** Invisible border required by the theme */
  GtkBorder invisible_border;
  /** Border/padding of the entire frame */
  GtkBorder frame_border;
  /** Border/padding of the titlebar region */
  GtkBorder titlebar_border;
  /** Border/padding of titlebar buttons */
  GtkBorder button_border;

  /** Margin of title */
  GtkBorder title_margin;
  /** Margin of titlebar buttons */
  GtkBorder button_margin;

  /** Min size of titlebar region */
  GtkRequisition titlebar_min_size;
  /** Min size of titlebar buttons */
  GtkRequisition button_min_size;

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

  GtkBorder content_border;

  /* used for a memset hack */
#define ADDRESS_OF_BUTTON_RECTS(fgeom) (((char*)(fgeom)) + G_STRUCT_OFFSET (MetaFrameGeometry, close_rect))
#define LENGTH_OF_BUTTON_RECTS (G_STRUCT_OFFSET (MetaFrameGeometry, appmenu_rect) + sizeof (MetaButtonSpace) - G_STRUCT_OFFSET (MetaFrameGeometry, close_rect))

  /* The button rects (if changed adjust memset hack) */
  MetaButtonSpace close_rect;
  MetaButtonSpace max_rect;
  MetaButtonSpace min_rect;
  MetaButtonSpace menu_rect;
  MetaButtonSpace appmenu_rect;
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
  META_BUTTON_TYPE_LAST
} MetaButtonType;

typedef enum
{
  META_STYLE_ELEMENT_WINDOW,
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

/* Kinds of frame...
 *
 *  normal ->   focused / unfocused
 *  max    ->   focused / unfocused
 *  shaded ->   focused / unfocused
 *  max/shaded -> focused / unfocused
 *
 *  so 4 states with 2 sub-states each, meaning 8 total
 *
 * 8 window states times 7 or 8 window types. Except some
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
  META_FRAME_FOCUS_NO,
  META_FRAME_FOCUS_YES,
  META_FRAME_FOCUS_LAST
} MetaFrameFocus;

/**
 * A theme. This is a singleton class which groups all settings from a theme
 * together.
 */
struct _MetaTheme
{
  MetaFrameLayout *layouts[META_FRAME_TYPE_LAST];
};

void               meta_frame_layout_apply_scale (const MetaFrameLayout *layout,
                                                  PangoFontDescription  *font_desc);

MetaFrameLayout* meta_theme_get_frame_layout (MetaTheme     *theme,
                                              MetaFrameType  type);

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
                            cairo_surface_t        *mini_icon);

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
int                   meta_theme_get_window_scaling_factor (void);

#endif /* META_THEME_PRIVATE_H */
