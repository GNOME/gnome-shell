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

#include "frames.h"
#include "gradient.h"
#include <gtk/gtkrc.h>

typedef struct _MetaFrameStyle MetaFrameStyle;
typedef struct _MetaFrameStyleSet MetaFrameStyleSet;
typedef struct _MetaTextureSpec MetaTextureSpec;
typedef struct _MetaGradientSpec MetaGradientSpec;
typedef struct _MetaColorSpec MetaColorSpec;

typedef enum
{
  META_COLOR_SPEC_BASIC,
  META_COLOR_SPEC_GTK,
  META_COLOR_SPEC_BLEND
} MetaColorSpecType;

struct _MetaColorSpec
{
  MetaColorSpecType type;
  union
  {
    struct {
      GdkColor color;
    } basic;
    struct {
      GtkRcFlags component;
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
  META_TEXTURE_SOLID,
  META_TEXTURE_GRADIENT,
  META_TEXTURE_IMAGE
} MetaTextureType;

struct _MetaTextureSpec
{
  MetaTextureType type;

  union
  {
    struct {
      MetaColorSpec *color_spec;
    } solid;
    struct {
      MetaGradientSpec *gradient_spec;
    } gradient;
    struct {
      GdkPixbuf *pixbuf;
    } image;
  } data;
};

typedef enum
{
  META_BUTTON_STATE_UNFOCUSED,
  META_BUTTON_STATE_FOCUSED,
  META_BUTTON_STATE_INSENSITIVE,
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
  
  /* place over entire frame, scaled both */
  META_FRAME_PIECE_ENTIRE_BACKGROUND,
  /* place over entire titlebar background, scaled both */
  META_FRAME_PIECE_TITLEBAR_BACKGROUND,
  /* place on left end of titlebar, scaled vert */
  META_FRAME_PIECE_LEFT_TITLEBAR_EDGE,
  /* place on right end of titlebar, scaled vert */
  META_FRAME_PIECE_RIGHT_TITLEBAR_EDGE,
  /* place on top edge of titlebar, scaled horiz */
  META_FRAME_PIECE_TOP_TITLEBAR_EDGE,
  /* place on bottom edge of titlebar, scaled horiz */
  META_FRAME_PIECE_BOTTOM_TITLEBAR_EDGE,
  /* place on left end of top edge of titlebar, unscaled */
  META_FRAME_PIECE_LEFT_END_OF_TOP_TITLEBAR_EDGE,
  /* place on right end of top edge of titlebar, unscaled */
  META_FRAME_PIECE_RIGHT_END_OF_TOP_TITLEBAR_EDGE,
  /* place on left end of bottom edge of titlebar, unscaled */
  META_FRAME_PIECE_LEFT_END_OF_BOTTOM_TITLEBAR_EDGE,
  /* place on right end of bottom edge of titlebar, unscaled */
  META_FRAME_PIECE_RIGHT_END_OF_BOTTOM_TITLEBAR_EDGE,
  /* place on top end of left titlebar edge, unscaled */
  META_FRAME_PIECE_TOP_END_OF_LEFT_TITLEBAR_EDGE,
  /* place on bottom end of left titlebar edge, unscaled */
  META_FRAME_PIECE_BOTTOM_END_OF_LEFT_TITLEBAR_EDGE,
  /* place on top end of right titlebar edge, unscaled */
  META_FRAME_PIECE_TOP_END_OF_RIGHT_TITLEBAR_EDGE,
  /* place on bottom end of right titlebar edge, unscaled */
  META_FRAME_PIECE_BOTTOM_END_OF_RIGHT_TITLEBAR_EDGE,
  /* render over title background (text area), scaled both */
  META_FRAME_PIECE_TITLE_BACKGROUND,
  /* render over left side of TITLE_BACKGROUND, scaled vert */
  META_FRAME_PIECE_LEFT_TITLE_BACKGROUND,
  /* render over right side of TITLE_BACKGROUND, scaled vert */
  META_FRAME_PIECE_RIGHT_TITLE_BACKGROUND,
  /* place on left edge of the frame, scaled vert */
  META_FRAME_PIECE_LEFT_EDGE,
  /* place on right edge of the frame, scaled vert */
  META_FRAME_PIECE_RIGHT_EDGE,
  /* place on bottom edge of the frame, scaled horiz */
  META_FRAME_PIECE_BOTTOM_EDGE,
  /* place on top end of left edge of the frame, unscaled */
  META_FRAME_PIECE_TOP_END_OF_LEFT_EDGE,
  /* place on bottom end of left edge of the frame, unscaled */
  META_FRAME_PIECE_BOTTOM_END_OF_LEFT_EDGE,
  /* place on top end of right edge of the frame, unscaled */
  META_FRAME_PIECE_TOP_END_OF_RIGHT_EDGE,
  /* place on bottom end of right edge of the frame, unscaled */
  META_FRAME_PIECE_BOTTOM_END_OF_RIGHT_EDGE,
  /* place on left end of bottom edge of the frame, unscaled */
  META_FRAME_PIECE_LEFT_END_OF_BOTTOM_EDGE,
  /* place on right end of bottom edge of the frame, unscaled */
  META_FRAME_PIECE_RIGHT_END_OF_BOTTOM_EDGE,
  /* Used to get size of the enum */
  META_FRAME_PIECE_LAST
} MetaFramePiece;

struct _MetaFrameStyle
{
  MetaTextureSpec *button_icons[META_BUTTON_TYPE_LAST][META_BUTTON_STATE_LAST];
  MetaTextureSpec *button_backgrounds[META_BUTTON_TYPE_LAST][META_BUTTON_STATE_LAST];
  MetaTextureSpec *pieces[META_FRAME_PIECE_LAST];  
};

typedef enum
{
  META_TEXTURE_DRAW_UNSCALED,
  META_TEXTURE_DRAW_SCALED_VERTICALLY,
  META_TEXTURE_DRAW_SCALED_HORIZONTALLY,
  META_TEXTURE_DRAW_SCALED_BOTH
} MetaTextureDrawMode;

MetaColorSpec* meta_color_spec_new    (MetaColorSpecType  type);
void           meta_color_spec_free   (MetaColorSpec     *spec);
void           meta_color_spec_render (MetaColorSpec     *spec,
                                       GtkWidget         *widget,
                                       GdkColor          *color);

MetaGradientSpec* meta_gradient_spec_new    (MetaGradientType        type);
void              meta_gradient_spec_free   (MetaGradientSpec       *desc);
GdkPixbuf*        meta_gradient_spec_render (const MetaGradientSpec *desc,
                                             GtkWidget              *widget,
                                             int                     width,
                                             int                     height);

MetaTextureSpec* meta_texture_spec_new    (MetaTextureType        type);
void             meta_texture_spec_free   (MetaTextureSpec       *desc);
void             meta_texture_spec_draw   (const MetaTextureSpec *desc,
                                           GtkWidget             *widget,
                                           GdkDrawable           *drawable,
                                           const GdkRectangle    *clip,
                                           MetaTextureDrawMode    mode,
                                           /* logical region being drawn,
                                            * scale to this area if in SCALED
                                            * mode
                                            */
                                           int                    x,
                                           int                    y,
                                           int                    width,
                                           int                    height);

#endif
