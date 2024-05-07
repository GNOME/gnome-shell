#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#pragma once

#include <clutter/clutter.h>

#include <st/st-types.h>

G_BEGIN_DECLS

#define SHELL_TYPE_DND_START_GESTURE (st_dnd_start_gesture_get_type ())
G_DECLARE_FINAL_TYPE (StDndStartGesture, st_dnd_start_gesture,
                      ST, DND_START_GESTURE, ClutterGesture)

void st_dnd_start_gesture_start_drag (StDndStartGesture  *self,
                                      const ClutterEvent *start_event);

void st_dnd_start_gesture_get_drag_coords (StDndStartGesture *self,
                                           graphene_point_t  *coords_out);

const ClutterEvent * st_dnd_start_gesture_get_point_begin_event (StDndStartGesture *self);

const ClutterEvent * st_dnd_start_gesture_get_drag_triggering_event (StDndStartGesture *self);

gboolean st_dnd_start_gesture_get_manual_mode (StDndStartGesture *self);

void st_dnd_start_gesture_set_manual_mode (StDndStartGesture *self,
                                           gboolean           manual_mode);

guint32 st_dnd_start_gesture_get_timeout_threshold (StDndStartGesture *self);

void st_dnd_start_gesture_set_timeout_threshold (StDndStartGesture *self,
                                                 uint32_t           timeout_threshold_ms);

G_END_DECLS
