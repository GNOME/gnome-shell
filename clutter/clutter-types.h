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

#include <cairo.h>
#include <cogl/cogl.h>
#include <clutter/clutter-macros.h>
#include <clutter/clutter-enums.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ACTOR_BOX          (clutter_actor_box_get_type ())
#define CLUTTER_TYPE_FOG                (clutter_fog_get_type ())
#define CLUTTER_TYPE_GEOMETRY           (clutter_geometry_get_type ())
#define CLUTTER_TYPE_KNOT               (clutter_knot_get_type ())
#define CLUTTER_TYPE_MARGIN             (clutter_margin_get_type ())
#define CLUTTER_TYPE_MATRIX             (clutter_matrix_get_type ())
#define CLUTTER_TYPE_PAINT_VOLUME       (clutter_paint_volume_get_type ())
#define CLUTTER_TYPE_PERSPECTIVE        (clutter_perspective_get_type ())
#define CLUTTER_TYPE_VERTEX             (clutter_vertex_get_type ())
#define CLUTTER_TYPE_POINT              (clutter_point_get_type ())
#define CLUTTER_TYPE_SIZE               (clutter_size_get_type ())
#define CLUTTER_TYPE_RECT               (clutter_rect_get_type ())

typedef struct _ClutterActor                    ClutterActor;

typedef struct _ClutterStage                    ClutterStage;
typedef struct _ClutterContainer                ClutterContainer; /* dummy */
typedef struct _ClutterChildMeta                ClutterChildMeta;
typedef struct _ClutterLayoutMeta               ClutterLayoutMeta;
typedef struct _ClutterActorMeta                ClutterActorMeta;
typedef struct _ClutterLayoutManager            ClutterLayoutManager;
typedef struct _ClutterActorIter                ClutterActorIter;
typedef struct _ClutterPaintNode                ClutterPaintNode;
typedef struct _ClutterContent                  ClutterContent; /* dummy */
typedef struct _ClutterScrollActor	        ClutterScrollActor;

typedef struct _ClutterInterval         	ClutterInterval;
typedef struct _ClutterAnimatable       	ClutterAnimatable; /* dummy */
typedef struct _ClutterTimeline         	ClutterTimeline;
typedef struct _ClutterTransition       	ClutterTransition;
typedef struct _ClutterPropertyTransition       ClutterPropertyTransition;
typedef struct _ClutterKeyframeTransition       ClutterKeyframeTransition;
typedef struct _ClutterTransitionGroup		ClutterTransitionGroup;

typedef struct _ClutterAction                   ClutterAction;
typedef struct _ClutterConstraint               ClutterConstraint;
typedef struct _ClutterEffect                   ClutterEffect;

typedef struct _ClutterPath                     ClutterPath;
typedef struct _ClutterPathNode                 ClutterPathNode;

typedef struct _ClutterActorBox                 ClutterActorBox;
typedef struct _ClutterColor                    ClutterColor;
typedef struct _ClutterGeometry                 ClutterGeometry; /* XXX:2.0 - remove */
typedef struct _ClutterKnot                     ClutterKnot;
typedef struct _ClutterMargin                   ClutterMargin;
typedef struct _ClutterPerspective              ClutterPerspective;
typedef struct _ClutterPoint                    ClutterPoint;
typedef struct _ClutterRect                     ClutterRect;
typedef struct _ClutterSize                     ClutterSize;
typedef struct _ClutterVertex                   ClutterVertex;

typedef struct _ClutterAlpha            	ClutterAlpha;
typedef struct _ClutterAnimation                ClutterAnimation;
typedef struct _ClutterAnimator         	ClutterAnimator;
typedef struct _ClutterState            	ClutterState;

typedef struct _ClutterInputDevice              ClutterInputDevice;

typedef CoglMatrix                              ClutterMatrix;

typedef union _ClutterEvent                     ClutterEvent;

/**
 * ClutterEventSequence:
 *
 * The #ClutterEventSequence structure is an opaque
 * type used to denote the event sequence of a touch event.
 *
 * Since: 1.12
 */
