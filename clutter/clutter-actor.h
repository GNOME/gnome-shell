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

#ifndef __CLUTTER_ACTOR_H__
#define __CLUTTER_ACTOR_H__

/* clutter-actor.h */

#include <glib-object.h>
#include <pango/pango.h>

#include <clutter/clutter-color.h>
#include <clutter/clutter-types.h>
#include <clutter/clutter-event.h>
#include <clutter/clutter-shader.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ACTOR              (clutter_actor_get_type ())
#define CLUTTER_ACTOR(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ACTOR, ClutterActor))
#define CLUTTER_ACTOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ACTOR, ClutterActorClass))
#define CLUTTER_IS_ACTOR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ACTOR))
#define CLUTTER_IS_ACTOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ACTOR))
#define CLUTTER_ACTOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ACTOR, ClutterActorClass))

/**
 * CLUTTER_ACTOR_SET_FLAGS:
 * @a: a #ClutterActor
 * @f: the #ClutterActorFlags to set
 *
 * Sets the given flags on a #ClutterActor
 */
#define CLUTTER_ACTOR_SET_FLAGS(a,f)    (((ClutterActor*)(a))->flags |= (f))

/**
 * CLUTTER_ACTOR_UNSET_FLAGS:
 * @a: a #ClutterActor
 * @f: the #ClutterActorFlags to unset
 *
 * Unsets the given flags on a #ClutterActor
 */
#define CLUTTER_ACTOR_UNSET_FLAGS(a,f)  (((ClutterActor*)(a))->flags &= ~(f))

#define CLUTTER_ACTOR_IS_MAPPED(a)      ((((ClutterActor*)(a))->flags & CLUTTER_ACTOR_MAPPED) != FALSE)
#define CLUTTER_ACTOR_IS_REALIZED(a)    ((((ClutterActor*)(a))->flags & CLUTTER_ACTOR_REALIZED) != FALSE)
#define CLUTTER_ACTOR_IS_VISIBLE(a)     ((((ClutterActor*)(a))->flags & CLUTTER_ACTOR_VISIBLE) != FALSE)
#define CLUTTER_ACTOR_IS_REACTIVE(a)    ((((ClutterActor*)(a))->flags & CLUTTER_ACTOR_REACTIVE) != FALSE)

typedef struct _ClutterActorClass    ClutterActorClass;
typedef struct _ClutterActorPrivate  ClutterActorPrivate;

/**
 * ClutterCallback:
 * @actor: a #ClutterActor
 * @data: user data
 *
 * Generic callback
 */
typedef void (*ClutterCallback) (ClutterActor *actor,
                                 gpointer      data);

/**
 * CLUTTER_CALLBACK
 * @f: a function
 *
 * Convenience macro to cast a function to #ClutterCallback
 */
#define CLUTTER_CALLBACK(f)        ((ClutterCallback) (f))

/**
 * ClutterActorFlags:
 * @CLUTTER_ACTOR_MAPPED: the actor will be painted (is visible, and inside a toplevel, and all parents visible)
 * @CLUTTER_ACTOR_REALIZED: the resources associated to the actor have been
 *   allocated
 * @CLUTTER_ACTOR_REACTIVE: the actor 'reacts' to mouse events emmitting event
 *   signals
 * @CLUTTER_ACTOR_VISIBLE: the actor has been shown by the application program
 *
 * Flags used to signal the state of an actor.
 */
typedef enum
{
  CLUTTER_ACTOR_MAPPED   = 1 << 1,
  CLUTTER_ACTOR_REALIZED = 1 << 2,
  CLUTTER_ACTOR_REACTIVE = 1 << 3,
  CLUTTER_ACTOR_VISIBLE  = 1 << 4
} ClutterActorFlags;

/**
 * ClutterAllocationFlags:
 * @CLUTTER_ALLOCATION_NONE: No flag set
 * @CLUTTER_ABSOLUTE_ORIGIN_CHANGED: Whether the absolute origin of the
 *   actor has changed; this implies that any ancestor of the actor has
 *   been moved
 *
 * Flags passed to the #ClutterActor::allocate() virtual function and
 * to the clutter_actor_allocate() function
 *
 * Since: 1.0
 */
