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

/**
 * SECTION:clutter-gdk
 * @short_description: GDK specific API
 *
 * The GDK backend for Clutter provides some specific API, allowing
 * integration with the GDK API for manipulating the stage window and
 * handling events outside of Clutter.
 */

#ifndef __CLUTTER_GDK_H__
#define __CLUTTER_GDK_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

CLUTTER_AVAILABLE_IN_1_10
GdkDisplay *    clutter_gdk_get_default_display         (void);
CLUTTER_AVAILABLE_IN_1_10
void            clutter_gdk_set_display                 (GdkDisplay   *display);

CLUTTER_AVAILABLE_IN_1_10
GdkWindow *     clutter_gdk_get_stage_window            (ClutterStage *stage);
CLUTTER_AVAILABLE_IN_1_10
gboolean        clutter_gdk_set_stage_foreign           (ClutterStage *stage,
                                                         GdkWindow    *window);

CLUTTER_AVAILABLE_IN_1_10
GdkFilterReturn clutter_gdk_handle_event                (GdkEvent     *event);

CLUTTER_AVAILABLE_IN_1_10
ClutterStage *  clutter_gdk_get_stage_from_window       (GdkWindow    *window);

CLUTTER_AVAILABLE_IN_1_10
void            clutter_gdk_disable_event_retrieval     (void);

G_END_DECLS

#endif /* __CLUTTER_GDK_H__ */
