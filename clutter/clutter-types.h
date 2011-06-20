/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

#ifndef __CLUTTER_TYPES_H__
#define __CLUTTER_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ACTOR_BOX          (clutter_actor_box_get_type ())
#define CLUTTER_TYPE_GEOMETRY           (clutter_geometry_get_type ())
#define CLUTTER_TYPE_KNOT               (clutter_knot_get_type ())
#define CLUTTER_TYPE_PAINT_VOLUME       (clutter_paint_volume_get_type ())
#define CLUTTER_TYPE_VERTEX             (clutter_vertex_get_type ())

/* Forward delarations to avoid header catch 22's */
typedef struct _ClutterActor            ClutterActor;

typedef struct _ClutterStage            ClutterStage;
typedef struct _ClutterContainer        ClutterContainer; /* dummy */
typedef struct _ClutterChildMeta        ClutterChildMeta;
typedef struct _ClutterLayoutMeta       ClutterLayoutMeta;
typedef struct _ClutterActorMeta        ClutterActorMeta;

typedef struct _ClutterAnimator         ClutterAnimator;

typedef struct _ClutterAction           ClutterAction;
typedef struct _ClutterConstraint       ClutterConstraint;
typedef struct _ClutterEffect           ClutterEffect;

#if !defined(CLUTTER_DISABLE_DEPRECATED) || defined(CLUTTER_COMPILATION)
typedef struct _ClutterShader           ClutterShader;
#endif

typedef struct _ClutterColor            ClutterColor;

typedef union _ClutterEvent             ClutterEvent;

/**
 * ClutterGravity:
 * @CLUTTER_GRAVITY_NONE: Do not apply any gravity
 * @CLUTTER_GRAVITY_NORTH: Scale from topmost downwards
 * @CLUTTER_GRAVITY_NORTH_EAST: Scale from the top right corner
 * @CLUTTER_GRAVITY_EAST: Scale from the right side
 * @CLUTTER_GRAVITY_SOUTH_EAST: Scale from the bottom right corner
 * @CLUTTER_GRAVITY_SOUTH: Scale from the bottom upwards
 * @CLUTTER_GRAVITY_SOUTH_WEST: Scale from the bottom left corner
 * @CLUTTER_GRAVITY_WEST: Scale from the left side
 * @CLUTTER_GRAVITY_NORTH_WEST: Scale from the top left corner
 * @CLUTTER_GRAVITY_CENTER: Scale from the center.
 *
 * Gravity of the scaling operations. When a gravity different than
 * %CLUTTER_GRAVITY_NONE is used, an actor is scaled keeping the position
 * of the specified portion at the same coordinates.
 *
 * Since: 0.2
 */
typedef enum { /*< prefix=CLUTTER_GRAVITY >*/
  CLUTTER_GRAVITY_NONE       = 0,
  CLUTTER_GRAVITY_NORTH,
  CLUTTER_GRAVITY_NORTH_EAST,
  CLUTTER_GRAVITY_EAST,
  CLUTTER_GRAVITY_SOUTH_EAST,
  CLUTTER_GRAVITY_SOUTH,
  CLUTTER_GRAVITY_SOUTH_WEST,
  CLUTTER_GRAVITY_WEST,
  CLUTTER_GRAVITY_NORTH_WEST,
  CLUTTER_GRAVITY_CENTER
} ClutterGravity;

typedef struct _ClutterActorBox         ClutterActorBox;
typedef struct _ClutterGeometry         ClutterGeometry;
typedef struct _ClutterKnot             ClutterKnot;
typedef struct _ClutterVertex           ClutterVertex;

/**
 * ClutterPaintVolume:
 *
 * <structname>ClutterPaintVolume</structname> is an opaque structure
 * whose members cannot be directly accessed.
 *
 * A <structname>ClutterPaintVolume</structname> represents an
 * a bounding volume whos internal representation isn't defined but
 * can be set and queried in terms of an axis aligned bounding box.
 *
 * Other internal representation and methods for describing the
 * bounding volume may be added in the future.
 *
 * Since: 1.4
 */
typedef struct _ClutterPaintVolume      ClutterPaintVolume;

/**
 * ClutterVertex:
 * @x: X coordinate of the vertex
 * @y: Y coordinate of the vertex
 * @z: Z coordinate of the vertex
 *
 * Vertex of an actor in 3D space, expressed in pixels
 *
 * Since: 0.4
 */
