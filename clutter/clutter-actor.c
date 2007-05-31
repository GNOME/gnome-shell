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
 * #ClutterActor is a base abstract class for all visual elements. 
 */

#include "config.h"

#include "clutter-actor.h"
#include "clutter-group.h"
#include "clutter-main.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-units.h"
#include "cogl.h"

G_DEFINE_ABSTRACT_TYPE (ClutterActor,
			clutter_actor,
			G_TYPE_INITIALLY_UNOWNED);

static guint32 __id = 0;


#define CLUTTER_ACTOR_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_ACTOR, ClutterActorPrivate))

struct _ClutterActorPrivate
{
  ClutterActorBox coords;

  ClutterGeometry clip;
  guint           has_clip : 1;
  ClutterFixed    rxang, ryang, rzang;           /* Rotation*/
  gint            rzx, rzy, rxy, rxz, ryx, ryz;
  gint            z;
  guint8          opacity;
  ClutterActor   *parent_actor;
  gchar          *name;
  ClutterFixed    scale_x, scale_y;
  guint32         id; /* Unique ID */
};

enum
{
  PROP_0,
  PROP_X,
  PROP_Y,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_CLIP,
  PROP_HAS_CLIP,
  PROP_OPACITY,
  PROP_NAME,
  PROP_VISIBLE
};

enum
{
  SHOW,
  HIDE,
  DESTROY,
  PARENT_SET,

  LAST_SIGNAL
};

static guint actor_signals[LAST_SIGNAL] = { 0, };

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
 * Recursively show an actor, and any child actor if @self has
 * child actors.
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

      g_signal_emit (self, actor_signals[HIDE], 0);
      g_object_notify (G_OBJECT (self), "visible");

      g_object_unref (self);
    }
}

/**
 * clutter_actor_hide_all:
 * @self: a #ClutterActor
 *
 * Recursively hides an actor, and any child actor if @self
 * has child actors.
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

/**
 * clutter_actor_pick:
 * @self: A #ClutterActor
 * @color: A #ClutterColor
 *
 * Renders a silhouette of the actor in supplied color.
 *
 * This function should not never be called directly by applications. 
 **/
void                  
clutter_actor_pick (ClutterActor       *self, 
		    const ClutterColor *color)
{
  ClutterActorClass *klass;

  klass = CLUTTER_ACTOR_GET_CLASS (self);

  if (G_UNLIKELY(klass->pick))
    {
      /* Its pretty unlikely anything other than a container actor 
       * would need to supply its own pick method.
      */
      (klass->pick) (self, color);
    }
  else
    {
      cogl_color (color);
      cogl_rectangle (0, 
		      0, 
		      clutter_actor_get_width(self), 
		      clutter_actor_get_height(self));
    }
}

/*
 * Utility functions for manipulating transformation matrix
 *
 * Matrix: 4x4 of ClutterFixed
 */
#define M(m,row,col)  (m)[col*4+row]

/* Make indentity matrix */
static void
mtx_identity (ClutterFixed * m)
{
    memset (m, 0, sizeof (ClutterFixed) * 16);
    M (m,0,0) = CFX_ONE;
    M (m,1,1) = CFX_ONE;
    M (m,2,2) = CFX_ONE;
    M (m,3,3) = CFX_ONE;
}

/* Add translation by (x,y,z) to matrix */
static void
mtx_translate (ClutterFixed * m,
	       ClutterFixed x, ClutterFixed y, ClutterFixed z)
{
    M (m,0,3) += CFX_MUL (M (m,0,0), x) + CFX_MUL (M (m,0,1), y) +
	CFX_MUL (M (m,0,2), z);

    M (m,1,3) += CFX_MUL (M (m,1,0), x) + CFX_MUL (M (m,1,1), y) +
	CFX_MUL (M (m,1,2), z);
    
    M (m,2,3) += CFX_MUL (M (m,2,0), x) + CFX_MUL (M (m,2,1), y) +
	CFX_MUL (M (m,2,2), z);
    
    M (m,3,3) += CFX_MUL (M (m,3,0), x) + CFX_MUL (M (m,3,1), y) +
	CFX_MUL (M (m,3,2), z);
}

/* Add rotation around Z axis by ang to matrix */
static void
mtx_rotate_z (ClutterFixed *m, ClutterFixed ang)
{
    ClutterFixed m0, m1;
    ClutterAngle a = CLUTTER_ANGLE_FROM_DEGX (ang);
    ClutterFixed c = clutter_cosi (a);
    ClutterFixed s = clutter_sini (a);

    m0 = CFX_MUL (M (m,0,0),  c) + CFX_MUL (M (m,0,1), s);
    m1 = CFX_MUL (M (m,0,0), -s) + CFX_MUL (M (m,0,1), c);
    M (m,0,0) = m0;
    M (m,0,1) = m1;
    
    m0 = CFX_MUL (M (m,1,0),  c) + CFX_MUL (M (m,1,1), s);
    m1 = CFX_MUL (M (m,1,0), -s) + CFX_MUL (M (m,1,1), c);
    M (m,1,0) = m0;
    M (m,1,1) = m1;
    
    m0 = CFX_MUL (M (m,2,0),  c) + CFX_MUL (M (m,2,1), s);
    m1 = CFX_MUL (M (m,2,0), -s) + CFX_MUL (M (m,2,1), c);
    M (m,2,0) = m0;
    M (m,2,1) = m1;

    m0 = CFX_MUL (M (m,3,0),  c) + CFX_MUL (M (m,3,1), s);
    m1 = CFX_MUL (M (m,3,0), -s) + CFX_MUL (M (m,3,1), c);
    M (m,3,0) = m0;
    M (m,3,1) = m1;
}

