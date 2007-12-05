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
#include <clutter/clutter-color.h>
#include <clutter/clutter-fixed.h>
#include <clutter/clutter-types.h>
#include <clutter/clutter-units.h>
#include <clutter/clutter-event.h>
#include <clutter/clutter-shader.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ACTOR_BOX  (clutter_actor_box_get_type ())
#define CLUTTER_TYPE_ACTOR      (clutter_actor_get_type ())

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

#define CLUTTER_ACTOR_SET_FLAGS(e,f)    (((ClutterActor*)(e))->flags |= (f))
#define CLUTTER_ACTOR_UNSET_FLAGS(e,f)  (((ClutterActor*)(e))->flags &= ~(f))

#define CLUTTER_ACTOR_IS_MAPPED(e)      (((ClutterActor*)(e))->flags & CLUTTER_ACTOR_MAPPED)
#define CLUTTER_ACTOR_IS_REALIZED(e)    (((ClutterActor*)(e))->flags & CLUTTER_ACTOR_REALIZED)
#define CLUTTER_ACTOR_IS_VISIBLE(e)     (CLUTTER_ACTOR_IS_MAPPED (e) && \
                                         CLUTTER_ACTOR_IS_REALIZED (e))
#define CLUTTER_ACTOR_IS_REACTIVE(e)    (((ClutterActor*)(e))->flags & CLUTTER_ACTOR_REACTIVE)
/*                                        && CLUTTER_ACTOR_IS_VISIBLE(e)) */


typedef struct _ClutterActorClass    ClutterActorClass;
typedef struct _ClutterActorBox      ClutterActorBox;
typedef struct _ClutterActorPrivate  ClutterActorPrivate;

/**
 * ClutterCallback:
 * @actor: a #ClutterActor
 * @data: user data
 *
 * Generic callback
 */
typedef void (*ClutterCallback) (ClutterActor *actor, gpointer data);
#define CLUTTER_CALLBACK(f)	((ClutterCallback) (f))

/**
 * ClutterActorFlags:
 * @CLUTTER_ACTOR_MAPPED: the actor has been painted
 * @CLUTTER_ACTOR_REALIZED: the resources associated to the actor have been
 *   allocated
 * @CLUTTER_ACTOR_REACTIVE: the actor 'reacts' to mouse events emmitting event
 *   signals
 *
 * Flags used to signal the state of an actor.
 */
typedef enum
{
  CLUTTER_ACTOR_MAPPED   = 1 << 1,
  CLUTTER_ACTOR_REALIZED = 1 << 2,
  CLUTTER_ACTOR_REACTIVE = 1 << 3
} ClutterActorFlags;

/**
 * ClutterActorBox:
 * @x1: X coordinate of the top left corner
 * @y1: Y coordinate of the top left corner
 * @x2: X coordinate of the bottom right corner
 * @y2: Y coordinate of the bottom right corner
 *
 * Bounding box of an actor. The coordinates of the top left and right bottom
 * corners of an actor. The coordinates of the two points are expressed in
 * #ClutterUnit<!-- -->s, that is are device-independent. If you want to obtain
 * the box dimensions in pixels, use clutter_actor_get_geometry().
 */
struct _ClutterActorBox
{
  ClutterUnit x1;
  ClutterUnit y1;
  ClutterUnit x2;
  ClutterUnit y2;
};

GType clutter_actor_box_get_type (void) G_GNUC_CONST;

/**
 * ClutterActor:
 * @flags: #ClutterActorFlags
 *
 * Base class for actors.
 */
struct _ClutterActor
{
  /*< private >*/
  GInitiallyUnowned parent_instance;

  /*< public >*/
  guint32 flags;

  /*< private >*/
  guint32 private_flags;

  ClutterActorPrivate *priv;
};

/**
 * ClutterActorClass:
 * @show: signal class handler for the ClutterActor::show signal
 * @show_all: virtual function for containers and composite actors, to
 *   determine which children should be shown when calling
 *   clutter_actor_show_all() on the actor. Defaults to calling
 *   clutter_actor_show().
 * @hide: signal class handler for the ClutterActor::hide signal
 * @hide_all: virtual function for containers and composite actors, to
 *   determine which children should be shown when calling
 *   clutter_actor_hide_all() on the actor. Defaults to calling
 *   clutter_actor_show().
 * @realize: virtual function, used to allocate resources for the actor;
 *   it should chain up to the parent's implementation
 * @unrealize: virtual function, used to deallocate resources allocated
 *   in ::realized; it should chain up to the parent's implementation
 * @paint: virtual function, used to paint the actor
 * @request_coords: virtual function, used when setting the coordinates
 *   of an actor
 * @query_coords: virtual function, used when querying the actor for
 *   its coordinates
 * @set_depth: virtual function, used when setting the depth
 * @get_depth: virtual function, used when getting the depth
 * @parent_set: signal class closure for the ClutterActor::parent-set
 *   signal
 * @destroy: signal class closure for the ClutterActor::destroy signal
 * @pick: virtual functions, used to draw an outline of the actor
 *
 * Base class for actors.
 */