struct _ClutterVertex
{
  gfloat x;
  gfloat y;
  gfloat z;
};

GType          clutter_vertex_get_type (void) G_GNUC_CONST;
ClutterVertex *clutter_vertex_new      (gfloat               x,
                                        gfloat               y,
                                        gfloat               z);
ClutterVertex *clutter_vertex_copy     (const ClutterVertex *vertex);
void           clutter_vertex_free     (ClutterVertex       *vertex);
gboolean       clutter_vertex_equal    (const ClutterVertex *vertex_a,
                                        const ClutterVertex *vertex_b);

/**
 * ClutterActorBox:
 * @x1: X coordinate of the top left corner
 * @y1: Y coordinate of the top left corner
 * @x2: X coordinate of the bottom right corner
 * @y2: Y coordinate of the bottom right corner
 *
 * Bounding box of an actor. The coordinates of the top left and right bottom
 * corners of an actor. The coordinates of the two points are expressed in
 * pixels with sub-pixel precision
 */
struct _ClutterActorBox
{
  gfloat x1;
  gfloat y1;

  gfloat x2;
  gfloat y2;
};

GType            clutter_actor_box_get_type      (void) G_GNUC_CONST;
ClutterActorBox *clutter_actor_box_new           (gfloat                 x_1,
                                                  gfloat                 y_1,
                                                  gfloat                 x_2,
                                                  gfloat                 y_2);
ClutterActorBox *clutter_actor_box_copy          (const ClutterActorBox *box);
void             clutter_actor_box_free          (ClutterActorBox       *box);
gboolean         clutter_actor_box_equal         (const ClutterActorBox *box_a,
                                                  const ClutterActorBox *box_b);
gfloat           clutter_actor_box_get_x         (const ClutterActorBox *box);
gfloat           clutter_actor_box_get_y         (const ClutterActorBox *box);
gfloat           clutter_actor_box_get_width     (const ClutterActorBox *box);
gfloat           clutter_actor_box_get_height    (const ClutterActorBox *box);
void             clutter_actor_box_get_origin    (const ClutterActorBox *box,
                                                  gfloat                *x,
                                                  gfloat                *y);
void             clutter_actor_box_get_size      (const ClutterActorBox *box,
                                                  gfloat                *width,
                                                  gfloat                *height);
gfloat           clutter_actor_box_get_area      (const ClutterActorBox *box);
gboolean         clutter_actor_box_contains      (const ClutterActorBox *box,
                                                  gfloat                 x,
                                                  gfloat                 y);
void             clutter_actor_box_from_vertices (ClutterActorBox       *box,
                                                  const ClutterVertex    verts[]);
void             clutter_actor_box_interpolate   (const ClutterActorBox *initial,
                                                  const ClutterActorBox *final,
                                                  gdouble                progress,
                                                  ClutterActorBox       *result);
void             clutter_actor_box_clamp_to_pixel (ClutterActorBox       *box);
void             clutter_actor_box_union          (const ClutterActorBox *a,
                                                   const ClutterActorBox *b,
                                                   ClutterActorBox       *result);

void             clutter_actor_box_set_origin     (ClutterActorBox       *box,
                                                   gfloat                 x,
                                                   gfloat                 y);
void             clutter_actor_box_set_size       (ClutterActorBox       *box,
                                                   gfloat                 width,
                                                   gfloat                 height);

/**
 * ClutterGeometry:
 * @x: X coordinate of the top left corner of an actor
 * @y: Y coordinate of the top left corner of an actor
 * @width: width of an actor
 * @height: height of an actor
 *
 * The rectangle containing an actor's bounding box, measured in pixels.
 */
struct _ClutterGeometry
{
  /*< public >*/
  gint   x;
  gint   y;
  guint  width;
  guint  height;
};

GType clutter_geometry_get_type (void) G_GNUC_CONST;

void      clutter_geometry_union      (const ClutterGeometry *geometry_a,
                                       const ClutterGeometry *geometry_b,
                                       ClutterGeometry       *result);
gboolean  clutter_geometry_intersects (const ClutterGeometry *geometry0,
                                       const ClutterGeometry *geometry1);

