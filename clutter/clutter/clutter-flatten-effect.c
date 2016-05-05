/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011  Intel Corporation.
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
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

/* This is an internal-only effect used to implement the
   'offscreen-redirect' property of ClutterActor. It doesn't actually
   need to do anything on top of the ClutterOffscreenEffect class so
   it only exists because that class is abstract */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include "clutter-flatten-effect.h"
#include "clutter-private.h"
#include "clutter-actor-private.h"

G_DEFINE_TYPE (ClutterFlattenEffect,
               _clutter_flatten_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

static void
_clutter_flatten_effect_class_init (ClutterFlattenEffectClass *klass)
{
}

static void
_clutter_flatten_effect_init (ClutterFlattenEffect *self)
{
}

ClutterEffect *
_clutter_flatten_effect_new (void)
{
  return g_object_new (CLUTTER_TYPE_FLATTEN_EFFECT, NULL);
}
