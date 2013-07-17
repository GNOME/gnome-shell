/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd
 * Copyright (C) 2009, 2010 Intel Corp
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

#include <pango/pango.h>
#include <atk/atk.h>

#include <cogl/cogl.h>

#include <clutter/clutter-types.h>
#include <clutter/clutter-event.h>

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
 * @data: (closure): user data
 *
 * Generic callback
 */
typedef void (*ClutterCallback) (ClutterActor *actor,
                                 gpointer      data);

/**
 * CLUTTER_CALLBACK:
 * @f: a function
 *
 * Convenience macro to cast a function to #ClutterCallback
 */
#define CLUTTER_CALLBACK(f)        ((ClutterCallback) (f))

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
 *   clutter_actor_show(). This virtual function is deprecated and it
 *   should not be overridden.
 * @hide: signal class handler for #ClutterActor::hide; it must chain
 *   up to the parent's implementation
 * @hide_all: virtual function for containers and composite actors, to
 *   determine which children should be shown when calling
 *   clutter_actor_hide_all() on the actor. Defaults to calling
 *   clutter_actor_hide(). This virtual function is deprecated and it
 *   should not be overridden.
 * @realize: virtual function, used to allocate resources for the actor;
 *   it should chain up to the parent's implementation. This virtual
 *   function is deprecated and should not be overridden in newly
 *   written code.
 * @unrealize: virtual function, used to deallocate resources allocated
 *   in ::realize; it should chain up to the parent's implementation. This
 *   function is deprecated and should not be overridden in newly
 *   written code.
 * @map: virtual function for containers and composite actors, to
 *   map their children; it must chain up to the parent's implementation.
 *   Overriding this function is optional.
 * @unmap: virtual function for containers and composite actors, to
 *   unmap their children; it must chain up to the parent's implementation.
 *   Overriding this function is optional.
 * @paint: virtual function, used to paint the actor
 * @get_preferred_width: virtual function, used when querying the minimum
 *   and natural widths of an actor for a given height; it is used by
 *   clutter_actor_get_preferred_width()
 * @get_preferred_height: virtual function, used when querying the minimum
 *   and natural heights of an actor for a given width; it is used by
 *   clutter_actor_get_preferred_height()
 * @allocate: virtual function, used when settings the coordinates of an
 *   actor; it is used by clutter_actor_allocate(); it must chain up to
 *   the parent's implementation, or call clutter_actor_set_allocation()
 * @apply_transform: virtual function, used when applying the transformations
 *   to an actor before painting it or when transforming coordinates or
 *   the allocation; it must chain up to the parent's implementation
 * @parent_set: signal class handler for the #ClutterActor::parent-set
 * @destroy: signal class handler for #ClutterActor::destroy. It must
 *   chain up to the parent's implementation
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
 * @key_focus_in: signal class closure for #ClutterActor::key-focus-in
 * @key_focus_out: signal class closure for #ClutterActor::key-focus-out
 * @queue_relayout: class handler for #ClutterActor::queue-relayout
 * @get_accessible: virtual function, returns the accessible object that
 *   describes the actor to an assistive technology.
 * @get_paint_volume: virtual function, for sub-classes to define their
 *   #ClutterPaintVolume
 * @has_overlaps: virtual function for
 *   sub-classes to advertise whether they need an offscreen redirect
 *   to get the correct opacity. See
 *   clutter_actor_set_offscreen_redirect() for details.
 * @paint_node: virtual function for creating paint nodes and attaching
 *   them to the render tree
 * @touch_event: signal class closure for #ClutterActor::touch-event
 *
 * Base class for actors.
 */
struct _ClutterActorClass
{
  /*< private >*/
  GInitiallyUnownedClass parent_class;

