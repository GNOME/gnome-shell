/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011  Intel Corporation
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

#ifndef __CLUTTER_ENUMS_H__
#define __CLUTTER_ENUMS_H__

#include <glib-object.h>

G_BEGIN_DECLS

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
typedef enum { /*< prefix=CLUTTER_REQUEST >*/
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
 * @CLUTTER_STEPS: parametrized step function; see clutter_timeline_set_step_progress()
 *   for further details. (Since 1.12)
 * @CLUTTER_STEP_START: equivalent to %CLUTTER_STEPS with a number of steps
 *   equal to 1, and a step mode of %CLUTTER_STEP_MODE_START. (Since 1.12)
 * @CLUTTER_STEP_END: equivalent to %CLUTTER_STEPS with a number of steps
 *   equal to 1, and a step mode of %CLUTTER_STEP_MODE_END. (Since 1.12)
 * @CLUTTER_CUBIC_BEZIER: cubic bezier between (0, 0) and (1, 1) with two
 *   control points; see clutter_timeline_set_cubic_bezier_progress(). (Since 1.12)
 * @CLUTTER_EASE: equivalent to %CLUTTER_CUBIC_BEZIER with control points
 *   in (0.25, 0.1) and (0.25, 1.0). (Since 1.12)
 * @CLUTTER_EASE_IN: equivalent to %CLUTTER_CUBIC_BEZIER with control points
 *   in (0.42, 0) and (1.0, 1.0). (Since 1.12)
 * @CLUTTER_EASE_OUT: equivalent to %CLUTTER_CUBIC_BEZIER with control points
 *   in (0, 0) and (0.58, 1.0). (Since 1.12)
 * @CLUTTER_EASE_IN_OUT: equivalent to %CLUTTER_CUBIC_BEZIER with control points
 *   in (0.42, 0) and (0.58, 1.0). (Since 1.12)
 * @CLUTTER_ANIMATION_LAST: last animation mode, used as a guard for
 *   registered global alpha functions
 *
 * The animation modes used by #ClutterAlpha and #ClutterAnimation. This
 * enumeration can be expanded in later versions of Clutter.
 *
 * <figure id="easing-modes">
 *   <title>Easing modes provided by Clutter</title>
 *   <graphic fileref="easing-modes.png" format="PNG"/>
 * </figure>
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

  /* step functions (see css3-transitions) */
  CLUTTER_STEPS,
  CLUTTER_STEP_START, /* steps(1, start) */
  CLUTTER_STEP_END, /* steps(1, end) */

  /* cubic bezier (see css3-transitions) */
  CLUTTER_CUBIC_BEZIER,
  CLUTTER_EASE,
  CLUTTER_EASE_IN,
  CLUTTER_EASE_OUT,
  CLUTTER_EASE_IN_OUT,

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
typedef enum { /*< prefix=CLUTTER_FONT >*/
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

/**
 * ClutterActorFlags:
 * @CLUTTER_ACTOR_MAPPED: the actor will be painted (is visible, and inside
 *   a toplevel, and all parents visible)
 * @CLUTTER_ACTOR_REALIZED: the resources associated to the actor have been
 *   allocated
 * @CLUTTER_ACTOR_REACTIVE: the actor 'reacts' to mouse events emmitting event
 *   signals
 * @CLUTTER_ACTOR_VISIBLE: the actor has been shown by the application program
 * @CLUTTER_ACTOR_NO_LAYOUT: the actor provides an explicit layout management
 *   policy for its children; this flag will prevent Clutter from automatic
 *   queueing of relayout and will defer all layouting to the actor itself
 *
 * Flags used to signal the state of an actor.
 */
typedef enum { /*< prefix=CLUTTER_ACTOR >*/
  CLUTTER_ACTOR_MAPPED    = 1 << 1,
  CLUTTER_ACTOR_REALIZED  = 1 << 2,
  CLUTTER_ACTOR_REACTIVE  = 1 << 3,
  CLUTTER_ACTOR_VISIBLE   = 1 << 4,
  CLUTTER_ACTOR_NO_LAYOUT = 1 << 5
} ClutterActorFlags;

/**
 * ClutterOffscreenRedirect:
 * @CLUTTER_OFFSCREEN_REDIRECT_AUTOMATIC_FOR_OPACITY: Only redirect
 *   the actor if it is semi-transparent and its has_overlaps()
 *   virtual returns %TRUE. This is the default.
 * @CLUTTER_OFFSCREEN_REDIRECT_ALWAYS: Always redirect the actor to an
 *   offscreen buffer even if it is fully opaque.
 *
 * Possible flags to pass to clutter_actor_set_offscreen_redirect().
 *
 * Since: 1.8
 */
typedef enum { /*< prefix=CLUTTER_OFFSCREEN_REDIRECT >*/
  CLUTTER_OFFSCREEN_REDIRECT_AUTOMATIC_FOR_OPACITY = 1<<0,
  CLUTTER_OFFSCREEN_REDIRECT_ALWAYS = 1<<1
} ClutterOffscreenRedirect;

/**
 * ClutterAllocationFlags:
 * @CLUTTER_ALLOCATION_NONE: No flag set
 * @CLUTTER_ABSOLUTE_ORIGIN_CHANGED: Whether the absolute origin of the
 *   actor has changed; this implies that any ancestor of the actor has
 *   been moved.
 * @CLUTTER_DELEGATE_LAYOUT: Whether the allocation should be delegated
 *   to the #ClutterLayoutManager instance stored inside the
 *   #ClutterActor:layout-manager property of #ClutterActor. This flag
 *   should only be used if you are subclassing #ClutterActor and
 *   overriding the #ClutterActorClass.allocate() virtual function, but
 *   you wish to use the default implementation of the virtual function
 *   inside #ClutterActor. Added in Clutter 1.10.
 *
 * Flags passed to the #ClutterActorClass.allocate() virtual function
 * and to the clutter_actor_allocate() function.
 *
 * Since: 1.0
 */
typedef enum {
  CLUTTER_ALLOCATION_NONE         = 0,
  CLUTTER_ABSOLUTE_ORIGIN_CHANGED = 1 << 1,
  CLUTTER_DELEGATE_LAYOUT         = 1 << 2
} ClutterAllocationFlags;

/**
 * ClutterAlignAxis:
 * @CLUTTER_ALIGN_X_AXIS: Maintain the alignment on the X axis
 * @CLUTTER_ALIGN_Y_AXIS: Maintain the alignment on the Y axis
 * @CLUTTER_ALIGN_BOTH: Maintain the alignment on both the X and Y axis
 *
 * Specifies the axis on which #ClutterAlignConstraint should maintain
 * the alignment.
 *
 * Since: 1.4
 */
typedef enum { /*< prefix=CLUTTER_ALIGN >*/
  CLUTTER_ALIGN_X_AXIS,
  CLUTTER_ALIGN_Y_AXIS,
  CLUTTER_ALIGN_BOTH
} ClutterAlignAxis;

/**
 * ClutterInterpolation:
 * @CLUTTER_INTERPOLATION_LINEAR: linear interpolation
 * @CLUTTER_INTERPOLATION_CUBIC: cubic interpolation
 *
 * The mode of interpolation between key frames
 *
 * Since: 1.2
 */
typedef enum {
  CLUTTER_INTERPOLATION_LINEAR,
  CLUTTER_INTERPOLATION_CUBIC
} ClutterInterpolation;

/**
 * ClutterBinAlignment:
 * @CLUTTER_BIN_ALIGNMENT_FIXED: Fixed position alignment; the
 *   #ClutterBinLayout will honour the fixed position provided
 *   by the actors themselves when allocating them
 * @CLUTTER_BIN_ALIGNMENT_FILL: Fill the allocation size
 * @CLUTTER_BIN_ALIGNMENT_START: Position the actors at the top
 *   or left side of the container, depending on the axis
 * @CLUTTER_BIN_ALIGNMENT_END: Position the actors at the bottom
 *   or right side of the container, depending on the axis
 * @CLUTTER_BIN_ALIGNMENT_CENTER: Position the actors at the
 *   center of the container, depending on the axis
 *
 * The alignment policies available on each axis for #ClutterBinLayout
 *
 * Since: 1.2
 *
 * Deprecated: 1.12: Use #ClutterActorAlign and the #ClutterActor
 *   API instead
 */
typedef enum {
  CLUTTER_BIN_ALIGNMENT_FIXED,
  CLUTTER_BIN_ALIGNMENT_FILL,
  CLUTTER_BIN_ALIGNMENT_START,
  CLUTTER_BIN_ALIGNMENT_END,
  CLUTTER_BIN_ALIGNMENT_CENTER
} ClutterBinAlignment;

/**
 * ClutterBindCoordinate:
 * @CLUTTER_BIND_X: Bind the X coordinate
 * @CLUTTER_BIND_Y: Bind the Y coordinate
 * @CLUTTER_BIND_WIDTH: Bind the width
 * @CLUTTER_BIND_HEIGHT: Bind the height
 * @CLUTTER_BIND_POSITION: Equivalent to to %CLUTTER_BIND_X and
 *   %CLUTTER_BIND_Y (added in Clutter 1.6)
 * @CLUTTER_BIND_SIZE: Equivalent to %CLUTTER_BIND_WIDTH and
 *   %CLUTTER_BIND_HEIGHT (added in Clutter 1.6)
 * @CLUTTER_BIND_ALL: Equivalent to %CLUTTER_BIND_POSITION and
 *   %CLUTTER_BIND_SIZE (added in Clutter 1.10)
 *
 * Specifies which property should be used in a binding
 *
 * Since: 1.4
 */
typedef enum { /*< prefix=CLUTTER_BIND >*/
  CLUTTER_BIND_X,
  CLUTTER_BIND_Y,
  CLUTTER_BIND_WIDTH,
  CLUTTER_BIND_HEIGHT,
  CLUTTER_BIND_POSITION,
  CLUTTER_BIND_SIZE,
  CLUTTER_BIND_ALL
} ClutterBindCoordinate;

/**
 * ClutterEffectPaintFlags:
 * @CLUTTER_EFFECT_PAINT_ACTOR_DIRTY: The actor or one of its children
 *   has queued a redraw before this paint. This implies that the effect
 *   should call clutter_actor_continue_paint() to chain to the next
 *   effect and can not cache any results from a previous paint.
 *
 * Flags passed to the ‘paint’ or ‘pick’ method of #ClutterEffect.
 */
typedef enum { /*< prefix=CLUTTER_EFFECT_PAINT >*/
  CLUTTER_EFFECT_PAINT_ACTOR_DIRTY = (1 << 0)
} ClutterEffectPaintFlags;

/**
 * ClutterBoxAlignment:
 * @CLUTTER_BOX_ALIGNMENT_START: Align the child to the top or to
 *   to the left, depending on the used axis
 * @CLUTTER_BOX_ALIGNMENT_CENTER: Align the child to the center
 * @CLUTTER_BOX_ALIGNMENT_END: Align the child to the bottom or to
 *   the right, depending on the used axis
 *
 * The alignment policies available on each axis of the #ClutterBoxLayout
 *
 * Since: 1.2
 */
typedef enum {
  CLUTTER_BOX_ALIGNMENT_START,
  CLUTTER_BOX_ALIGNMENT_END,
  CLUTTER_BOX_ALIGNMENT_CENTER
} ClutterBoxAlignment;

/**
 * ClutterLongPressState:
 * @CLUTTER_LONG_PRESS_QUERY: Queries the action whether it supports
 *   long presses
 * @CLUTTER_LONG_PRESS_ACTIVATE: Activates the action on a long press
 * @CLUTTER_LONG_PRESS_CANCEL: The long press was cancelled
 *
 * The states for the #ClutterClickAction::long-press signal.
 *
 * Since: 1.8
 */
typedef enum { /*< prefix=CLUTTER_LONG_PRESS >*/
  CLUTTER_LONG_PRESS_QUERY,
  CLUTTER_LONG_PRESS_ACTIVATE,
  CLUTTER_LONG_PRESS_CANCEL
} ClutterLongPressState;

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

/**
 * ClutterDragAxis:
 * @CLUTTER_DRAG_AXIS_NONE: No constraint
 * @CLUTTER_DRAG_X_AXIS: Set a constraint on the X axis
 * @CLUTTER_DRAG_Y_AXIS: Set a constraint on the Y axis
 *
 * The axis of the constraint that should be applied on the
 * dragging action
 *
 * Since: 1.4
 */
typedef enum { /*< prefix=CLUTTER_DRAG >*/
  CLUTTER_DRAG_AXIS_NONE = 0,

  CLUTTER_DRAG_X_AXIS,
  CLUTTER_DRAG_Y_AXIS
} ClutterDragAxis;

/**
 * ClutterEventFlags:
 * @CLUTTER_EVENT_NONE: No flag set
 * @CLUTTER_EVENT_FLAG_SYNTHETIC: Synthetic event
 *
 * Flags for the #ClutterEvent
 *
 * Since: 0.6
 */
typedef enum { /*< flags prefix=CLUTTER_EVENT >*/
  CLUTTER_EVENT_NONE           = 0,
  CLUTTER_EVENT_FLAG_SYNTHETIC = 1 << 0
} ClutterEventFlags;

/**
 * ClutterEventType:
 * @CLUTTER_NOTHING: Empty event
 * @CLUTTER_KEY_PRESS: Key press event
 * @CLUTTER_KEY_RELEASE: Key release event
 * @CLUTTER_MOTION: Pointer motion event
 * @CLUTTER_ENTER: Actor enter event
 * @CLUTTER_LEAVE: Actor leave event
 * @CLUTTER_BUTTON_PRESS: Pointer button press event
 * @CLUTTER_BUTTON_RELEASE: Pointer button release event
 * @CLUTTER_SCROLL: Pointer scroll event
 * @CLUTTER_STAGE_STATE: Stage state change event
 * @CLUTTER_DESTROY_NOTIFY: Destroy notification event
 * @CLUTTER_CLIENT_MESSAGE: Client message event
 * @CLUTTER_DELETE: Stage delete event
 * @CLUTTER_TOUCH_BEGIN: A new touch event sequence has started;
 *   event added in 1.10
 * @CLUTTER_TOUCH_UPDATE: A touch event sequence has been updated;
 *   event added in 1.10
 * @CLUTTER_TOUCH_END: A touch event sequence has finished;
 *   event added in 1.10
 * @CLUTTER_TOUCH_CANCEL: A touch event sequence has been canceled;
 *   event added in 1.10
 * @CLUTTER_EVENT_LAST: Marks the end of the #ClutterEventType enumeration;
 *   added in 1.10
 *
 * Types of events.
 *
 * Since: 0.4
 */
typedef enum { /*< prefix=CLUTTER >*/
  CLUTTER_NOTHING = 0,
  CLUTTER_KEY_PRESS,
  CLUTTER_KEY_RELEASE,
  CLUTTER_MOTION,
  CLUTTER_ENTER,
  CLUTTER_LEAVE,
  CLUTTER_BUTTON_PRESS,
  CLUTTER_BUTTON_RELEASE,
  CLUTTER_SCROLL,
  CLUTTER_STAGE_STATE,
  CLUTTER_DESTROY_NOTIFY,
  CLUTTER_CLIENT_MESSAGE,
  CLUTTER_DELETE,
  CLUTTER_TOUCH_BEGIN,
  CLUTTER_TOUCH_UPDATE,
  CLUTTER_TOUCH_END,
  CLUTTER_TOUCH_CANCEL,

  CLUTTER_EVENT_LAST            /* helper */
} ClutterEventType;

/**
 * ClutterScrollDirection:
 * @CLUTTER_SCROLL_UP: Scroll up
 * @CLUTTER_SCROLL_DOWN: Scroll down
 * @CLUTTER_SCROLL_LEFT: Scroll left
 * @CLUTTER_SCROLL_RIGHT: Scroll right
 * @CLUTTER_SCROLL_SMOOTH: Precise scrolling delta (available in 1.10)
 *
 * Direction of a pointer scroll event.
 *
 * The %CLUTTER_SCROLL_SMOOTH value implies that the #ClutterScrollEvent
 * has precise scrolling delta information.
 *
 * Since: 0.4
 */
typedef enum { /*< prefix=CLUTTER_SCROLL >*/
  CLUTTER_SCROLL_UP,
  CLUTTER_SCROLL_DOWN,
  CLUTTER_SCROLL_LEFT,
  CLUTTER_SCROLL_RIGHT,
  CLUTTER_SCROLL_SMOOTH
} ClutterScrollDirection;

/**
 * ClutterStageState:
 * @CLUTTER_STAGE_STATE_FULLSCREEN: Fullscreen mask
 * @CLUTTER_STAGE_STATE_OFFSCREEN: Offscreen mask (deprecated)
 * @CLUTTER_STAGE_STATE_ACTIVATED: Activated mask
 *
 * Stage state masks, used by the #ClutterEvent of type %CLUTTER_STAGE_STATE.
 *
 * Since: 0.4
 */
typedef enum {
  CLUTTER_STAGE_STATE_FULLSCREEN       = (1 << 1),
  CLUTTER_STAGE_STATE_OFFSCREEN        = (1 << 2),
  CLUTTER_STAGE_STATE_ACTIVATED        = (1 << 3)
} ClutterStageState;

/**
 * ClutterFeatureFlags:
 * @CLUTTER_FEATURE_TEXTURE_NPOT: Set if NPOTS textures supported.
 * @CLUTTER_FEATURE_SYNC_TO_VBLANK: Set if vblank syncing supported.
 * @CLUTTER_FEATURE_TEXTURE_YUV: Set if YUV based textures supported.
 * @CLUTTER_FEATURE_TEXTURE_READ_PIXELS: Set if texture pixels can be read.
 * @CLUTTER_FEATURE_STAGE_STATIC: Set if stage size if fixed (i.e framebuffer)
 * @CLUTTER_FEATURE_STAGE_USER_RESIZE: Set if stage is able to be user resized.
 * @CLUTTER_FEATURE_STAGE_CURSOR: Set if stage has a graphical cursor.
 * @CLUTTER_FEATURE_SHADERS_GLSL: Set if the backend supports GLSL shaders.
 * @CLUTTER_FEATURE_OFFSCREEN: Set if the backend supports offscreen rendering.
 * @CLUTTER_FEATURE_STAGE_MULTIPLE: Set if multiple stages are supported.
 * @CLUTTER_FEATURE_SWAP_EVENTS: Set if the GLX_INTEL_swap_event is supported.
 *
 * Runtime flags indicating specific features available via Clutter window
 * sysytem and graphics backend.
 *
 * Since: 0.4
 */
typedef enum
{
  CLUTTER_FEATURE_TEXTURE_NPOT           = (1 << 2),
  CLUTTER_FEATURE_SYNC_TO_VBLANK         = (1 << 3),
  CLUTTER_FEATURE_TEXTURE_YUV            = (1 << 4),
  CLUTTER_FEATURE_TEXTURE_READ_PIXELS    = (1 << 5),
  CLUTTER_FEATURE_STAGE_STATIC           = (1 << 6),
  CLUTTER_FEATURE_STAGE_USER_RESIZE      = (1 << 7),
  CLUTTER_FEATURE_STAGE_CURSOR           = (1 << 8),
  CLUTTER_FEATURE_SHADERS_GLSL           = (1 << 9),
  CLUTTER_FEATURE_OFFSCREEN              = (1 << 10),
  CLUTTER_FEATURE_STAGE_MULTIPLE         = (1 << 11),
  CLUTTER_FEATURE_SWAP_EVENTS            = (1 << 12)
} ClutterFeatureFlags;

/**
 * ClutterFlowOrientation:
 * @CLUTTER_FLOW_HORIZONTAL: Arrange the children of the flow layout
 *   horizontally first
 * @CLUTTER_FLOW_VERTICAL: Arrange the children of the flow layout
 *   vertically first
 *
 * The direction of the arrangement of the children inside
 * a #ClutterFlowLayout
 *
 * Since: 1.2
 */
typedef enum { /*< prefix=CLUTTER_FLOW >*/
  CLUTTER_FLOW_HORIZONTAL,
  CLUTTER_FLOW_VERTICAL
} ClutterFlowOrientation;

/**
 * ClutterInputDeviceType:
 * @CLUTTER_POINTER_DEVICE: A pointer device
 * @CLUTTER_KEYBOARD_DEVICE: A keyboard device
 * @CLUTTER_EXTENSION_DEVICE: A generic extension device
 * @CLUTTER_JOYSTICK_DEVICE: A joystick device
 * @CLUTTER_TABLET_DEVICE: A tablet device
 * @CLUTTER_TOUCHPAD_DEVICE: A touchpad device
 * @CLUTTER_TOUCHSCREEN_DEVICE: A touch screen device
 * @CLUTTER_PEN_DEVICE: A pen device
 * @CLUTTER_ERASER_DEVICE: An eraser device
 * @CLUTTER_CURSOR_DEVICE: A cursor device
 * @CLUTTER_N_DEVICE_TYPES: The number of device types
 *
 * The types of input devices available.
 *
 * The #ClutterInputDeviceType enumeration can be extended at later
 * date; not every platform supports every input device type.
 *
 * Since: 1.0
 */
typedef enum {
  CLUTTER_POINTER_DEVICE,
  CLUTTER_KEYBOARD_DEVICE,
  CLUTTER_EXTENSION_DEVICE,
  CLUTTER_JOYSTICK_DEVICE,
  CLUTTER_TABLET_DEVICE,
  CLUTTER_TOUCHPAD_DEVICE,
  CLUTTER_TOUCHSCREEN_DEVICE,
  CLUTTER_PEN_DEVICE,
  CLUTTER_ERASER_DEVICE,
  CLUTTER_CURSOR_DEVICE,

  CLUTTER_N_DEVICE_TYPES
} ClutterInputDeviceType;

/**
 * ClutterInputMode:
 * @CLUTTER_INPUT_MODE_MASTER: A master, virtual device
 * @CLUTTER_INPUT_MODE_SLAVE: A slave, physical device, attached to
 *   a master device
 * @CLUTTER_INPUT_MODE_FLOATING: A slave, physical device, not attached
 *   to a master device
 *
 * The mode for input devices available.
 *
 * Since: 1.6
 */
typedef enum {
  CLUTTER_INPUT_MODE_MASTER,
  CLUTTER_INPUT_MODE_SLAVE,
  CLUTTER_INPUT_MODE_FLOATING
} ClutterInputMode;

/**
 * ClutterInputAxis:
 * @CLUTTER_INPUT_AXIS_IGNORE: Unused axis
 * @CLUTTER_INPUT_AXIS_X: The position on the X axis
 * @CLUTTER_INPUT_AXIS_Y: The position of the Y axis
 * @CLUTTER_INPUT_AXIS_PRESSURE: The pressure information
 * @CLUTTER_INPUT_AXIS_XTILT: The tilt on the X axis
 * @CLUTTER_INPUT_AXIS_YTILT: The tile on the Y axis
 * @CLUTTER_INPUT_AXIS_WHEEL: A wheel
 * @CLUTTER_INPUT_AXIS_DISTANCE: Distance (Since 1.12)
 * @CLUTTER_INPUT_AXIS_LAST: Last value of the enumeration; this value is
 *   useful when iterating over the enumeration values (Since 1.12)
 *
 * The type of axes Clutter recognizes on a #ClutterInputDevice
 *
 * Since: 1.6
 */
typedef enum {
  CLUTTER_INPUT_AXIS_IGNORE,

  CLUTTER_INPUT_AXIS_X,
  CLUTTER_INPUT_AXIS_Y,
  CLUTTER_INPUT_AXIS_PRESSURE,
  CLUTTER_INPUT_AXIS_XTILT,
  CLUTTER_INPUT_AXIS_YTILT,
  CLUTTER_INPUT_AXIS_WHEEL,
  CLUTTER_INPUT_AXIS_DISTANCE,

  CLUTTER_INPUT_AXIS_LAST
} ClutterInputAxis;

/**
 * ClutterSnapEdge:
 * @CLUTTER_SNAP_EDGE_TOP: the top edge
 * @CLUTTER_SNAP_EDGE_RIGHT: the right edge
 * @CLUTTER_SNAP_EDGE_BOTTOM: the bottom edge
 * @CLUTTER_SNAP_EDGE_LEFT: the left edge
 *
 * The edge to snap
 *
 * Since: 1.6
 */
typedef enum {
  CLUTTER_SNAP_EDGE_TOP,
  CLUTTER_SNAP_EDGE_RIGHT,
  CLUTTER_SNAP_EDGE_BOTTOM,
  CLUTTER_SNAP_EDGE_LEFT
} ClutterSnapEdge;

/**
 * ClutterPickMode:
 * @CLUTTER_PICK_NONE: Do not paint any actor
 * @CLUTTER_PICK_REACTIVE: Paint only the reactive actors
 * @CLUTTER_PICK_ALL: Paint all actors
 *
 * Controls the paint cycle of the scene graph when in pick mode
 *
 * Since: 1.0
 */
typedef enum {
  CLUTTER_PICK_NONE = 0,
  CLUTTER_PICK_REACTIVE,
  CLUTTER_PICK_ALL
} ClutterPickMode;

/**
 * ClutterSwipeDirection:
 * @CLUTTER_SWIPE_DIRECTION_UP: Upwards swipe gesture
 * @CLUTTER_SWIPE_DIRECTION_DOWN: Downwards swipe gesture
 * @CLUTTER_SWIPE_DIRECTION_LEFT: Leftwards swipe gesture
 * @CLUTTER_SWIPE_DIRECTION_RIGHT: Rightwards swipe gesture
 *
 * The main direction of the swipe gesture
 *
 * Since: 1.8
 */
typedef enum { /*< prefix=CLUTTER_SWIPE_DIRECTION >*/
  CLUTTER_SWIPE_DIRECTION_UP    = 1 << 0,
  CLUTTER_SWIPE_DIRECTION_DOWN  = 1 << 1,
  CLUTTER_SWIPE_DIRECTION_LEFT  = 1 << 2,
  CLUTTER_SWIPE_DIRECTION_RIGHT = 1 << 3
} ClutterSwipeDirection;

/**
 * ClutterPanAxis:
 * @CLUTTER_PAN_AXIS_NONE: No constraint
 * @CLUTTER_PAN_X_AXIS: Set a constraint on the X axis
 * @CLUTTER_PAN_Y_AXIS: Set a constraint on the Y axis
 *
 * The axis of the constraint that should be applied on the
 * panning action
 *
 * Since: 1.12
 */
typedef enum { /*< prefix=CLUTTER_PAN >*/
  CLUTTER_PAN_AXIS_NONE = 0,

  CLUTTER_PAN_X_AXIS,
  CLUTTER_PAN_Y_AXIS
} ClutterPanAxis;


/**
 * ClutterTableAlignment:
 * @CLUTTER_TABLE_ALIGNMENT_START: Align the child to the top or to the
 *   left of a cell in the table, depending on the axis
 * @CLUTTER_TABLE_ALIGNMENT_CENTER: Align the child to the center of
 *   a cell in the table
 * @CLUTTER_TABLE_ALIGNMENT_END: Align the child to the bottom or to the
 *   right of a cell in the table, depending on the axis
 *
 * The alignment policies available on each axis of the #ClutterTableLayout
 *
 * Since: 1.4
 */
typedef enum {
  CLUTTER_TABLE_ALIGNMENT_START,
  CLUTTER_TABLE_ALIGNMENT_CENTER,
  CLUTTER_TABLE_ALIGNMENT_END
} ClutterTableAlignment;

/**
 * ClutterTextureFlags:
 * @CLUTTER_TEXTURE_NONE: No flags
 * @CLUTTER_TEXTURE_RGB_FLAG_BGR: FIXME
 * @CLUTTER_TEXTURE_RGB_FLAG_PREMULT: FIXME
 * @CLUTTER_TEXTURE_YUV_FLAG_YUV2: FIXME
 *
 * Flags for clutter_texture_set_from_rgb_data() and
 * clutter_texture_set_from_yuv_data().
 *
 * Since: 0.4
 */
typedef enum { /*< prefix=CLUTTER_TEXTURE >*/
  CLUTTER_TEXTURE_NONE             = 0,
  CLUTTER_TEXTURE_RGB_FLAG_BGR     = 1 << 1,
  CLUTTER_TEXTURE_RGB_FLAG_PREMULT = 1 << 2, /* FIXME: not handled */
  CLUTTER_TEXTURE_YUV_FLAG_YUV2    = 1 << 3

  /* FIXME: add compressed types ? */
} ClutterTextureFlags;

/**
 * ClutterTextureQuality:
 * @CLUTTER_TEXTURE_QUALITY_LOW: fastest rendering will use nearest neighbour
 *   interpolation when rendering. good setting.
 * @CLUTTER_TEXTURE_QUALITY_MEDIUM: higher quality rendering without using
 *   extra resources.
 * @CLUTTER_TEXTURE_QUALITY_HIGH: render the texture with the best quality
 *   available using extra memory.
 *
 * Enumaration controlling the texture quality.
 *
 * Since: 0.8
 */
typedef enum { /*< prefix=CLUTTER_TEXTURE_QUALITY >*/
  CLUTTER_TEXTURE_QUALITY_LOW,
  CLUTTER_TEXTURE_QUALITY_MEDIUM,
  CLUTTER_TEXTURE_QUALITY_HIGH
} ClutterTextureQuality;

/**
 * ClutterTimelineDirection:
 * @CLUTTER_TIMELINE_FORWARD: forward direction for a timeline
 * @CLUTTER_TIMELINE_BACKWARD: backward direction for a timeline
 *
 * The direction of a #ClutterTimeline
 *
 * Since: 0.6
 */
typedef enum {
  CLUTTER_TIMELINE_FORWARD,
  CLUTTER_TIMELINE_BACKWARD
} ClutterTimelineDirection;

/**
 * ClutterUnitType:
 * @CLUTTER_UNIT_PIXEL: Unit expressed in pixels (with subpixel precision)
 * @CLUTTER_UNIT_EM: Unit expressed in em
 * @CLUTTER_UNIT_MM: Unit expressed in millimeters
 * @CLUTTER_UNIT_POINT: Unit expressed in points
 * @CLUTTER_UNIT_CM: Unit expressed in centimeters
 *
 * The type of unit in which a value is expressed
 *
 * This enumeration might be expanded at later date
 *
 * Since: 1.0
 */
typedef enum { /*< prefix=CLUTTER_UNIT >*/
  CLUTTER_UNIT_PIXEL,
  CLUTTER_UNIT_EM,
  CLUTTER_UNIT_MM,
  CLUTTER_UNIT_POINT,
  CLUTTER_UNIT_CM
} ClutterUnitType;

#define CLUTTER_PATH_RELATIVE           (32)

/**
 * ClutterPathNodeType:
 * @CLUTTER_PATH_MOVE_TO: jump to the given position
 * @CLUTTER_PATH_LINE_TO: create a line from the last node to the
 *   given position
 * @CLUTTER_PATH_CURVE_TO: bezier curve using the last position and
 *   three control points.
 * @CLUTTER_PATH_CLOSE: create a line from the last node to the last
 *   %CLUTTER_PATH_MOVE_TO node.
 * @CLUTTER_PATH_REL_MOVE_TO: same as %CLUTTER_PATH_MOVE_TO but with
 *   coordinates relative to the last node.
 * @CLUTTER_PATH_REL_LINE_TO: same as %CLUTTER_PATH_LINE_TO but with
 *   coordinates relative to the last node.
 * @CLUTTER_PATH_REL_CURVE_TO: same as %CLUTTER_PATH_CURVE_TO but with
 *   coordinates relative to the last node.
 *
 * Types of nodes in a #ClutterPath.
 *
 * Since: 1.0
 */
typedef enum {
  CLUTTER_PATH_MOVE_TO      = 0,
  CLUTTER_PATH_LINE_TO      = 1,
  CLUTTER_PATH_CURVE_TO     = 2,
  CLUTTER_PATH_CLOSE        = 3,

  CLUTTER_PATH_REL_MOVE_TO  = CLUTTER_PATH_MOVE_TO | CLUTTER_PATH_RELATIVE,
  CLUTTER_PATH_REL_LINE_TO  = CLUTTER_PATH_LINE_TO | CLUTTER_PATH_RELATIVE,
  CLUTTER_PATH_REL_CURVE_TO = CLUTTER_PATH_CURVE_TO | CLUTTER_PATH_RELATIVE
} ClutterPathNodeType;

/**
 * ClutterActorAlign:
 * @CLUTTER_ACTOR_ALIGN_FILL: Stretch to cover the whole allocated space
 * @CLUTTER_ACTOR_ALIGN_START: Snap to left or top side, leaving space
 *   to the right or bottom. For horizontal layouts, in right-to-left
 *   locales this should be reversed.
 * @CLUTTER_ACTOR_ALIGN_CENTER: Center the actor inside the allocation
 * @CLUTTER_ACTOR_ALIGN_END: Snap to right or bottom side, leaving space
 *   to the left or top. For horizontal layouts, in right-to-left locales
 *   this should be reversed.
 *
 * Controls how a #ClutterActor should align itself inside the extra space
 * assigned to it during the allocation.
 *
 * Alignment only matters if the allocated space given to an actor is
 * bigger than its natural size; for example, when the #ClutterActor:x-expand
 * or the #ClutterActor:y-expand properties of #ClutterActor are set to %TRUE.
 *
 * Since: 1.10
 */
typedef enum {
  CLUTTER_ACTOR_ALIGN_FILL,
  CLUTTER_ACTOR_ALIGN_START,
  CLUTTER_ACTOR_ALIGN_CENTER,
  CLUTTER_ACTOR_ALIGN_END
} ClutterActorAlign;

/**
 * ClutterRepaintFlags:
 * @CLUTTER_REPAINT_FLAGS_PRE_PAINT: Run the repaint function prior to
 *   painting the stages
 * @CLUTTER_REPAINT_FLAGS_POST_PAINT: Run the repaint function after
 *   painting the stages
 * @CLUTTER_REPAINT_FLAGS_QUEUE_REDRAW_ON_ADD: Ensure that a new frame
 *   is queued after adding the repaint function
 *
 * Flags to pass to clutter_threads_add_repaint_func_full().
 *
 * Since: 1.10
 */
typedef enum {
  CLUTTER_REPAINT_FLAGS_PRE_PAINT = 1 << 0,
  CLUTTER_REPAINT_FLAGS_POST_PAINT = 1 << 1,
  CLUTTER_REPAINT_FLAGS_QUEUE_REDRAW_ON_ADD = 1 << 2
} ClutterRepaintFlags;

/**
 * ClutterContentGravity:
 * @CLUTTER_CONTENT_GRAVITY_TOP_LEFT: Align the content to the top left corner
 * @CLUTTER_CONTENT_GRAVITY_TOP: Align the content to the top edge
 * @CLUTTER_CONTENT_GRAVITY_TOP_RIGHT: Align the content to the top right corner
 * @CLUTTER_CONTENT_GRAVITY_LEFT: Align the content to the left edge
 * @CLUTTER_CONTENT_GRAVITY_CENTER: Align the content to the center
 * @CLUTTER_CONTENT_GRAVITY_RIGHT: Align the content to the right edge
 * @CLUTTER_CONTENT_GRAVITY_BOTTOM_LEFT: Align the content to the bottom left corner
 * @CLUTTER_CONTENT_GRAVITY_BOTTOM: Align the content to the bottom edge
 * @CLUTTER_CONTENT_GRAVITY_BOTTOM_RIGHT: Align the content to the bottom right corner
 * @CLUTTER_CONTENT_GRAVITY_RESIZE_FILL: Resize the content to fill the allocation
 * @CLUTTER_CONTENT_GRAVITY_RESIZE_ASPECT: Resize the content to remain within the
 *   allocation, while maintaining the aspect ratio
 *
 * Controls the alignment of the #ClutterContent inside a #ClutterActor.
 *
 * Since: 1.10
 */
typedef enum {
  CLUTTER_CONTENT_GRAVITY_TOP_LEFT,
  CLUTTER_CONTENT_GRAVITY_TOP,
  CLUTTER_CONTENT_GRAVITY_TOP_RIGHT,

  CLUTTER_CONTENT_GRAVITY_LEFT,
  CLUTTER_CONTENT_GRAVITY_CENTER,
  CLUTTER_CONTENT_GRAVITY_RIGHT,

  CLUTTER_CONTENT_GRAVITY_BOTTOM_LEFT,
  CLUTTER_CONTENT_GRAVITY_BOTTOM,
  CLUTTER_CONTENT_GRAVITY_BOTTOM_RIGHT,

  CLUTTER_CONTENT_GRAVITY_RESIZE_FILL,
  CLUTTER_CONTENT_GRAVITY_RESIZE_ASPECT
} ClutterContentGravity;

/**
 * ClutterScalingFilter:
 * @CLUTTER_SCALING_FILTER_LINEAR: Linear interpolation filter
 * @CLUTTER_SCALING_FILTER_NEAREST: Nearest neighbor interpolation filter
 * @CLUTTER_SCALING_FILTER_TRILINEAR: Trilinear minification filter, with
 *   mipmap generation; this filter linearly interpolates on every axis,
 *   as well as between mipmap levels.
 *
 * The scaling filters to be used with the #ClutterActor:minification-filter
 * and #ClutterActor:magnification-filter properties.
 *
 * Since: 1.10
 */
typedef enum {
  CLUTTER_SCALING_FILTER_LINEAR,
  CLUTTER_SCALING_FILTER_NEAREST,
  CLUTTER_SCALING_FILTER_TRILINEAR
} ClutterScalingFilter;

/**
 * ClutterOrientation:
 * @CLUTTER_ORIENTATION_HORIZONTAL: An horizontal orientation
 * @CLUTTER_ORIENTATION_VERTICAL: A vertical orientation
 *
 * Represents the orientation of actors or layout managers.
 *
 * Since: 1.12
 */
typedef enum {
  CLUTTER_ORIENTATION_HORIZONTAL,
  CLUTTER_ORIENTATION_VERTICAL
} ClutterOrientation;

/**
 * ClutterScrollMode:
 * @CLUTTER_SCROLL_NONE: Ignore scrolling
 * @CLUTTER_SCROLL_HORIZONTALLY: Scroll only horizontally
 * @CLUTTER_SCROLL_VERTICALLY: Scroll only vertically
 * @CLUTTER_SCROLL_BOTH: Scroll in both directions
 *
 * Scroll modes.
 *
 * Since: 1.12
 */
typedef enum { /*< prefix=CLUTTER_SCROLL >*/
  CLUTTER_SCROLL_NONE         = 0,

  CLUTTER_SCROLL_HORIZONTALLY = 1 << 0,
  CLUTTER_SCROLL_VERTICALLY   = 1 << 1,

  CLUTTER_SCROLL_BOTH         = CLUTTER_SCROLL_HORIZONTALLY | CLUTTER_SCROLL_VERTICALLY
} ClutterScrollMode;

/**
 * ClutterGridPosition:
 * @CLUTTER_GRID_POSITION_LEFT: left position
 * @CLUTTER_GRID_POSITION_RIGHT: right position
 * @CLUTTER_GRID_POSITION_TOP: top position
 * @CLUTTER_GRID_POSITION_BOTTOM: bottom position
 *
 * Grid position modes.
 *
 * Since: 1.12
 */
typedef enum {
  CLUTTER_GRID_POSITION_LEFT,
  CLUTTER_GRID_POSITION_RIGHT,
  CLUTTER_GRID_POSITION_TOP,
  CLUTTER_GRID_POSITION_BOTTOM
} ClutterGridPosition;

/**
 * ClutterContentRepeat:
 * @CLUTTER_REPEAT_NONE: No repeat
 * @CLUTTER_REPEAT_X_AXIS: Repeat the content on the X axis
 * @CLUTTER_REPEAT_Y_AXIS: Repeat the content on the Y axis
 * @CLUTTER_REPEAT_BOTH: Repeat the content on both axis
 *
 * Content repeat modes.
 *
 * Since: 1.12
 */
typedef enum {
  CLUTTER_REPEAT_NONE   = 0,
  CLUTTER_REPEAT_X_AXIS = 1 << 0,
  CLUTTER_REPEAT_Y_AXIS = 1 << 1,
  CLUTTER_REPEAT_BOTH   = CLUTTER_REPEAT_X_AXIS | CLUTTER_REPEAT_Y_AXIS
} ClutterContentRepeat;

/**
 * ClutterStepMode:
 * @CLUTTER_STEP_MODE_START: The change in the value of a
 *   %CLUTTER_STEP progress mode should occur at the start of
 *   the transition
 * @CLUTTER_STEP_MODE_END: The change in the value of a
 *   %CLUTTER_STEP progress mode should occur at the end of
 *   the transition
 *
 * Change the value transition of a step function.
 *
 * See clutter_timeline_set_step_progress().
 *
 * Since: 1.12
 */
typedef enum {
  CLUTTER_STEP_MODE_START,
  CLUTTER_STEP_MODE_END
} ClutterStepMode;

/**
 * ClutterZoomAxis:
 * @CLUTTER_ZOOM_X_AXIS: Scale only on the X axis
 * @CLUTTER_ZOOM_Y_AXIS: Scale only on the Y axis
 * @CLUTTER_ZOOM_BOTH: Scale on both axis
 *
 * The axis of the constraint that should be applied by the
 * zooming action.
 *
 * Since: 1.12
 */
typedef enum { /*< prefix=CLUTTER_ZOOM >*/
  CLUTTER_ZOOM_X_AXIS,
  CLUTTER_ZOOM_Y_AXIS,
  CLUTTER_ZOOM_BOTH
} ClutterZoomAxis;

/**
 * ClutterGestureTriggerEdge:
 * @CLUTTER_GESTURE_TRIGGER_EDGE_NONE: Tell #ClutterGestureAction that
 * the gesture must begin immediately and there's no drag limit that
 * will cause its cancellation;
 * @CLUTTER_GESTURE_TRIGGER_EDGE_AFTER: Tell #ClutterGestureAction that
 * it needs to wait until the drag threshold has been exceeded before
 * considering that the gesture has begun;
 * @CLUTTER_GESTURE_TRIGGER_EDGE_BEFORE: Tell #ClutterGestureAction that
 * the gesture must begin immediately and that it must be cancelled
 * once the drag exceed the configured threshold.
 *
 * Enum passed to the clutter_gesture_action_set_threshold_trigger_edge()
 * function.
 *
 * Since: 1.18
 */
typedef enum {
  CLUTTER_GESTURE_TRIGGER_EDGE_NONE  = 0,
  CLUTTER_GESTURE_TRIGGER_EDGE_AFTER,
  CLUTTER_GESTURE_TRIGGER_EDGE_BEFORE
} ClutterGestureTriggerEdge;

G_END_DECLS

#endif /* __CLUTTER_ENUMS_H__ */