typedef struct _ClutterEventSequence            ClutterEventSequence;

typedef struct _ClutterFog                      ClutterFog; /* deprecated */
typedef struct _ClutterBehaviour                ClutterBehaviour; /* deprecated */
typedef struct _ClutterShader                   ClutterShader; /* deprecated */

/**
 * ClutterPaintVolume:
 *
 * #ClutterPaintVolume is an opaque structure
 * whose members cannot be directly accessed.
 *
 * A #ClutterPaintVolume represents an
 * a bounding volume whose internal representation isn't defined but
 * can be set and queried in terms of an axis aligned bounding box.
 *
 * A #ClutterPaintVolume for a #ClutterActor
 * is defined to be relative from the current actor modelview matrix.
 *
 * Other internal representation and methods for describing the
 * bounding volume may be added in the future.
 *
 * Since: 1.4
 */
typedef struct _ClutterPaintVolume      ClutterPaintVolume;

/**
 * ClutterPoint:
 * @x: X coordinate, in pixels
 * @y: Y coordinate, in pixels
 *
 * A point in 2D space.
 *
 * Since: 1.12
 */
struct _ClutterPoint
{
  float x;
  float y;
};

/**
 * CLUTTER_POINT_INIT:
 * @x: X coordinate
 * @y: Y coordinate
 *
 * A simple macro for initializing a #ClutterPoint when declaring it, e.g.:
 *
 * |[
 *   ClutterPoint p = CLUTTER_POINT_INIT (100, 100);
 * ]|
 *
 * Since: 1.12
 */
#define CLUTTER_POINT_INIT(x,y)         { (x), (y) }

/**
 * CLUTTER_POINT_INIT_ZERO:
 *
 * A simple macro for initializing a #ClutterPoint to (0, 0) when
 * declaring it.
 *
 * Since: 1.12
 */
#define CLUTTER_POINT_INIT_ZERO         CLUTTER_POINT_INIT (0.f, 0.f)

CLUTTER_AVAILABLE_IN_1_12
GType clutter_point_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_12
const ClutterPoint *    clutter_point_zero      (void);
CLUTTER_AVAILABLE_IN_1_12
ClutterPoint *          clutter_point_alloc     (void);
CLUTTER_AVAILABLE_IN_1_12
ClutterPoint *          clutter_point_init      (ClutterPoint       *point,
                                                 float               x,
                                                 float               y);
CLUTTER_AVAILABLE_IN_1_12
ClutterPoint *          clutter_point_copy      (const ClutterPoint *point);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_point_free      (ClutterPoint       *point);
CLUTTER_AVAILABLE_IN_1_12
gboolean                clutter_point_equals    (const ClutterPoint *a,
                                                 const ClutterPoint *b);
CLUTTER_AVAILABLE_IN_1_12
float                   clutter_point_distance  (const ClutterPoint *a,
                                                 const ClutterPoint *b,
                                                 float              *x_distance,
                                                 float              *y_distance);

/**
 * ClutterSize:
 * @width: the width, in pixels
 * @height: the height, in pixels
 *
 * A size, in 2D space.
 *
 * Since: 1.12
 */
struct _ClutterSize
{
  float width;
  float height;
};

/**
 * CLUTTER_SIZE_INIT:
 * @width: the width
 * @height: the height
 *
 * A simple macro for initializing a #ClutterSize when declaring it, e.g.:
 *
 * |[
 *   ClutterSize s = CLUTTER_SIZE_INIT (200, 200);
 * ]|
 *
 * Since: 1.12
 */
#define CLUTTER_SIZE_INIT(width,height) { (width), (height) }

/**
 * CLUTTER_SIZE_INIT_ZERO:
 *
 * A simple macro for initializing a #ClutterSize to (0, 0) when
 * declaring it.
 *
 * Since: 1.12
 */
#define CLUTTER_SIZE_INIT_ZERO          CLUTTER_SIZE_INIT (0.f, 0.f)

