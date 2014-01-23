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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:cally-texture
 * @Title: CallyTexture
 * @short_description: Implementation of the ATK interfaces for a #ClutterTexture
 * @see_also: #ClutterTexture
 *
 * #CallyTexture implements the required ATK interfaces of #ClutterTexture
 *
 * In particular it sets a proper role for the texture.
 */

#include "cally-texture.h"
#include "cally-actor-private.h"

/* AtkObject */
static void                  cally_texture_real_initialize (AtkObject *obj,
                                                           gpointer   data);

G_DEFINE_TYPE (CallyTexture, cally_texture, CALLY_TYPE_ACTOR)

static void
cally_texture_class_init (CallyTextureClass *klass)
{
/*   GObjectClass   *gobject_class = G_OBJECT_CLASS (klass); */
  AtkObjectClass *class         = ATK_OBJECT_CLASS (klass);

  class->initialize      = cally_texture_real_initialize;
}

static void
cally_texture_init (CallyTexture *texture)
{
  /* nothing to do yet */
}

/**
 * cally_texture_new:
 * @actor: a #ClutterActor
 *
 * Creates a new #CallyTexture for the given @actor. @actor must be
 * a #ClutterTexture.
 *
 * Return value: the newly created #AtkObject
 *
 * Since: 1.4
 */
AtkObject*
cally_texture_new (ClutterActor *actor)
{
  GObject   *object     = NULL;
  AtkObject *accessible = NULL;

  g_return_val_if_fail (CLUTTER_IS_TEXTURE (actor), NULL);

  object = g_object_new (CALLY_TYPE_TEXTURE, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, actor);

  return accessible;
}

static void
cally_texture_real_initialize (AtkObject *obj,
                              gpointer   data)
{
  ATK_OBJECT_CLASS (cally_texture_parent_class)->initialize (obj, data);

  /* default role */
  obj->role = ATK_ROLE_IMAGE;
}
