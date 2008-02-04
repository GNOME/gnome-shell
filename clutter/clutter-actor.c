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

/**
 * SECTION:clutter-actor
 * @short_description: Base abstract class for all visual stage actors.
 *
 * #ClutterActor is a base abstract class for all visual elements on the
 * stage. Every object that must appear on the main #ClutterStage must also
 * be a #ClutterActor, either by using one of the classes provided by
 * Clutter, or by implementing a new #ClutterActor subclass.
 *
 * Actor Transformations
 * The OpenGL modelview matrix for the actor is constructed from the actor
 * settings by the following order of operations:
 * <orderedlist>
 *   <listitem><para>Translation by actor x, y coords,</para></listitem>
 *   <listitem><para>Scaling by scale_x, scale_y,</para></listitem>
 *   <listitem><para>Negative translation by anchor point x, y,</para>
 *   </listitem>
 *   <listitem><para>Rotation around z axis,</para></listitem>
 *   <listitem><para>Rotation around y axis,</para></listitem>
 *   <listitem><para>Rotation around x axis,</para></listitem>
 *   <listitem><para>Translation by actor depth (z),</para></listitem>
 *   <listitem><para>Clip stencil is applied (not an operation on the matrix
 *   as such, but done as part of the transform set up).</para>
 *   </listitem>
 * </orderedlist>
 *
 * NB: the position of any children is referenced from the top-left corner of
 * the parent, not the parent's anchor point.
 *
 * Event handling
 * <orderedlist>
 *   <listitem><para>Actors emit pointer events if set reactive, see
 *   clutter_actor_set_reactive()</para></listitem>
 *   <listitem><para>The stage is always reactive</para></listitem>
 *   <listitem><para>Events are handled by connecting signal handlers to
 *   the numerous event signal types.</para></listitem>
 *   <listitem><para>Event handlers must return %TRUE if they handled
 *   the event and wish to block the event emission chain, or %FALSE
 *   if the emission chain must continue</para></listitem>
 *   <listitem><para>Keyboard events are emitted if actor has focus, see
 *   clutter_stage_set_key_focus()</para></listitem>
 *   <listitem><para>Motion events (motion, enter, leave) are not emitted
 *   if clutter_set_motion_events_enabled() is called with %FALSE.
 *   See clutter_set_motion_events_enabled() documentation for more
 *   information.</para></listitem>
 *   <listitem><para>Once emitted, an event emission chain has two
 *   phases: capture and bubble. A emitted event starts in the capture
 *   phase beginning at the stage and traversing every child actor until
 *   the event source actor is reached. The emission then enters the bubble
 *   phase, traversing back up the chain via parents until it reaches the
 *   stage. Any event handler can abort this chain by returning
 *   %TRUE (meaning "event handled").</para></listitem>
 *   <listitem><para>Pointer events will 'pass through' non reactive
 *   overlapping actors.</para></listitem>
 * </orderedlist>
 */

/**
 * CLUTTER_ACTOR_IS_MAPPED:
 * @e: a #ClutterActor
 *
 * Evaluates to %TRUE if the %CLUTTER_ACTOR_MAPPED flag is set.
 *
 * Since: 0.2
 */

/**
 * CLUTTER_ACTOR_IS_REALIZED:
 * @e: a #ClutterActor
 *
 * Evaluates to %TRUE if the %CLUTTER_ACTOR_REALIZED flag is set.
 *
 * Since: 0.2
 */

/**
 * CLUTTER_ACTOR_IS_VISIBLE:
 * @e: a #ClutterActor
 *
 * Evaluates to %TRUE if the actor is both realized and mapped.
 *
 * Since: 0.2
 */

/**
 * CLUTTER_ACTOR_IS_REACTIVE:
 * @e: a #ClutterActor
 *
 * Evaluates to %TRUE if the %CLUTTER_ACTOR_REACTIVE flag is set.
 *
 * Since: 0.6
 */

/**
 * CLUTTER_ACTOR_SET_FLAGS:
 * @e: a #ClutterActor
 * @f: the flags to set
 *
 * Sets flags on the given #ClutterActor
 *
 * Since: 0.2
 */

/**
 * CLUTTER_ACTOR_UNSET_FLAGS:
 * @e: a #ClutterActor
 * @f: the flags to unset
 *
 * Unsets flags on the given #ClutterActor
 *
 * Since: 0.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-actor.h"
#include "clutter-container.h"
#include "clutter-main.h"
#include "clutter-enum-types.h"
#include "clutter-scriptable.h"
#include "clutter-script.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-units.h"
#include "cogl.h"

static guint32 __id = 0;

typedef struct _ShaderData ShaderData;

#define CLUTTER_ACTOR_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_ACTOR, ClutterActorPrivate))

struct _ClutterActorPrivate
{
  ClutterActorBox coords;

  ClutterUnit     clip[4];
  guint           has_clip : 1;

  /* Rotation angles */
  ClutterFixed    rxang;
  ClutterFixed    ryang;
  ClutterFixed    rzang;

  /* Rotation center: X axis */
  ClutterUnit     rxy;
  ClutterUnit     rxz;

  /* Rotation center: Y axis */
  ClutterUnit     ryx;
  ClutterUnit     ryz;

  /* Rotation center: Z axis */
  ClutterUnit     rzx;
  ClutterUnit     rzy;

  /* Anchor point coordinates */
  ClutterUnit     anchor_x;
  ClutterUnit     anchor_y;

  /* depth */
  ClutterUnit     z;

  guint8          opacity;

  ClutterActor   *parent_actor;

  gchar          *name;
  guint32         id; /* Unique ID */

  ClutterFixed    scale_x;
  ClutterFixed    scale_y;

  ShaderData     *shader_data;
};

enum
{
  PROP_0,

  PROP_NAME,

  PROP_X,
  PROP_Y,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_DEPTH,

  PROP_CLIP,
  PROP_HAS_CLIP,

  PROP_OPACITY,
  PROP_VISIBLE,
  PROP_REACTIVE,

  PROP_SCALE_X,
  PROP_SCALE_Y,

  PROP_ROTATION_ANGLE_X,
  PROP_ROTATION_ANGLE_Y,
  PROP_ROTATION_ANGLE_Z,
  PROP_ROTATION_CENTER_X,
  PROP_ROTATION_CENTER_Y,
  PROP_ROTATION_CENTER_Z
};

enum
{
  SHOW,
  HIDE,
  DESTROY,
  PARENT_SET,
  FOCUS_IN,
  FOCUS_OUT,

  EVENT,
  CAPTURED_EVENT,
  BUTTON_PRESS_EVENT,
  BUTTON_RELEASE_EVENT,
  SCROLL_EVENT,
  KEY_PRESS_EVENT,
  KEY_RELEASE_EVENT,
  MOTION_EVENT,
  ENTER_EVENT,
  LEAVE_EVENT,

  LAST_SIGNAL
};

static guint actor_signals[LAST_SIGNAL] = { 0, };

static void clutter_scriptable_iface_init (ClutterScriptableIface *iface);

static void _clutter_actor_apply_modelview_transform           (ClutterActor *self);
static void _clutter_actor_apply_modelview_transform_recursive (ClutterActor *self);

static void clutter_actor_shader_pre_paint (ClutterActor *actor,
                                            gboolean      repeat);
static void clutter_actor_shader_post_paint (ClutterActor *actor);
static void destroy_shader_data (ClutterActor *self);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ClutterActor,
                                  clutter_actor,
                                  G_TYPE_INITIALLY_UNOWNED,
                                  G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_SCRIPTABLE,
                                                         clutter_scriptable_iface_init));

static gboolean
redraw_update_idle (gpointer data)
{
  ClutterMainContext *ctx = CLUTTER_CONTEXT();

  if (ctx->update_idle)
    {
      g_source_remove (ctx->update_idle);
      ctx->update_idle = 0;
    }

  clutter_redraw ();

  return FALSE;
}

static void
clutter_actor_real_show (ClutterActor *self)
{
  if (!CLUTTER_ACTOR_IS_VISIBLE (self))
    {
      if (!CLUTTER_ACTOR_IS_REALIZED (self))
        clutter_actor_realize (self);

      /* the mapped flag on the top-level actors is set by the
       * per-backend implementation because it might be asynchronous
       */
      if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL))
        CLUTTER_ACTOR_SET_FLAGS (self, CLUTTER_ACTOR_MAPPED);

      if (CLUTTER_ACTOR_IS_VISIBLE (self))
        clutter_actor_queue_redraw (self);
    }
}

/**
 * clutter_actor_show
 * @self: A #ClutterActor
 *
 * Flags a clutter actor to be displayed. An actor not shown will not
 * appear on the display.
 **/
void
clutter_actor_show (ClutterActor *self)
{
  if (!CLUTTER_ACTOR_IS_VISIBLE (self))
    {
      g_object_ref (self);

      g_signal_emit (self, actor_signals[SHOW], 0);
      g_object_notify (G_OBJECT (self), "visible");

      g_object_unref (self);
    }
}

/**
 * clutter_actor_show_all:
 * @self: a #ClutterActor
 *
 * Call show() on all children of a actor (if any).
 *
 * Since: 0.2
 */
void
clutter_actor_show_all (ClutterActor *self)
{
  ClutterActorClass *klass;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  klass = CLUTTER_ACTOR_GET_CLASS (self);
  if (klass->show_all)
    klass->show_all (self);
}

void
clutter_actor_real_hide (ClutterActor *self)
{
  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    {
      /* see comment in clutter_actor_real_show() on why we don't set
       * the mapped flag on the top-level actors
       */
      if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL))
        CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_MAPPED);

      clutter_actor_queue_redraw (self);
    }
}

/**
 * clutter_actor_hide
 * @self: A #ClutterActor
 *
 * Flags a clutter actor to be hidden. An actor not shown will not
 * appear on the display.
 **/
void
clutter_actor_hide (ClutterActor *self)
{
  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    {
      g_object_ref (self);

      if (CLUTTER_ACTOR_IS_REACTIVE(self))
	; 			/* FIXME: decrease global reactive count */

      g_signal_emit (self, actor_signals[HIDE], 0);
      g_object_notify (G_OBJECT (self), "visible");

      g_object_unref (self);
    }
}

/**
 * clutter_actor_hide_all:
 * @self: a #ClutterActor
 *
 * Call hide() on all child actors (if any).
 *
 * Since: 0.2
 */
void
clutter_actor_hide_all (ClutterActor *self)
{
  ClutterActorClass *klass;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  klass = CLUTTER_ACTOR_GET_CLASS (self);
  if (klass->hide_all)
    klass->hide_all (self);
}

/**
 * clutter_actor_realize
 * @self: A #ClutterActor
 *
 * Creates any underlying graphics resources needed by the actor to be
 * displayed.
 **/
void
clutter_actor_realize (ClutterActor *self)
{
  ClutterActorClass *klass;

  if (CLUTTER_ACTOR_IS_REALIZED (self))
    return;

  CLUTTER_ACTOR_SET_FLAGS (self, CLUTTER_ACTOR_REALIZED);

  klass = CLUTTER_ACTOR_GET_CLASS (self);

  if (klass->realize)
    (klass->realize) (self);
}

/**
 * clutter_actor_unrealize
 * @self: A #ClutterActor
 *
 * Frees up any underlying graphics resources needed by the actor to be
 * displayed.
 **/
void
clutter_actor_unrealize (ClutterActor *self)
{
  ClutterActorClass *klass;

  if (!CLUTTER_ACTOR_IS_REALIZED (self))
    return;

  CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_REALIZED);

  klass = CLUTTER_ACTOR_GET_CLASS (self);

  if (klass->unrealize)
    (klass->unrealize) (self);
}

static void
clutter_actor_real_pick (ClutterActor       *self,
			 const ClutterColor *color)
{
  if (clutter_actor_should_pick_paint (self))
    {
      cogl_color (color);
      cogl_rectangle (0,
		      0,
		      clutter_actor_get_width(self),
		      clutter_actor_get_height(self));
    }
}

/**
 * clutter_actor_pick:
 * @self: A #ClutterActor
 * @color: A #ClutterColor
 *
 * Renders a silhouette of the actor in supplied color. Used internally for
 * mapping pointer events to actors.
 *
 * This function should not never be called directly by applications.
 *
 * Subclasses overiding this method should call
 * clutter_actor_should_pick_paint() to decide if to render there
 * silhouette but in any case should still recursively call pick for
 * any children.
 *
 * Since 0.4
 **/
void
clutter_actor_pick (ClutterActor       *self,
		    const ClutterColor *color)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (color != NULL);

  CLUTTER_ACTOR_GET_CLASS (self)->pick(self, color);
}

/**
 * clutter_actor_should_pick_paint:
 * @self: A #ClutterActor
 *
 * Utility call for subclasses overiding the pick method.
 *
 * This function should not never be called directly by applications.
 *
 * Return value: %TRUE if the actor should paint its silhouette,
 *   %FALSE otherwise
 */
gboolean
clutter_actor_should_pick_paint (ClutterActor *self)
{
  ClutterMainContext *context;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);

  context = clutter_context_get_default ();

  if (CLUTTER_ACTOR_IS_MAPPED (self) &&
      (G_UNLIKELY (context->pick_mode == CLUTTER_PICK_ALL) ||
       CLUTTER_ACTOR_IS_REACTIVE (self)))
    return TRUE;

  return FALSE;
}

/*
 * Utility functions for manipulating transformation matrix
 *
 * Matrix: 4x4 of ClutterFixed
 */
#define M(m,row,col)  (m)[(col) * 4 + (row)]

/* Transform point (x,y,z) by matrix */
static void
mtx_transform (ClutterFixed m[16],
	       ClutterFixed *x, ClutterFixed *y, ClutterFixed *z,
	       ClutterFixed *w)
{
    ClutterFixed _x, _y, _z, _w;
    _x = *x;
    _y = *y;
    _z = *z;
    _w = *w;

    /* We care lot about precission here, so have to use QMUL */
    *x = CFX_QMUL (M (m,0,0), _x) + CFX_QMUL (M (m,0,1), _y) +
	 CFX_QMUL (M (m,0,2), _z) + CFX_QMUL (M (m,0,3), _w);

    *y = CFX_QMUL (M (m,1,0), _x) + CFX_QMUL (M (m,1,1), _y) +
	 CFX_QMUL (M (m,1,2), _z) + CFX_QMUL (M (m,1,3), _w);

    *z = CFX_QMUL (M (m,2,0), _x) + CFX_QMUL (M (m,2,1), _y) +
	 CFX_QMUL (M (m,2,2), _z) + CFX_QMUL (M (m,2,3), _w);

    *w = CFX_QMUL (M (m,3,0), _x) + CFX_QMUL (M (m,3,1), _y) +
	 CFX_QMUL (M (m,3,2), _z) + CFX_QMUL (M (m,3,3), _w);

    /* Specially for Matthew: was going to put a comment here, but could not
     * think of anything at all to say ;)
     */
}

#undef M

/* Applies the transforms associated with this actor and its ancestors,
 * retrieves the resulting OpenGL modelview matrix, and uses the matrix
 * to transform the supplied point
 */
static void
clutter_actor_transform_point (ClutterActor *actor,
			       ClutterUnit  *x,
			       ClutterUnit  *y,
			       ClutterUnit  *z,
			       ClutterUnit  *w)
{
  ClutterFixed           mtx[16];
  ClutterActorPrivate   *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = actor->priv;

  cogl_push_matrix();
  _clutter_actor_apply_modelview_transform_recursive (actor);

  cogl_get_modelview_matrix (mtx);

  mtx_transform (mtx, x, y, z, w);

  cogl_pop_matrix();
}

/* Help macros to scale from OpenGL <-1,1> coordinates system to our
 * X-window based <0,window-size> coordinates
 */
#define MTX_GL_SCALE_X(x,w,v1,v2) (CFX_MUL (((CFX_DIV ((x), (w)) + CFX_ONE) >> 1), (v1)) + (v2))
#define MTX_GL_SCALE_Y(y,w,v1,v2) ((v1) - CFX_MUL (((CFX_DIV ((y), (w)) + CFX_ONE) >> 1), (v1)) + (v2))
#define MTX_GL_SCALE_Z(z,w,v1,v2) (MTX_GL_SCALE_X ((z), (w), (v1), (v2)))

/**
 * clutter_actor_apply_transform_to_point:
 * @self: A #ClutterActor
 * @point: A point as #ClutterVertex
 * @vertex: The translated #ClutterVertex
 *
 * Transforms point in coordinates relative to the actor
 * into screen coordiances with the current actor tranform
 * (i.e. scale, rotation etc)
 *
 * Since: 0.4
 **/
