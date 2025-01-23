/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-scroll-view-fade.h: Edge fade effect for StScrollView
 *
 * Copyright 2010 Intel Corporation.
 * Copyright 2011 Adel Gadllah
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define ST_TYPE_SCROLL_VIEW_FADE        (st_scroll_view_fade_get_type ())
G_DECLARE_FINAL_TYPE (StScrollViewFade, st_scroll_view_fade,
                      ST, SCROLL_VIEW_FADE, ClutterShaderEffect)

ClutterEffect *st_scroll_view_fade_new (void);

void st_scroll_view_fade_set_fade_margins (StScrollViewFade *fade,
                                           ClutterMargin    *margins);
ClutterMargin * st_scroll_view_fade_get_fade_margins (StScrollViewFade *fade);

void st_scroll_view_fade_set_fade_edges (StScrollViewFade *fade,
                                         gboolean          fade_edges);
gboolean st_scroll_view_fade_get_fade_edges (StScrollViewFade *fade);

void st_scroll_view_fade_set_extend_fade_area (StScrollViewFade *fade,
                                               gboolean          extend_fade_area);
gboolean st_scroll_view_fade_get_extend_fade_area (StScrollViewFade *fade);

G_END_DECLS