CLUTTER_AVAILABLE_IN_1_12
GType clutter_size_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_12
ClutterSize *   clutter_size_alloc      (void);
CLUTTER_AVAILABLE_IN_1_12
ClutterSize *   clutter_size_init       (ClutterSize       *size,
                                         float              width,
                                         float              height);
CLUTTER_AVAILABLE_IN_1_12
ClutterSize *   clutter_size_copy       (const ClutterSize *size);
CLUTTER_AVAILABLE_IN_1_12
void            clutter_size_free       (ClutterSize       *size);
CLUTTER_AVAILABLE_IN_1_12
gboolean        clutter_size_equals     (const ClutterSize *a,
                                         const ClutterSize *b);

/**
 * ClutterRect:
 * @origin: the origin of the rectangle
 * @size: the size of the rectangle
 *
 * The location and size of a rectangle.
 *
 * The width and height of a #ClutterRect can be negative; Clutter considers
 * a rectangle with an origin of [ 0.0, 0.0 ] and a size of [ 10.0, 10.0 ] to
 * be equivalent to a rectangle with origin of [ 10.0, 10.0 ] and size of
 * [ -10.0, -10.0 ].
 *
 * Application code can normalize rectangles using clutter_rect_normalize():
 * this function will ensure that the width and height of a #ClutterRect are
 * positive values. All functions taking a #ClutterRect as an argument will
 * implicitly normalize it before computing eventual results. For this reason
 * it is safer to access the contents of a #ClutterRect by using the provided
 * API at all times, instead of directly accessing the structure members.
 *
 * Since: 1.12
 */
struct _ClutterRect
{
  ClutterPoint origin;
  ClutterSize size;
};

/**
 * CLUTTER_RECT_INIT:
 * @x: the X coordinate
 * @y: the Y coordinate
 * @width: the width
 * @height: the height
 *
 * A simple macro for initializing a #ClutterRect when declaring it, e.g.:
 *
 * |[
 *   ClutterRect r = CLUTTER_RECT_INIT (100, 100, 200, 200);
 * ]|
 *
 * Since: 1.12
 */
#define CLUTTER_RECT_INIT(x,y,width,height)     { { (x), (y) }, { (width), (height) } }

/**
 * CLUTTER_RECT_INIT_ZERO:
 *
 * A simple macro for initializing a #ClutterRect to (0, 0, 0, 0) when
 * declaring it.
 *
 * Since: 1.12
 */
#define CLUTTER_RECT_INIT_ZERO                  CLUTTER_RECT_INIT (0.f, 0.f, 0.f, 0.f)

CLUTTER_AVAILABLE_IN_1_12
GType clutter_rect_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_12
const ClutterRect *     clutter_rect_zero               (void);
CLUTTER_AVAILABLE_IN_1_12
ClutterRect *           clutter_rect_alloc              (void);
CLUTTER_AVAILABLE_IN_1_12
ClutterRect *           clutter_rect_init               (ClutterRect       *rect,
                                                         float              x,
                                                         float              y,
                                                         float              width,
                                                         float              height);
CLUTTER_AVAILABLE_IN_1_12
ClutterRect *           clutter_rect_copy               (const ClutterRect *rect);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_rect_free               (ClutterRect       *rect);
CLUTTER_AVAILABLE_IN_1_12
gboolean                clutter_rect_equals             (ClutterRect       *a,
                                                         ClutterRect       *b);

CLUTTER_AVAILABLE_IN_1_12
ClutterRect *           clutter_rect_normalize          (ClutterRect       *rect);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_rect_get_center         (ClutterRect       *rect,
                                                         ClutterPoint      *center);
CLUTTER_AVAILABLE_IN_1_12
gboolean                clutter_rect_contains_point     (ClutterRect       *rect,
                                                         ClutterPoint      *point);
