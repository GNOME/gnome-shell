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
#include <clutter/clutter-units.h>

G_BEGIN_DECLS

#define CLUTTER_ANGLE_MAX_DEG 1509949439.6

#define CLUTTER_TYPE_GEOMETRY   (clutter_geometry_get_type ())
#define CLUTTER_TYPE_KNOT       (clutter_knot_get_type ())
#define CLUTTER_TYPE_VERTEX     (clutter_vertex_get_type ())

/* Forward delarations to avoid header catch 22's */
typedef struct _ClutterActor            ClutterActor;
typedef struct _ClutterStage            ClutterStage;
typedef struct _ClutterContainer        ClutterContainer; /* dummy */
typedef struct _ClutterChildMeta        ClutterChildMeta;

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

typedef struct _ClutterGeometry         ClutterGeometry;
typedef struct _ClutterKnot             ClutterKnot;
typedef struct _ClutterVertex           ClutterVertex;

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


/**
 * ClutterVertex:
 * @x: X coordinate of the vertex
 * @y: Y coordinate of the vertex
 * @z: Z coordinate of the vertex
 *
 * Vertex of an actor in 3D space, expressed in device independent units.
 *
 * Since: 0.4
 */
struct _ClutterVertex
{
  ClutterUnit x;
  ClutterUnit y;
  ClutterUnit z;
};

GType clutter_vertex_get_type (void) G_GNUC_CONST;

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
 * @CLUTTER_LINEAR: linear progress
 * @CLUTTER_SINE_IN: sine-in progress
 * @CLUTTER_SINE_OUT: sine-out progress
 * @CLUTTER_SINE_IN_OUT: sine-in-out progress
 * @CLUTTER_EASE_IN: ease-in progress
 * @CLUTTER_EASE_OUT: ease-out progress
 * @CLUTTER_EASE_IN_OUT: ease-in-out progress
 *
 * The animation modes used by #ClutterAlpha and #ClutterAnimation. This
 * enumeration can be expanded in later versions of Clutter.
 *
 * Since: 1.0
 */
typedef enum {
  CLUTTER_CUSTOM_MODE,
  CLUTTER_LINEAR,
  CLUTTER_SINE_IN,
  CLUTTER_SINE_OUT,
  CLUTTER_SINE_IN_OUT,
  CLUTTER_EASE_IN,
  CLUTTER_EASE_OUT,
  CLUTTER_EASE_IN_OUT
} ClutterAnimationMode;

G_END_DECLS

#endif /* __CLUTTER_TYPES_H__ */