void
clutter_actor_apply_transform_to_point (ClutterActor  *self,
					ClutterVertex *point,
					ClutterVertex *vertex)
{
  ClutterFixed  mtx_p[16];
  ClutterFixed  v[4];
  ClutterFixed  w = CFX_ONE;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  /* First we tranform the point using the OpenGL modelview matrix */
  clutter_actor_transform_point (self, &point->x, &point->y, &point->z, &w);

  cogl_get_projection_matrix (mtx_p);
  cogl_get_viewport (v);

  /* Now, transform it again with the projection matrix */
  mtx_transform (mtx_p, &point->x, &point->y, &point->z, &w);

  /* Finaly translate from OpenGL coords to window coords */
  vertex->x = MTX_GL_SCALE_X (point->x, w, v[2], v[0]);
  vertex->y = MTX_GL_SCALE_Y (point->y, w, v[3], v[1]);
  vertex->z = MTX_GL_SCALE_Z (point->z, w, v[2], v[0]);
}

/* Recursively tranform supplied vertices with the tranform for the current
 * actor and all its ancestors (like clutter_actor_transform_point() but
 * for all the vertices in one go).
 */
static void
clutter_actor_transform_vertices (ClutterActor    * self,
				  ClutterVertex     verts[4],
				  ClutterFixed      w[4])
{
  ClutterFixed           mtx[16];
  ClutterFixed           _x, _y, _z, _w;
  ClutterActorBox        coords;

  /*
   * Need to query coords here, so that we get coorect values for actors that
   * do not modify priv->coords.
   */
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_query_coords (self, &coords);

  cogl_push_matrix();
  _clutter_actor_apply_modelview_transform_recursive (self);

  cogl_get_modelview_matrix (mtx);

  _x = 0;
  _y = 0;
  _z = 0;
  _w = CFX_ONE;

  mtx_transform (mtx, &_x, &_y, &_z, &_w);

  verts[0].x = _x;
  verts[0].y = _y;
  verts[0].z = _z;
  w[0] = _w;

  _x = coords.x2 - coords.x1;
  _y = 0;
  _z = 0;
  _w = CFX_ONE;

  mtx_transform (mtx, &_x, &_y, &_z, &_w);

  verts[1].x = _x;
  verts[1].y = _y;
  verts[1].z = _z;
  w[1] = _w;

  _x = 0;
  _y = coords.y2 - coords.y1;
  _z = 0;
  _w = CFX_ONE;

  mtx_transform (mtx, &_x, &_y, &_z, &_w);

  verts[2].x = _x;
  verts[2].y = _y;
  verts[2].z = _z;
  w[2] = _w;

  _x = coords.x2 - coords.x1;
  _y = coords.y2 - coords.y1;
  _z = 0;
  _w = CFX_ONE;

  mtx_transform (mtx, &_x, &_y, &_z, &_w);

  verts[3].x = _x;
  verts[3].y = _y;
  verts[3].z = _z;
  w[3] = _w;

  cogl_pop_matrix();
}

/**
 * clutter_actor_get_vertices:
 * @self: A #ClutterActor
 * @verts: Pointer to a location of an array of 4 #ClutterVertex where to
 * store the result.
 *
 * Calculates the tranformed screen coordinates of the four corners of
 * the actor; the returned vertices relate to the ClutterActorBox
 * coordinates  as follows:
 *
 *  v[0] contains (x1, y1)
 *  v[1] contains (x2, y1)
 *  v[2] contains (x1, y2)
 *  v[3] contains (x2, y2)
 *
 * Since: 0.4
 **/
void
clutter_actor_get_vertices (ClutterActor    *self,
                            ClutterVertex    verts[4])
{
  ClutterFixed           mtx_p[16];
  ClutterFixed           v[4];
  ClutterFixed           w[4];
  ClutterActorPrivate   *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  clutter_actor_transform_vertices (self, verts, w);

  cogl_get_projection_matrix (mtx_p);
  cogl_get_viewport (v);

  mtx_transform (mtx_p,
		 &verts[0].x,
		 &verts[0].y,
		 &verts[0].z,
		 &w[0]);

  verts[0].x = MTX_GL_SCALE_X (verts[0].x, w[0], v[2], v[0]);
  verts[0].y = MTX_GL_SCALE_Y (verts[0].y, w[0], v[3], v[1]);
  verts[0].z = MTX_GL_SCALE_Z (verts[0].z, w[0], v[2], v[0]);

  mtx_transform (mtx_p,
		 &verts[1].x,
		 &verts[1].y,
		 &verts[1].z,
		 &w[1]);

  verts[1].x = MTX_GL_SCALE_X (verts[1].x, w[1], v[2], v[0]);
  verts[1].y = MTX_GL_SCALE_Y (verts[1].y, w[1], v[3], v[1]);
  verts[1].z = MTX_GL_SCALE_Z (verts[1].z, w[1], v[2], v[0]);

  mtx_transform (mtx_p,
		 &verts[2].x,
		 &verts[2].y,
		 &verts[2].z,
		 &w[2]);

  verts[2].x = MTX_GL_SCALE_X (verts[2].x, w[2], v[2], v[0]);
  verts[2].y = MTX_GL_SCALE_Y (verts[2].y, w[2], v[3], v[1]);
  verts[2].z = MTX_GL_SCALE_Z (verts[2].z, w[2], v[2], v[0]);

  mtx_transform (mtx_p,
		 &verts[3].x,
		 &verts[3].y,
		 &verts[3].z,
		 &w[3]);

  verts[3].x = MTX_GL_SCALE_X (verts[3].x, w[3], v[2], v[0]);
  verts[3].y = MTX_GL_SCALE_Y (verts[3].y, w[3], v[3], v[1]);
  verts[3].z = MTX_GL_SCALE_Z (verts[3].z, w[3], v[2], v[0]);
}

/* Applies the transforms associated with this actor to the
 * OpenGL modelview matrix.
 *
 * This function does not push/pop matrix; it is the responsibility
 * of the caller to do so as appropriate
 */
static void
_clutter_actor_apply_modelview_transform (ClutterActor * self)
{
  ClutterActorPrivate *priv = self->priv;
  ClutterActor        *parent;

  parent = clutter_actor_get_parent (self);

  if (parent != NULL)
    {
      cogl_translatex (CLUTTER_UNITS_TO_FIXED (priv->coords.x1),
		       CLUTTER_UNITS_TO_FIXED (priv->coords.y1),
		       0);
    }

  /*
   * because the rotation involves translations, we must scale before
   * applying the rotations (if we apply the scale after the rotations,
   * the translations included in the rotation are not scaled and so the
   * entire object will move on the screen as a result of rotating it).
   */
  if (priv->scale_x != CFX_ONE ||
      priv->scale_y != CFX_ONE)
    {
      cogl_scale (priv->scale_x, priv->scale_y);
    }

   if (priv->rzang)
    {
      cogl_translatex (CLUTTER_UNITS_TO_FIXED (priv->rzx),
		       CLUTTER_UNITS_TO_FIXED (priv->rzy),
		       0);

      cogl_rotatex (priv->rzang, 0, 0, CFX_ONE);

      cogl_translatex (CLUTTER_UNITS_TO_FIXED (-priv->rzx),
		       CLUTTER_UNITS_TO_FIXED (-priv->rzy),
		       0);
    }

  if (priv->ryang)
    {
      cogl_translatex (CLUTTER_UNITS_TO_FIXED (priv->ryx),
		       0,
		       CLUTTER_UNITS_TO_FIXED (priv->z + priv->ryz));

      cogl_rotatex (priv->ryang, 0, CFX_ONE, 0);

      cogl_translatex (CLUTTER_UNITS_TO_FIXED (-priv->ryx),
		       0,
		       CLUTTER_UNITS_TO_FIXED (-(priv->z + priv->ryz)));
    }

  if (priv->rxang)
    {
      cogl_translatex (0,
		       CLUTTER_UNITS_TO_FIXED (priv->rxy),
		       CLUTTER_UNITS_TO_FIXED (priv->z + priv->rxz));

      cogl_rotatex (priv->rxang, CFX_ONE, 0, 0);

      cogl_translatex (0,
		       CLUTTER_UNITS_TO_FIXED (-priv->rxy),
		       CLUTTER_UNITS_TO_FIXED (-(priv->z + priv->rxz)));
    }

  if (parent && (priv->anchor_x || priv->anchor_y))
    {
      cogl_translatex (CLUTTER_UNITS_TO_FIXED (-priv->anchor_x),
		       CLUTTER_UNITS_TO_FIXED (-priv->anchor_y),
		       0);
    }

  if (priv->z)
    cogl_translatex (0, 0, priv->z);

  if (priv->has_clip)
    cogl_clip_set (CLUTTER_UNITS_TO_FIXED (priv->clip[0]),
                   CLUTTER_UNITS_TO_FIXED (priv->clip[1]),
                   CLUTTER_UNITS_TO_FIXED (priv->clip[2]),
                   CLUTTER_UNITS_TO_FIXED (priv->clip[3]));
}

/* Recursively applies the transforms associated with this actor and
 * its ancestors to the OpenGL modelview matrix.
 *
 * This function does not push/pop matrix; it is the responsibility
 * of the caller to do so as appropriate
 */
static void
_clutter_actor_apply_modelview_transform_recursive (ClutterActor * self)
{
  ClutterActor * parent;

  parent = clutter_actor_get_parent (self);

  if (parent)
    _clutter_actor_apply_modelview_transform_recursive (parent);
  else if (self != clutter_stage_get_default ())
    _clutter_actor_apply_modelview_transform (clutter_stage_get_default());

  _clutter_actor_apply_modelview_transform (self);
}

/**
 * clutter_actor_paint:
 * @self: A #ClutterActor
 *
 * Renders the actor to display.
 *
 * This function should not be called directly by applications instead
 * #clutter_actor_queue_redraw should be used to queue paints.
 **/
void
clutter_actor_paint (ClutterActor *self)
{
  ClutterActorPrivate *priv;
  ClutterActorClass *klass;
  ClutterMainContext *context;
  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  priv = self->priv;

  if (!CLUTTER_ACTOR_IS_REALIZED (self))
    {
      CLUTTER_NOTE (PAINT, "Attempting realize via paint()");
      clutter_actor_realize(self);

      if (!CLUTTER_ACTOR_IS_REALIZED (self))
	{
	  CLUTTER_NOTE (PAINT, "Attempt failed, aborting paint");
	  return;
	}
    }

  context = clutter_context_get_default ();
  klass   = CLUTTER_ACTOR_GET_CLASS (self);

  cogl_push_matrix();

  _clutter_actor_apply_modelview_transform (self);

  if (G_UNLIKELY(context->pick_mode != CLUTTER_PICK_NONE))
    {
      gint         r, g, b;
      ClutterColor col;
      guint32      id;

      id = clutter_actor_get_gid (self);

      cogl_get_bitmasks (&r, &g, &b, NULL);

      /* Encode the actor id into a color, taking into account bpp */
      col.red = ((id >> (g+b)) & (0xff>>(8-r)))<<(8-r);
      col.green = ((id >> b)  & (0xff>>(8-g))) << (8-g);
      col.blue = (id & (0xff>>(8-b)))<<(8-b);
      col.alpha = 0xff;

      /* Actor will then paint silhouette of itself in supplied
       * color.  See clutter_stage_get_actor_at_pos() for where
       * picking is enabled.
       */
      clutter_actor_pick (self, &col);
    }
  else
    {
      clutter_actor_shader_pre_paint (self, FALSE);

      if (G_LIKELY (klass->paint))
         klass->paint (self);

      clutter_actor_shader_post_paint (self);
    }

  if (priv->has_clip)
    cogl_clip_unset();

  cogl_pop_matrix();
}

static void
clutter_actor_real_request_coords (ClutterActor    *self,
                                   ClutterActorBox *box)
{
  self->priv->coords = *box;
}

/**
 * clutter_actor_request_coords:
 * @self: A #ClutterActor
 * @box: A #ClutterActorBox with the new coordinates, in ClutterUnits
 *
 * Requests new untransformed coordinates for the bounding box of
 * a #ClutterActor. The coordinates must be relative to the current
 * parent of the actor.
 *
 * This function should not be called directly by applications;
 * instead, the various position/geometry methods should be used.
 *
 * Note: Actors overriding the ClutterActor::request_coords() virtual
 * function should always chain up to the parent class request_coords()
 * method. Actors should override this function only if they need to
 * recompute some internal state or need to reposition their evental
 * children.
 */
void
clutter_actor_request_coords (ClutterActor    *self,
			      ClutterActorBox *box)
{
  ClutterActorPrivate *priv;
  gboolean x_change, y_change, width_change, height_change;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (box != NULL);

  priv = self->priv;

  /* avoid calling request coords if the coordinates did not change */
  x_change      = (priv->coords.x1 != box->x1);
  y_change      = (priv->coords.y1 != box->y1);
  width_change  = ((priv->coords.x2 - priv->coords.x1) != (box->x2 - box->x1));
  height_change = ((priv->coords.y2 - priv->coords.y1) != (box->y2 - box->y1));

  if (x_change || y_change || width_change || height_change)
    {
      g_object_ref (self);
      g_object_freeze_notify (G_OBJECT (self));

      CLUTTER_ACTOR_GET_CLASS (self)->request_coords (self, box);

      if (CLUTTER_ACTOR_IS_VISIBLE (self))
	clutter_actor_queue_redraw (self);

      if (x_change)
	g_object_notify (G_OBJECT (self), "x");

      if (y_change)
	g_object_notify (G_OBJECT (self), "y");

      if (width_change)
	g_object_notify (G_OBJECT (self), "width");

      if (height_change)
	g_object_notify (G_OBJECT (self), "height");

      g_object_thaw_notify (G_OBJECT (self));
      g_object_unref (self);
    }
}

/**
 * clutter_actor_query_coords:
 * @self: A #ClutterActor
 * @box: A location to store the actors #ClutterActorBox co-ordinates
 *
 * Requests the untransformed co-ordinates (in ClutterUnits) for the
 * #ClutterActor relative to any parent.
 *
 * This function should not be called directly by applications instead
 * the various position/geometry methods should be used.
 **/
void
clutter_actor_query_coords (ClutterActor    *self,
			    ClutterActorBox *box)
{
  ClutterActorClass *klass;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (box != NULL);

  klass = CLUTTER_ACTOR_GET_CLASS (self);

  box->x1 = self->priv->coords.x1;
  box->y1 = self->priv->coords.y1;
  box->x2 = self->priv->coords.x2;
  box->y2 = self->priv->coords.y2;

  if (klass->query_coords)
    {
      /* FIXME: This is kind of a cludge - we pass out *private*
       *        co-ords down to any subclasses so they can modify
       *        we then resync any changes. Needed for group class.
       *        Need to figure out nicer way.
      */
      klass->query_coords(self, box);

      self->priv->coords.x1 = box->x1;
      self->priv->coords.y1 = box->y1;
      self->priv->coords.x2 = box->x2;
      self->priv->coords.y2 = box->y2;
    }
}

