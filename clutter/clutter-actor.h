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

#ifndef _HAVE_CLUTTER_ACTOR_H
#define _HAVE_CLUTTER_ACTOR_H

/* clutter-actor.h */

#include <glib-object.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_GEOMETRY (clutter_geometry_get_type ())
#define CLUTTER_TYPE_ACTOR_BOX (clutter_actor_box_get_type ())

#define CLUTTER_TYPE_ACTOR clutter_actor_get_type()

#define CLUTTER_ACTOR(obj) \
 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ACTOR, ClutterActor))
#define CLUTTER_ACTOR_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ACTOR, ClutterActorClass))
#define CLUTTER_IS_ACTOR(obj) \
 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ACTOR))
#define CLUTTER_IS_ACTOR_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ACTOR))
#define CLUTTER_ACTOR_GET_CLASS(obj) \
 (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ACTOR, ClutterActorClass))

#define CLUTTER_ACTOR_SET_FLAGS(e,f) ((e)->flags |= (f))  
#define CLUTTER_ACTOR_UNSET_FLAGS(e,f) ((e)->flags &= ~(f))  

#define CLUTTER_ACTOR_IS_MAPPED(e) ((e)->flags & CLUTTER_ACTOR_MAPPED)  
#define CLUTTER_ACTOR_IS_REALIZED(e) ((e)->flags & CLUTTER_ACTOR_REALIZED)
#define CLUTTER_ACTOR_IS_VISIBLE(e) \
 (CLUTTER_ACTOR_IS_MAPPED(e) && CLUTTER_ACTOR_IS_REALIZED(e))  

typedef struct _ClutterActor         ClutterActor;
typedef struct _ClutterActorClass    ClutterActorClass;
typedef struct _ClutterActorBox      ClutterActorBox;
typedef struct _ClutterActorPrivate  ClutterActorPrivate;
typedef struct _ClutterGeometry        ClutterGeometry;

typedef void (*ClutterCallback) (ClutterActor *actor, gpointer data);

struct _ClutterGeometry
{ 
  gint x;
  gint y;
  guint width;
  guint height;
};

GType clutter_geometry_get_type (void) G_GNUC_CONST;

typedef enum 
{
  CLUTTER_ACTOR_MIRROR_X = 1 << 1,
  CLUTTER_ACTOR_MIRROR_Y = 1 << 2
} ClutterActorTransform;

typedef enum
{
  CLUTTER_ACTOR_MAPPED   = 1 << 1,
  CLUTTER_ACTOR_REALIZED = 1 << 2
} ClutterActorFlags;

struct _ClutterActorBox { gint x1, y1, x2, y2; };

GType clutter_actor_box_get_type (void) G_GNUC_CONST;

struct _ClutterActor
{
  /*< public >*/
  GObject                 parent;
  guint32                 flags;

  /*< private >*/
  ClutterActorPrivate  *priv;
};

struct _ClutterActorClass
{
  GObjectClass parent_class;

  void (* show)                (ClutterActor        *actor);
  void (* hide)                (ClutterActor        *actor);
  void (* realize)             (ClutterActor        *actor);
  void (* unrealize)           (ClutterActor        *actor);
  void (* paint)               (ClutterActor        *actor);
  void (* request_coords)      (ClutterActor        *actor,
				ClutterActorBox     *box);
  void (* allocate_coords)     (ClutterActor        *actor,
				ClutterActorBox     *box);
  void (* set_depth)           (ClutterActor        *actor,
		                gint                   depth);
  gint (* get_depth)           (ClutterActor        *actor);

  /* to go ? */
  void (* show_all)            (ClutterActor        *actor);
  void (* hide_all)            (ClutterActor        *actor);
  void (* queue_redraw)        (ClutterActor        *actor);
};

GType clutter_actor_get_type (void);

void
clutter_actor_show (ClutterActor *self);

void
clutter_actor_hide (ClutterActor *self);

void
clutter_actor_realize (ClutterActor *self);

void
clutter_actor_unrealize (ClutterActor *self);

void
clutter_actor_paint (ClutterActor *self);

void
clutter_actor_queue_redraw (ClutterActor  *self);

void
clutter_actor_request_coords (ClutterActor    *self,
				ClutterActorBox *box);

void
clutter_actor_allocate_coords (ClutterActor    *self,
				 ClutterActorBox *box);

void
clutter_actor_set_geometry (ClutterActor  *self,
			      ClutterGeometry *geom);

void
clutter_actor_get_geometry (ClutterActor  *self,
			      ClutterGeometry *geom);

void
clutter_actor_get_coords (ClutterActor *self,
			    gint           *x1,
			    gint           *y1,
			    gint           *x2,
			    gint           *y2);

void
clutter_actor_set_position (ClutterActor *self,
			      gint            x,
			      gint            y);

void
clutter_actor_set_size (ClutterActor *self,
			  gint            width,
			  gint            height);

void
clutter_actor_get_abs_position (ClutterActor *self,
				  gint           *x,
				  gint           *y);

guint
clutter_actor_get_width (ClutterActor *self);

guint
clutter_actor_get_height (ClutterActor *self);

gint
clutter_actor_get_x (ClutterActor *self);

gint
clutter_actor_get_y (ClutterActor *self);

void
clutter_actor_rotate_z (ClutterActor          *self,
			  gfloat                   angle,
			  gint                     x,
			  gint                     y);

void
clutter_actor_rotate_x (ClutterActor          *self,
			  gfloat                   angle,
			  gint                     y,
			  gint                     z);

void
clutter_actor_rotate_y (ClutterActor          *self,
			  gfloat                   angle,
			  gint                     x,
			  gint                     z);

void
clutter_actor_set_opacity (ClutterActor *self,
			     guint8          opacity);

guint8
clutter_actor_get_opacity (ClutterActor *self);

void
clutter_actor_set_name (ClutterActor *self,
			  const gchar    *id);

const gchar*
clutter_actor_get_name (ClutterActor *self);

guint32
clutter_actor_get_id (ClutterActor *self);

void
clutter_actor_set_clip (ClutterActor *self,
			  gint            xoff, 
			  gint            yoff, 
			  gint            width, 
			  gint            height);

void
clutter_actor_remove_clip (ClutterActor *self);

void
clutter_actor_set_parent (ClutterActor *self, ClutterActor *parent);

ClutterActor*
clutter_actor_get_parent (ClutterActor *self);

void
clutter_actor_raise (ClutterActor *self, ClutterActor *below);

void
clutter_actor_lower (ClutterActor *self, ClutterActor *above);

void
clutter_actor_raise_top (ClutterActor *self);

void
clutter_actor_lower_bottom (ClutterActor *self);

void
clutter_actor_set_depth (ClutterActor *self,
                           gint            depth);

gint
clutter_actor_get_depth (ClutterActor *self);


G_END_DECLS

#endif
