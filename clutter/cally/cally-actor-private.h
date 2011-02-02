/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2008 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 *
 * Some parts are based on GailWidget from GAIL
 * GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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
 */

#ifndef __CALLY_ACTOR_PRIVATE_H__
#define __CALLY_ACTOR_PRIVATE_H__

#include "cally-actor.h"

/*
 * Auxiliar define, in order to get the clutter actor from the AtkObject using
 * AtkGObject methods
 *
 */
#define CALLY_GET_CLUTTER_ACTOR(cally_object) \
  (CLUTTER_ACTOR (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (cally_object))))

void _cally_actor_get_top_level_origin (ClutterActor *actor,
                                        gint         *x,
                                        gint         *y);

#endif /* __CALLY_ACTOR_PRIVATE_H__ */