static void
clutter_actor_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{

  ClutterActor        *actor;
  ClutterActorPrivate *priv;

  actor = CLUTTER_ACTOR(object);
  priv = actor->priv;

  switch (prop_id)
    {
    case PROP_X:
      clutter_actor_set_x (actor, g_value_get_int (value));
      break;
    case PROP_Y:
      clutter_actor_set_y (actor, g_value_get_int (value));
      break;
    case PROP_WIDTH:
      clutter_actor_set_width (actor, g_value_get_int (value));
      break;
    case PROP_HEIGHT:
      clutter_actor_set_height (actor, g_value_get_int (value));
      break;
    case PROP_DEPTH:
      clutter_actor_set_depth (actor, g_value_get_int (value));
      break;
    case PROP_OPACITY:
      clutter_actor_set_opacity (actor, g_value_get_uchar (value));
      break;
    case PROP_NAME:
      clutter_actor_set_name (actor, g_value_get_string (value));
      break;
    case PROP_VISIBLE:
      if (g_value_get_boolean (value) == TRUE)
	clutter_actor_show (actor);
      else
	clutter_actor_hide (actor);
      break;
    case PROP_SCALE_X:
      clutter_actor_set_scalex
                         (actor,
			  CLUTTER_FLOAT_TO_FIXED (g_value_get_double (value)),
			  priv->scale_y);
      break;
    case PROP_SCALE_Y:
      clutter_actor_set_scalex
                         (actor,
			  priv->scale_x,
			  CLUTTER_FLOAT_TO_FIXED (g_value_get_double (value)));
      break;
    case PROP_CLIP:
      {
        ClutterGeometry *geom = g_value_get_boxed (value);

	clutter_actor_set_clip (actor,
				geom->x, geom->y,
				geom->width, geom->height);
      }
      break;
    case PROP_REACTIVE:
      clutter_actor_set_reactive (actor, g_value_get_boolean (value));
      break;
    case PROP_ROTATION_ANGLE_X:
      clutter_actor_set_rotation (actor,
                                  CLUTTER_X_AXIS,
                                  g_value_get_double (value),
                                  0, priv->rxy, priv->rxz);
      break;
    case PROP_ROTATION_ANGLE_Y:
      clutter_actor_set_rotation (actor,
                                  CLUTTER_Y_AXIS,
                                  g_value_get_double (value),
                                  priv->ryx, 0, priv->ryz);
      break;
    case PROP_ROTATION_ANGLE_Z:
      clutter_actor_set_rotation (actor,
                                  CLUTTER_Z_AXIS,
                                  g_value_get_double (value),
                                  priv->rzx, priv->rzy, 0);
      break;
    case PROP_ROTATION_CENTER_X:
      {
        ClutterVertex *center;

        center = g_value_get_boxed (value);
        clutter_actor_set_rotationx (actor,
                                     CLUTTER_X_AXIS,
                                     priv->rxang,
                                     0,
                                     CLUTTER_UNITS_TO_DEVICE (center->y),
                                     CLUTTER_UNITS_TO_DEVICE (center->z));
      }
      break;
    case PROP_ROTATION_CENTER_Y:
      {
        ClutterVertex *center;

        center = g_value_get_boxed (value);
        clutter_actor_set_rotationx (actor,
                                     CLUTTER_X_AXIS,
                                     priv->ryang,
                                     CLUTTER_UNITS_TO_DEVICE (center->x),
                                     0,
                                     CLUTTER_UNITS_TO_DEVICE (center->z));
      }
      break;
    case PROP_ROTATION_CENTER_Z:
      {
        ClutterVertex *center;

        center = g_value_get_boxed (value);
        clutter_actor_set_rotationx (actor,
                                     CLUTTER_X_AXIS,
                                     priv->rzang,
                                     CLUTTER_UNITS_TO_DEVICE (center->x),
                                     CLUTTER_UNITS_TO_DEVICE (center->y),
                                     0);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_actor_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  ClutterActor        *actor;
  ClutterActorPrivate *priv;

  actor = CLUTTER_ACTOR(object);
  priv = actor->priv;

  switch (prop_id)
    {
    case PROP_X:
      g_value_set_int (value, clutter_actor_get_x (actor));
      break;
    case PROP_Y:
      g_value_set_int (value, clutter_actor_get_y (actor));
      break;
    case PROP_WIDTH:
      g_value_set_int (value, clutter_actor_get_width (actor));
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, clutter_actor_get_height (actor));
      break;
    case PROP_DEPTH:
      g_value_set_int (value, clutter_actor_get_depth (actor));
      break;
    case PROP_OPACITY:
      g_value_set_uchar (value, priv->opacity);
      break;
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value,
		           (CLUTTER_ACTOR_IS_VISIBLE (actor) != FALSE));
      break;
    case PROP_HAS_CLIP:
      g_value_set_boolean (value, priv->has_clip);
      break;
    case PROP_CLIP:
      {
        ClutterGeometry clip = { 0, };

        clip.x      = CLUTTER_UNITS_TO_DEVICE (priv->clip[0]);
        clip.y      = CLUTTER_UNITS_TO_DEVICE (priv->clip[1]);
        clip.width  = CLUTTER_UNITS_TO_DEVICE (priv->clip[2]);
        clip.height = CLUTTER_UNITS_TO_DEVICE (priv->clip[3]);

        g_value_set_boxed (value, &clip);
      }
      break;
    case PROP_SCALE_X:
      g_value_set_double (value, CLUTTER_FIXED_TO_DOUBLE (priv->scale_x));
      break;
    case PROP_SCALE_Y:
      g_value_set_double (value, CLUTTER_FIXED_TO_DOUBLE (priv->scale_y));
      break;
    case PROP_REACTIVE:
      g_value_set_boolean (value, clutter_actor_get_reactive (actor));
      break;
    case PROP_ROTATION_ANGLE_X:
      g_value_set_double (value, CLUTTER_FIXED_TO_DOUBLE (priv->rxang));
      break;
    case PROP_ROTATION_ANGLE_Y:
      g_value_set_double (value, CLUTTER_FIXED_TO_DOUBLE (priv->ryang));
      break;
    case PROP_ROTATION_ANGLE_Z:
      g_value_set_double (value, CLUTTER_FIXED_TO_DOUBLE (priv->rzang));
      break;
    case PROP_ROTATION_CENTER_X:
      {
        ClutterVertex center = { 0, };

        center.y = priv->rxy;
        center.z = priv->rxz;

        g_value_set_boxed (value, &center);
      }
      break;
    case PROP_ROTATION_CENTER_Y:
      {
        ClutterVertex center = { 0, };

        center.x = priv->ryx;
        center.z = priv->ryz;

        g_value_set_boxed (value, &center);
      }
      break;
    case PROP_ROTATION_CENTER_Z:
      {
        ClutterVertex center = { 0, };

        center.x = priv->rzx;
        center.y = priv->rzy;

        g_value_set_boxed (value, &center);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_actor_dispose (GObject *object)
{
  ClutterActor *self = CLUTTER_ACTOR (object);

  CLUTTER_NOTE (MISC, "Disposing of object (id=%d) of type `%s' (ref_count:%d)",
		self->priv->id,
		g_type_name (G_OBJECT_TYPE (self)),
                object->ref_count);

  destroy_shader_data (self);

 if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_DESTRUCTION))
    {
      CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IN_DESTRUCTION);

      g_signal_emit (self, actor_signals[DESTROY], 0);

      CLUTTER_UNSET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IN_DESTRUCTION);
    }

  G_OBJECT_CLASS (clutter_actor_parent_class)->dispose (object);
}

static void
clutter_actor_finalize (GObject *object)
{
  ClutterActor *actor = CLUTTER_ACTOR (object);

  CLUTTER_NOTE (MISC, "Finalize object (id=%d) of type `%s'",
		actor->priv->id,
		g_type_name (G_OBJECT_TYPE (actor)));

  g_free (actor->priv->name);

  G_OBJECT_CLASS (clutter_actor_parent_class)->finalize (object);
}