CLUTTER_AVAILABLE_IN_1_12
gboolean                clutter_rect_contains_rect      (ClutterRect       *a,
                                                         ClutterRect       *b);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_rect_union              (ClutterRect       *a,
                                                         ClutterRect       *b,
                                                         ClutterRect       *res);
CLUTTER_AVAILABLE_IN_1_12
gboolean                clutter_rect_intersection       (ClutterRect       *a,
                                                         ClutterRect       *b,
                                                         ClutterRect       *res);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_rect_offset             (ClutterRect       *rect,
                                                         float              d_x,
                                                         float              d_y);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_rect_inset              (ClutterRect       *rect,
                                                         float              d_x,
                                                         float              d_y);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_rect_clamp_to_pixel     (ClutterRect       *rect);
CLUTTER_AVAILABLE_IN_1_12
float                   clutter_rect_get_x              (ClutterRect       *rect);
CLUTTER_AVAILABLE_IN_1_12
float                   clutter_rect_get_y              (ClutterRect       *rect);
CLUTTER_AVAILABLE_IN_1_12
float                   clutter_rect_get_width          (ClutterRect       *rect);
CLUTTER_AVAILABLE_IN_1_12
float                   clutter_rect_get_height         (ClutterRect       *rect);

/**
 * ClutterVertex:
 * @x: X coordinate of the vertex
 * @y: Y coordinate of the vertex
 * @z: Z coordinate of the vertex
 *
 * A point in 3D space, expressed in pixels
 *
 * Since: 0.4
 */
struct _ClutterVertex
{
  gfloat x;
  gfloat y;
  gfloat z;
};

/**
 * CLUTTER_VERTEX_INIT:
 * @x: the X coordinate of the vertex
 * @y: the Y coordinate of the vertex
 * @z: the Z coordinate of the vertex
 *
 * A simple macro for initializing a #ClutterVertex when declaring it, e.g.:
 *
 * |[
 *   ClutterVertex v = CLUTTER_VERTEX_INIT (x, y, z);
 * ]|
 *
 * Since: 1.10
 */
#define CLUTTER_VERTEX_INIT(x,y,z)      { (x), (y), (z) }

/**
 * CLUTTER_VERTEX_INIT_ZERO:
 *
 * A simple macro for initializing a #ClutterVertex to (0, 0, 0).
 *
 * Since: 1.12
 */
#define CLUTTER_VERTEX_INIT_ZERO        CLUTTER_VERTEX_INIT (0.f, 0.f, 0.f)

CLUTTER_AVAILABLE_IN_ALL
GType          clutter_vertex_get_type (void) G_GNUC_CONST;
CLUTTER_AVAILABLE_IN_ALL
ClutterVertex *clutter_vertex_new      (gfloat               x,
                                        gfloat               y,
                                        gfloat               z);
CLUTTER_AVAILABLE_IN_1_12
ClutterVertex *clutter_vertex_alloc    (void);
CLUTTER_AVAILABLE_IN_ALL
ClutterVertex *clutter_vertex_init     (ClutterVertex       *vertex,
                                        gfloat               x,
                                        gfloat               y,
                                        gfloat               z);
CLUTTER_AVAILABLE_IN_ALL
ClutterVertex *clutter_vertex_copy     (const ClutterVertex *vertex);
CLUTTER_AVAILABLE_IN_ALL
void           clutter_vertex_free     (ClutterVertex       *vertex);
CLUTTER_AVAILABLE_IN_ALL
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

/**
 * CLUTTER_ACTOR_BOX_INIT:
 * @x_1: the X coordinate of the top left corner
 * @y_1: the Y coordinate of the top left corner
 * @x_2: the X coordinate of the bottom right corner
 * @y_2: the Y coordinate of the bottom right corner
 *
 * A simple macro for initializing a #ClutterActorBox when declaring
 * it, e.g.:
 *
 * |[
 *   ClutterActorBox box = CLUTTER_ACTOR_BOX_INIT (0, 0, 400, 600);
 * ]|
 *
 * Since: 1.10
 */