struct _ClutterActorClass
{
  /*< private >*/
  GInitiallyUnownedClass parent_class;

  /*< public >*/
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

  /* event signals */
  gboolean (* event)                (ClutterActor         *actor,
				     ClutterEvent         *event);
  gboolean (* button_press_event)   (ClutterActor         *actor,
				     ClutterButtonEvent   *event);
  gboolean (* button_release_event) (ClutterActor         *actor,
				     ClutterButtonEvent   *event);
  gboolean (* scroll_event)         (ClutterActor         *actor,
				     ClutterScrollEvent   *event);
  gboolean (* key_press_event)      (ClutterActor         *actor,
				     ClutterKeyEvent      *event);
  gboolean (* key_release_event)    (ClutterActor         *actor,
				     ClutterKeyEvent      *event);
  gboolean (* motion_event)         (ClutterActor         *actor,
				     ClutterMotionEvent   *event);
  gboolean (* enter_event)          (ClutterActor         *actor,
				     ClutterCrossingEvent *event);
  gboolean (* leave_event)          (ClutterActor         *actor,
				     ClutterCrossingEvent *event);
  gboolean (* captured_event)       (ClutterActor         *actor,
				     ClutterEvent         *event);
  void     (* focus_in)             (ClutterActor         *actor);
  void     (* focus_out)            (ClutterActor         *actor);

  gboolean shadable;
  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[31];
};

GType                 clutter_actor_get_type         (void) G_GNUC_CONST;

void                  clutter_actor_show             (ClutterActor          *self);
void                  clutter_actor_show_all         (ClutterActor          *self);
void                  clutter_actor_hide             (ClutterActor          *self);
void                  clutter_actor_hide_all         (ClutterActor          *self);
void                  clutter_actor_realize          (ClutterActor          *self);
void                  clutter_actor_unrealize        (ClutterActor          *self);
void                  clutter_actor_paint            (ClutterActor          *self);
void                  clutter_actor_pick             (ClutterActor          *self,
						      const ClutterColor    *color);
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
						      gint                  *x_1,
						      gint                  *y_1,
						      gint                  *x_2,
						      gint                  *y_2);
void                  clutter_actor_set_size         (ClutterActor          *self,
						      gint                   width,
						      gint                   height);
void                  clutter_actor_set_sizeu        (ClutterActor          *self,
						      ClutterUnit            width,
						      ClutterUnit            height);
void                  clutter_actor_set_position     (ClutterActor          *self,
						      gint                   x,
						      gint                   y);
void                  clutter_actor_set_positionu    (ClutterActor          *self,
						      ClutterUnit            x,
						      ClutterUnit            y);
void                  clutter_actor_get_position     (ClutterActor          *self,
                                                      gint                  *x,
                                                      gint                  *y);
void                  clutter_actor_get_abs_position (ClutterActor          *self,
						      gint                  *x,
						      gint                  *y);
guint                 clutter_actor_get_width        (ClutterActor          *self);
ClutterUnit           clutter_actor_get_widthu       (ClutterActor          *self);
guint                 clutter_actor_get_height       (ClutterActor          *self);
ClutterUnit           clutter_actor_get_heightu      (ClutterActor          *self);
void                  clutter_actor_set_width        (ClutterActor          *self,
						      guint                  width);
void                  clutter_actor_set_widthu       (ClutterActor          *self,
						      ClutterUnit            width);
void                  clutter_actor_set_height       (ClutterActor          *self,
						      guint                  height);
void                  clutter_actor_set_heightu      (ClutterActor          *self,
						      ClutterUnit            height);
gint                  clutter_actor_get_x            (ClutterActor          *self);
ClutterUnit           clutter_actor_get_xu           (ClutterActor          *self);
gint                  clutter_actor_get_y            (ClutterActor          *self);
ClutterUnit           clutter_actor_get_yu           (ClutterActor          *self);
void                  clutter_actor_set_x            (ClutterActor          *self,
                                                      gint                   x);
void                  clutter_actor_set_xu           (ClutterActor          *self,
                                                      ClutterUnit            x);
void                  clutter_actor_set_y            (ClutterActor          *self,
                                                      gint                   y);