typedef enum
{
  CLUTTER_ALLOCATION_NONE         = 0,
  CLUTTER_ABSOLUTE_ORIGIN_CHANGED = 1 << 1
} ClutterAllocationFlags;

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
 * @show: signal class handler for #ClutterActor::show; it must chain
 *   up to the parent's implementation
 * @show_all: virtual function for containers and composite actors, to
 *   determine which children should be shown when calling
 *   clutter_actor_show_all() on the actor. Defaults to calling
 *   clutter_actor_show().
 * @hide: signal class handler for #ClutterActor::hide; it must chain
 *   up to the parent's implementation
 * @hide_all: virtual function for containers and composite actors, to
 *   determine which children should be shown when calling
 *   clutter_actor_hide_all() on the actor. Defaults to calling
 *   clutter_actor_hide().
 * @realize: virtual function, used to allocate resources for the actor;
 *   it should chain up to the parent's implementation
 * @unrealize: virtual function, used to deallocate resources allocated
 *   in ::realize; it should chain up to the parent's implementation
 * @map: virtual function for containers and composite actors, to
 *   map their children; it must chain up to the parent's implementation
 * @unmap: virtual function for containers and composite actors, to
 *   unmap their children; it must chain up to the parent's implementation
 * @paint: virtual function, used to paint the actor
 * @get_preferred_width: virtual function, used when querying the minimum
 *   and natural widths of an actor for a given height; it is used by
 *   clutter_actor_get_preferred_width()
 * @get_preferred_height: virtual function, used when querying the minimum
 *   and natural heights of an actor for a given width; it is used by
 *   clutter_actor_get_preferred_height()
 * @allocate: virtual function, used when settings the coordinates of an
 *   actor; it is used by clutter_actor_allocate()
 * @parent_set: signal class handler for the #ClutterActor::parent-set
 * @destroy: signal class handler for #ClutterActor::destroy
 * @pick: virtual function, used to draw an outline of the actor with
 *   the given color
 * @queue_redraw: class handler for #ClutterActor::queue-redraw
 * @event: class handler for #ClutterActor::event
 * @button_press_event: class handler for #ClutterActor::button-press-event
 * @button_release_event: class handler for
 *   #ClutterActor::button-release-event
 * @scroll_event: signal class closure for #ClutterActor::scroll-event
 * @key_press_event: signal class closure for #ClutterActor::key-press-event
 * @key_release_event: signal class closure for
 *   #ClutterActor::key-release-event
 * @motion_event: signal class closure for #ClutterActor::motion-event
 * @enter_event: signal class closure for #ClutterActor::enter-event
 * @leave_event: signal class closure for #ClutterActor::leave-event
 * @captured_event: signal class closure for #ClutterActor::captured-event
 * @key_focus_in: signal class closure for #ClutterActor::focus-in
 * @key_focus_out: signal class closure for #ClutterActor::focus-out
 *
 * Base class for actors.
 */
struct _ClutterActorClass
{
  /*< private >*/
  GInitiallyUnownedClass parent_class;

  /*< public >*/
  void (* show)                 (ClutterActor          *actor);
  void (* show_all)             (ClutterActor          *actor);
  void (* hide)                 (ClutterActor          *actor);
  void (* hide_all)             (ClutterActor          *actor);
  void (* realize)              (ClutterActor          *actor);
  void (* unrealize)            (ClutterActor          *actor);
  void (* map)                  (ClutterActor          *actor);
  void (* unmap)                (ClutterActor          *actor);
  void (* paint)                (ClutterActor          *actor);
  void (* parent_set)           (ClutterActor          *actor,
                                 ClutterActor          *old_parent);

  void (* destroy)              (ClutterActor          *actor);
  void (* pick)                 (ClutterActor          *actor,
                                 const ClutterColor    *color);

  void (* queue_redraw)         (ClutterActor          *actor,
                                 ClutterActor          *leaf_that_queued);

