/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-theme-node-transition.h: Theme node transitions for StWidget.
 *
 * Copyright 2010 Florian Müllner
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ST_THEME_NODE_TRANSITION_H__
#define __ST_THEME_NODE_TRANSITION_H__

#include <clutter/clutter.h>

#include "st-widget.h"
#include "st-theme-node.h"

G_BEGIN_DECLS

#define ST_TYPE_THEME_NODE_TRANSITION (st_theme_node_transition_get_type ())
G_DECLARE_FINAL_TYPE (StThemeNodeTransition, st_theme_node_transition,
                      ST, THEME_NODE_TRANSITION, GObject)

StThemeNodeTransition *st_theme_node_transition_new (ClutterActor          *actor,
                                                     StThemeNode           *from_node,
                                                     StThemeNode           *to_node,
                                                     StThemeNodePaintState *old_paint_state,
                                                     guint                  duration);

void  st_theme_node_transition_update   (StThemeNodeTransition *transition,
                                         StThemeNode           *new_node);

void  st_theme_node_transition_paint    (StThemeNodeTransition *transition,
                                         CoglFramebuffer       *framebuffer,
                                         ClutterActorBox       *allocation,
                                         guint8                 paint_opacity,
                                         float                  resource_scale);

void  st_theme_node_transition_get_paint_box (StThemeNodeTransition *transition,
                                              const ClutterActorBox *allocation,
                                              ClutterActorBox       *paint_box);

StThemeNodePaintState * st_theme_node_transition_get_new_paint_state (StThemeNodeTransition *transition);

G_END_DECLS

#endif
