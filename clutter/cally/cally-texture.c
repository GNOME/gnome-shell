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

#define CALLY_TEXTURE_DEFAULT_DESCRIPTION "A texture"

static void cally_texture_class_init (CallyTextureClass *klass);
static void cally_texture_init       (CallyTexture *texture);

/* AtkObject */
static void                  cally_texture_real_initialize (AtkObject *obj,
                                                           gpointer   data);
static G_CONST_RETURN gchar *cally_texture_get_description (AtkObject *obj);


G_DEFINE_TYPE (CallyTexture, cally_texture, CALLY_TYPE_ACTOR)

static void
cally_texture_class_init (CallyTextureClass *klass)
{
/*   GObjectClass   *gobject_class = G_OBJECT_CLASS (klass); */
  AtkObjectClass *class         = ATK_OBJECT_CLASS (klass);

  class->initialize      = cally_texture_real_initialize;
  class->get_description = cally_texture_get_description;
}

static void
cally_texture_init (CallyTexture *texture)
{
  /* nothing to do yet */
}

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

static G_CONST_RETURN gchar *
cally_texture_get_description (AtkObject *obj)
{
  G_CONST_RETURN gchar *description = NULL;

  g_return_val_if_fail (CALLY_IS_TEXTURE (obj), NULL);

  description = ATK_OBJECT_CLASS (cally_texture_parent_class)->get_description (obj);
  if (description == NULL)
    description = CALLY_TEXTURE_DEFAULT_DESCRIPTION;

  return description;
}
