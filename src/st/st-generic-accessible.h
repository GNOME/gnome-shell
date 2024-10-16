/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-generic-accessible.h: generic accessible
 *
 * Copyright 2013 Igalia, S.L.
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

#include <clutter/clutter.h>
#include <st/st-widget-accessible.h>

G_BEGIN_DECLS

#define ST_TYPE_GENERIC_ACCESSIBLE                 (st_generic_accessible_get_type ())

G_DECLARE_FINAL_TYPE (StGenericAccessible,
                      st_generic_accessible,
                      ST, GENERIC_ACCESSIBLE,
                      StWidgetAccessible)

AtkObject*  st_generic_accessible_new_for_actor (ClutterActor *actor);

G_END_DECLS