/**
 * ClutterKnot:
 * @x: X coordinate of the knot
 * @y: Y coordinate of the knot
 *
 * Point in a path behaviour.
 *
 * Since: 0.2
 */
struct _ClutterKnot
{
  gint x;
  gint y;
};

GType        clutter_knot_get_type (void) G_GNUC_CONST;
ClutterKnot *clutter_knot_copy     (const ClutterKnot *knot);
void         clutter_knot_free     (ClutterKnot       *knot);
gboolean     clutter_knot_equal    (const ClutterKnot *knot_a,
                                    const ClutterKnot *knot_b);

/**
 * ClutterRotateAxis:
 * @CLUTTER_X_AXIS: Rotate around the X axis
 * @CLUTTER_Y_AXIS: Rotate around the Y axis
 * @CLUTTER_Z_AXIS: Rotate around the Z axis
 *
 * Axis of a rotation.
 *
 * Since: 0.4
 */
typedef enum { /*< prefix=CLUTTER >*/
  CLUTTER_X_AXIS,
  CLUTTER_Y_AXIS,
  CLUTTER_Z_AXIS
} ClutterRotateAxis;

/**
 * ClutterRotateDirection:
 * @CLUTTER_ROTATE_CW: Clockwise rotation
 * @CLUTTER_ROTATE_CCW: Counter-clockwise rotation
 *
 * Direction of a rotation.
 *
 * Since: 0.4
 */
typedef enum { /*< prefix=CLUTTER_ROTATE >*/
  CLUTTER_ROTATE_CW,
  CLUTTER_ROTATE_CCW
} ClutterRotateDirection;

/**
 * ClutterRequestMode:
 * @CLUTTER_REQUEST_HEIGHT_FOR_WIDTH: Height for width requests
 * @CLUTTER_REQUEST_WIDTH_FOR_HEIGHT: Width for height requests
 *
 * Specifies the type of requests for a #ClutterActor.
 *
 * Since: 0.8
 */
typedef enum {
  CLUTTER_REQUEST_HEIGHT_FOR_WIDTH,
  CLUTTER_REQUEST_WIDTH_FOR_HEIGHT
} ClutterRequestMode;