/* Add rotation around X axis by ang to matrix */
static void
mtx_rotate_x (ClutterFixed *m, ClutterFixed ang)
{
    ClutterFixed m1, m2;
    ClutterAngle a = CLUTTER_ANGLE_FROM_DEGX (ang);
    ClutterFixed c = clutter_cosi (a);
    ClutterFixed s = clutter_sini (a);
    
    m1 = CFX_MUL (M (m,0,1),  c) + CFX_MUL (M (m,0,2), s);
    m2 = CFX_MUL (M (m,0,1), -s) + CFX_MUL (M (m,0,2), c);
    M (m,0,1) = m1;
    M (m,0,2) = m2;
    
    m1 = CFX_MUL (M (m,1,1),  c) + CFX_MUL (M (m,1,2), s);
    m2 = CFX_MUL (M (m,1,1), -s) + CFX_MUL (M (m,1,2), c);
    M (m,1,1) = m1;
    M (m,1,2) = m2;
    
    m1 = CFX_MUL (M (m,2,1),  c) + CFX_MUL (M (m,2,2), s);
    m2 = CFX_MUL (M (m,2,1), -s) + CFX_MUL (M (m,2,2), c);
    M (m,2,1) = m1;
    M (m,2,2) = m2;
    
    m1 = CFX_MUL (M (m,3,1),  c) + CFX_MUL (M (m,3,2), s);
    m2 = CFX_MUL (M (m,3,1), -s) + CFX_MUL (M (m,3,2), c);
    M (m,3,1) = m1;
    M (m,3,2) = m2;
}

/* Add rotation around Y axis by ang to matrix */
static void
mtx_rotate_y (ClutterFixed *m, ClutterFixed ang)
{
    ClutterFixed m0, m2;
    ClutterAngle a = CLUTTER_ANGLE_FROM_DEGX (ang);
    ClutterFixed c = clutter_cosi (a);
    ClutterFixed s = clutter_sini (a);
    
    m0 = CFX_MUL (M (m,0,0), c) + CFX_MUL (M (m,0,2), -s);
    m2 = CFX_MUL (M (m,0,0), s) + CFX_MUL (M (m,0,2),  c);
    M (m,0,0) = m0;
    M (m,0,2) = m2;

    m0 = CFX_MUL (M (m,1,0), c) + CFX_MUL (M (m,1,2), -s);
    m2 = CFX_MUL (M (m,1,0), s) + CFX_MUL (M (m,1,2),  c);
    M (m,1,0) = m0;
    M (m,1,2) = m2;

    m0 = CFX_MUL (M (m,2,0), c) + CFX_MUL (M (m,2,2), -s);
    m2 = CFX_MUL (M (m,2,0), s) + CFX_MUL (M (m,2,2),  c);
    M (m,2,0) = m0;
    M (m,2,2) = m2;

    m0 = CFX_MUL (M (m,3,0), c) + CFX_MUL (M (m,3,2), -s);
    m2 = CFX_MUL (M (m,3,0), s) + CFX_MUL (M (m,3,2),  c);
    M (m,3,0) = m0;
    M (m,3,2) = m2;
}

/* Apply scale by factors x, y to matrix */
static void
mtx_scale (ClutterFixed *m, ClutterFixed x, ClutterFixed y)
{
    M (m,0,0) = CFX_MUL (M (m,0,0), x);
    M (m,1,1) = CFX_MUL (M (m,1,1), y);
}

/* Transform point (x,y,z) by matrix */
static void
mtx_transform (ClutterFixed *m,
	       ClutterFixed *x, ClutterFixed *y, ClutterFixed *z)
{
    ClutterFixed _x, _y, _z;
    _x = *x;
    _y = *y;
    _z = *z;
    
    *x = CFX_MUL (M (m,0,0), _x) + CFX_MUL (M (m,0,1), _y) +
	CFX_MUL (M (m,0,2), _z) + M (m,0,3);

    *y = CFX_MUL (M (m,1,0), _x) + CFX_MUL (M (m,1,1), _y) +
	CFX_MUL (M (m,1,2), _z) + M (m,1,3);

    *z = CFX_MUL (M (m,2,0), _x) + CFX_MUL (M (m,2,1), _y) +
	CFX_MUL (M (m,2,2), _z) + M (m,2,3);
}

/**
 * clutter_actor_get_transformed_vertices:
 * @self: A #ClutterActor
 * @verts: Pointer to a location of #ClutterVertices where to store the result.
 * Calculates the vertices of the translated, rotated and scaled actor in 3D
 * space.
 *
 * Since: 0.4
 **/