static void
clutter_actor_class_init (ClutterActorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = clutter_actor_set_property;
  object_class->get_property = clutter_actor_get_property;
  object_class->dispose      = clutter_actor_dispose;
  object_class->finalize     = clutter_actor_finalize;

  g_type_class_add_private (klass, sizeof (ClutterActorPrivate));

  /**
   * ClutterActor:x:
   *
   * X coordinate of the actor.
   */
  g_object_class_install_property (object_class,
                                   PROP_X,
                                   g_param_spec_int ("x",
                                                     "X co-ord",
                                                     "X co-ord of actor",
                                                     -G_MAXINT, G_MAXINT,
                                                     0,
                                                     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor:y:
   *
   * Y coordinate of the actor.
   */
  g_object_class_install_property (object_class,
                                   PROP_Y,
                                   g_param_spec_int ("y",
                                                     "Y co-ord",
                                                     "Y co-ord of actor",
                                                     -G_MAXINT, G_MAXINT,
                                                     0,
                                                     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor:width:
   *
   * Width of the actor (in pixels).
   */
  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_int ("width",
                                                     "Width",
                                                     "Width of actor in pixels",
                                                     0, G_MAXINT,
                                                     0,
                                                     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor:height:
   *
   * Height of the actor (in pixels).
   */
  g_object_class_install_property (object_class,
                                   PROP_HEIGHT,
                                   g_param_spec_int ("height",
                                                     "Height",
                                                     "Height of actor in pixels",
                                                     0, G_MAXINT,
                                                     0,
                                                     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor:depth:
   *
   * Depth of the actor.
   *
   * Since: 0.6
   */
  g_object_class_install_property (object_class,
                                   PROP_DEPTH,
                                   g_param_spec_int ("depth",
                                                     "Depth",
                                                     "Depth of actor",
                                                     -G_MAXINT, G_MAXINT,
                                                     0,
                                                     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor:opacity:
   *
   * Opacity of the actor.
   */
  g_object_class_install_property (object_class,
                                   PROP_OPACITY,
                                   g_param_spec_uchar ("opacity",
                                                       "Opacity",
                                                       "Opacity of actor",
                                                       0, 0xff,
                                                       0xff,
                                                       CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor:visible:
   *
   * Whether the actor is visible or not.
   */
  g_object_class_install_property (object_class,
                                   PROP_VISIBLE,
                                   g_param_spec_boolean ("visible",
                                                         "Visible",
                                                         "Whether the actor is visible or not",
                                                         FALSE,
                                                         CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor:reactive:
   *
   * Whether the actor is reactive to events or not.
   *
   * Since: 0.6
   */
  g_object_class_install_property (object_class,
                                   PROP_REACTIVE,
                                   g_param_spec_boolean ("reactive",
                                                         "Reactive",
                                                         "Whether the actor is reactive to events or not",
                                                         FALSE,
                                                         CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor:has-clip:
   *
   * Whether the actor has the clip property set or not.
   */
  g_object_class_install_property (object_class,
                                   PROP_HAS_CLIP,
                                   g_param_spec_boolean ("has-clip",
                                                         "Has Clip",
                                                         "Whether the actor has a clip set or not",
                                                         FALSE,
                                                         CLUTTER_PARAM_READABLE));
  /**
   * ClutterActor:clip:
   *
   * The clip region for the actor.
   */
  g_object_class_install_property (object_class,
                                   PROP_CLIP,
                                   g_param_spec_boxed ("clip",
                                                       "Clip",
                                                       "The clip region for the actor",
                                                       CLUTTER_TYPE_GEOMETRY,
                                                       CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor:name:
   *
   * The name of the actor.
   *
   * Since: 0.2
   */
  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Name of the actor",
                                                        NULL,
                                                        CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor::scale-x:
   *
   * The horizontal scale of the actor
   *
   * Since: 0.6
   */
  g_object_class_install_property
                       (object_class,
			PROP_SCALE_X,
			g_param_spec_double ("scale-x",
					     "Scale-X",
					     "Scale X",
					     0.0,
					     G_MAXDOUBLE,
					     1.0,
					     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor::scale-y:
   *
   * The vertical scale of the actor
   *
   * Since: 0.6
   */
  g_object_class_install_property
                       (object_class,
			PROP_SCALE_Y,
			g_param_spec_double ("scale-y",
					     "Scale-Y",
					     "Scale Y",
					     0.0,
					     G_MAXDOUBLE,
					     1.0,
					     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor::rotation-angle-x:
   *
   * The rotation angle on the X axis.
   *
   * Since: 0.6
   */
  g_object_class_install_property
                       (object_class,
			PROP_ROTATION_ANGLE_X,
			g_param_spec_double ("rotation-angle-x",
					     "Rotation Angle X",
					     "The rotation angle on the X axis",
					     0.0,
					     G_MAXDOUBLE,
					     0.0,
					     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor::rotation-angle-y:
   *
   * The rotation angle on the Y axis.
   *
   * Since: 0.6
   */
  g_object_class_install_property
                       (object_class,
			PROP_ROTATION_ANGLE_Y,
			g_param_spec_double ("rotation-angle-y",
					     "Rotation Angle Y",
					     "The rotation angle on the Y axis",
					     0.0,
					     G_MAXDOUBLE,
					     0.0,
					     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor::rotation-angle-z:
   *
   * The rotation angle on the Z axis.
   *
   * Since: 0.6
   */
  g_object_class_install_property
                       (object_class,
			PROP_ROTATION_ANGLE_Z,
			g_param_spec_double ("rotation-angle-z",
					     "Rotation Angle Z",
					     "The rotation angle on the Z axis",
					     0.0,
					     G_MAXDOUBLE,
					     0.0,
					     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor::rotation-center-x:
   *
   * The rotation center on the X axis.
   *
   * Since: 0.6
   */
  g_object_class_install_property
                       (object_class,
			PROP_ROTATION_CENTER_X,
			g_param_spec_boxed ("rotation-center-x",
					    "Rotation Center X",
					    "The rotation center on the X axis",
					    CLUTTER_TYPE_VERTEX,
					    CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor::rotation-center-y:
   *
   * The rotation center on the Y axis.
   *
   * Since: 0.6
   */
  g_object_class_install_property
                       (object_class,
			PROP_ROTATION_CENTER_Y,
			g_param_spec_boxed ("rotation-center-y",
					    "Rotation Center Y",
					    "The rotation center on the Y axis",
					    CLUTTER_TYPE_VERTEX,
					    CLUTTER_PARAM_READWRITE));
  /**
   * ClutterActor::rotation-center-z:
   *
   * The rotation center on the Z axis.
   *
   * Since: 0.6
   */
  g_object_class_install_property
                       (object_class,
			PROP_ROTATION_CENTER_Z,
			g_param_spec_boxed ("rotation-center-z",
					    "Rotation Center Z",
					    "The rotation center on the Z axis",
					    CLUTTER_TYPE_VERTEX,
					    CLUTTER_PARAM_READWRITE));

  /**
   * ClutterActor::destroy:
   * @actor: the object which received the signal
   *
   * The ::destroy signal is emitted when an actor is destroyed,
   * either by direct invocation of clutter_actor_destroy() or
   * when the #ClutterGroup that contains the actor is destroyed.
   *
   * Since: 0.2
   */
  actor_signals[DESTROY] =
    g_signal_new ("destroy",
		  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		  G_STRUCT_OFFSET (ClutterActorClass, destroy),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterActor::show:
   * @actor: the object which received the signal
   *
   * The ::show signal is emitted when an actor becomes visible.
   *
   * Since: 0.2
   */
  actor_signals[SHOW] =
    g_signal_new ("show",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ClutterActorClass, show),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterActor::hide:
   * @actor: the object which received the signal
   *
   * The ::hide signal is emitted when an actor is no longer visible.
   *
   * Since: 0.2
   */
  actor_signals[HIDE] =
    g_signal_new ("hide",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ClutterActorClass, hide),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  /**
   * ClutterActor::parent-set:
   * @actor: the object which received the signal
   * @old_parent: the previous parent of the actor, or %NULL
   *
   * This signal is emitted when the parent of the actor changes.
   *
   * Since: 0.2
   */
  actor_signals[PARENT_SET] =
    g_signal_new ("parent-set",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterActorClass, parent_set),
                  NULL, NULL,
                  clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);

  /**
   * ClutterActor::event:
   * @actor: the actor which received the event
   * @event: a #ClutterEvent
   *
   * The ::event signal is emitted each time and event is received
   * by the @actor. This signal will be emitted on every actor,
   * following the hierarchy chain, until it reaches the top-level
   * container (the #ClutterStage).
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[EVENT] =
    g_signal_new ("event",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ClutterActor::button-press-event:
   * @actor: the actor which received the event
   * @event: a #ClutterButtonEvent
   *
   * The ::button-press-event signal is emitted each time a mouse button
   * is pressed on @actor.
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[BUTTON_PRESS_EVENT] =
    g_signal_new ("button-press-event",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, button_press_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ClutterActor::button-release-event:
   * @actor: the actor which received the event
   * @event: a #ClutterButtonEvent
   *
   * The ::button-release-event signal is emitted each time a mouse button
   * is released on @actor.
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[BUTTON_RELEASE_EVENT] =
    g_signal_new ("button-release-event",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, button_release_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ClutterActor::scroll-event:
   * @actor: the actor which received the event
   * @event: a #ClutterScrollEvent
   *
   * The ::scroll-event signal is emitted each time a the mouse is
   * scrolled on @actor
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[SCROLL_EVENT] =
    g_signal_new ("scroll-event",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, scroll_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ClutterActor::key-press-event:
   * @actor: the actor which received the event
   * @event: a #ClutterKeyEvent
   *
   * The ::key-press-event signal is emitted each time a keyboard button
   * is pressed on @actor.
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[KEY_PRESS_EVENT] =
    g_signal_new ("key-press-event",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, key_press_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ClutterActor::key-release-event:
   * @actor: the actor which received the event
   * @event: a #ClutterKeyEvent
   *
   * The ::key-release-event signal is emitted each time a keyboard button
   * is released on @actor.
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[KEY_RELEASE_EVENT] =
    g_signal_new ("key-release-event",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, key_release_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * ClutterActor::motion-event:
   * @actor: the actor which received the event
   * @event: a #ClutterMotionEvent
   *
   * The ::motion-event signal is emitted each time the mouse pointer is
   * moved on @actor.
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[MOTION_EVENT] =
    g_signal_new ("motion-event",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, motion_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * ClutterActor::focus-in:
   * @actor: the actor which now has key focus
   *
   * The ::focus-in signal is emitted when @actor recieves key focus.
   *
   * Since: 0.6
   */
  actor_signals[FOCUS_IN] =
    g_signal_new ("focus-in",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, focus_in),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * ClutterActor::focus-out:
   * @actor: the actor which now has key focus
   *
   * The ::focus-out signal is emitted when @actor loses key focus.
   *
   * Since: 0.6
   */
  actor_signals[FOCUS_OUT] =
    g_signal_new ("focus-out",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, focus_out),
		  NULL, NULL,
		  clutter_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * ClutterActor::enter-event:
   * @actor: the actor which the pointer has entered.
   * @event: a #ClutterCrossingEvent
   *
   * The ::enter-event signal is emitted when the pointer enters the @actor
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[ENTER_EVENT] =
    g_signal_new ("enter-event",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, enter_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * ClutterActor::leave-event:
   * @actor: the actor which the pointer has left
   * @event: a #ClutterCrossingEvent
   *
   * The ::leave-event signal is emitted when the pointer leaves the @actor.
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[LEAVE_EVENT] =
    g_signal_new ("leave-event",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, leave_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * ClutterActor::captured-event:
   * @actor: the actor which received the signal
   * @event: a #ClutterEvent
   *
   * The ::captured-event signal is emitted when an event is captured
   * by Clutter. This signal will be emitted starting from the top-level
   * container (the #ClutterStage) to the actor which received the event
   * going down the hierarchy. This signal can be used to intercept every
   * event before the specialized events (like
   * ClutterActor::button-press-event or ::key-released-event) are
   * emitted.
   *
   * Return value: %TRUE if the event has been handled by the actor,
   *   or %FALSE to continue the emission.
   *
   * Since: 0.6
   */
  actor_signals[CAPTURED_EVENT] =
    g_signal_new ("captured-event",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterActorClass, captured_event),
		  _clutter_boolean_handled_accumulator, NULL,
		  clutter_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  klass->show = clutter_actor_real_show;
  klass->show_all = clutter_actor_show;
  klass->hide = clutter_actor_real_hide;
  klass->hide_all = clutter_actor_hide;
  klass->pick = clutter_actor_real_pick;
  klass->request_coords = clutter_actor_real_request_coords;
}

static void
clutter_actor_init (ClutterActor *self)
{
  ClutterActorPrivate *priv;
  ClutterActorBox box = { 0, };

  self->priv = priv = CLUTTER_ACTOR_GET_PRIVATE (self);

  priv->parent_actor = NULL;
  priv->has_clip     = FALSE;
  priv->opacity      = 0xff;
  priv->id           = __id++;
  priv->scale_x      = CFX_ONE;
  priv->scale_y      = CFX_ONE;
  priv->shader_data     = NULL;

  memset (priv->clip, 0, sizeof (ClutterUnit) * 4);

  clutter_actor_request_coords (self, &box);
}

/**
 * clutter_actor_destroy:
 * @self: a #ClutterActor
 *
 * Destroys an actor.  When an actor is destroyed, it will break any
 * references it holds to other objects.  If the actor is inside a
 * container, the actor will be removed.
 *
 * When you destroy a container its children will be destroyed as well.
 *
 * Note: you cannot destroy the #ClutterStage returned by
 * clutter_stage_get_default().
 */
void
clutter_actor_destroy (ClutterActor *self)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
    {
      g_warning ("Calling clutter_actor_destroy() on an actor of type `%s' "
                 "is not possible. This is usually an application bug.",
                 g_type_name (G_OBJECT_TYPE (self)));
      return;
    }

  priv = self->priv;

  if (priv->parent_actor)
    {
      ClutterActor *parent = priv->parent_actor;

      if (CLUTTER_IS_CONTAINER (parent))
        {
          g_object_ref (self);
          clutter_container_remove_actor (CLUTTER_CONTAINER (parent), self);
        }
      else
        priv->parent_actor = NULL;
    }

  if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_DESTRUCTION))
    g_object_run_dispose (G_OBJECT (self));

  g_object_unref (self);
}

/**
 * clutter_actor_queue_redraw:
 * @self: A #ClutterActor
 *
 * Queues up a redraw of an actor and any children. The redraw occurs
 * once the main loop becomes idle (after the current batch of events
 * has been processed, roughly).
 *
 * Applications rarely need to call this as redraws are handled automatically
 * by modification functions.
 */
void
clutter_actor_queue_redraw (ClutterActor *self)
{
  ClutterMainContext *ctx = CLUTTER_CONTEXT();

  if (!ctx->update_idle)
    {
      CLUTTER_TIMESTAMP (SCHEDULER, "Adding idle source for actor: %p", self);

      ctx->update_idle =
        clutter_threads_add_idle_full (G_PRIORITY_DEFAULT + 10,
                                       redraw_update_idle,
                                       NULL, NULL);
    }
}

/**
 * clutter_actor_set_geometry:
 * @self: A #ClutterActor
 * @geometry: A #ClutterGeometry
 *
 * Sets the actors untransformed geometry in pixels relative to any
 * parent actor.
 */
void
clutter_actor_set_geometry (ClutterActor          *self,
			    const ClutterGeometry *geometry)
{
  ClutterActorBox box;

  box.x1 = CLUTTER_UNITS_FROM_INT (geometry->x);
  box.y1 = CLUTTER_UNITS_FROM_INT (geometry->y);
  box.x2 = CLUTTER_UNITS_FROM_INT (geometry->x + geometry->width);
  box.y2 = CLUTTER_UNITS_FROM_INT (geometry->y + geometry->height);

  clutter_actor_request_coords (self, &box);
}

/**
 * clutter_actor_get_geometry:
 * @self: A #ClutterActor
 * @geometry: A location to store actors #ClutterGeometry
 *
 * Gets the actors untransformed geometry in pixels relative to any
 * parent actor.
 */
void
clutter_actor_get_geometry (ClutterActor    *self,
			    ClutterGeometry *geometry)
{
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_query_coords (self, &box);

  geometry->x      = CLUTTER_UNITS_TO_DEVICE (box.x1);
  geometry->y      = CLUTTER_UNITS_TO_DEVICE (box.y1);
  geometry->width  = CLUTTER_UNITS_TO_DEVICE (box.x2 - box.x1);
  geometry->height = CLUTTER_UNITS_TO_DEVICE (box.y2 - box.y1);
}

/**
 * clutter_actor_get_coords:
 * @self: A #ClutterActor
 * @x_1: A location to store actors left position, or %NULL.
 * @y_1: A location to store actors top position, or %NULL.
 * @x_2: A location to store actors right position, or %NULL.
 * @y_2: A location to store actors bottom position, or %NULL.
 *
 * Gets the actors untransformed bounding rectangle co-ordinates in pixels
 * relative to any parent actor.
 */
void
clutter_actor_get_coords (ClutterActor *self,
			  gint         *x_1,
			  gint         *y_1,
			  gint         *x_2,
			  gint         *y_2)
{
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_query_coords (self, &box);

  if (x_1)
    *x_1 = CLUTTER_UNITS_TO_DEVICE (box.x1);

  if (y_1)
    *y_1 = CLUTTER_UNITS_TO_DEVICE (box.y1);

  if (x_2)
    *x_2 = CLUTTER_UNITS_TO_DEVICE (box.x2);

  if (y_2)
    *y_2 = CLUTTER_UNITS_TO_DEVICE (box.y2);
}

/**
 * clutter_actor_set_position
 * @self: A #ClutterActor
 * @x: New left position of actor in pixels.
 * @y: New top position of actor in pixels.
 *
 * Sets the actors position in pixels relative to any
 * parent actor.
 */
void
clutter_actor_set_position (ClutterActor *self,
			    gint          x,
			    gint          y)
{
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_query_coords (self, &box);

  box.x2 += (CLUTTER_UNITS_FROM_INT (x) - box.x1);
  box.y2 += (CLUTTER_UNITS_FROM_INT (y) - box.y1);

  box.x1 = CLUTTER_UNITS_FROM_INT (x);
  box.y1 = CLUTTER_UNITS_FROM_INT (y);

  clutter_actor_request_coords (self, &box);
}

/**
 * clutter_actor_set_positionu
 * @self: A #ClutterActor
 * @x: New left position of actor in #ClutterUnit
 * @y: New top position of actor in #ClutterUnit
 *
 * Sets the actors position in #ClutterUnit relative to any
 * parent actor.
 *
 * Since: 0.6
 */
void
clutter_actor_set_positionu (ClutterActor *self,
			     ClutterUnit   x,
			     ClutterUnit   y)
{
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_query_coords (self, &box);

  box.x2 += (x - box.x1);
  box.y2 += (y - box.y1);

  box.x1 = x;
  box.y1 = y;

  clutter_actor_request_coords (self, &box);
}

/**
 * clutter_actor_move_by:
 * @self: A #ClutterActor
 * @dx: Distance to move Actor on X axis.
 * @dy: Distance to move Actor on Y axis.
 *
 * Moves an actor by specified distance relative to
 * current position in pixels.
 *
 * Since: 0.2
 */
void
clutter_actor_move_by (ClutterActor *self,
		       gint          dx,
		       gint          dy)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_move_byu (self,
                          CLUTTER_UNITS_FROM_DEVICE (dx),
                          CLUTTER_UNITS_FROM_DEVICE (dy));
}

/**
 * clutter_actor_move_byu:
 * @self: A #ClutterActor
 * @dx: Distance to move Actor on X axis, in #ClutterUnit<!-- -->s.
 * @dy: Distance to move Actor on Y axis, in #ClutterUnit<!-- -->s.
 *
 * Moves an actor by specified distance relative to the current position.
 *
 * Since: 0.6
 */
void
clutter_actor_move_byu (ClutterActor *self,
                        ClutterUnit   dx,
                        ClutterUnit   dy)
{
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_query_coords (self, &box);

  box.x2 += dx;
  box.y2 += dy;
  box.x1 += dx;
  box.y1 += dy;

  clutter_actor_request_coords (self, &box);
}

/* local inline version, without type checking to be used by
 * set_width() and set_height(). if one of the dimensions is < 0
 * it will not be changed
 */
static inline void
clutter_actor_set_size_internal (ClutterActor *self,
                                 gint          width,
                                 gint          height)
{
  ClutterActorBox box;

  clutter_actor_query_coords (self, &box);

  if (width > 0)
    box.x2 = box.x1 + CLUTTER_UNITS_FROM_INT (width);

  if (height > 0)
    box.y2 = box.y1 + CLUTTER_UNITS_FROM_INT (height);

  clutter_actor_request_coords (self, &box);
}

/* local inline unit version, without type checking to be used by
 * set_width() and set_height(). if one of the dimensions is < 0
 * it will not be changed
 */
static inline void
clutter_actor_set_size_internalu (ClutterActor *self,
				  ClutterUnit   width,
                                  ClutterUnit   height)
{
  ClutterActorBox box;

  clutter_actor_query_coords (self, &box);

  if (width > 0)
    box.x2 = box.x1 + width;

  if (height > 0)
    box.y2 = box.y1 + height;

  clutter_actor_request_coords (self, &box);
}

/**
 * clutter_actor_set_size
 * @self: A #ClutterActor
 * @width: New width of actor in pixels, or -1
 * @height: New height of actor in pixels, or -1
 *
 * Sets the actors size in pixels. If @width and/or @height are -1 the
 * actor will assume the same size of its bounding box.
 */
void
clutter_actor_set_size (ClutterActor *self,
			gint          width,
			gint          height)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_size_internal (self, width, height);
}

/**
 * clutter_actor_set_sizeu
 * @self: A #ClutterActor
 * @width: New width of actor in #ClutterUnit, or -1
 * @height: New height of actor in #ClutterUnit, or -1
 *
 * Sets the actors size in #ClutterUnit. If @width and/or @height are -1 the
 * actor will assume the same size of its bounding box.
 *
 * Since: 0.6
 */
void
clutter_actor_set_sizeu (ClutterActor *self,
			 ClutterUnit   width,
			 ClutterUnit   height)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_size_internalu (self, width, height);
}

/**
 * clutter_actor_get_size:
 * @self: A #ClutterActor
 * @width: Location to store width if non NULL.
 * @height: Location to store height if non NULL.
 *
 * Gets the size of an actor in pixels ignoring any scaling factors.
 *
 * Since: 0.2
 */
void
clutter_actor_get_size (ClutterActor *self,
			guint        *width,
			guint        *height)
{
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_query_coords (self, &box);

  if (width)
    *width = CLUTTER_UNITS_TO_DEVICE (box.x2 - box.x1);

  if (height)
    *height = CLUTTER_UNITS_TO_DEVICE (box.y2 - box.y1);
}

/**
 * clutter_actor_get_sizeu:
 * @self: A #ClutterActor
 * @width: Location to store width if non NULL.
 * @height: Location to store height if non NULL.
 *
 * Gets the size of an actor in #ClutterUnit<!-- -->s ignoring any scaling
 * factors.
 *
 * Since: 0.6
 */
void
clutter_actor_get_sizeu (ClutterActor *self,
                         ClutterUnit  *width,
                         ClutterUnit  *height)
{
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_query_coords (self, &box);

  if (width)
    *width = box.x2 - box.x1;

  if (height)
    *height = box.y2 - box.y1;
}

/**
 * clutter_actor_get_position:
 * @self: a #ClutterActor
 * @x: return location for the X coordinate, or %NULL
 * @y: return location for the Y coordinate, or %NULL
 *
 * Retrieves the position of an actor.
 *
 * Since: 0.6
 */
void
clutter_actor_get_position (ClutterActor *self,
                            gint         *x,
                            gint         *y)
{
  ClutterActorBox box = { 0, };

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_query_coords (self, &box);

  if (x)
    *x = CLUTTER_UNITS_TO_DEVICE (box.x1);

  if (y)
    *y = CLUTTER_UNITS_TO_DEVICE (box.y1);
}

/**
 * clutter_actor_get_positionu:
 * @self: a #ClutterActor
 * @x: return location for the X coordinate, or %NULL
 * @y: return location for the Y coordinate, or %NULL
 *
 * Retrieves the position of an actor in #ClutterUnit<!-- -->s.
 *
 * Since: 0.6
 */
void
clutter_actor_get_positionu (ClutterActor *self,
                             ClutterUnit  *x,
                             ClutterUnit  *y)
{
  ClutterActorBox box = { 0, };

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_query_coords (self, &box);

  if (x)
    *x = box.x1;

  if (y)
    *y = box.y1;
}

/*
 * clutter_actor_get_abs_position_units
 * @self: A #ClutterActor
 * @x: Location to store x position if non NULL.
 * @y: Location to store y position if non NULL.
 *
 * Gets the absolute position of an actor in clutter units relative
 * to the stage.
 *
 * Since: 0.4
 */
static void
clutter_actor_get_abs_position_units (ClutterActor *self,
				      gint32       *x,
				      gint32       *y)
{
  ClutterVertex v1;
  ClutterVertex v2;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  v1.x = v1.y = v1.z = 0;
  clutter_actor_apply_transform_to_point (self, &v1, &v2);

  if (x)
    *x = v2.x;
  if (y)
    *y = v2.y;
}

/**
 * clutter_actor_get_abs_position
 * @self: A #ClutterActor
 * @x: Location to store x position if non NULL.
 * @y: Location to store y position if non NULL.
 *
 * Gets the absolute position of an actor in pixels relative
 * to the stage.
 */
void
clutter_actor_get_abs_position (ClutterActor *self,
				gint         *x,
				gint         *y)
{
  ClutterUnit xu, yu;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  xu = yu = 0;
  clutter_actor_get_abs_position_units (self, &xu, &yu);

  if (x)
    *x = CLUTTER_UNITS_TO_DEVICE (xu);
  if (y)
    *y = CLUTTER_UNITS_TO_DEVICE (yu);
}

/*
 * clutter_actor_get_abs_size_units:
 * @self: A #ClutterActor
 * @width: Location to store width if non NULL.
 * @height: Location to store height if non NULL.
 *
 * Gets the absolute size of an actor in clutter units taking into account
 * an scaling factors.
 *
 * Note: When the actor (or one of its ancestors) is rotated around the x or y
 * axis, it no longer appears as on the stage as a rectangle, but as a generic
 * quadrangle; in that case this function returns the size of the smallest
 * rectangle that encapsulates the entire quad. Please note that in this case
 * no assumptions can be made about the relative position of this envelope to
 * the absolute position of the actor, as returned by
 * clutter_actor_get_abs_position() - if you need this information, you need
 * to use clutter_actor_get_vertices() to get the coords of the actual
 * quadrangle.
 *
 * Since: 0.4
 */
static void
clutter_actor_get_abs_size_units (ClutterActor *self,
				  gint32       *width,
				  gint32       *height)
{
  ClutterVertex v[4];
  ClutterFixed  x_min, x_max, y_min, y_max;
  gint i;

  clutter_actor_get_vertices (self, v);

  x_min = x_max = v[0].x;
  y_min = y_max = v[0].y;

  for (i = 1; i < sizeof(v)/sizeof(v[0]); ++i)
    {
      if (v[i].x < x_min)
	x_min = v[i].x;

      if (v[i].x > x_max)
	x_max = v[i].x;

      if (v[i].y < y_min)
	y_min = v[i].y;

      if (v[i].y > y_max)
	y_max = v[i].y;
    }

  *width  = x_max - x_min;
  *height = y_max - y_min;
}

/**
 * clutter_actor_get_abs_size:
 * @self: A #ClutterActor
 * @width: Location to store width if non NULL.
 * @height: Location to store height if non NULL.
 *
 * Gets the absolute size of an actor taking into account
 * an scaling factors
 */
void
clutter_actor_get_abs_size (ClutterActor *self,
			    guint        *width,
			    guint        *height)
{
  gint32 wu, hu;
  clutter_actor_get_abs_size_units (self, &wu, &hu);

  *width  = CLUTTER_UNITS_TO_DEVICE (wu);
  *height = CLUTTER_UNITS_TO_DEVICE (hu);
}


/**
 * clutter_actor_get_width
 * @self: A #ClutterActor
 *
 * Retrieves the actors width ignoring any scaling factors.
 *
 * Return value: The actor width in pixels
 **/
guint
clutter_actor_get_width (ClutterActor *self)
{
  ClutterActorBox box;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  clutter_actor_query_coords (self, &box);

  return CLUTTER_UNITS_TO_DEVICE (box.x2 - box.x1);
}

/**
 * clutter_actor_get_widthu
 * @self: A #ClutterActor
 *
 * Retrieves the actors width ignoring any scaling factors.
 *
 * Return value: The actor width in #ClutterUnit
 *
 * since: 0.6
 **/
ClutterUnit
clutter_actor_get_widthu (ClutterActor *self)
{
  ClutterActorBox box;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  clutter_actor_query_coords (self, &box);

  return box.x2 - box.x1;
}

/**
 * clutter_actor_get_height
 * @self: A #ClutterActor
 *
 * Retrieves the actors height ignoring any scaling factors.
 *
 * Return value: The actor height in pixels
 **/
guint
clutter_actor_get_height (ClutterActor *self)
{
  ClutterActorBox box;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  clutter_actor_query_coords (self, &box);

  return CLUTTER_UNITS_TO_DEVICE (box.y2 - box.y1);
}

/**
 * clutter_actor_get_heightu
 * @self: A #ClutterActor
 *
 * Retrieves the actors height ignoring any scaling factors.
 *
 * Return value: The actor height in #ClutterUnit
 *
 * since: 0.6
 **/
ClutterUnit
clutter_actor_get_heightu (ClutterActor *self)
{
  ClutterActorBox box;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  clutter_actor_query_coords (self, &box);

  return box.y2 - box.y1;
}

/**
 * clutter_actor_set_width
 * @self: A #ClutterActor
 * @width: Requested new width for actor
 *
 * Requests a new width for actor
 *
 * since: 0.2
 **/
void
clutter_actor_set_width (ClutterActor *self,
                         guint         width)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_size_internal (self, width, -1);
}

/**
 * clutter_actor_set_widthu
 * @self: A #ClutterActor
 * @width: Requested new width for actor in #ClutterUnit
 *
 * Requests a new width for actor
 *
 * since: 0.6
 **/
void
clutter_actor_set_widthu (ClutterActor *self,
                          ClutterUnit   width)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_size_internalu (self, width, -1);
}

/**
 * clutter_actor_set_height
 * @self: A #ClutterActor
 * @height: Requested new height for actor
 *
 * Requests a new height for actor
 *
 * since: 0.2
 **/
void
clutter_actor_set_height (ClutterActor *self,
                          guint         height)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_size_internal (self, -1, height);
}

/**
 * clutter_actor_set_heightu
 * @self: A #ClutterActor
 * @height: Requested new height for actor in #ClutterUnit
 *
 * Requests a new height for actor
 *
 * since: 0.6
 **/
void
clutter_actor_set_heightu (ClutterActor *self,
                           ClutterUnit   height)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_size_internalu (self, -1, height);
}

/**
 * clutter_actor_set_x:
 * @self: a #ClutterActor
 * @x: the actors position on the X axis
 *
 * Sets the actor's x position relative to its parent.
 *
 * Since: 0.6
 */
void
clutter_actor_set_x (ClutterActor *self,
                     gint          x)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_position (self,
                              x,
                              clutter_actor_get_y (self));
}

/**
 * clutter_actor_set_xu:
 * @self: a #ClutterActor
 * @x: the actors position on the X axis in #ClutterUnit
 *
 * Sets the actor's x position relative to its parent.
 *
 * Since: 0.6
 */
void
clutter_actor_set_xu (ClutterActor *self,
		      ClutterUnit   x)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_positionu (self,
			       x,
			       clutter_actor_get_yu (self));
}

/**
 * clutter_actor_set_y:
 * @self: a #ClutterActor
 * @y: the actors position on the Y axis
 *
 * Sets the actor's y position relative to its parent.
 *
 * Since: 0.6
 */
void
clutter_actor_set_y (ClutterActor *self,
                     gint          y)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_position (self,
                              clutter_actor_get_x (self),
                              y);
}

/**
 * clutter_actor_set_yu:
 * @self: a #ClutterActor
 * @y: the actors position on the Y axis in #ClutterUnit
 *
 * Sets the actor's y position relative to its parent.
 *
 * Since: 0.6
 */
void
clutter_actor_set_yu (ClutterActor *self,
		      ClutterUnit   y)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_positionu (self,
			       clutter_actor_get_xu (self),
			       y);
}

/**
 * clutter_actor_get_x
 * @self: A #ClutterActor
 *
 * Retrieves the actors x position relative to any parent.
 *
 * Return value: The actor x position in pixels ignoring any tranforms
 * (i.e scaling, rotation).
 **/
gint
clutter_actor_get_x (ClutterActor *self)
{
  ClutterActorBox box;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  clutter_actor_query_coords (self, &box);

  return CLUTTER_UNITS_TO_DEVICE (box.x1);
}

/**
 * clutter_actor_get_xu
 * @self: A #ClutterActor
 *
 * Retrieves the actors x position relative to any parent, in #ClutterUnit
 *
 * Return value: The actor x position in #ClutterUnit ignoring any tranforms
 * (i.e scaling, rotation).
 *
 * Since: 0.6
 **/
ClutterUnit
clutter_actor_get_xu (ClutterActor *self)
{
  ClutterActorBox box;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  clutter_actor_query_coords (self, &box);

  return box.x1;
}

/**
 * clutter_actor_get_y:
 * @self: A #ClutterActor
 *
 * Retrieves the actors y position relative to any parent.
 *
 * Return value: The actor y position in pixels ignoring any tranforms
 * (i.e scaling, rotation).
 **/
gint
clutter_actor_get_y (ClutterActor *self)
{
  ClutterActorBox box;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  clutter_actor_query_coords (self, &box);

  return CLUTTER_UNITS_TO_DEVICE (box.y1);
}

/**
 * clutter_actor_get_yu:
 * @self: A #ClutterActor
 *
 * Retrieves the actors y position relative to any parent, in #ClutterUnit
 *
 * Return value: The actor y position in #ClutterUnit ignoring any tranforms
 * (i.e scaling, rotation).
 **/
ClutterUnit
clutter_actor_get_yu (ClutterActor *self)
{
  ClutterActorBox box;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  clutter_actor_query_coords (self, &box);

  return box.y1;
}

/**
 * clutter_actor_set_scalex:
 * @self: A #ClutterActor
 * @scale_x: #ClutterFixed factor to scale actor by horizontally.
 * @scale_y: #ClutterFixed factor to scale actor by vertically.
 *
 * Scales an actor with fixed point parameters.
 */
void
clutter_actor_set_scalex (ClutterActor *self,
			  ClutterFixed  scale_x,
			  ClutterFixed  scale_y)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  g_object_ref (self);
  g_object_freeze_notify (G_OBJECT (self));

  self->priv->scale_x = scale_x;
  g_object_notify (G_OBJECT (self), "scale-x");

  self->priv->scale_y = scale_y;
  g_object_notify (G_OBJECT (self), "scale-y");

  g_object_thaw_notify (G_OBJECT (self));
  g_object_unref (self);

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (self);
}

/**
 * clutter_actor_set_scale:
 * @self: A #ClutterActor
 * @scale_x: double factor to scale actor by horizontally.
 * @scale_y: double factor to scale actor by vertically.
 *
 * Scales an actor with floating point parameters.
 *
 * Since: 0.2
 */
void
clutter_actor_set_scale (ClutterActor *self,
			 double        scale_x,
			 double        scale_y)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_scalex (self,
			    CLUTTER_FLOAT_TO_FIXED (scale_x),
			    CLUTTER_FLOAT_TO_FIXED (scale_y));
}

/**
 * clutter_actor_get_scalex:
 * @self: A #ClutterActor
 * @scale_x: Location to store horizonal fixed scale factor if non NULL.
 * @scale_y: Location to store vertical fixed scale factor if non NULL.
 *
 * Retrieves an actors scale in fixed point.
 *
 * Since: 0.2
 */
void
clutter_actor_get_scalex (ClutterActor *self,
			  ClutterFixed *scale_x,
			  ClutterFixed *scale_y)
{
  if (scale_x)
    *scale_x = self->priv->scale_x;

  if (scale_y)
    *scale_y = self->priv->scale_y;
}

/**
 * clutter_actor_get_scale:
 * @self: A #ClutterActor
 * @scale_x: Location to store horizonal float scale factor if non NULL.
 * @scale_y: Location to store vertical float scale factor if non NULL.
 *
 * Retrieves an actors scale in floating point.
 *
 * Since: 0.2
 */
void
clutter_actor_get_scale (ClutterActor *self,
			 gdouble      *scale_x,
			 gdouble      *scale_y)
{
  if (scale_x)
    *scale_x = CLUTTER_FIXED_TO_FLOAT (self->priv->scale_x);

  if (scale_y)
    *scale_y = CLUTTER_FIXED_TO_FLOAT (self->priv->scale_y);
}

/**
 * clutter_actor_set_opacity:
 * @self: A #ClutterActor
 * @opacity: New opacity value for actor.
 *
 * Sets the actors opacity, with zero being completely transparent and
 * 255 (0xff) as fully opaque.
 */
void
clutter_actor_set_opacity (ClutterActor *self,
			   guint8        opacity)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  self->priv->opacity = opacity;

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (self);
}

/**
 * clutter_actor_get_opacity:
 * @self: A #ClutterActor
 *
 * Retrieves the actors opacity.
 *
 * Return value: The actor opacity value.
 */
guint8
clutter_actor_get_opacity (ClutterActor *self)
{
  ClutterActor *parent;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  parent = self->priv->parent_actor;

  /* Factor in the actual actors opacity with parents */
  if (parent && clutter_actor_get_opacity (parent) != 0xff)
      return (clutter_actor_get_opacity(parent) * self->priv->opacity) / 0xff;

  return self->priv->opacity;
}

/**
 * clutter_actor_set_name:
 * @self: A #ClutterActor
 * @name: Textual tag to apply to actor
 *
 * Sets a textual tag to the actor.
 */
void
clutter_actor_set_name (ClutterActor *self,
			const gchar  *name)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  g_object_ref (self);

  g_free (self->priv->name);

  if (name && name[0] != '\0')
    self->priv->name = g_strdup(name);

  g_object_notify (G_OBJECT (self), "name");
  g_object_unref (self);
}

/**
 * clutter_actor_get_name:
 * @self: A #ClutterActor
 *
 * Retrieves the name of @self.
 *
 * Return value: pointer to textual tag for the actor.  The
 *   returned string is owned by the actor and should not
 *   be modified or freed.
 */
G_CONST_RETURN gchar *
clutter_actor_get_name (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), NULL);

  return self->priv->name;
}

/**
 * clutter_actor_get_gid:
 * @self: A #ClutterActor
 *
 * Retrieves the unique id for @self.
 *
 * Return value: Globally unique value for object instance.
 *
 * Since: 0.6
 */
guint32
clutter_actor_get_gid (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  return self->priv->id;
}

/**
 * clutter_actor_set_depth:
 * @self: a #ClutterActor
 * @depth: Z co-ord
 *
 * Sets the Z co-ordinate of @self to @depth. The Units of which are dependant
 * on the perspective setup.
 */
void
clutter_actor_set_depth (ClutterActor *self,
                         gint          depth)
{
  clutter_actor_set_depthu (self, CLUTTER_UNITS_FROM_DEVICE (depth));
}

/**
 * clutter_actor_set_depthu:
 * @self: a #ClutterActor
 * @depth: Z co-ord in #ClutterUnit
 *
 * Sets the Z co-ordinate of @self to @depth in #ClutterUnit, the Units of
 * which are dependant on the perspective setup.
 */
void
clutter_actor_set_depthu (ClutterActor *self,
			  ClutterUnit   depth)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  if (priv->z != depth)
    {
      /* Sets Z value. - FIXME: should invert ?*/
      priv->z = depth;

      if (priv->parent_actor && CLUTTER_IS_CONTAINER (priv->parent_actor))
        {
          ClutterContainer *parent;

          /* We need to resort the container stacking order as to
           * correctly render alpha values.
           *
           * FIXME: This is sub optimal. maybe queue the the sort
           *        before stacking
           */
          parent = CLUTTER_CONTAINER (priv->parent_actor);
          clutter_container_sort_depth_order (parent);
        }

      if (CLUTTER_ACTOR_IS_VISIBLE (self))
        clutter_actor_queue_redraw (self);

      g_object_notify (G_OBJECT (self), "depth");
    }
}

/**
 * clutter_actor_get_depth:
 * @self: a #ClutterActor
 *
 * Retrieves the depth of @self.
 *
 * Return value: the depth of a #ClutterActor
 */
gint
clutter_actor_get_depth (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), -1);

  return CLUTTER_UNITS_TO_DEVICE (self->priv->z);
}

/**
 * clutter_actor_get_depthu:
 * @self: a #ClutterActor
 *
 * Retrieves the depth of @self.
 *
 * Return value: the depth of a #ClutterActor in #ClutterUnit
 *
 * Since: 0.6
 */
ClutterUnit
clutter_actor_get_depthu (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), -1);

  return self->priv->z;
}

/**
 * clutter_actor_set_rotationx:
 * @self: a #ClutterActor
 * @axis: the axis of rotation
 * @angle: the angle of rotation
 * @x: X coordinate of the rotation center
 * @y: Y coordinate of the rotation center
 * @z: Z coordinate of the rotation center
 *
 * Sets the rotation angle of @self around the given axis.
 *
 * This function is the fixed point variant of clutter_actor_set_rotation().
 *
 * Since: 0.6
 */
void
clutter_actor_set_rotationx (ClutterActor      *self,
                             ClutterRotateAxis  axis,
                             ClutterFixed       angle,
                             gint               x,
                             gint               y,
                             gint               z)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  g_object_ref (self);
  g_object_freeze_notify (G_OBJECT (self));

  switch (axis)
    {
    case CLUTTER_X_AXIS:
      priv->rxang = angle;
      priv->rxy = CLUTTER_UNITS_FROM_DEVICE (y);
      priv->rxz = CLUTTER_UNITS_FROM_DEVICE (z);
      g_object_notify (G_OBJECT (self), "rotation-angle-x");
      g_object_notify (G_OBJECT (self), "rotation-center-x");
      break;

    case CLUTTER_Y_AXIS:
      priv->ryang = angle;
      priv->ryx = CLUTTER_UNITS_FROM_DEVICE (x);
      priv->ryz = CLUTTER_UNITS_FROM_DEVICE (z);
      g_object_notify (G_OBJECT (self), "rotation-angle-y");
      g_object_notify (G_OBJECT (self), "rotation-center-y");
      break;
    case CLUTTER_Z_AXIS:
      priv->rzang = angle;
      priv->rzx = CLUTTER_UNITS_FROM_DEVICE (x);
      priv->rzy = CLUTTER_UNITS_FROM_DEVICE (y);
      g_object_notify (G_OBJECT (self), "rotation-angle-z");
      g_object_notify (G_OBJECT (self), "rotation-center-z");
      break;
    }

  g_object_thaw_notify (G_OBJECT (self));
  g_object_unref (self);

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (self);
}

/**
 * clutter_actor_set_rotation:
 * @self: a #ClutterActor
 * @axis: the axis of rotation
 * @angle: the angle of rotation
 * @x: X coordinate of the rotation center
 * @y: Y coordinate of the rotation center
 * @z: Z coordinate of the rotation center
 *
 * Sets the rotation angle of @self around the given axis.
 *
 * The rotation center coordinates used depend on the value of @axis:
 * <itemizedlist>
 *   <listitem><para>%CLUTTER_X_AXIS requires @y and @z</para></listitem>
 *   <listitem><para>%CLUTTER_Y_AXIS requires @x and @z</para></listitem>
 *   <listitem><para>%CLUTTER_Z_AXIS requires @x and @y</para></listitem>
 * </itemizedlist>
 *
 * The rotation coordinates are relative to the anchor point of the
 * actor, set using clutter_actor_set_anchor_point(). If no anchor
 * point is set, the upper left corner is assumed as the origin.
 *
 * Since: 0.6
 */
void
clutter_actor_set_rotation (ClutterActor      *self,
                            ClutterRotateAxis  axis,
                            gdouble            angle,
                            gint               x,
                            gint               y,
                            gint               z)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_rotationx (self, axis,
                               CLUTTER_FLOAT_TO_FIXED (angle),
                               x, y, z);
}

/**
 * clutter_actor_get_rotationx:
 * @self: a #ClutterActor
 * @axis: the axis of rotation
 * @x: return value for the X coordinate of the center of rotation
 * @y: return value for the Y coordinate of the center of rotation
 * @z: return value for the Z coordinate of the center of rotation
 *
 * Retrieves the angle and center of rotation on the given axis,
 * set using clutter_actor_set_rotation().
 *
 * This function is the fixed point variant of clutter_actor_get_rotation().
 *
 * Return value: the angle of rotation as a fixed point value.
 *
 * Since: 0.6
 */
ClutterFixed
clutter_actor_get_rotationx (ClutterActor      *self,
                             ClutterRotateAxis  axis,
                             gint              *x,
                             gint              *y,
                             gint              *z)
{
  ClutterActorPrivate *priv;
  ClutterFixed retval = 0;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  priv = self->priv;

  switch (axis)
    {
    case CLUTTER_X_AXIS:
      retval = priv->rxang;
      if (y)
        *y = CLUTTER_UNITS_TO_DEVICE (priv->rxy);
      if (z)
        *z = CLUTTER_UNITS_TO_DEVICE (priv->rxz);
      break;

    case CLUTTER_Y_AXIS:
      retval = priv->ryang;
      if (x)
        *x = CLUTTER_UNITS_TO_DEVICE (priv->ryx);
      if (z)
        *z = CLUTTER_UNITS_TO_DEVICE (priv->ryz);
      break;

    case CLUTTER_Z_AXIS:
      retval = priv->rzang;
      if (x)
        *x = CLUTTER_UNITS_TO_DEVICE (priv->rzx);
      if (y)
        *y = CLUTTER_UNITS_TO_DEVICE (priv->rzy);
      break;
    }

  return retval;
}

/**
 * clutter_actor_get_rotation:
 * @self: a #ClutterActor
 * @axis: the axis of rotation
 * @x: return value for the X coordinate of the center of rotation
 * @y: return value for the Y coordinate of the center of rotation
 * @z: return value for the Z coordinate of the center of rotation
 *
 * Retrieves the angle and center of rotation on the given axis,
 * set using clutter_actor_set_angle().
 *
 * The coordinates of the center returned by this function depend on
 * the axis passed.
 *
 * Return value: the angle of rotation.
 *
 * Since: 0.6
 */
gdouble
clutter_actor_get_rotation (ClutterActor      *self,
                            ClutterRotateAxis  axis,
                            gint              *x,
                            gint              *y,
                            gint              *z)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0.0);

  return CLUTTER_FIXED_TO_FLOAT (clutter_actor_get_rotationx (self,
                                                              axis,
                                                              x, y, z));
}

/**
 * clutter_actor_set_clipu:
 * @self: A #ClutterActor
 * @xoff: X offset of the clip rectangle, in #ClutterUnit<!-- -->s
 * @yoff: Y offset of the clip rectangle, in #ClutterUnit<!-- -->s
 * @width: Width of the clip rectangle, in #ClutterUnit<!-- -->s
 * @height: Height of the clip rectangle, in #ClutterUnit<!-- -->s
 *
 * Unit-based variant of clutter_actor_set_clip()
 *
 * Sets clip area for @self. The clip area is always computed from the
 * upper left corner of the actor, even if the anchor point is set
 * otherwise.
 *
 * Since: 0.6
 */
void
clutter_actor_set_clipu (ClutterActor *self,
			 ClutterUnit   xoff,
			 ClutterUnit   yoff,
			 ClutterUnit   width,
			 ClutterUnit   height)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  priv->clip[0] = xoff;
  priv->clip[1] = yoff;
  priv->clip[2] = width;
  priv->clip[3] = height;

  priv->has_clip = TRUE;

  g_object_notify (G_OBJECT (self), "has-clip");
  g_object_notify (G_OBJECT (self), "clip");
}

/**
 * clutter_actor_set_clip:
 * @self: A #ClutterActor
 * @xoff: X offset of the clip rectangle, in pixels
 * @yoff: Y offset of the clip rectangle, in pixels
 * @width: Width of the clip rectangle, in pixels
 * @height: Height of the clip rectangle, in pixels
 *
 * Sets clip area in pixels for @self. The clip area is always computed
 * from the upper left corner of the actor, even if the anchor point is
 * set otherwise.
 */
void
clutter_actor_set_clip (ClutterActor *self,
                        gint          xoff,
                        gint          yoff,
                        gint          width,
                        gint          height)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_set_clipu (self,
                           CLUTTER_UNITS_FROM_DEVICE (xoff),
                           CLUTTER_UNITS_FROM_DEVICE (yoff),
                           CLUTTER_UNITS_FROM_DEVICE (width),
                           CLUTTER_UNITS_FROM_DEVICE (height));
}

/**
 * clutter_actor_remove_clip
 * @self: A #ClutterActor
 *
 * Removes clip area in pixels from @self.
 */
void
clutter_actor_remove_clip (ClutterActor *self)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  self->priv->has_clip = FALSE;

  g_object_notify (G_OBJECT (self), "has-clip");
}

