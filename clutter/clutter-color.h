/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By: Matthew Allum  <mallum@openedhand.com>
 *              Emmanuele Bassi <ebassi@linux.intel.com>
 *
 * Copyright (C) 2006, 2007, 2008 OpenedHand
 * Copyright (C) 2009 Intel Corp.
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
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_COLOR_H__
#define __CLUTTER_COLOR_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_COLOR	(clutter_color_get_type ())

/**
 * ClutterColor:
 * @red: red component, between 0 and 255
 * @green: green component, between 0 and 255
 * @blue: blue component, between 0 and 255
 * @alpha: alpha component, between 0 and 255
 *
 * Color representation.
 */
struct _ClutterColor
{
  /*< public >*/
  guint8 red;
  guint8 green;
  guint8 blue;
  
  guint8 alpha;
};

GType         clutter_color_get_type   (void) G_GNUC_CONST;

ClutterColor *clutter_color_new         (guint8              red,
                                         guint8              green,
                                         guint8              blue,
                                         guint8              alpha);
ClutterColor *clutter_color_copy        (const ClutterColor *color);
void          clutter_color_free        (ClutterColor       *color);

void          clutter_color_add         (const ClutterColor *a,
                                         const ClutterColor *b,
                                         ClutterColor       *result);
void          clutter_color_subtract    (const ClutterColor *a,
                                         const ClutterColor *b,
                                         ClutterColor       *result);
void          clutter_color_lighten     (const ClutterColor *color,
                                         ClutterColor       *result);
void          clutter_color_darken      (const ClutterColor *color,
                                         ClutterColor       *result);
void          clutter_color_shade       (const ClutterColor *color,
                                         gdouble             factor,
                                         ClutterColor       *result);

gchar *       clutter_color_to_string   (const ClutterColor *color);
gboolean      clutter_color_from_string (ClutterColor       *color,
                                         const gchar        *str);

void          clutter_color_to_hls      (const ClutterColor *color,
                                         gfloat             *hue,
					 gfloat             *luminance,
					 gfloat             *saturation);
void          clutter_color_from_hls    (ClutterColor       *color,
                                         gfloat              hue,
                                         gfloat              luminance,
                                         gfloat              saturation);

guint32       clutter_color_to_pixel    (const ClutterColor *color);
void          clutter_color_from_pixel  (ClutterColor       *color,
                                         guint32             pixel);

guint         clutter_color_hash        (gconstpointer       v);
gboolean      clutter_color_equal       (gconstpointer       v1,
                                         gconstpointer       v2);

void          clutter_color_interpolate (const ClutterColor *initial,
                                         const ClutterColor *final,
                                         gdouble             progress,
                                         ClutterColor       *result);

#define CLUTTER_TYPE_PARAM_COLOR           (clutter_param_color_get_type ())
#define CLUTTER_PARAM_SPEC_COLOR(pspec)    (G_TYPE_CHECK_INSTANCE_CAST ((pspec), CLUTTER_TYPE_PARAM_COLOR, ClutterParamSpecColor))
#define CLUTTER_IS_PARAM_SPEC_COLOR(pspec) (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), CLUTTER_TYPE_PARAM_COLOR))

/**
 * CLUTTER_VALUE_HOLDS_COLOR:
 * @x: a #GValue
 *
 * Evaluates to %TRUE if @x holds a #ClutterColor<!-- -->.
 *
 * Since: 1.0
 */
#define CLUTTER_VALUE_HOLDS_COLOR(x)       (G_VALUE_HOLDS ((x), CLUTTER_TYPE_COLOR))

typedef struct _ClutterParamSpecColor  ClutterParamSpecColor;

/**
 * ClutterParamSpecColor: (skip)
 * @default_value: default color value
 *
 * A #GParamSpec subclass for defining properties holding
 * a #ClutterColor.
 *
 * Since: 1.0
 */
struct _ClutterParamSpecColor
{
  /*< private >*/
  GParamSpec    parent_instance;

  /*< public >*/
  ClutterColor *default_value;
};