void
clutter_actor_get_transformed_vertices (ClutterActor    * self,
					ClutterVertices * verts)
{
  ClutterActorPrivate  * priv;
  ClutterFixed           mtx[16];
  ClutterFixed           x, y, z;
  
  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  priv = self->priv;

  mtx_identity (&mtx[0]);

  /*
   * All the rotation ops are relative to the actor, not the overall
   * coordiante system; so first of all, we carry out a translation from
   * 0,0 to where our actor is.
   */
  mtx_translate (&mtx[0],
	     CLUTTER_UNITS_TO_FIXED (priv->coords.x1), 
	     CLUTTER_UNITS_TO_FIXED (priv->coords.y1), 
	     CLUTTER_INT_TO_FIXED (priv->z));

  if (self->priv->rzang)
    {
      mtx_translate (&mtx[0],
		 CLUTTER_INT_TO_FIXED (priv->rzx),
		 CLUTTER_INT_TO_FIXED (priv->rzy),
		 0);
      
      mtx_rotate_z (&mtx[0], priv->rzang);
      
      mtx_translate (&mtx[0],
		 CLUTTER_INT_TO_FIXED (-priv->rzx),
		 CLUTTER_INT_TO_FIXED (-priv->rzy),
		 0);
    }

  if (self->priv->ryang)
    {
      mtx_translate (&mtx[0],
		 CLUTTER_INT_TO_FIXED (priv->ryx),
		 0,
		 CLUTTER_INT_TO_FIXED (priv->z + priv->ryz));
      
      mtx_rotate_y (&mtx[0], priv->ryang);
      
      mtx_translate (&mtx[0],
		 CLUTTER_INT_TO_FIXED (-priv->ryx),
		 0,
		 CLUTTER_INT_TO_FIXED (-(priv->z + priv->ryz)));
    }

  if (self->priv->rxang)
    {
      mtx_translate (&mtx[0],
		 0,
		 CLUTTER_INT_TO_FIXED (priv->rxy),
		 CLUTTER_INT_TO_FIXED (priv->z + priv->rxz));
      
      mtx_rotate_x (&mtx[0], priv->rxang);
      
      mtx_translate (&mtx[0],
		 0,
		 CLUTTER_INT_TO_FIXED (-priv->rxy),
		 CLUTTER_INT_TO_FIXED (-(priv->z - priv->rxz)));
    }

  if (self->priv->z)
    mtx_translate (&mtx[0], 0, 0, CLUTTER_INT_TO_FIXED (priv->z));

  if (self->priv->scale_x != CFX_ONE || self->priv->scale_y != CFX_ONE)
    {
      mtx_scale (&mtx[0], priv->scale_x, priv->scale_y);
    }

#if 0
  g_debug ("Matrix\n"
	   "       %f, %f, %f, %f\n"
	   "       %f, %f, %f, %f\n"
	   "       %f, %f, %f, %f\n"
	   "       %f, %f, %f, %f",
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,0,0)),
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,0,1)),
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,0,2)),
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,0,3)),

	   CLUTTER_FIXED_TO_FLOAT (M(mtx,1,0)),
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,1,1)),
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,1,2)),
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,1,3)),
	   
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,2,0)),
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,2,1)),
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,2,2)),
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,2,3)),

	   CLUTTER_FIXED_TO_FLOAT (M(mtx,3,0)),
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,3,1)),
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,3,2)),
	   CLUTTER_FIXED_TO_FLOAT (M(mtx,3,3)));
#endif
  
  x = 0;
  y = 0;
  z = 0;
      
  mtx_transform (&mtx[0], &x, &y, &z);

  verts->topleft.x = CLUTTER_UNITS_FROM_FIXED (x);
  verts->topleft.y = CLUTTER_UNITS_FROM_FIXED (y);
  verts->topleft.z = CLUTTER_UNITS_FROM_FIXED (z);

  x = CLUTTER_UNITS_TO_FIXED (priv->coords.x2 - priv->coords.x1);
  y = CLUTTER_UNITS_TO_FIXED (priv->coords.y2 - priv->coords.y1);
  z = CLUTTER_INT_TO_FIXED (priv->z);

  mtx_transform (&mtx[0], &x, &y, &z);
  
  verts->bottomright.x = CLUTTER_UNITS_FROM_FIXED (x);
  verts->bottomright.y = CLUTTER_UNITS_FROM_FIXED (y);
  verts->bottomright.z = CLUTTER_UNITS_FROM_FIXED (z);

  x = 0;
  y = CLUTTER_UNITS_TO_FIXED (priv->coords.y2 - priv->coords.y1);
  z = CLUTTER_INT_TO_FIXED (priv->z);
  
  mtx_transform (&mtx[0], &x, &y, &z);
  
  verts->bottomleft.x = CLUTTER_UNITS_FROM_FIXED (x);
  verts->bottomleft.y = CLUTTER_UNITS_FROM_FIXED (y);
  verts->bottomleft.z = CLUTTER_UNITS_FROM_FIXED (z);
  
  x = CLUTTER_UNITS_TO_FIXED (priv->coords.x2 - priv->coords.x1);
  y = 0;
  z = CLUTTER_INT_TO_FIXED (priv->z);
  
  mtx_transform (&mtx[0], &x, &y, &z);
  
  verts->topright.x = CLUTTER_UNITS_FROM_FIXED (x);
  verts->topright.y = CLUTTER_UNITS_FROM_FIXED (y);
  verts->topright.z = CLUTTER_UNITS_FROM_FIXED (z);

#if 0
  if (priv->coords.x1 != verts->topleft.x ||
      priv->coords.y1 != verts->topleft.y ||
      priv->coords.x2 != verts->bottomright.x ||
      priv->coords.y2 != verts->bottomright.y)
      g_debug ("Box [%f,%f],[%f,%f], \n"
	       "                rxy %d, rxz %d, rxa %f\n"
	       "                ryx %d, ryz %d, rya %f\n"
	       "                rzx %d, rzy %d, rza %f\n"
	       "Vertices: \n"
	       " tl [%f,%f,%f], tr [%f,%f,%f] \n"
	       " bl [%f,%f,%f], br [%f,%f,%f] \n",
	       CLUTTER_FIXED_TO_FLOAT (priv->coords.x1),
	       CLUTTER_FIXED_TO_FLOAT (priv->coords.y1),
	       CLUTTER_FIXED_TO_FLOAT (priv->coords.x2),
	       CLUTTER_FIXED_TO_FLOAT (priv->coords.y2),
	       priv->rxy, priv->rxz, CLUTTER_FIXED_TO_FLOAT (priv->rxang),
	       priv->ryx, priv->ryz, CLUTTER_FIXED_TO_FLOAT (priv->ryang),
	       priv->rzx, priv->rzy, CLUTTER_FIXED_TO_FLOAT (priv->rzang),
	       CLUTTER_FIXED_TO_FLOAT (verts->topleft.x),
	       CLUTTER_FIXED_TO_FLOAT (verts->topleft.y),
	       CLUTTER_FIXED_TO_FLOAT (verts->topleft.z),
	       CLUTTER_FIXED_TO_FLOAT (verts->topright.x),
	       CLUTTER_FIXED_TO_FLOAT (verts->topright.y),
	       CLUTTER_FIXED_TO_FLOAT (verts->topright.z),
	       CLUTTER_FIXED_TO_FLOAT (verts->bottomleft.x),
	       CLUTTER_FIXED_TO_FLOAT (verts->bottomleft.y),
	       CLUTTER_FIXED_TO_FLOAT (verts->bottomleft.z),
	       CLUTTER_FIXED_TO_FLOAT (verts->bottomright.x),
	       CLUTTER_FIXED_TO_FLOAT (verts->bottomright.y),
	       CLUTTER_FIXED_TO_FLOAT (verts->bottomright.z));