  /*< public >*/
  void (* show)                 (ClutterActor          *self);
  void (* show_all)             (ClutterActor          *self);
  void (* hide)                 (ClutterActor          *self);
  void (* hide_all)             (ClutterActor          *self);
  void (* realize)              (ClutterActor          *self);
  void (* unrealize)            (ClutterActor          *self);
  void (* map)                  (ClutterActor          *self);
  void (* unmap)                (ClutterActor          *self);
  void (* paint)                (ClutterActor          *self);
  void (* parent_set)           (ClutterActor          *actor,
                                 ClutterActor          *old_parent);

  void (* destroy)              (ClutterActor          *self);
  void (* pick)                 (ClutterActor          *actor,
                                 const ClutterColor    *color);

  void (* queue_redraw)         (ClutterActor          *actor,
                                 ClutterActor          *leaf_that_queued);

  /* size negotiation */
  void (* get_preferred_width)  (ClutterActor           *self,
                                 gfloat                  for_height,
                                 gfloat                 *min_width_p,
                                 gfloat                 *natural_width_p);
  void (* get_preferred_height) (ClutterActor           *self,
                                 gfloat                  for_width,
                                 gfloat                 *min_height_p,
                                 gfloat                 *natural_height_p);
  void (* allocate)             (ClutterActor           *self,
                                 const ClutterActorBox  *box,
                                 ClutterAllocationFlags  flags);

  /* transformations */
  void (* apply_transform)      (ClutterActor           *actor,
                                 ClutterMatrix          *matrix);

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

  void     (* queue_relayout)       (ClutterActor         *self);

  /* accessibility support */
  AtkObject * (* get_accessible)    (ClutterActor         *self);

  gboolean (* get_paint_volume)     (ClutterActor         *actor,
                                     ClutterPaintVolume   *volume);

  gboolean (* has_overlaps)         (ClutterActor         *self);

  void     (* paint_node)           (ClutterActor         *self,
                                     ClutterPaintNode     *root);

  gboolean (* touch_event)          (ClutterActor         *self,
                                     ClutterTouchEvent    *event);

  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[26];
};

/**
 * ClutterActorIter:
 *
 * An iterator structure that allows to efficiently iterate over a
 * section of the scene graph.
 *
 * The contents of the <structname>ClutterActorIter</structname> structure
 * are private and should only be accessed using the provided API.
 *
 * Since: 1.10
 */
struct _ClutterActorIter
{
  /*< private >*/
  gpointer CLUTTER_PRIVATE_FIELD (dummy1);
  gpointer CLUTTER_PRIVATE_FIELD (dummy2);
  gpointer CLUTTER_PRIVATE_FIELD (dummy3);
  gint     CLUTTER_PRIVATE_FIELD (dummy4);
  gpointer CLUTTER_PRIVATE_FIELD (dummy5);
};

GType clutter_actor_get_type (void) G_GNUC_CONST;

ClutterActor *                  clutter_actor_new                               (void);

void                            clutter_actor_set_flags                         (ClutterActor                *self,
                                                                                 ClutterActorFlags            flags);
void                            clutter_actor_unset_flags                       (ClutterActor                *self,
                                                                                 ClutterActorFlags            flags);