/**
 * clutter_actor_has_clip:
 * @self: a #ClutterActor
 *
 * Gets whether the actor has a clip set or not.
 *
 * Return value: %TRUE if the actor has a clip set.
 *
 * Since: 0.1.1
 */
gboolean
clutter_actor_has_clip (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);

  return self->priv->has_clip;
}

/**
 * clutter_actor_get_clipu:
 * @self: a #ClutterActor
 * @xoff: return location for the X offset of the clip rectangle, or %NULL
 * @yoff: return location for the Y offset of the clip rectangle, or %NULL
 * @width: return location for the width of the clip rectangle, or %NULL
 * @height: return location for the height of the clip rectangle, or %NULL
 *
 * Unit-based variant of clutter_actor_get_clip().
 *
 * Gets the clip area for @self, in #ClutterUnit<!-- -->s.
 *
 * Since: 0.6
 */
void
clutter_actor_get_clipu (ClutterActor *self,
                         ClutterUnit  *xoff,
			 ClutterUnit  *yoff,
			 ClutterUnit  *width,
			 ClutterUnit  *height)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  if (!priv->has_clip)
    return;

  if (xoff)
    *xoff = priv->clip[0];

  if (yoff)
    *yoff = priv->clip[1];

  if (width)
    *width = priv->clip[2];

  if (height)
    *height = priv->clip[3];
}