/**
 * ClutterAnimationMode:
 * @CLUTTER_CUSTOM_MODE: custom progress function
 * @CLUTTER_LINEAR: linear tweening
 * @CLUTTER_EASE_IN_QUAD: quadratic tweening
 * @CLUTTER_EASE_OUT_QUAD: quadratic tweening, inverse of
 *    %CLUTTER_EASE_IN_QUAD
 * @CLUTTER_EASE_IN_OUT_QUAD: quadratic tweening, combininig
 *    %CLUTTER_EASE_IN_QUAD and %CLUTTER_EASE_OUT_QUAD
 * @CLUTTER_EASE_IN_CUBIC: cubic tweening
 * @CLUTTER_EASE_OUT_CUBIC: cubic tweening, invers of
 *    %CLUTTER_EASE_IN_CUBIC
 * @CLUTTER_EASE_IN_OUT_CUBIC: cubic tweening, combining
 *    %CLUTTER_EASE_IN_CUBIC and %CLUTTER_EASE_OUT_CUBIC
 * @CLUTTER_EASE_IN_QUART: quartic tweening
 * @CLUTTER_EASE_OUT_QUART: quartic tweening, inverse of
 *    %CLUTTER_EASE_IN_QUART
 * @CLUTTER_EASE_IN_OUT_QUART: quartic tweening, combining
 *    %CLUTTER_EASE_IN_QUART and %CLUTTER_EASE_OUT_QUART
 * @CLUTTER_EASE_IN_QUINT: quintic tweening
 * @CLUTTER_EASE_OUT_QUINT: quintic tweening, inverse of
 *    %CLUTTER_EASE_IN_QUINT
 * @CLUTTER_EASE_IN_OUT_QUINT: fifth power tweening, combining
 *    %CLUTTER_EASE_IN_QUINT and %CLUTTER_EASE_OUT_QUINT
 * @CLUTTER_EASE_IN_SINE: sinusoidal tweening
 * @CLUTTER_EASE_OUT_SINE: sinusoidal tweening, inverse of
 *    %CLUTTER_EASE_IN_SINE
 * @CLUTTER_EASE_IN_OUT_SINE: sine wave tweening, combining
 *    %CLUTTER_EASE_IN_SINE and %CLUTTER_EASE_OUT_SINE
 * @CLUTTER_EASE_IN_EXPO: exponential tweening
 * @CLUTTER_EASE_OUT_EXPO: exponential tweening, inverse of
 *    %CLUTTER_EASE_IN_EXPO
 * @CLUTTER_EASE_IN_OUT_EXPO: exponential tweening, combining
 *    %CLUTTER_EASE_IN_EXPO and %CLUTTER_EASE_OUT_EXPO
 * @CLUTTER_EASE_IN_CIRC: circular tweening
 * @CLUTTER_EASE_OUT_CIRC: circular tweening, inverse of
 *    %CLUTTER_EASE_IN_CIRC
 * @CLUTTER_EASE_IN_OUT_CIRC: circular tweening, combining
 *    %CLUTTER_EASE_IN_CIRC and %CLUTTER_EASE_OUT_CIRC
 * @CLUTTER_EASE_IN_ELASTIC: elastic tweening, with offshoot on start
 * @CLUTTER_EASE_OUT_ELASTIC: elastic tweening, with offshoot on end
 * @CLUTTER_EASE_IN_OUT_ELASTIC: elastic tweening with offshoot on both ends
 * @CLUTTER_EASE_IN_BACK: overshooting cubic tweening, with
 *   backtracking on start
 * @CLUTTER_EASE_OUT_BACK: overshooting cubic tweening, with
 *   backtracking on end
 * @CLUTTER_EASE_IN_OUT_BACK: overshooting cubic tweening, with
 *   backtracking on both ends
 * @CLUTTER_EASE_IN_BOUNCE: exponentially decaying parabolic (bounce)
 *   tweening, with bounce on start
 * @CLUTTER_EASE_OUT_BOUNCE: exponentially decaying parabolic (bounce)
 *   tweening, with bounce on end
 * @CLUTTER_EASE_IN_OUT_BOUNCE: exponentially decaying parabolic (bounce)
 *   tweening, with bounce on both ends
 * @CLUTTER_ANIMATION_LAST: last animation mode, used as a guard for
 *   registered global alpha functions
 *
 * The animation modes used by #ClutterAlpha and #ClutterAnimation. This
 * enumeration can be expanded in later versions of Clutter. See the
 * #ClutterAlpha documentation for a graph of all the animation modes.
 *
 * Every global alpha function registered using clutter_alpha_register_func()
 * or clutter_alpha_register_closure() will have a logical id greater than
 * %CLUTTER_ANIMATION_LAST.
 *
 * Since: 1.0
 */
typedef enum {
  CLUTTER_CUSTOM_MODE = 0,

  /* linear */
  CLUTTER_LINEAR,

  /* quadratic */
  CLUTTER_EASE_IN_QUAD,
  CLUTTER_EASE_OUT_QUAD,
  CLUTTER_EASE_IN_OUT_QUAD,

  /* cubic */
  CLUTTER_EASE_IN_CUBIC,
  CLUTTER_EASE_OUT_CUBIC,
  CLUTTER_EASE_IN_OUT_CUBIC,

  /* quartic */
  CLUTTER_EASE_IN_QUART,
  CLUTTER_EASE_OUT_QUART,
  CLUTTER_EASE_IN_OUT_QUART,

  /* quintic */
  CLUTTER_EASE_IN_QUINT,
  CLUTTER_EASE_OUT_QUINT,
  CLUTTER_EASE_IN_OUT_QUINT,

  /* sinusoidal */
  CLUTTER_EASE_IN_SINE,
  CLUTTER_EASE_OUT_SINE,
  CLUTTER_EASE_IN_OUT_SINE,

  /* exponential */
  CLUTTER_EASE_IN_EXPO,
  CLUTTER_EASE_OUT_EXPO,
  CLUTTER_EASE_IN_OUT_EXPO,

  /* circular */
  CLUTTER_EASE_IN_CIRC,
  CLUTTER_EASE_OUT_CIRC,
  CLUTTER_EASE_IN_OUT_CIRC,

  /* elastic */
  CLUTTER_EASE_IN_ELASTIC,
  CLUTTER_EASE_OUT_ELASTIC,
  CLUTTER_EASE_IN_OUT_ELASTIC,

  /* overshooting cubic */
  CLUTTER_EASE_IN_BACK,
  CLUTTER_EASE_OUT_BACK,
  CLUTTER_EASE_IN_OUT_BACK,

  /* exponentially decaying parabolic */
  CLUTTER_EASE_IN_BOUNCE,
  CLUTTER_EASE_OUT_BOUNCE,
  CLUTTER_EASE_IN_OUT_BOUNCE,

  /* guard, before registered alpha functions */
  CLUTTER_ANIMATION_LAST
} ClutterAnimationMode;