void                    clutter_value_set_color         (GValue             *value,
                                                         const ClutterColor *color);
const ClutterColor *    clutter_value_get_color         (const GValue       *value);

GType       clutter_param_color_get_type (void) G_GNUC_CONST;
GParamSpec *clutter_param_spec_color     (const gchar        *name,
                                          const gchar        *nick,
                                          const gchar        *blurb,
                                          const ClutterColor *default_value,
                                          GParamFlags         flags);

/**
 * ClutterStaticColor:
 * @CLUTTER_COLOR_WHITE: White color (ffffffff)
 * @CLUTTER_COLOR_BLACK: Black color (000000ff)
 * @CLUTTER_COLOR_RED: Red color (ff0000ff)
 * @CLUTTER_COLOR_DARK_RED: Dark red color (800000ff)
 * @CLUTTER_COLOR_GREEN: Green color (00ff00ff)
 * @CLUTTER_COLOR_DARK_GREEN: Dark green color (008000ff)
 * @CLUTTER_COLOR_BLUE: Blue color (0000ffff)
 * @CLUTTER_COLOR_DARK_BLUE: Dark blue color (000080ff)
 * @CLUTTER_COLOR_CYAN: Cyan color (00ffffff)
 * @CLUTTER_COLOR_DARK_CYAN: Dark cyan color (008080ff)
 * @CLUTTER_COLOR_MAGENTA: Magenta color (ff00ffff)
 * @CLUTTER_COLOR_DARK_MAGENTA: Dark magenta color (800080ff)
 * @CLUTTER_COLOR_YELLOW: Yellow color (ffff00ff)
 * @CLUTTER_COLOR_DARK_YELLOW: Dark yellow color (808000ff)
 * @CLUTTER_COLOR_GRAY: Gray color (a0a0a4ff)
 * @CLUTTER_COLOR_DARK_GRAY: Dark Gray color (808080ff)
 * @CLUTTER_COLOR_LIGHT_GRAY: Light gray color (c0c0c0ff)
 * @CLUTTER_COLOR_BUTTER: Butter color (edd400ff)
 * @CLUTTER_COLOR_BUTTER_LIGHT: Light butter color (fce94fff)
 * @CLUTTER_COLOR_BUTTER_DARK: Dark butter color (c4a000ff)
 * @CLUTTER_COLOR_ORANGE: Orange color (f57900ff)
 * @CLUTTER_COLOR_ORANGE_LIGHT: Light orange color (fcaf3fff)
 * @CLUTTER_COLOR_ORANGE_DARK: Dark orange color (ce5c00ff)
 * @CLUTTER_COLOR_CHOCOLATE: Chocolate color (c17d11ff)
 * @CLUTTER_COLOR_CHOCOLATE_LIGHT: Light chocolate color (e9b96eff)
 * @CLUTTER_COLOR_CHOCOLATE_DARK: Dark chocolate color (8f5902ff)
 * @CLUTTER_COLOR_CHAMELEON: Chameleon color (73d216ff)
 * @CLUTTER_COLOR_CHAMELEON_LIGHT: Light chameleon color (8ae234ff)
 * @CLUTTER_COLOR_CHAMELEON_DARK: Dark chameleon color (4e9a06ff)
 * @CLUTTER_COLOR_SKY_BLUE: Sky color (3465a4ff)
 * @CLUTTER_COLOR_SKY_BLUE_LIGHT: Light sky color (729fcfff)
 * @CLUTTER_COLOR_SKY_BLUE_DARK: Dark sky color (204a87ff)
 * @CLUTTER_COLOR_PLUM: Plum color (75507bff)
 * @CLUTTER_COLOR_PLUM_LIGHT: Light plum color (ad7fa8ff)
 * @CLUTTER_COLOR_PLUM_DARK: Dark plum color (5c3566ff)
 * @CLUTTER_COLOR_SCARLET_RED: Scarlet red color (cc0000ff)
 * @CLUTTER_COLOR_SCARLET_RED_LIGHT: Light scarlet red color (ef2929ff)
 * @CLUTTER_COLOR_SCARLET_RED_DARK: Dark scarlet red color (a40000ff)
 * @CLUTTER_COLOR_ALUMINIUM_1: Aluminium, first variant (eeeeecff)
 * @CLUTTER_COLOR_ALUMINIUM_2: Aluminium, second variant (d3d7cfff)
 * @CLUTTER_COLOR_ALUMINIUM_3: Aluminium, third variant (babdb6ff)
 * @CLUTTER_COLOR_ALUMINIUM_4: Aluminium, fourth variant (888a85ff)
 * @CLUTTER_COLOR_ALUMINIUM_5: Aluminium, fifth variant (555753ff)
 * @CLUTTER_COLOR_ALUMINIUM_6: Aluminium, sixth variant (2e3436ff)
 * @CLUTTER_COLOR_TRANSPARENT: Transparent color (00000000)
 *
 * Named colors, for accessing global colors defined by Clutter
 *
 * Since: 1.6
 */