/**
 * clutter_actor_get_clip:
 * @self: a #ClutterActor
 * @xoff: return location for the X offset of the clip rectangle, or %NULL
 * @yoff: return location for the Y offset of the clip rectangle, or %NULL
 * @width: return location for the width of the clip rectangle, or %NULL
 * @height: return location for the height of the clip rectangle, or %NULL
 *
 * Gets the clip area for @self, in pixels.
 *
 * Since: 0.6
 */
void
clutter_actor_get_clip (ClutterActor *self,
                        gint         *xoff,
                        gint         *yoff,
                        gint         *width,
                        gint         *height)
{
  struct clipu { ClutterUnit x, y, width, height; } c = { 0, };

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_get_clipu (self, &c.x, &c.y, &c.width, &c.height);

  if (xoff)
    *xoff = CLUTTER_UNITS_TO_DEVICE (c.x);

  if (yoff)
    *yoff = CLUTTER_UNITS_TO_DEVICE (c.y);

  if (width)
    *width = CLUTTER_UNITS_TO_DEVICE (c.width);

  if (height)
    *height = CLUTTER_UNITS_TO_DEVICE (c.height);
}

/**
 * clutter_actor_set_parent:
 * @self: A #ClutterActor
 * @parent: A new #ClutterActor parent
 *
 * Sets the parent of @self to @parent.  The opposite function is
 * clutter_actor_unparent().
 *
 * This function should not be used by applications but by custom
 * 'composite' actor sub classes.
 */
void
clutter_actor_set_parent (ClutterActor *self,
		          ClutterActor *parent)
{
  ClutterMainContext *clutter_context;

  clutter_context = clutter_context_get_default ();

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (CLUTTER_IS_ACTOR (parent));
  g_return_if_fail (self != parent);
  g_return_if_fail (clutter_context != NULL);

  if (self->priv->parent_actor != NULL)
    {
      g_warning ("Cannot set a parent on an actor which has a parent.\n"
		 "You must use clutter_actor_unparent() first.\n");

      return;
    }

  if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
    {
      g_warning ("Cannot set a parent on a toplevel actor\n");

      return;
    }

  g_hash_table_insert (clutter_context->actor_hash,
		       GUINT_TO_POINTER (clutter_actor_get_gid (self)),
		       (gpointer)self);

  g_object_ref_sink (self);
  self->priv->parent_actor = parent;
  g_signal_emit (self, actor_signals[PARENT_SET], 0, NULL);

  if (CLUTTER_ACTOR_IS_REALIZED (self->priv->parent_actor))
    clutter_actor_realize (self);

  if (CLUTTER_ACTOR_IS_VISIBLE (self->priv->parent_actor) &&
      CLUTTER_ACTOR_IS_VISIBLE (self))
    {
      clutter_actor_queue_redraw (self);
    }
}

/**
 * clutter_actor_get_parent:
 * @self: A #ClutterActor
 *
 * Retrieves the parent of @self.
 *
 * Return Value: The #ClutterActor parent or NULL
 */
ClutterActor *
clutter_actor_get_parent (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), NULL);

  return self->priv->parent_actor;
}

/**
 * clutter_actor_unparent:
 * @self: a #ClutterActor
 *
 * This function should not be used in applications.  It should be called by
 * implementations of container actors, to dissociate a child from the
 * container.
 *
 * Since: 0.1.1
 */
void
clutter_actor_unparent (ClutterActor *self)
{
  ClutterActor *old_parent;
  ClutterMainContext *clutter_context;

  clutter_context = clutter_context_get_default ();

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (clutter_context != NULL);

  if (self->priv->parent_actor == NULL)
    return;

  /* just hide the actor if we are reparenting it */
  if (CLUTTER_ACTOR_IS_REALIZED (self))
    {
      if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_REPARENT)
        clutter_actor_hide (self);
      else
        clutter_actor_unrealize (self);
    }

  old_parent = self->priv->parent_actor;
  self->priv->parent_actor = NULL;
  g_signal_emit (self, actor_signals[PARENT_SET], 0, old_parent);

  g_hash_table_remove (clutter_context->actor_hash,
		       GUINT_TO_POINTER (clutter_actor_get_gid (self)));

  g_object_unref (self);
}

/**
 * clutter_actor_reparent:
 * @self: a #ClutterActor
 * @new_parent: the new #ClutterActor parent
 *
 * This function resets the parent actor of @self.  It is
 * logically equivalent to calling clutter_actory_unparent()
 * and clutter_actor_set_parent().
 *
 * Since: 0.2
 */
void
clutter_actor_reparent (ClutterActor *self,
                        ClutterActor *new_parent)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (CLUTTER_IS_ACTOR (new_parent));
  g_return_if_fail (self != new_parent);

  if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
    {
      g_warning ("Cannot set a parent on a toplevel actor\n");
      return;
    }

  priv = self->priv;

  if (priv->parent_actor != new_parent)
    {
      ClutterActor *old_parent;

      /* if the actor and the parent have already been realized,
       * mark the actor as reparenting, so that clutter_actor_unparent()
       * just hides the actor instead of unrealize it.
       */
      if (CLUTTER_ACTOR_IS_REALIZED (self) &&
          CLUTTER_ACTOR_IS_REALIZED (new_parent))
        {
          CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IN_REPARENT);
        }

      old_parent = priv->parent_actor;

      g_object_ref (self);

      /* FIXME: below assumes only containers can reparent */
      if (CLUTTER_IS_CONTAINER (priv->parent_actor))
        clutter_container_remove_actor (CLUTTER_CONTAINER (priv->parent_actor),
                                        self);
      else
        priv->parent_actor = NULL;

      if (CLUTTER_IS_CONTAINER (new_parent))
        clutter_container_add_actor (CLUTTER_CONTAINER (new_parent), self);
      else
        priv->parent_actor = new_parent;

      g_object_unref (self);

      if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_REPARENT)
        {
          CLUTTER_UNSET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IN_REPARENT);

          clutter_actor_queue_redraw (self);
        }
   }
}
/**
 * clutter_actor_raise:
 * @self: A #ClutterActor
 * @below: A #ClutterActor to raise above.
 *
 * Puts @self above @below.
 * Both actors must have the same parent.
 */
void
clutter_actor_raise (ClutterActor *self,
                     ClutterActor *below)
{
  ClutterActor *parent;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  parent = clutter_actor_get_parent (self);
  if (!parent)
    {
      g_warning ("Actor of type %s is not inside a container",
                 g_type_name (G_OBJECT_TYPE (self)));
      return;
    }

  if (below)
    {
      if (parent != clutter_actor_get_parent (below))
        {
          g_warning ("Actor of type %s is not in the same "
                     "container of actor of type %s",
                     g_type_name (G_OBJECT_TYPE (self)),
                     g_type_name (G_OBJECT_TYPE (below)));
          return;
        }
    }

  clutter_container_raise_child (CLUTTER_CONTAINER (parent), self, below);
}

/**
 * clutter_actor_lower:
 * @self: A #ClutterActor
 * @above: A #ClutterActor to lower below
 *
 * Puts @self below @above.
 * Both actors must have the same parent.
 */
void
clutter_actor_lower (ClutterActor *self,
                     ClutterActor *above)
{
  ClutterActor *parent;

  g_return_if_fail (CLUTTER_IS_ACTOR(self));

  parent = clutter_actor_get_parent (self);
  if (!parent)
    {
      g_warning ("Actor of type %s is not inside a container",
                 g_type_name (G_OBJECT_TYPE (self)));
      return;
    }

  if (above)
    {
      if (parent != clutter_actor_get_parent (above))
        {
          g_warning ("Actor of type %s is not in the same "
                     "container of actor of type %s",
                     g_type_name (G_OBJECT_TYPE (self)),
                     g_type_name (G_OBJECT_TYPE (above)));
          return;
        }
    }

  clutter_container_lower_child (CLUTTER_CONTAINER (parent), self, above);
}

/**
 * clutter_actor_raise_top:
 * @self: A #ClutterActor
 *
 * Raises @self to the top.
 */
void
clutter_actor_raise_top (ClutterActor *self)
{
  clutter_actor_raise (self, NULL);
}

/**
 * clutter_actor_lower_bottom:
 * @self: A #ClutterActor
 *
 * Lowers @self to the bottom.
 */
void
clutter_actor_lower_bottom (ClutterActor *self)
{
  clutter_actor_lower (self, NULL);
}

/*
 * Event handling
 */

/**
 * clutter_actor_event:
 * @actor: a #ClutterActor
 * @event: a #ClutterEvent
 * @capture: TRUE if event in in capture phase, FALSE otherwise.
 *
 * This function is used to emit an event on the main stage.
 * You should rarely need to use this function, except for
 * synthetising events.
 *
 * Return value: the return value from the signal emission: %TRUE
 *   if the actor handled the event, or %FALSE if the event was
 *   not handled
 *
 * Since: 0.6
 */
gboolean
clutter_actor_event (ClutterActor *actor,
                     ClutterEvent *event,
		     gboolean      capture)
{
  gboolean retval = FALSE;
  gint signal_num = -1;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  g_object_ref (actor);

  if (capture)
    {
      g_signal_emit (actor, actor_signals[CAPTURED_EVENT], 0,
		     event,
                     &retval);
      goto out;
    }

  g_signal_emit (actor, actor_signals[EVENT], 0, event, &retval);

  if (!retval)
    {
      switch (event->type)
	{
	case CLUTTER_NOTHING:
	  break;
	case CLUTTER_BUTTON_PRESS:
	  signal_num = BUTTON_PRESS_EVENT;
	  break;
	case CLUTTER_BUTTON_RELEASE:
	  signal_num = BUTTON_RELEASE_EVENT;
	  break;
	case CLUTTER_SCROLL:
	  signal_num = SCROLL_EVENT;
	  break;
	case CLUTTER_KEY_PRESS:
	  signal_num = KEY_PRESS_EVENT;
	  break;
	case CLUTTER_KEY_RELEASE:
	  signal_num = KEY_RELEASE_EVENT;
	  break;
	case CLUTTER_MOTION:
	  signal_num = MOTION_EVENT;
	  break;
	case CLUTTER_ENTER:
	  signal_num = ENTER_EVENT;
	  break;
	case CLUTTER_LEAVE:
	  signal_num = LEAVE_EVENT;
	  break;
	case CLUTTER_DELETE:
	case CLUTTER_DESTROY_NOTIFY:
	case CLUTTER_CLIENT_MESSAGE:
	default:
	  signal_num = -1;
	  break;
	}

      if (signal_num != -1)
	g_signal_emit (actor, actor_signals[signal_num], 0,
		       event, &retval);
    }

out:
  g_object_unref (actor);

  return retval;
}

/**
 * clutter_actor_set_reactive:
 * @actor: a #ClutterActor
 * @reactive: whether the actor should be reactive to events
 *
 * Sets @actor as reactive. Reactive actors will receive events.
 *
 * Since: 0.6
 */
