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

/*
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

/* ClutterActorTraverseFlags:
 *
 * Controls some options for how clutter_actor_traverse() iterates
 * through the graph.
 */
typedef enum _ClutterActorTraverseFlags
{
  CLUTTER_ACTOR_TRAVERSE_PLACE_HOLDER  = 1L<<0
} ClutterActorTraverseFlags;

/* ClutterForeachCallback:
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
                                            void *user_data);

gint          _clutter_actor_get_n_children             (ClutterActor *self);
gboolean      _clutter_actor_foreach_child              (ClutterActor *self,
                                                         ClutterForeachCallback callback,
                                                         void *user_data);
gboolean      _clutter_actor_traverse                   (ClutterActor *actor,
                                                         ClutterActorTraverseFlags flags,
                                                         ClutterForeachCallback callback,
                                                         void *user_data);
ClutterActor *_clutter_actor_get_stage_internal         (ClutterActor *actor);

void _clutter_actor_apply_modelview_transform           (ClutterActor *self,
                                                         CoglMatrix *matrix);
void _clutter_actor_apply_modelview_transform_recursive (ClutterActor *self,
						         ClutterActor *ancestor,
                                                         CoglMatrix *matrix);

void _clutter_actor_rerealize           (ClutterActor    *self,
                                         ClutterCallback  callback,
                                         void            *data);

void _clutter_actor_set_opacity_parent (ClutterActor *self,
                                        ClutterActor *parent);

void _clutter_actor_set_enable_model_view_transform (ClutterActor *self,
                                                     gboolean      enable);

void _clutter_actor_set_enable_paint_unmapped (ClutterActor *self,
                                               gboolean      enable);

void _clutter_actor_set_has_pointer (ClutterActor *self,
                                     gboolean      has_pointer);

void _clutter_actor_queue_redraw_with_clip   (ClutterActor              *self,
                                              ClutterRedrawFlags         flags,
                                              ClutterPaintVolume        *clip_volume);
const ClutterPaintVolume *_clutter_actor_get_queue_redraw_clip (ClutterActor *self);
void _clutter_actor_set_queue_redraw_clip     (ClutterActor             *self,
                                               const ClutterPaintVolume *clip_volume);
void _clutter_actor_finish_queue_redraw       (ClutterActor             *self,
                                               ClutterPaintVolume       *clip);

gboolean           _clutter_actor_set_default_paint_volume (ClutterActor *self,
                                                            GType         check_gtype,
                                                            ClutterPaintVolume *volume);

G_CONST_RETURN gchar *_clutter_actor_get_debug_name (ClutterActor *self);

G_END_DECLS

#endif /* __CLUTTER_ACTOR_PRIVATE_H__ */