  /* size negotiation */
  void (* get_preferred_width)  (ClutterActor           *actor,
                                 gfloat                  for_height,
                                 gfloat                 *min_width_p,
                                 gfloat                 *natural_width_p);
  void (* get_preferred_height) (ClutterActor           *actor,
                                 gfloat                  for_width,
                                 gfloat                 *min_height_p,
                                 gfloat                 *natural_height_p);
  void (* allocate)             (ClutterActor           *actor,
                                 const ClutterActorBox  *box,
                                 ClutterAllocationFlags  flags);
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
  void     (* key_focus_in)         (ClutterActor         *actor);
  void     (* key_focus_out)        (ClutterActor         *actor);

  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[32];
};

GType                 clutter_actor_get_type                  (void) G_GNUC_CONST;

void                  clutter_actor_set_flags                 (ClutterActor          *self,
                                                               ClutterActorFlags      flags);
void                  clutter_actor_unset_flags               (ClutterActor          *self,
                                                               ClutterActorFlags      flags);
ClutterActorFlags     clutter_actor_get_flags                 (ClutterActor          *self);

void                  clutter_actor_show                      (ClutterActor          *self);
void                  clutter_actor_show_all                  (ClutterActor          *self);
void                  clutter_actor_hide                      (ClutterActor          *self);
void                  clutter_actor_hide_all                  (ClutterActor          *self);
void                  clutter_actor_realize                   (ClutterActor          *self);
void                  clutter_actor_unrealize                 (ClutterActor          *self);
void                  clutter_actor_map                       (ClutterActor          *self);
void                  clutter_actor_unmap                     (ClutterActor          *self);
void                  clutter_actor_paint                     (ClutterActor          *self);
void                  clutter_actor_queue_redraw              (ClutterActor          *self);
void                  clutter_actor_queue_relayout            (ClutterActor          *self);
void                  clutter_actor_destroy                   (ClutterActor          *self);

/* size negotiation */
void                  clutter_actor_get_preferred_width       (ClutterActor          *self,
                                                               gfloat                 for_height,
                                                               gfloat                *min_width_p,
                                                               gfloat                *natural_width_p);
void                  clutter_actor_get_preferred_height      (ClutterActor          *self,
                                                               gfloat                 for_width,
                                                               gfloat                *min_height_p,
                                                               gfloat                *natural_height_p);
void                  clutter_actor_get_preferred_size        (ClutterActor          *self,
                                                               gfloat                *min_width_p,
                                                               gfloat                *min_height_p,
                                                               gfloat                *natural_width_p,
                                                               gfloat                *natural_height_p);
void                  clutter_actor_allocate                  (ClutterActor          *self,
                                                               const ClutterActorBox *box,
                                                               ClutterAllocationFlags flags);
void                  clutter_actor_allocate_preferred_size   (ClutterActor          *self,
                                                               ClutterAllocationFlags flags);
void                  clutter_actor_allocate_available_size   (ClutterActor          *self,
                                                               gfloat                 x,
                                                               gfloat                 y,
                                                               gfloat                 available_width,
                                                               gfloat                 available_height,
                                                               ClutterAllocationFlags flags);
void                  clutter_actor_get_allocation_coords     (ClutterActor          *self,
                                                               gint                  *x_1,
                                                               gint                  *y_1,
                                                               gint                  *x_2,
                                                               gint                  *y_2);
void                  clutter_actor_get_allocation_box        (ClutterActor          *self,
                                                               ClutterActorBox       *box);
void                  clutter_actor_get_allocation_geometry   (ClutterActor          *self,
                                                               ClutterGeometry       *geom);
void                  clutter_actor_get_allocation_vertices   (ClutterActor          *self,
							       ClutterActor          *ancestor,
                                                               ClutterVertex          verts[4]);

void                  clutter_actor_set_geometry              (ClutterActor          *self,
                                                               const ClutterGeometry *geometry);
void                  clutter_actor_get_geometry              (ClutterActor          *self,
                                                               ClutterGeometry       *geometry);
void                  clutter_actor_set_size                  (ClutterActor          *self,
                                                               gfloat                 width,
                                                               gfloat                 height);
void                  clutter_actor_get_size                  (ClutterActor          *self,
                                                               gfloat                *width,
                                                               gfloat                *height);
void                  clutter_actor_get_transformed_size      (ClutterActor          *self,
                                                               gfloat                *width,
                                                               gfloat                *height);
void                  clutter_actor_set_position              (ClutterActor          *self,
                                                               gfloat                 x,
                                                               gfloat                 y);
