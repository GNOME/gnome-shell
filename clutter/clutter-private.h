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
 *
 *
 */

#ifndef __CLUTTER_PRIVATE_H__
#define __CLUTTER_PRIVATE_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <math.h>

#include <glib.h>

#include <glib/gi18n-lib.h>

#include "pango/cogl-pango.h"

#include "clutter-backend.h"
#include "clutter-device-manager.h"
#include "clutter-effect.h"
#include "clutter-event.h"
#include "clutter-feature.h"
#include "clutter-id-pool.h"
#include "clutter-layout-manager.h"
#include "clutter-master-clock.h"
#include "clutter-settings.h"
#include "clutter-stage-manager.h"
#include "clutter-stage-window.h"
#include "clutter-stage.h"
#include "clutter-timeline.h"

G_BEGIN_DECLS

typedef struct _ClutterMainContext      ClutterMainContext;

#define _clutter_notify_by_pspec(obj, pspec) \
  g_object_notify_by_pspec ((obj), (pspec))

#define _clutter_object_class_install_properties(oclass, n_pspecs, pspecs) \
  g_object_class_install_properties ((oclass), (n_pspecs), (pspecs))

#define CLUTTER_REGISTER_VALUE_TRANSFORM_TO(TYPE_TO,func)             { \
  g_value_register_transform_func (g_define_type_id, TYPE_TO, func);    \
}

#define CLUTTER_REGISTER_VALUE_TRANSFORM_FROM(TYPE_FROM,func)         { \
  g_value_register_transform_func (TYPE_FROM, g_define_type_id, func);  \
}

#define CLUTTER_REGISTER_INTERVAL_PROGRESS(func)                      { \
  clutter_interval_register_progress_func (g_define_type_id, func);     \
}

#define CLUTTER_PRIVATE_FLAGS(a)	 (((ClutterActor *) (a))->private_flags)
#define CLUTTER_SET_PRIVATE_FLAGS(a,f)	 (CLUTTER_PRIVATE_FLAGS (a) |= (f))
#define CLUTTER_UNSET_PRIVATE_FLAGS(a,f) (CLUTTER_PRIVATE_FLAGS (a) &= ~(f))

#define CLUTTER_ACTOR_IS_TOPLEVEL(a)            ((CLUTTER_PRIVATE_FLAGS (a) & CLUTTER_IS_TOPLEVEL) != FALSE)
#define CLUTTER_ACTOR_IS_INTERNAL_CHILD(a)      ((CLUTTER_PRIVATE_FLAGS (a) & CLUTTER_INTERNAL_CHILD) != FALSE)
#define CLUTTER_ACTOR_IN_DESTRUCTION(a)         ((CLUTTER_PRIVATE_FLAGS (a) & CLUTTER_IN_DESTRUCTION) != FALSE)
#define CLUTTER_ACTOR_IN_REPARENT(a)            ((CLUTTER_PRIVATE_FLAGS (a) & CLUTTER_IN_REPARENT) != FALSE)
#define CLUTTER_ACTOR_IN_PAINT(a)               ((CLUTTER_PRIVATE_FLAGS (a) & CLUTTER_IN_PAINT) != FALSE)
#define CLUTTER_ACTOR_IN_RELAYOUT(a)            ((CLUTTER_PRIVATE_FLAGS (a) & CLUTTER_IN_RELAYOUT) != FALSE)
#define CLUTTER_STAGE_IN_RESIZE(a)              ((CLUTTER_PRIVATE_FLAGS (a) & CLUTTER_IN_RESIZE) != FALSE)

typedef enum {
  CLUTTER_ACTOR_UNUSED_FLAG = 0,

  CLUTTER_IN_DESTRUCTION = 1 << 0,
  CLUTTER_IS_TOPLEVEL    = 1 << 1,
  CLUTTER_IN_REPARENT    = 1 << 2,

  /* Used to avoid recursion */
  CLUTTER_IN_PAINT       = 1 << 3,

  /* Used to avoid recursion */
  CLUTTER_IN_RELAYOUT    = 1 << 4,

  /* Used by the stage if resizing is an asynchronous operation (like on
   * X11) to delay queueing relayouts until we got a notification from the
   * event handling
   */
  CLUTTER_IN_RESIZE      = 1 << 5,

  /* a flag for internal children of Containers */
  CLUTTER_INTERNAL_CHILD = 1 << 6
} ClutterPrivateFlags;

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

struct _ClutterMainContext
{
  ClutterBackend  *backend;            /* holds a pointer to the windowing
                                          system backend */
  GQueue          *events_queue;       /* the main event queue */

