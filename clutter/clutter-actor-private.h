/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

#ifndef __CLUTTER_ACTOR_PRIVATE_H__
#define __CLUTTER_ACTOR_PRIVATE_H__

#include <clutter/clutter-actor.h>

G_BEGIN_DECLS

/*< private >
 * ClutterRedrawFlags:
 * @CLUTTER_REDRAW_CLIPPED_TO_ALLOCATION: Tells clutter the maximum
 *   extents of what needs to be redrawn lies within the actors
 *   current allocation. (Only use this for 2D actors though because
 *   any actor with depth may be projected outside of its allocation)
 *
 * Flags passed to the clutter_actor_queue_redraw_with_clip ()
 * function
 *
 * Since: 1.6
 */
typedef enum
{
  CLUTTER_REDRAW_CLIPPED_TO_ALLOCATION  = 1 << 0
} ClutterRedrawFlags;

/*< private >
 * ClutterActorTraverseFlags:
 * CLUTTER_ACTOR_TRAVERSE_DEPTH_FIRST: Traverse the graph in
 *   a depth first order.
 * CLUTTER_ACTOR_TRAVERSE_BREADTH_FIRST: Traverse the graph in a
 *   breadth first order.
 *
 * Controls some options for how clutter_actor_traverse() iterates
 * through the graph.
 */
typedef enum {
  CLUTTER_ACTOR_TRAVERSE_DEPTH_FIRST   = 1L<<0,
  CLUTTER_ACTOR_TRAVERSE_BREADTH_FIRST = 1L<<1
} ClutterActorTraverseFlags;

/*< private >
 * ClutterActorTraverseVisitFlags:
 * CLUTTER_ACTOR_TRAVERSE_VISIT_CONTINUE: Continue traversing as
 *   normal
 * CLUTTER_ACTOR_TRAVERSE_VISIT_SKIP_CHILDREN: Don't traverse the
 *   children of the last visited actor. (Not applicable when using
 *   %CLUTTER_ACTOR_TRAVERSE_DEPTH_FIRST_POST_ORDER since the children
 *   are visited before having an opportunity to bail out)
 * CLUTTER_ACTOR_TRAVERSE_VISIT_BREAK: Immediately bail out without
 *   visiting any more actors.
 *
 * Each time an actor is visited during a scenegraph traversal the
 * ClutterTraverseCallback can return a set of flags that may affect
 * the continuing traversal. It may stop traversal completely, just
 * skip over children for the current actor or continue as normal.
 */
typedef enum {
  CLUTTER_ACTOR_TRAVERSE_VISIT_CONTINUE       = 1L<<0,
  CLUTTER_ACTOR_TRAVERSE_VISIT_SKIP_CHILDREN  = 1L<<1,
  CLUTTER_ACTOR_TRAVERSE_VISIT_BREAK          = 1L<<2
} ClutterActorTraverseVisitFlags;

/*< private >
 * ClutterTraverseCallback:
 *
 * The callback prototype used with clutter_actor_traverse. The
 * returned flags can be used to affect the continuing traversal
 * either by continuing as normal, skipping over children of an
 * actor or bailing out completely.
 */
typedef ClutterActorTraverseVisitFlags (*ClutterTraverseCallback) (ClutterActor *actor,
                                                                   gint          depth,
                                                                   gpointer      user_data);

/*< private >
 * ClutterForeachCallback:
 * @actor: The actor being iterated
 * @user_data: The private data specified when starting the iteration
 *
 * A generic callback for iterating over actor, such as with
 * _clutter_actor_foreach_child. The difference when compared to
 * #ClutterCallback is that it returns a boolean so it is possible to break
 * out of an iteration early.
 *
 * Return value: %TRUE to continue iterating or %FALSE to break iteration
 * early.
 */
typedef gboolean (*ClutterForeachCallback) (ClutterActor *actor,
                                            gpointer      user_data);

gint          _clutter_actor_get_n_children             (ClutterActor *self);
gboolean      _clutter_actor_foreach_child              (ClutterActor *self,
                                                         ClutterForeachCallback callback,
                                                         gpointer user_data);
void          _clutter_actor_traverse                   (ClutterActor *actor,
                                                         ClutterActorTraverseFlags flags,
                                                         ClutterTraverseCallback before_children_callback,
                                                         ClutterTraverseCallback after_children_callback,
                                                         gpointer user_data);
ClutterActor *_clutter_actor_get_stage_internal         (ClutterActor *actor);

void _clutter_actor_apply_modelview_transform           (ClutterActor *self,
                                                         CoglMatrix *matrix);
void _clutter_actor_apply_modelview_transform_recursive (ClutterActor *self,
						         ClutterActor *ancestor,
                                                         CoglMatrix *matrix);

void _clutter_actor_rerealize (ClutterActor    *self,
                               ClutterCallback  callback,
                               gpointer         data);

void _clutter_actor_set_opacity_override (ClutterActor *self,
                                          gint          opacity);
gint _clutter_actor_get_opacity_override (ClutterActor *self);
void _clutter_actor_set_in_clone_paint (ClutterActor *self,
                                        gboolean      is_in_clone_paint);

void _clutter_actor_set_enable_model_view_transform (ClutterActor *self,
                                                     gboolean      enable);

void _clutter_actor_set_enable_paint_unmapped (ClutterActor *self,
                                               gboolean      enable);

void _clutter_actor_set_has_pointer (ClutterActor *self,
                                     gboolean      has_pointer);

void _clutter_actor_queue_redraw_with_clip   (ClutterActor              *self,
                                              ClutterRedrawFlags         flags,
                                              ClutterPaintVolume        *clip_volume);
ClutterPaintVolume *_clutter_actor_get_queue_redraw_clip (ClutterActor *self);
void _clutter_actor_set_queue_redraw_clip     (ClutterActor             *self,
                                               ClutterPaintVolume *clip_volume);
void _clutter_actor_finish_queue_redraw       (ClutterActor             *self,
                                               ClutterPaintVolume       *clip);

gboolean           _clutter_actor_set_default_paint_volume (ClutterActor *self,
                                                            GType         check_gtype,
                                                            ClutterPaintVolume *volume);

G_CONST_RETURN gchar *_clutter_actor_get_debug_name (ClutterActor *self);

void _clutter_actor_push_clone_paint (void);
void _clutter_actor_pop_clone_paint  (void);

guint32 _clutter_actor_get_pick_id (ClutterActor *self);

G_END_DECLS

#endif /* __CLUTTER_ACTOR_PRIVATE_H__ */