void                  clutter_actor_set_yu           (ClutterActor          *self,
                                                      ClutterUnit            y);
void                  clutter_actor_set_rotation     (ClutterActor          *self,
                                                      ClutterRotateAxis      axis,
                                                      gdouble                angle,
                                                      gint                   x,
                                                      gint                   y,
                                                      gint                   z);
void                  clutter_actor_set_rotationx    (ClutterActor          *self,
                                                      ClutterRotateAxis      axis,
                                                      ClutterFixed           angle,
                                                      gint                   x,
                                                      gint                   y,
                                                      gint                   z);
gdouble               clutter_actor_get_rotation     (ClutterActor          *self,
                                                      ClutterRotateAxis      axis,
                                                      gint                  *x,
                                                      gint                  *y,
                                                      gint                  *z);
ClutterFixed          clutter_actor_get_rotationx    (ClutterActor          *self,
                                                      ClutterRotateAxis      axis,
                                                      gint                  *x,
                                                      gint                  *y,
                                                      gint                  *z);

void                  clutter_actor_set_opacity      (ClutterActor          *self,
						      guint8                 opacity);
guint8                clutter_actor_get_opacity      (ClutterActor          *self);
void                  clutter_actor_set_name         (ClutterActor          *self,
						      const gchar           *name);
G_CONST_RETURN gchar *clutter_actor_get_name         (ClutterActor          *self);
guint32               clutter_actor_get_gid          (ClutterActor          *self);
void                  clutter_actor_set_clip         (ClutterActor          *self,
						      gint                   xoff,
						      gint                   yoff,
						      gint                   width,
						      gint                   height);
void                  clutter_actor_remove_clip      (ClutterActor          *self);
gboolean              clutter_actor_has_clip         (ClutterActor          *self);
void                  clutter_actor_get_clip         (ClutterActor          *self,
                                                      gint                  *xoff,
                                                      gint                  *yoff,
                                                      gint                  *width,
                                                      gint                  *height);
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
void                  clutter_actor_set_depthu       (ClutterActor          *self,
						      ClutterUnit            depth);
ClutterUnit           clutter_actor_get_depthu       (ClutterActor          *self);
void                  clutter_actor_set_reactive     (ClutterActor          *actor,
                                                      gboolean               reactive);
gboolean              clutter_actor_get_reactive     (ClutterActor          *actor);
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

void                  clutter_actor_get_abs_size             (ClutterActor          *self,
                                                              guint                 *width,
                                                              guint                 *height);
void                  clutter_actor_get_size                 (ClutterActor          *self,
                                                              guint                 *width,
                                                              guint                 *height);
void                  clutter_actor_move_by                  (ClutterActor          *self,
                                                              gint                   dx,
                                                              gint                   dy);

void                  clutter_actor_get_vertices             (ClutterActor          *self,
						              ClutterVertex          verts[4]);

void                  clutter_actor_apply_transform_to_point (ClutterActor          *self,
						              ClutterVertex         *point,
							      ClutterVertex         *vertex);

gboolean              clutter_actor_event          (ClutterActor *actor,
                                                    ClutterEvent *event,
                                                    gboolean      capture);
ClutterActor *        clutter_get_actor_by_gid     (guint32       id);

gboolean              clutter_actor_should_pick_paint (ClutterActor *self);

gboolean              clutter_actor_apply_shader (ClutterActor  *self,
                                                  ClutterShader *shader);

void                  clutter_actor_set_shader_param (ClutterActor *self,
                                                      const gchar  *param,
                                                      gfloat        value);


void                  clutter_actor_set_anchor_point  (ClutterActor          *self,
						       gint                   anchor_x,
                                                       gint                   anchor_y);
void                  clutter_actor_get_anchor_point  (ClutterActor          *self,
						       gint                  *anchor_x,
						       gint                  *anchor_y);
void                  clutter_actor_set_anchor_pointu (ClutterActor          *self,
						       ClutterUnit            anchor_x,
						       ClutterUnit            anchor_y);
void                  clutter_actor_get_anchor_pointu (ClutterActor          *self,
						       ClutterUnit           *anchor_x,
						       ClutterUnit           *anchor_y);
void                  clutter_actor_set_anchor_point_from_gravity (ClutterActor          *self,
								   ClutterGravity         gravity);

gboolean              clutter_actor_transform_stage_point (ClutterActor  *self,
							   ClutterUnit    x,
							   ClutterUnit    y,
							   ClutterUnit   *x_out,
							   ClutterUnit   *y_out);

G_END_DECLS

#endif /* _HAVE_CLUTTER_ACTOR_H */
