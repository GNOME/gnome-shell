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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __CLUTTER_TYPES_H__
#define __CLUTTER_TYPES_H__

#include <glib-object.h>
#include <clutter/clutter-units.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_GEOMETRY   (clutter_geometry_get_type ())
#define CLUTTER_TYPE_KNOT       (clutter_knot_get_type ())
#define CLUTTER_TYPE_VERTEX     (clutter_vertex_get_type ())

/**
 * ClutterGravity:
 * @CLUTTER_GRAVITY_NONE:
 * @CLUTTER_GRAVITY_NORTH:
 * @CLUTTER_GRAVITY_NORTH_EAST:
 * @CLUTTER_GRAVITY_EAST:
 * @CLUTTER_GRAVITY_SOUTH_EAST:
 * @CLUTTER_GRAVITY_SOUTH:
 * @CLUTTER_GRAVITY_SOUTH_WEST:
 * @CLUTTER_GRAVITY_WEST:
 * @CLUTTER_GRAVITY_NORTH_WEST:
 * @CLUTTER_GRAVITY_CENTER:
 *
 * Gravity of the scaling operations.
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
 * Rectangle containing an actor.
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

G_END_DECLS

#endif /* __CLUTTER_TYPES_H__ */
