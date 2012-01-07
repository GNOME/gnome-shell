/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2008 Iain Holmes
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_COMPOSITOR_H
#define META_COMPOSITOR_H

#include <glib.h>
#include <X11/Xlib.h>

#include <meta/types.h>
#include <meta/boxes.h>
#include <meta/window.h>
#include <meta/workspace.h>

/**
 * MetaCompEffect:
 * @META_COMP_EFFECT_CREATE: The window is newly created
 *   (also used for a window that was previously on a different
 *   workspace and is changed to become visible on the active
 *   workspace.)
 * @META_COMP_EFFECT_UNMINIMIZE: The window should be shown
 *   as unminimizing from its icon geometry.
 * @META_COMP_EFFECT_DESTROY: The window is being destroyed
 * @META_COMP_EFFECT_MINIMIZE: The window should be shown
 *   as minimizing to its icon geometry.
 * @META_COMP_EFFECT_NONE: No effect, the window should be
 *   shown or hidden immediately.
 *
 * Indicates the appropriate effect to show the user for
 * meta_compositor_show_window() and meta_compositor_hide_window()
 */
typedef enum
{
  META_COMP_EFFECT_CREATE,
  META_COMP_EFFECT_UNMINIMIZE,
  META_COMP_EFFECT_DESTROY,
  META_COMP_EFFECT_MINIMIZE,
  META_COMP_EFFECT_NONE
} MetaCompEffect;

MetaCompositor *meta_compositor_new     (MetaDisplay    *display);
void            meta_compositor_destroy (MetaCompositor *compositor);

void meta_compositor_manage_screen   (MetaCompositor *compositor,
                                      MetaScreen     *screen);
void meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                      MetaScreen     *screen);

void meta_compositor_window_x11_shape_changed (MetaCompositor *compositor,
                                               MetaWindow     *window);

gboolean meta_compositor_process_event (MetaCompositor *compositor,
                                        XEvent         *event,
                                        MetaWindow     *window);

gboolean meta_compositor_filter_keybinding (MetaCompositor *compositor,
                                            MetaScreen     *screen,
                                            MetaKeyBinding *binding);

void meta_compositor_add_window    (MetaCompositor *compositor,
                                    MetaWindow     *window);
void meta_compositor_remove_window (MetaCompositor *compositor,
                                    MetaWindow     *window);

void meta_compositor_show_window       (MetaCompositor      *compositor,
                                        MetaWindow          *window,
                                        MetaCompEffect       effect);
void meta_compositor_hide_window       (MetaCompositor      *compositor,
                                        MetaWindow          *window,
                                        MetaCompEffect       effect);
void meta_compositor_switch_workspace  (MetaCompositor      *compositor,
                                        MetaScreen          *screen,
                                        MetaWorkspace       *from,
                                        MetaWorkspace       *to,
                                        MetaMotionDirection  direction);

void meta_compositor_maximize_window   (MetaCompositor      *compositor,
                                        MetaWindow          *window,
                                        MetaRectangle       *old_rect,
                                        MetaRectangle       *new_rect);
void meta_compositor_unmaximize_window (MetaCompositor      *compositor,
                                        MetaWindow          *window,
                                        MetaRectangle       *old_rect,
                                        MetaRectangle       *new_rect);

void meta_compositor_window_mapped        (MetaCompositor *compositor,
                                           MetaWindow     *window);
void meta_compositor_window_unmapped      (MetaCompositor *compositor,
                                           MetaWindow     *window);
void meta_compositor_sync_window_geometry (MetaCompositor *compositor,
                                           MetaWindow     *window,
                                           gboolean        did_placement);
void meta_compositor_set_updates_frozen   (MetaCompositor *compositor,
                                           MetaWindow     *window,
                                           gboolean        updates_frozen);
void meta_compositor_queue_frame_drawn    (MetaCompositor *compositor,
                                           MetaWindow     *window,
                                           gboolean        no_delay_frame);

void meta_compositor_sync_stack                (MetaCompositor *compositor,
                                                MetaScreen     *screen,
                                                GList          *stack);
void meta_compositor_sync_screen_size          (MetaCompositor *compositor,
                                                MetaScreen     *screen,
                                                guint           width,
                                                guint           height);

void meta_compositor_flash_screen              (MetaCompositor *compositor,
                                                MetaScreen     *screen);

#endif /* META_COMPOSITOR_H */
