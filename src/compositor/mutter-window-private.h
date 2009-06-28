/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef MUTTER_WINDOW_PRIVATE_H
#define MUTTER_WINDOW_PRIVATE_H

#include <X11/extensions/Xdamage.h>
#include "compositor-mutter.h"

MutterWindow *mutter_window_new (MetaWindow *window);

void mutter_window_destroy   (MutterWindow *cw);

void mutter_window_show (MutterWindow   *cw,
                         MetaCompEffect  effect);
void mutter_window_hide (MutterWindow   *cw,
                         MetaCompEffect  effect);

void mutter_window_maximize   (MutterWindow  *cw,
                               MetaRectangle *old_rect,
                               MetaRectangle *new_rect);
void mutter_window_unmaximize (MutterWindow  *cw,
                               MetaRectangle *old_rect,
                               MetaRectangle *new_rect);

void     mutter_window_process_damage          (MutterWindow       *cw,
                                                XDamageNotifyEvent *event);
void     mutter_window_pre_paint               (MutterWindow       *self);

gboolean mutter_window_effect_in_progress      (MutterWindow       *cw);
void     mutter_window_sync_actor_position     (MutterWindow       *cw);
void     mutter_window_sync_visibility         (MutterWindow       *cw);
void     mutter_window_update_window_type      (MutterWindow       *cw);
void     mutter_window_update_shape            (MutterWindow       *cw,
                                                gboolean            shaped);
void     mutter_window_update_opacity          (MutterWindow       *cw);
void     mutter_window_mapped                  (MutterWindow       *cw);
void     mutter_window_unmapped                (MutterWindow       *cw);

void mutter_window_effect_completed (MutterWindow *actor,
                                     gulong        event);

#endif /* MUTTER_WINDOW_PRIVATE_H */
