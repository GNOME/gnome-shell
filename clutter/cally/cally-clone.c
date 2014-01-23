/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2010 Igalia, S.L.
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:cally-clone
 * @Title: CallyClone
 * @short_description: Implementation of the ATK interfaces for a #ClutterClone
 * @see_also: #ClutterClone
 *
 * #CallyClone implements the required ATK interfaces of #ClutterClone
 *
 * In particular it sets a proper role for the clone, as just a image,
 * as it is the sanest and simplest approach.
 */

/* Design rationale for CallyClone:
 *
 * In the old times, it was just ClutterCloneTexture. So, from a a11y POV
 * CallyCloneTexture was just another image, like ClutterTexture, and if
 * it was a clone was irrevelant. So on cally-0.8, CallyCloneTexture
 * expose a object with role ATK_ROLE_IMAGE. But now, ClutterClone is more
 * general. You can clone any object, including groups, and made things
 * like have one text entry, and a clone with different properties in the
 * same window, updated both at once.
 *
 * The question is if the idea is have a ClutterClone as a "first-class"
 * citizen inside the stage hierarchy (full clone), or it is just supposed
 * to be a mirror image of the original object.
 *
 * In the case of the a11y POV this would mean that if the text changes on
 * the source, the clone should emit as well the text-changing signals.
 *
 * As ClutterClone smartly just paint the same object with different
 * parameters, this would mean that it should be the cally object the one
 * that should replicate the source clutter hierarchy to do that,
 * something that just sound crazy.
 *
 * Taking into account that:
 *
 * - ClutterClone doesn't re-emit mirrored signals from the source 
 *   I think that likely the answer would be "yes, it is just a
 *   mirrored image, not a real full clone".
 *
 * - You can't interact directly with the clone (ie: focus, and so on).
 *   Its basic usage (right now) is clone textures.
 *
 * Any other solution could be overwhelming.
 *
 * I think that the final solution would be that ClutterClone from the
 * a11y POV should still be managed as a image (with the proper properties,
 * position, size, etc.).
 */

#include "cally-clone.h"
#include "cally-actor-private.h"

/* AtkObject */
static void                  cally_clone_real_initialize (AtkObject *obj,
                                                          gpointer   data);

G_DEFINE_TYPE (CallyClone, cally_clone, CALLY_TYPE_ACTOR)

static void
cally_clone_class_init (CallyCloneClass *klass)
{
/*   GObjectClass   *gobject_class = G_OBJECT_CLASS (klass); */
  AtkObjectClass *class         = ATK_OBJECT_CLASS (klass);

  class->initialize      = cally_clone_real_initialize;
}

static void
cally_clone_init (CallyClone *clone)
{
  /* nothing to do yet */
}

/**
 * cally_clone_new:
 * @actor: a #ClutterActor
 *
 * Creates a new #CallyClone for the given @actor. @actor must be a
 * #ClutterClone.
 *
 * Return value: the newly created #AtkObject
 *
 * Since: 1.4
 */
AtkObject*
cally_clone_new (ClutterActor *actor)
{
  GObject   *object     = NULL;
  AtkObject *accessible = NULL;

  g_return_val_if_fail (CLUTTER_IS_CLONE (actor), NULL);

  object = g_object_new (CALLY_TYPE_CLONE, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, actor);

  return accessible;
}

static void
cally_clone_real_initialize (AtkObject *obj,
                              gpointer   data)
{
  ATK_OBJECT_CLASS (cally_clone_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_IMAGE;
}
