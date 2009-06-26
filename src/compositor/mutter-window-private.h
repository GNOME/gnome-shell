/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef MUTTER_WINDOW_PRIVATE_H
#define MUTTER_WINDOW_PRIVATE_H

#include <X11/extensions/Xdamage.h>
#include "compositor-mutter.h"

MutterWindow *mutter_window_new (MetaWindow *window);

void mutter_window_map      (MutterWindow *cw);
void mutter_window_unmap    (MutterWindow *cw);
void mutter_window_minimize (MutterWindow *cw);
void mutter_window_destroy  (MutterWindow *cw);

void mutter_window_maximize   (MutterWindow  *cw,
                               MetaRectangle *window_rect);
void mutter_window_unmaximize (MutterWindow  *cw,
                               MetaRectangle *window_rect);

void     mutter_window_process_damage          (MutterWindow       *cw,
                                                XDamageNotifyEvent *event);
gboolean mutter_window_effect_in_progress      (MutterWindow       *cw,
                                                gboolean            include_destroy);
void     mutter_window_sync_actor_position     (MutterWindow       *cw);
void     mutter_window_finish_workspace_switch (MutterWindow       *cw);
void     mutter_window_update_window_type      (MutterWindow       *cw);
void     mutter_window_update_shape            (MutterWindow       *cw,
                                                gboolean            shaped);
void     mutter_window_update_opacity          (MutterWindow       *cw);
void     mutter_window_set_hidden              (MutterWindow       *cw,
                                                gboolean            hidden);
void     mutter_window_queue_map_change        (MutterWindow       *cw,
                                                gboolean            should_be_mapped);

void mutter_window_effect_completed (MutterWindow *actor,
                                     gulong        event);

#endif /* MUTTER_WINDOW_PRIVATE_H */