#endif
}

#undef M

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
#if 1
  ClutterVertices verts;
#endif
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

#if 1
  clutter_actor_get_transformed_vertices (self, &verts);
#endif
  
  context = clutter_context_get_default ();
  klass   = CLUTTER_ACTOR_GET_CLASS (self);

  cogl_push_matrix();

  if (clutter_actor_get_parent (self) != NULL)
    {
      cogl_translate (CLUTTER_UNITS_TO_INT (priv->coords.x1), 
		      CLUTTER_UNITS_TO_INT (priv->coords.y1), 
		      0);
    }

  if (self->priv->rzang)
    {
      cogl_translate (priv->rzx, priv->rzy, 0);
      cogl_rotatex (priv->rzang, 0, 0, CFX_ONE);
      cogl_translate (-priv->rzx, -priv->rzy, 0);
    }

  if (self->priv->ryang)
    {
      cogl_translate (priv->ryx, 0, priv->z + priv->ryz);
      cogl_rotatex (priv->ryang, 0, CFX_ONE, 0);
      cogl_translate (-priv->ryx, 0, -(priv->z + priv->ryz));
    }

  if (self->priv->rxang)
    {
      cogl_translate (0, priv->rxy, priv->z + priv->rxz);
      cogl_rotatex (priv->rxang, CFX_ONE, 0, CFX_ONE);
      cogl_translate (0, -priv->rxy, -(priv->z - priv->rxz));
    }

  if (self->priv->z)
    cogl_translate (0, 0, priv->z);

  if (self->priv->scale_x != CFX_ONE ||
      self->priv->scale_y != CFX_ONE)
    {
      cogl_scale (priv->scale_x, priv->scale_y);
    }

  if (priv->has_clip)
    cogl_clip_set (&(priv->clip));

  if (G_UNLIKELY(context->pick_mode == TRUE))
    {
      ClutterColor col;
      guint32      id;

      id = clutter_actor_get_id (self);

      /* Encode the actor id into a color */
      col.red   = (id >> 16) & 0xff;
      col.green = (id >> 8) & 0xff;
      col.blue  =  id & 0xff;
      col.alpha = 0xff;

      /* Actor will then paint silhouette of itself in supplied color.
       * See clutter_stage_get_actor_at_pos() for where picking is enabled.  
      */
      clutter_actor_pick (self, &col);
    }
  else
    {
      if (G_LIKELY(klass->paint))
	(klass->paint) (self);
    }

  if (priv->has_clip)
    cogl_clip_unset();

  if (priv->scale_x != CFX_ONE || priv->scale_y != CFX_ONE)
    cogl_scale (CFX_ONE, CFX_ONE);

  cogl_pop_matrix();
}

/**
 * clutter_actor_request_coords:
 * @self: A #ClutterActor
 * @box: A #ClutterActorBox with requested new co-ordinates.
 *
 * Requests new co-ordinates for the #ClutterActor ralative to any parent.
 *
 * This function should not be called directly by applications instead 
 * the various position/geometry methods should be used.
 **/
void
clutter_actor_request_coords (ClutterActor    *self,
			      ClutterActorBox *box)
{
  ClutterActorClass *klass;
  gboolean x_change, y_change, width_change, height_change;
  
  klass = CLUTTER_ACTOR_GET_CLASS (self);

  if (klass->request_coords)
    klass->request_coords (self, box);
  
  x_change     = (self->priv->coords.x1 != box->x1);
  y_change     = (self->priv->coords.y1 != box->y1);
  width_change = ((self->priv->coords.x2 - self->priv->coords.x1) != 
      (box->x2 - box->x1));
  height_change = ((self->priv->coords.y2 - self->priv->coords.y1) !=
		   (box->y2 - box->y1));

  if (x_change || y_change || width_change || height_change)
    {
      self->priv->coords.x1 = box->x1;
      self->priv->coords.y1 = box->y1; 
      self->priv->coords.x2 = box->x2; 
      self->priv->coords.y2 = box->y2; 
      
      if (CLUTTER_ACTOR_IS_VISIBLE (self))
	clutter_actor_queue_redraw (self);
      
      g_object_ref (self);
      g_object_freeze_notify (G_OBJECT (self));
      
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
 * clutter_actor_allocate_coords:
 * @self: A #ClutterActor
 * @box: A location to store the actors #ClutterActorBox co-ordinates
 *
 * Requests the allocated co-ordinates for the #ClutterActor relative 
 * to any parent.
 *
 * This function should not be called directly by applications instead 
 * the various position/geometry methods should be used.
 **/
void
clutter_actor_allocate_coords (ClutterActor    *self,
			       ClutterActorBox *box)
{
  ClutterActorClass *klass;

  klass = CLUTTER_ACTOR_GET_CLASS (self);

  box->x1 = self->priv->coords.x1;
  box->y1 = self->priv->coords.y1;
  box->x2 = self->priv->coords.x2;
  box->y2 = self->priv->coords.y2;

  if (klass->allocate_coords)
    {
      /* FIXME: This is kind of a cludge - we pass out *private* 
       *        co-ords down to any subclasses so they can modify
       *        we then resync any changes. Needed for group class.
       *        Need to figure out nicer way.
      */
      klass->allocate_coords(self, box);

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
      clutter_actor_set_position (actor, 
				    g_value_get_int (value), 
				    clutter_actor_get_y (actor));
      break;
    case PROP_Y:
      clutter_actor_set_position (actor, 
				    clutter_actor_get_x (actor),
				    g_value_get_int (value));
      break;
    case PROP_WIDTH:
      clutter_actor_set_size (actor, 
				g_value_get_int (value),
				clutter_actor_get_height (actor));
      break;
    case PROP_HEIGHT:
      clutter_actor_set_size (actor, 
				clutter_actor_get_width (actor),
				g_value_get_int (value));
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
    case PROP_CLIP:
      {
        ClutterGeometry *geom = g_value_get_boxed (value);
	
	clutter_actor_set_clip (actor,
				geom->x, geom->y,
				geom->width, geom->height);
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
      g_value_set_boxed (value, &(priv->clip));
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

  CLUTTER_NOTE (MISC, "Disposing of object (id=%d) of type `%s'",
		self->priv->id,
		g_type_name (G_OBJECT_TYPE (self)));
  
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
                                                       G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE));
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

  klass->show = clutter_actor_real_show;
  klass->show_all = clutter_actor_show;
  klass->hide = clutter_actor_real_hide;
  klass->hide_all = clutter_actor_hide;
}

static void
clutter_actor_init (ClutterActor *self)
{
  self->priv = CLUTTER_ACTOR_GET_PRIVATE (self); 

  self->priv->parent_actor = NULL;
  self->priv->has_clip     = FALSE;
  self->priv->opacity      = 0xff;
  self->priv->id           = __id++;
  self->priv->scale_x      = CFX_ONE;
  self->priv->scale_y      = CFX_ONE;

  clutter_actor_set_position (self, 0, 0);
  clutter_actor_set_size (self, 0, 0);
}

/**
 * clutter_actor_destroy:
 * @self: a #ClutterActor
 *
 * Destroys an actor.  When an actor is destroyed, it will break any
 * references it holds to other objects.  If the actor is inside a
 * group, the actor will be removed from the group.
 *
 * When you destroy a group its children will be destroyed as well.
 */
void
clutter_actor_destroy (ClutterActor *self)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (!(CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IN_DESTRUCTION))
    g_object_run_dispose (G_OBJECT (self));
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
      ctx->update_idle = g_idle_add_full (-100 , /* very high priority */
					  redraw_update_idle, 
					  NULL, NULL);
    }
}