void                  clutter_actor_get_position              (ClutterActor          *self,
                                                               gfloat                *x,
                                                               gfloat                *y);
void                  clutter_actor_get_transformed_position  (ClutterActor          *self,
                                                               gfloat                *x,
                                                               gfloat                *y);

gboolean              clutter_actor_get_fixed_position_set    (ClutterActor          *self);
void                  clutter_actor_set_fixed_position_set    (ClutterActor          *self,
                                                               gboolean               is_set);

gfloat                clutter_actor_get_width                 (ClutterActor          *self);
gfloat                clutter_actor_get_height                (ClutterActor          *self);
void                  clutter_actor_set_width                 (ClutterActor          *self,
                                                               gfloat                 width);
void                  clutter_actor_set_height                (ClutterActor          *self,
                                                               gfloat                 height);
gfloat                clutter_actor_get_x                     (ClutterActor          *self);
gfloat                clutter_actor_get_y                     (ClutterActor          *self);
void                  clutter_actor_set_x                     (ClutterActor          *self,
                                                               gfloat                 x);
void                  clutter_actor_set_y                     (ClutterActor          *self,
                                                               gfloat                 y);
void                  clutter_actor_set_rotation              (ClutterActor          *self,
                                                               ClutterRotateAxis      axis,
                                                               gdouble                angle,
                                                               gfloat                 x,
                                                               gfloat                 y,
                                                               gfloat                 z);
void                  clutter_actor_set_z_rotation_from_gravity (ClutterActor        *self,
                                                               gdouble                angle,
                                                               ClutterGravity         gravity);
gdouble               clutter_actor_get_rotation              (ClutterActor          *self,
                                                               ClutterRotateAxis      axis,
                                                               gfloat                *x,
                                                               gfloat                *y,
                                                               gfloat                *z);
ClutterGravity        clutter_actor_get_z_rotation_gravity    (ClutterActor          *self);

void                  clutter_actor_set_opacity               (ClutterActor          *self,
                                                               guint8                 opacity);
guint8                clutter_actor_get_opacity               (ClutterActor          *self);

guint8                clutter_actor_get_paint_opacity         (ClutterActor          *self);
gboolean              clutter_actor_get_paint_visibility      (ClutterActor          *self);


void                  clutter_actor_set_name                  (ClutterActor          *self,
                                                               const gchar           *name);
G_CONST_RETURN gchar *clutter_actor_get_name                  (ClutterActor          *self);

guint32               clutter_actor_get_gid                   (ClutterActor          *self);
void                  clutter_actor_set_clip                  (ClutterActor          *self,
                                                               gfloat                 xoff,
                                                               gfloat                 yoff,
                                                               gfloat                 width,
                                                               gfloat                 height);
void                  clutter_actor_remove_clip               (ClutterActor          *self);
gboolean              clutter_actor_has_clip                  (ClutterActor          *self);
void                  clutter_actor_get_clip                  (ClutterActor          *self,
                                                               gfloat                *xoff,
                                                               gfloat                *yoff,
                                                               gfloat                *width,
                                                               gfloat                *height);

void                  clutter_actor_set_parent                (ClutterActor          *self,
                                                               ClutterActor          *parent);
ClutterActor *        clutter_actor_get_parent                (ClutterActor          *self);
void                  clutter_actor_reparent                  (ClutterActor          *self,
                                                               ClutterActor          *new_parent);
void                  clutter_actor_unparent                  (ClutterActor          *self);
ClutterActor*         clutter_actor_get_stage                 (ClutterActor          *actor);

void                  clutter_actor_raise                     (ClutterActor          *self,
                                                               ClutterActor          *below);
void                  clutter_actor_lower                     (ClutterActor          *self,
                                                               ClutterActor          *above);
void                  clutter_actor_raise_top                 (ClutterActor          *self);
void                  clutter_actor_lower_bottom              (ClutterActor          *self);
void                  clutter_actor_set_depth                 (ClutterActor          *self,
                                                               gfloat                 depth);
gfloat                clutter_actor_get_depth                 (ClutterActor          *self);

void                  clutter_actor_set_scale                 (ClutterActor          *self,
                                                               gdouble                scale_x,
                                                               gdouble                scale_y);
