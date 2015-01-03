/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2009 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:cally-rectangle
 * @short_description: Implementation of the ATK interfaces for a #ClutterRectangle
 * @see_also: #ClutterRectangle
 *
 * #CallyRectangle implements the required ATK interfaces of #ClutterRectangle
 *
 * In particular it sets a proper role for the rectangle.
 */

#include "config.h"

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS

#include "cally-rectangle.h"
#include "cally-actor-private.h"

#include "clutter-color.h"
#include "deprecated/clutter-rectangle.h"

/* AtkObject */
static void                  cally_rectangle_real_initialize (AtkObject *obj,
                                                              gpointer   data);

G_DEFINE_TYPE (CallyRectangle, cally_rectangle, CALLY_TYPE_ACTOR)

static void
cally_rectangle_class_init (CallyRectangleClass *klass)
{
/*   GObjectClass   *gobject_class = G_OBJECT_CLASS (klass); */
  AtkObjectClass *class         = ATK_OBJECT_CLASS (klass);

  class->initialize      = cally_rectangle_real_initialize;
}

static void
cally_rectangle_init (CallyRectangle *rectangle)
{
  /* nothing to do yet */
}

/**
 * cally_rectangle_new:
 * @actor: a #ClutterActor
 *
 * Creates a new #CallyRectangle for the given @actor. @actor must be
 * a #ClutterRectangle.
 *
 * Return value: the newly created #AtkObject
 *
 * Since: 1.4
 */
AtkObject*
cally_rectangle_new (ClutterActor *actor)
{
  GObject   *object     = NULL;
  AtkObject *accessible = NULL;

  g_return_val_if_fail (CLUTTER_IS_RECTANGLE (actor), NULL);

  object = g_object_new (CALLY_TYPE_RECTANGLE, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, actor);

  return accessible;
}

static void
cally_rectangle_real_initialize (AtkObject *obj,
                                 gpointer   data)
{
  ATK_OBJECT_CLASS (cally_rectangle_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_IMAGE;
}