/**
 * clutter_actor_set_geometry:
 * @self: A #ClutterActor
 * @geometry: A #ClutterGeometry
 *
 * Sets the actors geometry in pixels relative to any parent actor.
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
 * Gets the actors geometry in pixels relative to any parent actor.
 */
void
clutter_actor_get_geometry (ClutterActor    *self,
			    ClutterGeometry *geometry)
{
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  
  clutter_actor_allocate_coords (self, &box);

  geometry->x      = CLUTTER_UNITS_TO_INT (box.x1);
  geometry->y      = CLUTTER_UNITS_TO_INT (box.y1);
  geometry->width  = CLUTTER_UNITS_TO_INT (box.x2 - box.x1);
  geometry->height = CLUTTER_UNITS_TO_INT (box.y2 - box.y1);
}

/**
 * clutter_actor_get_coords:
 * @self: A #ClutterActor
 * @x1: A location to store actors left position if non NULL.
 * @y1: A location to store actors top position if non NULL.
 * @x2: A location to store actors right position if non NULL.
 * @y2: A location to store actors bottom position if non NULL.
 *
 * Gets the actors bounding rectangle co-ordinates in pixels 
 * relative to any parent actor. 
 */
void
clutter_actor_get_coords (ClutterActor *self,
			  gint         *x1,
			  gint         *y1,
			  gint         *x2,
			  gint         *y2)
{
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  
  clutter_actor_allocate_coords (self, &box);

  if (x1)
    *x1 = CLUTTER_UNITS_TO_INT (box.x1);
  
  if (y1)
    *y1 = CLUTTER_UNITS_TO_INT (box.y1);
  
  if (x2)
    *x2 = CLUTTER_UNITS_TO_INT (box.x2);
  
  if (y2)
    *y2 = CLUTTER_UNITS_TO_INT (box.y2);
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

  clutter_actor_allocate_coords (self, &box);

  box.x2 += (CLUTTER_UNITS_FROM_INT (x) - box.x1);
  box.y2 += (CLUTTER_UNITS_FROM_INT (y) - box.y1);

  box.x1 = CLUTTER_UNITS_FROM_INT (x);
  box.y1 = CLUTTER_UNITS_FROM_INT (y);

  clutter_actor_request_coords (self, &box);
}

/**
 * clutter_actor_move_by
 * @self: A #ClutterActor
 * @dx: Distance to move Actor on X axis.
 * @dy: Distance to move Actor on Y axis.
 *
 * Moves an actor by specified distance relative to 
 * current position.
 *
 * Since: 0.2
 */
void
clutter_actor_move_by (ClutterActor *self,
		       gint          dx,
		       gint          dy)
{
  ClutterActorBox box;
  gint32 dxu = CLUTTER_UNITS_FROM_INT (dx);
  gint32 dyu = CLUTTER_UNITS_FROM_INT (dy);
  
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_allocate_coords (self, &box);

  box.x2 += dxu;
  box.y2 += dyu;
  box.x1 += dxu;
  box.y1 += dyu;

  clutter_actor_request_coords (self, &box);
}

/**
 * clutter_actor_set_size
 * @self: A #ClutterActor
 * @width: New width of actor in pixels 
 * @height: New height of actor in pixels
 *
 * Sets the actors position in pixels relative to any
 * parent actor. 
 */
void
clutter_actor_set_size (ClutterActor *self,
			gint          width,
			gint          height)
{
  ClutterActorBox box;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_allocate_coords (self, &box);

  box.x2 = box.x1 + CLUTTER_UNITS_FROM_INT (width);
  box.y2 = box.y1 + CLUTTER_UNITS_FROM_INT (height);

  clutter_actor_request_coords (self, &box);
}

/**
 * clutter_actor_get_size:
 * @self: A #ClutterActor
 * @width: Location to store width if non NULL.
 * @height: Location to store height if non NULL.
 *
 * Gets the size of an actor ignoring any scaling factors
 *
 * Since: 0.2
 */