void                  clutter_actor_set_scale_full            (ClutterActor          *self,
                                                               gdouble                scale_x,
                                                               gdouble                scale_y,
                                                               gfloat                 center_x,
                                                               gfloat                 center_y);
void                  clutter_actor_set_scale_with_gravity    (ClutterActor          *self,
                                                               gdouble                scale_x,
                                                               gdouble                scale_y,
                                                               ClutterGravity         gravity);
void                  clutter_actor_get_scale                 (ClutterActor          *self,
                                                               gdouble               *scale_x,
                                                               gdouble               *scale_y);
void                  clutter_actor_get_scale_center          (ClutterActor          *self,
                                                               gfloat                *center_x,
                                                               gfloat                *center_y);
ClutterGravity        clutter_actor_get_scale_gravity         (ClutterActor          *self);

void                  clutter_actor_move_by                   (ClutterActor          *self,
                                                               gfloat                 dx,
                                                               gfloat                 dy);

void                  clutter_actor_set_reactive              (ClutterActor          *actor,
                                                               gboolean               reactive);
gboolean              clutter_actor_get_reactive              (ClutterActor          *actor);

gboolean              clutter_actor_event                     (ClutterActor          *actor,
                                                               ClutterEvent          *event,
                                                               gboolean               capture);

ClutterActor *        clutter_get_actor_by_gid                (guint32                id);

gboolean              clutter_actor_set_shader                (ClutterActor          *self,
                                                               ClutterShader         *shader);
ClutterShader *       clutter_actor_get_shader                (ClutterActor          *self);
void                  clutter_actor_set_shader_param          (ClutterActor          *self,
                                                               const gchar           *param,
                                                               const GValue          *value);
void                  clutter_actor_set_shader_param_int      (ClutterActor          *self,
                                                               const gchar           *param,
                                                               gint                   value);
void                  clutter_actor_set_shader_param_float    (ClutterActor          *self,
                                                               const gchar           *param,
                                                               gfloat                 value);

void     clutter_actor_set_anchor_point               (ClutterActor   *self,
                                                       gfloat          anchor_x,
                                                       gfloat          anchor_y);
void     clutter_actor_move_anchor_point              (ClutterActor   *self,
                                                       gfloat          anchor_x,
                                                       gfloat          anchor_y);
void     clutter_actor_get_anchor_point               (ClutterActor   *self,
                                                       gfloat         *anchor_x,
                                                       gfloat         *anchor_y);
ClutterGravity clutter_actor_get_anchor_point_gravity (ClutterActor   *self);
void     clutter_actor_set_anchor_point_from_gravity  (ClutterActor   *self,
                                                       ClutterGravity  gravity);
void     clutter_actor_move_anchor_point_from_gravity (ClutterActor   *self,
                                                       ClutterGravity  gravity);

gboolean clutter_actor_transform_stage_point          (ClutterActor   *self,
                                                       gfloat          x,
                                                       gfloat          y,
                                                       gfloat         *x_out,
                                                       gfloat         *y_out);
gboolean clutter_actor_is_rotated                     (ClutterActor   *self);
gboolean clutter_actor_is_scaled                      (ClutterActor   *self);
gboolean clutter_actor_should_pick_paint              (ClutterActor   *self);

void clutter_actor_get_abs_allocation_vertices        (ClutterActor        *self,
                                                       ClutterVertex        verts[4]);

void clutter_actor_apply_transform_to_point           (ClutterActor        *self,
                                                       const ClutterVertex *point,
                                                       ClutterVertex       *vertex);
void clutter_actor_apply_relative_transform_to_point  (ClutterActor        *self,
                                                       ClutterActor        *ancestor,
                                                       const ClutterVertex *point,
                                                       ClutterVertex       *vertex);

void          clutter_actor_grab_key_focus            (ClutterActor        *self);

PangoContext *clutter_actor_get_pango_context         (ClutterActor        *self);
PangoContext *clutter_actor_create_pango_context      (ClutterActor        *self);
PangoLayout * clutter_actor_create_pango_layout       (ClutterActor        *self,
                                                       const gchar         *text);

G_END_DECLS

#endif /* __CLUTTER_ACTOR_H__ */