/**
 * ClutterFontFlags:
 * @CLUTTER_FONT_MIPMAPPING: Set to use mipmaps for the glyph cache textures.
 * @CLUTTER_FONT_HINTING: Set to enable hinting on the glyphs.
 *
 * Runtime flags to change the font quality. To be used with
 * clutter_set_font_flags().
 *
 * Since: 1.0
 */
typedef enum
{
  CLUTTER_FONT_MIPMAPPING = (1 << 0),
  CLUTTER_FONT_HINTING    = (1 << 1)
} ClutterFontFlags;

/**
 * ClutterTextDirection:
 * @CLUTTER_TEXT_DIRECTION_DEFAULT: Use the default setting, as returned
 *   by clutter_get_default_text_direction()
 * @CLUTTER_TEXT_DIRECTION_LTR: Use left-to-right text direction
 * @CLUTTER_TEXT_DIRECTION_RTL: Use right-to-left text direction
 *
 * The text direction to be used by #ClutterActor<!-- -->s
 *
 * Since: 1.2
 */
typedef enum {
  CLUTTER_TEXT_DIRECTION_DEFAULT,
  CLUTTER_TEXT_DIRECTION_LTR,
  CLUTTER_TEXT_DIRECTION_RTL
} ClutterTextDirection;

/**
 * ClutterShaderType:
 * @CLUTTER_VERTEX_SHADER: a vertex shader
 * @CLUTTER_FRAGMENT_SHADER: a fragment shader
 *
 * The type of GLSL shader program
 *
 * Since: 1.4
 */
typedef enum {
  CLUTTER_VERTEX_SHADER,
  CLUTTER_FRAGMENT_SHADER
} ClutterShaderType;

GType clutter_paint_volume_get_type (void) G_GNUC_CONST;

ClutterPaintVolume *clutter_paint_volume_copy                (const ClutterPaintVolume *pv);
void                clutter_paint_volume_free                (ClutterPaintVolume       *pv);

void                clutter_paint_volume_set_origin          (ClutterPaintVolume       *pv,
                                                              const ClutterVertex      *origin);
void                clutter_paint_volume_get_origin          (const ClutterPaintVolume *pv,
                                                              ClutterVertex            *vertex);
void                clutter_paint_volume_set_width           (ClutterPaintVolume       *pv,
                                                              gfloat                    width);
gfloat              clutter_paint_volume_get_width           (const ClutterPaintVolume *pv);
void                clutter_paint_volume_set_height          (ClutterPaintVolume       *pv,
                                                              gfloat                    height);
gfloat              clutter_paint_volume_get_height          (const ClutterPaintVolume *pv);
void                clutter_paint_volume_set_depth           (ClutterPaintVolume       *pv,
                                                              gfloat                    depth);
gfloat              clutter_paint_volume_get_depth           (const ClutterPaintVolume *pv);
void                clutter_paint_volume_union               (ClutterPaintVolume       *pv,
                                                              const ClutterPaintVolume *another_pv);

gboolean            clutter_paint_volume_set_from_allocation (ClutterPaintVolume       *pv,
                                                              ClutterActor             *actor);