void
clutter_actor_get_size (ClutterActor *self,
			guint        *width,
			guint        *height)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  if (width)
    *width = clutter_actor_get_width (self);

  if (height)
    *height = clutter_actor_get_height (self);
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
  ClutterActorBox   box;
  ClutterActor     *parent;
  gint32            px = 0, py = 0;
  
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  clutter_actor_allocate_coords (self, &box);

  parent = self->priv->parent_actor;

  /* FIXME: must be nicer way to get 0,0 for stage ? */
  if (parent)
    {
      ClutterFixed parent_scale_x, parent_scale_y;

      clutter_actor_get_scalex (parent,
                                &parent_scale_x,
                                &parent_scale_y);

      if (parent_scale_x != CFX_ONE ||
          parent_scale_y != CFX_ONE)
	{
	  box.x1 = CFX_MUL (box.x1, parent_scale_x); 
	  box.y1 = CFX_MUL (box.y1, parent_scale_y); 
	}

      if (!CLUTTER_IS_STAGE (parent))
	clutter_actor_get_abs_position_units (parent, &px, &py);
    }

  if (x)
    *x = px + box.x1;
  
  if (y)
    *y = py + box.y1;
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
				gint           *x,
				gint           *y)
{
  gint32 xu, yu;
  clutter_actor_get_abs_position_units (self, &xu, &yu);

  *x = CLUTTER_UNITS_TO_INT (xu);
  *y = CLUTTER_UNITS_TO_INT (yu);
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
 * Since: 0.4
 */
static void
clutter_actor_get_abs_size_units (ClutterActor *self,
				  gint32       *width,
				  gint32       *height)
{
  ClutterActorBox  box;
  ClutterActor    *parent;

  clutter_actor_allocate_coords (self, &box);

  if (width)
    *width  = box.x2 - box.x1;

  if (height)
    *height = box.y2 - box.y1;

  parent = self;

  do
    {
      if (parent->priv->scale_x != CFX_ONE ||
          parent->priv->scale_y != CFX_ONE)
	{
	  ClutterFixed fx, fy;

	  if (width)
	    {
	      fx = CLUTTER_FIXED_MUL (CLUTTER_UNITS_TO_FIXED (*width),
				      parent->priv->scale_x); 
	      *width = CLUTTER_UNITS_FROM_FIXED (fx);
	    }

	  if (height)
	    {
	      fy = CLUTTER_FIXED_MUL (CLUTTER_UNITS_TO_FIXED (*height),
				      parent->priv->scale_x); 
	      *height = CLUTTER_UNITS_FROM_FIXED (fy);
	    }
	}
    }
  while ((parent = clutter_actor_get_parent (parent)) != NULL);
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
  
  *width  = CLUTTER_UNITS_TO_INT (wu);
  *height = CLUTTER_UNITS_TO_INT (hu);
}


/**
 * clutter_actor_get_width
 * @self: A #ClutterActor
 *
 * Retrieves the actors width.
 *
 * Return value: The actor width in pixels
 **/
guint
clutter_actor_get_width (ClutterActor *self)
{
  ClutterActorBox box;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);
  
  clutter_actor_allocate_coords (self, &box);

  return CLUTTER_UNITS_TO_INT (box.x2 - box.x1);
}

/**
 * clutter_actor_get_height
 * @self: A #ClutterActor
 *
 * Retrieves the actors height.
 * 
 * Return value: The actor height in pixels
 **/
guint
clutter_actor_get_height (ClutterActor *self)
{
  ClutterActorBox box;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);
  
  clutter_actor_allocate_coords (self, &box);

  return CLUTTER_UNITS_TO_INT (box.y2 - box.y1);
}

/**
 * clutter_actor_set_width
 * @self: A #ClutterActor
 * @width: Requested new width for actor
 * 
 * Requests a new width for actor
 *
 * since: 2.0
 **/
void
clutter_actor_set_width (ClutterActor *self, guint width)
{
  clutter_actor_set_size (self, 
			  width,
			  clutter_actor_get_height (self));
}

/**
 * clutter_actor_set_height
 * @self: A #ClutterActor
 * @height: Requested new height for actor
 * 
 * Requests a new height for actor
 *
 * since: 2.0
 **/
void
clutter_actor_set_height (ClutterActor *self, guint height)
{
  clutter_actor_set_size (self, 
			  clutter_actor_get_width (self),
			  height);
}


/**
 * clutter_actor_get_x
 * @self: A #ClutterActor
 *
 * Retrieves the actors x position relative to any parent.
 *
 * Return value: The actor x position in pixels
 **/
gint
clutter_actor_get_x (ClutterActor *self)
{
  ClutterActorBox box;
  
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  clutter_actor_allocate_coords (self, &box);

  return CLUTTER_UNITS_TO_INT (box.x1);
}

/**
 * clutter_actor_get_y:
 * @self: A #ClutterActor
 *
 * Retrieves the actors y position relative to any parent.
 *
 * Return value: The actor y position in pixels
 **/
gint
clutter_actor_get_y (ClutterActor *self)
{
  ClutterActorBox box;

  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);

  clutter_actor_allocate_coords (self, &box);

  return CLUTTER_UNITS_TO_INT (box.y1);
}

/**
 * clutter_actor_set_scalex:
 * @self: A #ClutterActor
 * @scale_x: #ClutterFixed factor to scale actor by horizontally.
 * @scale_y: #ClutterFixed factor to scale actor by vertically.
 *
 * Scale an actor.
 */
void
clutter_actor_set_scalex (ClutterActor *self,
			  ClutterFixed  scale_x,
			  ClutterFixed  scale_y)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  self->priv->scale_x = scale_x;
  self->priv->scale_y = scale_y;

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (self);
}

/**
 * clutter_actor_set_scale:
 * @self: A #ClutterActor
 * @scale_x: double
 * @scale_y: double
 *
 * FIXME
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
 * @scale_x: FIXME
 * @scale_y: FIXME
 *
 * FIXME
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
 * @scale_x: FIXME
 * @scale_y: FIXME
 *
 * FIXME
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
 * clutter_actor_set_scale_with_gravity:
 * @self: A #ClutterActor
 * @scale_x: scaling factor for x axis
 * @scale_y: scaling factor for y axis
 * @gravity: #ClutterGravity to apply to scaling.
 *
 * Scales the actor by scale_x, scale_y taking into consideration the
 * required gravity.
 *
 * Since: 0.4
 */