#define CLUTTER_ACTOR_BOX_INIT(x_1,y_1,x_2,y_2)         { (x_1), (y_1), (x_2), (y_2) }

/**
 * CLUTTER_ACTOR_BOX_INIT_ZERO:
 *
 * A simple macro for initializing a #ClutterActorBox to 0 when
 * declaring it, e.g.:
 *
 * |[
 *   ClutterActorBox box = CLUTTER_ACTOR_BOX_INIT_ZERO;
 * ]|
 *
 * Since: 1.12
 */
#define CLUTTER_ACTOR_BOX_INIT_ZERO                     CLUTTER_ACTOR_BOX_INIT (0.f, 0.f, 0.f, 0.f)

CLUTTER_AVAILABLE_IN_ALL
GType            clutter_actor_box_get_type      (void) G_GNUC_CONST;
CLUTTER_AVAILABLE_IN_ALL
ClutterActorBox *clutter_actor_box_new           (gfloat                 x_1,
                                                  gfloat                 y_1,
                                                  gfloat                 x_2,
                                                  gfloat                 y_2);
CLUTTER_AVAILABLE_IN_1_12
ClutterActorBox *clutter_actor_box_alloc         (void);
CLUTTER_AVAILABLE_IN_ALL
ClutterActorBox *clutter_actor_box_init          (ClutterActorBox       *box,
                                                  gfloat                 x_1,
                                                  gfloat                 y_1,
                                                  gfloat                 x_2,
                                                  gfloat                 y_2);
CLUTTER_AVAILABLE_IN_ALL
void             clutter_actor_box_init_rect     (ClutterActorBox       *box,
                                                  gfloat                 x,
                                                  gfloat                 y,
                                                  gfloat                 width,
                                                  gfloat                 height);
CLUTTER_AVAILABLE_IN_ALL
ClutterActorBox *clutter_actor_box_copy          (const ClutterActorBox *box);
CLUTTER_AVAILABLE_IN_ALL
void             clutter_actor_box_free          (ClutterActorBox       *box);
CLUTTER_AVAILABLE_IN_ALL
gboolean         clutter_actor_box_equal         (const ClutterActorBox *box_a,
                                                  const ClutterActorBox *box_b);
CLUTTER_AVAILABLE_IN_ALL
gfloat           clutter_actor_box_get_x         (const ClutterActorBox *box);
CLUTTER_AVAILABLE_IN_ALL
gfloat           clutter_actor_box_get_y         (const ClutterActorBox *box);
CLUTTER_AVAILABLE_IN_ALL
gfloat           clutter_actor_box_get_width     (const ClutterActorBox *box);
CLUTTER_AVAILABLE_IN_ALL
gfloat           clutter_actor_box_get_height    (const ClutterActorBox *box);
CLUTTER_AVAILABLE_IN_ALL
void             clutter_actor_box_get_origin    (const ClutterActorBox *box,
                                                  gfloat                *x,
                                                  gfloat                *y);
CLUTTER_AVAILABLE_IN_ALL
void             clutter_actor_box_get_size      (const ClutterActorBox *box,
                                                  gfloat                *width,
                                                  gfloat                *height);
CLUTTER_AVAILABLE_IN_ALL
gfloat           clutter_actor_box_get_area      (const ClutterActorBox *box);
CLUTTER_AVAILABLE_IN_ALL
gboolean         clutter_actor_box_contains      (const ClutterActorBox *box,
                                                  gfloat                 x,
                                                  gfloat                 y);
CLUTTER_AVAILABLE_IN_ALL
void             clutter_actor_box_from_vertices (ClutterActorBox       *box,
                                                  const ClutterVertex    verts[]);
CLUTTER_AVAILABLE_IN_ALL
void             clutter_actor_box_interpolate   (const ClutterActorBox *initial,
                                                  const ClutterActorBox *final,
                                                  gdouble                progress,
                                                  ClutterActorBox       *result);