/**
 * ClutterModifierType:
 * @CLUTTER_SHIFT_MASK: Mask applied by the Shift key
 * @CLUTTER_LOCK_MASK: Mask applied by the Caps Lock key
 * @CLUTTER_CONTROL_MASK: Mask applied by the Control key
 * @CLUTTER_MOD1_MASK: Mask applied by the first Mod key
 * @CLUTTER_MOD2_MASK: Mask applied by the second Mod key
 * @CLUTTER_MOD3_MASK: Mask applied by the third Mod key
 * @CLUTTER_MOD4_MASK: Mask applied by the fourth Mod key
 * @CLUTTER_MOD5_MASK: Mask applied by the fifth Mod key
 * @CLUTTER_BUTTON1_MASK: Mask applied by the first pointer button
 * @CLUTTER_BUTTON2_MASK: Mask applied by the second pointer button
 * @CLUTTER_BUTTON3_MASK: Mask applied by the third pointer button
 * @CLUTTER_BUTTON4_MASK: Mask applied by the fourth pointer button
 * @CLUTTER_BUTTON5_MASK: Mask applied by the fifth pointer button
 * @CLUTTER_SUPER_MASK: Mask applied by the Super key
 * @CLUTTER_HYPER_MASK: Mask applied by the Hyper key
 * @CLUTTER_META_MASK: Mask applied by the Meta key
 * @CLUTTER_RELEASE_MASK: Mask applied during release
 * @CLUTTER_MODIFIER_MASK: A mask covering all modifier types
 *
 * Masks applied to a #ClutterEvent by modifiers.
 *
 * Note that Clutter may add internal values to events which include
 * reserved values such as %CLUTTER_MODIFIER_RESERVED_13_MASK.  Your code
 * should preserve and ignore them.  You can use %CLUTTER_MODIFIER_MASK to
 * remove all reserved values.
 *
 * Since: 0.4
 */
typedef enum {
  CLUTTER_SHIFT_MASK    = 1 << 0,
  CLUTTER_LOCK_MASK     = 1 << 1,
  CLUTTER_CONTROL_MASK  = 1 << 2,
  CLUTTER_MOD1_MASK     = 1 << 3,
  CLUTTER_MOD2_MASK     = 1 << 4,
  CLUTTER_MOD3_MASK     = 1 << 5,
  CLUTTER_MOD4_MASK     = 1 << 6,
  CLUTTER_MOD5_MASK     = 1 << 7,
  CLUTTER_BUTTON1_MASK  = 1 << 8,
  CLUTTER_BUTTON2_MASK  = 1 << 9,
  CLUTTER_BUTTON3_MASK  = 1 << 10,
  CLUTTER_BUTTON4_MASK  = 1 << 11,
  CLUTTER_BUTTON5_MASK  = 1 << 12,

#ifndef __GTK_DOC_IGNORE__
  CLUTTER_MODIFIER_RESERVED_13_MASK  = 1 << 13,
  CLUTTER_MODIFIER_RESERVED_14_MASK  = 1 << 14,
  CLUTTER_MODIFIER_RESERVED_15_MASK  = 1 << 15,
  CLUTTER_MODIFIER_RESERVED_16_MASK  = 1 << 16,
  CLUTTER_MODIFIER_RESERVED_17_MASK  = 1 << 17,
  CLUTTER_MODIFIER_RESERVED_18_MASK  = 1 << 18,
  CLUTTER_MODIFIER_RESERVED_19_MASK  = 1 << 19,
  CLUTTER_MODIFIER_RESERVED_20_MASK  = 1 << 20,
  CLUTTER_MODIFIER_RESERVED_21_MASK  = 1 << 21,
  CLUTTER_MODIFIER_RESERVED_22_MASK  = 1 << 22,
  CLUTTER_MODIFIER_RESERVED_23_MASK  = 1 << 23,
  CLUTTER_MODIFIER_RESERVED_24_MASK  = 1 << 24,
  CLUTTER_MODIFIER_RESERVED_25_MASK  = 1 << 25,
#endif

  CLUTTER_SUPER_MASK    = 1 << 26,
  CLUTTER_HYPER_MASK    = 1 << 27,
  CLUTTER_META_MASK     = 1 << 28,

#ifndef __GTK_DOC_IGNORE__
  CLUTTER_MODIFIER_RESERVED_29_MASK  = 1 << 29,
#endif

  CLUTTER_RELEASE_MASK  = 1 << 30,

  /* Combination of CLUTTER_SHIFT_MASK..CLUTTER_BUTTON5_MASK + CLUTTER_SUPER_MASK
     + CLUTTER_HYPER_MASK + CLUTTER_META_MASK + CLUTTER_RELEASE_MASK */
  CLUTTER_MODIFIER_MASK = 0x5c001fff
} ClutterModifierType;

G_END_DECLS

#endif /* __CLUTTER_TYPES_H__ */