void
clutter_actor_set_scale_with_gravity (ClutterActor     *self,
				  gfloat            scale_x,
				  gfloat            scale_y,
				  ClutterGravity    gravity)
{
    clutter_actor_set_scale_with_gravityx (self,
					   CLUTTER_FLOAT_TO_FIXED (scale_x),
					   CLUTTER_FLOAT_TO_FIXED (scale_y),
					   gravity);
}

/**
 * clutter_actor_set_scale_with_gravityx:
 * @self: A #ClutterActor
 * @scale_x: #ClutterFixed scaling factor for x axis
 * @scale_y: #ClutterFixed scaling factor for y axis
 * @gravity: #ClutterGravity to apply to scaling.
 *
 * Scales the actor by scale_x, scale_y taking into consideration the
 * required gravity.
 *
 * Since: 0.4
 */
void
clutter_actor_set_scale_with_gravityx (ClutterActor     *self,
				       ClutterFixed      scale_x,
				       ClutterFixed      scale_y,
				       ClutterGravity    gravity)
{
  ClutterActorBox box;
  gint32 sw, sh, w, h;
  gint32 x, y;

  clutter_actor_get_abs_size_units (self, &w, &h);
  clutter_actor_set_scalex (self, scale_x, scale_y);

  if (gravity == CLUTTER_GRAVITY_NONE ||
      gravity == CLUTTER_GRAVITY_NORTH_WEST)
    return;

  clutter_actor_get_abs_size_units (self, &sw, &sh);

  clutter_actor_allocate_coords (self, &box);
  x = box.x1;
  y = box.y1;
  
  switch (gravity)
    {
    case CLUTTER_GRAVITY_NORTH:
      x = x - ((sw - w) / 2);
      break;
    case CLUTTER_GRAVITY_NORTH_EAST:
      x = x + w - sw;
      break;
    case CLUTTER_GRAVITY_EAST:
      x = x + w - sw;
      y = y - ((sh - h) / 2);
      break;
    case CLUTTER_GRAVITY_SOUTH_EAST:
      x = x + w - sw;
      y = y + h - sh;
      break;
    case CLUTTER_GRAVITY_SOUTH:
      x = x - ((sw - w) / 2);
      y = y + h - sh;
      break;
    case CLUTTER_GRAVITY_SOUTH_WEST:
      y = y + h - sh;
      break;
    case CLUTTER_GRAVITY_WEST:
      y = y - ((sh - h) / 2);
      break;
    case CLUTTER_GRAVITY_CENTER:
      x = x - ((sw - w) / 2);
      y = y - ((sh - h) / 2);
    default:
      break;
    }

  box.x2 += (x - box.x1);
  box.y2 += (y - box.y1);
  box.x1 = x;
  box.y1 = y;

  clutter_actor_request_coords (self, &box);
}

/**
 * clutter_actor_set_opacity:
 * @self: A #ClutterActor
 * @opacity: New opacity value for actor.
 *
 * Sets the actors opacity, with zero being completely transparent.
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
 * clutter_actor_get_id:
 * @self: A #ClutterActor
 *
 * Retrieves the unique id for @self.
 * 
 * Return value: Globally unique value for object instance.
 */
guint32
clutter_actor_get_id (ClutterActor *self)
{
  g_return_val_if_fail (CLUTTER_IS_ACTOR (self), 0);
  
  return self->priv->id;
}

/**
 * clutter_actor_set_depth:
 * @self: a #ClutterActor
 * @depth: Z co-ord
 *
 * Sets the Z co-ordinate of @self to @depth.
 */
void
clutter_actor_set_depth (ClutterActor *self,
                         gint          depth)
{
  ClutterActorPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  priv = self->priv;

  /* Sets Z value. - FIXME: should invert ?*/
  priv->z = depth;

  if (priv->parent_actor)
    {
      /* We need to resort the group stacking order as to
       * correctly render alpha values. 
       *
       * FIXME: This is sub optimal. maybe queue the the sort 
       *        before stacking  
      */
      clutter_group_sort_depth_order (CLUTTER_GROUP (priv->parent_actor));
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
  
  return self->priv->z;
}

/**
 * clutter_actor_rotate_z:
 * @self: A #ClutterActor
 * @angle: Angle of rotation
 * @x:     X co-ord to rotate actor around ( relative to actor position )
 * @y:     Y co-ord to rotate actor around ( relative to actor position )
 *
 * Rotates actor around the Z axis.
 */
void
clutter_actor_rotate_z (ClutterActor *self,
			gfloat        angle,
			gint          x,
			gint          y)
{
  /* FIXME: FIXED VERSION */
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  self->priv->rzang = CLUTTER_FLOAT_TO_FIXED (angle);
  self->priv->rzx   = x;
  self->priv->rzy   = y;

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (self);
}

/**
 * clutter_actor_rotate_x:
 * @self:  A #ClutterActor
 * @angle: Angle of rotation
 * @y:     Y co-ord to rotate actor around ( relative to actor position )
 * @z:     Z co-ord to rotate actor around ( relative to actor position )
 *
 * Rotates actor around the X axis.
 */
void
clutter_actor_rotate_x (ClutterActor *self,
			gfloat        angle,
			gint          y,
			gint          z)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  
  /* FIXME: FIXED API Version */

  self->priv->rxang = CLUTTER_FLOAT_TO_FIXED(angle);
  self->priv->rxy   = y;
  self->priv->rxz   = z;

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (self);
}

/**
 * clutter_actor_rotate_y:
 * @self:  A #ClutterActor
 * @angle: Angle of rotation
 * @x:     X co-ord to rotate actor around ( relative to actor position )
 * @z:     Z co-ord to rotate actor around ( relative to actor position )
 *
 * Rotates actor around the X axis.
 */
