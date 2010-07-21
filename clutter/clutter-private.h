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

  /* Used by the stage to indicate GL viewport / perspective etc needs
   * (re)setting.
   */
  CLUTTER_SYNC_MATRICES  = 1 << 3,

  /* Used to avoid recursion */
  CLUTTER_IN_PAINT       = 1 << 4,

  /* Used to avoid recursion */
  CLUTTER_IN_RELAYOUT    = 1 << 5,

  /* Used by the stage if resizing is an asynchronous operation (like on
   * X11) to delay queueing relayouts until we got a notification from the
   * event handling
   */
  CLUTTER_IN_RESIZE      = 1 << 6,

  /* a flag for internal children of Containers */
  CLUTTER_INTERNAL_CHILD = 1 << 7
} ClutterPrivateFlags;

struct _ClutterInputDevice
{
  GObject parent_instance;

  gint id;

  ClutterInputDeviceType device_type;

  gchar *device_name;

  /* the actor underneath the pointer */
  ClutterActor *cursor_actor;

  /* the actor that has a grab in place for the device */
  ClutterActor *pointer_grab_actor;

  /* the current click count */
  gint click_count;

  /* the stage the device is on */
  ClutterStage *stage;

  /* the current state */
  gint current_x;
  gint current_y;
  guint32 current_time;
  gint current_button_number;
  ClutterModifierType current_state;

  /* the previous state, used for click count generation */
  gint previous_x;
  gint previous_y;
  guint32 previous_time;
  gint previous_button_number;
  ClutterModifierType previous_state;
};

struct _ClutterStageManager
{
  GObject parent_instance;

  GSList *stages;
};

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

/* device manager */
void _clutter_device_manager_add_device     (ClutterDeviceManager *device_manager,
                                             ClutterInputDevice   *device);
void _clutter_device_manager_remove_device  (ClutterDeviceManager *device_manager,
                                             ClutterInputDevice   *device);
void _clutter_device_manager_update_devices (ClutterDeviceManager *device_manager);

/* input device */
void          _clutter_input_device_set_coords (ClutterInputDevice  *device,
                                                gint                 x,
                                                gint                 y);
void          _clutter_input_device_set_state  (ClutterInputDevice  *device,
                                                ClutterModifierType  state);
void          _clutter_input_device_set_time   (ClutterInputDevice  *device,
                                                guint32              time_);
void          _clutter_input_device_set_stage  (ClutterInputDevice  *device,
                                                ClutterStage        *stage);
void          _clutter_input_device_set_actor  (ClutterInputDevice  *device,
                                                ClutterActor        *actor);
ClutterActor *_clutter_input_device_update     (ClutterInputDevice  *device);

/* stage manager */
void _clutter_stage_manager_add_stage         (ClutterStageManager *stage_manager,
                                               ClutterStage        *stage);
void _clutter_stage_manager_remove_stage      (ClutterStageManager *stage_manager,
                                               ClutterStage        *stage);
void _clutter_stage_manager_set_default_stage (ClutterStageManager *stage_manager,
                                               ClutterStage        *stage);

/* stage */
void                _clutter_stage_set_window           (ClutterStage       *stage,
                                                         ClutterStageWindow *stage_window);
ClutterStageWindow *_clutter_stage_get_window           (ClutterStage       *stage);
ClutterStageWindow *_clutter_stage_get_default_window   (void);
void                _clutter_stage_maybe_setup_viewport (ClutterStage       *stage);
void                _clutter_stage_maybe_relayout       (ClutterActor       *stage);
gboolean            _clutter_stage_needs_update         (ClutterStage       *stage);
gboolean            _clutter_stage_do_update            (ClutterStage       *stage);


void     _clutter_stage_queue_event            (ClutterStage *stage,
					        ClutterEvent *event);
gboolean _clutter_stage_has_queued_events      (ClutterStage *stage);
void     _clutter_stage_process_queued_events  (ClutterStage *stage);
void     _clutter_stage_update_input_devices   (ClutterStage *stage);

int      _clutter_stage_get_pending_swaps      (ClutterStage *stage);

gboolean _clutter_stage_has_full_redraw_queued (ClutterStage *stage);

/* vfuncs implemented by backend */
GType         _clutter_backend_impl_get_type  (void);

void          _clutter_backend_redraw         (ClutterBackend  *backend,
                                               ClutterStage    *stage);
ClutterStageWindow *_clutter_backend_create_stage   (ClutterBackend  *backend,
                                               ClutterStage    *wrapper,
                                               GError         **error);
void          _clutter_backend_ensure_context (ClutterBackend  *backend,
                                               ClutterStage    *stage);
void          _clutter_backend_ensure_context_internal
                                              (ClutterBackend  *backend,
                                               ClutterStage    *stage);
gboolean      _clutter_backend_create_context (ClutterBackend  *backend,
                                               GError         **error);

void          _clutter_backend_add_options    (ClutterBackend  *backend,
                                               GOptionGroup    *group);
gboolean      _clutter_backend_pre_parse      (ClutterBackend  *backend,
                                               GError         **error);
gboolean      _clutter_backend_post_parse     (ClutterBackend  *backend,
                                               GError         **error);
void          _clutter_backend_init_events    (ClutterBackend  *backend);

void          _clutter_backend_copy_event_data (ClutterBackend  *backend,
                                                ClutterEvent    *src,
                                                ClutterEvent    *dest);
void          _clutter_backend_free_event_data (ClutterBackend  *backend,
                                                ClutterEvent    *event);

ClutterFeatureFlags _clutter_backend_get_features (ClutterBackend *backend);

gfloat        _clutter_backend_get_units_per_em   (ClutterBackend       *backend,
                                                   PangoFontDescription *font_desc);

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

void _clutter_actor_apply_modelview_transform_recursive (ClutterActor *self,
						       ClutterActor *ancestor);

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

void _clutter_actor_transform_and_project_box (ClutterActor          *self,
					       const ClutterActorBox *box,
					       ClutterVertex          verts[]);

void _clutter_actor_queue_redraw_with_clip (ClutterActor          *self,
                                            ClutterRedrawFlags     flags,
                                            ClutterActorBox       *clip);
const ClutterActorBox *_clutter_actor_get_queue_redraw_clip (ClutterActor *self);
void _clutter_actor_set_queue_redraw_clip (ClutterActor *self,
                                           const ClutterActorBox *clip);

void _clutter_run_repaint_functions (void);

gint32 _clutter_backend_get_units_serial (ClutterBackend *backend);

gboolean _clutter_effect_pre_paint  (ClutterEffect *effect);
void     _clutter_effect_post_paint (ClutterEffect *effect);

GType _clutter_layout_manager_get_child_meta_type (ClutterLayoutManager *manager);

void     _clutter_event_set_platform_data (ClutterEvent       *event,
                                           gpointer            data);
gpointer _clutter_event_get_platform_data (const ClutterEvent *event);

G_END_DECLS

#endif /* _HAVE_CLUTTER_PRIVATE_H */