void
clutter_actor_set_reactive (ClutterActor *actor,
                            gboolean      reactive)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  if (reactive == CLUTTER_ACTOR_IS_REACTIVE (actor))
    return;

  if (reactive)
    CLUTTER_ACTOR_SET_FLAGS (actor, CLUTTER_ACTOR_REACTIVE);
  else
    CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REACTIVE);

  g_object_notify (G_OBJECT (actor), "reactive");
}

/**
 * clutter_actor_get_reactive:
 * @actor: a #ClutterActor
 *
 * Checks whether @actor is marked as reactive.
 *
 * Return value: %TRUE if the actor is reactive
 *
 * Since: 0.6
 */
gboolean
clutter_actor_get_reactive (ClutterActor *actor)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (actor), FALSE);

  return CLUTTER_ACTOR_IS_REACTIVE (actor);
}

/**
 * clutter_actor_set_anchor_point:
 * @self: a #ClutterActor
 * @anchor_x: X coordinate of the anchor point
 * @anchor_y: Y coordinate of the anchor point
 *
 * Sets an anchor point for the @actor. The anchor point is a point in the
 * coordinate space of an actor to which the actor position within its
 * parent is relative; the default is (0, 0), i.e. the top-left corner of
 * the actor.
 *
 * Since: 0.6
 */
void
clutter_actor_set_anchor_point (ClutterActor *self,
				gint          anchor_x,
                                gint          anchor_y)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  priv->anchor_x = CLUTTER_UNITS_FROM_DEVICE (anchor_x);
  priv->anchor_y = CLUTTER_UNITS_FROM_DEVICE (anchor_y);
}

/**
 * clutter_actor_move_anchor_point:
 * @self: a #ClutterActor
 * @anchor_x: X coordinate of the anchor point
 * @anchor_y: Y coordinate of the anchor point
 *
 * Sets an anchor point for the @actor, and adjusts the actor postion so that
 * the relative position of the actor toward its parent remains the same.
 *
 * Since: 0.6
 */
void
clutter_actor_move_anchor_point (ClutterActor *self,
				 gint          anchor_x,
				 gint          anchor_y)
{
  ClutterActorPrivate *priv;
  ClutterUnit ax = CLUTTER_UNITS_FROM_DEVICE (anchor_x);
  ClutterUnit ay = CLUTTER_UNITS_FROM_DEVICE (anchor_y);
  ClutterUnit dx;
  ClutterUnit dy;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  dx = ax - priv->anchor_x;
  dy = ay - priv->anchor_y;

  priv->anchor_x = ax;
  priv->anchor_y = ay;

  priv->coords.x1 -= dx;
  priv->coords.x2 -= dx;
  priv->coords.y1 -= dy;
  priv->coords.y2 -= dy;
}

/**
 * clutter_actor_get_anchor_point:
 * @self: a #ClutterActor
 * @anchor_x: return location for the X coordinate of the anchor point
 * @anchor_y: return location for the y coordinate of the anchor point
 *
 * Gets the current anchor point of the @actor in pixels.
 *
 * Since: 0.6
 */
void
clutter_actor_get_anchor_point (ClutterActor *self,
				gint         *anchor_x,
                                gint         *anchor_y)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  if (anchor_x)
    *anchor_x = CLUTTER_UNITS_TO_DEVICE (priv->anchor_x);

  if (anchor_y)
    *anchor_y = CLUTTER_UNITS_TO_DEVICE (priv->anchor_y);
}

/**
 * clutter_actor_set_anchor_pointu:
 * @self: a #ClutterActor
 * @anchor_x: X coordinate of the anchor point, in #ClutterUnit<!-- -->s
 * @anchor_y: Y coordinate of the anchor point, in #ClutterUnit<!-- -->s
 *
 * Sets an anchor point for the @self. The anchor point is a point in the
 * coordinate space of an actor to which the actor position within its
 * parent is relative; the default is (0, 0), i.e. the top-left corner
 * of the actor.
 *
 * Since: 0.6
 */
void
clutter_actor_set_anchor_pointu (ClutterActor *self,
				 ClutterUnit   anchor_x,
                                 ClutterUnit   anchor_y)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  priv->anchor_x = anchor_x;
  priv->anchor_y = anchor_y;
}

/**
 * clutter_actor_move_anchor_pointu:
 * @self: a #ClutterActor
 * @anchor_x: X coordinate of the anchor point
 * @anchor_y: Y coordinate of the anchor point
 *
 * Sets an anchor point for the @actor, and adjusts the actor postion so that
 * the relative position of the actor toward its parent remains the same.
 *
 * Since: 0.6
 */
void
clutter_actor_move_anchor_pointu (ClutterActor *self,
				  ClutterUnit   anchor_x,
				  ClutterUnit   anchor_y)
{
  ClutterActorPrivate *priv;
  ClutterUnit dx;
  ClutterUnit dy;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  dx = anchor_x - priv->anchor_x;
  dy = anchor_y - priv->anchor_y;

  priv->anchor_x = anchor_x;
  priv->anchor_y = anchor_y;

  priv->coords.x1 -= dx;
  priv->coords.x2 -= dx;
  priv->coords.y1 -= dy;
  priv->coords.y2 -= dy;
}

/**
 * clutter_actor_get_anchor_pointu:
 * @self: a #ClutterActor
 * @anchor_x: return location for the X coordinace of the anchor point
 * @anchor_y: return location for the X coordinace of the anchor point
 *
 * Gets the current anchor point of the @actor in #ClutterUnit<!-- -->s.
 *
 * Since: 0.6
 */
void
clutter_actor_get_anchor_pointu (ClutterActor *self,
				 ClutterUnit  *anchor_x,
                                 ClutterUnit  *anchor_y)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  if (anchor_x)
    *anchor_x = priv->anchor_x;

  if (anchor_y)
    *anchor_y = priv->anchor_y;
}

/**
 * clutter_actor_move_anchor_point_from_gravity:
 * @self: a #ClutterActor
 * @gravity: #ClutterGravity.
 *
 * Sets an anchor point of the actor based on the given gravity, adjusting the
 * actor postion so that its relative position within its parent remainst
 * unchanged.
 *
 * Since: 0.6
 */
void
clutter_actor_move_anchor_point_from_gravity (ClutterActor   *self,
					      ClutterGravity  gravity)
{
  ClutterUnit ax, ay, dx, dy;
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  ax = priv->anchor_x;
  ay = priv->anchor_y;

  clutter_actor_set_anchor_point_from_gravity (self, gravity);

  dx = ax - priv->anchor_x;
  dy = ay - priv->anchor_y;

  priv->coords.x1 -= dx;
  priv->coords.x2 -= dx;
  priv->coords.y1 -= dy;
  priv->coords.y2 -= dy;
}

/**
 * clutter_actor_set_anchor_point_from_gravity:
 * @self: a #ClutterActor
 * @gravity: #ClutterGravity.
 *
 * Sets an anchor point the actor based on the given gravity (this is a
 * convenience function wrapping clutter_actor_set_anchor_point()).
 *
 * Since: 0.6
 */
void
clutter_actor_set_anchor_point_from_gravity (ClutterActor   *self,
					     ClutterGravity  gravity)
{
  ClutterActorPrivate *priv;
  ClutterActorBox box;
  ClutterUnit w, h, x, y;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  clutter_actor_query_coords (self, &box);

  x = 0;
  y = 0;
  w  = box.x2 - box.x1;
  h  = box.y2 - box.y1;

  switch (gravity)
    {
    case CLUTTER_GRAVITY_NORTH:
      x = w/2;
      break;
    case CLUTTER_GRAVITY_SOUTH:
      x = w/2;
      y = h;
      break;
    case CLUTTER_GRAVITY_EAST:
      x = w;
      y = h/2;
      break;
    case CLUTTER_GRAVITY_NORTH_EAST:
      x = w;
      break;
    case CLUTTER_GRAVITY_SOUTH_EAST:
      x = w;
      y = h;
      break;
    case CLUTTER_GRAVITY_SOUTH_WEST:
      y = h;
      break;
    case CLUTTER_GRAVITY_WEST:
      y = h/2;
      break;
    case CLUTTER_GRAVITY_CENTER:
      x = w/2;
      y = h/2;
      break;
    case CLUTTER_GRAVITY_NONE:
    case CLUTTER_GRAVITY_NORTH_WEST:
    default:
      break;
    }

  priv->anchor_x = x;
  priv->anchor_y = y;
}

typedef enum
{
  PARSE_X,
  PARSE_Y,
  PARSE_WIDTH,
  PARSE_HEIGHT
} ParseDimension;

static ClutterUnit
parse_units (ClutterActor   *self,
             ParseDimension  dimension,
             JsonNode       *node)
{
  GValue value = { 0, };
  ClutterUnit retval = 0;

  if (JSON_NODE_TYPE (node) != JSON_NODE_VALUE)
    return 0;

  json_node_get_value (node, &value);

  if (G_VALUE_HOLDS (&value, G_TYPE_INT))
    {
      gint pixels = g_value_get_int (&value);

      retval = CLUTTER_UNITS_FROM_DEVICE (pixels);
    }
  else if (G_VALUE_HOLDS (&value, G_TYPE_STRING))
    {
      gint64 val;
      gchar *end;

      val = g_ascii_strtoll (g_value_get_string (&value), &end, 10);

      /* assume pixels */
      if (*end == '\0')
        {
          retval = CLUTTER_UNITS_FROM_DEVICE (val);
          goto out;
        }

      if (strcmp (end, "px") == 0)
        {
          retval = CLUTTER_UNITS_FROM_DEVICE (val);
          goto out;
        }

      if (strcmp (end, "mm") == 0)
        {
          retval = CLUTTER_UNITS_FROM_MM (val);
          goto out;
        }

      if (strcmp (end, "pt") == 0)
        {
          retval = CLUTTER_UNITS_FROM_POINTS (val);
          goto out;
        }

      if (end[0] == '%' && end[1] == '\0')
        {
          if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
            {
              g_warning ("Unable to set percentage of %s on a top-level "
                         "actor of type `%s'",
                         (dimension == PARSE_X || dimension == PARSE_WIDTH) ? "width"
                                                                            : "height",
                         g_type_name (G_OBJECT_TYPE (self)));
              retval = 0;
              goto out;
            }

          if (dimension == PARSE_X || dimension == PARSE_WIDTH)
            retval = CLUTTER_UNITS_FROM_STAGE_WIDTH_PERCENTAGE (val);
          else
            retval = CLUTTER_UNITS_FROM_STAGE_HEIGHT_PERCENTAGE (val);

          goto out;
        }

      g_warning ("Invalid value `%s': integers, strings or floating point "
                 "values can be used for the x, y, width and height "
                 "properties. Valid modifiers for strings are `px', 'mm' "
                 "and '%%'.",
                 g_value_get_string (&value));

      retval = 0;
    }
  else if (G_VALUE_HOLDS (&value, G_TYPE_DOUBLE))
    {
      gint val;

      if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
        {
          g_warning ("Unable to set percentage of %s on a top-level "
                     "actor of type `%s'",
                     (dimension == PARSE_X || dimension == PARSE_WIDTH) ? "width"
                                                                        : "height",
                     g_type_name (G_OBJECT_TYPE (self)));
          retval = 0;
          goto out;
        }

      val = CLAMP (g_value_get_double (&value) * 100, 0, 100);

      if (dimension == PARSE_X || dimension == PARSE_WIDTH)
        retval = CLUTTER_UNITS_FROM_STAGE_WIDTH_PERCENTAGE (val);
      else
        retval = CLUTTER_UNITS_FROM_STAGE_HEIGHT_PERCENTAGE (val);
    }
  else
    {
      g_warning ("Invalid value of type `%s': integers, strings of floating "
                 "point values can be used for the x, y, width and height "
                 "properties.",
                 g_type_name (G_VALUE_TYPE (&value)));
    }

out:
  g_value_unset (&value);

  return retval;
}

typedef struct {
  ClutterRotateAxis axis;

  gdouble angle;

  ClutterUnit center_x;
  ClutterUnit center_y;
  ClutterUnit center_z;
} RotationInfo;