CLUTTER_AVAILABLE_IN_ALL
void             clutter_actor_box_clamp_to_pixel (ClutterActorBox       *box);
CLUTTER_AVAILABLE_IN_ALL
void             clutter_actor_box_union          (const ClutterActorBox *a,
                                                   const ClutterActorBox *b,
                                                   ClutterActorBox       *result);

CLUTTER_AVAILABLE_IN_ALL
void             clutter_actor_box_set_origin     (ClutterActorBox       *box,
                                                   gfloat                 x,
                                                   gfloat                 y);
CLUTTER_AVAILABLE_IN_ALL
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
 *
 * You should not use #ClutterGeometry, or operate on its fields
 * directly; you should use #cairo_rectangle_int_t or #ClutterRect if you
 * need a rectangle type, depending on the precision required.
 *
 * Deprecated: 1.16
 */
struct _ClutterGeometry
{
  /*< public >*/
  gint   x;
  gint   y;
  guint  width;
  guint  height;
};

CLUTTER_AVAILABLE_IN_ALL
GType clutter_geometry_get_type (void) G_GNUC_CONST;

CLUTTER_DEPRECATED_IN_1_16
void      clutter_geometry_union      (const ClutterGeometry *geometry_a,
                                       const ClutterGeometry *geometry_b,
                                       ClutterGeometry       *result);
CLUTTER_DEPRECATED_IN_1_16
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

CLUTTER_AVAILABLE_IN_ALL
GType        clutter_knot_get_type (void) G_GNUC_CONST;
CLUTTER_AVAILABLE_IN_ALL
ClutterKnot *clutter_knot_copy     (const ClutterKnot *knot);
CLUTTER_AVAILABLE_IN_ALL
void         clutter_knot_free     (ClutterKnot       *knot);
CLUTTER_AVAILABLE_IN_ALL
gboolean     clutter_knot_equal    (const ClutterKnot *knot_a,
                                    const ClutterKnot *knot_b);

/**
 * ClutterPathNode:
 * @type: the node's type
 * @points: the coordinates of the node
 *
 * Represents a single node of a #ClutterPath.
 *
 * Some of the coordinates in @points may be unused for some node
 * types. %CLUTTER_PATH_MOVE_TO and %CLUTTER_PATH_LINE_TO use only one
 * pair of coordinates, %CLUTTER_PATH_CURVE_TO uses all three and
 * %CLUTTER_PATH_CLOSE uses none.
 *
 * Since: 1.0
 */
struct _ClutterPathNode
{
  ClutterPathNodeType type;

  ClutterKnot points[3];
};

CLUTTER_AVAILABLE_IN_1_0
GType clutter_path_node_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_0
ClutterPathNode *clutter_path_node_copy  (const ClutterPathNode *node);
CLUTTER_AVAILABLE_IN_1_0
void             clutter_path_node_free  (ClutterPathNode       *node);
CLUTTER_AVAILABLE_IN_1_0
gboolean         clutter_path_node_equal (const ClutterPathNode *node_a,
                                          const ClutterPathNode *node_b);

/*
 * ClutterPaintVolume
 */

CLUTTER_AVAILABLE_IN_1_2
GType clutter_paint_volume_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_2
ClutterPaintVolume *clutter_paint_volume_copy                (const ClutterPaintVolume *pv);
CLUTTER_AVAILABLE_IN_1_2
void                clutter_paint_volume_free                (ClutterPaintVolume       *pv);

CLUTTER_AVAILABLE_IN_1_2
void                clutter_paint_volume_set_origin          (ClutterPaintVolume       *pv,
                                                              const ClutterVertex      *origin);
CLUTTER_AVAILABLE_IN_1_2
void                clutter_paint_volume_get_origin          (const ClutterPaintVolume *pv,
                                                              ClutterVertex            *vertex);
CLUTTER_AVAILABLE_IN_1_2
void                clutter_paint_volume_set_width           (ClutterPaintVolume       *pv,
                                                              gfloat                    width);