typedef enum { /*< prefix=CLUTTER_COLOR >*/
  /* CGA/EGA-like palette */
  CLUTTER_COLOR_WHITE           = 0,
  CLUTTER_COLOR_BLACK,
  CLUTTER_COLOR_RED,
  CLUTTER_COLOR_DARK_RED,
  CLUTTER_COLOR_GREEN,
  CLUTTER_COLOR_DARK_GREEN,
  CLUTTER_COLOR_BLUE,
  CLUTTER_COLOR_DARK_BLUE,
  CLUTTER_COLOR_CYAN,
  CLUTTER_COLOR_DARK_CYAN,
  CLUTTER_COLOR_MAGENTA,
  CLUTTER_COLOR_DARK_MAGENTA,
  CLUTTER_COLOR_YELLOW,
  CLUTTER_COLOR_DARK_YELLOW,
  CLUTTER_COLOR_GRAY,
  CLUTTER_COLOR_DARK_GRAY,
  CLUTTER_COLOR_LIGHT_GRAY,

  /* Tango icon palette */
  CLUTTER_COLOR_BUTTER,
  CLUTTER_COLOR_BUTTER_LIGHT,
  CLUTTER_COLOR_BUTTER_DARK,
  CLUTTER_COLOR_ORANGE,
  CLUTTER_COLOR_ORANGE_LIGHT,
  CLUTTER_COLOR_ORANGE_DARK,
  CLUTTER_COLOR_CHOCOLATE,
  CLUTTER_COLOR_CHOCOLATE_LIGHT,
  CLUTTER_COLOR_CHOCOLATE_DARK,
  CLUTTER_COLOR_CHAMELEON,
  CLUTTER_COLOR_CHAMELEON_LIGHT,
  CLUTTER_COLOR_CHAMELEON_DARK,
  CLUTTER_COLOR_SKY_BLUE,
  CLUTTER_COLOR_SKY_BLUE_LIGHT,
  CLUTTER_COLOR_SKY_BLUE_DARK,
  CLUTTER_COLOR_PLUM,
  CLUTTER_COLOR_PLUM_LIGHT,
  CLUTTER_COLOR_PLUM_DARK,
  CLUTTER_COLOR_SCARLET_RED,
  CLUTTER_COLOR_SCARLET_RED_LIGHT,
  CLUTTER_COLOR_SCARLET_RED_DARK,
  CLUTTER_COLOR_ALUMINIUM_1,
  CLUTTER_COLOR_ALUMINIUM_2,
  CLUTTER_COLOR_ALUMINIUM_3,
  CLUTTER_COLOR_ALUMINIUM_4,
  CLUTTER_COLOR_ALUMINIUM_5,
  CLUTTER_COLOR_ALUMINIUM_6,

  /* Fully transparent black */
  CLUTTER_COLOR_TRANSPARENT
} ClutterStaticColor;

const ClutterColor *clutter_color_get_static (ClutterStaticColor color);

G_END_DECLS

#endif /* __CLUTTER_COLOR_H__ */
