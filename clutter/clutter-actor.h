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
#include <clutter/clutter-fixed.h>
#include <clutter/clutter-units.h>
#include <clutter/clutter-color.h>

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

#define CLUTTER_ACTOR_SET_FLAGS(e,f)    ((e)->flags |= (f))  
#define CLUTTER_ACTOR_UNSET_FLAGS(e,f)  ((e)->flags &= ~(f))  

#define CLUTTER_ACTOR_IS_MAPPED(e)      ((e)->flags & CLUTTER_ACTOR_MAPPED)  
#define CLUTTER_ACTOR_IS_REALIZED(e)    ((e)->flags & CLUTTER_ACTOR_REALIZED)
#define CLUTTER_ACTOR_IS_VISIBLE(e)     (CLUTTER_ACTOR_IS_MAPPED (e) && \
                                         CLUTTER_ACTOR_IS_REALIZED (e))

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

typedef struct _ClutterActor         ClutterActor;
typedef struct _ClutterActorClass    ClutterActorClass;
typedef struct _ClutterActorBox      ClutterActorBox;
typedef struct _ClutterActorPrivate  ClutterActorPrivate;
typedef struct _ClutterGeometry      ClutterGeometry;
typedef struct _ClutterVertex        ClutterVertex;

typedef void (*ClutterCallback) (ClutterActor *actor, gpointer data);
#define CLUTTER_CALLBACK(f)	((ClutterCallback) (f))

struct _ClutterGeometry
{
  /* FIXME: 
   * It is likely gonna save a load of pain if we make 
   * x,y unsigned...
   *
   * No, no, no, usigned is evil; we should make width and height signed.
   */
  gint   x;
  gint   y;
  guint  width;
  guint  height;
};

GType clutter_geometry_get_type (void) G_GNUC_CONST;

typedef enum
{
  CLUTTER_ACTOR_MAPPED   = 1 << 1,
  CLUTTER_ACTOR_REALIZED = 1 << 2
} ClutterActorFlags;

struct _ClutterActorBox { ClutterUnit x1, y1, x2, y2; };

GType clutter_actor_box_get_type (void) G_GNUC_CONST;

struct _ClutterVertex
{
  ClutterUnit x;
  ClutterUnit y;
  ClutterUnit z;
};

GType clutter_vertices_get_type (void) G_GNUC_CONST;

struct _ClutterActor
{
  /*< public >*/
  GObject parent_instance;
  guint32 flags;

  /*< private >*/
  guint32 private_flags;
  
  ClutterActorPrivate *priv;
};

struct _ClutterActorClass
{
  GObjectClass parent_class;

  void (* show)            (ClutterActor        *actor);
  void (* show_all)        (ClutterActor        *actor);
  void (* hide)            (ClutterActor        *actor);
  void (* hide_all)        (ClutterActor        *actor);
  void (* realize)         (ClutterActor        *actor);
  void (* unrealize)       (ClutterActor        *actor);
  void (* paint)           (ClutterActor        *actor);
  void (* request_coords)  (ClutterActor        *actor,
			    ClutterActorBox     *box);
  void (* query_coords)    (ClutterActor        *actor,
			    ClutterActorBox     *box);
  void (* set_depth)       (ClutterActor        *actor,
		            gint                 depth);
  gint (* get_depth)       (ClutterActor        *actor);
  void (* parent_set)      (ClutterActor        *actor,
                            ClutterActor        *old_parent);

  void (* destroy)         (ClutterActor        *actor);
  void (* pick)            (ClutterActor        *actor,
                            const ClutterColor  *color);

  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[32];
};

GType                 clutter_actor_get_type         (void) G_GNUC_CONST;
void                  clutter_actor_show             (ClutterActor          *self);
void                  clutter_actor_show_all         (ClutterActor          *self);
void                  clutter_actor_hide             (ClutterActor          *self);
void                  clutter_actor_hide_all         (ClutterActor          *self);
void                  clutter_actor_realize          (ClutterActor          *self);
void                  clutter_actor_unrealize        (ClutterActor          *self);
void                  clutter_actor_paint            (ClutterActor          *self);
void                  clutter_actor_pick             (ClutterActor        *actor, 
						      const ClutterColor  *color);
void                  clutter_actor_queue_redraw     (ClutterActor          *self);
void                  clutter_actor_destroy          (ClutterActor          *self);
void                  clutter_actor_request_coords   (ClutterActor          *self,
						      ClutterActorBox       *box);
void                  clutter_actor_query_coords     (ClutterActor          *self,
						      ClutterActorBox       *box);
void                  clutter_actor_set_geometry     (ClutterActor          *self,
						      const ClutterGeometry *geometry);
void                  clutter_actor_get_geometry     (ClutterActor          *self,
						      ClutterGeometry       *geometry);
