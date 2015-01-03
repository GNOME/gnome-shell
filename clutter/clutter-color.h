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

/**
 * CLUTTER_COLOR_INIT:
 * @r: value for the red channel, between 0 and 255
 * @g: value for the green channel, between 0 and 255
 * @b: value for the blue channel, between 0 and 255
 * @a: value for the alpha channel, between 0 and 255
 *
 * A macro that initializes a #ClutterColor, to be used when declaring it.
 *
 * Since: 1.12
 */
#define CLUTTER_COLOR_INIT(r,g,b,a)     { (r), (g), (b), (a) }

CLUTTER_AVAILABLE_IN_ALL
GType clutter_color_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_ALL
ClutterColor *clutter_color_new         (guint8              red,
                                         guint8              green,
                                         guint8              blue,
                                         guint8              alpha);
CLUTTER_AVAILABLE_IN_1_12
ClutterColor *clutter_color_alloc       (void);
CLUTTER_AVAILABLE_IN_1_12
ClutterColor *clutter_color_init        (ClutterColor       *color,
                                         guint8              red,
                                         guint8              green,
                                         guint8              blue,
                                         guint8              alpha);
CLUTTER_AVAILABLE_IN_ALL
ClutterColor *clutter_color_copy        (const ClutterColor *color);
CLUTTER_AVAILABLE_IN_ALL
void          clutter_color_free        (ClutterColor       *color);

CLUTTER_AVAILABLE_IN_ALL
void          clutter_color_add         (const ClutterColor *a,
                                         const ClutterColor *b,
                                         ClutterColor       *result);
CLUTTER_AVAILABLE_IN_ALL
void          clutter_color_subtract    (const ClutterColor *a,
                                         const ClutterColor *b,
                                         ClutterColor       *result);
CLUTTER_AVAILABLE_IN_ALL
void          clutter_color_lighten     (const ClutterColor *color,
                                         ClutterColor       *result);
CLUTTER_AVAILABLE_IN_ALL
void          clutter_color_darken      (const ClutterColor *color,
                                         ClutterColor       *result);
CLUTTER_AVAILABLE_IN_ALL
void          clutter_color_shade       (const ClutterColor *color,
                                         gdouble             factor,
                                         ClutterColor       *result);

CLUTTER_AVAILABLE_IN_ALL
gchar *       clutter_color_to_string   (const ClutterColor *color);
CLUTTER_AVAILABLE_IN_1_0
gboolean      clutter_color_from_string (ClutterColor       *color,
                                         const gchar        *str);

CLUTTER_AVAILABLE_IN_ALL
void          clutter_color_to_hls      (const ClutterColor *color,
                                         gfloat             *hue,
					 gfloat             *luminance,
					 gfloat             *saturation);
CLUTTER_AVAILABLE_IN_ALL
void          clutter_color_from_hls    (ClutterColor       *color,
                                         gfloat              hue,
                                         gfloat              luminance,
                                         gfloat              saturation);

CLUTTER_AVAILABLE_IN_ALL
guint32       clutter_color_to_pixel    (const ClutterColor *color);
CLUTTER_AVAILABLE_IN_ALL
void          clutter_color_from_pixel  (ClutterColor       *color,
                                         guint32             pixel);

CLUTTER_AVAILABLE_IN_1_0
guint         clutter_color_hash        (gconstpointer       v);
CLUTTER_AVAILABLE_IN_ALL
gboolean      clutter_color_equal       (gconstpointer       v1,
                                         gconstpointer       v2);

CLUTTER_AVAILABLE_IN_1_6
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

CLUTTER_AVAILABLE_IN_1_0
void                    clutter_value_set_color         (GValue             *value,
                                                         const ClutterColor *color);
CLUTTER_AVAILABLE_IN_1_0
const ClutterColor *    clutter_value_get_color         (const GValue       *value);

CLUTTER_AVAILABLE_IN_1_0
GType clutter_param_color_get_type (void) G_GNUC_CONST;
CLUTTER_AVAILABLE_IN_1_0
GParamSpec *    clutter_param_spec_color        (const gchar        *name,
                                                 const gchar        *nick,
                                                 const gchar        *blurb,
                                                 const ClutterColor *default_value,
                                                 GParamFlags         flags);

CLUTTER_AVAILABLE_IN_1_6
const ClutterColor *clutter_color_get_static (ClutterStaticColor color);

G_END_DECLS

#endif /* __CLUTTER_COLOR_H__ */