CLUTTER_AVAILABLE_IN_1_2
gfloat              clutter_paint_volume_get_width           (const ClutterPaintVolume *pv);
CLUTTER_AVAILABLE_IN_1_2
void                clutter_paint_volume_set_height          (ClutterPaintVolume       *pv,
                                                              gfloat                    height);
CLUTTER_AVAILABLE_IN_1_2
gfloat              clutter_paint_volume_get_height          (const ClutterPaintVolume *pv);
CLUTTER_AVAILABLE_IN_1_2
void                clutter_paint_volume_set_depth           (ClutterPaintVolume       *pv,
                                                              gfloat                    depth);
CLUTTER_AVAILABLE_IN_1_2
gfloat              clutter_paint_volume_get_depth           (const ClutterPaintVolume *pv);
CLUTTER_AVAILABLE_IN_1_2
void                clutter_paint_volume_union               (ClutterPaintVolume       *pv,
                                                              const ClutterPaintVolume *another_pv);
CLUTTER_AVAILABLE_IN_1_10
void                clutter_paint_volume_union_box           (ClutterPaintVolume       *pv,
                                                              const ClutterActorBox    *box);

CLUTTER_AVAILABLE_IN_1_2
gboolean            clutter_paint_volume_set_from_allocation (ClutterPaintVolume       *pv,
                                                              ClutterActor             *actor);

/**
 * ClutterMargin:
 * @left: the margin from the left
 * @right: the margin from the right
 * @top: the margin from the top
 * @bottom: the margin from the bottom
 *
 * A representation of the components of a margin.
 *
 * Since: 1.10
 */
struct _ClutterMargin
{
  float left;
  float right;
  float top;
  float bottom;
};

CLUTTER_AVAILABLE_IN_1_10
GType clutter_margin_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_10
ClutterMargin * clutter_margin_new      (void) G_GNUC_MALLOC;
CLUTTER_AVAILABLE_IN_1_10
ClutterMargin * clutter_margin_copy     (const ClutterMargin *margin_);
CLUTTER_AVAILABLE_IN_1_10
void            clutter_margin_free     (ClutterMargin       *margin_);

/**
 * ClutterProgressFunc:
 * @a: the initial value of an interval
 * @b: the final value of an interval
 * @progress: the progress factor, between 0 and 1
 * @retval: the value used to store the progress
 *
 * Prototype of the progress function used to compute the value
 * between the two ends @a and @b of an interval depending on
 * the value of @progress.
 *
 * The #GValue in @retval is already initialized with the same
 * type as @a and @b.
 *
 * This function will be called by #ClutterInterval if the
 * type of the values of the interval was registered using
 * clutter_interval_register_progress_func().
 *
 * Return value: %TRUE if the function successfully computed
 *   the value and stored it inside @retval
 *
 * Since: 1.0
 */
typedef gboolean (* ClutterProgressFunc) (const GValue *a,
                                          const GValue *b,
                                          gdouble       progress,
                                          GValue       *retval);

CLUTTER_AVAILABLE_IN_1_0
void clutter_interval_register_progress_func (GType               value_type,
                                              ClutterProgressFunc func);

CLUTTER_AVAILABLE_IN_1_12
GType clutter_matrix_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_12
ClutterMatrix * clutter_matrix_alloc            (void);
CLUTTER_AVAILABLE_IN_1_12
ClutterMatrix * clutter_matrix_init_identity    (ClutterMatrix       *matrix);
CLUTTER_AVAILABLE_IN_1_12
ClutterMatrix * clutter_matrix_init_from_array  (ClutterMatrix       *matrix,
                                                 const float          values[16]);
CLUTTER_AVAILABLE_IN_1_12
ClutterMatrix * clutter_matrix_init_from_matrix (ClutterMatrix       *a,
                                                 const ClutterMatrix *b);
CLUTTER_AVAILABLE_IN_1_12
void            clutter_matrix_free             (ClutterMatrix       *matrix);

G_END_DECLS

#endif /* __CLUTTER_TYPES_H__ */