ClutterActorFlags               clutter_actor_get_flags                         (ClutterActor                *self);
void                            clutter_actor_show                              (ClutterActor                *self);
void                            clutter_actor_hide                              (ClutterActor                *self);
void                            clutter_actor_realize                           (ClutterActor                *self);
void                            clutter_actor_unrealize                         (ClutterActor                *self);
void                            clutter_actor_map                               (ClutterActor                *self);
void                            clutter_actor_unmap                             (ClutterActor                *self);
void                            clutter_actor_paint                             (ClutterActor                *self);
void                            clutter_actor_continue_paint                    (ClutterActor                *self);
void                            clutter_actor_queue_redraw                      (ClutterActor                *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_queue_redraw_with_clip            (ClutterActor                *self,
                                                                                 const cairo_rectangle_int_t *clip);
void                            clutter_actor_queue_relayout                    (ClutterActor                *self);
void                            clutter_actor_destroy                           (ClutterActor                *self);
void                            clutter_actor_set_name                          (ClutterActor                *self,
                                                                                 const gchar                 *name);
const gchar *                   clutter_actor_get_name                          (ClutterActor                *self);
AtkObject *                     clutter_actor_get_accessible                    (ClutterActor                *self);

/* Size negotiation */
void                            clutter_actor_set_request_mode                  (ClutterActor                *self,
                                                                                 ClutterRequestMode           mode);
ClutterRequestMode              clutter_actor_get_request_mode                  (ClutterActor                *self);
void                            clutter_actor_get_preferred_width               (ClutterActor                *self,
                                                                                 gfloat                       for_height,
                                                                                 gfloat                      *min_width_p,
                                                                                 gfloat                      *natural_width_p);
void                            clutter_actor_get_preferred_height              (ClutterActor                *self,
                                                                                 gfloat                       for_width,
                                                                                 gfloat                      *min_height_p,
                                                                                 gfloat                      *natural_height_p);
void                            clutter_actor_get_preferred_size                (ClutterActor                *self,
                                                                                 gfloat                      *min_width_p,
                                                                                 gfloat                      *min_height_p,
                                                                                 gfloat                      *natural_width_p,
                                                                                 gfloat                      *natural_height_p);
void                            clutter_actor_allocate                          (ClutterActor                *self,
                                                                                 const ClutterActorBox       *box,
                                                                                 ClutterAllocationFlags       flags);
void                            clutter_actor_allocate_preferred_size           (ClutterActor                *self,
                                                                                 ClutterAllocationFlags       flags);
void                            clutter_actor_allocate_available_size           (ClutterActor                *self,
                                                                                 gfloat                       x,
                                                                                 gfloat                       y,
                                                                                 gfloat                       available_width,
                                                                                 gfloat                       available_height,
                                                                                 ClutterAllocationFlags       flags);
void                            clutter_actor_allocate_align_fill               (ClutterActor                *self,
                                                                                 const ClutterActorBox       *box,
                                                                                 gdouble                      x_align,
                                                                                 gdouble                      y_align,
                                                                                 gboolean                     x_fill,
                                                                                 gboolean                     y_fill,
                                                                                 ClutterAllocationFlags       flags);
void                            clutter_actor_set_allocation                    (ClutterActor                *self,
                                                                                 const ClutterActorBox       *box,
                                                                                 ClutterAllocationFlags       flags);
void                            clutter_actor_get_allocation_box                (ClutterActor                *self,
                                                                                 ClutterActorBox             *box);
void                            clutter_actor_get_allocation_vertices           (ClutterActor                *self,
                                                                                 ClutterActor                *ancestor,
                                                                                 ClutterVertex                verts[]);
gboolean                        clutter_actor_has_allocation                    (ClutterActor                *self);
void                            clutter_actor_set_size                          (ClutterActor                *self,
                                                                                 gfloat                       width,
                                                                                 gfloat                       height);
void                            clutter_actor_get_size                          (ClutterActor                *self,
                                                                                 gfloat                      *width,
                                                                                 gfloat                      *height);
void                            clutter_actor_set_position                      (ClutterActor                *self,
                                                                                 gfloat                       x,
                                                                                 gfloat                       y);
void                            clutter_actor_get_position                      (ClutterActor                *self,
                                                                                 gfloat                      *x,
                                                                                 gfloat                      *y);
gboolean                        clutter_actor_get_fixed_position_set            (ClutterActor                *self);
void                            clutter_actor_set_fixed_position_set            (ClutterActor                *self,
                                                                                 gboolean                     is_set);
void                            clutter_actor_move_by                           (ClutterActor                *self,
                                                                                 gfloat                       dx,
                                                                                 gfloat                       dy);

/* Actor geometry */
gfloat                          clutter_actor_get_width                         (ClutterActor                *self);
gfloat                          clutter_actor_get_height                        (ClutterActor                *self);
void                            clutter_actor_set_width                         (ClutterActor                *self,
                                                                                 gfloat                       width);
void                            clutter_actor_set_height                        (ClutterActor                *self,
                                                                                 gfloat                       height);
gfloat                          clutter_actor_get_x                             (ClutterActor                *self);
gfloat                          clutter_actor_get_y                             (ClutterActor                *self);
void                            clutter_actor_set_x                             (ClutterActor                *self,
                                                                                 gfloat                       x);
void                            clutter_actor_set_y                             (ClutterActor                *self,
                                                                                 gfloat                       y);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_set_z_position                    (ClutterActor                *self,
                                                                                 gfloat                       z_position);
CLUTTER_AVAILABLE_IN_1_12
gfloat                          clutter_actor_get_z_position                    (ClutterActor                *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_layout_manager                (ClutterActor                *self,
                                                                                 ClutterLayoutManager        *manager);
CLUTTER_AVAILABLE_IN_1_10
ClutterLayoutManager *          clutter_actor_get_layout_manager                (ClutterActor                *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_x_align                       (ClutterActor                *self,
                                                                                 ClutterActorAlign            x_align);
CLUTTER_AVAILABLE_IN_1_10
ClutterActorAlign               clutter_actor_get_x_align                       (ClutterActor                *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_y_align                       (ClutterActor                *self,
                                                                                 ClutterActorAlign            y_align);
CLUTTER_AVAILABLE_IN_1_10
ClutterActorAlign               clutter_actor_get_y_align                       (ClutterActor                *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_margin_top                    (ClutterActor                *self,
                                                                                 gfloat                       margin);
CLUTTER_AVAILABLE_IN_1_10
gfloat                          clutter_actor_get_margin_top                    (ClutterActor                *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_margin_bottom                 (ClutterActor                *self,
                                                                                 gfloat                       margin);
CLUTTER_AVAILABLE_IN_1_10
gfloat                          clutter_actor_get_margin_bottom                 (ClutterActor                *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_margin_left                   (ClutterActor                *self,
                                                                                 gfloat                       margin);
CLUTTER_AVAILABLE_IN_1_10
gfloat                          clutter_actor_get_margin_left                   (ClutterActor                *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_margin_right                  (ClutterActor                *self,
                                                                                 gfloat                       margin);
CLUTTER_AVAILABLE_IN_1_10
gfloat                          clutter_actor_get_margin_right                  (ClutterActor                *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_margin                        (ClutterActor                *self,
                                                                                 const ClutterMargin         *margin);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_get_margin                        (ClutterActor                *self,
                                                                                 ClutterMargin               *margin);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_set_x_expand                      (ClutterActor                *self,
                                                                                 gboolean                     expand);
CLUTTER_AVAILABLE_IN_1_12
gboolean                        clutter_actor_get_x_expand                      (ClutterActor                *self);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_set_y_expand                      (ClutterActor                *self,
                                                                                 gboolean                     expand);
CLUTTER_AVAILABLE_IN_1_12
gboolean                        clutter_actor_get_y_expand                      (ClutterActor                *self);
CLUTTER_AVAILABLE_IN_1_12
gboolean                        clutter_actor_needs_expand                      (ClutterActor                *self,
                                                                                 ClutterOrientation           orientation);

/* Paint */
void                            clutter_actor_set_clip                          (ClutterActor                *self,
                                                                                 gfloat                       xoff,
                                                                                 gfloat                       yoff,
                                                                                 gfloat                       width,
                                                                                 gfloat                       height);
void                            clutter_actor_remove_clip                       (ClutterActor               *self);
gboolean                        clutter_actor_has_clip                          (ClutterActor               *self);
void                            clutter_actor_get_clip                          (ClutterActor               *self,
                                                                                 gfloat                     *xoff,
                                                                                 gfloat                     *yoff,
                                                                                 gfloat                     *width,
                                                                                 gfloat                     *height);
void                            clutter_actor_set_clip_to_allocation            (ClutterActor               *self,
                                                                                 gboolean                    clip_set);
gboolean                        clutter_actor_get_clip_to_allocation            (ClutterActor               *self);
void                            clutter_actor_set_opacity                       (ClutterActor               *self,
                                                                                 guint8                      opacity);
guint8                          clutter_actor_get_opacity                       (ClutterActor               *self);
guint8                          clutter_actor_get_paint_opacity                 (ClutterActor               *self);
gboolean                        clutter_actor_get_paint_visibility              (ClutterActor               *self);
void                            clutter_actor_set_offscreen_redirect            (ClutterActor               *self,
                                                                                 ClutterOffscreenRedirect    redirect);
ClutterOffscreenRedirect        clutter_actor_get_offscreen_redirect            (ClutterActor               *self);
gboolean                        clutter_actor_should_pick_paint                 (ClutterActor               *self);
gboolean                        clutter_actor_is_in_clone_paint                 (ClutterActor               *self);
gboolean                        clutter_actor_get_paint_box                     (ClutterActor               *self,
                                                                                 ClutterActorBox            *box);
gboolean                        clutter_actor_has_overlaps                      (ClutterActor               *self);

/* Content */
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_content                       (ClutterActor               *self,
                                                                                 ClutterContent             *content);
CLUTTER_AVAILABLE_IN_1_10
ClutterContent *                clutter_actor_get_content                       (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_content_gravity               (ClutterActor               *self,
                                                                                 ClutterContentGravity       gravity);
CLUTTER_AVAILABLE_IN_1_10
ClutterContentGravity           clutter_actor_get_content_gravity               (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_content_scaling_filters       (ClutterActor               *self,
                                                                                 ClutterScalingFilter        min_filter,
                                                                                 ClutterScalingFilter        mag_filter);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_get_content_scaling_filters       (ClutterActor               *self,
                                                                                 ClutterScalingFilter       *min_filter,
                                                                                 ClutterScalingFilter       *mag_filter);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_set_content_repeat                (ClutterActor               *self,
                                                                                 ClutterContentRepeat        repeat);
CLUTTER_AVAILABLE_IN_1_12
ClutterContentRepeat            clutter_actor_get_content_repeat                (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_get_content_box                   (ClutterActor               *self,
                                                                                 ClutterActorBox            *box);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_background_color              (ClutterActor               *self,
                                                                                 const ClutterColor         *color);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_get_background_color              (ClutterActor               *self,
                                                                                 ClutterColor               *color);
const ClutterPaintVolume *      clutter_actor_get_paint_volume                  (ClutterActor               *self);
const ClutterPaintVolume *      clutter_actor_get_transformed_paint_volume      (ClutterActor               *self,
                                                                                 ClutterActor               *relative_to_ancestor);
CLUTTER_AVAILABLE_IN_1_10
const ClutterPaintVolume *      clutter_actor_get_default_paint_volume          (ClutterActor               *self);

/* Events */
void                            clutter_actor_set_reactive                      (ClutterActor               *actor,
                                                                                 gboolean                    reactive);
gboolean                        clutter_actor_get_reactive                      (ClutterActor               *actor);
gboolean                        clutter_actor_has_key_focus                     (ClutterActor               *self);
void                            clutter_actor_grab_key_focus                    (ClutterActor               *self);
gboolean                        clutter_actor_event                             (ClutterActor               *actor,
                                                                                 const ClutterEvent         *event,
                                                                                 gboolean                    capture);
gboolean                        clutter_actor_has_pointer                       (ClutterActor               *self);

/* Text */
PangoContext *                  clutter_actor_get_pango_context                 (ClutterActor               *self);
PangoContext *                  clutter_actor_create_pango_context              (ClutterActor               *self);
PangoLayout *                   clutter_actor_create_pango_layout               (ClutterActor               *self,
                                                                                 const gchar                *text);
void                            clutter_actor_set_text_direction                (ClutterActor               *self,
                                                                                 ClutterTextDirection        text_dir);
ClutterTextDirection            clutter_actor_get_text_direction                (ClutterActor               *self);

/* Actor hierarchy */
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_add_child                         (ClutterActor               *self,
                                                                                 ClutterActor               *child);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_insert_child_at_index             (ClutterActor               *self,
                                                                                 ClutterActor               *child,
                                                                                 gint                        index_);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_insert_child_above                (ClutterActor               *self,
                                                                                 ClutterActor               *child,
                                                                                 ClutterActor               *sibling);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_insert_child_below                (ClutterActor               *self,
                                                                                 ClutterActor               *child,
                                                                                 ClutterActor               *sibling);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_replace_child                     (ClutterActor               *self,
                                                                                 ClutterActor               *old_child,
                                                                                 ClutterActor               *new_child);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_remove_child                      (ClutterActor               *self,
                                                                                 ClutterActor               *child);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_remove_all_children               (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_destroy_all_children              (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
GList *                         clutter_actor_get_children                      (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
gint                            clutter_actor_get_n_children                    (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
ClutterActor *                  clutter_actor_get_child_at_index                (ClutterActor               *self,
                                                                                 gint                        index_);
CLUTTER_AVAILABLE_IN_1_10
ClutterActor *                  clutter_actor_get_previous_sibling              (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
ClutterActor *                  clutter_actor_get_next_sibling                  (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
ClutterActor *                  clutter_actor_get_first_child                   (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
ClutterActor *                  clutter_actor_get_last_child                    (ClutterActor               *self);
ClutterActor *                  clutter_actor_get_parent                        (ClutterActor               *self);
gboolean                        clutter_actor_contains                          (ClutterActor               *self,
                                                                                 ClutterActor               *descendant);
ClutterActor*                   clutter_actor_get_stage                         (ClutterActor               *actor);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_child_below_sibling           (ClutterActor               *self,
                                                                                 ClutterActor               *child,
                                                                                 ClutterActor               *sibling);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_child_above_sibling           (ClutterActor               *self,
                                                                                 ClutterActor               *child,
                                                                                 ClutterActor               *sibling);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_child_at_index                (ClutterActor               *self,
                                                                                 ClutterActor               *child,
                                                                                 gint                        index_);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_iter_init                         (ClutterActorIter           *iter,
                                                                                 ClutterActor               *root);
CLUTTER_AVAILABLE_IN_1_10
gboolean                        clutter_actor_iter_next                         (ClutterActorIter           *iter,
                                                                                 ClutterActor              **child);
CLUTTER_AVAILABLE_IN_1_10
gboolean                        clutter_actor_iter_prev                         (ClutterActorIter           *iter,
                                                                                 ClutterActor              **child);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_iter_remove                       (ClutterActorIter           *iter);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_iter_destroy                      (ClutterActorIter           *iter);
CLUTTER_AVAILABLE_IN_1_12
gboolean                        clutter_actor_iter_is_valid                     (const ClutterActorIter     *iter);

/* Transformations */
gboolean                        clutter_actor_is_rotated                        (ClutterActor               *self);
gboolean                        clutter_actor_is_scaled                         (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_set_pivot_point                   (ClutterActor               *self,
                                                                                 gfloat                      pivot_x,
                                                                                 gfloat                      pivot_y);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_get_pivot_point                   (ClutterActor               *self,
                                                                                 gfloat                     *pivot_x,
                                                                                 gfloat                     *pivot_y);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_set_pivot_point_z                 (ClutterActor               *self,
                                                                                 gfloat                      pivot_z);
CLUTTER_AVAILABLE_IN_1_12
gfloat                          clutter_actor_get_pivot_point_z                 (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_set_rotation_angle                (ClutterActor               *self,
                                                                                 ClutterRotateAxis           axis,
                                                                                 gdouble                     angle);
CLUTTER_AVAILABLE_IN_1_12
gdouble                         clutter_actor_get_rotation_angle                (ClutterActor               *self,
                                                                                 ClutterRotateAxis           axis);
void                            clutter_actor_set_scale                         (ClutterActor               *self,
                                                                                 gdouble                     scale_x,
                                                                                 gdouble                     scale_y);
void                            clutter_actor_get_scale                         (ClutterActor               *self,
                                                                                 gdouble                    *scale_x,
                                                                                 gdouble                    *scale_y);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_set_scale_z                       (ClutterActor               *self,
                                                                                 gdouble                     scale_z);
CLUTTER_AVAILABLE_IN_1_12
gdouble                         clutter_actor_get_scale_z                       (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_set_translation                   (ClutterActor               *self,
                                                                                 gfloat                      translate_x,
                                                                                 gfloat                      translate_y,
                                                                                 gfloat                      translate_z);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_get_translation                   (ClutterActor               *self,
                                                                                 gfloat                     *translate_x,
                                                                                 gfloat                     *translate_y,
                                                                                 gfloat                     *translate_z);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_set_transform                     (ClutterActor               *self,
                                                                                 const ClutterMatrix        *transform);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_get_transform                     (ClutterActor               *self,
                                                                                 ClutterMatrix              *transform);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_set_child_transform               (ClutterActor               *self,
                                                                                 const ClutterMatrix        *transform);
CLUTTER_AVAILABLE_IN_1_12
void                            clutter_actor_get_child_transform               (ClutterActor               *self,
                                                                                 ClutterMatrix              *transform);
void                            clutter_actor_get_transformed_position          (ClutterActor               *self,
                                                                                 gfloat                     *x,
                                                                                 gfloat                     *y);
void                            clutter_actor_get_transformed_size              (ClutterActor               *self,
                                                                                 gfloat                     *width,
                                                                                 gfloat                     *height);
gboolean                        clutter_actor_transform_stage_point             (ClutterActor               *self,
                                                                                 gfloat                      x,
                                                                                 gfloat                      y,
                                                                                 gfloat                     *x_out,
                                                                                 gfloat                     *y_out);
void                            clutter_actor_get_abs_allocation_vertices       (ClutterActor               *self,
                                                                                 ClutterVertex               verts[]);
void                            clutter_actor_apply_transform_to_point          (ClutterActor               *self,
                                                                                 const ClutterVertex        *point,
                                                                                 ClutterVertex              *vertex);
void                            clutter_actor_apply_relative_transform_to_point (ClutterActor               *self,
                                                                                 ClutterActor               *ancestor,
                                                                                 const ClutterVertex        *point,
                                                                                 ClutterVertex              *vertex);

/* Implicit animations */
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_save_easing_state                 (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_restore_easing_state              (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_easing_mode                   (ClutterActor               *self,
                                                                                 ClutterAnimationMode        mode);
CLUTTER_AVAILABLE_IN_1_10
ClutterAnimationMode            clutter_actor_get_easing_mode                   (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_easing_duration               (ClutterActor               *self,
                                                                                 guint                       msecs);
CLUTTER_AVAILABLE_IN_1_10
guint                           clutter_actor_get_easing_duration               (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_set_easing_delay                  (ClutterActor               *self,
                                                                                 guint                       msecs);
CLUTTER_AVAILABLE_IN_1_10
guint                           clutter_actor_get_easing_delay                  (ClutterActor               *self);
CLUTTER_AVAILABLE_IN_1_10
ClutterTransition *             clutter_actor_get_transition                    (ClutterActor               *self,
                                                                                 const char                 *name);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_add_transition                    (ClutterActor               *self,
                                                                                 const char                 *name,
                                                                                 ClutterTransition          *transition);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_remove_transition                 (ClutterActor               *self,
                                                                                 const char                 *name);
CLUTTER_AVAILABLE_IN_1_10
void                            clutter_actor_remove_all_transitions            (ClutterActor               *self);


/* Experimental API */
#ifdef CLUTTER_ENABLE_EXPERIMENTAL_API
CLUTTER_AVAILABLE_IN_1_16
gboolean                        clutter_actor_has_mapped_clones                 (ClutterActor *self);
#endif

G_END_DECLS

#endif /* __CLUTTER_ACTOR_H__ */
