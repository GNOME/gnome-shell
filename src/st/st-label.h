/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-label.h: Plain label actor
 *
 * Copyright 2008, 2009 Intel Corporation.
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

#ifndef __ST_LABEL_H__
#define __ST_LABEL_H__

G_BEGIN_DECLS

#include <st/st-widget.h>

#define ST_TYPE_LABEL                (st_label_get_type ())
G_DECLARE_FINAL_TYPE (StLabel, st_label, ST, LABEL, StWidget)

typedef struct _StLabelPrivate       StLabelPrivate;

/**
 * StLabel:
 *
 * The contents of this structure is private and should only be accessed using
 * the provided API.
 */
struct _StLabel
{
  /*< private >*/
  StWidget parent_instance;

  StLabelPrivate *priv;
};

StWidget *     st_label_new              (const gchar *text);
const gchar *  st_label_get_text         (StLabel     *label);
void           st_label_set_text         (StLabel     *label,
                                          const gchar *text);
ClutterActor * st_label_get_clutter_text (StLabel     *label);

G_END_DECLS

#endif /* __ST_LABEL_H__ */
