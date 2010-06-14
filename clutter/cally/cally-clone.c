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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:callyclutterclone
 * @short_description: Implementation of the ATK interfaces for a #ClutterClone
 * @see_also: #ClutterClone
 *
 * #CallyClutterClone implements the required ATK interfaces of #ClutterClone
 *
 * In particular it sets a proper role for the clone, as just a image,
 * as it is the sanest and simplest approach.
 *
 * Check http://lists.o-hand.com/clutter/3797.html for more information
 */

#include "cally-clone.h"
#include "cally-actor-private.h"

#define CALLY_CLONE_DEFAULT_DESCRIPTION "ClutterClone accessibility object"

static void cally_clone_class_init (CallyCloneClass *klass);
static void cally_clone_init       (CallyClone *clone);

/* AtkObject */
static void                  cally_clone_real_initialize (AtkObject *obj,
                                                           gpointer   data);
static G_CONST_RETURN gchar *cally_clone_get_description (AtkObject *obj);


G_DEFINE_TYPE (CallyClone, cally_clone, CALLY_TYPE_ACTOR)

static void
cally_clone_class_init (CallyCloneClass *klass)
{
/*   GObjectClass   *gobject_class = G_OBJECT_CLASS (klass); */
  AtkObjectClass *class         = ATK_OBJECT_CLASS (klass);

  class->initialize      = cally_clone_real_initialize;
  class->get_description = cally_clone_get_description;
}

static void
cally_clone_init (CallyClone *clone)
{
  /* nothing to do yet */
}

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

static G_CONST_RETURN gchar *
cally_clone_get_description (AtkObject *obj)
{
  G_CONST_RETURN gchar *description = NULL;

  g_return_val_if_fail (CALLY_IS_CLONE (obj), NULL);

  description = ATK_OBJECT_CLASS (cally_clone_parent_class)->get_description (obj);
  if (description == NULL)
    description = CALLY_CLONE_DEFAULT_DESCRIPTION;

  return description;
}