  guint            is_initialized : 1;
  guint            motion_events_per_actor : 1;/* set for enter/leave events */
  guint            defer_display_setup : 1;
  guint            options_parsed : 1;

  GTimer          *timer;	       /* Used for debugging scheduler */

  ClutterPickMode  pick_mode;          /* Indicates pick render mode   */

  gint             num_reactives;      /* Num of reactive actors */

  ClutterIDPool   *id_pool;            /* mapping between reused integer ids
                                        * and actors
                                        */
  guint            frame_rate;         /* Default FPS */

  ClutterActor    *pointer_grab_actor; /* The actor having the pointer grab
                                        * (or NULL if there is no pointer grab
                                        */
  ClutterActor    *keyboard_grab_actor; /* The actor having the pointer grab
                                         * (or NULL if there is no pointer
                                         *  grab)
                                         */
  GSList          *shaders;            /* stack of overridden shaders */

  ClutterActor    *motion_last_actor;

  /* fb bit masks for col<->id mapping in picking */
  gint fb_r_mask, fb_g_mask, fb_b_mask;
  gint fb_r_mask_used, fb_g_mask_used, fb_b_mask_used;

  PangoContext     *pango_context;      /* Global Pango context */
  CoglPangoFontMap *font_map;           /* Global font map */

  ClutterEvent *current_event;
  guint32 last_event_time;

  gulong redraw_count;

  GList *repaint_funcs;

  ClutterSettings *settings;
};

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

#define CLUTTER_CONTEXT()	(_clutter_context_get_default ())
ClutterMainContext *_clutter_context_get_default (void);
gboolean            _clutter_context_is_initialized (void);
PangoContext *_clutter_context_create_pango_context (ClutterMainContext *self);
PangoContext *_clutter_context_get_pango_context    (ClutterMainContext *self);

#define CLUTTER_PARAM_READABLE  \
        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB
#define CLUTTER_PARAM_WRITABLE  \
        G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB
#define CLUTTER_PARAM_READWRITE \
        G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |G_PARAM_STATIC_BLURB

#define I_(str)  (g_intern_static_string ((str)))

/* mark all properties under the "Property" context */
#ifdef ENABLE_NLS
#define P_(String) (_clutter_gettext ((String)))
#else
#define P_(String) (String)
#endif

G_CONST_RETURN gchar *_clutter_gettext (const gchar *str);

gboolean      _clutter_feature_init (GError **error);

/* Reinjecting queued events for processing */
void _clutter_process_event (ClutterEvent *event);

/* Picking code */
ClutterActor *_clutter_do_pick (ClutterStage    *stage,
				gint             x,
				gint             y,
				ClutterPickMode  mode);

/* the actual redraw */
void          _clutter_do_redraw (ClutterStage *stage);

guint         _clutter_pixel_to_id (guchar pixel[4]);

void          _clutter_id_to_color (guint id, ClutterColor *col);

/* use this function as the accumulator if you have a signal with
 * a G_TYPE_BOOLEAN return value; this will stop the emission as
 * soon as one handler returns TRUE
 */
gboolean _clutter_boolean_handled_accumulator (GSignalInvocationHint *ihint,
                                               GValue                *return_accu,
                                               const GValue          *handler_return,
                                               gpointer               dummy);

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

void _clutter_run_repaint_functions (void);

gboolean _clutter_effect_pre_paint        (ClutterEffect      *effect);
void     _clutter_effect_post_paint       (ClutterEffect      *effect);
gboolean _clutter_effect_get_paint_volume (ClutterEffect      *effect,
                                           ClutterPaintVolume *volume);

void _clutter_constraint_update_allocation (ClutterConstraint *constraint,
                                            ClutterActor      *actor,
                                            ClutterActorBox   *allocation);

GType _clutter_layout_manager_get_child_meta_type (ClutterLayoutManager *manager);

void     _clutter_event_set_platform_data (ClutterEvent       *event,
                                           gpointer            data);
gpointer _clutter_event_get_platform_data (const ClutterEvent *event);




void                _clutter_util_fully_transform_vertices (const CoglMatrix *modelview,
                                                            const CoglMatrix *projection,
                                                            const int *viewport,
                                                            const ClutterVertex *vertices_in,
                                                            ClutterVertex *vertices_out,
                                                            int n_vertices);

gboolean           _clutter_actor_set_default_paint_volume (ClutterActor *self,
                                                            GType         check_gtype,
                                                            ClutterPaintVolume *volume);

G_END_DECLS

#endif /* _HAVE_CLUTTER_PRIVATE_H */
