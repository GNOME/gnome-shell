/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:ClutterAction
 * @Title: ClutterAction
 * @Short_Description: Abstract class for actor actions
 * @See_Also: #ClutterEffect
 *
 * #ClutterAction is an abstract base class for action that modify the
 * user interaction of a #ClutterActor, just like #ClutterEffect is an
 * abstract class for modifiers of the appearance of a #ClutterActor.
 *
 * Implementations of #ClutterAction are associated to an actor and can
 * provide behavioral changes when dealing with user input - for instance
 * drag and drop capabilities, or scrolling, or panning.
 *
 * #ClutterAction is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-action.h"

#include "clutter-debug.h"
#include "clutter-private.h"

G_DEFINE_ABSTRACT_TYPE (ClutterAction, clutter_action, CLUTTER_TYPE_ACTOR_META);

static void
clutter_action_class_init (ClutterActionClass *klass)
{
}

static void
clutter_action_init (ClutterAction *self)
{
}