void                  clutter_actor_get_coords       (ClutterActor          *self,
						      gint                  *x1,
						      gint                  *y1,
						      gint                  *x2,
						      gint                  *y2);
void                  clutter_actor_set_size         (ClutterActor          *self,
						      gint                   width,
						      gint                   height);
void                  clutter_actor_set_position     (ClutterActor          *self,
						      gint                   x,
						      gint                   y);
void                  clutter_actor_get_abs_position (ClutterActor          *self,
						      gint                  *x,
						      gint                  *y);
guint                 clutter_actor_get_width        (ClutterActor          *self);
guint                 clutter_actor_get_height       (ClutterActor          *self);

void                  clutter_actor_set_width         (ClutterActor *self, 
						       guint         width);

void                  clutter_actor_set_height        (ClutterActor *self, 
						       guint         height);


gint                  clutter_actor_get_x            (ClutterActor          *self);
gint                  clutter_actor_get_y            (ClutterActor          *self);
void                  clutter_actor_rotate_x         (ClutterActor          *self,
						      gfloat                 angle,
						      gint                   y,
						      gint                   z);
void                  clutter_actor_rotate_y         (ClutterActor          *self,
						      gfloat                 angle,
						      gint                   x,
						      gint                   z);
void                  clutter_actor_rotate_z         (ClutterActor          *self,
						      gfloat                 angle,
						      gint                   x,
						      gint                   y);
void                  clutter_actor_set_opacity      (ClutterActor          *self,
						      guint8                 opacity);
guint8                clutter_actor_get_opacity      (ClutterActor          *self);
void                  clutter_actor_set_name         (ClutterActor          *self,
						      const gchar           *name);
G_CONST_RETURN gchar *clutter_actor_get_name         (ClutterActor          *self);
guint32               clutter_actor_get_id           (ClutterActor          *self);
void                  clutter_actor_set_clip         (ClutterActor          *self,
						      gint                   xoff, 
						      gint                   yoff, 
						      gint                   width, 
						      gint                   height);
void                  clutter_actor_remove_clip      (ClutterActor          *self);
gboolean              clutter_actor_has_clip         (ClutterActor          *self);
void                  clutter_actor_set_parent       (ClutterActor          *self,
						      ClutterActor          *parent);
ClutterActor *        clutter_actor_get_parent       (ClutterActor          *self);
void                  clutter_actor_reparent         (ClutterActor          *self,
						      ClutterActor          *new_parent);
void                  clutter_actor_unparent         (ClutterActor          *self);
void                  clutter_actor_raise            (ClutterActor          *self,
						      ClutterActor          *below);
void                  clutter_actor_lower            (ClutterActor          *self,
						      ClutterActor          *above);
void                  clutter_actor_raise_top        (ClutterActor          *self);
void                  clutter_actor_lower_bottom     (ClutterActor          *self);
void                  clutter_actor_set_depth        (ClutterActor          *self,
						      gint                   depth);
gint                  clutter_actor_get_depth        (ClutterActor          *self);
void                  clutter_actor_set_scalex       (ClutterActor          *self,
                                                      ClutterFixed           scale_x,
                                                      ClutterFixed           scale_y);
void                  clutter_actor_set_scale        (ClutterActor          *self,
                                                      gdouble                scale_x,
                                                      gdouble                scale_y);
void                  clutter_actor_get_scalex       (ClutterActor          *self,
                                                      ClutterFixed          *scale_x,
                                                      ClutterFixed          *scale_y);
void                  clutter_actor_get_scale        (ClutterActor          *self,
                                                      gdouble               *scale_x,
                                                      gdouble               *scale_y);

void                  clutter_actor_set_scale_with_gravityx (ClutterActor          *self,
							     ClutterFixed      scale_x,
							     ClutterFixed      scale_y,
							     ClutterGravity    gravity);

void                  clutter_actor_set_scale_with_gravity  (ClutterActor          *self,
							     gfloat                 scale_x,
							     gfloat                 scale_y,
							     ClutterGravity         gravity);

void                  clutter_actor_get_abs_size     (ClutterActor          *self,
                                                      guint                 *width,
                                                      guint                 *height);
void                  clutter_actor_get_size         (ClutterActor          *self,
                                                      guint                 *width,
                                                      guint                 *height);
void                  clutter_actor_move_by          (ClutterActor          *self,
                                                      gint                   dx,
                                                      gint                   dy);

void                  clutter_actor_project_vertices (ClutterActor          *self,
						      ClutterVertex          verts[4]);

void                  clutter_actor_project_point    (ClutterActor          *actor, 
						      ClutterUnit           *x,
						      ClutterUnit           *y,
						      ClutterUnit           *z);
     
G_END_DECLS

#endif /* _HAVE_CLUTTER_ACTOR_H */