void
clutter_actor_rotate_y (ClutterActor *self,
			gfloat        angle,
			gint          x,
			gint          z)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));

  /* FIXME: FIXED API VERSION */
  
  self->priv->ryang = CLUTTER_FLOAT_TO_FIXED(angle);

  self->priv->ryx   = x;
  self->priv->ryz   = z;

  if (CLUTTER_ACTOR_IS_VISIBLE (self))
    clutter_actor_queue_redraw (self);
}


/**
 * clutter_actor_set_clip:
 * @self: A #ClutterActor
 * @xoff: X offset of the clip rectangle
 * @yoff: Y offset of the clip rectangle
 * @width: Width of the clip rectangle
 * @height: Height of the clip rectangle
 *
 * Sets clip area for @self.
 */
void
clutter_actor_set_clip (ClutterActor *self,
			gint          xoff, 
			gint          yoff, 
			gint          width, 
			gint          height)
{
  ClutterGeometry *clip;
  
  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  
  clip = &(self->priv->clip);
  
  clip->x      = xoff;
  clip->y      = yoff;
  clip->width  = width;
  clip->height = height;

  self->priv->has_clip = TRUE;

  g_object_notify (G_OBJECT (self), "has-clip");
  g_object_notify (G_OBJECT (self), "clip");
} 

/**
 * clutter_actor_remove_clip
 * @self: A #ClutterActor
 *
 * Removes clip area from @self.
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
 * clutter_actor_set_parent:
 * @self: A #ClutterActor
 * @parent: A new #ClutterActor parent
 *
 * Sets the parent of @self to @parent.  The opposite function is
 * clutter_actor_unparent().
 * 
 * This function should not be used by applications.
 */
void
clutter_actor_set_parent (ClutterActor *self,
		          ClutterActor *parent)
{
  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (CLUTTER_IS_ACTOR (parent));
  g_return_if_fail (self != parent);

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
 * implementations of group actors, to dissociate a child from the container.
 *
 * Since: 0.1.1
 */
void
clutter_actor_unparent (ClutterActor *self)
{
  ClutterActor *old_parent;

  g_return_if_fail (CLUTTER_IS_ACTOR (self));

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
  g_return_if_fail (CLUTTER_IS_ACTOR (self));
  g_return_if_fail (CLUTTER_IS_ACTOR (new_parent));
  g_return_if_fail (self != new_parent);
  
  if (CLUTTER_PRIVATE_FLAGS (self) & CLUTTER_ACTOR_IS_TOPLEVEL)
    {
      g_warning ("Cannot set a parent on a toplevel actor\n");
      return;
    }

  if (self->priv->parent_actor != new_parent)
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

      old_parent = self->priv->parent_actor;

      g_object_ref (self);
      /* FIXME: below assumes only groups can reparent
      */
      clutter_group_remove (CLUTTER_GROUP (self->priv->parent_actor), self);
      clutter_group_add (CLUTTER_GROUP (new_parent), self);
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
      g_warning ("Actor of type %s is not inside a group",
                 g_type_name (G_OBJECT_TYPE (self)));
      return;
    }

  if (below)
    {
      if (parent != clutter_actor_get_parent (below))
        {
          g_warning ("Actor of type %s is not in the same "
                     "group of actor of type %s",
                     g_type_name (G_OBJECT_TYPE (self)),
                     g_type_name (G_OBJECT_TYPE (below)));
          return;
        }
    }

  clutter_group_raise (CLUTTER_GROUP (parent), self, below);
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
      g_warning ("Actor of type %s is not inside a group",
                 g_type_name (G_OBJECT_TYPE (self)));
      return;
    }

  if (above)
    {
      if (parent != clutter_actor_get_parent (above))
        {
          g_warning ("Actor of type %s is not in the same "
                     "group of actor of type %s",
                     g_type_name (G_OBJECT_TYPE (self)),
                     g_type_name (G_OBJECT_TYPE (above)));
          return;
        }
    }

  /* FIXME: group_lower should be an overidable method ? */
  clutter_group_lower (CLUTTER_GROUP (parent), self, above);
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
 * ClutterGemoetry
 */

static ClutterGeometry*
clutter_geometry_copy (const ClutterGeometry *geometry)
{
  ClutterGeometry *result = g_new (ClutterGeometry, 1);

  *result = *geometry;

  return result;
}

GType
clutter_geometry_get_type (void)
{
  static GType our_type = 0;

  if (our_type == 0)
    our_type = g_boxed_type_register_static (
              g_intern_static_string ("ClutterGeometry"),
	      (GBoxedCopyFunc) clutter_geometry_copy,
	      (GBoxedFreeFunc) g_free);

  return our_type;
}

/*
 * ClutterVertices
 */

static ClutterVertices*
clutter_vertices_copy (const ClutterVertices *vertices)
{
  ClutterVertices *result = g_new (ClutterVertices, 1);

  *result = *vertices;

  return result;
}

GType
clutter_vertices_get_type (void)
{
  static GType our_type = 0;

  if (our_type == 0)
    our_type = g_boxed_type_register_static (
              g_intern_static_string ("ClutterVertices"),
	      (GBoxedCopyFunc) clutter_vertices_copy,
	      (GBoxedFreeFunc) g_free);

  return our_type;
}

/*
 * ClutterActorBox
 */
static ClutterActorBox *
clutter_actor_box_copy (const ClutterActorBox *box)
{
  ClutterActorBox *result = g_new (ClutterActorBox, 1);

  *result = *box;

  return result;
}

GType
clutter_actor_box_get_type (void)
{
  static GType our_type = 0;

  if (our_type == 0)
    our_type = g_boxed_type_register_static (
              g_intern_static_string ("ClutterActorBox"),
	      (GBoxedCopyFunc) clutter_actor_box_copy,
	      (GBoxedFreeFunc) g_free);
  return our_type;
}

