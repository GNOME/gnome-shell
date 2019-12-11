/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-box-layout.h: box layout actor
 *
 * Copyright 2009 Intel Corporation.
 * Copyright 2009, 2010 Red Hat, Inc.
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

#ifndef _ST_BOX_LAYOUT_H
#define _ST_BOX_LAYOUT_H

#include <st/st-widget.h>
#include <st/st-viewport.h>

G_BEGIN_DECLS

#define ST_TYPE_BOX_LAYOUT st_box_layout_get_type()
G_DECLARE_FINAL_TYPE (StBoxLayout, st_box_layout, ST, BOX_LAYOUT, StViewport)

typedef struct _StBoxLayout StBoxLayout;
typedef struct _StBoxLayoutPrivate StBoxLayoutPrivate;

/**
 * StBoxLayout:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _StBoxLayout
{
  /*< private >*/
  StViewport parent;

  StBoxLayoutPrivate *priv;
};

StWidget *st_box_layout_new (void);

void     st_box_layout_set_vertical   (StBoxLayout *box,
                                       gboolean     vertical);
gboolean st_box_layout_get_vertical   (StBoxLayout *box);

void     st_box_layout_set_pack_start (StBoxLayout *box,
                                       gboolean     pack_start);
gboolean st_box_layout_get_pack_start (StBoxLayout *box);

G_END_DECLS

#endif /* _ST_BOX_LAYOUT_H */