static inline gboolean
parse_rotation_array (ClutterActor *actor,
                      JsonArray    *array,
                      RotationInfo *info)
{
  JsonNode *element;

  if (json_array_get_length (array) != 2)
    return FALSE;

  /* angle */
  element = json_array_get_element (array, 0);
  if (JSON_NODE_TYPE (element) == JSON_NODE_VALUE)
    info->angle = json_node_get_double (element);
  else
    return FALSE;

  /* center */
  element = json_array_get_element (array, 1);
  if (JSON_NODE_TYPE (element) == JSON_NODE_ARRAY)
    {
      JsonArray *center = json_node_get_array (element);

      if (json_array_get_length (center) != 2)
        return FALSE;

      switch (info->axis)
        {
        case CLUTTER_X_AXIS:
          info->center_y = parse_units (actor, PARSE_Y,
                                        json_array_get_element (center, 0));
          info->center_z = parse_units (actor, PARSE_Y,
                                        json_array_get_element (center, 1));
          return TRUE;

        case CLUTTER_Y_AXIS:
          info->center_x = parse_units (actor, PARSE_X,
                                        json_array_get_element (center, 0));
          info->center_z = parse_units (actor, PARSE_X,
                                        json_array_get_element (center, 1));
          return TRUE;

        case CLUTTER_Z_AXIS:
          info->center_x = parse_units (actor, PARSE_X,
                                        json_array_get_element (center, 0));
          info->center_y = parse_units (actor, PARSE_Y,
                                        json_array_get_element (center, 1));
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
parse_rotation (ClutterActor *actor,
                JsonNode     *node,
                RotationInfo *info)
{
  JsonArray *array;
  guint len, i;
  gboolean retval = FALSE;

  if (JSON_NODE_TYPE (node) != JSON_NODE_ARRAY)
    {
      g_warning ("Invalid node of type `%s' found, expecting an array",
                 json_node_type_name (node));
      return FALSE;
    }

  array = json_node_get_array (node);
  len = json_array_get_length (array);

  for (i = 0; i < len; i++)
    {
      JsonNode *element = json_array_get_element (array, i);
      JsonObject *object;
      JsonNode *member;

      if (JSON_NODE_TYPE (element) != JSON_NODE_OBJECT)
        {
          g_warning ("Invalid node of type `%s' found, expecting an object",
                     json_node_type_name (element));
          return FALSE;
        }

      object = json_node_get_object (element);

      if (json_object_has_member (object, "x-axis"))
        {
          member = json_object_get_member (object, "x-axis");

          info->axis = CLUTTER_X_AXIS;

          if (JSON_NODE_TYPE (member) == JSON_NODE_VALUE)
            {
              info->angle = json_node_get_double (member);
              retval = TRUE;
            }
          else if (JSON_NODE_TYPE (member) == JSON_NODE_ARRAY)
            retval = parse_rotation_array (actor,
                                           json_node_get_array (member),
                                           info);
          else
            retval = FALSE;
        }
      else if (json_object_has_member (object, "y-axis"))
        {
          member = json_object_get_member (object, "y-axis");

          info->axis = CLUTTER_Y_AXIS;

          if (JSON_NODE_TYPE (member) == JSON_NODE_VALUE)
            {
              info->angle = json_node_get_double (member);
              retval = TRUE;
            }
          else if (JSON_NODE_TYPE (member) == JSON_NODE_ARRAY)
            retval = parse_rotation_array (actor,
                                           json_node_get_array (member),
                                           info);
          else
            retval = FALSE;
        }
      else if (json_object_has_member (object, "z-axis"))
        {
          member = json_object_get_member (object, "z-axis");

          info->axis = CLUTTER_Z_AXIS;

          if (JSON_NODE_TYPE (member) == JSON_NODE_VALUE)
            {
              info->angle = json_node_get_double (member);
              retval = TRUE;
            }
          else if (JSON_NODE_TYPE (member) == JSON_NODE_ARRAY)
            retval = parse_rotation_array (actor,
                                           json_node_get_array (member),
                                           info);
          else
            retval = FALSE;
        }
    }

  return retval;
}

static gboolean
clutter_actor_parse_custom_node (ClutterScriptable *scriptable,
                                 ClutterScript     *script,
                                 GValue            *value,
                                 const gchar       *name,
                                 JsonNode          *node)
{
  ClutterActor *actor = CLUTTER_ACTOR (scriptable);
  gboolean retval = FALSE;

  if ((name[0] == 'x' && name[1] == '\0') ||
      (name[0] == 'y' && name[1] == '\0') ||
      (strcmp (name, "width") == 0) ||
      (strcmp (name, "height") == 0))
    {
      ClutterUnit units;
      ParseDimension dimension;

      if (name[0] == 'x')
        dimension = PARSE_X;
      else if (name[0] == 'y')
        dimension = PARSE_Y;
      else if (name[0] == 'w')
        dimension = PARSE_WIDTH;
      else
        dimension = PARSE_HEIGHT;

      units = parse_units (actor, dimension, node);

      /* convert back to pixels */
      g_value_init (value, G_TYPE_INT);
      g_value_set_int (value, CLUTTER_UNITS_TO_DEVICE (units));

      retval = TRUE;
    }
  else if (strcmp (name, "rotation") == 0)
    {
      RotationInfo *info;

      info = g_slice_new0 (RotationInfo);
      retval = parse_rotation (actor, node, info);

      if (retval)
        {
          g_value_init (value, G_TYPE_POINTER);
          g_value_set_pointer (value, info);
        }
      else
        g_slice_free (RotationInfo, info);
    }

  return retval;
}

static void
clutter_actor_set_custom_property (ClutterScriptable *scriptable,
                                   ClutterScript     *script,
                                   const gchar       *name,
                                   const GValue      *value)
{
  if (strcmp (name, "rotation") == 0)
    {
      RotationInfo *info;

      if (!G_VALUE_HOLDS (value, G_TYPE_POINTER))
        return;

      info = g_value_get_pointer (value);

      clutter_actor_set_rotation (CLUTTER_ACTOR (scriptable),
                                  info->axis, info->angle,
                                  CLUTTER_UNITS_TO_DEVICE (info->center_x),
                                  CLUTTER_UNITS_TO_DEVICE (info->center_y),
                                  CLUTTER_UNITS_TO_DEVICE (info->center_z));

      g_slice_free (RotationInfo, info);
    }
  else
    g_object_set_property (G_OBJECT (scriptable), name, value);
}

static void
clutter_scriptable_iface_init (ClutterScriptableIface *iface)
{
  iface->parse_custom_node = clutter_actor_parse_custom_node;
  iface->set_custom_property = clutter_actor_set_custom_property;
}

/**
 * clutter_actor_transform_stage_point
 * @self: A #ClutterActor
 * @x: x screen coordinate of the point to unproject, in #ClutterUnit<!-- -->s
 * @y: y screen coordinate of the point to unproject, in #ClutterUnit<!-- -->s
 * @x_out: return location for the unprojected x coordinance, in
 *   #ClutterUnit<!-- -->s
 * @y_out: return location for the unprojected y coordinance, in
 *   #ClutterUnit<!-- -->s
 *
 * The function translates point with screen coordinates (@x, @y) to
 * coordinates relative to the actor, i.e. it can be used to translate
 * screen events from global screen coordinates into local coordinates.
 *
 * The conversion can fail, notably if the transform stack results in the
 * actor being projected on the screen as a mere line.
 *
 * The conversion should not be expected to be pixel-perfect due to the
 * nature of the operation. In general the error grows when the skewing
 * of the actor rectangle on screen increases.
 *
 * Note: This function is fairly computationally intensive.
 *
 * Return value: %TRUE if conversion was successful.
 *
 * Since: 0.6
 */
gboolean
clutter_actor_transform_stage_point (ClutterActor  *self,
				     ClutterUnit    x,
				     ClutterUnit    y,
				     ClutterUnit   *x_out,
				     ClutterUnit   *y_out)
{
  ClutterVertex v[4];
  ClutterFixed  ST[3][3];
  ClutterFixed  RQ[3][3];
  int du, dv, xi, yi;
  ClutterFixed xf, yf, wf, px, py, det;
  ClutterActorPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);

  priv = self->priv;

  /*
   * This implementation is based on the quad -> quad projection algorithm
   * described by Paul Heckbert in
   *
   * http://www.cs.cmu.edu/~ph/texfund/texfund.pdf
   *
   * and the sample implementaion at http://www.cs.cmu.edu/~ph/src/texfund/.
   *
   * Our texture is a rectangle with origin [0,0], so we are mapping from quad
   * to rectangle only, which significantly simplifies things; the function
   * calls have been unrolled, and most of the math is done in fixed point.
   */

  clutter_actor_get_vertices (self, v);

  /*
   * Keeping these as ints simplifies the multiplication (no significant loss
   * of precission here).
   */
  du = CLUTTER_UNITS_TO_DEVICE (priv->coords.x2 - priv->coords.x1);
  dv = CLUTTER_UNITS_TO_DEVICE (priv->coords.y2 - priv->coords.y1);

  if (!du || !dv)
    return FALSE;

#define FP2FX CLUTTER_FLOAT_TO_FIXED
#define FX2FP CLUTTER_FIXED_TO_DOUBLE
#define FP2INT CLUTTER_FLOAT_TO_INT
#define DET2X(a,b, c,d) (CFX_QMUL(a,d) - CFX_QMUL(b,c))

  /*
   * First, find mapping from unit uv square to xy quadrilateral; this
   * equivalent to the pmap_square_quad() functions in the sample
   * implementation, which we can simplify, since our target is always
   * a rectangle.
   */
  px = v[0].x - v[1].x + v[3].x - v[2].x;
  py = v[0].y - v[1].y + v[3].y - v[2].y;

  if (!px && !py)
    { /* affine transform */
      RQ[0][0] = v[1].x - v[0].x;
      RQ[1][0] = v[3].x - v[1].x;
      RQ[2][0] = v[0].x;
      RQ[0][1] = v[1].y - v[0].y;
      RQ[1][1] = v[3].y - v[1].y;
      RQ[2][1] = v[0].y;
      RQ[0][2] = 0;
      RQ[1][2] = 0;
      RQ[2][2] = CFX_ONE;
    }
  else
    { /* projective transform */
      ClutterFixed dx1, dx2, dy1, dy2, del;

      dx1 = v[1].x - v[3].x;
      dx2 = v[2].x - v[3].x;
      dy1 = v[1].y - v[3].y;
      dy2 = v[2].y - v[3].y;

      del = DET2X (dx1,dx2, dy1,dy2);

      if (!del)
	return FALSE;

      /*
       * The division here needs to be done in floating point for
       * precisions reasons.
       */
      RQ[0][2] = FP2FX (FX2FP (DET2X (px,dx2, py,dy2) / FX2FP (del)));
      RQ[1][2] = FP2FX (FX2FP (DET2X (dx1,px, dy1,py) / FX2FP (del)));
      RQ[1][2] = CFX_DIV (DET2X(dx1,px, dy1,py), del);
      RQ[2][2] = CFX_ONE;
      RQ[0][0] = v[1].x - v[0].x + CFX_QMUL (RQ[0][2], v[1].x);
      RQ[1][0] = v[2].x - v[0].x + CFX_QMUL (RQ[1][2], v[2].x);
      RQ[2][0] = v[0].x;
      RQ[0][1] = v[1].y - v[0].y + CFX_QMUL (RQ[0][2], v[1].y);
      RQ[1][1] = v[2].y - v[0].y + CFX_QMUL (RQ[1][2], v[2].y);
      RQ[2][1] = v[0].y;
    }

  /*
   * Now combine with transform from our rectangle (u0,v0,u1,v1) to unit
   * square. Since our rectangle is based at 0,0 we only need to scale.
   */
  RQ[0][0] /= du;
  RQ[1][0] /= dv;
  RQ[0][1] /= du;
  RQ[1][1] /= dv;
  RQ[0][2] /= du;
  RQ[1][2] /= dv;

  /*
   * Now RQ is transform from uv rectangle to xy quadrilateral; we need an
   * inverse of that.
   */
  ST[0][0] = DET2X(RQ[1][1], RQ[1][2], RQ[2][1], RQ[2][2]);
  ST[1][0] = DET2X(RQ[1][2], RQ[1][0], RQ[2][2], RQ[2][0]);
  ST[2][0] = DET2X(RQ[1][0], RQ[1][1], RQ[2][0], RQ[2][1]);
  ST[0][1] = DET2X(RQ[2][1], RQ[2][2], RQ[0][1], RQ[0][2]);
  ST[1][1] = DET2X(RQ[2][2], RQ[2][0], RQ[0][2], RQ[0][0]);
  ST[2][1] = DET2X(RQ[2][0], RQ[2][1], RQ[0][0], RQ[0][1]);
  ST[0][2] = DET2X(RQ[0][1], RQ[0][2], RQ[1][1], RQ[1][2]);
  ST[1][2] = DET2X(RQ[0][2], RQ[0][0], RQ[1][2], RQ[1][0]);
  ST[2][2] = DET2X(RQ[0][0], RQ[0][1], RQ[1][0], RQ[1][1]);

  /*
   * Check the resutling martix is OK.
   */
  det = CFX_QMUL (RQ[0][0], ST[0][0]) + CFX_QMUL (RQ[0][1], ST[0][1]) +
    CFX_QMUL (RQ[0][2], ST[0][2]);

  if (!det)
    return FALSE;

  /*
   * Now transform our point with the ST matrix; the notional w coordiance
   * is 1, hence the last part is simply added.
   */
  xi = CLUTTER_UNITS_TO_DEVICE (x);
  yi = CLUTTER_UNITS_TO_DEVICE (y);

  xf = xi*ST[0][0] + yi*ST[1][0] + ST[2][0];
  yf = xi*ST[0][1] + yi*ST[1][1] + ST[2][1];
  wf = xi*ST[0][2] + yi*ST[1][2] + ST[2][2];

  /*
   * The division needs to be done in floating point for precission reasons.
   */
  *x_out = CLUTTER_UNITS_FROM_FLOAT (FX2FP (xf) / FX2FP (wf));
  *y_out = CLUTTER_UNITS_FROM_FLOAT (FX2FP (yf) / FX2FP (wf));

#undef FP2FX
#undef FX2FP
#undef FP2INT
#undef DET2X

  return TRUE;
}

/*
 * ClutterGeometry
 */

static ClutterGeometry*
clutter_geometry_copy (const ClutterGeometry *geometry)
{
  ClutterGeometry *result = g_slice_new (ClutterGeometry);

  *result = *geometry;

  return result;
}

static void
clutter_geometry_free (ClutterGeometry *geometry)
{
  if (G_LIKELY (geometry))
    g_slice_free (ClutterGeometry, geometry);
}

GType
clutter_geometry_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type =
      g_boxed_type_register_static (I_("ClutterGeometry"),
                                    (GBoxedCopyFunc) clutter_geometry_copy,
                                    (GBoxedFreeFunc) clutter_geometry_free);

  return our_type;
}

/*
 * ClutterVertices
 */

static ClutterVertex *
clutter_vertex_copy (const ClutterVertex *vertex)
{
  ClutterVertex *result = g_slice_new (ClutterVertex);

  *result = *vertex;

  return result;
}

static void
clutter_vertex_free (ClutterVertex *vertex)
{
  if (G_UNLIKELY (vertex))
    g_slice_free (ClutterVertex, vertex);
}

GType
clutter_vertex_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type =
      g_boxed_type_register_static (I_("ClutterVertex"),
                                    (GBoxedCopyFunc) clutter_vertex_copy,
                                    (GBoxedFreeFunc) clutter_vertex_free);

  return our_type;
}

/*
 * ClutterActorBox
 */
static ClutterActorBox *
clutter_actor_box_copy (const ClutterActorBox *box)
{
  ClutterActorBox *result = g_slice_new (ClutterActorBox);

  *result = *box;

  return result;
}

static void
clutter_actor_box_free (ClutterActorBox *box)
{
  if (G_LIKELY (box))
    g_slice_free (ClutterActorBox, box);
}

GType
clutter_actor_box_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type =
      g_boxed_type_register_static (I_("ClutterActorBox"),
                                    (GBoxedCopyFunc) clutter_actor_box_copy,
                                    (GBoxedFreeFunc) clutter_actor_box_free);
  return our_type;
}

/******************************************************************************/

typedef struct _BoxedFloat BoxedFloat;

struct _BoxedFloat
{
  gfloat value;
};

static void
boxed_float_free (gpointer data)
{
  if (G_LIKELY (data))
    g_slice_free (BoxedFloat, data);
}

struct _ShaderData
{
  ClutterShader *shader;
  GHashTable    *float1f_hash; /*< list of values that should be set
                                *  on the shader before each paint cycle
                                */
};

static void
destroy_shader_data (ClutterActor *self)
{
  ClutterActorPrivate *actor_priv = self->priv;
  ShaderData          *shader_data   = actor_priv->shader_data;

  if (!shader_data)
    return;

  if (shader_data->shader)
    {
      g_object_unref (shader_data->shader);
      shader_data->shader = NULL;
    }

  if (shader_data->float1f_hash)
    {
      g_hash_table_destroy (shader_data->float1f_hash);
      shader_data->float1f_hash = NULL;
    }

  g_free (shader_data);
  actor_priv->shader_data = NULL;
}


/**
 * clutter_actor_get_shader:
 * @self: a #ClutterActor
 * @shader: a #ClutterShader or %NULL
 *
 * Queries the currently set #ClutterShader on @self.
 *
 * Return value: The currently set #ClutterShader or NULL if no shader is set.
 *
 * Since: 0.6
 */
ClutterShader *
clutter_actor_get_shader (ClutterActor *self)
{
  ClutterActorPrivate *actor_priv;
  ShaderData     *shader_data;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);

  actor_priv = self->priv;
  shader_data = actor_priv->shader_data;

  if (!shader_data)
    {
      return NULL;
    }
  return shader_data->shader;
}

/**
 * clutter_actor_set_shader:
 * @self: a #ClutterActor
 * @shader: a #ClutterShader or %NULL to unset the shader.
 *
 * Sets the #ClutterShader to be used when rendering @self, pass in NULL
 * to unset a currently set shader for an actor.
 *
 * Return value: %TRUE if the shader was successfully applied
 *
 * Since: 0.6
 */
gboolean
clutter_actor_set_shader (ClutterActor  *self,
                          ClutterShader *shader)
{
  ClutterActorPrivate *actor_priv;
  ShaderData          *shader_data;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), FALSE);
  g_return_val_if_fail (shader == NULL || CLUTTER_IS_SHADER (shader), FALSE);

  /* if shader passed in is NULL we destroy the shader */
  if (shader == NULL)
    {
      destroy_shader_data (self);
    }

  actor_priv = self->priv;
  shader_data = actor_priv->shader_data;

  if (!shader_data)
    {
      actor_priv->shader_data = shader_data = g_new0 (ShaderData, 1);
      shader_data->float1f_hash =
        g_hash_table_new_full (g_str_hash, g_str_equal,
                               g_free,
                               boxed_float_free);
    }
  if (shader_data->shader)
    {
      g_object_unref (shader_data->shader);
      shader_data->shader = NULL;
    }

  if (shader)
    {
      shader_data->shader = g_object_ref (shader);
    }


  clutter_actor_queue_redraw (self);

  return TRUE;
}


static void
set_each_param (gpointer key,
                gpointer value,
                gpointer user_data)
{
  ClutterShader *shader = CLUTTER_SHADER (user_data);
  BoxedFloat *box = value;

  clutter_shader_set_uniform_1f (shader, key, box->value);
}

static void
clutter_actor_shader_pre_paint (ClutterActor *actor,
                                gboolean      repeat)
{
  ClutterActorPrivate *priv;
  ShaderData          *shader_data;
  ClutterShader       *shader;
  ClutterMainContext  *context;

  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = actor->priv;
  shader_data = priv->shader_data;

  if (!shader_data)
    return;

  context = clutter_context_get_default ();
  shader = shader_data->shader;

  if (shader)
    {
      clutter_shader_set_is_enabled (shader, TRUE);

      g_hash_table_foreach (shader_data->float1f_hash, set_each_param, shader);

      if (!repeat)
        context->shaders = g_slist_prepend (context->shaders, actor);
    }
}

static void
clutter_actor_shader_post_paint (ClutterActor *actor)
{
  ClutterActorPrivate *priv;
  ShaderData          *shader_data;
  ClutterShader       *shader;
  ClutterMainContext  *context;

  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  priv = actor->priv;
  shader_data = priv->shader_data;

  if (!shader_data)
    return;

  context = clutter_context_get_default ();
  shader = shader_data->shader;

  if (shader)
    {
      clutter_shader_set_is_enabled (shader, FALSE);

      context->shaders = g_slist_remove (context->shaders, actor);
      if (context->shaders)
        {
          /* call pre-paint again, this time with the second argument being
           * TRUE, indicating that we are reapplying the shader and thus
           * should not be prepended to the stack
           */
          clutter_actor_shader_pre_paint (context->shaders->data, TRUE);
        }
    }
}

/**
 * clutter_actor_set_shader_param:
 * @self: a #ClutterActor
 * @param: the name of the parameter
 * @value: the value of the parameter
 *
 * Sets the value for a named parameter of the shader applied
 * to @actor.
 *
 * Since: 0.6
 */
void
clutter_actor_set_shader_param (ClutterActor *self,
                                const gchar  *param,
                                gfloat        value)
{
  ClutterActorPrivate *priv;
  ShaderData *shader_data;
  BoxedFloat *box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (param != NULL);

  priv = self->priv;
  shader_data = priv->shader_data;

  if (!shader_data)
    return;

  box = g_slice_new (BoxedFloat);
  box->value = value;
  g_hash_table_insert (shader_data->float1f_hash, g_strdup (param), box);
}
