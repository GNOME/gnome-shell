/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-widget-accessible.h: Accessible object for StWidget
 *
 * Copyright 2010 Igalia, S.L.
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
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

G_BEGIN_DECLS

#include <st/st-widget.h>
#include <clutter/clutter.h>

#define ST_TYPE_WIDGET_ACCESSIBLE st_widget_accessible_get_type ()

G_DECLARE_DERIVABLE_TYPE (StWidgetAccessible,
                          st_widget_accessible,
                          ST, WIDGET_ACCESSIBLE,
                          ClutterActorAccessible)

typedef struct _StWidgetAccessibleClass  StWidgetAccessibleClass;


struct _StWidgetAccessibleClass
{
  ClutterActorAccessibleClass parent_class;
};


G_END_DECLS
