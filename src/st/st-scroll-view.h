/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-scroll-view.h: Container with scroll-bars
 *
 * Copyright 2008 OpenedHand
 * Copyright 2009 Intel Corporation.
 * Copyright 2010 Red Hat, Inc.
 * Copyright 2010 Maxim Ermilov
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

#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif

#pragma once

#include <st/st-widget.h>
#include <st/st-scrollable.h>

G_BEGIN_DECLS

#define ST_TYPE_SCROLL_VIEW            (st_scroll_view_get_type())
G_DECLARE_DERIVABLE_TYPE (StScrollView, st_scroll_view, ST, SCROLL_VIEW, StWidget)

struct _StScrollViewClass
{
  StWidgetClass parent_class;
};

typedef enum
{
  ST_POLICY_ALWAYS,
  ST_POLICY_AUTOMATIC,
  ST_POLICY_NEVER,
  ST_POLICY_EXTERNAL,
} StPolicyType;

StWidget *st_scroll_view_new (void);

StScrollable *st_scroll_view_get_child           (StScrollView *scroll);
void          st_scroll_view_set_child           (StScrollView *scroll,
                                                  StScrollable *child);

StAdjustment *st_scroll_view_get_hadjustment     (StScrollView *scroll);
StAdjustment *st_scroll_view_get_vadjustment     (StScrollView *scroll);

gboolean      st_scroll_view_get_hscrollbar_visible (StScrollView *scroll);
gboolean      st_scroll_view_get_vscrollbar_visible (StScrollView *scroll);

gfloat        st_scroll_view_get_column_size     (StScrollView *scroll);
void          st_scroll_view_set_column_size     (StScrollView *scroll,
                                                  gfloat        column_size);

gfloat        st_scroll_view_get_row_size        (StScrollView *scroll);
void          st_scroll_view_set_row_size        (StScrollView *scroll,
                                                  gfloat        row_size);

void          st_scroll_view_set_mouse_scrolling (StScrollView *scroll,
                                                  gboolean      enabled);
gboolean      st_scroll_view_get_mouse_scrolling (StScrollView *scroll);

void          st_scroll_view_set_overlay_scrollbars (StScrollView *scroll,
                                                     gboolean      enabled);
gboolean      st_scroll_view_get_overlay_scrollbars (StScrollView *scroll);

void          st_scroll_view_set_policy          (StScrollView   *scroll,
                                                  StPolicyType    hscroll,
                                                  StPolicyType    vscroll);
void          st_scroll_view_update_fade_effect  (StScrollView  *scroll,
                                                  ClutterMargin *fade_margins);

G_END_DECLS
